// Writes tests/golden/mountain_generate.txt from the TypeScript ORACLE: game5's own generateLevel,
// on the maps M4a ports (generator v1, the single piste): LEVEL_SEEDS in the alpine, the
// benchmark's map, and one map in each region that lays a crust. The generator reaches sin,
// cos, atan2, acos, exp, log and pow, so this runs under game5's pinned Node on Linux x86-64
// (oracle-node.mjs refuses anything else).
//
//   <node 24> --experimental-strip-types tools/parity/golden-mapgen.mjs \
//     [--framework ../oss-game-framework] [--game5 ../game5]
//
// One row per map, words of text: `seed version region` then `key=value` pairs. `digest` is
// game5's own levelDigest and `attempt` the sub-seed the search accepted; every other value is
// a 64-bit FNV-1a (hex) over one SECTION of the level laid out as little-endian bytes, so a
// failing C++ test names the section that moved. The layout is tests/mountain_generate_test.cpp's
// `LevelBytes` (keep the two in step):
//   ground, packed, crust  the grids' float32 samples (crust "-" when the region lays none)
//   points                 x z y s heading width (f64) per station
//   gates                  x z y heading width s (f64), colour (u8: 0 red, 1 blue)
//   start                  spawn x z heading, then every grid slot's (f64)
//   trees                  x z y height radius age crown (f64), kind (u8, TREE_KINDS index,
//                          0 when absent), clump (i32, -1 when absent)
//   kickers                id bytes + 0, x z y heading height ramp landing width (f64),
//                          onTrack (u8), s (f64, NaN when absent)
//   cliffs                 id bytes + 0, x z y heading drop face apron landing shelf width (f64),
//                          onTrack (u8), s (f64, NaN when absent)
//   drifts                 from to (f64)
//   world                  mountain (summit xyz, base xyz, vertical altitude treeLine sea), sun
//                          (hour dayOfYear latitude), weather (kind u8 in WEATHER_KINDS order,
//                          snowfall fog wind windFrom, evening u8) — all f64 but the u8s
//   findings               the analysis' findings as "rule:severity" words joined by ','
//   log                    the debug lines the search wrote (the attempts it refused and why),
//                          UTF-8 as hex, joined by '\n' ("-" when it accepted the first)
//
// ge:summary Writes tests/golden/mountain_generate.txt from game5's own generateLevel (v1)
// ge:keywords golden parity generator mapgen generateLevel levelDigest
// ge:skill testing sim
import { writeFileSync } from "node:fs";
import { registerHooks } from "node:module";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { requireOracleNode } from "./oracle-node.mjs";

const argv = process.argv.slice(2);
const opt = (name, fallback) => {
  const at = argv.indexOf(name);
  return resolve(at >= 0 ? argv[at + 1] : fallback);
};
const fw = opt("--framework", "../oss-game-framework");
const game5 = opt("--game5", "../game5");
const banner = requireOracleNode(game5, "golden-mapgen");

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
const load = (path) => import(pathToFileURL(resolve(game5, path)).href);
const { generateLevel } = await load("engine/mapgen/generate.ts");
const { levelDigest } = await load("engine/mapgen/digest.ts");
const { analyzeLevel } = await load("engine/analysis/index.ts");
const { TREE_KINDS } = await load("engine/mapgen/regions.ts");
const { WEATHER_KINDS } = await load("engine/mapgen/weather.ts");
const { LEVEL_SEEDS } = await load("tests/support/levels.ts");
const { BENCHMARK } = await load("pwa/src/game/benchmark-plan.ts");
const output = await import(pathToFileURL(resolve(fw, "src/core/output.ts")).href);
let log = [];
output.setDebugEnabled(true);
output.setOutputSink((level, message) => {
  if (level === "debug") log.push(message);
});

// A little-endian byte writer, hashed as it goes (FNV-1a 64).
class Section {
  constructor() {
    this.h = 0xcbf29ce484222325n;
    this.view = new DataView(new ArrayBuffer(8));
  }
  byte(b) {
    this.h ^= BigInt(b & 0xff);
    this.h = (this.h * 0x100000001b3n) & 0xffffffffffffffffn;
  }
  bytes(u8) {
    for (const b of u8) this.byte(b);
  }
  f64(v) {
    this.view.setFloat64(0, v ?? NaN, true);
    for (let i = 0; i < 8; i++) this.byte(this.view.getUint8(i));
  }
  i32(v) {
    this.view.setInt32(0, v, true);
    for (let i = 0; i < 4; i++) this.byte(this.view.getUint8(i));
  }
  u8(v) {
    this.byte(v);
  }
  str(s) {
    for (let i = 0; i < s.length; i++) this.byte(s.charCodeAt(i));
    this.byte(0);
  }
  hex() {
    return this.h.toString(16).padStart(16, "0");
  }
}
const grid = (f) => {
  if (!f) return "-";
  const s = new Section();
  s.bytes(new Uint8Array(f.data.buffer, f.data.byteOffset, f.data.byteLength));
  return s.hex();
};
const each = (list, write) => {
  const s = new Section();
  for (const item of list) write(s, item);
  return s.hex();
};

