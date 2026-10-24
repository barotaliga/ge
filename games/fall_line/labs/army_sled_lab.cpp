// LAB `army-sleds`: the army's snowmobiles on a real ski area. The hunted man stands still (stood
// again every step) at a spot of the map while the hunt is pinned at its stars and a helicopter
// hovers over him, and the army runs through the real `step` (`step_army_sleds` after the men):
// THE SLEDS table (per scenario, the machines called, the most out at once, the short halts, the
// times dug out and stood back on their belts, the nearest any came, when the first came within
// its halt range, the crews off to take him in, the arrest and whose it was, and the machines still
// caught at the end) and, per scenario, the map with every machine's path over it.
//
// Scenarios: `open` (open snow some 350 m up the mountain from the village, three stars, the
// army shooting to kill: the machines drive up and halt side-on to him), `run` (on a run some
// 40% of the way from the village to the top of the runs, five stars: the roster's three and the
// waves' machines climb the mountain at him), `face` (beside the highest lift's line, more than
// halfway up it on a face steeper than a laden machine climbs, four stars: they come as far as the
// face's foot and wait there), `arrest` (the open spot with the army not shooting to kill: a
// machine runs him down and its crew get off and take him in), `answer` (his answer, at the open
// spot at three stars: once a machine has halted side-on to him he skis into it at a run and
// knocks its crew off, then takes the empty machine with the machine press and rides it off
// toward the village; THE ANSWER table and a close-up of the knock and the theft).
//
// A machine still caught at the end, a scenario shooting to kill within reach with no halt, an
// arrest that never comes, or an answer with no crew knocked off, no theft or no heat raised by
// either is a failure (exit 1).
//
// ge:summary Lab: the army's snowmobiles on seed 38: the call, the drive, the halt, the arrest
// ge:keywords lab army snowmobile sled army-sled halt dismount arrest dug righted paths stuck
// ge:keywords scenario open slope answer knock-off steal stolen hull
// ge:skill sim labs
// ge:test tests/fall_line_army_sled_test.cpp tests/fall_line_army_meet_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_sled.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/skier.hpp"
#include "snowmobile/spec.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace as = fall_line::sim::army_sled;
namespace lm = ge::core::libm;
using ge::core::hypot;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

enum class Scene : uint8_t { open, run, face, arrest };
constexpr Scene kScenes[] = {Scene::open, Scene::run, Scene::face, Scene::arrest};

const char* scene_name(Scene s) {
  return s == Scene::open ? "open" : s == Scene::run ? "run" : s == Scene::face ? "face" : "arrest";
}

// The open spot: this far up the mountain from the village's machines at the least, m, clear of
// every trunk by this much, m.
constexpr double kOpenM = 350;
constexpr double kClearM = 15;
// The run spot: this share of the way up from the village to the top of the runs.
constexpr double kRunUp = 0.4;
// The face spot: up the highest lift's line, its share, and this far beside it, m.
constexpr double kFaceUp = 0.6;
constexpr double kFaceBesideM = 80;
// The helicopter keeping sight of him, m over him.
constexpr double kHoverM = 150;
// The colours the machines' paths are drawn in, by id.
constexpr Rgba kPathColours[] = {{0, 200, 255}, {255, 140, 0},   {0, 230, 120},
                                 {255, 230, 0}, {200, 120, 255}, {255, 90, 90}};

struct Hunt {
  fs::Spot q;
  int32_t stars = 0;
  int32_t called = 0;
  int32_t most = 0;
  int32_t halts = 0;
  int32_t dug = 0;
  int32_t righted = 0;
  int32_t dismounts = 0;
  double nearest = 1e9;
  double first_near = -1;  // s, the first machine within its halt range
  double arrested_at = -1;
  bool by_crew = false;  // the arrest was a snowmobile crew's
  int32_t caught = 0;    // machines caught (two windows or more in a row) at the end
  std::vector<std::vector<fs::Spot>> paths;
  std::vector<fs::ArmySled> last;
};

bool clear_of_trees(const fs::GameState& st, double x, double z) {
  for (const mg::TreeDef& t : st.level->trees) {
    if (hypot(t.x - x, t.z - z) < kClearM + t.radius) return false;
  }
  return true;
}

