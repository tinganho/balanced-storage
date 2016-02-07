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

Then in some other class's method we instantiate the view with a reference user model:
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
    this.showAlert(); // `this` is the reference to view. So `user` is still referencing the `view`.
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
Whenever you toogle on something you must toogle it off. Otherwise the compiler won't compile. A toogle is spreading upwards, if there is no local off statement matching one on statement:

The above code won't compile, since there is no `off` declaration. Just adding this line will let the compiler compile:
```typescript
import {UserChangeTitle} from '/model'

class SuperView{
    someMethod() {
        this.subView = new View(this.user); // Turns the toggle on.
        this.subView.removeUser(); // Turns the toggle off.
        this.subView = null;
    }
}
```
