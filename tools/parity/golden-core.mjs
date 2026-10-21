// Writes tests/golden/core_*.txt and loop_*.txt from the TypeScript ORACLE —
// the oss-game-framework sources the C++ core is ported from — for every
// module that reaches no transcendental (those are golden-core-libm.mjs's,
// under game5's pinned Node). Each line is whitespace-separated hex: inputs,
// then outputs, as IEEE 754 bit patterns, so the C++ test compares bits and
// nothing is lost to decimal printing. core_output.txt is text: a transcript.
//
//   node --experimental-strip-types tools/parity/golden-core.mjs [--framework ../oss-game-framework]
//
// Re-run only when the oracle moves (a framework tag bump); a diff in the
// golden files is then the change to review.
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { oracleLoader } from "./oracle-import.mjs";

const argv = process.argv.slice(2);
const at = argv.indexOf("--framework");
const fw = resolve(at >= 0 ? argv[at + 1] : "../oss-game-framework");
const load = oracleLoader(fw);
const { createRng } = await load("src/core/prng.ts");
const { hypot, hypot3, hypot4, smoothstep, lerp, angleDiff, approach } = await load(
  "src/core/math.ts",
);
const { hash2, valueNoise } = await load("src/core/noise.ts");
const hf = await load("src/core/heightfield.ts");
const output = await load("src/core/output.ts");
const { createRunClock } = await load("src/loop/run-clock.ts");

const view = new DataView(new ArrayBuffer(8));
const hex = (v) => {
  view.setFloat64(0, v);
  return view.getBigUint64(0).toString(16).padStart(16, "0");
};
const out = (name, lines) => {
  const path = resolve("tests/golden", name);
  writeFileSync(path, lines.join("\n") + "\n");
  console.log(`${path}: ${lines.length} rows`);
};

// PRNG: per seed, the first 64 draws of next(), then int(-5, 17) and range(-3, 9).
const prng = [];
for (const seed of [0, 1, 7, 38, 12345, 0x7fffffff, 0xffffffff, 2654435769]) {
  const r = createRng(seed);
  const row = [seed.toString(16)];
  for (let i = 0; i < 64; i++) row.push(hex(r.next()));
  for (let i = 0; i < 16; i++) row.push(hex(r.int(-5, 17)));
  for (let i = 0; i < 16; i++) row.push(hex(r.range(-3, 9)));
  prng.push(row.join(" "));
}
out("core_prng.txt", prng);

// hypot family + smoothstep over awkward magnitudes, drawn off a fixed stream.
const src = createRng(20261021);
const mag = () => (src.next() - 0.5) * 10 ** Math.floor(src.next() * 40 - 20);
const math = [];
for (let i = 0; i < 4000; i++) {
  const a = mag(), b = mag(), c = mag(), d = mag();
  const lo = -Math.abs(c), hi = Math.abs(d);
  math.push(
    [a, b, c, d, hypot(a, b), hypot3(a, b, c), hypot4(a, b, c, d), smoothstep(lo, hi, a)]
      .concat([lerp(a, b, c), approach(a, b, Math.abs(c))])
      .map(hex)
      .join(" "),
  );
}
out("core_math.txt", math);

// angleDiff: a, b, angleDiff(a, b) — angles in game5's ranges, exact multiples of pi (the
// (-pi, pi] boundary), huge and special values.
const angles = [];
const SPECIAL = [0, -0, Math.PI, -Math.PI, 2 * Math.PI, -2 * Math.PI, 3 * Math.PI, 1e300, NaN, Infinity];
for (const a of SPECIAL) for (const b of SPECIAL) angles.push([a, b, angleDiff(a, b)].map(hex).join(" "));
for (let i = 0; i < 2000; i++) {
  const pick = src.next();
  const a = pick < 0.4 ? (src.next() - 0.5) * 40 : pick < 0.7 ? Math.round((src.next() - 0.5) * 16) * Math.PI : mag();
  const b = pick < 0.5 ? (src.next() - 0.5) * 40 : a + (Math.round((src.next() - 0.5) * 8) * Math.PI);
  angles.push([a, b, angleDiff(a, b)].map(hex).join(" "));
}
out("core_math_angle.txt", angles);

// noise: ix iz seed hash2(ix, iz, seed) | x z scale vseed valueNoise(x, z, scale, vseed).
// Integer lattice points, game5's `seed ^ SALT | 0` words, uint32 seeds past 2^31 (their
// product passes 2^53 and rounds), fractions, NaN and infinities. Rows come in runs of 40
// sharing a scale and seed, walking in small steps, so the C++ test can read each run through
// one NoiseField as game5's bakes do and hold it to the same column.
const noise = [];
const odd = [NaN, Infinity, -Infinity, 0, -0, 2 ** 31, -(2 ** 31), 2 ** 32 - 1, 2 ** 53, 0.5, -1.5];
let scale = 1, vseed = 0, x = 0, z = 0;
for (let i = 0; i < 4000; i++) {
  const k = src.next();
  const ix = k < 0.05 ? odd[i % odd.length] : Math.floor((src.next() - 0.5) * (k < 0.5 ? 400 : 4e6));
  const iz = k > 0.95 ? odd[(i * 7) % odd.length] : Math.floor((src.next() - 0.5) * 4e6);
  const seed = k < 0.3 ? (src.next() * 2 ** 32) >>> 0 : k < 0.6 ? (src.next() * 2 ** 32) | 0 : k < 0.65 ? src.next() * 1e6 : Math.floor(src.next() * 100);
  if (i % 40 === 0) {
    scale = [0.3, 1, 7.5, 37, 120, 500, 2000][Math.floor(src.next() * 7)];
    vseed = Math.floor(src.next() * 2 ** 32);
    x = (src.next() - 0.5) * 2e4;
    z = (src.next() - 0.5) * 2e4;
  } else {
    x += (src.next() - 0.5) * scale * 0.6;
    z += (src.next() - 0.5) * scale * 0.6;
  }
  noise.push([ix, iz, seed, hash2(ix, iz, seed), x, z, scale, vseed, valueNoise(x, z, scale, vseed)].map(hex).join(" "));
}
out("core_noise.txt", noise);

