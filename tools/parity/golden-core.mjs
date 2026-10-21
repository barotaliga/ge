// Writes tests/golden/core_*.txt from the TypeScript ORACLE — the
// oss-game-framework sources the C++ core is ported from. Each line is
// whitespace-separated hex: inputs, then outputs, as IEEE 754 bit patterns,
// so the C++ test compares bits and nothing is lost to decimal printing.
//
//   node --experimental-strip-types tools/parity/golden-core.mjs [--framework ../oss-game-framework]
//
// Re-run only when the oracle moves (a framework tag bump); a diff in the
// golden files is then the change to review.
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const argv = process.argv.slice(2);
const at = argv.indexOf("--framework");
const fw = resolve(at >= 0 ? argv[at + 1] : "../oss-game-framework");
const load = (p) => import(pathToFileURL(resolve(fw, p)).href);
const { createRng } = await load("src/core/prng.ts");
const { hypot, hypot3, hypot4, smoothstep } = await load("src/core/math.ts");

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
      .map(hex)
      .join(" "),
  );
}
out("core_math.txt", math);
