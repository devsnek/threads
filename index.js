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

  send(data) {
    this[kWorker].send(data);
  }

  join(handler) {
    this[kWorker].getPromise().then(handler);
  }

  catch(handler) {
    this[kWorker].getPromise().catch(handler);
  }

  terminate() {
    this[kWorker].terminate();
  }

  on(event, handler) {
    if (event !== 'message')
      return;

    const worker = this[kWorker];

    const tickHandle = () => {
      const [has, val] = worker.checkOutgoingMessages();

      if (has)
        handler(val);

      if (worker.isRunning())
        process.nextTick(tickHandle);
    };
    process.nextTick(tickHandle);
  }
}

module.exports = Thread;
