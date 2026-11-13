// THE WAY TO AN ITEM (M18r.md 1a, "how people come to an item"): off a folder of one map's
// recordings, every time a key item is taken (the armours, the mega health, the quad, the
// pentagram, the rocket launcher) the taker's path over the `--before` seconds before it, drawn on
// the map from above and grouped into the few ways people come in. A second folder (`--versus`,
// the bots' `se-team --save` recordings) is drawn beside it under the same rules, so a way people
// take that the bots never do shows at a glance.
//
// THE TAKER is the live player nearest the item's spot at the frame it went (within a player's
// reach of it); a pickup with nobody there (a recording's gap) is left out. THE WAY IN is where
// the path was `--from` seconds before the pickup: those points grouped by a leader pass (each
// point joins the first group within `--group` units of its first point, in time order), the
// groups numbered by size. Each group's row: its share, the time from there, the speed over the
// last two seconds, the share of the path in the air, the height climbed.
//
// THE PICTURE per item: the floor from above (to scale, cropped round the paths, north up), each
// path a line from blue (the start) to orange (the pickup), the item a red ring, each way in a
// numbered disc with its share. `--paths=<file>` writes every path's points as a TSV for reading
// elsewhere (a bot's routes off it, say).
//
// ge:summary Lab: every key-item pickup's last seconds off recordings, drawn and grouped
// ge:keywords lab se-approach approach item-routes pickup taker path ways-in yellow-armour
// ge:keywords red-armour mega quad launcher people-routes recordings map-top item-control
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/nav_top.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr int kKinds = 7;
const char* const kNames[kKinds] = {"ga", "ya", "ra", "mh", "quad", "pent", "rl"};
const char* const kLong[kKinds] = {"GREEN ARMOUR", "YELLOW ARMOUR", "RED ARMOUR", "MEGA HEALTH",
                                   "QUAD",         "PENTAGRAM",     "ROCKET LAUNCHER"};
constexpr float kReachXY = 64;      // the taker this near the spot across, units
constexpr float kReachZ = 96;       // and this near in height
constexpr double kStep_s = 0.05;    // a path's point every this many seconds
constexpr double kTail_s = 2;       // the speed is read over the path's last this many seconds
constexpr float kJumpGap = 96;      // a step this long between two points is a teleport or a spawn
constexpr int kMaxPx = 1100;        // a panel's longer side, pixels

int item_kind(const std::string& model, int skin) {
  if (model == "progs/armor.mdl") return skin >= 0 && skin <= 2 ? skin : -1;
  if (model == "maps/b_bh100.bsp") return 3;
  if (model == "progs/quaddama.mdl") return 4;
  if (model == "progs/invulner.mdl") return 5;
  if (model == "progs/g_rock2.mdl") return 6;
  return -1;
}

struct Point {
  double t = 0;
  sh::Vec3f at;
  bool air = false;
};

// One pickup: the item, where it lay, the recording, and the taker's path up to it (oldest first).
struct Approach {
  int kind = -1;
  int recording = 0;
  double t = 0;
  sh::Vec3f item;
  std::vector<Point> path;
};

struct Read {
  bool ok = false, other_map = false;
  std::string error;
  double seconds = 0;
  int players = 0;
  std::vector<Approach> approaches;
  int64_t taken_no_taker = 0;
};