const CASES = [
  ...LEVEL_SEEDS.map((seed) => ({ seed, region: "alpine" })),
  { seed: BENCHMARK.seed, region: BENCHMARK.region },
  { seed: 1, region: "fell" },
  { seed: 38, region: "continental" },
  // Seeds whose first attempts the analysis refuses (R6), so the search loop is held too.
  { seed: 20, region: "alpine" },
  { seed: 50, region: "alpine" },
];
const rows = [];
for (const { seed, region } of CASES) {
  log = [];
  const t = performance.now();
  const level = generateLevel(seed, { version: 1, region });
  const ms = performance.now() - t;
  const analysis = analyzeLevel(level);
  const words = [
    `${seed}`,
    "1",
    region,
    `digest=${levelDigest(level)}`,
    `attempt=${level.attempt}`,
    `ground=${grid(level.ground)}`,
    `packed=${grid(level.packed)}`,
    `crust=${grid(level.crust)}`,
    `points=${each(level.track.points, (s, p) => [p.x, p.z, p.y, p.s, p.heading, p.width].forEach((v) => s.f64(v)))}`,
    `gates=${each(level.checkpoints, (s, c) => {
      [c.x, c.z, c.y, c.heading, c.width, c.s].forEach((v) => s.f64(v));
      s.u8(c.colour === "red" ? 0 : 1);
    })}`,
    `start=${each([level.spawn, ...level.grid], (s, g) => [g.x, g.z, g.heading].forEach((v) => s.f64(v)))}`,
    `trees=${each(level.trees, (s, t) => {
      [t.x, t.z, t.y, t.height, t.radius, t.age, t.crown].forEach((v) => s.f64(v));
      s.u8(t.kind === undefined ? 0 : TREE_KINDS.indexOf(t.kind));
      s.i32(t.clump ?? -1);
    })}`,
    `kickers=${each(level.kickers, (s, k) => {
      s.str(k.id);
      [k.x, k.z, k.y, k.heading, k.height, k.ramp, k.landing, k.width].forEach((v) => s.f64(v));
      s.u8(k.onTrack ? 1 : 0);
      s.f64(k.s);
    })}`,
    `cliffs=${each(level.cliffs, (s, c) => {
      s.str(c.id);
      [c.x, c.z, c.y, c.heading, c.drop, c.face, c.apron, c.landing, c.shelf, c.width].forEach((v) =>
        s.f64(v),
      );
      s.u8(c.onTrack ? 1 : 0);
      s.f64(c.s);
    })}`,
    `drifts=${each(level.drifts, (s, d) => [d.from, d.to].forEach((v) => s.f64(v)))}`,
    `world=${each([level], (s, l) => {
      const m = l.mountain;
      [m.summit.x, m.summit.y, m.summit.z, m.base.x, m.base.y, m.base.z].forEach((v) => s.f64(v));
      [m.vertical, m.altitude, m.treeLine, m.sea].forEach((v) => s.f64(v));
      [l.sun.hour, l.sun.dayOfYear, l.sun.latitude].forEach((v) => s.f64(v));
      const w = l.weather;
      s.u8(WEATHER_KINDS.indexOf(w.kind));
      [w.snowfall, w.fog, w.wind, w.windFrom].forEach((v) => s.f64(v));
      s.u8(w.evening ? 1 : 0);
    })}`,
    `findings=${analysis.findings.map((f) => `${f.rule}:${f.severity}`).join(",") || "-"}`,
    `log=${log.length ? Buffer.from(log.join("\n"), "utf8").toString("hex") : "-"}`,
  ];
  rows.push(words.join(" "));
  console.log(`${seed} ${region}: ${levelDigest(level)} attempt ${level.attempt} in ${ms.toFixed(0)} ms`);
}
const path = resolve("tests/golden/mountain_generate.txt");
writeFileSync(path, [banner, ...rows].join("\n") + "\n");
console.log(`${path}: ${rows.length} rows`);
