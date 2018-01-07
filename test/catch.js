const Thread = require('..');

const t1 = new Thread(() => {
  x + 1;
});

t1.join().catch((e) => {
  console.error('CATCH1', e);
});

const t2 = new Thread(`() => {
  <invalid syntax>
}`);

t2.join().catch((e) => {
  console.error('CATCH2', e);
});
