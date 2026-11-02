// LAB `se-kills`: WHERE THE KILLS HAPPEN ON A MAP, compiled from one recorded game or hundreds.
// Every demo in `--dir` (or `--file`; by default four scripted matches written in code on the
// fixture halls) is scanned (client/demo_scan.hpp) for its kills (where the killer and the victim
// stood, the weapon in hand, the rocket or grenade that did it) and for every rocket and grenade
// flown, hit or miss, with its shooter. Demos of another map than the one read are skipped.
//
// The NOISE FILTER: the map is cut into cubes of `--cell` units; a kill's SHOT is the pair of
// cubes the killer and the victim stood in, a flight's the pair it went from and to. A pair seen
// `--gold` times or more (by default one in 2000 of the kills, or flights, shown; 3 at least)
// is a GOLD SHOT: a shot players make again and again. What is left is noise and is drawn faint or
// not at all. `--weapon` keeps one weapon's kills (and, for the rocket or grenade launcher, its
// flights).
//
// What it shows: a table of each weapon's kills (share, distance, in the air); every kill as a line
// from the killer to the victim over the map's wireframe, from above and at an angle, coloured by
// weapon; the gold shots alone, thick by how often, numbered as in their table (xyz of both ends);
// one small map per weapon with its gold shots; where players die and where rockets land as heat
// over the plan; every rocket's flight; the top gold shot seen from inside the map from behind the
// killer; each weapon's distances. kills.tsv and gold.tsv beside the report hold every kill's and
// gold shot's xyz for a bot's tables. Deterministic: the digest holds the counts.
//
// ge:summary Lab: kills and rocket flights from many demos over a map's wireframe, gold shots
// ge:keywords lab se-kills kills deaths shots gold-shots kill-map heatmap rockets grenades weapon
// ge:keywords filter cluster demos many-demos wireframe impacts aim-spots
// ge:skill shooter labs
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/wire_inside.hpp"
#include "shooters_edge/labs/wire_view.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace la = shooters_edge::labs;
namespace hud = shooters_edge::hud;
using ge::core::num::to_fixed;

using In = std::vector<std::pair<std::string, std::string>>;

constexpr int kWeapons = 8;  // hud::Weapon's values, the melee one first
const std::array<const char*, kWeapons> kWeaponName = {"melee",   "shotgun",       "double shotgun",
                                                       "nailgun", "heavy nailgun", "grenades",
                                                       "rockets", "beam"};
const std::array<const char*, kWeapons> kWeaponFlag = {"melee", "sg", "ssg", "ng",
                                                       "sng",   "gl", "rl",  "lg"};
const std::array<gl::Rgba, kWeapons> kWeaponColour = {{{110, 110, 110, 255},
                                                       {150, 100, 40, 255},
                                                       {200, 140, 20, 255},
                                                       {30, 150, 150, 255},
                                                       {30, 90, 200, 255},
                                                       {40, 160, 50, 255},
                                                       {215, 40, 30, 255},
                                                       {150, 60, 200, 255}}};

gl::Rgba with_alpha(gl::Rgba c, int a) {
  c.a = static_cast<uint8_t>(std::clamp(a, 0, 255));
  return c;
}

// A kill kept: its demo, and the scan's record.
struct Kill {
  int demo = 0;
  cl::DemoKill k;
};
struct Flight {
  int demo = 0;
  cl::DemoFlight f;
};

using Cube = std::array<int32_t, 3>;
Cube cube_of(sh::Vec3f p, double cell) {
  return {static_cast<int32_t>(std::floor(p.x / cell)),
          static_cast<int32_t>(std::floor(p.y / cell)),
          static_cast<int32_t>(std::floor(p.z / cell))};
}

// A shot seen again and again: the pair of cubes, how often, its ends' means.
struct Gold {
  Cube from{}, to{};
  int weapon = 0;  // -1: a flight's (rockets and grenades both)
  int count = 0, airborne = 0, demos = 0;
  sh::Vec3f from_at, to_at;  // the means
  std::vector<float> distances;
  int last_demo = -1;
};

sh::Vec3f lerp(sh::Vec3f a, sh::Vec3f b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
std::string xyz(sh::Vec3f p) {
  return to_fixed(p.x, 0) + " " + to_fixed(p.y, 0) + " " + to_fixed(p.z, 0);
}

// Pairs of cubes counted; those seen `gold` times or more, most first (ties by the cubes).
std::vector<Gold> gold_shots(
    const std::vector<std::tuple<sh::Vec3f, sh::Vec3f, int, int, bool>>& shots, double cell,
    int gold) {
  std::map<std::tuple<Cube, Cube, int>, Gold> by;
  for (const auto& [a, b, weapon, demo, air] : shots) {
    Gold& g = by[{cube_of(a, cell), cube_of(b, cell), weapon}];
    g.from = cube_of(a, cell);
    g.to = cube_of(b, cell);
    g.weapon = weapon;
    const float n = static_cast<float>(g.count);
    g.from_at = lerp(a, g.from_at, n / (n + 1));
    g.to_at = lerp(b, g.to_at, n / (n + 1));
    g.count++;
    g.airborne += air ? 1 : 0;
    if (demo != g.last_demo) g.demos++, g.last_demo = demo;
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    g.distances.push_back(
        static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz))));
  }
  std::vector<Gold> out;
  for (auto& [key, g] : by)
    if (g.count >= gold) out.push_back(std::move(g));
  std::stable_sort(out.begin(), out.end(),
                   [](const Gold& x, const Gold& y) { return x.count > y.count; });
  return out;
}

