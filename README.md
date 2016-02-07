# toggle-modifier-proposal

And we extend it with a following class to create a user model:

```typescript
class User extends EventEmitter {
    private title: string;
    
    public setTitle(title: string) {
        this.title = title;
        this.emit('change:title', title);
    }
}
```
What we have done is that 

```typescript
class View<M> extends EventEmitter {
    constructor(private user: User) {
        this.user.on('change:title', () => {
            this.showAlert();
        });
    }
    
    public showAlert(title: string) {
        alert(title);
    }
}

let view = new View(user);
view = null;
```
