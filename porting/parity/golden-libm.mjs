// Writes tests/golden/libm_<fn>.txt: every transcendental game5's engine calls, evaluated by
// the ORACLE's runtime. Each line is hex IEEE 754 bit patterns: the input(s), then the result.
// The C++ test (tests/core_libm_test.cpp) compares engine/core/libm against them bit for bit.
//
//   make golden-libm NODE=/path/to/node24
//
// It must run under game5's pinned Node major (../game5/.nvmrc, 24): V8's Math functions
// changed between majors (porting/research.md section 2). Math.pow in Node 24 is the HOST libm,
// so the tables must also come from game5's reference platform: Linux x86-64 with glibc. The
// script refuses anything else rather than writing tables the engine should not match.
//
// ge:summary Writes tests/golden/libm_*.txt under game5's pinned Node
// ge:keywords golden parity generator libm node24
// ge:skill testing
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { requireOracleNode } from "./oracle-node.mjs";

const argv = process.argv.slice(2);
const at = argv.indexOf("--game5");
const game5 = resolve(at >= 0 ? argv[at + 1] : "../game5");
const ROWS = 2048;
const banner = requireOracleNode(game5, "golden-libm");

// mulberry32 off a fixed seed, so the inputs are the same on every run.
let s = 20261021 >>> 0;
const r = () => {
  s = (s + 0x6d2b79f5) >>> 0;
  let t = s;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};
const U = (a, b) => a + (b - a) * r();
const mag = (lo, hi) => (r() < 0.5 ? -1 : 1) * 10 ** U(lo, hi);
const pick = (...fs) => fs[Math.floor(r() * fs.length)]();
const bitsView = new DataView(new ArrayBuffer(8));
const anyBits = () => {
  bitsView.setUint32(0, (r() * 2 ** 32) >>> 0);
  bitsView.setUint32(4, (r() * 2 ** 32) >>> 0);
  return bitsView.getFloat64(0);
};
const hex = (v) => {
  bitsView.setFloat64(0, v);
  return bitsView.getBigUint64(0).toString(16).padStart(16, "0");
};

const SPECIAL = [0, -0, Infinity, -Infinity, NaN, 1, -1, 0.5, -0.5, 2, 5e-324, -5e-324,
  2.2250738585072014e-308, 1.7976931348623157e308, -1.7976931348623157e308, Math.PI,
  Math.PI / 2, -Math.PI / 2, Math.PI / 4, 1e-300, 1e300, 709.782712893384, -745.1332191019411];
const near1 = () => (r() < 0.5 ? -1 : 1) * (1 - 10 ** U(-16, 0));
// Input generators by the domain each function cares about: game5's ranges, the reductions'
// awkward spots (multiples of pi/4, huge arguments), and arbitrary bit patterns.
const trig = () => pick(() => U(-10, 10), () => U(-7, 7), () => mag(-20, 3), () => mag(3, 22),
  () => (Math.floor(U(-64, 64)) * Math.PI) / 4 + U(-1e-9, 1e-9), anyBits);
const UNARY = {
  sin: [Math.sin, trig],
  cos: [Math.cos, trig],
  tan: [Math.tan, trig],
  asin: [Math.asin, () => pick(() => U(-1, 1), near1, () => mag(-20, 0))],
  acos: [Math.acos, () => pick(() => U(-1, 1), near1, () => mag(-20, 0))],
  atan: [Math.atan, () => pick(() => U(-10, 10), () => mag(-20, 20), anyBits)],
  exp: [Math.exp, () => pick(() => U(-10, 10), () => U(-50, 50), () => U(-746, 710), () => mag(-20, 0), anyBits)],
  expm1: [Math.expm1, () => pick(() => U(-10, 10), () => U(-50, 50), () => mag(-20, 0), anyBits)],
  log: [Math.log, () => pick(() => U(0, 10), () => Math.abs(mag(-300, 300)), () => 1 + mag(-16, -1), anyBits)],
  log2: [Math.log2, () => pick(() => U(0, 10), () => Math.abs(mag(-300, 300)), () => 1 + mag(-16, -1), () => 2 ** Math.floor(U(-1074, 1024)), anyBits)],
  cbrt: [Math.cbrt, () => pick(() => U(-10, 10), () => mag(-300, 300), () => Math.floor(U(-1000, 1000)) ** 3, anyBits)],
  tanh: [Math.tanh, () => pick(() => U(-10, 10), () => U(-25, 25), () => mag(-20, 0), anyBits)],
};
const anyArg = () => pick(() => U(-10, 10), () => mag(-20, 20), () => 0, anyBits);
const BINARY = {
  atan2: [Math.atan2, () => [anyArg(), anyArg()]],
  // game5's pow: fractional powers of [0, 2] (2/3, 1/3, 5.2559 ...), small integer powers of
  // signed bases, ** 2 / 3 / 4 / 0.5, extreme magnitudes, near-1 bases to large powers.
  pow: [Math.pow, () => pick(
    () => [U(0, 10), U(-5, 5)],
    () => [U(0, 2), pick(() => 2 / 3, () => 1 / 3, () => 1.2, () => 5.2559, () => 0.6, () => 1 / 0.9, () => U(0, 4))],
    () => [U(-10, 10), Math.floor(U(-10, 10))],
    () => [U(-1e3, 1e3), pick(() => 2, () => 3, () => 4, () => 0.5)],
    () => [Math.abs(mag(-300, 300)), U(-2, 2)],
    () => [1 + mag(-16, -1), mag(0, 18)],
    () => [U(0.01, 100), U(-1080, 1080) / Math.log(U(1.01, 100))],
    () => [anyBits(), anyBits()])],
};

const write = (name, lines) => {
  const path = resolve("tests/golden", `libm_${name}.txt`);
  writeFileSync(path, [banner, ...lines].join("\n") + "\n");
  console.log(`${path}: ${lines.length} rows`);
};
for (const [name, [fn, gen]] of Object.entries(UNARY)) {
  const rows = SPECIAL.map((x) => `${hex(x)} ${hex(fn(x))}`);
  while (rows.length < ROWS) {
    const x = gen();
    rows.push(`${hex(x)} ${hex(fn(x))}`);
  }
  write(name, rows);
}
for (const [name, [fn, gen]] of Object.entries(BINARY)) {
  const rows = [];
  for (const a of SPECIAL) for (const b of [0, -0, Infinity, -Infinity, NaN, 1, -1, 0.5, 2, 3, -3, 1 / 3])
    rows.push(`${hex(a)} ${hex(b)} ${hex(fn(a, b))}`);
  while (rows.length < ROWS) {
    const [a, b] = gen();
    rows.push(`${hex(a)} ${hex(b)} ${hex(fn(a, b))}`);
  }
  write(name, rows);
}
