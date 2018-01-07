/* eslint-disable no-console */

const Thread = require('..');

const t = new Thread((x, y, { on, send, id }) => {
  send(1);
  on('message', (d) => {
    console.log('MESSAGE INSIDE', d);
    send(d);
  });
  return { x, y, id };
}, [5, 'meme']);

t.join()
  .then((r) => console.log('JOIN', r))
  .catch((e) => console.error('CATCH', e));

t.on('message', (d) => {
  console.log('MESSAGE', d);
  if (d === 3)
    t.terminate();
});

t.send(2);
t.send(3);