float median(std::vector<float> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

// An arrow from a to b on the picture: the line and a head at b.
void arrow(gl::Picture& p, const la::WireView& v, sh::Vec3f a, sh::Vec3f b, gl::Rgba c,
           double thickness, double head) {
  double ax, ay, af, bx, by, bf;
  v.pixel(a, ax, ay, af);
  v.pixel(b, bx, by, bf);
  gl::draw_line(p, ax, ay, bx, by, c, thickness);
  const double dx = bx - ax, dy = by - ay, len = ge::core::hypot(dx, dy);
  if (len < 1 || head <= 0) return;
  const double ux = dx / len, uy = dy / len;
  gl::draw_line(p, bx, by, bx - head * (ux - uy * 0.5), by - head * (uy + ux * 0.5), c, thickness);
  gl::draw_line(p, bx, by, bx - head * (ux + uy * 0.5), by - head * (uy - ux * 0.5), c, thickness);
}

void weapon_legend(gl::Picture& p, int x, int y, const std::array<int, kWeapons>& counts) {
  for (int w = 0; w < kWeapons; w++) {
    if (counts[static_cast<size_t>(w)] == 0) continue;
    gl::fill_rect(p, x, y + 1, 10, 6, kWeaponColour[static_cast<size_t>(w)]);
    const std::string s = std::string(kWeaponName[static_cast<size_t>(w)]) + " " +
                          std::to_string(counts[static_cast<size_t>(w)]);
    gl::draw_text(p, x + 14, y, s, {40, 40, 40, 255});
    x += 14 + gl::text_width(s) + 14;
  }
}

// LINES ADDED UP: every line sampled a pixel at a time into a count and a colour sum, then laid
// over the picture with the colour the lines there average and an opacity by the count's log
// against the 99th percentile's, so ten lines and ten thousand both read as a density.
struct Density {
  int w = 0, h = 0;
  std::vector<float> n, r, g, b;
  Density(int width, int height)
      : w(width),
        h(height),
        n(static_cast<size_t>(width * height)),
        r(n.size()),
        g(n.size()),
        b(n.size()) {}
  void dot(double x, double y, gl::Rgba c, float weight) {
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    if (ix < 0 || iy < 0 || ix >= w || iy >= h) return;
    const size_t i = static_cast<size_t>(iy) * static_cast<size_t>(w) + static_cast<size_t>(ix);
    n[i] += weight;
    r[i] += weight * c.r;
    g[i] += weight * c.g;
    b[i] += weight * c.b;
  }
  void line(double ax, double ay, double bx, double by, gl::Rgba c, float weight = 1) {
    const int steps = std::max(1, static_cast<int>(ge::core::hypot(bx - ax, by - ay)));
    for (int k = 0; k <= steps; k++) {
      const double t = static_cast<double>(k) / steps;
      dot(ax + (bx - ax) * t, ay + (by - ay) * t, c, weight);
    }
  }
  void disc(double x, double y, double radius, gl::Rgba c, float weight) {
    const int rr = static_cast<int>(radius);
    for (int dy = -rr; dy <= rr; dy++)
      for (int dx = -rr; dx <= rr; dx++)
        if (dx * dx + dy * dy <= radius * radius) dot(x + dx, y + dy, c, weight);
  }
  void over(gl::Picture& p) const {
    std::vector<float> seen;
    for (float v : n)
      if (v > 0) seen.push_back(v);
    if (seen.empty()) return;
    std::sort(seen.begin(), seen.end());
    const double top = ge::core::libm::log(1.0 + seen[seen.size() * 99 / 100]);
    for (int y = 0; y < h && y < p.height; y++)
      for (int x = 0; x < w && x < p.width; x++) {
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        if (n[i] <= 0) continue;
        auto sq = [](double v) { return v * v; };
        const double a = std::min(1.0, 0.06 + 0.94 * sq(ge::core::libm::log(1.0 + n[i]) / top));
        gl::set_pixel(p, x, y,
                      {static_cast<uint8_t>(r[i] / n[i]), static_cast<uint8_t>(g[i] / n[i]),
                       static_cast<uint8_t>(b[i] / n[i]), static_cast<uint8_t>(255 * a)});
      }
  }
};

// The fixture's matches: the highlight match moved a little each time (a seeded offset for the
// whole match and a smaller one a player), so its shots gather into gold shots over a few demos.
std::vector<std::vector<uint8_t>> written_matches(int count) {
  std::vector<std::vector<uint8_t>> out;
  ge::core::Rng rng = ge::core::make_rng(43);
  for (int m = 0; m < count; m++) {
    cl::MatchScript s = cl::highlight_match();
    const auto off = [&](double r) {
      return sh::Vec3f{static_cast<float>(ge::core::range(rng, -r, r)),
                       static_cast<float>(ge::core::range(rng, -r, r)), 0};
    };
    const sh::Vec3f all = off(20);
    for (cl::MatchPlayer& p : s.players) {
      const sh::Vec3f mine = off(10);
      for (sh::Vec3f& o : p.origins) o = {o.x + all.x + mine.x, o.y + all.y + mine.y, o.z};
    }
    for (cl::MatchRocket& r : s.rockets) {
      r.start = {r.start.x + all.x, r.start.y + all.y, r.start.z};
      r.end = {r.end.x + all.x, r.end.y + all.y, r.end.z};
    }
    out.push_back(cl::write_multiview_match(s));
  }
  return out;
}

int weapon_of_flag(const std::string& f) {
  if (f.empty() || f == "all") return -1;
  for (int w = 0; w < kWeapons; w++)
    if (f == kWeaponFlag[static_cast<size_t>(w)] || f == kWeaponName[static_cast<size_t>(w)])
      return w;
  return -2;
}

void run(gl::Lab& lab) {
  std::string error;
  la::StudyMap m;
  if (!la::load_study_map(lab.arg("map"), lab.arg("basedir"), lab.arg("game"), lab.arg("mapname"),
                          m, error)) {
    lab.fail("se-kills: " + error, "next: make lab LAB=se-kills ARGS=--help; skill: shooter");
    return;
  }
  const int only = weapon_of_flag(lab.arg("weapon"));
  if (only == -2) {
    lab.fail("se-kills: no weapon " + lab.arg("weapon"),
             "next: pass --weapon= all, melee, sg, ssg, ng, sng, gl, rl or lg");
    return;
  }
  const double cell = std::max(16.0, lab.arg_num("cell"));
  const int gold_given = static_cast<int>(lab.arg_num("gold"));
  const int most = static_cast<int>(lab.arg_num("most"));

  // The demos: --file, every demo file in --dir (by name), or the matches written in code.
  std::vector<std::string> paths;
  const std::string dir = lab.arg("dir"), file = lab.arg("file");
  if (!file.empty()) paths.push_back(file);
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file()) paths.push_back(e.path().string());
    if (ec) {
      lab.fail("se-kills: the folder " + dir + " does not list: " + ec.message(),
               "next: pass --dir=<a folder of demos>");
      return;
    }
    std::sort(paths.begin() + (file.empty() ? 0 : 1), paths.end());
  }
  if (most > 0 && static_cast<int>(paths.size()) > most) paths.resize(static_cast<size_t>(most));
  const bool written = paths.empty();
  std::vector<std::vector<uint8_t>> made;
  if (written) made = written_matches(static_cast<int>(lab.arg_num("written")));
  const size_t demo_count = written ? made.size() : paths.size();

  std::vector<Kill> kills;
  std::vector<Flight> flights;
  int read = 0, other_map = 0, broken = 0;
  int64_t deaths = 0, unmatched = 0;
  double seconds = 0;
  // Each demo scanned on its own (side by side: a scan reads one demo and writes one slot), then
  // gathered in the demos' order, so the result is the same for any number of threads.
  struct Scanned {
    bool ok = false;
    std::string error;
    cl::DemoScan scan;
  };
  std::vector<Scanned> scans(demo_count);
  ge::core::parallel_bands(demo_count, demo_count, [&](size_t b, size_t e) {
    for (size_t i = b; i < e; i++) {
      cl::Demo d;
      Scanned& o = scans[i];
      o.ok = cl::open_demo(written ? made[i] : la::read_whole(paths[i]), d, o.error) &&
             cl::scan_demo(d, o.scan, o.error);
    }
  });
  for (size_t i = 0; i < demo_count; i++) {
    if (!scans[i].ok) {
      if (broken++ < 3)
        lab.note("skipped " + (written ? "match " + std::to_string(i) : paths[i]) + ": " +
                 scans[i].error);
      continue;
    }
    const cl::DemoScan& scan = scans[i].scan;
    if (scan.map != m.name) {
      other_map++;
      continue;
    }
    const int at = read++;
    seconds += scan.length_s;
    deaths += scan.deaths;
    unmatched += scan.unmatched;
    for (const cl::DemoKill& k : scan.kills)
      if (k.killer >= 0 && k.killer != k.victim) kills.push_back({at, k});
    for (const cl::DemoFlight& f : scan.flights) flights.push_back({at, f});
  }
  if (other_map > 0)
    lab.note(std::to_string(other_map) + " demos were of another map than " + m.name + ", skipped");
  if (read == 0) {
    lab.fail("se-kills: no demo of " + m.name + " read",
             "next: pass --dir= a folder of " + m.name + "'s demos, and --map= its .bsp");
    return;
  }

  // A gold shot is one in 2000 of the kills (or flights) shown, and seen three times at least.
  const auto gold_of = [&](size_t items) {
    return gold_given > 0 ? gold_given : std::max(3, static_cast<int>(items / 2000));
  };
  const In in = {{"demos", written ? std::to_string(demo_count) + " written in code" : dir + file},
                 {"map", m.name + " (" + m.from + ")"},
                 {"weapon", only < 0 ? "all" : kWeaponName[static_cast<size_t>(only)]},
                 {"cell", lab.arg("cell")},
                 {"gold", gold_given > 0 ? lab.arg("gold") : "auto"}};

  // Each weapon's kills.
  std::array<int, kWeapons> counts{}, shown{};
  std::array<std::vector<float>, kWeapons> dist;
  std::array<int, kWeapons> air{};
  for (const Kill& k : kills) {
    const auto w = static_cast<size_t>(k.k.weapon);
    counts[w]++;
    dist[w].push_back(k.k.distance);
    air[w] += k.k.airborne ? 1 : 0;
  }
  gl::LabTable wt{"each weapon's kills",
                  {"weapon", "kills", "share %", "distance p25", "p50", "p75", "victim in air %"},
                  {}};
  for (int w = 0; w < kWeapons; w++) {
    std::vector<float> v = dist[static_cast<size_t>(w)];
    if (v.empty()) continue;
    std::sort(v.begin(), v.end());
    wt.rows.push_back(
        {kWeaponName[static_cast<size_t>(w)], std::to_string(v.size()),
         to_fixed(100.0 * static_cast<double>(v.size()) / static_cast<double>(kills.size()), 1),
         to_fixed(v[v.size() / 4], 0), to_fixed(v[v.size() / 2], 0),
         to_fixed(v[v.size() * 3 / 4], 0),
         to_fixed(100.0 * air[static_cast<size_t>(w)] / static_cast<double>(v.size()), 1)});
  }
  lab.table(wt);

  // The kills and flights the filter keeps.
  std::vector<const Kill*> keep;
  for (const Kill& k : kills)
    if (only < 0 || static_cast<int>(k.k.weapon) == only) {
      keep.push_back(&k);
      shown[static_cast<size_t>(k.k.weapon)]++;
    }
  const bool rockets_too = only < 0 || only == static_cast<int>(hud::Weapon::rocket_launcher) ||
                           only == static_cast<int>(hud::Weapon::grenade_launcher);
  std::vector<const Flight*> flown;
  int64_t rockets = 0, grenades = 0, owned = 0;
  for (const Flight& f : flights) {
    if (!rockets_too) break;
    if (only == static_cast<int>(hud::Weapon::rocket_launcher) && f.f.grenade) continue;
    if (only == static_cast<int>(hud::Weapon::grenade_launcher) && !f.f.grenade) continue;
    flown.push_back(&f);
    (f.f.grenade ? grenades : rockets)++;
    owned += f.f.shooter >= 0 ? 1 : 0;
  }

  // The gold shots: kills, and flights.
  std::vector<std::tuple<sh::Vec3f, sh::Vec3f, int, int, bool>> shots, throws;
  for (const Kill* k : keep)
    shots.push_back(
        {k->k.killer_at, k->k.victim_at, static_cast<int>(k->k.weapon), k->demo, k->k.airborne});
  // A rocket gone within kFeet of where it showed hit the shooter's feet or a wall at his face:
  // a rocket jump or a point-blank shot, kept apart from the lanes.
  constexpr float kFeet = 64;
  std::vector<std::tuple<sh::Vec3f, sh::Vec3f, int, int, bool>> feet;
  for (const Flight* f : flown) {
    const float fx = f->f.to.x - f->f.from.x, fy = f->f.to.y - f->f.from.y,
                fz = f->f.to.z - f->f.from.z;
    const bool near = !f->f.grenade && fx * fx + fy * fy + fz * fz < kFeet * kFeet;
    (near ? feet : throws).push_back({f->f.from, f->f.from, f->f.grenade ? 5 : 6, f->demo, false});
    if (!near) std::get<1>(throws.back()) = f->f.to;
  }
  const int gold = gold_of(shots.size()), gold_lane = gold_of(throws.size());
  const std::vector<Gold> golds = gold_shots(shots, cell, gold);
  const std::vector<Gold> lanes = gold_shots(throws, cell, gold_lane);
  const std::vector<Gold> jumps = gold_shots(feet, cell, gold_of(feet.size()));
  int gold_kills = 0;
  for (const Gold& g : golds) gold_kills += g.count;

  lab.metric("demos.read", read, "count");
  lab.metric("demos.other-map", other_map, "count");
  lab.metric("demos.broken", broken, "count");
  lab.metric("demos.minutes", seconds / 60, "min");
  lab.metric("deaths", static_cast<double>(deaths), "count");
  lab.metric("deaths.unmatched", static_cast<double>(unmatched), "count");
  lab.metric("kills", static_cast<double>(kills.size()), "count");
  lab.metric("kills.shown", static_cast<double>(keep.size()), "count");
  lab.metric("flights.rockets", static_cast<double>(rockets), "count");
  lab.metric("flights.grenades", static_cast<double>(grenades), "count");
  lab.metric("flights.with-shooter", static_cast<double>(owned), "count");
  lab.metric("gold.shots", static_cast<double>(golds.size()), "count");
  lab.metric("gold.kills-share",
             keep.empty() ? 0 : static_cast<double>(gold_kills) / static_cast<double>(keep.size()),
             "share");
  lab.metric("gold.lanes", static_cast<double>(lanes.size()), "count");
  if (!golds.empty()) lab.metric("gold.top-count", golds[0].count, "count");

  gl::LabTable gt{"the gold shots: kills from the same spot onto the same spot, most first",
                  {"#", "weapon", "kills", "demos", "killer at (x y z)", "victim at (x y z)",
                   "distance p50", "in air %"},
                  {}};
  const size_t rows = std::min<size_t>(golds.size(), static_cast<size_t>(lab.arg_num("rows")));
  for (size_t i = 0; i < rows; i++) {
    const Gold& g = golds[i];
    gt.rows.push_back({std::to_string(i + 1), kWeaponName[static_cast<size_t>(g.weapon)],
                       std::to_string(g.count), std::to_string(g.demos), xyz(g.from_at),
                       xyz(g.to_at), to_fixed(median(g.distances), 0),
                       to_fixed(100.0 * g.airborne / g.count, 0)});
  }
  lab.table(gt);
  gl::LabTable lt{
      "the rocket and grenade lanes: flights from the same spot to the same spot, hit or "
      "miss",
      {"#", "kind", "flights", "demos", "from (x y z)", "to (x y z)", "flight p50"},
      {}};
  for (size_t i = 0; i < std::min<size_t>(lanes.size(), rows); i++) {
    const Gold& g = lanes[i];
    lt.rows.push_back({std::to_string(i + 1), g.weapon == 5 ? "grenade" : "rocket",
                       std::to_string(g.count), std::to_string(g.demos), xyz(g.from_at),
                       xyz(g.to_at), to_fixed(median(g.distances), 0)});
  }
  lab.table(lt);
  lab.metric("flights.at-the-feet", static_cast<double>(feet.size()), "count");
  gl::LabTable ft{"where rockets go off at the feet (rocket jumps, point-blank), most first",
                  {"#", "rockets", "demos", "at (x y z)"},
                  {}};
  for (size_t i = 0; i < std::min<size_t>(jumps.size(), 10); i++)
    ft.rows.push_back({std::to_string(i + 1), std::to_string(jumps[i].count),
                       std::to_string(jumps[i].demos), xyz(jumps[i].from_at)});
  lab.table(ft);

  // The files a bot's tables are made from.
  {
    std::string t =
        "demo\tt\tweapon\tkiller_x\tkiller_y\tkiller_z\tvictim_x\tvictim_y\tvictim_z\t"
        "distance\tairborne\tshot_from_x\tshot_from_y\tshot_from_z\tshot_to_x\tshot_to_y\t"
        "shot_to_z\n";
    for (const Kill& k : kills) {
      const cl::DemoKill& d = k.k;
      t += std::to_string(k.demo) + "\t" + to_fixed(d.t, 2) + "\t" +
           kWeaponFlag[static_cast<size_t>(d.weapon)] + "\t" + to_fixed(d.killer_at.x, 0) + "\t" +
           to_fixed(d.killer_at.y, 0) + "\t" + to_fixed(d.killer_at.z, 0) + "\t" +
           to_fixed(d.victim_at.x, 0) + "\t" + to_fixed(d.victim_at.y, 0) + "\t" +
           to_fixed(d.victim_at.z, 0) + "\t" + to_fixed(d.distance, 0) + "\t" +
           (d.airborne ? "1" : "0");
      if (d.shot)
        t += "\t" + to_fixed(d.shot_from.x, 0) + "\t" + to_fixed(d.shot_from.y, 0) + "\t" +
             to_fixed(d.shot_from.z, 0) + "\t" + to_fixed(d.shot_to.x, 0) + "\t" +
             to_fixed(d.shot_to.y, 0) + "\t" + to_fixed(d.shot_to.z, 0);
      else
        t += "\t\t\t\t\t\t";
      t += "\n";
    }
    lab.attach("kills.tsv", std::vector<uint8_t>(t.begin(), t.end()));
    std::string g =
        "kind\tweapon\tcount\tdemos\tfrom_x\tfrom_y\tfrom_z\tto_x\tto_y\tto_z\tdistance_p50\n";
    for (const auto* list : {&golds, &lanes})
      for (const Gold& s : *list)
        g += std::string(list == &golds ? "kill" : "flight") + "\t" +
             kWeaponFlag[static_cast<size_t>(s.weapon)] + "\t" + std::to_string(s.count) + "\t" +
             std::to_string(s.demos) + "\t" + to_fixed(s.from_at.x, 0) + "\t" +
             to_fixed(s.from_at.y, 0) + "\t" + to_fixed(s.from_at.z, 0) + "\t" +
             to_fixed(s.to_at.x, 0) + "\t" + to_fixed(s.to_at.y, 0) + "\t" +
             to_fixed(s.to_at.z, 0) + "\t" + to_fixed(median(s.distances), 0) + "\n";
    lab.attach("gold.tsv", std::vector<uint8_t>(g.begin(), g.end()));
  }

  const std::vector<la::WireEdge> edges = la::wire_edges(m.map);
  const std::vector<sh::Vec3f> map_pts = la::edge_points(edges);
  const gl::Rgba paper{250, 250, 246, 255};
  std::string what = "KILL";
  if (only >= 0) {
    what = kWeaponName[static_cast<size_t>(only)];
    for (char& ch : what) ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
    what += " KILL";
  }
  const std::string over = std::to_string(read) + " demo" + (read == 1 ? "" : "s");

  for (const auto& [id, elevation] : {std::pair<std::string, double>{"kills-top", 90.0},
                                      std::pair<std::string, double>{"kills-angle", 40.0}}) {
    if (!lab.wants(id)) continue;
    la::WireView v;
    v.width = 1400;
    v.height = 1050;
    v.elevation = elevation;
    v.yaw = elevation == 90 ? 0 : lab.arg_num("yaw");
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v, paper);
    Density dn(v.width, v.height);
    for (const Kill* k : keep) {
      const gl::Rgba c = kWeaponColour[static_cast<size_t>(k->k.weapon)];
      double ax, ay, af, bx, by, bf;
      v.pixel(k->k.killer_at, ax, ay, af);
      v.pixel(k->k.victim_at, bx, by, bf);
      dn.line(ax, ay, bx, by, c);
      dn.disc(bx, by, 2.5, c, 2);
    }
    dn.over(p);
    weapon_legend(p, 10, 6, shown);
    std::string title = "EVERY " + what;
    title += " ON " + m.name;
    title += elevation == 90 ? " FROM ABOVE, " : " AT AN ANGLE, ";
    title += over;
    lab.shot({id, title,
              "The map as a wireframe, and every kill in the recordings as a line from where the "
              "killer stood to where the victim stood the frame before he died, a dot at the "
              "victim, coloured by the killer's weapon in hand. The lines are added up: a pixel "
              "crossed by many is solid, by few pale, its colour the weapons' mix there.",
              "the lines gather on the few spots players hold and the ways between them; long "
              "lines are rockets and the beam, short ones the shotguns and melee",
              in},
             p);
  }

  const size_t top = std::min<size_t>(golds.size(), static_cast<size_t>(lab.arg_num("rows")));
  auto draw_golds = [&](gl::Picture& p, const la::WireView& v, const std::vector<Gold>& list,
                        size_t n, bool numbers) {
    const int peak = list.empty() ? 1 : list[0].count;
    for (size_t i = n; i-- > 0;) {
      const Gold& g = list[i];
      const double t = 1.2 + 4.0 * g.count / peak;
      const gl::Rgba c = g.weapon >= 0 && g.weapon < kWeapons
                             ? kWeaponColour[static_cast<size_t>(g.weapon)]
                             : gl::Rgba{215, 40, 30, 255};
      arrow(p, v, g.from_at, g.to_at, with_alpha(c, 200), t, 8 + t);
      double x, y, f;
      v.pixel(g.from_at, x, y, f);
      gl::fill_disc(p, x, y, 2 + t * 0.6, {30, 30, 30, 220});
      if (numbers) {
        double tx, ty, tf;
        v.pixel(lerp(g.from_at, g.to_at, 0.5f), tx, ty, tf);
        const std::string s = std::to_string(i + 1);
        gl::fill_rect(p, tx - 1, ty - 5, gl::text_width(s) + 2, 10, {255, 255, 255, 220});
        gl::draw_text(p, tx, ty - 4, s, {20, 20, 20, 255});
      }
    }
  };
  for (const auto& [id, elevation] : {std::pair<std::string, double>{"gold-top", 90.0},
                                      std::pair<std::string, double>{"gold-angle", 40.0}}) {
    if (!lab.wants(id)) continue;
    la::WireView v;
    v.width = 1400;
    v.height = 1050;
    v.elevation = elevation;
    v.yaw = elevation == 90 ? 0 : lab.arg_num("yaw");
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v, paper);
    for (const Kill* k : keep) {  // the noise, faint
      double ax, ay, af, bx, by, bf;
      v.pixel(k->k.killer_at, ax, ay, af);
      v.pixel(k->k.victim_at, bx, by, bf);
      gl::draw_line(p, ax, ay, bx, by, {160, 160, 160, 18});
    }
    draw_golds(p, v, golds, top, true);
    weapon_legend(p, 10, 6, shown);
    lab.shot({id,
              "THE GOLD SHOTS ON " + m.name + (elevation == 90 ? " FROM ABOVE" : " AT AN ANGLE") +
                  ", " + over,
              "Only the kills made again and again: a killer in the same " + lab.arg("cell") +
                  "-unit cube killing a victim in the same cube at least " + std::to_string(gold) +
                  " times. Each is an arrow from the killers' mean spot to the victims', thicker "
                  "the more often, coloured by weapon and numbered as in the gold shots' table "
                  "(its xyz). The rest of the kills are the grey haze.",
              "a handful of arrows hold a large share of the kills: the spots worth a bot's "
              "pre-aim and the lanes worth a rocket fired blind",
              in},
             p);
  }

  // One small map a weapon, its own gold shots.
  if (lab.wants("by-weapon") && only < 0) {
    const int cols = 4, w = 600, h = 460;
    gl::Picture sheet = gl::create_picture(cols * w, 2 * h, paper);
    la::WireView v;
    v.width = w;
    v.height = h;
    v.margin = 14;
    v.fit(map_pts);
    const gl::Picture base = la::wire_picture(edges, v, paper);
    for (int wpn = 0; wpn < kWeapons; wpn++) {
      gl::Picture p = base;
      // Its own bar: one in 2000 of its own kills, so a rare weapon's habits show too.
      std::vector<std::tuple<sh::Vec3f, sh::Vec3f, int, int, bool>> own;
      for (const Kill& k : kills)
        if (static_cast<int>(k.k.weapon) == wpn)
          own.push_back({k.k.killer_at, k.k.victim_at, wpn, k.demo, k.k.airborne});
      const std::vector<Gold> mine = gold_shots(own, cell, gold_of(own.size()));
      for (const Kill& k : kills) {
        if (static_cast<int>(k.k.weapon) != wpn) continue;
        double ax, ay, af, bx, by, bf;
        v.pixel(k.k.killer_at, ax, ay, af);
        v.pixel(k.k.victim_at, bx, by, bf);
        gl::draw_line(p, ax, ay, bx, by, with_alpha(kWeaponColour[static_cast<size_t>(wpn)], 30));
      }
      draw_golds(p, v, mine, std::min<size_t>(mine.size(), 12), false);
      const std::string label = std::string(kWeaponName[static_cast<size_t>(wpn)]) + ": " +
                                std::to_string(counts[static_cast<size_t>(wpn)]) + " kills, " +
                                std::to_string(mine.size()) + " gold";
      gl::draw_text(p, 8, 4, label, kWeaponColour[static_cast<size_t>(wpn)], 2);
      gl::blit(sheet, p, (wpn % cols) * w, (wpn / cols) * h);
    }
    lab.shot({"by-weapon", "EACH WEAPON'S KILLS AND GOLD SHOTS ON " + m.name,
              "One map a weapon, from above: its kills as faint lines and its twelve most frequent "
              "gold shots (by its own bar: one in 2000 of its kills, 3 at least) as arrows "
              "(thicker the more often), so which shots each weapon is good "
              "for reads side by side.",
              "rockets and the beam own the long lines across rooms; the shotguns the doorways "
              "and corners",
              in},
             sheet);
  }

  // Heat over the plan: where players die, and where rockets and grenades land.
  auto heat = [&](const std::string& id, const std::string& title, const std::vector<sh::Vec3f>& at,
                  gl::Rgba hot, const std::string& caption, const std::string& look) {
    if (!lab.wants(id)) return;
    la::WireView v;
    v.width = 1400;
    v.height = 1050;
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v, paper);
    const double bin = lab.arg_num("heat");
    std::map<std::pair<int32_t, int32_t>, int> n;
    for (const sh::Vec3f& q : at)
      n[{static_cast<int32_t>(std::floor(q.x / bin)),
         static_cast<int32_t>(std::floor(q.y / bin))}]++;
    int peak = 1;
    for (const auto& [k, c] : n) peak = std::max(peak, c);
    for (const auto& [k, c] : n) {
      double x0, y0, f0, x1, y1, f1;
      v.pixel({static_cast<float>(k.first * bin), static_cast<float>(k.second * bin), 0}, x0, y0,
              f0);
      v.pixel(
          {static_cast<float>((k.first + 1) * bin), static_cast<float>((k.second + 1) * bin), 0},
          x1, y1, f1);
      const double share = ge::core::libm::pow(static_cast<double>(c) / peak, 0.6);
      gl::fill_rect(p, std::min(x0, x1), std::min(y0, y1), std::fabs(x1 - x0) + 1,
                    std::fabs(y1 - y0) + 1, with_alpha(hot, static_cast<int>(30 + 200 * share)));
    }
    gl::draw_text(p, 10, 6,
                  std::to_string(at.size()) + " in " + to_fixed(bin, 0) +
                      "-unit squares; the darkest " + std::to_string(peak),
                  {40, 40, 40, 255});
    lab.shot({id, title + " ON " + m.name + ", " + over, caption, look, in}, p);
  };
  {
    std::vector<sh::Vec3f> at;
    at.reserve(keep.size());
    for (const Kill* k : keep) at.push_back(k->k.victim_at);
    heat("deaths-heat", "WHERE PLAYERS DIE", at, {200, 30, 40, 255},
         "Every victim's spot (the frame before he died) of the kills shown, counted in squares "
         "of the plan and drawn darker the more (the share to the power 0.6, so the rare spots "
         "still show). Floors over one another share a square.",
         "a few hot squares: the item spots and the doors into them");
  }
  if (rockets_too) {
    std::vector<sh::Vec3f> at;
    at.reserve(flown.size());
    for (const Flight* f : flown) at.push_back(f->f.to);
    heat("impacts-heat", "WHERE ROCKETS AND GRENADES LAND", at, {230, 110, 20, 255},
         "Where every rocket and grenade flown was last seen, hit or miss, counted in squares of "
         "the plan. The hot squares are where a rocket fired without a target is most likely to "
         "find one.",
         "the impacts pile up on the walls and floors round the item spots, not on open floor");
  }
  if (rockets_too && lab.wants("flights")) {
    la::WireView v;
    v.width = 1400;
    v.height = 1050;
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v, paper);
    Density dn(v.width, v.height);
    for (const Flight* f : flown) {
      double ax, ay, af, bx, by, bf;
      v.pixel(f->f.from, ax, ay, af);
      v.pixel(f->f.to, bx, by, bf);
      const gl::Rgba c = f->f.grenade ? gl::Rgba{40, 160, 50, 255} : gl::Rgba{215, 40, 30, 255};
      dn.line(ax, ay, bx, by, c);
      dn.disc(bx, by, 1.5, c, 2);
    }
    dn.over(p);
    draw_golds(p, v, lanes, std::min<size_t>(lanes.size(), top), true);
    gl::draw_text(p, 10, 6,
                  std::to_string(rockets) + " rockets (red), " + std::to_string(grenades) +
                      " grenades (green); the arrows are the lanes flown most",
                  {40, 40, 40, 255});
    lab.shot({"flights", "EVERY ROCKET AND GRENADE ON " + m.name + ", " + over,
              "Every rocket and grenade from where it was first seen to where it went, hit or "
              "miss, added up (solid where many cross), a dot where it ended; the arrows are the "
              "lanes flown from the "
              "same cube to the same cube most often, numbered as in the lanes' table.",
              "the lanes are the shots fired at a spot before anyone is seen there: the spam a "
              "bot can learn",
              in},
             p);
  }

  // The top gold shot from inside the map, from behind the killers' spot.
  // The first gold shot of 256 units or more (a short one is a fight in one spot).
  size_t pick = 0;
  while (pick < golds.size() && median(golds[pick].distances) < 256) pick++;
  if (pick < golds.size() && lab.wants("inside")) {
    const Gold& g = golds[pick];
    la::InsideCamera c;
    const double dx = g.to_at.x - g.from_at.x, dy = g.to_at.y - g.from_at.y;
    const double len = std::max(1.0, ge::core::hypot(dx, dy));
    c.eye = {static_cast<float>(g.from_at.x - dx / len * 170),
             static_cast<float>(g.from_at.y - dy / len * 170), g.from_at.z + 80};
    const sh::HullTrace tr = sh::trace_hull(m.world->solids[0].point_hull,
                                            {g.from_at.x, g.from_at.y, g.from_at.z + 22}, c.eye);
    if (tr.fraction < 1)
      c.eye = lerp({g.from_at.x, g.from_at.y, g.from_at.z + 22}, c.eye,
                   std::max(0.0f, tr.fraction - 0.08f));
    // Looking from the eye at the victims' spot.
    const double ex = g.to_at.x - c.eye.x, ey = g.to_at.y - c.eye.y;
    c.yaw = ge::core::libm::atan2(ey, ex) * 180 / ge::core::kPi;
    c.pitch =
        ge::core::libm::atan2(g.to_at.z - c.eye.z, ge::core::hypot(ex, ey)) * 180 / ge::core::kPi;
    c.width = 1200;
    c.height = 720;
    la::InsideView iv(c);
    iv.faces(m.map);
    iv.edges(edges);
    for (size_t i = std::min<size_t>(golds.size(), top); i-- > 0;) {
      const Gold& s = golds[i];
      const gl::Rgba col = kWeaponColour[static_cast<size_t>(s.weapon)];
      iv.line(s.from_at, s.to_at, col, i == pick ? 4 : 2, false, 0);
    }
    for (const Kill* k : keep) {
      if (cube_of(k->k.killer_at, cell) != g.from || cube_of(k->k.victim_at, cell) != g.to)
        continue;
      double x, y;
      if (iv.pixel(k->k.victim_at, x, y)) gl::fill_disc(iv.picture(), x, y, 3, {200, 30, 40, 200});
      if (iv.pixel(k->k.killer_at, x, y)) gl::fill_disc(iv.picture(), x, y, 3, {20, 20, 20, 200});
    }
    const std::string n = std::to_string(pick + 1);
    lab.shot({"inside", "GOLD SHOT " + n + " SEEN FROM BEHIND THE KILLER, " + m.name,
              "A camera in the map just behind the killers' mean spot of gold shot " + n +
                  " (the first of 256 units or more), looking at the victims': the map's faces "
                  "filled pale, the outlines where no wall is nearer, the gold shots as lines in "
                  "the world (shot " +
                  n +
                  " thick), a black dot at each of its killers and a red "
                  "one at each of its victims.",
              "the line runs through an open sight line onto a spot players cross", in},
             iv.picture());
  }

  if (lab.wants("distances")) {
    std::vector<gl::Sample> s;
    for (int w = 0; w < kWeapons; w++) {
      if (dist[static_cast<size_t>(w)].empty()) continue;
      gl::Sample q{kWeaponName[static_cast<size_t>(w)], {}};
      for (float d : dist[static_cast<size_t>(w)]) q.values.push_back(std::min(d, 2000.0f));
      s.push_back(q);
    }
    const gl::Chart ch =
        gl::histogram_chart({"HOW FAR EACH WEAPON KILLS", over + " on " + m.name,
                             "killer to victim (units, 2000+ in the last bin)", "kills", 960, 420},
                            s, 40);
    lab.shot({"distances", ch.title,
              "The distance from killer to victim of every kill, by the killer's weapon.",
              "the shotguns and melee close, rockets in the middle, the beam far", in},
             gl::to_picture(ch.plot));
  }
}

