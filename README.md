Static Typing for Memory Leak Prevention
====================

# Introduction
Software has been daunted with memory leaks for a long time. There exists one interesting question to ask, can we make memory management more safe with static code analysis? Can we make a compiler help us with common mistakes made, when dealing with memory management?

# Table of Contents
* [Introduction](#abstract)
* [Memory Leaks](#memory-leaks)
  * [Definition](#definition)
  * [Example](#example) 
* [Add-Sub Method Classification](#add-sub-method-classification)
  * [Method Classification](#method-classification)
  * [Inheritance](#inheritance)
  * [Call Paths](#call-paths)
  * [Aliasing](#aliasing)
  * [Control Flow](#control-flow)
  * [Balanced Storage Annotation](#balanced-storage-annotation)
    * [Add-Sub Method Definition](#add-sub-method-definition)
    * [Syntax](#syntax)
    * [Add Method Example](#add-method-example)
    * [Sub Method Example](#sub-method-example)
    * [False Add-Sub Method Example](#false-add-sub-method-example)
  * [Heap Object Graph](#heap-object-graph)

# Memory Leaks

Long runnning applications needs to allocate memory to store objects that lives a long time. Though, during allocation and storing of objects a developer might forget to handle the case when the object is no longer needed and it needs to be deleted. Even though, the developer remembers to handle the deletion of objects, there still exists blind spots where the reference count of objects does not reach zero and thus creates a memory leak in a garbage collected language or languages that uses reference counting. We will try to cover some of these problems and present a solution to these problems.

## Definition
A memory leak is objects we intended to delete. But instead of being deleted, they remained on runtime.

## Example

We extend an EventEmitter class to create a user model:

```ts
class User extends EventEmitter {
    private title: string;
    
    public setTitle(title: string) {
        this.title = title;
        this.emit('change:title', title);
    }
}
```

We also define the following view class:

```ts
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

```ts
class SubView {
    show() {
        this.view = new View(this.user);
        this.view = null; // this.user persists.
        // A memory leak, `view` cannot be garbage collected.
    }
}
```

As you can see we did a mistake. We unreferenced the sub view. And we expected it to be garbage collected. But instead we caused a memory leak. Did you spot in which line was causing a memory leak? It is on this line:

```ts
this.user.on('change:title', () => {
    this.showAlert(); // `this` inside the closure is referencing view. So `this.user` is referencing `view`.
});
```

As the comment says, `this` inside the closure is referencing  the view. So `this.user` is referencing `view`. Because the reference count haven't reached zero, the garbabge collector cannot garbage collect the sub view.

# Add-Sub Method Classification

We want to prevent the memory leak by static code analysis. But, in doing so we must analyse the source of memory leaks. By definition a memory leak is an unused resource at runtime. We allocate memory and initialize our resource. When the resource is no longer needed we need to deallocate it. In a garbage collected language we can unreference objects so they get garbage collected. And for a manual managed memory programming languages, we must deallocate it manually by writing some sort of expressions. In a majority of cases, if not all, a memory leaked resource often has one or more references to itself. In a garbage collected language this always holds true, they always have at least one reference to itself(otherwise they would be garbage collected). 

Let us just annotate these methods that uses these references. I have not shown you the `EventEmitter` class yet and lets begin by showing it to you:

```ts
export class EventEmitter {
    public eventCallbacks: EventCallbacks = {}

    public register(event: string, callback: Callback) {
        if (!this.eventCallbacks[event]) {
            this.eventCallbacks[event] = [];
        }
        this.eventCallbacks[event].push(callback);
    }

    public unregister(event: string, callback: Callback): void {
        let callbacks = this.eventCallbacks[event].length;
        for (let i = 0;i < callback.length; i++) {
            if (this.eventCallbacks[event][i] === callback) {
                this.eventCallbacks[event].splice(i, 1);
            }
        }
    }

    public emit(event: string, args: any[]) {
        if (this.eventCallbacks[event]) {
            for (let callback of this.eventCallbacks[event]) {
                callback.apply(null, args);
            }
        }
    }
}
```

The `eventCallbacks` above is hashmap of a list of callbacks for each event. We register new events with the `register` method and unregister them with the `unregister` method. We can emit a new event with the `emit` method. The property `eventCallbacks` is a potential leaking resource storage, because it can hold callbacks on events and a developer might forgot to unregister some of those events when no longer needed. Though the essentials here, is the `register` and `unregister` methods. Because their role is to register and unregister events. This leads us to think, can we somehow require a user who calls `register` always call `unregister`? If possible, we would prevent having any memory leaks. Let us answer this question later, and begin with annotating them first. 

## Method Classification

Lets just the add a temporary classification syntax for our methods:

<pre>
<i>MethodClassifiction ::</i> <b>add</b> | <b>sub</b> <i>Name MethodDeclaration</i>
</pre>

The `add` and `sub` keywords are operators that classify methods with a name that identifies that elements is being added or subtracted when the method is called. So for our `User` model which is an extension of the `EventEmitter` class, we go ahead and classify our methods.

```ts
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

Now, every consumer of these two methods will have some additional checks that they need to pass. First, if they are in the same scope they need to call `register` before `unregister`. Or in other words an `add` classified method needs to be called before a `sub` classified method. And through out this document we would refer an `add` classified method as an add method. And a sub classified method as a sub method.

```ts
class View<M> {
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
        this.user.unregister('change:title', this.showAlert);
    }
}
```

Though, in this case having the method calls on the same scope is not quite useful. Since we unregister the event directly. It would be as good as not calling anything at all. But in order to pass the compiler checks we can also call a sub method in another method.

```ts
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

Notice first, that whenever there is a scope with an unmatched add or sub methods. The unmatched methods classifies the containing method. Here we show the inherited classification in the comments below:

```ts
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

The methods in our class is now balanced. This leads us to our second rule. A class's methods needs to be balanced. A balanced class has two methods of which one of them having an add method and another having a corresponding sub method. And when a class is balanced it implicitly infers that the a balance check should be done in an another scope other than in the current class's methods. This could be inside an another class's method that uses the current class's add method.

Now, let use the above class in an another class we call `SubView`:

```ts
class SubView {
    show() {
        this.subView = new View(this.user); // This call is an add method
        this.subView = null;
    }
}
```

The above code does not pass the compiler check, because there is no matching sub method. Also the code causes a memory leak.

We can add the call the expression `this.view.removeUser()` below, to match our add method. Now, on the same scope we have a matching add and sub methods. So the compiler will compile the following code. Also, the code, causes no memory leaks:

```ts
class SuperView {
    show() {
        this.view = new View(this.user); // Add constructor.
        this.view.removeUser(); // Sub method.
        this.view = null;
    }
}
```

If we don't add a sub method call above. The containing method will be classified as an add method:

```ts
class SubView {
	// add UserChangeTitleCallback
    show() {
        this.view = new View(this.user); // Turns the toggle on.
        this.view = null;
    }
}
```

The method `show` inherited the `add` classification from the expression `new View(this.user)`. This inheritance loop goes on and on.

## Call Paths

We have so far only considered object having an instant death. And this is not so useful. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler checks will pass. Now, this leads us to our next rule:

Passing a sub method method as a callback argument will balance an add method in current scope:

```ts
class SubView {
    public show() {
        this.view = new View(this.user); // Add method.
		this.onDestroy(this.remove); // Passing a sub method to a call expression matches the add method above.
    }
	
	private onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	// sub UserChangeTitleCallback
	public remove() {
        this.view.removeUser(); // Sub method.
        this.view = null;
	}
}
```
Now, we have ensured a possible death of our `view`, because `this.remove` has an inherited a `sub` classification:
```ts
	this.subView = new View(this.user); // Add method.
	this.onDestroy(this.remove); // `this.onDestroy` takes a callback. And we passed in a sub method. Which mean we have a possible death for our `subView`.
```

The scope is balanced and the compiler will not complain. Notice, whenever an add method is balanced with a sub method directly or whenever there is a path(call path) that can be reached, to balance a sub method. The code will pass the compiler check. Because in other words, we have ensured a possible death of our allocated resource.
```
BIRTH ---> DEATH
BIRTH ?---> CALL1 ---> CALL2 ---> CALLN ---> DEATH
```
Notice, that we say a possible death and not a certain death. This is because it is upto the business logic to decide when these callbacks should be called or not. Just take our `onDestroy` method in our `SubView` class:

```ts
onDestroy(callback: () => void) {
	this.deleteButton.addEventListener(callback); 
}
```

We cannot guarantee that the callback is being called. It is upto the end-user to click the delete button. Though we can guarantee that a call path has a path that at the end calls a method that either adds or subtracts an element in a balanced storage.

## Aliasing

We some times, need to deal with multiple references of the same class of objects. The compiler will not pass the code if there is two classifications that have the same name. This is because we want to associate one type of allocation/deallocation of resource with one identifier. This will make code more safe, because one type of allocation cannot be checked against another type of deallocation.

In order to satisfy our compiler we would need to give our classifications some aliases. And the syntax for aliasing a classification is:
<pre>
<i>AddSubAliasClassification ::</i> <b>add</b> | <b>sub</b> <i>Name</i> <b>as</b> <i>Alias CallExpression</i>
</pre>
Lets go ahead and add these classifications:

```ts
import { View } from './view';

class SubView {
	private view: View;
	private anotherView: View;

	// add UserChangeTitleCallbackOnView
	// add UserChangeTitleCallbackOnAnotherView
    public show() {
		add UserChangeTitleCallback as UserChangeTitleCallbackOnView
        this.view = new View(this.user); // Add method.
		add UserChangeTitleEventCallback as UserChangeTitleCallbackOnAnotherView
        this.anotherView = new View(this.user); // Add method.
        
	    // sub UserChangeTitleCallbackOnView
	    // sub UserChangeTitleCallbackOnAnotherSubView
        this.onDestroy(() => {
            this.removeView();  // Sub method.
            this.removeAnotherView();  // Sub method.
        });
    }
	
	private onDestroy(callback: () => void) {
		this.deleteButton.addEventListener(callback); 
	}
	
	// sub UserChangeTitleCallbackOnView
	public removeView() {
		sub UserChangeTitleCallback as UserChangeTitleCallbackOnView
        this.view.removeUser(); // Sub method.
        this.view = null;
	}
	// off UserChangeTitleCallbackOnAnotherView
	public removeAnotherView() {
		off UserChangeTitleCallback as UserChangeTitleCallbackOnAnotherView
        this.anotherView.removeUser(); // Sub method.
        this.subView = null;
	}
}
```

Notice, that the anonymous lambda function will inherit the classifications:

```ts
// sub UserChangeTitleCallbackOnView
// sub UserChangeTitleCallbackOnAnotherView
this.onDestroy(() => {
    this.removeView();  // Sub method.
    this.removeAnotherView();  // Sub method.
});
```

This is due to the fact that the lambda's function's scope does not have matching add method calls for the current sub method calls. Also, since `this.onDestroy` is a method which accept callbacks, and it will balance the containing scope:

```ts
add UserChangeTitleCallback as UserChangeTitleCallbackOnView
this.view = new View(this.user); // Add method.
add UserChangeTitleCallback as UserChangeTitleCallbackOnAnotherView
this.anotherView = new View(this.user); // Add method.
```

So in other words, The above code will compile. It also causes no memory leaks.

## Balanced Storage Annotation

One could ask why not annotating the source of the memory leak directly instead of classifying the methods? It's possible and it is even more safer. Lets revisit our `EventEmitter` class:

```ts
export class EventEmitter {
    public eventCallbacks: EventCallbacks = {}
}
```

Our general concept so far, is that for every method for addition of objects, there should exists at least one matching method for subtraction of objects. We can safely say that new assignements will increase the elements count. But also, in the event emitter case, array element additions. With static code analysis we can also make sure that elements are added or subtracted. Before it was upto the developer to manually annotate and implement the method. But nothing stops the developer to implement a method with a sub method which increases the element count.

### Add-Sub Method Definition

For the add-sub methods the following holds true:

```
Add methods: Adds 1 elements.
Sub methods: Subtracts the added element.
```

And let it be our definitions for our add-sub methods for our case (Though it is upto a programming language implementor to decide what is the definition of the add-sub methods).

### Syntax

We also want to introduce a new syntax to annotate a storage:
<pre>
<i>AddSubClassification ::</i> <b>balance</b> <i>Storage</i> <b>as</b> <i>ElementName StorageDeclaration</i>
<i>Storage :: Type, { IndexExpression }</i>
<i>IndexExpression :: "[", Index, "]"</i>
</pre>
*StorageDeclaration* is a property declaration in a class. *ElementName* is the name of an element in *StorageDeclaration*.

Lets annotate our `eventCallbacks` store:

```ts
export class User extends EventEmitter {
    balance EventCallbacks[event] as EventCallback
    public eventCallbacks: EventCallbacks = {}
}
```

Notice, also when we now have an annotation for the storage. We don't need to classify methods with `add|sub NAME` anymore. Though, you still need to alias some call expressions with `add|sub NAME as ALIAS` to prevent name collisions.

### Add Method Example

Now, we can staticly analyse the `register` method, which is suppose to be an add method:

```ts
public register(event: string, callback: Callback) {
    if (!this.eventCallbacks[event]) {
        this.eventCallbacks[event] = []; 
    }
    this.eventCallbacks[event].push(callback);
}
```

Lets check out our if statement and body:

```ts
if (!this.eventCallbacks[event]) {
    this.eventCallbacks[event] = [];
}
```

An assignment adds 0 or 1 new elements to the store. Though we have only specified that an index element of `this.eventCallbacks[event]` are storage elements. So the above code does not add anything to store.

In comparison, the following expression adds one element to the store:

```ts
this.eventCallbacks[event].push(callback);
```

So in all, we can safely say that the method adds 1 element to the store `eventCallbacks` each time the method is called. And it satisfies our add method definition above.

### Sub Method Example

Now, lets examine our `unregister` method:

```ts
public unregister(event: string, callback: Callback): void {
    let callbacks = this.eventCallbacks[event].length;
    for (let i = 0; i < callback.length; i++) {
        if (this.eventCallbacks[event][i] === callback) {
            this.eventCallbacks[event].splice(i, 1);
        }
    }
}

```
We can statically confirm that this method subtracts 0 or 1 elements from our store with the following expression(even though it is encapsulated in a for-loop and an if statement block):

```ts
this.eventCallbacks[event].splice(i, 1);
```
Please also notice that the add methods adds 1 element and the sub method subtracts 0 or 1 element. So logically there still could be a potential memory leak. Though, our method above has a for loop that loops through all elements and a check for an element existens before a subtraction of element occur. This is crucial for balancing our storage:

```ts
for (let i = 0; i < callback.length; i++) {
    if (this.eventCallbacks[event][i] === callback) {
        this.eventCallbacks[event].splice(i, 1);
    }
}
```

So, in order to have a balance, we must know that whatever was passed on our add method. Would be passed in our sub method. In that way we will know for sure that whatever was added will eventually get deleted. Lets examine our `View` once again. 

```ts
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

We can see that whatever was passed in our add method:

```ts
this.user.register('change:title', this.showAlert);
```
The same arguments was passed to our sub method:
```ts
this.user.unregister('change:title', this.showAlert);
```
So we got a balance. Now, our add and sub method annotated the containg methods in the `View` class. So who ever consumes the `View` class must balance the method calls in order to not cause a memory leak. And our compiler will always check that this is the case.

### False Add-Sub Method example

Lets also examine an false add-sub method example. Lets take our `emit` method as an example:

```typescript
public emit(event: string, args: any[]) {
    if (this.eventCallbackStore[event]) {
        for (let callback of this.eventCallbackStore[event]) {
            callback.apply(null, args);
        }
    }
}
```

There is no expression in above that increases our elements count in our store. There exists index look up such as `this.eventCallbackStore[event]`, though they don't add any elements. So we can safely say that this method does not satisfy any of our add-sub method definitions above.

### Types of storage

We only considered a hash map so far. Though any type that can grow the heap can be annotated.

## Heap Object Graph

When we arrived at our final syntax. We discovered that we could annotate a property and let static analysis discover our toggle methods. Let us illustrate what this means:

When we have a memory leak. Essentially what it means is that our heap object graph can grow to an infinite amount of nodes, starting from some node (we use a tree instead of a graph below):

![Heap object infinity nodes](https://raw.githubusercontent.com/tinganho/a-toggle-modifier-proposal/master/HeapObjectTreeHorizontalInfinity%402x.jpg)

And we want to statically annotate that any consumer of this node needs to call an add and sub methods:

![Heap object infinity with on and off toggles](https://raw.githubusercontent.com/tinganho/a-toggle-modifier-proposal/master/HeapObjectTreeHorizontalOnOff%402x.jpg)
