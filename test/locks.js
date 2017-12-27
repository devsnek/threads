const Thread = require('..');

const t = new Thread(({ lock }) => {
  return lock();
});

t.join((r) => console.log('JOIN', r));

console.log('1', t.lock());
console.log('2', t.lock());
