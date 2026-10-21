// Writes tests/golden/racing_*.txt from the TypeScript ORACLE: oss-game-framework's
// src/racing/ (tape, records, standings) and, for one real ghost, game5 itself. Nothing here
// reaches a transcendental in the module under test, so any Node 22+ writes the same bytes;
// the ghost's provenance line still names the Node that rode it, because the bot's line (not
// the codec) depends on game5's libm.
//
//   node --experimental-strip-types porting/parity/golden-racing.mjs \
//     [--framework ../oss-game-framework] [--game5 ../game5]
//
// Files (hex words unless noted; doubles as IEEE 754 bit patterns):
//   racing_tape_axis.txt     kind v snapAxis axisToByte uint8(byte) byteToAxis(uint8)
//   racing_tape_stream.txt   TEXT: "enc kind n v... base64" and "dec steps text bytes"
//                            (text and bytes as hex strings, "-" when empty)
//   racing_ghost.txt         TEXT: game5's own ghost of a bot lap: "tape steps s1..s5", then
//                            "at step v1..v5" samples and "null step" past either end
//   racing_fingerprint.txt   n v1..vn digest   (n in hex too)
//   racing_standings.txt     0 done fx fz nx nz x z legProgress
//                            1 n (finished time progress)*n order*n placeAmong*n
//   racing_records.txt       0 value hasStanding standing better beats
//                            1 hasRecord hasSplits n splits... index time hasGap gap
//                            2 better id value at hasSplits n splits... record   (a noteRecord)
//                            3 rows (id value at hasSplits n splits...)*rows     (the book after)
//
// ge:summary Writes tests/golden/racing_* from the framework oracle and a game5 ghost
// ge:keywords golden parity generator racing tape ghost records standings
// ge:skill testing
import { writeFileSync } from "node:fs";
import { registerHooks } from "node:module";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";

const argv = process.argv.slice(2);
const opt = (name, fallback) => {
  const at = argv.indexOf(name);
  return resolve(at >= 0 ? argv[at + 1] : fallback);
};
const fw = opt("--framework", "../oss-game-framework");
const game5 = opt("--game5", "../game5");

// game5 spells the framework by package name and the engine as `@engine`; the framework writes
// extensionless relative imports. Map all three onto the checkouts beside this one.
const PKG = "@niclaslindstedt/oss-game-framework/";
registerHooks({
  resolve(spec, context, next) {
    if (spec.startsWith(PKG)) {
      return next(pathToFileURL(resolve(fw, "src", `${spec.slice(PKG.length)}.ts`)).href, context);
    }
    if (spec === "@engine") return next(pathToFileURL(resolve(game5, "engine/index.ts")).href, context);
    try {
      return next(spec, context);
    } catch (err) {
      if (err?.code !== "ERR_MODULE_NOT_FOUND" || !spec.startsWith(".")) throw err;
      return next(`${spec}.ts`, context);
    }
  },
});
const load = (root, path) => import(pathToFileURL(resolve(root, path)).href);

const tape = await load(fw, "src/racing/tape.ts");
const records = await load(fw, "src/racing/records.ts");
const standings = await load(fw, "src/racing/standings.ts");
const { createRng } = await load(fw, "src/core/prng.ts");

const view = new DataView(new ArrayBuffer(8));
const hex = (v) => {
  view.setFloat64(0, v);
  return view.getBigUint64(0).toString(16).padStart(16, "0");
};
const textHex = (s) => (s.length ? Buffer.from(s, "latin1").toString("hex") : "-");
const bytesHex = (b) => (b.length ? Buffer.from(b).toString("hex") : "-");
const out = (name, lines) => {
  const path = resolve("tests/golden", name);
  writeFileSync(path, lines.join("\n") + "\n");
  console.log(`${path}: ${lines.length} rows`);
};
const src = createRng(20261021);
const KINDS = ["signed", "lever", "flags"];