// heightfield: per field, a header row  originX originZ cell cols rows nSamples, the data row
// (cols*rows samples as fillField stored them, widened to double), then nSamples rows of
//   x z sampleField  sampleFieldGradient[0..2]  fieldGradient.gx .gz
// The data comes from fillField over a function the C++ test restates, so the float stores
// are checked too. Samples fall inside, on cell edges, past every border, and NaN.
const fieldFn = (x, z) => valueNoise(x, z, 37, 5) * 100 + x * 0.013 - z * 0.007 + 1e-3 / (1 + x * x);
const heights = [];
for (const [ox, oz, cell, cols, rows] of [
  [-120.5, 33.25, 2, 64, 48],
  [0, 0, 0.75, 17, 1],
  [5, -7, 3, 1, 9],
  [0, 0, 10, 1, 1],
  [1000.125, -2048, 12.5, 40, 40],
]) {
  const field = hf.createHeightfield(ox, oz, cell, cols, rows);
  hf.fillField(field, fieldFn);
  const samples = [];
  const w = cols * cell, h = rows * cell;
  for (let i = 0; i < 300; i++) {
    const k = src.next();
    let x = ox + (src.next() * 1.4 - 0.2) * w;
    let z = oz + (src.next() * 1.4 - 0.2) * h;
    if (k < 0.15) x = ox + Math.floor(src.next() * cols) * cell; // on a column
    if (k > 0.85) z = oz + Math.floor(src.next() * rows) * cell; // on a row
    if (i === 0) x = NaN;
    if (i === 1) z = NaN;
    const g = new Float64Array(3);
    hf.sampleFieldGradient(field, x, z, g);
    const fg = hf.fieldGradient(field, x, z);
    samples.push([x, z, hf.sampleField(field, x, z), g[0], g[1], g[2], fg.gx, fg.gz].map(hex).join(" "));
  }
  heights.push([ox, oz, cell, cols, rows, samples.length].map(hex).join(" "));
  heights.push(Array.from(field.data, hex).join(" "));
  heights.push(...samples);
}
out("core_heightfield.txt", heights);

// run clock: per clock, a header row  hz maxFrame nOps, then nOps rows of
//   op arg  steps alpha dropped paused   (op 0 = frame(arg), 1 = pause, 2 = resume)
// Frames at display rates with jitter, stalls past the clamp, negative and zero deltas.
const clocks = [];
for (const [hz, maxFrame] of [[120, 0.1], [60, 0.1], [120, 0.05], [90, 0.25]]) {
  const clock = maxFrame === 0.1 ? createRunClock(hz) : createRunClock(hz, maxFrame);
  const ops = [];
  for (let i = 0; i < 600; i++) {
    const k = src.next();
    let op = 0, arg = 0, steps = 0;
    if (k < 0.03) op = 1;
    else if (k < 0.06) op = 2;
    else {
      const rate = [60, 120, 144, 30, 240, 59.94][Math.floor(src.next() * 6)];
      arg = k < 0.1 ? src.next() * 2 : k < 0.12 ? -src.next() : k < 0.14 ? 0 : (1 / rate) * (1 + (src.next() - 0.5) * 0.2);
    }
    if (op === 0) steps = clock.frame(arg);
    else if (op === 1) clock.pause();
    else clock.resume();
    ops.push([op, arg, steps, clock.alpha(), clock.dropped(), clock.paused() ? 1 : 0].map(hex).join(" "));
  }
  clocks.push([hz, maxFrame, ops.length].map(hex).join(" "));
  clocks.push(...ops);
}
out("loop_run_clock.txt", clocks);

// output: a transcript, one line per thing a host sees. tests/core_output_test.cpp replays
// the SAME script (keep the two in step) and must print the same lines.
//   sink<n> <level> <message>   a line the attached sink n received
//   recent <count>              recentLogs().length, then one "recent <level> <message>" each
const transcript = [];
const sinkTo = (n) => (level, message) => transcript.push(`sink${n} ${level} ${message}`);
const dumpRecent = () => {
  const recent = output.recentLogs();
  transcript.push(`recent ${recent.length}`);
  for (const line of recent) transcript.push(`recent ${line.level} ${line.message}`);
};
for (let i = 0; i < 5; i++) output.status(`boot ${i}`);
output.debug("dropped before any sink");
output.setOutputSink(sinkTo(1));
output.info("info line");
output.warn("warn line");
output.error("error line");
output.header("Level");
output.debug("dropped while debug is off");
output.setDebugEnabled(true);
output.debug("kept while debug is on");
output.setDebugEnabled(false);
output.debug("dropped again");
dumpRecent();
for (let i = 0; i < 250; i++) output.status(`fill ${i}`);
output.setOutputSink(null);
output.warn("emitted with no sink");
output.setOutputSink(sinkTo(2));
dumpRecent();
out("core_output.txt", transcript);
