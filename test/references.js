const Thread = require('..');

const x = 1;

const t = new Thread((context) => {
  console.log(context);
}, { test: () => x });

t.join().then(console.log);
