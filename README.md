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

TODO:
- [ ] Fix up Thread#catch with the event loop
- [ ] Maybe inject node buffers into them
- [ ] Allow passing "references" instead of copies
