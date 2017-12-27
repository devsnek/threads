const Thread = require('..');

const t = new Thread(() => {
  return Buffer.from('hi');
});

t.join(console.log);
