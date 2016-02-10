Static Code Analyzation and Prevention of Memory Leaks
====================

Software has been daunted with memory leaks for a long time. There exists one interesting question to ask, can we make memory management more safe with static code analysis? Can we make a compiler help us with common mistakes made, when dealing with memory management?

# Contents
* [Defintiion](#definition)
* [Common Patterns](#problem)
  * [Event Emitter Pattern](#event-emitter-pattern)
* [Proposal](#proposal)

# Definition
A memory leak is objects we intended to delete. But instead of being deleted, they remained on the runtime.

# Memory Mistake
Long runnning applications needs to allocate memory to store objects that lives a long time. Though, during allocation and storing of objects a developer might forget to handle the case when the object is no longer needed and it needs to be deleted. Even though, the developer remembers to handle the deletion of objects, there still exists blind spots where the reference count of objects does not reach zero and thus creates a memory leak in a garbage collected language or languages that uses reference counted smart pointers. We will try to cover some of these problems and present a solution to these problems.

# Common Patterns
We will try to describe common patterns for memory leaks. We use TypeScript as our programming language.

#### Event Emitter Pattern
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
As you can see we did a mistake. We unreferenced the sub view. And we expected it to be garbage collected. But instead we caused a memory leak. Did you spot in which line was causing a memory leak? It is on this line:
```typescript
this.user.on('change:title', () => {
    this.showAlert(); // `this` inside the closure is referencing view. So `this.user` is referencing `view`.
});
```
As the comment says, `this` inside the closure is referencing  the view. So `this.user` is referencing `view`. Because the reference count haven't reached zero, the garbabge collector cannot garbage collect the sub view.

### Proposal

We want to prevent the memory leak by static code analysis. But, in doing so we must analyse the source of memory leaks. By definition a memory leak is an unused resource on runtime. We allocate memory and initialize our resource. When the resource is no longer needed we need to deallocate it. In a garbage collected language we can unreference objects so they get garbage collected. And for a manual manage memory programming languages, we must deallocate it manually by writing some sort of expressions. In a majority of cases, if not all, a memory leaked resource often has one or more references to itself. In a garbage collected language they always have at least one reference to itself(otherwise they would be garbage collected). 

I propose, that methods that uses these references, to be annotated. I have not shown you the `EventEmitter` class yet and lets begin by showing it to you first:

```typescript
export class EventEmitter {
    public eventCallbackStore: EventCallbackStore = {}

    public on(event: string, callback: Callback) {
        if (!this.eventCallbackStore[event]) {
            this.eventCallbackStore[event] = [];
        }
        this.eventCallbackStore[event].push(callback);
    }

    public off(event: string, callback: Callback): void {
        let callbacks = this.eventCallbackStore[event].length;
        for (let i = 0;i < callback.length; i++) {
            if (this.eventCallbackStore[event][i] === callback) {
                this.eventCallbackStore[event].splice(i, 1);
            }
        }
    }

    public emit(event: string, args: any[]) {
        if (this.eventCallbackStore[event]) {
            for (let callback of this.eventCallbackStore[event]) {
                callback.apply(null, args);
            }
        }
    }
}
```
The `eventCallbackStore` above is hashmap of a list of callbacks for each event. We register new events with the `on(...)` method and unregister them with the `off(...)` method. We can emit a new event with the `emit(...)` method. The property `eventCallbackStore` is a potential leaking resource, because it can hold callbacks on events and a developer might forgot to unregister some of those events when no longer needed. Though the essentials here, is the `on` and `off` method. Because their role is to register and unregister events. This leads us to think, can we somehow require a user who calls `on` always call `off`? Let us answer this question later, and begin with annotating them first. 
I propose in this case, the following annotation syntax:
```
[on|off] IDENTIFIER
```
The `on` and `off` is an operator that annotates methods with an toogle identifier. So for our `User` model which is an extension of the `EventEmitter`, we go ahead and annotate the method with `on` and `off`.
```typescript
export class User extends EventEmitter {
    on UserChangelTitle
    public on(event: string, callback: Callback) {
        super.on.apply(this, arguments);
    }
    
    off UserChangelTitle
    public off(event: string, callback: Callback): void {
        super.off.apply(this, arguments);
    }
}
```

Now, every consumer of these two methods will have some additional checks that they need to pass. First, if they are in the same scope they need to call `on` first before `off`. 

```typescript
class View<M> {
    constructor(private user: User) {
        this.user.on('change:title', this.showAlert);
        this.user.off('change:title', this.showAlert);
    }
}
```
Though, in this case having the method calls on the same scope is not quite useful. Since we unregister the event directly. It would be as good as not calling anything at all. But in order to pass the compiler check we can also call the `off` toogle in another method. The `off` toogle is the `off` annotated method.

```typescript
class View<M> {
    constructor(private user: User) {
        this.user.on('change:title', this.showAlert);
    }
    
    public removeUser() {
        this.user.off('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}
```
Notice first, that when 

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

We previously also said, that we could add one `off` toggle to the same scope as the `on` toggle. But we can also define an another method that balances the toogle. Now, the method is inferred as:

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

Passing an off toggle method as callback argument will match an on toggle.
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

We can alias the toggles. Having multiple togggles with the same name will generate a compile error.
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

We would need to add a named collection of toggles for this.
```typescript
class SuperView {
	private subViews: View[] = [];
	
    showSubViews() {
		for (let i = 0; i < 10; i++) {
			on UserChangelTitle[] as UserChangeTitles // We name a collection of toggles as UserChangeTitles
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
			on UserChangelTitle[] as UserChangeTitles // We name a collection of toggles as UserChangeTitles
        	this.subView.push(new View(this.user));
		}
		this.onDestroy(this.removeSubViews); // This line tells the compiler the toggle(memory) management shoud be done here.
    }
	
	onDestroy(callback: () => void) {
		this.deleteAllButton.addEventListener(callback); 
	}
	
	removeSubViews() {
		for (let i = 0; i < 10; i++) {
			off UserChangeTitle[] as UserChangeTitles
        	this.subView[i].removeUser();
		}
		this.subView = [];
    }
}
```
Not naming the above collection of toggles works as well if you only have one collection of toggles.

Note, there is no compile error, even though the for loop in `removeSubViews` is not matched with `showSubView`.

```typescript
for (let i = 0; i < 9; i++) {// Loop only 9 elements and not 10 causes another memory leak.
	off UserChangeTitle[] as UserChangeTitles
	this.subView[i].removeUser();
}
```
This is because it is very difficult to do that kind of assertion. Though, we match the symbol of `UserChangeTitles` and that alone gives us some safety.
