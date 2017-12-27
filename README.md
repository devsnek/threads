# threads

These are real threads that don't crash **and** run your js without exploding.

```javascript
const Thread = require('threads');

const t = new Thread((foo, bar) => {
  return foo + bar;
}, 1, 2);

t.join((result) => {
  assert(result === 3);
});
```

```javascript
const adder = new Thread(({ on, send }) => {
  on('message', (d) => {
    send(d + 5);
  });
});

adder.on('message', (result) => {
  assert(result === 10);
});

adder.send(5);
```

TODO:
- [ ] Fix up Thread#catch with the event loop
- [ ] Maybe inject node buffers into them
- [ ] Allow passing "references" instead of copies
