'use strict';

const Thread = require('../');

const t = new Thread(() => {
  function x() {
    throw new Error('awoo');
  }
  x();
});

t.join().then(console.log, console.error);
