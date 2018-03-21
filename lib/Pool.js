'use strict';

const Thread = require('.');

const kFn = Symbol('fn');

class Pool {
  constructor(fn) {
    this[kFn] = fn;
  }

  run(count, args) {
    return Array.from({ length: count }, () => new Thread(this[kFn], ...args).join());
  }
}

module.exports = Pool;
