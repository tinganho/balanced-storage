Static Typing for Memory Leak Prevention
====================

# Introduction
Software has been daunted with memory leaks for a long time. There exists one interesting question to ask, can we make memory management more safe with static code analysis? Can we make a compiler help us with common mistakes made, when dealing with memory management?

# Table of Contents
* [Introduction](#abstract)
* [Memory Leaks](#memory-leaks)
  * [Definition](#definition)
  * [Example](#example) 
* [Balanced Storage](#balanced-storage)
  * [Balance](#balance)
  * [Method Classification](#method-classification)
  * [Inheritance](#inheritance)
  * [Call Paths](#call-paths)
  * [Loop Paths](#loop-paths)
  * [Aliasing](#aliasing)
  * [Multiple Simultaneous Addition And Subtraction of Objects](#multiple )
  * [Initialization and Deallaction](#initialization-and-deallocation)
  * [Control Flow Analysis](#control-flow-analysis)
  * [Weak Balance](#weak-balance)
  * [Annotation](#annotation)
    * [Add-Sub Method Definition](#add-sub-method-definition)
    * [Syntax](#syntax)
    * [Add Method Example](#add-method-example)
    * [Sub Method Example](#sub-method-example)
    * [False Add-Sub Method Example](#false-add-sub-method-example)
  * [Heap Object Graph](#heap-object-graph)

# Memory Leaks

Long runnning applications needs to allocate memory to store objects that lives a long time. Though, during allocation and storing of objects a developer might forget to handle the case when the object is no longer needed, and it needs to be deleted. Even though, the developer remembers to handle the deletion of objects, there still exists blind spots where the reference count of objects does not reach zero and thus creates a memory leak in a garbage collected language or languages that uses reference counting. We will try to cover some of these problems and present a solution to these problems.

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
class View {
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

As you can see we did a mistake. We unreferenced the view. And we expected it to be garbage collected. But instead we caused a memory leak. Did you spot in which line was causing a memory leak? It is on this line:

```ts
this.user.on('change:title', () => {
    this.showAlert(); // `this` inside the closure is referencing view. So `this.user` is referencing `view`.
});
```

As the comment says, `this` inside the closure is referencing  the view. So `this.user` is referencing `view`. Because the reference count haven't reached zero, the garbabge collector cannot garbage collect the `view`.

# Balanced Storage

We want to prevent the memory leaks by static code analysis. But in doing so, we must analyse the source of memory leaks. By definition a memory leak is an unused resource at runtime. We allocate memory and initialize our resource. When the resource is no longer needed we need to deallocate it. In a garbage collected language we can unreference objects so they get garbage collected. And for a manual managed memory programming languages, we must deallocate it manually by writing some sort of expressions. In a majority of cases, if not all, a memory leaked resource often has one or more references to itself. In a garbage collected language this always holds true, they always have at least one reference to itself(otherwise they would be garbage collected). 

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

The `eventCallbacks` above is hashmap of a list of callbacks for each event. We register new events with the `register` method and unregister them with the `unregister` method. We can emit a new event with the `emit` method. The property `eventCallbacks` is a potential leaking resource storage, because it can hold callbacks on events and a developer might forgot to unregister. Though the essentials here, is the `register` and `unregister` methods. Because their role is to register and unregister events. This leads us to think, can we somehow require a user who calls `register` always call `unregister`? If possible, we would prevent having any memory leaks. Let us answer this question later, and begin with annotating them first. 

## Method Classification

Lets just the add a temporary classification syntax for our methods:

<pre>
<i>MethodClassification ::</i>
&emsp;&emsp;<b>add</b> | <b>sub</b> <i>Name MethodDeclaration</i>
</pre>

The `add` and `sub` keywords are operators that classify methods with a name that identifies that elements is being added or subtracted from our storage, when the method is called. So for our `User` model which is an extension of the `EventEmitter` class, we go ahead and classify our methods.

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
class View {
    constructor(private user: User) {
        this.user.register('change:title', this.showAlert);
        this.user.unregister('change:title', this.showAlert);
    }
}
```

Though, in this case having the method calls on the same scope is not quite useful. Since we unregister the event directly. It would be as good as not calling anything at all. But in order to pass the compiler checks we can also call a sub method in another method.

```ts
class View {
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

In the above example. We call `this.user.unregister('change:title', this.showAlert);` in the method `removeUser` to pass the compiler check. We pass the compiler check because the class's methods is now balanced. There is one add method and one sub method on the class. 

To achieve balance either a scope needs to be balanced or a class's methods neeed to be balanced. A balanced scope, means that there is a sub method balancing an add method. A balanced class, means that there exists a corresponding sub method for an add method.

Target | Elements | Balance
--- | --- | ---
Class | Methods | For every add method there must exist a corresponding sub method.
Scope | Call expressions | Whenever an add method is called, there must be a possibility that a corresponding sub method is being called.

## Inheritance

Notice first, that whenever there is a scope with an unmatched add or sub methods. The unmatched methods classifies the containing method. Here we show the inherited classification in the comments below:

```ts
class View {
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

The methods in our class is now balanced. When a class is balanced it implicitly infers that the a balance check should be done in an another scope other than in the current class's methods. This could be inside an another class's method that uses the current class's add method.

Now, let use the above class in an another class we call `SubView`:

```ts
class SubView {
    private view: View;
    
    public show() {
        this.view = new View(this.user); // Add method(constructor).
        this.view = null;
    }
}
```

The above code does not pass the compiler check, because there is no matching sub method. Also the code causes a memory leak.

We can add the call expression `this.view.removeUser()` below, to match our add method. Now, on the same scope we have a matching add and sub methods. So the compiler will compile the following code. Also, the code, causes no memory leaks:

```ts
class SubView {
    private view: View;
    
    public show() {
        this.view = new View(this.user); // Add method(constructor).
        this.view.removeUser(); // Sub method.
        this.view = null;
    }
}
```

If we don't add a sub method call above. The containing method will be classified as an add method:

```ts
class SubView {
    private view: View;
    
	// add UserChangeTitleCallback
    public show() {
        this.view = new View(this.user); // Add method.
        this.view = null;
    }
}
```

The method `show` inherited the `add` classification from the expression `new View(this.user)`. This inheritance loop goes on and on.

## Call Paths

We have so far only considered object having an instant death. And this is not so useful. What about objects living longer than an instant? We want to keep the goal whenever an object has a possible death the compiler checks will pass. Now, this leads us to our next rule:

```
Passing a sub method method as a callback argument will balance an add method in current scope.
```

Lets go ahead and add our call expression, that accepts a corresponding sub method for our add method:

```ts
class SubView {
    private view: View;
    
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

Now, we have ensured a possible death of our `view`, because the method `remove` has inherited a `sub` classification. So `this.remove` is a callback that corresponds to the add method(constructor) `new View(this.user)`:

```ts
	this.view= new View(this.user); // Add method(constructor).
	this.onDestroy(this.remove); // 
```

`this.onDestroy` takes a callback. And we passed in a corresponding sub method for our add method above. Which means, we have a possible death for our `view`. The scope is balanced and the compiler will not complain. Notice, whenever an add method is balanced with a sub method directly or whenever there is a path(call path) that can be reached, to balance a sub method. The code will pass the compiler check. Because in other words, we have ensured a possible death of our allocated resource.

```
BIRTH ---> DEATH
BIRTH ?---> CALL1 ---> CALL2 ---> CALLN ---> DEATH
```

Notice, that we say a possible death and not a certain death. This is because it is sometimes upto the business logic to decide when these callbacks should be called or not. Just take our `onDestroy` method in our `SubView` class:

```ts
onDestroy(callback: () => void) {
	this.deleteButton.addEventListener(callback); 
}
```

We cannot guarantee that the callback is being called. It is upto the end-user to click the delete button. Though we can guarantee that a call path has a path that at the end calls a method that either adds or subtracts an element in a balanced storage.

## Loop Paths

In addition to call paths, we can also create a loop path, to  make objects live longer than an instance.

We will use a C++ console application to show what a loop path is. Our console application will give you four choices:

1. Add an item.
2. Delete an item.
3. Show all items.
4. Quit application.

Each item is defined as:

```c++
struct Item {
    string name;
    Item(string _name):name(_name) {}
};
```

And our storage as:

```c++
class Storage {
public:
    vector<Item*> items;
    
    Storage(): items { new Item { "eyeglasses" } } {}
    
    void addItem(string name) {
        items.push_back(new Item {name});
    }
    
    void deleteItem(int index) {
        auto item = items.begin() + index;
        objects.erase(item);
        delete *item;
    }
};
```

The compiler will statically classify `addItem` is an add method and `deleteItem` is a sub method. We will cover the details later.

We then have while loop that ask us our options:

```c++
while (true) {
    cout << "What do you want to do?" << endl;
    cout << " (1) Add an item." << endl;
    cout << " (2) Delete an item." << endl;
    cout << " (3) Print all items." << endl;
    cout << " (q) Quit." << endl;
    cin >> answer;
    ...
}
```

And we can choose an option for adding one item:

```c++
    if (answer == '1') {
        string itemName;
        while (true) {
            cout << "Type the item you want to add:" << endl;
            cin >> itemName;
            if (cin.fail()) {
                clearInput();
                cout << "Wrong answer!" << endl;
                continue;
            }
            storage.addItem(itemName);
            cout << "Added object: " + itemName << endl;
            break;
        }
    }
```

The compiler will not compile the above code. Since we have not used the sub method `storage.deleteItem`.  Let us define that option too:

```c++
    else if(answer == '2') {
        int index;
        cout << "Type the index on the item you want to delete:" << endl;
        cin >> index;
        storage.deleteItem(index);
    }
```

Now, if we consider our while loop, looping infinite of time. And according to Murphys law, whatever can happen, will happen. we can derive that what eventually gets added to our storage must eventually be deleted. 

Though looping infinite of times is not a requirement for reaching a balance. The requirement is just looping at least once or twice(more on this later). Because then an added item have a possibility of being subtracted. Which corresponds to our definition of reaching balance in a scope.

![Loop Path of an Object](https://gitlab.com/tinganho/balanced-storage/raw/master/LoopPath@2x.jpg)

In the first loop it can enter a branch, which an element gets added. In the second loop or any subsequent loops it can enter a branch where it gets deleted. It can also enter a branch or two branches where an elements gets created and destroyed at the same loop.

So for loops, whether it is a while loop or a for loop that can loop at least more than once. The following rules applies:

<pre>
If in a loop, and the loop only loops once:
<i>&emsp;&emsp;Balance can be reached if in one branch has an add method call and a sub method call.</i>
<i>&emsp;&emsp;Balance can be reached, if one branch has an add method call and another branch has a sub method call, only if the add method and sub method branch is both being entered and the add method branch is entered before the sub method branch. </i>

If in a loop, and the loop loops twice or more:
<i>&emsp;&emsp;Balance can be reached if in one branch has an add method call and a sub method call.</i>
<i>&emsp;&emsp;Balance can be reached if in one branch has an add method and another branch has a sub method.</i>
</pre>

Here is the whole application source code:

```c++

#include <iostream>
#include <vector>
#include <ctype.h>

using namespace std;

struct Item {
    string name;
    Item(string _name):name(_name) {}
};

class Storage {
public:
    vector<Item*> objects;
    Storage(): objects { new Item { "eyeglasses" } } {}
    
    void addItem(string name) {
        objects.push_back(new Item {name});
    }
    
    void deleteItem(int index) {
        auto item = objects.begin() + index;
        objects.erase(item);
        delete *item;
    }
};

void clearInput()
{
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int main ()
{
    Storage storage;
    char answer;
    
    while (true) {
        cout << "What do you want to do?" << endl;
        cout << " (1) Add an item." << endl;
        cout << " (2) Delete an item." << endl;
        cout << " (3) Print all items." << endl;
        cout << " (q) Quit." << endl;
        cin >> answer;
        if (cin.fail()) {
            clearInput();
            cout << "Wrong answer!" << endl;
            continue;
        }
        if (answer == 'q') {
            break;
        }
        if (!(answer >= '1' && answer <= '3')) {
            clearInput();
            cout << "Wrong answer!" << endl;
            continue;
        }
        if (answer == '1') {
            string itemName;
            while (true) {
                cout << "Type the item you want to add:" << endl;
                cin >> itemName;
                if (cin.fail()) {
                    clearInput();
                    cout << "Wrong answer!" << endl;
                    continue;
                }
                storage.addItem(itemName);
                cout << "Added object: " + itemName << endl;
                break;
            }
        }
        else if(answer == '2') {
            int index;
            cout << "Type the index on the item you want to delete:" << endl;
            cin >> index;
            storage.deleteItem(index);
        }
        else {
            auto objects = storage.objects;
            auto storageLength = objects.size();
            for (int i = 0; i<storageLength; i++) {
                cout << objects[i]->name << endl;
            }
        }
    }
    
    return 0;
}

```

## Initialization and Deallocation of Objects

In C++ we initialize with `new` and deallocate objects with `delete`. Our balanced storage definition so far have only considered collections as data types. The question is, if we can apply the same balancing rules here too fight memory leaks? And it turns out that we can.

`new` in C++ will be considered as an add method. `delete` will be considered as a sub method.

First example is the most simpliest one:

```c++
int main ()
{
    auto item = new Item {"glasses"};
    delete item;
}
```

`Item` is the same from our previous examples. We allocated and initialize it and delete it. And we have a balanced scope inside `main`.

For more complex rules for loop paths and call paths, where balancing is considering branches and methods. The same rules applies for initialization and deallocation. 

Lets consider our storage class again:

```c++
class Storage {
public:
    vector<Item*> items;
    
    Storage(): items { new Item { "eyeglasses" } } {}
    
    void addItem(string name) {
        items.push_back(new Item {name});
    }
    
    void deleteItem(int index) {
        auto item = items.begin() + index;
        objects.erase(item);
        delete *item;
    }
};
```
We have an initialization statement on our add method:

```c++
items.push_back(new Item {name});
```

Because it is referenced to an index in `items`, our delete method also need to reference to an index in `items` in order to reach balance. And that is exactly what we do:

```c++
auto item = items.begin() + index;
objects.erase(item);
delete *item;
```

## Multiple Referenced Store

You can have two reference to the same storage instnace:

```c++
Storage* storage = new Storage();
void addToStorage(Item* item)
{
    storage->addItem(item);
}

void deleteFromStorage(Item* item)
{
    storage->deleteItem(item);
}

int main ()
{
    auto item = new Item {"glasses"};
    Storage* tmpStorage = storage;
    tmpStorage->addItem(item);
    storage->deleteItem(item);
    tmpStorage->showItems();
    delete item;
}
```

If you have two reference to two separate store you would not have balance, if you called the add method on one, and the delete method on the other.

## Aliasing

We some times, need to deal with multiple references of the same class of objects. The compiler will not pass the code if there is two classifications that have the same name. This is because we want to associate one type of allocation/deallocation of resource with one identifier. This will make code more safe, because one type of allocation cannot be checked against another type of deallocation.

In order to satisfy our compiler we would need to give our classifications some aliases. And the syntax for aliasing a classification is:
<pre>
<i>AddSubAliasClassification ::</i>
&emsp;&emsp;<b>add</b> | <b>sub</b> <i>Name</i> <b>as</b> <i>Alias CallExpression</i>
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

### Multiple Simultaneous Addition And Subtraction of Objects

When dealing with multiple simultaneous addition or subtraction of objects, it is good practice to have already balanced method calls on constructors or methods.

```typescript
import { View } from './view';

class SubView {
	private views: View[] = [];
	
    public show() {
		for (let i = 0; i < 10; i++) {
        	this.views.push(new View(this.user)); // The constructor is already balanced. So the compiler will not complain.
		}
    }
}
```

Because we don't need to deal with an even more complex balancing of add-sub methods.

For various reason, if one cannot use balanced methods/constructors. The classifications conventions works as before. An unmatched add or sub method call annotates the containing method. Even on a for loop below:

```typescript
class SubView {
	private views: View[] = [];
	
	// add UserChangeTitleCallback
    public show() {
		for (let i = 0; i < 10; i++) {
		    add UserChangeTitleCallback as UserChangeTitleCallbacks
        	this.views.push(new View(this.user));
		}
    }
}
```

As before, we would need to have a matching sub method to satisfy our compiler:

```typescript
class SubView {
	private views: View[] = [];
	
	// sub UserChangeTitleCallbacks
	public remove() {
		for (let i = 0; i < 10; i++) {
		    sub UserChangeTitleCallback as UserChangeTitleCallbacks
        	this.views[i].removeUser();
		}
		this.views = [];
    }
}
```

In the above example, `this.views[i].removeUser();` is a sub method and since there are no add method that balances it. It annotates the containing method `remove`.

Now, our whole class will look like this:

```typescript
class SubView {
    private views: View[] = [];
    
	// add UserChangeTitleCallback
    public show() {
		for (let i = 0; i < 10; i++) {
		    add UserChangeTitleCallback as UserChangeTitleCallbacks
        	this.views.push(new View(this.user));
		}
    }

    private onDestroy(callback: () => void) {
        this.deleteAllButton.addEventListener(callback); 
    }

	// sub UserChangeTitleCallbacks
	public remove() {
		for (let i = 0; i < 10; i++) {
		    sub UserChangeTitleCallback as UserChangeTitleCallbacks
        	this.views[i].removeUser();
		}
		this.views = [];
    }
}
```

Note, there is no compile error, even though the number of loops in `remove` is not matched with `show`. Like for instance, we could have only 9 loops instead of 10:

```typescript
for (let i = 0; i < 9; i++) {// Loop only 9 elements and not 10 causes another memory leak.
	off UserChangeTitleCallback as UserChangeTitleCallbacks
	this.subView[i].removeUser();
}
```

This is because it is very difficult to do that kind of assertion. A compiler cannot know the business logic of your application and thus cannot make that assertion. Notice, that adding multiple objects simultaneously and at least having a method of subtracting them, even though the the subtraction methods only subtracts one from the storage. It still satisfy our balance definition. Any objects added can eventually be subtracted.

## Control Flow Analysis

A control flow anlysis needs to be done by a compiler to check against that a add method is being called before a corresponding sub method. It would probably break or cause a leakage if a sub method is called before its corresponding add method.

## Weak Balance

Consider following code:
```c++
int main ()
{
    auto item = new Item {"glasses"};
    storage->addItem(item);
    if (something) {
        ...
    }
    else if (somethingElse) {
        storage->deleteItem(item);
    }
    else {
        ...
    }
    delete item;
}
```

If an add method is called and a corresponding sub method needs to be called. Due to branching above, a logic can reach a branch where the sub method is never being called. We consider cases such as this, for *weak balance*.

## Annotation

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
Add methods: Adds (1) elements to the storage.
Sub methods: Subtracts (1) elements to the storage.
```

And let it be our definitions for our add-sub methods for our case.

### Balance Scope Definition

We also need a definition for what a balance scope is.

```
Balance is reached whenever there is a possibility that one or more sub methods being called for one corresponding add method.
```

### Syntax

We also want to introduce a new syntax to annotate a storage:

<pre>
<i>AddSubClassification ::</i>
&emsp;&emsp;<b>balance</b> <i>Storage</i> <b>as</b> <i>ElementName StorageDeclaration</i>

<i>Storage ::</i>
&emsp;&emsp;<i>Type, { IndexExpression }</i>

<i>IndexExpression ::</i>
&emsp;&emsp;<i>"[", Key ,"]"</i>
</pre>

*StorageDeclaration* is a property declaration in a class. *ElementName* is the name of an element in *StorageDeclaration*. The *Key* above is an arbitrary identifier. It does not need to match anything. It is just there for readability. Though the whole index expression indicates it should go one or more levels deeper to indicate an element addition or subtraction is made when the compiler do static analysis of the add methods and sub methods. More on this later.

Lets annotate our `eventCallbacks` store:

```ts
export class User extends EventEmitter {
    balance EventCallbacks[event] as EventCallback
    public eventCallbacks: EventCallbacks = {}
}
```

The expression:
```ts
balance EventCallbacks[event] as EventCallback
```
indicates that only elements added/subtracted to `eventCallbacks[event]`(and not `eventCallbacks`) will be considered.

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
Please also notice that the add methods adds 1 element and the sub method subtracts 0 or 1 element for each call. So logically there still could be a potential memory leak. Though, our method above has a for loop that loops through all elements and a check for an element existens before a subtraction of element occur. This is crucial for balancing our storage:

```ts
for (let i = 0; i < callback.length; i++) {
    if (this.eventCallbacks[event][i] === callback) {
        this.eventCallbacks[event].splice(i, 1);
    }
}
```

So, in order to have a balance, we must know that whatever was passed on our add method. Would be passed in our sub method. In that way we will know for sure that whatever was added will eventually get deleted. Lets examine our `View` once again. 

```ts
class View {
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

```ts
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

![Heap object infinity with on and off toggles](https://gitlab.com/tinganho/balanced-storage/raw/master/HeapObjectTreeHorizontalOnOff@2x.jpg)
