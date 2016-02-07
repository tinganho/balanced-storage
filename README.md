# toggle-modifier-proposal

Let say, that we got the following event emitter class:

```typescript
type Callback = (...args: any[]) => any;

interface EventCallbackStore {
    [event: string]: Callback[];
}

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

And we extend it with a following class:

```typescript
class Model extends EventEmitter {
    private title: string;
    public remove() {
        this.eventCallbackStore = {};
    }
    
    public setTitle(title: string) {
        this.title = title;
        this.emit('change:title', title);
    }
}
```
What we have done is that 
