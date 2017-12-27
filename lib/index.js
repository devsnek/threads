const { Worker } = require('bindings')('threads');
const fs = require('fs');
const path = require('path');
const EventEmitter = require('events');

const preload = fs.readFileSync(path.resolve(__dirname, '..', 'preload.build.js')).toString();

const kWorker = Symbol('kWorker');
class Thread extends EventEmitter {
  constructor(fn, ...props) {
    super();
    const worker = new Worker(`(${fn})`, props, preload);

    Object.defineProperty(this, kWorker, {
      value: worker,
      writable: false,
      enumerable: false,
      configurable: false,
    });

    this.on('newListener', (event) => {
      if (event !== 'message')
        return;

      const tickHandle = () => {
        const [has, value] = worker.checkOutgoingMessages();

        if (has)
          this.emit('message', value);

        if (worker.isRunning())
          process.nextTick(tickHandle);
      };
      process.nextTick(tickHandle);
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

  lock() {
    return this[kWorker].lock();
  }

  unlock() {
    return this[kWorker].unlock();
  }
}

module.exports = Thread;
