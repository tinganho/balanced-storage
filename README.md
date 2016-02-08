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
export toggle UserChangelTitle;

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
But if we don't add the toggle `off` expression above. Our code it would be inferred as:

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

Because the above code would not compile, we previously also said that we could add one `off` toggle to the same scope as the `on` toggle. But we can also define an another method that balances the toogle. Now, the method is inferred as:

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
It also tells the compiler, the toggle management should be off loaded to an another location and not inside `showSubView()`. This is because we balanced it with an `off` toggle. 

### Callbacks

We have so far only considered object having an instant death. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler will not complain.

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