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
    this.showAlert(); // `this` is referencing view. So `user` is referencing `view`.
});
```

### Proposal

We want to prevent the memory leak by static code analysis. I propose the following syntax

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
import { UserChangeTitle } from '/model'

class SuperView{
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView = null;
    }
}
```

Just adding the call expression `this.subView.removeUser()` below. Will turn the toogle off. Now, on the same scope we have a matching `on` and `off` toogles. So the compiler will compile the following code.
```typescript
import { UserChangeTitle } from '/model'

class SuperView{
    showSubView() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
    }
}
```
