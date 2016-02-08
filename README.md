# toggle-modifier-proposal

We extend an EventEmitter class to create a user model:

```typescript
class User extends EventEmitter {
    private title: string;
    
    public setTitle(title: string) {
        this.title = title;
        this.emit('change:title', title);
    }
}
```
We also define the following view class:

```typescript
class View<M> {
    constructor(private user: User) {
        this.user.on('change:title', () => {
            this.showAlert();
        });
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}
```

Then in some other class's method we instantiate the view with a referenced user model:
```typescript
class SuperView{
    showSubView() {
        this.subView = new View(this.user);
        this.subView = null; // this.user persists.
        // A memory leak, `view` cannot be garbage collected.
    }
}
```
Did you spot what was causing the memory leak? It is on this line:
```typescript
this.user.on('change:title', () => {
    this.showAlert(); // `this` is referencing view. So `this.user` is referencing `view`.
});
```

### Proposal

We want to prevent the memory leak by static code analysis. And I propose the following syntax:

```typescript
toggle UserChangelTitle;

class View<M> {

    on UserChangeTitle
    constructor(private user: User) {
        this.user.on('change:title', this.showAlert);
    }
    
    off UserChangeTitle
    public removeUser() {
        this.user = null;
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}
```
Whenever you toogle on something you must toogle it off. Otherwise the compiler won't compile. In our previous example, our code would not compile because we only toogle `UserChangeTitle` `on`. But we never toggle it `off`.

```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView = null;
    }
}
```

Just adding the call expression `this.subView.removeUser()` below. Will turn the toogle `off`. Now, on the same scope we have a matching `on` and `off` toogles. So the compiler will compile the following code.
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
    }
}
```
But if we don't add the toggle `off` expression above. Our code would be inferred as:

```typescript
class SuperView {
	// on UserChangeTitle
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView = null;
    }
}
```
The `on` toggle is inferred on `showSubView()`, by the following expression `new View(this.user)`. Whenever there is no matching `off` toggle, a containing method or function will have an inferred toggle.

We previously also said that we could add one `off` toggle to the same scope as the `on` toggle. But we can also define an another method that balances the toogle. Now, the method is inferred as:

```typescript
class SuperView {
	// on UserChangeTitle
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
    }
	
	// off UserChangeTitle
	removeSubView() {
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
	}
}
```
This also tells the compiler, the toggle management should be off loaded to an another location and not inside `showSubView()`. This is because we balanced it with an `off` toggle. 

### Callbacks

We have so far only considered object having an instant death. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler will not complain.

Passing off toggle method as callback argument will match an on toggle.
```typescript
class SuperView {
	// no inferred on toggle
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
		this.onDestroy(this.removeSubView); // Passing an off toggle callback to a call expression also matches the on toggle above.
    }
	
	onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	// off UserChangeTitle
	removeSubView() {
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
	}
}
```
Now, we have ensured a possible death of our `subView`, because `this.removeSubView` has an inferred `off` toggle:
```typescript
	this.subView = new View(this.user); // Turns the toggle on.
	this.onDestroy(this.removeSubView); // `this.onDestroy` takes a certain callback. And we passed in a off toggle callback. Which mean we have a certain death for our `subView`.
```

### Mutiple references

We some times, need to deal with multiple references of the same toggle.

```typescript

import { View } from './view';

class SuperView {
	private subView: View;

	// no inferred on toggle
    showSubView() {
		on UserChangeTitle as UserChangeTitleOnSubView
        this.subView = new View(this.user); // Turns the toggle on.
		on UserChangeTitle as UserChangeTitleOnAnotherSubView
        this.anotherSubView = new View(this.user); // Turns the toggle on.
    }
	
	onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	removeSubView() {
		off UserChangeTitle as UserChangeTitleOnSubView
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
	}
	
	removeAnotherSubView() {
		off UserChangeTitle as UserChangeTitleOnAnotherSubView
        this.anotherSubView.removeUser(); // Turns the toggle off.
        this.subView = null;
	}
}
```

When dealing with collection, it is good practice to have already balanced constructors/methods.
```typescript
import { View } from './view';

class SuperView {
	private subViews: View[] = [];
    showSubViews() {
		for (let i = 0; i < 10; i++) {
        	this.subViews.push(new View(this.user)); // The constructor is already balanced. So the compiler will not complain.
		}
    }
}
```
For unbalanced methods the following code will not compile:

```typescript
class SuperView {
	private subViews: View[] = [];
	
    showSubViews() {
		for (let i = 0; i < 10; i++) {
        	this.subView.push(new View(this.user));
		}
    }
}
```

We would need add a named collection toggle for this.
```typescript
class SuperView {
	private subViews: View[] = [];
	
    showSubViews() {
		for (let i = 0; i < 10; i++) {
			on UserChangelTitle as UserChangelTitles // We name a collection of toggles as UserChangelTitles
        	this.subView.push(new View(this.user));
		}
    }
}
```
A collection of toggles is denoted as `UserChangeTitle[]` and there is no assurance of the length of a collection. The above code needs to be balanced. We haven't defined an off toggle yet.
```typescript
class SuperView {
	private subViews: View[] = [];
	
    showSubViews() {
		for (let i = 0; i < 10; i++) {
			on UserChangelTitle[] as UserChangelTitles // We name a collection of toggles as UserChangelTitles
        	this.subView.push(new View(this.user));
		}
		this.onDestroy(this.removeSubViews);
    }
	
	onDestroy(callback: () => void) {
		this.deleteAllButton.addEventListener(callback); 
	}
	
	removeSubViews() {
		for (let i = 0; i < 10; i++) {
			off UserChangeTitle[] as UserChangelTitles
        	this.subView[i].removeUser();
		}
		this.subView = [];
    }
}
```
Not naming the above collection toggles works as well if you only have one collection of toggles. 

Note, there is no compile error, even though the for loop in `removeSubViews` is not matched with `showSubView`.

```typescript
for (let i = 0; i < 9; i++) { Loop only 9 elements and not 10 causes another memory leak.
	off UserChangeTitle[] as UserChangelTitles
	this.subView[i].removeUser();
}
```
This is because it is very difficult to that kind of assertion. Though we match the symbol of `UserChangelTitles`. That alone gives some safety.