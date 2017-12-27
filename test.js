/* eslint-disable no-console */

const Thread = require('.');

const t = new Thread((x, y, context) => {
  context.send(1);
  context.on('message', (d) => context.send(d));
  return { x, y };
}, 5, 'meme');

t.catch((e) => console.error('CATCH', e));

t.join((r) => console.log('JOIN', r));

t.on('message', (d) => {
  console.log('MESSAGE', d);
  if (d === 3)
    t.terminate();
});

t.send(2);
t.send(3);
