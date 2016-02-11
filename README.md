Static Code Analyzation and Prevention of Memory Leaks
====================

# Abstract
Software has been daunted with memory leaks for a long time. There exists one interesting question to ask, can we make memory management more safe with static code analysis? Can we make a compiler help us with common mistakes made, when dealing with memory management?

# Table of Contents
* [Abstract](#abstract)
* [Memory Leaks](#memory-leaks)
  * [Defintiion](#definition)
  * [Example](#example) 
* [Add-Sub Annotation](#add-sub-annotation)
  * [Method Annotation](#syntax)
  * [Inheritance](#inheritance)
  * [Callbacks](#callbacks)
  * [Multiple References](#multiple-references)
  * [Collections](#collections)
    * [Unsafe add-sub collection methods](#unsafe-add-sub-collection-methods)
  * [Control Flow](#control-flow)
  * [Final Syntax](#final-syntax)
    * [Toggle Definition](#toggle-definition)
    * [On Toggle Example](#on-toggle-example)
    * [Off Toggle Example](#off-toggle-example)
    * [False Toggle Example](#false-toggle-example)
  * [Heap Object Graph](#heap-object-graph)

# Memory Mistakes
Long runnning applications needs to allocate memory to store objects that lives a long time. Though, during allocation and storing of objects a developer might forget to handle the case when the object is no longer needed and it needs to be deleted. Even though, the developer remembers to handle the deletion of objects, there still exists blind spots where the reference count of objects does not reach zero and thus creates a memory leak in a garbage collected language or languages that uses reference counting. We will try to cover some of these problems and present a solution to these problems.

## Definition
A memory leak is objects we intended to delete. But instead of being deleted, they remained on runtime.

## Example
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

# Add-Sub Annotations

We want to prevent the memory leak by static code analysis. But, in doing so we must analyse the source of memory leaks. By definition a memory leak is an unused resource at runtime. We allocate memory and initialize our resource. When the resource is no longer needed we need to deallocate it. In a garbage collected language we can unreference objects so they get garbage collected. And for a manual managed memory programming languages, we must deallocate it manually by writing some sort of expressions. In a majority of cases, if not all, a memory leaked resource often has one or more references to itself. In a garbage collected language this always holds true, they always have at least one reference to itself(otherwise they would be garbage collected). 

Let us just annotate these methods that uses these references. I have not shown you the `EventEmitter` class yet and lets begin by showing it to you:

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
The `eventCallbackStore` above is hashmap of a list of callbacks for each event. We register new events with the `register` method and unregister them with the `unregister` method. We can emit a new event with the `emit` method. The property `eventCallbackStore` is a potential leaking resource storage, because it can hold callbacks on events and a developer might forgot to unregister some of those events when no longer needed. Though the essentials here, is the `register` and `unregister` methods. Because their role is to register and unregister events. This leads us to think, can we somehow require a user who calls `register` always call `unregister`? If possible, we would prevent having any memory leaks. Let us answer this question later, and begin with annotating them first. 

## Method Annotation
I propose in this case, the following annotation syntax:
```
[add|sub] NAME
```
The `add` and `sub` is an operator that annotates methods with a name that identifies the resouce being added or subtracted. So for our `User` model which is an extension of the `EventEmitter` class, we go ahead and annotate the method with `add` and `sub`.
```typescript
export class User extends EventEmitter {
    add UserChangeTitleCallback
    public register(event: string, callback: Callback) {
        super.register.apply(this, arguments);
    }
    
    sub UserChangeTitleCallback
    public unregister(event: string, callback: Callback): void {
        super.unregister.apply(this, arguments);
    }
}
```
Now, every consumer of these two methods will have some additional checks that they need to pass. First, if they are in the same scope they need to call `register` before `unregister`. Or in other words an `add` annotated method needs to be called before a `sub` annotated method. And through this document we would refer an `add` annotated method as an add method. And a sub annotated method as a sub method.

```typescript
class View<M> {
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
        this.user.unregister('change:title', this.showAlert);
    }
}
```
Though, in this case having the method calls on the same scope is not quite useful. Since we unregister the event directly. It would be as good as not calling anything at all. But in order to pass the compiler checks we can also call a sub method in another method.

```typescript
class View<M> {
    private description = 'This is the view of: ';
    
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
    }
    
    public removeUser() {
        this.user.unregister('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(this.description + title);
    }
}
```
In the above example. We call `this.user.unregister('change:title', this.showAlert);` to pass the compiler check.

## Inheritance
Notice first, that whenever there is a scope with an unmatched add or sub methods. The unmatched methods annotates the containing method. Here we show the inherited annotation in the comments below:
```typescript
class View<M> {
    private description = 'This is the view of: ';
    
    // add UserChangeTitleCallback
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
    }
    
    // sub UserChangeTitleCallback
    public removeUser() {
        this.user.unregister('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(this.description + title);
    }
}
```
The methods in our class is now balanced. This leads us to our second rule. A class's method's needs to have balanced method annotations. A balanced annotation is two methods of which one of them having an add method and another having a corresponding sub method. And when a class is balanced it implicitly infers that the a balance check should be done in an another scope other than in the current class's methods. This could be an another class's method that uses the class's add method.

Now, let use the abve class in a class we call `SuperView`:
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // This call is an add method
        this.subView = null;
    }
}
```
The above code does not pass the compiler check, because there is no matching sub method. Also the code causes a memory leak.

Just adding the call expression `this.subView.removeUser()` below, will match our add annotated method. Now, on the same scope we have a matching add and sub methods. So the compiler will compile the following code. Also, the code, causes no memory leaks:
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // Add constructor.
        this.subView.removeUser(); // Sub method.
        this.subView = null;
    }
}
```
If we don't add a sub method call above. The containing method will be annotated as an add method:
```typescript
class SuperView {
	// add UserChangeTitleCallback
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView = null;
    }
}
```
The method `showSubView` inherited the `add` annotation from the expression `new View(this.user)`. This inheritance loop goes on and on.

## Callbacks

We have so far only considered object having an instant death. And this is not so useful. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler checks will pass. Now, this leads us to our next rule:

Passing a sub method method as a callback argument will balance an add method in current scope:
```typescript
class SuperView {
    showSubView() {
        this.subView = new View(this.user); // Add method.
		this.onDestroy(this.removeSubView); // Passing a sub method to a call expression matches the add method above.
    }
	
	onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	// sub UserChangeTitleCallback
	removeSubView() {
        this.subView.removeUser(); // Sub method.
        this.subView = null;
	}
}
```
Now, we have ensured a possible death of our `subView`, because `this.removeSubView` has an inherited a `sub` annotation:
```typescript
	this.subView = new View(this.user); // Has an 'add' annotation.
	this.onDestroy(this.removeSubView); // `this.onDestroy` takes a callback. And we passed in a 'sub' annotated method. Which mean we have a possible death for our `subView`.
```

The scope is balanced and the compiler will not complain. Notice, whenever an add method is matched with an sub method directly or whenever there is a path(call path) that can be reached, to match an sub method. The code will pass the compiler check. Because in other words, we have ensured a possible death of our allocated resource.
```
BIRTH ---> DEATH
BIRTH ?---> CALL1 ?---> CALL2 ?---> CALLN ?---> DEATH
```
Notice, that we say a possible death and not a certain death. We will get back to this later.

## Multiple References

We some times, need to deal with multiple references of the same class of objects. The compiler will not pass the code if there is two annotations that have the same name. This is because we want to associate one type of allocation/deallocation of resource with one identifier. This will make code more safe, because one type of allocation cannot be checked against another type of deallocation.

In order to satisfy our compiler we would need to give our annotations some aliases. And the syntax for aliasing an annotation is:
```
[add|sub] NAME as ALIAS
```
Lets go ahead and these annotations:
```typescript

import { View } from './view';

class SuperView {
	private subView: View;

	// add UserChangeTitleEventCallbackOnSubView
	// add UserChangeTitleEventCallbackOnAnotherSubView
    showSubView() {
		add UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnSubView
        this.subView = new View(this.user); // Add method.
		add UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnAnotherSubView
        this.anotherSubView = new View(this.user); // Add method.
        
	    // sub UserChangeTitleEventCallbackOnSubView
	    // sub UserChangeTitleEventCallbackOnAnotherSubView
        this.onDestroy(() => {
            this.removeSubView();  // Sub method.
            this.removeAnotherSubView();  // Sub method.
        });
    }
	
	onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	// sub UserChangeTitleEventCallbackOnSubView
	removeSubView() {
		sub UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnSubView
        this.subView.removeUser(); // Sub method.
        this.subView = null;
	}
	// off UserChangeTitleEventCallbackOnAnotherSubView
	removeAnotherSubView() {
		off UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnAnotherSubView
        this.anotherSubView.removeUser(); // Sub method.
        this.subView = null;
	}
}
```
Notice, that the anonymous lambda function will inherit the annotations:
```typescript
// sub UserChangeTitleEventCallbackOnSubView
// sub UserChangeTitleEventCallbackOnAnotherSubView
this.onDestroy(() => {
    this.removeSubView();  // Sub method.
    this.removeAnotherSubView();  // Sub method.
});
```
This is due to the fact that the lambda's function's scope does not have matching add method calls for the current sub method calls. Also, since `this.onDestroy` is a method which accept callbacks, it will balance the containing scope:
```typescript
add UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnSubView
this.subView = new View(this.user); // 'add' annotated
add UserChangeTitleEventCallback as UserChangeTitleEventCallbackOnAnotherSubView
this.anotherSubView = new View(this.user); // 'add' annotated
```
So in other words, The above code will compile. It also causes no memory leaks.

### Collections
When dealing with collection, it is good practice to have already balanced method calls on constructors or methods.
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
Because we don't need to deal with an even more complex matching of annotations.

For various reason, if one cannot use balanced methods/constructors. The annotation conventions works as before. An unmatched add or sub method calls annotates the containing method. Even on a for loop below:
```typescript
class SuperView {
	private subViews: View[] = [];
	
	// add UserChangeTitleEventCallback
    showSubViews() {
		for (let i = 0; i < 10; i++) {
		    add UserChangeTitleEventCallback as UserChangeTitleEventCallbacks
        	this.subView.push(new View(this.user));
		}
    }
}
```
As before, we would need to have a matching sub method to satisfy our compiler:
```typescript
class SuperView {
	private subViews: View[] = [];
	
	// sub UserChangeTitleEventCallbacks
	removeSubViews() {
		for (let i = 0; i < 10; i++) {
		    sub UserChangeTitleEventCallback as UserChangeTitleEventCallbacks
        	this.subView[i].removeUser();
		}
		this.subView = [];
    }
}
```
In the above example, `this.subView[i].removeUser();` is a sub method and since there are no add method that balances it. It annotates the containing method `removeSubViews`.

Now, our whole class will look like this:
```typescript
class SuperView {
    private subViews: View[] = [];
    
	// add UserChangeTitleEventCallback
    showSubViews() {
		for (let i = 0; i < 10; i++) {
		    add UserChangeTitleEventCallback as UserChangeTitleEventCallbacks
        	this.subView.push(new View(this.user));
		}
    }

    onDestroy(callback: () => void) {
        this.deleteAllButton.addEventListener(callback); 
    }

	// sub UserChangeTitleEventCallbacks
	removeSubViews() {
		for (let i = 0; i < 10; i++) {
		    sub UserChangeTitleEventCallback as UserChangeTitleEventCallbacks
        	this.subView[i].removeUser();
		}
		this.subView = [];
    }
}
```

### Unsafe Add-Sub Collection Methods
Note, there is no compile error, even though the for loop in `removeSubViews` is not matched with `showSubView`. Like for instance, if we would have only looped 9 loops instead of 10:
```typescript
for (let i = 0; i < 9; i++) {// Loop only 9 elements and not 10 causes another memory leak.
	off UserChangeTitle[] as UserChangeTitles
	this.subView[i].removeUser();
}
```
This is because it is very difficult to do that kind of assertion. A compiler cannot know the business logic of your application and thus cannot make that assertion. Though, we match the symbol of `UserChangeTitles` and that alone gives us some safety.

## Control Flow
## Final Syntax
One could ask why not annotating the source of the memory leak directly instead of annotating the methods? It's possible and it is even more safer. Lets revisit our `EventEmitter` class:

```typescript
export class EventEmitter {
    public eventCallbackStore: EventCallbackStore = {}
}
```
Our general concept so far, is that for every method for adding objects, there should exists at least one matching method for subtraction of objects. We can safely say that new assignements will increase the elements count. But also, in the event emitter case, array element additions. With static code analysis we can also make sure that elements are added or subtracted. Before it was upto the developer to manually annotate and implement the method. But nothing stops the developer to implement a method with an `off` toggle which increases the element count.

### Toggle Definition
For the toggle methods the following holds true:

```
On toogle methods: Adds 0 or more elements .
Off toggle methods: Subtracts 0 or more elements.
```
And let it be our definitions for our toggle methods.

So if we annotate our `eventCallbackStore` with:
```typescript
export class User extends EventEmitter {
    toggle UserEvent
    public eventCallbackStore: EventCallbackStore = {}
}
```

### On Toggle Example
We can staticly analyse the `register` method, which is suppose to be an `on` toggle:
```typescript
public register(event: string, callback: Callback) {
    if (!this.eventCallbackStore[event]) {
        this.eventCallbackStore[event] = []; 
    }
    this.eventCallbackStore[event].push(callback);
}
```
There is two expressions above that adds elements to the store:
```typescript
if (!this.eventCallbackStore[event]) {
    this.eventCallbackStore[event] = [];
}
```
An assignment adds 0 or 1 new elements to the store.

And the following `push` method adds one element to the store:
```typescript
this.eventCallbackStore[event].push(callback);
```
So in all, we can safely say that the method adds 0 or more elements to the store `eventCallbackStore`. And it satisfies our `on` toggle definition.


### Off Toggle Example

Lets examine our `unregister` method:
```typescript
public unregister(event: string, callback: Callback): void {
    let callbacks = this.eventCallbackStore[event].length;
    for (let i = 0;i < callback.length; i++) {
        if (this.eventCallbackStore[event][i] === callback) {
            this.eventCallbackStore[event].splice(i, 1);
        }
    }
}
```
We can statically confirm that this method subtracts 0 or 1 element from our store(even though it is encapsulated in a for-loop and an if statement block):
```typescript
this.eventCallbackStore[event].splice(i, 1);
```
So the `unregister` satisfies our `off` toggle definition. 

### False Toggle Example

Lets also examine an false toogle example. Lets take our `emit` method as an example:

```typescript
public emit(event: string, args: any[]) {
    if (this.eventCallbackStore[event]) {
        for (let callback of this.eventCallbackStore[event]) {
            callback.apply(null, args);
        }
    }
}
```
There is no expression in above that increases our elements count in our store. There exists index look up such as `this.eventCallbackStore[event]`, though they don't add any elements. So we can safely say that this method does not satisfy any of our toggle definitions.

### Types of storage
We only considered a hash map so far. Though any type that can grow the heap can be annotated.

## Heap Object Graph

When we arrived at our final syntax. We discovered that we could annotate a property and let static analysis discover our toggle methods. Let us illustrate what this means:

When we have a memory leak. Essentially what it means is that our heap object graph can grow to an infinite amount of nodes, starting in some node (we use a tree instead of a graph below):

![Heap object infinity nodes](https://raw.githubusercontent.com/tinganho/a-toggle-modifier-proposal/master/HeapObjectTreeHorizontalInfinity%402x.jpg)

And we want to statically annotate that any consumer of this node needs to call an `add` and `sub` annotated methods:

![Heap object infinity with on and off toggles](https://raw.githubusercontent.com/tinganho/a-toggle-modifier-proposal/master/HeapObjectTreeHorizontalOnOff%402x.jpg)