// AXES: every kind over its range, its edges, the half-steps Math.round decides, -0, NaN, inf.
const axis = [];
const special = [0, -0, 1, -1, 0.5, -0.5, 2, -2, 1e-300, -1e-300, NaN, Infinity, -Infinity];
for (let k = 0; k < 3; k++) {
  const vals = [...special];
  for (let i = -130; i <= 130; i++) vals.push(i / 127, i / 255, (i + 0.5) / 127, (i + 0.5) / 255);
  for (let i = 0; i < 200; i++) vals.push((src.next() - 0.5) * (k === 2 ? 1200 : 3));
  if (k === 2) vals.push(255, 256, 257, -1, -256, 2 ** 31, 2 ** 32 + 7, 3.7, -3.7);
  for (const v of vals) {
    const kind = KINDS[k];
    const byte = tape.axisToByte(v, kind);
    const u8 = new Uint8Array([byte])[0];
    axis.push([k, v, tape.snapAxis(v, kind), byte, u8, tape.byteToAxis(u8, kind)].map(hex).join(" "));
  }
}
out("racing_tape_axis.txt", axis);

// STREAMS: one-axis recordings (runs at the 255 cap, NaN, alternation, empty), then decodes of
// well-formed, short, long and damaged text (atob's forgiving rules: whitespace, missing
// padding, a stray character, a length of 4n+1).
const stream = [];
const recordOne = (k, values) => {
  const rec = tape.createTapeRecorder({ a: KINDS[k] });
  for (const v of values) rec.record({ a: v });
  const sealed = rec.seal();
  stream.push(["enc", k, values.length, ...values.map(hex), textHex(sealed.a)].join(" "));
};
const fill = (n, v) => Array.from({ length: n }, () => v);
for (let k = 0; k < 3; k++) {
  recordOne(k, []);
  recordOne(k, [0.25]);
  for (const n of [254, 255, 256, 510, 511, 777]) recordOne(k, fill(n, k === 2 ? 5 : 0.5));
  recordOne(k, [0, -0, 0, 1, 1, NaN, NaN, 1, 0.5, 0.5]);
  recordOne(k, [...fill(300, NaN), ...fill(3, 1)]);
  for (let r = 0; r < 12; r++) {
    const values = [];
    let v = 0;
    const n = Math.floor(src.next() * 900);
    for (let i = 0; i < n; i++) {
      if (src.next() < 0.08) v = k === 2 ? Math.floor(src.next() * 300) : (src.next() - 0.5) * 2.4;
      values.push(src.next() < 0.002 ? NaN : v);
    }
    recordOne(k, values);
  }
}
const decodeCase = (steps, text) =>
  stream.push(["dec", steps, textHex(text), bytesHex(tape.decodeStream(text, steps))].join(" "));
for (const [steps, text] of [
  [0, ""],
  [5, ""],
  [4, "AwcBCQ=="],
  [4, "AwcBCQ"],
  [4, "AwcBCQ="],
  [4, " Aw\tcB\nCQ\f=\r=  "],
  [9, "AwcBCQ=="],
  [2, "AwcBCQ=="],
  [6, "AwcB"],
  [6, "AwcBCQ==AwcB"],
  [6, "Aw=cBCQ"],
  [6, "AwcBC"],
  [6, "AwcB*Q=="],
  [6, "AAcBCQ=="],
  [6, "/wf/CQ=="],
  [700, "/wf/CQ=="],
  [6, "AwcBCR=="],
  [6, "AwcBCZ"],
  [6, "AwcBCZ=="],
  [6, "AwcBC==="],
  [6, "=AwcBCQ="],
  [3, "AQIDBAUGBwg="],
  [3, "AQIDBAUGBwgJ"],
]) {
  decodeCase(steps, text);
}
for (let i = 0; i < 40; i++) {
  const bytes = Array.from({ length: Math.floor(src.next() * 64) }, () => Math.floor(src.next() * 256));
  decodeCase(Math.floor(src.next() * 400), Buffer.from(bytes).toString("base64"));
}
out("racing_tape_stream.txt", stream);