// Where he stands in a scene.
fs::Spot spot_of(const fs::GameState& st, Scene scene) {
  if (scene == Scene::run) {
    // The point of a run nearest the height `kRunUp` of the way to the runs' top, on open snow.
    double top = st.level->ground_at(st.army.pad_x, st.army.pad_z);
    const double base = top;
    for (const mg::Track& t : *st.runs) {
      for (const mg::TrackPoint& p : t.points) top = p.y > top ? p.y : top;
    }
    const double aim = base + (top - base) * kRunUp;
    fs::Spot best{st.army.pad_x, 0, st.army.pad_z};
    double off = 1e9;
    for (const mg::Track& t : *st.runs) {
      for (size_t i = 0; i < t.points.size(); i += 10) {
        const mg::TrackPoint& p = t.points[i];
        const double e = p.y > aim ? p.y - aim : aim - p.y;
        if (e < off && clear_of_trees(st, p.x, p.z)) {
          off = e;
          best = {p.x, 0, p.z};
        }
      }
    }
    return best;
  }
  if (scene == Scene::face) {
    const auto& lifts = *st.lifts;
    size_t high = 0;
    const auto top = [&st](const ml::LiftPlan& p) { return st.level->ground_at(p.top.x, p.top.z); };
    for (size_t i = 1; i < lifts.size(); i++) {
      if (top(lifts[i]) > top(lifts[high])) high = i;
    }
    const ml::LiftPlan& p = lifts[high];
    const double u = p.length * kFaceUp;
    return {p.bottom.x + p.dx * u - p.dz * kFaceBesideM, 0,
            p.bottom.z + p.dz * u + p.dx * kFaceBesideM};
  }
  // Up the mountain from the machines' spot, the first open snow.
  const fs::ArmyState& a = st.army;
  const double fx = lm::sin(a.pad_heading);
  const double fz = lm::cos(a.pad_heading);
  for (int32_t i = 0; i < 40; ++i) {
    const double r = kOpenM + 10.0 * i;
    for (const double side : {0.0, 60.0, -60.0, 120.0, -120.0}) {
      const double x = a.pad_x + fx * r + fz * side;
      const double z = a.pad_z + fz * r - fx * side;
      if (clear_of_trees(st, x, z)) return {x, 0, z};
    }
  }
  return {a.pad_x + fx * kOpenM, 0, a.pad_z + fz * kOpenM};
}

Hunt run_scene(const std::shared_ptr<const mg::Level>& level, Scene scene, double limit) {
  fs::GameState st = fs::create_free_ride(level, 0);
  // Staged: the snowmobiles alone: the helicopters are the army-flight lab's.
  st.army.send_helis = false;
  st.army.fire = false;
  Hunt h;
  h.stars = scene == Scene::run ? 5 : scene == Scene::face ? 4 : 3;
  const fs::Spot at = spot_of(st, scene);
  st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = scene != Scene::arrest;
  fs::set_wanted(st, h.stars);
  h.q = fs::quarry_of(st);
  const fs::Controls none{};
  while (st.t < limit) {
    fs::WantedState& w = st.wanted;
    if (w.ended == fs::WantedEnd::running && w.stars > 0) {
      w.heat = h.stars;
      std::erase_if(w.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
      w.pursuers.push_back(
          fs::Pursuer{1, fs::PursuerKind::helicopter, h.q.x, h.q.y + kHoverM, h.q.z});
    }
    // He stands where he was put, even on a slope his skis would slide down.
    st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind == fs::EventKind::army && fs::army_change(e) == fs::ArmyChange::arrest) {
        h.arrested_at = st.t;
        for (const fs::Soldier& s : st.army.soldiers) {
          if (s.id == st.army.arrested_by) h.by_crew = s.from == fs::SoldierFrom::sled;
        }
      }
      if (e.kind != fs::EventKind::army_sled) continue;
      const auto c = static_cast<fs::ArmySledChange>(e.gate);
      h.called += c == fs::ArmySledChange::called ? 1 : 0;
      h.halts += c == fs::ArmySledChange::halt ? 1 : 0;
      h.dug += c == fs::ArmySledChange::dug ? 1 : 0;
      h.righted += c == fs::ArmySledChange::righted ? 1 : 0;
      h.dismounts += c == fs::ArmySledChange::dismount ? 1 : 0;
    }
    int32_t out = 0;
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.mode == fs::ArmySledMode::due) continue;
      out++;
      const double d = hypot(m.body.x - h.q.x, m.body.z - h.q.z);
      h.nearest = d < h.nearest ? d : h.nearest;
      if (h.first_near < 0 && d < as::kHaltRangeM) h.first_near = st.t;
    }
    h.most = out > h.most ? out : h.most;
    if (st.steps % static_cast<int64_t>(fs::kHz) == 0) {
      for (const fs::ArmySled& m : st.army.sleds) {
        if (m.mode == fs::ArmySledMode::due) continue;
        const auto id = static_cast<size_t>(m.id);
        if (h.paths.size() <= id) h.paths.resize(id + 1);
        h.paths[id].push_back({m.body.x, m.body.y, m.body.z});
      }
    }
    if (h.arrested_at >= 0 && st.t > h.arrested_at + 5) break;
  }
  for (const fs::ArmySled& m : st.army.sleds) h.caught += m.progress.slow >= 2 ? 1 : 0;
  h.last = st.army.sleds;
  return h;
}

