// LAB `se-midair`: MIDAIR on the rules every game's server shares (plugins/shooter/midair.hpp),
// played today by the sequel's rules (the classic arena's come with its rules' move to C++).
//
// - THE RULES: the least height by `k_midair_minheight`, the frags by the rise, and the push a
//   rocket gives by its target's height above the floor, as a chart.
// - THE SEQUEL'S RULES, STAGED: on the sequel's runner (server/sequel_server.hpp, `k_midair 1`) a
//   player holds a target still at a height above the floor (`--heights`) and fires one rocket at
//   him from the floor: whether it counts, the frags it scores, the line everyone reads, and how
//   fast the blow throws him; then one at the feet of a target standing on the floor (the lift).
//   On a hall built here, or on `--sequel-map` from `--sequel-basedir`'s game folder (the
//   sequel's packs), the target at the map's first spawn point.
//
// Deterministic: the digest holds the counts.
//
// ge:summary Lab: midair's shared rules charted and the sequel's hits staged on a map
// ge:keywords lab se-midair midair k_midair k_midair_minheight air-shot rocket bands push lift
// ge:keywords endif sequel
// ge:skill shooter labs
// ge:test tests/shooters_edge_midair_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/survey.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/midair.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

// ---- the rules ----

void rules(gl::Lab& lab) {
  gl::LabTable levels{"k_midair_minheight: the least height a hit counts at",
                      {"level", "name", "height (units)"},
                      {}};
  for (int l = 0; l < sh::kMidairLevels; l++)
    levels.rows.push_back(
        {std::to_string(l), sh::midair_level_name(l), to_fixed(sh::midair_min_height(l), 0)});
  lab.table(levels);
  gl::LabTable bands{
      "a counted hit's frags by its rise (the victim's height over where the "
      "shooter stood when he fired)",
      {"band", "rise", "frags"},
      {{"bronze", "256 or less (and below him)", "1"},
       {"silver", "over 256", "2"},
       {"gold", "over 512", "4"},
       {"platinum", "over 1024", "8"}}};
  lab.table(bands);
  gl::LineChart push;
  push.spikes = false;
  push.frame = {"A ROCKET'S PUSH BY ITS TARGET'S HEIGHT ABOVE THE FLOOR",
                "against the push without midair; the lift is straight up, on top",
                "height (units)", "times the usual push"};
  gl::Trace along{"along the blow (direct hit)", {}, {}},
      weak{"along the blow (weak splash)", {}, {}},
      lift{"straight up (direct hit, a blow from below)", {}, {}};
  for (int h = 0; h <= 300; h += 5) {
    const sh::MidairPush d = sh::midair_push(float(h), 110, false);
    const sh::MidairPush w = sh::midair_push(float(h), 40, false);
    along.x.push_back(h);
    along.y.push_back(d.scale);
    weak.x.push_back(h);
    weak.y.push_back(w.scale);
    lift.x.push_back(h);
    lift.y.push_back(d.scale * d.lift);
  }
  push.traces = {along, weak, lift};
  push.marks = {{sh::kMidairLowAir, "45: lifted below"}};
  lab.shot({"rules-push",
            push.frame.title,
            "How hard a rocket throws its target in midair, by his height above the floor, as a "
            "multiple of the push the game gives without it.",
            "flat below 45 units with a lift on top; from 45 up the push grows by a whole push "
            "every 64 units (double at 109, triple at 173)",
            {}},
           gl::to_picture(gl::line_chart(push).plot));
}

// ---- the sequel, staged ----

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -512, -16}, floor_hi[3] = {1024, 512, 0};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

std::shared_ptr<const br::Map> sequel_map(const gl::Lab& lab, std::string& name,
                                          std::string& error) {
  const std::string& basedir = lab.arg("sequel-basedir");
  if (basedir.empty()) {
    name = "hall";
    return hall(error);
  }
  name = lab.arg("sequel-map");
  cl::SearchPath files;
  if (!cl::open_game_dir(basedir, lab.arg("sequel-game"), files, error)) return nullptr;
  std::vector<uint8_t> store;
  const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
  if (f.empty()) {
    error = "no maps/" + name + ".bsp in the sequel's game folder";
    return nullptr;
  }
  auto map = std::make_shared<br::Map>();
  if (!br::read_bsp({f.data(), f.size()}, map.get(), &error)) return nullptr;
  return map;
}

