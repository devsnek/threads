const { Worker } = require('bindings')('threads');
const fs = require('fs');
const path = require('path');
const EventEmitter = require('events');

const preload = fs.readFileSync(path.resolve(__dirname, '..', 'preload.build.js')).toString();

const kWorker = Symbol('kWorker');

/**
 * @typedef {Object} Thread~Context
 * @prop {Function} on
 * @prop {Function} send
 * @prop {Function} terminate
 * @prop {Function} lock
 * @prop {Function} unlock
 */

/**
 * @callback Thread~fnCallback
 * @param {...args} args Arguments from the {@link Thread} constructor
 * @param {Thread~Context} context
 */

class Thread extends EventEmitter {
  /**
   * Create a thread
   * @param {Thread~fnCallback} fn Function that will run in a new thread
   * @param {...*} props Values to pass to the thread callback
   */
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

  /**
   * Send a value to the thread
   * @param {*} value Value to send
   */
  send(value) {
    this[kWorker].send(value);
  }

  /**
   * Wait for the thread to finish and call `handler` with its return value.
   * @param {Function} handler Callback
   */
  join(handler) {
    this[kWorker].getPromise().then(handler);
  }

  /**
   * Handle any errors that happen during execution of the thread's code
   * @param {Function} handler Callback
   */
  catch(handler) {
    this[kWorker].getPromise().catch(handler);
  }

  /**
   * Terminate the thread
   */
  terminate() {
    this[kWorker].terminate();
  }

  /**
   * Lock the thread's context's mutex, analogous to std::mutex::try_lock
   * @returns {boolean} If the lock was successfully obtained
   */
  lock() {
    return this[kWorker].lock();
  }

  /**
   * Unlock the thread context's mutex, analogous to std::mutex::unlock
   */
  unlock() {
    this[kWorker].unlock();
  }
}

module.exports = Thread;
