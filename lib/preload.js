global.Buffer = require('../node_modules/buffer/index.js').Buffer;

{
  const _console = global.console_;
  delete global.console_;

  const format = require('./util-format');

  global.console = {};
  const methods = ['log', 'debug', 'info', 'warn', 'error'];
  for (const method of methods)
    global.console[method] = (...args) => _console(method, format(...args));

  const _hrtime = performance._hrtime;
  delete performance._hrtime;
  const hrValues = new Uint32Array(3);

  performance.now = () => {
    _hrtime(hrValues);
    return (((hrValues[0] * 0x100000000) + hrValues[1]) * 1000) + (hrValues[2] / 1e6);
  };
}