void read_one(std::vector<uint8_t> bytes, const la::StudyMap& m, int recording, double before,
              Read& out) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, out.error)) return;
  const sh::Hull& body = m.bsp.models[0].hulls[1];
  struct Spot {
    int kind = -1, model = 0;
    bool there = false;
  };
  std::vector<Spot> spots;
  std::vector<std::deque<Point>> hist(32);
  std::vector<double> next(32, -1e9);
  double t0 = -1;
  for (;;) {
    const cl::DemoRead rd = cl::next_frame(d, out.error);
    if (rd == cl::DemoRead::error) return;
    if (rd == cl::DemoRead::end) break;
    const cl::DemoState& s = d.state;
    if (!s.map.empty() && s.map != m.name) {
      out.other_map = true;
      out.ok = true;
      return;
    }
    if (s.intermission || s.players.size() < 32) continue;
    if (t0 < 0) t0 = s.time;
    out.seconds = s.time - t0;
    // Each live player's path, a point every kStep_s; a death or a jump in place starts it over.
    int live_n = 0;
    for (size_t i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[i];
      const bool live = p.present && !p.dead && !s.names[i].empty();
      if (!live) {
        hist[i].clear();
        continue;
      }
      live_n++;
      if (s.time < next[i]) continue;
      next[i] = s.time + kStep_s;
      if (!hist[i].empty()) {
        const sh::Vec3f d3 = p.origin - hist[i].back().at;
        if (d3.x * d3.x + d3.y * d3.y + d3.z * d3.z > kJumpGap * kJumpGap * 4) hist[i].clear();
      }
      const sh::HullTrace down =
          sh::trace_hull(body, p.origin, {p.origin.x, p.origin.y, p.origin.z - 2});
      hist[i].push_back({s.time, p.origin, !(down.startsolid || down.fraction < 1)});
      while (!hist[i].empty() && s.time - hist[i].front().t > before + kStep_s) hist[i].pop_front();
    }
    out.players = std::max(out.players, live_n);
    // The key items: an entity seen with a key item's model is that item's spot from then on.
    if (spots.size() < s.entities.size()) spots.resize(s.entities.size());
    for (size_t e = 33; e < s.entities.size(); e++) {
      const cl::DemoEntity& en = s.entities[e];
      Spot& sp = spots[e];
      if (sp.kind < 0 && en.present && en.model > 0 &&
          static_cast<size_t>(en.model) < s.models.size()) {
        sp.kind = item_kind(s.models[static_cast<size_t>(en.model)], en.skin);
        sp.model = en.model;
      }
      if (sp.kind < 0) continue;
      const bool there = en.present && en.model == sp.model;
      if (!there && sp.there) {
        // Taken: the live player nearest its spot, within a player's reach.
        int taker = -1;
        float best = 1e30f;
        for (size_t i = 0; i < 32; i++) {
          const cl::DemoEntity& p = s.players[i];
          if (!p.present || p.dead || s.names[i].empty() || hist[i].empty()) continue;
          const float dx = p.origin.x - en.origin.x, dy = p.origin.y - en.origin.y;
          const float dz = p.origin.z - en.origin.z;
          const float d2 = dx * dx + dy * dy;
          if (d2 > kReachXY * kReachXY || dz > kReachZ || dz < -kReachZ || d2 >= best) continue;
          best = d2;
          taker = static_cast<int>(i);
        }
        if (taker < 0) {
          out.taken_no_taker++;
        } else {
          Approach a;
          a.kind = sp.kind;
          a.recording = recording;
          a.t = s.time;
          a.item = en.origin;
          a.path.assign(hist[static_cast<size_t>(taker)].begin(),
                        hist[static_cast<size_t>(taker)].end());
          out.approaches.push_back(std::move(a));
        }
      }
      sp.there = there;
    }
  }
  out.ok = true;
}

// The point of `a`'s path `back` seconds before its pickup (its first point when shorter).
const Point& at_back(const Approach& a, double back) {
  for (const Point& p : a.path)
    if (p.t >= a.t - back) return p;
  return a.path.back();
}

struct Way {
  sh::Vec3f at;        // its first point (the leader)
  sh::Vec3f sum{0, 0, 0};
  std::vector<size_t> members;
};

// The ways in: the points `from` seconds before each pickup grouped by a leader pass.
std::vector<Way> ways_in(const std::vector<const Approach*>& as, double from, float group) {
  std::vector<Way> ways;
  for (size_t k = 0; k < as.size(); k++) {
    const sh::Vec3f p = at_back(*as[k], from).at;
    Way* in = nullptr;
    for (Way& w : ways) {
      const sh::Vec3f d = p - w.at;
      if (d.x * d.x + d.y * d.y + d.z * d.z * 4 <= group * group) {
        in = &w;
        break;
      }
    }
    if (in == nullptr) {
      ways.push_back({p, {0, 0, 0}, {}});
      in = &ways.back();
    }
    in->members.push_back(k);
    in->sum = in->sum + p;
  }
  std::stable_sort(ways.begin(), ways.end(),
                   [](const Way& a, const Way& b) { return a.members.size() > b.members.size(); });
  return ways;
}

double quantile(std::vector<double> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[static_cast<size_t>(q * static_cast<double>(v.size() - 1) + 0.5)];
}

struct Side {
  std::string label;
  std::vector<Read> reads;
  double minutes = 0;  // player-minutes alive are not read: match minutes
};

