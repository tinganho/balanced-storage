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
    someMethod() {
        let view = new View(this.user);
        view = null; // this.user persists.
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
    constructor(private user: User) {
        on UserChangeTitle
        this.user.on('change:title', this.showAlert);
    }
    
    public showAlert(title: string) {
        alert(title);
    }
```
The above code won't compile, since there is no `off` statement. Just adding this line will let the compiler compile:
```typescript
class View<M> {
    ...
    public remove() {
        off UserChangeTitle
        this.user.off('change:title', this.showAlert)
    }
}
```
Whenever you toogle on something you must toogle it off. Otherwise the compiler won't compile.
