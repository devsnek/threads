const { Worker, constants } = require('bindings')('threads');
const fs = require('fs');
const path = require('path');
const EventEmitter = require('events');
const util = require('util');

const preload = fs.readFileSync(path.resolve(__dirname, '..', 'preload.build.js')).toString();

const kWorker = Symbol('kWorker');
const stateKeys = Object.keys(constants);

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

    Object.defineProperties(this, {
      [kWorker]: {
        value: worker,
        writable: false,
        enumerable: false,
        configurable: false,
      },
      id: {
        value: worker.getId(),
        writable: false,
        enumerable: true,
        configurable: false,
      },
    });

    this.on('newListener', (event) => {
      if (event !== 'message')
        return;

      const tickHandle = () => {
        const [has, value] = worker.checkOutgoingMessages();

        if (has)
          this.emit('message', value);

        if (worker.getState() === constants.running)
          process.nextTick(tickHandle);
      };
      process.nextTick(tickHandle);
    });
  }

  get state() {
    return stateKeys[this[kWorker].getState()];
  }

  /**
   * Send a value to the thread
   * @param {*} value Value to send
   */
  send(value) {
    this[kWorker].send(value);
  }

  /**
   * Return a promise that resolves when the thread finishes with the return value
   * or rejects when there is an execution error in the thread.
   * @returns {Promise<*>}
   */
  join() {
    return this[kWorker].getPromise();
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

  [util.inspect.custom]() {
    return { id: this.id, state: this.state };
  }
}

module.exports = Thread;