struct Stage {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p(int i) const { return s().players[i]; }
};

void put(sv::SequelPlayer& p, sh::Vec3f at) {
  p.move.origin[0] = static_cast<int16_t>(at.x * 8);
  p.move.origin[1] = static_cast<int16_t>(at.y * 8);
  p.move.origin[2] = static_cast<int16_t>(at.z * 8);
  for (int16_t& v : p.move.velocity) v = 0;
  for (int16_t& v : p.move.delta_angles) v = 0;  // the view his commands give, not a spawn's turn
  p.forced = false;
}

void tick(Stage& d, uint8_t buttons, float pitch, float yaw) {
  sh::Packet packets[2];
  for (int c = 0; c < 2; c++) {
    packets[c].client = c;
    packets[c].sequence = ++d.sequence;
    sh::MoveCmd cmd;
    cmd.msec = 13;
    if (c == 0) {
      cmd.buttons = buttons;
      cmd.angles = {pitch, yaw, 0};
    }
    packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
  }
  d.r.frame(packets, 2);
}

std::unique_ptr<Stage> stage(const std::shared_ptr<const br::Map>& map, const std::string& name,
                             int level, std::string& error, bool midair = true) {
  auto d = std::make_unique<Stage>();
  d->r = sv::sequel_runner(sv::make_sequel_server({}));
  d->r.console("deathmatch 1");
  d->r.console(midair ? "k_midair 1" : "k_midair 0");
  d->r.console("k_midair_minheight " + std::to_string(level));
  sv::MapSpec spec;
  spec.name = name;
  spec.brushes = map;
  if (!d->r.start(spec, error)) return nullptr;
  d->r.connect(0, "Ana", false, false);
  d->r.connect(1, "Ben", false, false);
  if (!midair) {  // the launcher in hand as midair gives it
    sv::SqBody& body = d->p(0).body;
    body.counts[sv::sq_item("weapon_rocketlauncher")] = 1;
    body.ammo[static_cast<int>(sv::SqAmmo::rockets)] = 50;
    d->r.client_command(0, "use Rocket Launcher");
  }
  for (int i = 0; i < 160; i++) tick(*d, 0, 0, 0);
  return d;
}

constexpr int32_t kWorld = sh::kSqSolid | sh::kSqWindow;
constexpr float kDist = 200;  // the target's distance ahead of the shooter