// THE GHOST: game5's own recorder (pwa/src/game/ghost.ts) on a bot lap of the synthetic
// slope, exactly as game5's tests/ghost_test.ts rides it. The C++ test decodes the five
// streams, re-encodes them byte for byte and reads the samples back.
const E = await load(game5, "engine/index.ts");
const G = await load(game5, "pwa/src/game/ghost.ts");
const { syntheticLevel } = await load(game5, "tests/support/synthetic.ts");
const level = syntheticLevel({ laps: 1 });
const run = E.createGame({ level, seed: 7, mode: "timeTrial", laps: 1, quiet: true });
const rec = G.createControlRecorder();
for (let i = 0; i < 240 * E.TUNING.physicsHz && !run.progress.finished; i++) {
  const input = G.snapInput(E.botInput(run));
  rec.record(input);
  E.step(run, input);
}
if (!run.progress.finished) throw new Error("golden-racing: the bot did not finish its lap");
const ghost = rec.seal();
const NAMES = ["steer", "lean", "tuck", "brake", "flags"];
const SCHEMA = { steer: "signed", lean: "signed", tuck: "lever", brake: "lever", flags: "flags" };
const ghostRows = [
  `# node ${process.versions.node} game5 ghost of a bot lap on syntheticLevel({ laps: 1 }), seed 7`,
  ["tape", ghost.steps, ...NAMES.map((n) => ghost[n])].join(" "),
];
const reader = tape.readTape(ghost, SCHEMA);
const axes = {};
for (const s of [-1, ghost.steps, ghost.steps + 5]) {
  if (reader.at(s, axes) !== null) throw new Error("golden-racing: read past the tape");
  ghostRows.push(`null ${s}`);
}
for (let s = 0; s < ghost.steps; s += s < 300 ? 1 : 37) {
  reader.at(s, axes);
  ghostRows.push(["at", s, ...NAMES.map((n) => hex(axes[n]))].join(" "));
}
reader.at(ghost.steps - 1, axes);
ghostRows.push(["at", ghost.steps - 1, ...NAMES.map((n) => hex(axes[n]))].join(" "));
out("racing_ghost.txt", ghostRows);

// FINGERPRINTS: game5's mapPrint inputs for the slope (seed, length, every checkpoint), then
// awkward numbers: hundredths at the round's half, past int32, NaN and inf.
const prints = [];
const printOf = (vals) => {
  const p = tape.createFingerprint();
  for (const v of vals) p.mix(v);
  prints.push([vals.length.toString(16), ...vals.map(hex), p.digest()].join(" "));
};
printOf([]);
const mapVals = [level.seed, level.track.length];
for (const cp of level.checkpoints) mapVals.push(cp.x, cp.z);
printOf(mapVals);
if (prints[1].split(" ").at(-1) !== G.mapPrint(level)) throw new Error("golden-racing: mapPrint moved");
printOf([0.005, -0.005, 0.015, 1e9, -1e9, 2 ** 40, NaN, Infinity, -Infinity, -0]);
for (let i = 0; i < 60; i++) {
  printOf(Array.from({ length: Math.floor(src.next() * 30) }, () => (src.next() - 0.5) * 10 ** (src.next() * 12 - 3)));
}
out("racing_fingerprint.txt", prints);

// STANDINGS: legs (zero-length, behind the line, at the gate, NaN); fields with dead heats in
// time and progress, -0, riders home and out, of sizes 1..12.
const stand = [];
const leg = (done, fx, fz, nx, nz, x, z) =>
  stand.push(
    [0, done, fx, fz, nx, nz, x, z, standings.legProgress(done, { x: fx, z: fz }, { x: nx, z: nz }, x, z)]
      .map(hex)
      .join(" "),
  );
