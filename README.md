Static Code Analyzation and Prevention of Memory Leaks
====================

Software has been daunted with memory leaks for a long time. There exists one interesting question to ask, can we make memory management more safe with static code analysis? Can we make a compiler help us with common mistakes made, when dealing with memory management?

# Table of Contents
* [Defintiion](#definition)
* [Memory Mistakes](#example)
  * [Event Emitter Example](#event-emitter-example) 
* [Toogle Annotation](#proposal)
  * [Syntax](#syntax)
  * [Inheritance](#inheritance)
  * [Callback](#callback)

# Definition
A memory leak is objects we intended to delete. But instead of being deleted, they remained on the runtime.

# Memory Mistakes
Long runnning applications needs to allocate memory to store objects that lives a long time. Though, during allocation and storing of objects a developer might forget to handle the case when the object is no longer needed and it needs to be deleted. Even though, the developer remembers to handle the deletion of objects, there still exists blind spots where the reference count of objects does not reach zero and thus creates a memory leak in a garbage collected language or languages that uses reference counted smart pointers. We will try to cover some of these problems and present a solution to these problems.

# Example
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

# Toggle Annotation

We want to prevent the memory leak by static code analysis. But, in doing so we must analyse the source of memory leaks. By definition a memory leak is an unused resource at runtime. We allocate memory and initialize our resource. When the resource is no longer needed we need to deallocate it. In a garbage collected language we can unreference objects so they get garbage collected. And for a manual manage memory programming languages, we must deallocate it manually by writing some sort of expressions. In a majority of cases, if not all, a memory leaked resource often has one or more references to itself. In a garbage collected language this always holds true, they always have at least one reference to itself(otherwise they would be garbage collected). 

I propose, that methods that uses these references, to be annotated. I have not shown you the `EventEmitter` class yet and lets begin by showing it to you:

```typescript
export class EventEmitter {
    public eventCallbackStore: EventCallbackStore = {}

    public register(event: string, callback: Callback) {
        if (!this.eventCallbackStore[event]) {
            this.eventCallbackStore[event] = [];
        }
        this.eventCallbackStore[event].push(callback);
    }

    public unregister(event: string, callback: Callback): void {
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
The `eventCallbackStore` above is hashmap of a list of callbacks for each event. We register new events with the `on` method and unregister them with the `unregister` method. We can emit a new event with the `emit` method. The property `eventCallbackStore` is a potential leaking resource storage, because it can hold callbacks on events and a developer might forgot to unregister some of those events when no longer needed. Though the essentials here, is the `register` and `unregister` methods. Because their role is to register and unregister events. This leads us to think, can we somehow require a user who calls `register` always call `unregister`? If possible, we would prevent having any memory leaks. Let us answer this question later, and begin with annotating them first. 
I propose in this case, the following annotation syntax:
```
[on|off] IDENTIFIER
```
The `on` and `off` is an operator that annotates methods with an toogle identifier. So for our `User` model which is an extension of the `EventEmitter` class, we go ahead and annotate the method with `on` and `off`.
```typescript
export class User extends EventEmitter {
    on UserChangelTitle
    public register(event: string, callback: Callback) {
        super.register.apply(this, arguments);
    }
    
    off UserChangelTitle
    public unregister(event: string, callback: Callback): void {
        super.unregister.apply(this, arguments);
    }
}
```
Now, every consumer of these two methods will have some additional checks that they need to pass. First, if they are in the same scope they need to call `register` before `unregister`. 
```typescript
class View<M> {
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
        this.user.unregister('change:title', this.showAlert);
    }
}
```
Though, in this case having the method calls on the same scope is not quite useful. Since we unregister the event directly. It would be as good as not calling anything at all. But in order to pass the compiler checks we can also call the `off` toogle in another method. The `off` toogle is the `off` annotated method, in this case the `unregister` method. And vice versa for the `on` toogle.
```typescript
class View<M> {
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
    }
    
    public removeUser() {
        this.user.unregister('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}
```
### Inheritance
Notice first, that whenever there is a scope with an unmatched `on` or `off` toggles. The unmatched toogle annotates the containing method. Here we show the inherited annotation in comments below:
```typescript
class View<M> {
    // on UserChangeTitle
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
    }
    
    // off UserChangeTitle
    public removeUser() {
        this.user.unregister('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}
```
The methods in our class is now balanced. This leads us to our second rule. A class's method's needs to have balanced toogle annotations. And when a class is balanced it implicitly infers that the a balance check should be done in an another scope other than in the current class's methods. This could be an another class's method that uses this class's `on` toggle.

Now, let use the abve class in a class we call `SuperView`:
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // This call has an on toggle.
        this.subView = null;
    }
}
```
The above code does not pass the compiler check, because there is no matching `off` toogle. Also the code causes a memory leak.

Just adding the call expression `this.subView.removeUser()` below, will match our `on` toogle. Now, on the same scope we have a matching `on` and `off` toogles. So the compiler will compile the following code. Also, the code, causes no memory leaks:
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // On toggle.
        this.subView.removeUser(); // Off toggle.
        this.subView = null;
    }
}
```
But if we don't add the toggle `off` expression above. The containing method will be annotated as:
```typescript
class SuperView {
	// on UserChangeTitle
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView = null;
    }
}
```
The method `showSubView` inherited the `on` toggle from the expression `new View(this.user)`. This inheritance loop goes on and on.

### Callbacks

We have so far only considered object having an instant death. And this is not so useful for our application. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler will not complain. Now this leads us to our next rule:

Passing an `off` toggle method as callback argument will match an `on` toggle in current scope.
```typescript
class SuperView {
	// no inferred on toggle
    showSubView() {
        this.subView = new View(this.user); // On toggle.
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
Now, we have ensured a possible death of our `subView`, because `this.removeSubView` has an inhertied `off` toggle:
```typescript
	this.subView = new View(this.user); // On toggle.
	this.onDestroy(this.removeSubView); // `this.onDestroy` takes a callback. And we passed in a off toggle. Which mean we have a possible death for our `subView`.
```

The scope is matched and the compiler will not complain. Notice whenever `on` toggle is matched with an `off` toggle directly or whenever there is a path(call path) that can be reached, to match an `on` toggle. The code will pass the check. Because in other words, we have ensured a possible death of our allocated resource.
```
BIRTH ---> DEATH
BIRTH ?---> CALL1 ?---> CALL2 ?---> CALLN ?---> DEATH
```
Notice that we say possible death and not certain death. We will get back to this later.

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
### Collections
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
			on UserChangelTitle as UserChangeTitles // We name a collection of toggles as UserChangeTitles
        	this.subView.push(new View(this.user));
		}
    }
}
```
A collection of toggles is denoted as `UserChangeTitle` and there is no assurance of the length of a collection. The above code needs to be balanced. We haven't defined an off toggle yet.
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
