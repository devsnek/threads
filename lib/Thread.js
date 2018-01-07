const { Worker, constants, setPreload } = require('bindings')('threads');
const fs = require('fs');
const path = require('path');
const EventEmitter = require('events');
const util = require('util');

setPreload(fs.readFileSync(path.resolve(__dirname, '..', 'preload.build.js')).toString());

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
   * @param {Array<*>} props Values to pass to the thread callback
   * @param {Object} references References to functions
   */
  constructor(fn, props = [], references = {}) {
    super();

    if (typeof props === 'object' && !Array.isArray(props)) {
      references = props;
      props = [];
    }

    if (!Array.isArray(props))
      throw new TypeError('props must be an array');


    for (const key of Object.keys(references)) {
      const v = references[key];
      if (!v || (typeof v !== 'object' && typeof v !== 'function'))
        throw new TypeError('references must be functions or objects');
    }

    const worker = new Worker(`(${fn})`, props, references);

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

    const messageBuffer = [false];
    this.on('newListener', (event) => {
      if (event !== 'message')
        return;

      const tickHandle = () => {
        worker.checkOutgoingMessages(messageBuffer);
        if (messageBuffer[0]) {
          this.emit('message', messageBuffer[1]);
          messageBuffer[0] = false;
        }

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
    return `Thread ${this.id.toString().padStart(2, 0)} { ${this.state} }`;
  }
}

module.exports = Thread;