// ─── his answer ──────────────────────────────────────────────────────────────────────────────

// THE ANSWER: how far out to the machine's right he sets off at it, m, and how fast, m/s (past the
// knock off, short of being thrown himself); how long after the knock he stands beside it to take
// it, s; how long he rides it off, s, at what throttle.
constexpr double kAnswerOffM = 3;
constexpr double kAnswerSpeed = 4.6;
constexpr double kStealAfterS = 1.5;
constexpr double kRideS = 25;
constexpr double kRideTuck = 0.7;
// The close-up: metres across it.
constexpr double kCloseM = 36;

struct Answer {
  bool halted = false;
  double halt_at = -1;            // s, the machine he answers settled in its halt
  double knocked_at = -1;         // s, its crew knocked off
  int32_t crew_down = 0;          // soldiers lying beside it after the knock
  int32_t crew_up = 0;            // ...up and after him again `kDownS` later
  double heat_before = 0;         // the heat before the knock
  double heat_knock = 0;          // ...after it
  double stolen_at = -1;          // s, the machine taken
  double heat_steal = 0;          // the heat after the theft
  int32_t stars_end = 0;          // the stars at the end
  double ridden = 0;              // m ridden on it
  bool thrown = false;            // he went down at the knock
  fs::Spot centre;                // the machine at the knock
  double heading = 0;             // ...its heading
  std::vector<fs::Spot> skier;    // his path to it, every step
  std::vector<fs::Spot> down_at;  // the crew lying, just after the knock
  std::vector<fs::Spot> up_at;    // ...and once up
  std::vector<fs::Spot> ride;     // the stolen machine's path, every 0.25 s
};

