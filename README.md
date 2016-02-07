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
    
    public showAlert(title: string) {
        alert(title);
    }
```
The above code won't compile, since there is no `off` statement. Just adding this line will let the compiler compile:
```typescript
import {UserChangeTitle} from '/model'

class SuperView{
    someMethod() {
        this.subView = new View(this.user);
        off UserChangeTitle:
        this.subView.user = null;
        this.subView = null;
    }
}
```
Whenever you toogle on something you must toogle it off. Otherwise the compiler won't compile. A toogle is spreading upwards also if you there is no off statement:
```typescript
import {UserChangeTitle} from '/model'

class SuperView{
    on UserChangeTitle
    someMethod() {
        this.subView = new View(this.user);
        this.subView = null;
    }
}
```
Since there is no off statement on the above code we must set an on statement to our method.
