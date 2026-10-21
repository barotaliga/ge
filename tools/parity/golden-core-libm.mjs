// Writes tests/golden/core_quat.txt and core_solar.txt: the core modules that reach a
// transcendental (Math.sin, atan2, asin, tan, **), evaluated by the ORACLE's runtime, so it
// runs under game5's pinned Node on Linux x86-64 like golden-libm.mjs (oracle-node.mjs says
// why). Each line is hex IEEE 754 bit patterns, inputs then outputs; booleans are 0 or 1 and
// an absent optional (`null`) is the pair `0 0` where a present one is `1 value`.
//
//   make golden-libm NODE=/path/to/node24 [FRAMEWORK=../oss-game-framework]
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { oracleLoader } from "./oracle-import.mjs";
import { requireOracleNode } from "./oracle-node.mjs";

const argv = process.argv.slice(2);
const arg = (name, fallback) => {
  const at = argv.indexOf(name);
  return resolve(at >= 0 ? argv[at + 1] : fallback);
};
const game5 = arg("--game5", "../game5");
const fw = arg("--framework", "../oss-game-framework");
const banner = requireOracleNode(game5, "golden-core-libm");
const load = oracleLoader(fw);
const { createRng } = await load("src/core/prng.ts");
const { hypot3 } = await load("src/core/math.ts");
const q = await load("src/core/quat.ts");
const solar = await load("src/core/solar.ts");

const view = new DataView(new ArrayBuffer(8));
const hex = (v) => {
  view.setFloat64(0, typeof v === "boolean" ? (v ? 1 : 0) : v);
  return view.getBigUint64(0).toString(16).padStart(16, "0");
};
const opt = (v) => (v === null ? [0, 0] : [1, v]);
const win = (w) => (w === null ? [0, 0, 0] : [1, w.min, w.max]);
const out = (name, lines) => {
  const path = resolve("tests/golden", name);
  writeFileSync(path, [banner, ...lines].join("\n") + "\n");
  console.log(`${path}: ${lines.length} rows`);
};
const src = createRng(20261021);
const U = (a, b) => a + (b - a) * src.next();
const quat4 = (x) => [x.x, x.y, x.z, x.w];
const vec3 = (v) => [v.x, v.y, v.z];

// quat, one row per case:
//   in:  heading pitch roll  ax ay az angle  wx wy wz dt  vx vy vz  s
//   out: fromEuler(h,p,r)[4]  fromAxisAngle(a, angle)[4]  multiply(qe, qa)[4]
//        rotate(qe, v)[3]  unrotate(qe, v)[3]  integrate(qe, w, dt)[4]  toEuler(qe)[3]
//        normalize(qe * s)[4]
// Angles across every quadrant and past +-pi, pitch at exactly +-pi/2 (the gimbal), rates under
// integrate's 1e-9 cut-off, and a zero quaternion for normalize's `|| 1`.
const quat = [];
for (let i = 0; i < 800; i++) {
  const k = src.next();
  const heading = k < 0.1 ? Math.round(U(-4, 4)) * (Math.PI / 2) : U(-7, 7);
  const pitch = k > 0.9 ? (src.next() < 0.5 ? -1 : 1) * (Math.PI / 2) : U(-2, 2);
  const roll = k > 0.85 && k <= 0.9 ? Math.PI : U(-4, 4);
  let ax = U(-1, 1), ay = U(-1, 1), az = U(-1, 1);
  const n = hypot3(ax, ay, az);
  ax /= n; ay /= n; az /= n;
  const angle = U(-10, 10);
  const tiny = src.next() < 0.05;
  const wx = tiny ? U(-1, 1) * 1e-10 : U(-12, 12);
  const wy = tiny ? 0 : U(-12, 12);
  const wz = tiny ? 0 : U(-12, 12);
  const dt = [1 / 120, 1 / 60, U(0, 0.5)][Math.floor(src.next() * 3)];
  const vx = U(-50, 50), vy = U(-50, 50), vz = U(-50, 50);
  const s = i === 0 ? 0 : U(0.5, 2);
  const qe = q.fromEuler(heading, pitch, roll);
  const qa = q.fromAxisAngle(ax, ay, az, angle);
  const e = q.toEuler(qe);
  const scaled = q.normalize({ x: qe.x * s, y: qe.y * s, z: qe.z * s, w: qe.w * s });
  const v = { x: vx, y: vy, z: vz };
  quat.push(
    [heading, pitch, roll, ax, ay, az, angle, wx, wy, wz, dt, vx, vy, vz, s]
      .concat(quat4(qe), quat4(qa), quat4(q.multiply(qe, qa)))
      .concat(vec3(q.rotate(qe, v)), vec3(q.unrotate(qe, v)))
      .concat(quat4(q.integrate(qe, wx, wy, wz, dt)), [e.heading, e.pitch, e.roll], quat4(scaled))
      .map(hex)
      .join(" "),
  );
}
out("core_quat.txt", quat);