// A panel: the floor in window `v`, the paths, the item, the ways in.
gl::Picture panel(const sh::MoveWorld& world, const la::TopView& v,
                  const std::vector<const Approach*>& as, const std::vector<Way>& ways,
                  const std::string& label, double from) {
  gl::Picture p = la::floor_picture(world, v, v.hi.z);
  for (const Approach* a : as) {
    const double t0 = a->path.front().t, span = std::max(0.01, a->t - t0);
    for (size_t k = 1; k < a->path.size(); k++) {
      const Point &u = a->path[k - 1], &w = a->path[k];
      const sh::Vec3f d = w.at - u.at;
      if (d.x * d.x + d.y * d.y > kJumpGap * kJumpGap) continue;  // a teleporter's hop
      const double f = (w.t - t0) / span;
      const gl::Rgba c{static_cast<uint8_t>(40 + 205 * f), static_cast<uint8_t>(90 + 40 * f),
                       static_cast<uint8_t>(220 - 190 * f), 90};
      gl::draw_line(p, v.px(u.at.x), v.py(u.at.y), v.px(w.at.x), v.py(w.at.y), c, w.air ? 1 : 2);
    }
  }
  if (!as.empty()) {
    const sh::Vec3f it = as.front()->item;
    gl::draw_ring(p, v.px(it.x), v.py(it.y), 14, {210, 20, 20, 255});
    gl::draw_ring(p, v.px(it.x), v.py(it.y), 15, {210, 20, 20, 255});
  }
  const double n = static_cast<double>(std::max<size_t>(1, as.size()));
  for (size_t w = 0; w < ways.size() && w < 9; w++) {
    const Way& way = ways[w];
    const double k = static_cast<double>(way.members.size());
    if (k / n < 0.03) break;
    const double cx = v.px(way.sum.x / k), cy = v.py(way.sum.y / k);
    gl::fill_disc(p, cx, cy, 11, {20, 20, 20, 230});
    gl::draw_text(p, cx - 5, cy - 7, std::to_string(w + 1), {255, 255, 255, 255}, 2);
    gl::draw_text(p, cx + 14, cy - 4, to_fixed(100 * k / n, 0) + "%", {20, 20, 20, 255}, 2);
  }
  gl::draw_text(p, 8, 6,
                label + ": " + std::to_string(as.size()) + " PICKUPS, WAYS IN " +
                    to_fixed(from, 1) + " S BEFORE",
                {20, 20, 20, 255}, 2);
  return p;
}

