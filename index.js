const { Worker } = require('bindings')('threads');

const kWorker = Symbol('kWorker');
class Thread {
  constructor(fn, ...props) {
    const worker = new Worker(`(${fn})`, props);
    Object.defineProperty(this, kWorker, {
      value: worker,
      writable: false,
      enumerable: false,
      configurable: false,
    });
  }

  join(handler) {
    this[kWorker].getPromise().then(handler);
  }

  catch(handler) {
    this[kWorker].getPromise().catch(handler);
  }
}

module.exports = Thread;
