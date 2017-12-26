const Thread = require('.');

const t = new Thread((x, y) => {
  const start = Date.now();
  const end = start + 1000;
  while (Date.now() < end) {};
  return { x, y };
}, 5, 'meme');

t.catch((e) => console.error('AA', e));

t.join((x) => console.log(process.uptime(), x));

console.log('not very blocking');
