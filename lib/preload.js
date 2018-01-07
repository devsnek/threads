global.Buffer = require('../node_modules/buffer/index.js').Buffer;

{
  const _console = global._console;
  delete global._console;

  const { format } = require('./util');

  global.console = {};
  const methods = ['log', 'debug', 'info', 'warn', 'error'];
  for (const method of methods)
    global.console[method] = (...args) => _console(method, format(...args));
}

{
  const _hrtime = performance._hrtime;
  delete performance._hrtime;
  const hrValues = new Uint32Array(3);

  performance.now = () => {
    _hrtime(hrValues);
    return (((hrValues[0] * 0x100000000) + hrValues[1]) * 1000) + (hrValues[2] / 1e6);
  };
}

/*
{
  const dlopen = global._util.dlopen;
  global.dlopen = (path) => {
    const module = { exports: {} };
    dlopen(module, path);
    return module.exports;
  };
}
*/

delete global._util;
