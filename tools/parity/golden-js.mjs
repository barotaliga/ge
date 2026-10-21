// Writes tests/golden/core_js.txt: the JS number semantics engine/core/js.hpp ports, evaluated
// by Node. These are exact operations, the same on every Node version. Each line is hex:
//   x  y  Math.round(x)  x|0 (as uint32)  x>>>0  Math.sign(x)  Math.max(x,y)  Math.min(x,y)
//   x%y  Math.imul(x|0, y|0) (as uint32)
// with doubles as IEEE 754 bit patterns.
//
//   node tools/parity/golden-js.mjs
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";

let s = 20261021 >>> 0;
const r = () => {
  s = (s + 0x6d2b79f5) >>> 0;
  let t = s;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};
const U = (a, b) => a + (b - a) * r();
const pick = (...fs) => fs[Math.floor(r() * fs.length)]();
const view = new DataView(new ArrayBuffer(8));
const anyBits = () => {
  view.setUint32(0, (r() * 2 ** 32) >>> 0);
  view.setUint32(4, (r() * 2 ** 32) >>> 0);
  return view.getFloat64(0);
};
const hex = (v) => {
  view.setFloat64(0, v);
  return view.getBigUint64(0).toString(16).padStart(16, "0");
};
const u32 = (n) => (n >>> 0).toString(16);

const SPECIAL = [0, -0, 0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 0.49999999999999994, -0.49999999999999994,
  4503599627370495.5, -4503599627370495.5, 2 ** 31, -(2 ** 31), 2 ** 31 - 1, 2 ** 32, 2 ** 32 + 7.9,
  -(2 ** 32) - 3.2, 2 ** 53, 1e21, -1e21, 1e300, Infinity, -Infinity, NaN, 5e-324, -5e-324];
// Halves, integers, the int32/uint32 edges, huge magnitudes and arbitrary bits.
const gen = () => pick(
  () => Math.floor(U(-100, 100)) / 2,
  () => U(-1e3, 1e3),
  () => Math.floor(U(-(2 ** 33), 2 ** 33)),
  () => U(-(2 ** 60), 2 ** 60),
  () => (r() < 0.5 ? -1 : 1) * 10 ** U(-20, 25),
  () => pick(() => 0, () => -0, () => NaN),
  anyBits);

const rows = [];
const row = (x, y) =>
  rows.push([hex(x), hex(y), hex(Math.round(x)), u32(x | 0), u32(x >>> 0), hex(Math.sign(x)),
    hex(Math.max(x, y)), hex(Math.min(x, y)), hex(x % y), u32(Math.imul(x | 0, y | 0))].join(" "));
for (const x of SPECIAL) for (const y of [0, -0, NaN, 1, -3.5]) row(x, y);
while (rows.length < 4096) row(gen(), gen());
const path = resolve("tests/golden/core_js.txt");
writeFileSync(path, rows.join("\n") + "\n");
console.log(`${path}: ${rows.length} rows`);