const gl::LabSpec kSpec{
    "se-kills",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client/demo_scan.hpp (kills, flights) over many demos of one map "
    "(games/shooters_edge/labs/se_kills_lab.cpp, wire_view.hpp)",
    "shooter",
    "Where on a map do the kills happen, compiled over many recorded games: which spots are shot "
    "from and onto again and again with each weapon, and where do rockets go, hit or miss?",
    {
        {"dir", "",
         "a folder of demos (any format, plain or gzip) of one map; other files skipped"},
        {"file", "", "one demo's path (with --dir: read too)"},
        {"written", "4", "with neither: how many scripted matches to write in code"},
        {"most", "0", "read at most this many demos of --dir (0: all)"},
        {"map", "", "the map's .bsp path; empty: --basedir, else the fixture halls"},
        {"basedir", "", "a folder holding a game folder, to read --mapname from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "", "with --basedir: the map's name"},
        {"weapon", "all", "keep one weapon: melee, sg, ssg, ng, sng, gl, rl, lg (or all)"},
        {"cell", "128", "the cube a shot's ends are put in, units"},
        {"gold", "0",
         "a shot seen this often or more is a gold shot (0: one in 2000 of those shown, 3 at "
         "least)"},
        {"rows", "20", "the gold shots tabled and numbered"},
        {"heat", "48", "the heat maps' square, units"},
        {"yaw", "30", "the angled views' turn about z, degrees"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
