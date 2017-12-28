const Thread = require('..');

const t = new Thread(() => {
  const sample = new Array(2500000);
  for (let i = 0; i < sample.length; i++)
    sample[i] = performance.now();
});

t.join().then(console.log);
