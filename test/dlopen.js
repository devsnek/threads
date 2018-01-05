const Thread = require('..');

const t = new Thread((c) => {
  console.log(c);
  const m = global.dlopen(c);
  console.log(m);
}, [require.resolve('../build/Debug/native-tests.node')]);
