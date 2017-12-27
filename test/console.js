const Thread = require('..');

new Thread(() => {
  console.log('hi');
  console.error('hi');
  console.debug('owo', 'whats', 1);
  console.info({ a: Symbol('meme') });
});