void run(gl::Lab& lab) {
  std::string error;
  const double before = std::max(0.5, lab.arg_num("before"));
  const double from = std::clamp(lab.arg_num("from"), 0.1, before);
  const auto group = static_cast<float>(std::max(32.0, lab.arg_num("group")));
  std::vector<std::pair<std::string, std::string>> dirs;  // label, folder
  if (!lab.arg("dir").empty()) dirs.push_back({lab.arg("label"), lab.arg("dir")});
  if (!lab.arg("versus").empty()) dirs.push_back({lab.arg("versus-label"), lab.arg("versus")});
  if (dirs.empty()) {
    lab.note("se-approach: no --dir: nothing to read (people's recordings of one map, and "
             "--versus=<se-team --save's folder>)");
    return;
  }
  la::StudyMap m;
  if (!la::load_study_map("", lab.arg("basedir"), lab.arg("game"), lab.arg("mapname"), m,
                          error)) {
    lab.fail("se-approach: " + error, "next: make lab LAB=se-approach ARGS=--help; skill: bots");
    return;
  }
  std::vector<Side> sides;
  for (const auto& [label, dir] : dirs) {
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file() && la::is_recording(e.path())) paths.push_back(e.path().string());
    if (ec || paths.empty()) {
      lab.fail("se-approach: the folder " + dir + " holds no recordings" +
                   (ec ? ": " + ec.message() : ""),
               "next: pass a folder of .mvd or .ged recordings of --mapname");
      return;
    }
    std::sort(paths.begin(), paths.end());
    Side sd;
    sd.label = label;
    sd.reads.resize(paths.size());
    ge::core::parallel_bands(paths.size(), paths.size(), [&](size_t b, size_t e) {
      for (size_t i = b; i < e; i++)
        read_one(la::read_whole(paths[i]), m, static_cast<int>(i), before, sd.reads[i]);
    });
    for (const Read& r : sd.reads)
      if (r.ok && !r.other_map) sd.minutes += r.seconds / 60;
    sides.push_back(std::move(sd));
  }

  gl::LabTable ways_t{"each item's ways in (the path " + to_fixed(from, 1) +
                          " s before the pickup, grouped within " + to_fixed(group, 0) +
                          " units): share, the time from there (s, p50), the speed over the "
                          "last 2 s (units/s, p50), the path in the air, the height climbed (p50)",
                      {"side", "item", "way", "x y z", "pickups", "share", "time", "speed", "air",
                       "climb"},
                      {}};
  std::string tsv = "side\titem\trecording\tpickup_t\tt_before\tx\ty\tz\tair\n";
  for (int kind = 0; kind < kKinds; kind++) {
    std::vector<std::vector<const Approach*>> per(sides.size());
    float lox = 1e30f, loy = 1e30f, hix = -1e30f, hiy = -1e30f, loz = 1e30f, hiz = -1e30f;
    for (size_t si = 0; si < sides.size(); si++)
      for (const Read& r : sides[si].reads)
        for (const Approach& a : r.approaches) {
          if (a.kind != kind || a.path.size() < 2) continue;
          per[si].push_back(&a);
          for (const Point& p : a.path) {
            lox = std::min(lox, p.at.x);
            hix = std::max(hix, p.at.x);
            loy = std::min(loy, p.at.y);
            hiy = std::max(hiy, p.at.y);
            loz = std::min(loz, p.at.z);
            hiz = std::max(hiz, p.at.z);
          }
        }
    size_t any = 0;
    for (const auto& v : per) any += v.size();
    if (any == 0) continue;
    const std::string key = kNames[kind];
    std::vector<gl::Picture> panels;
    const la::TopView view{{lox - 96, loy - 96, loz - 96}, {hix + 96, hiy + 96, hiz + 64}};
    for (size_t si = 0; si < sides.size(); si++) {
      const Side& sd = sides[si];
      const std::vector<const Approach*>& as = per[si];
      const std::string pre = "approach." + sd.label + "." + key;
      lab.metric(pre + ".pickups", static_cast<double>(as.size()), "count");
      lab.metric(pre + ".per-min",
                 sd.minutes > 0 ? static_cast<double>(as.size()) / sd.minutes : 0, "per-min");
      std::vector<double> speed, air, climb, run;
      for (const Approach* a : as) {
        const Point& tail = at_back(*a, kTail_s);
        const sh::Vec3f d = a->path.back().at - tail.at;
        double len = 0;
        int in_air = 0;
        for (size_t k = 1; k < a->path.size(); k++) {
          const sh::Vec3f s = a->path[k].at - a->path[k - 1].at;
          if (s.x * s.x + s.y * s.y > kJumpGap * kJumpGap) continue;
          len += std::sqrt(s.x * s.x + s.y * s.y);
          in_air += a->path[k].air ? 1 : 0;
        }
        (void)d;
        double tail_len = 0;
        for (size_t k = 1; k < a->path.size(); k++) {
          if (a->path[k].t < tail.t) continue;
          const sh::Vec3f s = a->path[k].at - a->path[k - 1].at;
          if (s.x * s.x + s.y * s.y > kJumpGap * kJumpGap) continue;
          tail_len += std::sqrt(s.x * s.x + s.y * s.y);
        }
        speed.push_back(tail_len / std::max(0.1, a->t - tail.t));
        air.push_back(static_cast<double>(in_air) / static_cast<double>(a->path.size() - 1));
        climb.push_back(a->path.back().at.z - a->path.front().at.z);
        run.push_back(len / std::max(0.1, a->t - a->path.front().t));
        for (const Point& p : a->path)
          tsv += sd.label + "\t" + key + "\t" + std::to_string(a->recording) + "\t" +
                 to_fixed(a->t, 2) + "\t" + to_fixed(a->t - p.t, 2) + "\t" + to_fixed(p.at.x, 0) +
                 "\t" + to_fixed(p.at.y, 0) + "\t" + to_fixed(p.at.z, 0) + "\t" +
                 (p.air ? "1" : "0") + "\n";
      }
      lab.metric(pre + ".speed-last-2s-p50", quantile(speed, 0.5), "units/s");
      lab.metric(pre + ".speed-p50", quantile(run, 0.5), "units/s");
      lab.metric(pre + ".air-p50", quantile(air, 0.5), "share");
      const std::vector<Way> ways = ways_in(as, from, group);
      for (size_t w = 0; w < ways.size() && w < 8; w++) {
        const Way& way = ways[w];
        const double k = static_cast<double>(way.members.size());
        std::vector<double> wt, ws, wa, wc;
        for (size_t mi : way.members) {
          const Approach& a = *as[mi];
          wt.push_back(a.t - at_back(a, from).t);
          ws.push_back(speed[mi]);
          wa.push_back(air[mi]);
          wc.push_back(climb[mi]);
        }
        ways_t.rows.push_back(
            {sd.label, key, std::to_string(w + 1),
             to_fixed(way.sum.x / k, 0) + " " + to_fixed(way.sum.y / k, 0) + " " +
                 to_fixed(way.sum.z / k, 0),
             std::to_string(way.members.size()),
             to_fixed(100 * k / static_cast<double>(as.size()), 0) + "%",
             to_fixed(quantile(wt, 0.5), 1), to_fixed(quantile(ws, 0.5), 0),
             to_fixed(100 * quantile(wa, 0.5), 0) + "%", to_fixed(quantile(wc, 0.5), 0)});
      }
      panels.push_back(panel(*m.world, view, as, ways, sd.label, from));
    }
    // The panels side by side, each scaled so its longer side is at most kMaxPx.
    const int pw = panels.front().width, ph = panels.front().height;
    const double scale = std::min(1.0, static_cast<double>(kMaxPx) / std::max(pw, ph));
    const int sw = std::max(1, static_cast<int>(pw * scale)),
              shh = std::max(1, static_cast<int>(ph * scale));
    gl::Picture out =
        gl::create_picture(sw * static_cast<int>(panels.size()) + 8 * static_cast<int>(panels.size() - 1),
                           shh, {255, 255, 255, 255});
    for (size_t i = 0; i < panels.size(); i++)
      gl::blit(out, gl::resized(panels[i], sw, shh), static_cast<int>(i) * (sw + 8), 0);
    std::string counts;
    for (size_t si = 0; si < sides.size(); si++)
      counts += (si ? ", " : "") + sides[si].label + " " + std::to_string(per[si].size()) +
                " pickups in " + to_fixed(sides[si].minutes, 0) + " match-minutes";
    lab.shot({"approach-" + key, std::string("THE WAY TO THE ") + kLong[kind],
              "From above, to scale, north up, the floor shaded by height (walls dark), cropped "
              "round the paths. Every pickup's taker's last " +
                  to_fixed(before, 1) +
                  " s: blue at its start to orange at the pickup, thin where he was in the air; "
                  "the item a red ring; each way in (where the path was " +
                  to_fixed(from, 1) +
                  " s before) a numbered disc with its share. " + counts + ".",
              "the ways people come in drawn thick where many paths agree; the item at the "
              "paths' orange ends; a way one side takes and the other never does",
              {{"before", to_fixed(before, 1)},
               {"from", to_fixed(from, 1)},
               {"group", to_fixed(group, 0)},
               {"map", m.name}}},
             out);
  }
  lab.table(ways_t);
  for (const Side& sd : sides) {
    int64_t none = 0;
    for (const Read& r : sd.reads) none += r.taken_no_taker;
    lab.metric("approach." + sd.label + ".no-taker", static_cast<double>(none), "count");
    lab.metric("approach." + sd.label + ".match-minutes", sd.minutes, "min");
  }
  if (const std::string to = lab.arg("paths"); !to.empty())
    if (!gl::write_file(to, {reinterpret_cast<const uint8_t*>(tsv.data()), tsv.size()}))
      lab.fail("se-approach: --paths " + to + " was not written",
               "next: pass --paths=<a writable path>");
}

const gl::LabSpec kSpec{
    "se-approach",
    gl::LabKind::sim,
    "game:shooters_edge",
    "how players come to the key items, off recordings: each pickup's last seconds drawn on the "
    "map and grouped into ways in (games/shooters_edge/labs/se_approach_lab.cpp)",
    "bots",
    "Which ways do players take to each key item in the seconds before they take it, how fast "
    "and how much in the air: people's recordings beside the bots'?",
    {
        {"dir", "", "a folder of one map's recordings (.mvd or .ged): people's"},
        {"label", "people", "the --dir side's name"},
        {"versus", "", "a second folder drawn beside it (se-team --save's: the bots')"},
        {"versus-label", "bots", "the --versus side's name"},
        {"basedir", "", "a folder holding the game folder the map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "e1m2", "the recordings' map"},
        {"before", "5", "the path kept before each pickup, s"},
        {"from", "2", "the way in read this many seconds before the pickup"},
        {"group", "200", "ways in this near are one, units (height counted twice)"},
        {"paths", "", "write every path's points to this .tsv"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