Answer run_answer(const std::shared_ptr<const mg::Level>& level, double limit) {
  fs::GameState st = fs::create_free_ride(level, 0);
  st.army.send_helis = false;
  st.army.fire = false;
  Answer a;
  const fs::Spot at = spot_of(st, Scene::open);
  st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = true;
  fs::set_wanted(st, 3);
  const fs::Controls none{};
  const auto hover = [&st](const fs::Spot& q) {
    fs::WantedState& w = st.wanted;
    std::erase_if(w.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
    w.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, q.x, q.y + kHoverM, q.z});
  };
  // The hunt pinned at three stars until a machine has settled in its halt.
  int32_t id = -1;
  while (st.t < limit && id < 0) {
    st.wanted.heat = 3;
    hover(fs::quarry_of(st));
    st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.mode == fs::ArmySledMode::halt && m.t > 2 * as::kSettleS && m.body.speed < 0.5) {
        id = m.id;
      }
    }
  }
  if (id < 0) return a;
  a.halted = true;
  a.halt_at = st.t;
  const auto sled = [&st](int32_t k) -> const fs::ArmySled* {
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.id == k) return &m;
    }
    return nullptr;
  };
  // At it from its right, at a run.
  const ge::snowmobile::SledState b = sled(id)->body;
  a.centre = {b.x, b.y, b.z};
  a.heading = b.heading;
  const double rx = lm::cos(b.heading);
  const double rz = -lm::sin(b.heading);
  st.racers[0].skier =
      ge::skier::stand_skier(st.world(), b.x + rx * kAnswerOffM, b.z + rz * kAnswerOffM,
                             lm::atan2(-rx, -rz), kAnswerSpeed);
  a.heat_before = st.wanted.heat;
  const double t0 = st.t;
  while (st.t < t0 + kStealAfterS) {
    hover(fs::quarry_of(st));
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    const ge::skier::SkierState& c = st.racers[0].skier;
    if (a.knocked_at < 0) a.skier.push_back({c.x, c.y, c.z});
    a.thrown = a.thrown || c.down;
    for (const fs::GameEvent& e : st.events) {
      if (e.kind == fs::EventKind::army_sled &&
          static_cast<fs::ArmySledChange>(e.gate) == fs::ArmySledChange::knocked) {
        a.knocked_at = st.t;
        a.heat_knock = st.wanted.heat;
      }
    }
  }
  for (const fs::Soldier& s : st.army.soldiers) {
    if (s.from != fs::SoldierFrom::sled || s.mode != fs::SoldierMode::down) continue;
    a.crew_down++;
    a.down_at.push_back({s.x, s.y, s.z});
  }
  // Stood beside the empty machine, still, and the press.
  const fs::ArmySled* m = sled(id);
  if (m == nullptr || m->mode != fs::ArmySledMode::empty) return a;
  st.racers[0].skier = ge::skier::stand_skier(st.world(), m->body.x + rx * 2, m->body.z + rz * 2,
                                              lm::atan2(-rx, -rz), 0);
  fs::Controls press{};
  press.machine = true;
  hover(fs::quarry_of(st));
  fs::step(st, std::span<const fs::Controls>(&press, 1));
  if (!st.stolen.rider) return a;
  a.stolen_at = st.t;
  a.heat_steal = st.wanted.heat;
  // Ridden off toward the village.
  const double t1 = st.t;
  double px = st.stolen.body.x;
  double pz = st.stolen.body.z;
  bool counted = false;
  while (st.t < t1 + kRideS && st.stolen.rider) {
    const ge::snowmobile::SledState& r = st.stolen.body;
    const double to = lm::atan2(st.army.pad_x - r.x, st.army.pad_z - r.z);
    fs::Controls gas{};
    gas.ski.tuck = kRideTuck;
    gas.ski.steer = ge::core::clamp(ge::core::angle_diff(r.heading, to) * 1.5, -1, 1);
    hover(fs::quarry_of(st));
    fs::step(st, std::span<const fs::Controls>(&gas, 1));
    a.ridden += hypot(st.stolen.body.x - px, st.stolen.body.z - pz);
    px = st.stolen.body.x;
    pz = st.stolen.body.z;
    if (st.steps % static_cast<int64_t>(fs::kHz / 4) == 0) a.ride.push_back({px, 0, pz});
    if (!counted && st.t > a.knocked_at + fall_line::sim::army::kDownS + 1) {
      counted = true;
      for (const fs::Soldier& s : st.army.soldiers) {
        if (s.from != fs::SoldierFrom::sled || s.mode == fs::SoldierMode::down) continue;
        a.crew_up++;
        a.up_at.push_back({s.x, s.y, s.z});
      }
    }
  }
  a.stars_end = st.wanted.stars;
  return a;
}

