// https://github.com/Microsoft/napajs/blob/master/examples/tutorial/estimate-pi-in-parallel/estimate-pi-in-parallel.js

const Thread = require('..');

function estimatePI(points) {
  let inside = 0;

  for (let i = points; i > 0; i--) {
    const x = Math.random();
    const y = Math.random();
    if ((x * x) + (y * y) <= 1)
      inside++;
  }

  return inside / points * 4;
}

async function run(points, workers) {
  const start = Date.now();

  const promises = Array.from({ length: workers }, () => new Thread(estimatePI, [points / workers]).join());
  const values = await Promise.all(promises);

  let aggregate = 0;
  for (const result of values)
    aggregate += result;

  const pi = aggregate / workers;
  const ms = Date.now() - start;
  const dev = Math.abs(pi - Math.PI).toPrecision(7);

  console.log(`\t${points}\t${workers}\t\t${ms}\t\t${pi.toPrecision(7)}\t${dev}`);
}

console.log();
console.log('\t# of points\t# of threads\tlatency in MS\testimated π\tdeviation');
console.log('\t---------------------------------------------------------------------------------------');

(async function doit() {
  const p = 40000000;
  await run(p, 1);
  await run(p, 2);
  await run(p, 4);
  await run(p, 8);
  await run(p, 16);
  await run(p, 32);
}());