// Where the shooter stands and which way he fires (one of the four axes, so a slice of the map
// shows the shot): of the map's deathmatch spawns and the four ways, the one with the most room
// above the spot `kDist` ahead, a clear line to it at eye height and a floor there level with his.
struct Spot {
  sh::Vec3f from;
  int way = 0;        // 0 +x, 1 +y, 2 −x, 3 −y
  float ceiling = 0;  // the room above the floor at the target's spot, units
  sh::Vec3f dir() const {
    const float d[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    return {d[way][0], d[way][1], 0};
  }
  float yaw() const { return 90.0f * float(way); }
};

Spot find_spot(const Stage& d) {
  const sv::SequelServer& s = d.s();
  Spot best;
  best.from = {0, 0, 34};
  best.ceiling = -1;
  for (const sv::SequelEntity& e : s.entities) {
    if (e.classname != "info_player_deathmatch") continue;
    // The floor under the spawn point, and the player's origin 24 above it.
    const sh::Vec3f top = e.origin + sh::Vec3f{0, 0, 16};
    const sh::SequelTrace down = sv::map_trace(s, top, {}, {}, top - sh::Vec3f{0, 0, 256}, kWorld);
    if (down.fraction >= 1) continue;
    const sh::Vec3f from = down.end + sh::Vec3f{0, 0, 24};
    for (int way = 0; way < 4; way++) {
      Spot c{from, way, 0};
      const sh::Vec3f eye = from + sh::Vec3f{0, 0, 22};
      const sh::Vec3f ahead = from + c.dir() * kDist;
      if (sv::map_trace(s, eye, {}, {}, ahead + sh::Vec3f{0, 0, 22}, kWorld).fraction < 1) continue;
      const sh::SequelTrace fl =
          sv::map_trace(s, ahead, {}, {}, ahead - sh::Vec3f{0, 0, 64}, kWorld);
      if (fl.fraction >= 1 || fl.end.z < from.z - 26 || fl.end.z > from.z - 22) continue;
      const sh::SequelTrace up =
          sv::map_trace(s, ahead, {}, {}, ahead + sh::Vec3f{0, 0, 2048}, kWorld);
      c.ceiling = up.end.z - fl.end.z;
      if (c.ceiling > best.ceiling) best = c;
    }
  }
  if (best.ceiling < 0) best.ceiling = 2048;
  return best;
}

struct Shot {
  float height = 0;  // as the rules measured it: his feet over the floor under him
  bool killed = false;
  float frags = 0;
  float up_speed = 0;   // the target's speed upward just after the blow
  bool landed = false;  // the blow landed (his armour wore, or he died)
  std::string line;
  sh::Vec3f at;
};

// One rocket from the floor at a target held `height` above the floor ahead (`feet`: at the floor
// 40 units in front of a standing target).
Shot fire_at(Stage& d, const Spot& sp, float height, bool feet) {
  Shot out;
  sv::SequelPlayer& shooter = d.p(0);
  sv::SequelPlayer& target = d.p(1);
  out.at = sp.from + sp.dir() * kDist + sh::Vec3f{0, 0, height};
  out.height = height;
  // His armour whole again (its wear shows the blow); without midair, his health too.
  if (sv::fight_midair(d.s())) {
    sv::fight_midair_kit(d.s(), 1);
  } else {
    target.health = 100;
    target.body.armour = sv::SqArmour::body;
    target.body.armour_count = 100;
  }
  put(shooter, sp.from);
  put(target, out.at);
  // The rocket leaves at the eye (22 up), aimed at the target's middle or at the floor.
  const float dz = feet ? -(22.0f + 24.0f) : height + 4 - 22.0f;
  const float run = feet ? kDist - 40 : kDist;
  const float pitch =
      -static_cast<float>(ge::core::libm::atan2(dz, run) * 180.0 / 3.14159265358979323846);
  const size_t lines = d.s().obituaries.size();
  const float frags = shooter.frags;
  for (int i = 0; i < 80; i++) {  // the launcher ready again after the last shot
    put(shooter, sp.from);
    put(target, out.at);
    tick(d, 0, pitch, sp.yaw());
  }
  tick(d, sh::kButtonAttack, pitch, sp.yaw());
  const int armour = target.body.armour_count;
  for (int i = 0; i < 160 && target.alive; i++) {
    if (target.body.armour_count != armour) {
      out.landed = true;
      out.up_speed = target.move.velocity[2] / 8.0f;
      break;
    }
    put(target, out.at);  // held there until the blow lands
    tick(d, 0, pitch, sp.yaw());
  }
  out.killed = !target.alive;
  out.landed = out.landed || out.killed;
  out.frags = shooter.frags - frags;
  if (d.s().obituaries.size() > lines) out.line = d.s().obituaries.back();
  for (int i = 0; i < 300 && !target.alive; i++) tick(d, 0, 0, sp.yaw());  // back by himself
  return out;
}

// The shots on a slice of the map through the shooter's spot along the way he fires: solid grey,
// open dark, the shooter, each target coloured by what the hit did and the rocket's line to it.
gl::Picture slice_picture(const Stage& d, const Spot& sp, const std::vector<Shot>& shots,
                          int level) {
  const sh::Vec3f eye = sp.from + sh::Vec3f{0, 0, 22};
  const bool along_x = sp.way % 2 == 0;
  const float sign = sp.way < 2 ? 1.0f : -1.0f;
  br::Slice slice;
  std::string error;
  const double step = 8;
  const bool ok = br::slice_map(*d.s().map, along_x ? 1 : 0, along_x ? sp.from.y : sp.from.x, step,
                                &slice, &error, 1200);
  // The window: from 160 behind the shooter to 200 past the target, the floor to the ceiling.
  const double u_from = (along_x ? sp.from.x : sp.from.y) - sign * 160;
  const double u_to = (along_x ? sp.from.x : sp.from.y) + sign * (kDist + 200);
  const double lo_u = std::min(u_from, u_to), hi_u = std::max(u_from, u_to);
  const double lo_z = sp.from.z - 24 - 64;
  const double hi_z = sp.from.z - 24 + std::min(sp.ceiling, 1300.0f) + 64;
  const double scale = 0.6;
  const int w = static_cast<int>((hi_u - lo_u) * scale) + 40;
  const int h = static_cast<int>((hi_z - lo_z) * scale) + 60;
  gl::Picture p = gl::create_picture(w, h, {24, 26, 32});
  // Map units to pixels, the shooter's way to the right.
  const auto px = [&](double u) { return 20 + (sign > 0 ? (u - lo_u) : (hi_u - u)) * scale; };
  const auto py = [&](double z) { return 40 + (hi_z - z) * scale; };
  if (ok) {
    for (size_t r = 0; r < slice.rows.size(); r++) {
      const double v = slice.v0 - double(r) * slice.step;
      for (size_t c = 0; c < slice.rows[r].size(); c++) {
        const double u = slice.u0 + double(c) * slice.step;
        if (u < lo_u - step || u > hi_u + step || v < lo_z - step || v > hi_z + step) continue;
        const char k = slice.rows[r][c];
        gl::Rgba col{24, 26, 32};
        if (k == '#') col = {120, 116, 108};
        if (k == '^') col = {60, 70, 110};
        if (k == '~' || k == '%' || k == '!') col = {40, 80, 140};
        if (k == '.') continue;
        const double x0 = std::min(px(u - step / 2), px(u + step / 2));
        gl::fill_rect(p, x0, py(v + step / 2), step * scale + 1, step * scale + 1, col);
      }
    }
  }
  const double eu = along_x ? eye.x : eye.y;
  gl::draw_text(p, 8, 8,
                "the sequel's rules, k_midair_minheight " + std::to_string(level) + " (" +
                    sh::midair_level_name(level) + ")",
                {230, 230, 230}, 1);
  gl::draw_text(p, 8, 20, "green: killed (frags) / amber: hit, no harm / grey: missed",
                {170, 170, 170}, 1);
  const double floor_y = py(sp.from.z - 24);
  const double min_y = py(sp.from.z - 24 + sh::midair_min_height(level));
  for (double x = 20; x < w - 20; x += 8)
    gl::draw_line(p, x, min_y, x + 4, min_y, {200, 80, 80}, 1);
  gl::draw_text(p, 24, min_y - 12, "least height", {200, 80, 80}, 1);
  (void)floor_y;
  for (const Shot& s : shots) {
    const double tu = along_x ? s.at.x : s.at.y;
    const gl::Rgba col = s.killed ? gl::Rgba{90, 210, 110}
                                  : (s.landed ? gl::Rgba{230, 170, 60} : gl::Rgba{140, 140, 140});
    gl::draw_line(p, px(eu), py(eye.z), px(tu), py(s.at.z + 4), {200, 120, 60, 160}, 1);
    gl::fill_rect(p, px(tu) - 16 * scale, py(s.at.z + 32), 32 * scale, 56 * scale, col);
    std::string tag = to_fixed(s.height, 0) + "u";
    if (s.killed) tag += "  +" + to_fixed(s.frags, 0);
    gl::draw_text(p, px(tu) + 14, py(s.at.z + 10), tag, col, 1);
  }
  gl::fill_rect(p, px(eu) - 16 * scale, py(sp.from.z + 32), 32 * scale, 56 * scale, {90, 160, 230});
  gl::draw_text(p, px(eu) - 30, py(sp.from.z - 24) + 6, "shooter", {90, 160, 230}, 1);
  return p;
}

void sequel(gl::Lab& lab) {
  std::string error, name;
  const std::shared_ptr<const br::Map> map = sequel_map(lab, name, error);
  if (map == nullptr) {
    lab.fail("se-midair: the sequel's map: " + error,
             "Pass --sequel-basedir=<the folder holding the sequel's game folder> and "
             "--sequel-map=<a map in it>, or leave both out for the hall. Skill: brush.");
    return;
  }
  const int level = static_cast<int>(lab.arg_num("minheight"));
  std::unique_ptr<Stage> d = stage(map, name, level, error);
  if (d == nullptr) {
    lab.fail("se-midair: the sequel's runner did not start: " + error,
             "Read the reason above. Skill: shooter.");
    return;
  }
  const Spot sp = find_spot(*d);
  gl::LabTable table{
      "the sequel (" + name +
          "): one rocket from the floor at a target held "
          "still above the floor " +
          to_fixed(kDist, 0) + " ahead, k_midair_minheight " + std::to_string(level),
      {"target's height", "counts", "frags", "his speed up after (units/s)", "what all read"},
      {}};
  std::vector<Shot> shots;
  int counted = 0;
  float frag_sum = 0;
  for (const double h : lab.arg_nums("heights")) {
    if (h > sp.ceiling - 64) continue;  // no room under this map's ceiling
    const Shot s = fire_at(*d, sp, float(h), false);
    shots.push_back(s);
    counted += s.killed ? 1 : 0;
    frag_sum += s.frags;
    table.rows.push_back({to_fixed(h, 0), s.killed ? "yes" : (s.landed ? "no" : "missed"),
                          to_fixed(s.frags, 0), to_fixed(s.up_speed, 0),
                          s.line.empty() ? "-" : s.line});
    lab.metric("sequel.h" + to_fixed(h, 0) + ".frags", s.frags, "count");
  }
  const Shot feet = fire_at(*d, sp, 0, true);
  table.rows.push_back({"0, at his feet", feet.killed ? "yes" : (feet.landed ? "no" : "missed"),
                        to_fixed(feet.frags, 0), to_fixed(feet.up_speed, 0),
                        feet.line.empty() ? "-" : feet.line});
  std::unique_ptr<Stage> plain = stage(map, name, level, error, false);
  if (plain != nullptr) {
    const Shot off = fire_at(*plain, sp, 0, true);
    table.rows.push_back({"0, at his feet, midair off", "-", to_fixed(off.frags, 0),
                          to_fixed(off.up_speed, 0), off.line.empty() ? "-" : off.line});
    lab.metric("sequel.feet-lift-off", off.up_speed, "units-per-s");
  }
  lab.table(table);
  lab.metric("sequel.counted", counted, "count");
  lab.metric("sequel.frags", frag_sum, "count");
  lab.metric("sequel.feet-lift", feet.up_speed, "units-per-s");
  lab.metric("sequel.room", sp.ceiling, "units");
  lab.shot({"sequel-shots",
            "MIDAIR ON " + name + ": ONE ROCKET AT EACH HEIGHT",
            "A slice of the map through the shooter's spot along the way he fires: each target "
            "held at its height above the floor, the rocket's line to it, and what the hit did.",
            "below the least height a hit only wears armour; above it the target dies and the "
            "shooter scores 1, 2, 4 or 8 by the rise",
            {{"map", name}, {"minheight", std::to_string(level)}}},
           slice_picture(*d, sp, shots, level));
}

void run(gl::Lab& lab) {
  rules(lab);
  sequel(lab);
}

const gl::LabSpec kSpec{
    "se-midair",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "midair on the shared rules (plugins/shooter/midair.hpp), played by the sequel's runner "
    "(server/sequel_fight.cpp)",
    "shooter",
    "Does a rocket count only on a player in the air, score by the rise, and throw as the rules "
    "say?",
    {
        {"heights", "0,100,130,300,600,1100", "the sequel: the target's heights above the floor"},
        {"minheight", "1", "k_midair_minheight: the least height, 0..4 (64..1024 units)"},
        {"sequel-basedir", "", "a folder holding the sequel's game folder (empty: a hall)"},
        {"sequel-game", "baseq2", "the sequel's game folder under --sequel-basedir"},
        {"sequel-map", "q2dm1", "with --sequel-basedir: the sequel's map"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