// THE CLOSE-UP: `kCloseM` across round the machine he knocked the crew off, from above.
ge::lab::Picture draw_answer(const Answer& a, int px) {
  ge::lab::Picture pic = ge::lab::create_picture(px, px, Rgba{236, 242, 248});
  const double k = px / kCloseM;
  const auto sx = [&](double x) { return (x - a.centre.x) * k + px / 2.0; };
  const auto sz = [&](double z) { return (z - a.centre.z) * k + px / 2.0; };
  for (int g = -18; g <= 18; g += 2) {
    const double u = (g + kCloseM / 2) * k;
    ge::lab::draw_line(pic, u, 0, u, px, Rgba{214, 222, 232}, 1);
    ge::lab::draw_line(pic, 0, u, px, u, Rgba{214, 222, 232}, 1);
  }
  // The machine's plan: its capsule's box, nose marked.
  const double fx = lm::sin(a.heading);
  const double fz = lm::cos(a.heading);
  const double hl = ge::snowmobile::spec::kLength / 2;
  const double hw = ge::snowmobile::spec::kHalfWidth;
  const double cx[4] = {hl, hl, -hl, -hl};
  const double cw[4] = {hw, -hw, -hw, hw};
  for (int i = 0; i < 4; i++) {
    const int j = (i + 1) % 4;
    const double x0 = a.centre.x + fx * cx[i] + fz * cw[i];
    const double z0 = a.centre.z + fz * cx[i] - fx * cw[i];
    const double x1 = a.centre.x + fx * cx[j] + fz * cw[j];
    const double z1 = a.centre.z + fz * cx[j] - fx * cw[j];
    ge::lab::draw_line(pic, sx(x0), sz(z0), sx(x1), sz(z1), Rgba{60, 70, 50}, 3);
  }
  ge::lab::fill_disc(pic, sx(a.centre.x + fx * hl), sz(a.centre.z + fz * hl), 5, Rgba{60, 70, 50});
  // The stolen machine's way off, his way in, the crew down and up.
  for (size_t i = 1; i < a.ride.size(); i++) {
    ge::lab::draw_line(pic, sx(a.ride[i - 1].x), sz(a.ride[i - 1].z), sx(a.ride[i].x),
                       sz(a.ride[i].z), Rgba{150, 60, 220}, 3);
  }
  for (size_t i = 1; i < a.skier.size(); i++) {
    ge::lab::draw_line(pic, sx(a.skier[i - 1].x), sz(a.skier[i - 1].z), sx(a.skier[i].x),
                       sz(a.skier[i].z), Rgba{20, 110, 240}, 3);
  }
  if (!a.skier.empty()) {
    ge::lab::fill_disc(pic, sx(a.skier.back().x), sz(a.skier.back().z), 6, Rgba{20, 110, 240});
  }
  for (const fs::Spot& s : a.down_at)
    ge::lab::fill_disc(pic, sx(s.x), sz(s.z), 6, Rgba{220, 40, 40});
  for (const fs::Spot& s : a.up_at) {
    for (const double r : {11.0, 12.0, 13.0})
      ge::lab::draw_ring(pic, sx(s.x), sz(s.z), r, Rgba{0, 160, 70});
  }
  return pic;
}

