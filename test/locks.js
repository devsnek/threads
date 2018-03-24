'use strict';

const Thread = require('..');

const t = new Thread(({ lock }) => {
  const success = lock();
  return success;
});

t.join().then((r) => console.log('JOIN', r));

console.log('1', t.lock());
console.log('2', t.lock());
