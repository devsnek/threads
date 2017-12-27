global.Buffer = require('../node_modules/buffer/index.js').Buffer;

{
  const _console = global.console_;
  delete global.console_;

  global.console = {};
  const methods = ['log', 'debug', 'info', 'warn', 'error'];
  for (const method of methods)
    global.console[method] = (...args) => _console(method, args.join(' '));
}