void answer(ge::lab::Lab& lab, const std::shared_ptr<const mg::Level>& level, double seed,
            double limit, int px, bool pictures) {
  const Answer a = run_answer(level, limit);
  const auto t = [](double v) { return v >= 0 ? fixed(v, 2) : std::string("-"); };
  ge::lab::LabTable table{
      "answer",
      {"halt s", "knocked s", "crew down", "crew up", "heat before", "heat knocked", "stolen s",
       "heat stolen", "ridden m", "stars end", "he fell"},
      {}};
  table.rows.push_back({t(a.halt_at), t(a.knocked_at), std::to_string(a.crew_down),
                        std::to_string(a.crew_up), fixed(a.heat_before, 2), fixed(a.heat_knock, 2),
                        t(a.stolen_at), fixed(a.heat_steal, 2), fixed(a.ridden, 1),
                        std::to_string(a.stars_end), a.thrown ? "yes" : "no"});
  lab.table(std::move(table));
  lab.metric("answer.knocked", a.knocked_at, "s");
  lab.metric("answer.crew-down", a.crew_down, "men");
  lab.metric("answer.crew-up", a.crew_up, "men");
  lab.metric("answer.heat-knocked", a.heat_knock - a.heat_before, "stars");
  lab.metric("answer.stolen", a.stolen_at, "s");
  lab.metric("answer.heat-stolen", a.heat_steal - a.heat_knock, "stars");
  lab.metric("answer.ridden", a.ridden, "m");
  if (!a.halted) {
    lab.fail("answer: no machine halted within " + fixed(limit, 0) + " s",
             "Run the open scenario and look at previews/army-sleds/open.png (skill: sim).");
    return;
  }
  if (a.knocked_at < 0 || a.heat_knock <= a.heat_before) {
    lab.fail("answer: the crew was not knocked off, or the heat did not rise",
             "Read meet_machines in games/fall_line/sim/army_meet.cpp and kKnockOffSpeed in "
             "army_sled_rules.hpp (skill: sim).");
  }
  if (a.stolen_at < 0 || a.heat_steal <= a.heat_knock) {
    lab.fail("answer: the empty machine was not stolen, or the heat did not rise",
             "Read take_of and steal in games/fall_line/sim/sled_ride.cpp (skill: sim).");
  }
  if (!pictures) return;
  lab.shot(
      {"answer",
       "SEED " + fixed(seed, 0) + " HIS ANSWER: A CREW KNOCKED OFF, THE MACHINE STOLEN",
       "A close-up " + fixed(kCloseM, 0) +
           " m across from above (+x right, +z down; grid 2 m) round an army snowmobile "
           "halted side-on to him (dark box, its nose the dot). Blue: his line in at " +
           fixed(kAnswerSpeed * 3.6, 0) +
           " km/h to the knock, the dot where it stopped him. Red dots: its crew knocked "
           "off, lying; green rings: the same men on their feet again " +
           fixed(fall_line::sim::army::kDownS + 1, 0) +
           " s later. Purple: the machine ridden off once he stole it. Heat " +
           fixed(a.heat_before, 1) + " before, " + fixed(a.heat_knock, 1) + " after the knock, " +
           fixed(a.heat_steal, 1) + " after the theft.",
       "a blue line ending at the side of the dark box, two red dots either side of it, "
       "a green ring round or near each, and a purple line leaving from the box",
       {{"seed", fixed(seed, 0)}, {"scenario", "answer"}, {"speed m/s", fixed(kAnswerSpeed, 1)}}},
      draw_answer(a, px));
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
  const int px = static_cast<int>(lab.arg_num("px"));
  const bool pictures = !lab.arg_bool("no-pictures");
  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(fs::free_map(seed, version));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  if (!level->resort || level->resort->lifts.empty()) {
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " has no ski area",
             "Pass a --version that builds a ski area (10 or later; skill: mountain).");
    return;
  }
  ge::lab::LabTable table{"sleds",
                          {"scenario", "stars", "called", "most out", "halts", "dug", "righted",
                           "nearest m", "in range s", "dismounts", "arrest s", "by", "caught"},
                          {}};
  const double k = px / level->size;
  for (const Scene scene : kScenes) {
    const std::string id = scene_name(scene);
    const Hunt h = run_scene(level, scene, limit);
    const bool within = scene == Scene::open || scene == Scene::run;
    table.rows.push_back({id, std::to_string(h.stars), std::to_string(h.called),
                          std::to_string(h.most), std::to_string(h.halts), std::to_string(h.dug),
                          std::to_string(h.righted), h.nearest < 1e9 ? fixed(h.nearest, 1) : "-",
                          h.first_near >= 0 ? fixed(h.first_near, 2) : "-",
                          std::to_string(h.dismounts),
                          h.arrested_at >= 0 ? fixed(h.arrested_at, 2) : "-",
                          h.arrested_at < 0 ? "-"
                          : h.by_crew       ? "crew"
                                            : "army",
                          std::to_string(h.caught)});
    lab.metric(id + ".called", h.called, "machines");
    lab.metric(id + ".most-out", h.most, "machines");
    lab.metric(id + ".halts", h.halts, "halts");
    lab.metric(id + ".dug", h.dug, "times");
    lab.metric(id + ".righted", h.righted, "times");
    lab.metric(id + ".nearest", h.nearest, "m");
    lab.metric(id + ".in-range", h.first_near, "s");
    lab.metric(id + ".caught", h.caught, "machines");
    if (h.caught > 0) {
      lab.fail(id + ": " + std::to_string(h.caught) + " machines still caught at the end",
               "Look at previews/army-sleds/" + id +
                   ".png for where the paths stop; read steer and watch in "
                   "games/fall_line/sim/army_sled.cpp and choose_way in army_drive.cpp (skill: "
                   "sim).");
    }
    if (within && h.halts == 0) {
      lab.fail(id + ": no machine made a short halt within " + fixed(limit, 0) + " s",
               "Look at previews/army-sleds/" + id +
                   ".png; read chase in games/fall_line/sim/army_sled.cpp (skill: sim).");
    }
    if (scene == Scene::arrest) {
      lab.metric(id + ".arrest", h.arrested_at, "s");
      if (h.arrested_at < 0) {
        lab.fail("arrest: nobody took him in within " + fixed(limit, 0) + " s",
                 "Look at previews/army-sleds/arrest.png; read chase and dismount in "
                 "games/fall_line/sim/army_sled.cpp (skill: sim).");
      }
    }
    if (!pictures) continue;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
    for (size_t i = 0; i < h.paths.size(); i++) {
      const auto& path = h.paths[i];
      const Rgba c = kPathColours[i % std::size(kPathColours)];
      for (size_t j = 1; j < path.size(); j++) {
        ge::lab::draw_line(pic, path[j - 1].x * k, path[j - 1].z * k, path[j].x * k, path[j].z * k,
                           c, 2);
      }
    }
    for (const fs::ArmySled& m : h.last) {
      const Rgba c = m.mode == fs::ArmySledMode::halt    ? Rgba{0, 0, 0}
                     : m.mode == fs::ArmySledMode::empty ? Rgba{255, 255, 255}
                     : m.progress.slow >= 2              ? Rgba{255, 0, 0}
                                                         : Rgba{255, 255, 0};
      ge::lab::fill_disc(pic, m.body.x * k, m.body.z * k, 3, c);
    }
    ge::lab::fill_disc(pic, h.q.x * k, h.q.z * k, 5, Rgba{255, 0, 255});
    lab.shot({id,
              "SEED " + fixed(seed, 0) + " ARMY SNOWMOBILES: " + std::string(scene_name(scene)),
              "The map from above (+x right, +z down; the runs red, trees dark green) with the man "
              "hunted (magenta dot) and every army snowmobile's path, a colour a machine, from "
              "the village. Where each machine ended: black halted side-on to him, white left "
              "empty by its crew, red still caught, yellow anything else. " +
                  std::to_string(h.called) + " called, " + std::to_string(h.halts) +
                  " halts; nearest " + fixed(h.nearest, 0) + " m.",
              scene == Scene::arrest
                  ? "one path from the village to the magenta dot and a white dot beside it; no red"
              : scene == Scene::face
                  ? "paths from the village that end in yellow dots at the foot of the steep face "
                    "below him, not wandering along it; no red"
                  : "paths from the village up the mountain toward him that stop a hundred metres "
                    "or more short of him, threading the woods rather than crossing them; no red",
              {{"seed", fixed(seed, 0)},
               {"version", lab.arg("version")},
               {"scenario", id},
               {"stars", std::to_string(h.stars)},
               {"limit s", fixed(limit, 0)}}},
             pic);
  }
  lab.table(std::move(table));
  answer(lab, level, seed, limit, px, pictures);
}

const ge::lab::LabSpec kSpec{
    "army-sleds",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/army_sled (the army's snowmobiles: the call, the drive over the snow, "
    "the short halt, the stand-off, the arrest)",
    "sim",
    "On a real ski area, do the army's snowmobiles get from the village to a man up the mountain "
    "without getting caught on the way, halt side-on to him where they shoot to kill, and run "
    "him down and take him in where they do not? And does his answer work: a crew skied into "
    "knocked off, the empty machine stolen, each raising the heat?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"limit", "420", "each scenario's time limit, s"},
        {"px", "768", "the pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--px=256 --limit=240",
    run,
};
GE_LAB(kSpec);

}  // namespace
