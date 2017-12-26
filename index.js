const { run } = require('bindings')('threads');

const kRunPromise = Symbol('kRunPromise');
class Thread {
  constructor(fn, ...props) {
    const promise = run(`(${fn})`, props);
    Object.defineProperty(this, kRunPromise, {
      value: promise,
      writable: false,
      enumerable: false,
      configurable: false,
    });
  }

  join(handler) {
    this[kRunPromise].then(handler);
  }

  catch(handler) {
    this[kRunPromise].catch(handler);
  }
}

module.exports = Thread;