// solar, one row per case:
//   in:  hour lat dec age year doy lon minEl el
//   out: sunAt(hour, lat, dec)[elevation azimuth rising]
//        hourOfElevation(minEl, true, lat, dec)[2]  hourOfElevation(minEl, false, lat, dec)[2]
//        daylightWindow(lat, minEl, dec)[3]
//        moonAt(hour, lat, dec, age)[elevation azimuth rising lit]
//        jd0 = julianDayOfYear(year, doy)  julianDay(year, month, day)  (month, day off doy)
//        solarCoordinates(jd0 + hour / 24)[declination equationOfTime longitude obliquity]
//        refraction(el)  universalHourOf(jd0, hour, lon)
//        sunOnDate(hour, lat, lon, jd0)[elevation azimuth rising trueElevation declination]
//        sunWindowOnDate(lat, lon, jd0)[3]  sunWindowOnDate(lat, lon, jd0, minEl)[3]
//        lunarCoordinates(jd0 + hour / 24)[longitude latitude distance rightAscension declination]
//        moonOnDate(hour, lat, lon, jd0)[elevation azimuth rising lit age declination]
// Latitudes from the equator to the poles both sides (the polar day and night take the
// window's null and whole-clock branches), hours past 24 and negative, both solstices.
const sun = [];
for (let i = 0; i < 600; i++) {
  const k = src.next();
  const hour = k < 0.05 ? U(-30, 50) : U(0, 24);
  const lat = k < 0.6 ? U(35, 70) : k < 0.8 ? U(-90, 90) : (src.next() < 0.5 ? -1 : 1) * U(66, 89.9);
  const dec = k > 0.95 ? (src.next() < 0.5 ? -23.44 : 23.44) : U(-23.44, 23.44);
  const age = U(-5, 60);
  const year = Math.floor(U(1990, 2061));
  const doy = Math.floor(U(1, 367));
  const lon = U(-180, 180);
  const minEl = U(-0.32, 0.6);
  const el = k < 0.3 ? U(-0.2, 0.2) : U(-0.4, 1.6);
  const month = 1 + (doy % 12);
  const day = 1 + (doy % 28);
  const jd0 = solar.julianDayOfYear(year, doy);
  const jd = jd0 + hour / 24;
  const at = solar.sunAt(hour, lat, dec);
  const moon = solar.moonAt(hour, lat, dec, age);
  const sc = solar.solarCoordinates(jd);
  const dated = solar.sunOnDate(hour, lat, lon, jd0);
  const lc = solar.lunarCoordinates(jd);
  const dm = solar.moonOnDate(hour, lat, lon, jd0);
  sun.push(
    [hour, lat, dec, age, year, doy, lon, minEl, el]
      .concat([at.elevation, at.azimuth, at.rising])
      .concat(opt(solar.hourOfElevation(minEl, true, lat, dec)))
      .concat(opt(solar.hourOfElevation(minEl, false, lat, dec)))
      .concat(win(solar.daylightWindow(lat, minEl, dec)))
      .concat([moon.elevation, moon.azimuth, moon.rising, moon.lit])
      .concat([jd0, solar.julianDay(year, month, day)])
      .concat([sc.declination, sc.equationOfTime, sc.longitude, sc.obliquity])
      .concat([solar.refraction(el), solar.universalHourOf(jd0, hour, lon)])
      .concat([dated.elevation, dated.azimuth, dated.rising, dated.trueElevation, dated.declination])
      .concat(win(solar.sunWindowOnDate(lat, lon, jd0)))
      .concat(win(solar.sunWindowOnDate(lat, lon, jd0, minEl)))
      .concat([lc.longitude, lc.latitude, lc.distance, lc.rightAscension, lc.declination])
      .concat([dm.elevation, dm.azimuth, dm.rising, dm.lit, dm.age, dm.declination])
      .map(hex)
      .join(" "),
  );
}
out("core_solar.txt", sun);