leg(0, 0, 0, 0, 0, 3, 4);
leg(2, 0, 0, 0, 10, 0, 10);
leg(2, 0, 0, 0, 10, 0, -5);
leg(1, 0, 0, 0, 10, 0, 9.999);
leg(1, 0, 0, NaN, 10, 0, 9);
leg(1, 0, 0, 0, 10, NaN, 9);
for (let i = 0; i < 400; i++) {
  const r = () => (src.next() - 0.5) * 400;
  leg(Math.floor(src.next() * 30), r(), r(), r(), r(), r(), r());
}
for (let i = 0; i < 400; i++) {
  const n = 1 + Math.floor(src.next() * 12);
  const field = Array.from({ length: n }, () => {
    const finished = src.next() < 0.4;
    const time = [61.25, 61.25, -0, 0][Math.floor(src.next() * 8)] ?? 50 + src.next() * 20;
    const progress = [3.5, 3.5, -0, 0][Math.floor(src.next() * 8)] ?? (src.next() - 0.1) * 30;
    return { finished, time, progress };
  });
  const order = standings.fieldOrder(field.map((_, k) => k), (k) => field[k]);
  const places = field.map((s, k) => standings.placeAmong(s, field.filter((_, j) => j !== k)));
  const row = [1, n];
  for (const s of field) row.push(s.finished ? 1 : 0, s.time, s.progress);
  stand.push([...row, ...order, ...places].map(hex).join(" "));
}
out("racing_standings.txt", stand);

// RECORDS: beats over non-figures and ties both ways; splitGap at every edge; a book grown by
// noteRecord, its whole state after each call.
const rec2 = [];
const figures = [NaN, Infinity, -Infinity, 0, -0, -1, 1e-300, 1, 61.25, 61.25000000000001, 120];
for (const better of [0, 1])
  for (const v of figures) {
    rec2.push([0, v, 0, 0, better, records.beats(v, null, better ? "higher" : "lower") ? 1 : 0].map(hex).join(" "));
    for (const s of figures) {
      const b = records.beats(v, { value: s }, better ? "higher" : "lower");
      rec2.push([0, v, 1, s, better, b ? 1 : 0].map(hex).join(" "));
    }
  }
const gap = (record, index, time) => {
  const g = records.splitGap(record, index, time);
  const head = record === null ? [0, 0, 0] : record.splits ? [1, 1, record.splits.length, ...record.splits] : [1, 0, 0];
  rec2.push([1, ...head, index, time, g === null ? 0 : 1, g ?? 0].map(hex).join(" "));
};
const splits = [10.5, NaN, 33.25, Infinity, 47];
for (const record of [null, {}, { splits: [] }, { splits }])
  for (const index of [-1, 0, 1, 2, 3, 4, 5]) for (const time of [11, NaN, -Infinity]) gap(record, index, time);
let book = {};
for (let i = 0; i < 300; i++) {
  const better = i < 150 ? 0 : 1;
  const id = Math.floor(src.next() * 9);
  const pick = src.next();
  const value = pick < 0.1 ? figures[Math.floor(src.next() * figures.length)] : 20 + Math.floor(src.next() * 40) / 4;
  const run = { value, at: 1.7e12 + i };
  if (src.next() < 0.6) run.splits = Array.from({ length: Math.floor(src.next() * 5) }, () => src.next() * 60);
  const res = records.noteRecord(book, `r/${id}`, run, better ? "higher" : "lower");
  const head = [2, better, id, value, run.at, run.splits ? 1 : 0, run.splits?.length ?? 0, ...(run.splits ?? [])];
  rec2.push([...head, res.record ? 1 : 0].map(hex).join(" "));
  book = res.book;
  const ids = Object.keys(book);
  const state = [3, ids.length];
  for (const k of ids) {
    const r = book[k];
    state.push(Number(k.slice(2)), r.value, r.at, r.splits ? 1 : 0, r.splits?.length ?? 0, ...(r.splits ?? []));
  }
  rec2.push(state.map(hex).join(" "));
}
out("racing_records.txt", rec2);
