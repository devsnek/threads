const Thread = require('..');

const t1 = new Thread(() => {
  x + 1;
});

t1.catch((e) => {
  console.error('CATCH1', e);
});

const t2 = new Thread(`() => {
  <invalid syntax>
}`);

t2.catch((e) => {
  console.error('CATCH2', e);
});
