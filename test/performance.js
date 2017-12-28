const Thread = require('..');

const t = new Thread(() => {
  return performance.now();
});

t.join().then(console.log);
