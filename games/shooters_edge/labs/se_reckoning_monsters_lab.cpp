// LAB `se-reckoning-monsters`: THE RECKONING'S MONSTERS ON THE QUAKE II RUNNER, under the pack's
// switch (server/q2_reckoning_monsters.cpp, the pack's rows in q2_monster_kinds.hpp), in a hall
// built here (no game files):
//
// - **Their attacks**: each of the pack's fighting kinds faces a sturdy, still player 300 units
//   away for `--seconds`: the damage it deals over time (a chart, a line a kind), when it first
//   hurts him, and what it fired (discs, blue bolts, spit, phalanx rounds, heat rockets, beams).
// - **The shields**: a blow of 30 every half second on the beta gladiator, the beta super tank and
//   the base gladiator: health and shield points over time.
// - **The repair bot**: let loose with a repair object and a dead guard in the hall: its path from
//   above, when the object was welded through and when the guard rose.
//
// With `--pak=<the pack's pak0.pak>` (the player's own copy, for PRIVATE pictures only) every map
// in it is also started under the pack and run for `--seconds` with the player at the start: each
// of the pack's kinds placed, how many spawned and how many are still standing, and the first
// map's floors from above with its monsters marked.
//
// ge:summary Lab: The Reckoning's monsters on the runner: attacks, shields, the repair bot, maps
// ge:keywords lab se-reckoning-monsters q2 reckoning mission-pack xatrix gekk fixbot gladb
// ge:keywords power-shield laser-guard monster-damage overhead pack-maps
// ge:skill shooter labs
// ge:test tests/shooters_edge_q2_reckoning_monsters_test.cpp
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace ro = shooters_edge::server::roster;

using Pairs = std::vector<std::pair<std::string, std::string>>;
constexpr float kSturdy = 100000;  // the player's health: never killed, so every blow counts
constexpr int kSecond = 77;        // host ticks of 13 ms

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things, std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  for (const Pairs& t : things) w.entities.push_back(entity(t));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Run {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

std::unique_ptr<Run> start(const std::shared_ptr<const br::Map>& map, const std::string& name,
                           std::string& error) {
  auto run = std::make_unique<Run>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::reckoning;
  run->r = sv::q2_runner(sv::make_q2_server(o));
  run->r.console("deathmatch 0");
  run->r.console("skill 1");
  sv::MapSpec spec;
  spec.name = name;
  spec.brushes = map;
  if (!run->r.start(spec, error)) return nullptr;
  run->r.connect(0, "Ana", false, false);
  return run;
}

void tick(Run& run) {
  sh::Packet pk;
  pk.client = 0;
  pk.sequence = ++run.sequence;
  sh::MoveCmd c;
  c.msec = 13;
  pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
  run.r.frame(&pk, 1);
}

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

struct Fighter {
  const char* classname;
  const char* label;
};
constexpr Fighter kFighters[] = {
    {"monster_soldier_ripper", "ripper guard"},  {"monster_soldier_hypergun", "hyper guard"},
    {"monster_soldier_lasergun", "laser guard"}, {"monster_gekk", "gekk"},
    {"monster_gladb", "beta gladiator"},         {"monster_chick_heat", "beta iron maiden"},
    {"monster_boss5", "beta super tank"},        {"monster_brain", "brain (the pack's)"},
    {"monster_fixbot", "repair bot (no flags)"},
};

const char* shot_name(sv::SqShot k) {
  switch (k) {
    case sv::SqShot::ripper:
      return "discs";
    case sv::SqShot::blue_bolt:
      return "blue bolts";
    case sv::SqShot::plasma:
      return "phalanx rounds";
    case sv::SqShot::heat_rocket:
      return "heat rockets";
    case sv::SqShot::spit:
      return "spit";
    case sv::SqShot::bolt:
      return "bolts";
    case sv::SqShot::rocket:
      return "rockets";
    case sv::SqShot::grenade:
      return "grenades";
    default:
      return "other";
  }
}

void attacks(gl::Lab& lab, double seconds) {
  gl::LabTable table{"each kind facing a still player 300 units away, skill 1",
                     {"kind", "health", "shield", "first hurt (s)", "damage", "fired"},
                     {}};
  // Two charts, so no two kinds share a colour: the guards, the gekk and the repair bot; the big
  // ones.
  gl::LineChart small, big;
  small.spikes = big.spikes = false;
  small.frame = {"THE RECKONING'S GUARDS, GEKK AND REPAIR BOT: DAMAGE DEALT",
                 "a still player 300 units away, under the pack's switch", "t (s)", "damage dealt"};
  big.frame = {"THE RECKONING'S BETA MONSTERS AND BRAIN: DAMAGE DEALT",
               "a still player 300 units away, under the pack's switch", "t (s)", "damage dealt"};
  const int ticks = static_cast<int>(seconds * kSecond);
  for (const Fighter& f : kFighters) {
    std::string error;
    const auto map =
        hall({{{"classname", f.classname}, {"origin", "0 0 40"}, {"angle", "180"}}}, error);
    std::unique_ptr<Run> run = map != nullptr ? start(map, "hall", error) : nullptr;
    if (run == nullptr) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    tick(*run);
    sv::Q2Server& s = run->s();
    if (s.monsters.empty()) {
      lab.fail(std::string(f.classname) + " did not spawn under the pack",
               "Read kind_named in server/q2_monster_kinds.hpp. Skill: shooter.");
      return;
    }
    const sv::Q2Monster& m = s.monsters[0];
    const sv::Q2Entity& e = s.entities[size_t(m.entity)];
    const int health = e.health, shield = m.shield;
    s.players[0].health = kSturdy;
    gl::Trace trace{f.label, {}, {}};
    double first = -1;
    int fired[16] = {};
    // A row's launch time marks each shot, even one that hit within its first frame.
    std::vector<double> seen(std::size(s.missiles), -1);
    int beams = 0;
    bool beam_on = false;
    for (int i = 0; i < ticks; i++) {
      tick(*run);
      const double dealt = kSturdy - s.players[0].health;
      if (first < 0 && dealt > 0) first = (i + 1) * 0.013;
      for (size_t k = 0; k < std::size(s.missiles); k++) {
        const sv::Q2Missile& q = s.missiles[k];
        if (q.from == m.entity && q.dies_at != seen[k] && q.dies_at > 0)
          fired[static_cast<int>(q.kind) & 15]++;
        seen[k] = q.dies_at;
      }
      const bool on = m.beam >= 0 && s.level_time < m.beam_until;
      if (on && !beam_on) beams++;
      beam_on = on;
      if (i % 8 == 0) {
        trace.x.push_back((i + 1) * 0.013);
        trace.y.push_back(dealt);
      }
    }
    const double dealt = kSturdy - s.players[0].health;
    std::string what;
    for (int k = 0; k < 16; k++)
      if (fired[k] > 0)
        what += (what.empty() ? "" : ", ") + std::to_string(fired[k]) + " " +
                shot_name(static_cast<sv::SqShot>(k));
    if (beams > 0) what += (what.empty() ? "" : ", ") + std::to_string(beams) + " beam bursts";
    if (what.empty()) what = "blows";
    const std::string key = std::string("kind.") + f.classname;
    lab.metric(key + ".damage", dealt, "points");
    lab.metric(key + ".first-hurt", first, "s");
    table.rows.push_back({f.label, std::to_string(health), std::to_string(shield),
                          first < 0 ? "none" : fixed(first, 2), fixed(dealt, 0), what});
    const std::string_view c = f.classname;
    const bool large = c == "monster_gladb" || c == "monster_chick_heat" || c == "monster_boss5" ||
                       c == "monster_brain";
    (large ? big : small).traces.push_back(std::move(trace));
  }
  lab.table(table);
  lab.shot(
      {"attacks",
       small.frame.title,
       "The Reckoning's guards, the gekk and the repair bot (no spawn flags), each 300 units "
       "from a still player at skill 1, under the pack's switch: the damage dealt so far.",
       "the ripper and hyperblaster guards' bursts of three in steps; the laser guard's beam a "
       "point a frame; the gekk's claws once it has leapt in; the repair bot fighting like any "
       "monster once it sees her",
       {{"seconds", fixed(seconds, 0)}}},
      gl::to_picture(gl::line_chart(small).plot));
  lab.shot({"attacks-big",
            big.frame.title,
            "The Reckoning's beta gladiator, beta iron maiden, beta super tank and its brain, each "
            "300 units from a still player at skill 1: the damage dealt so far.",
            "the beta gladiator's phalanx rounds 100 at a time; the heat rockets; the super tank's "
            "rockets 50 each; the brain's eyes a point a frame",
            {{"seconds", fixed(seconds, 0)}}},
           gl::to_picture(gl::line_chart(big).plot));
}

void shields(gl::Lab& lab) {
  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"POWER SHIELDS UNDER A BLOW OF 30 EVERY HALF SECOND",
                 "two thirds taken, two damage a point, from every side", "t (s)", "points"};
  const Fighter kinds[] = {{"monster_gladb", "beta gladiator"},
                           {"monster_boss5", "beta super tank"},
                           {"monster_gladiator", "gladiator (no shield)"}};
  for (const Fighter& f : kinds) {
    std::string error;
    // Far off and facing away: no one to fight, only the blows.
    const auto map =
        hall({{{"classname", f.classname}, {"origin", "900 0 40"}, {"angle", "0"}}}, error);
    std::unique_ptr<Run> run = map != nullptr ? start(map, "hall", error) : nullptr;
    if (run == nullptr) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    tick(*run);
    sv::Q2Server& s = run->s();
    const sv::Q2Monster& m = s.monsters[0];
    const sv::Q2Entity& e = s.entities[size_t(m.entity)];
    const int full = e.health;
    gl::Trace lost{std::string(f.label) + ": health lost", {}, {}};
    gl::Trace points{std::string(f.label) + ": shield", {}, {}};
    for (int i = 0; i < 60; i++) {
      sv::monster_damage(s, m.entity, -1, 30, {-1, 0, 0}, true);
      for (int t = 0; t < kSecond / 2; t++) tick(*run);
      lost.x.push_back(i * 0.5);
      lost.y.push_back(full - e.health);
      if (m.shield > 0 || i == 0 || points.y.back() > 0) {
        points.x.push_back(i * 0.5);
        points.y.push_back(m.shield);
      }
    }
    lab.metric(std::string("shield.") + f.classname + ".lost", full - e.health, "points");
    chart.traces.push_back(std::move(lost));
    if (std::string(f.classname) != "monster_gladiator") chart.traces.push_back(std::move(points));
  }
  lab.shot(
      {"shields",
       chart.frame.title,
       "A blow of 30 every half second on the beta gladiator and the beta super tank (power "
       "shields of 400) beside the base gladiator: the health lost and the shield's points.",
       "the shielded kinds losing 10 a blow while the shield lasts (it pays 10 points for the 20 "
       "it covers, so 400 lasts 40 blows), then 30 a blow like the gladiator",
       {}},
      gl::to_picture(gl::line_chart(chart).plot));
}

void repair_bot(gl::Lab& lab, double seconds) {
  std::string error;
  const auto map =
      hall({{{"classname", "monster_fixbot"},
             {"origin", "-200 150 60"},
             {"angle", "0"},
             {"spawnflags", "4"}},
            {{"classname", "func_object_repair"}, {"origin", "500 -150 40"}},
            {{"classname", "monster_soldier"}, {"origin", "200 100 40"}, {"angle", "90"}}},
           error);
  std::unique_ptr<Run> run = map != nullptr ? start(map, "hall", error) : nullptr;
  if (run == nullptr) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  tick(*run);
  sv::Q2Server& s = run->s();
  int object = -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].classname == "object_repair") object = static_cast<int>(i);
  const sv::Q2Monster& bot = s.monsters[0];
  sv::map_damage(s, s.monsters[1].entity, 0, 25);  // the guard: 50, dead at -20
  gl::Trace path{"the repair bot's path", {}, {}};
  double welded = -1, rose = -1;
  const int ticks = static_cast<int>(std::max(seconds, 30.0) * kSecond);
  for (int i = 0; i < ticks; i++) {
    tick(*run);
    const sv::Q2Entity& e = s.entities[size_t(bot.entity)];
    if (i % 8 == 0) {
      path.x.push_back(e.origin.x);
      path.y.push_back(e.origin.y);
    }
    if (welded < 0 && object >= 0 && s.entities[size_t(object)].health < 0) welded = i * 0.013;
    if (rose < 0 && !s.monsters[1].dead) rose = i * 0.013;
  }
  lab.metric("repair-bot.welded", welded, "s");
  lab.metric("repair-bot.raised", rose, "s");
  // The repair object and the guard as small squares.
  const auto square = [](std::string name, double x, double y) {
    return gl::Trace{
        std::move(name), {x - 8, x + 8, x + 8, x - 8, x - 8}, {y - 8, y - 8, y + 8, y + 8, y - 8}};
  };
  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"THE REPAIR BOT FROM ABOVE", "spawn flag 4: it roams, welds and raises the dead",
                 "x", "y"};
  chart.traces = {path, square("the repair object", 500, -150), square("the dead guard", 200, 100)};
  lab.shot({"repair-bot",
            chart.frame.title,
            "The repair bot let loose (spawn flag 4) in the hall with a repair object and a dead "
            "guard: its path from above.",
            "first to the corpse it sees (raised with its blue beam), then the welding",
            {{"raised (s)", rose < 0 ? "never" : fixed(rose, 1)},
             {"welded through (s)", welded < 0 ? "never" : fixed(welded, 1)}}},
           gl::to_picture(gl::line_chart(chart).plot));
  if (rose < 0)
    lab.fail("the repair bot never raised the guard",
             "Read tend in q2_monster_medic.cpp. Skill: shooter.");
  if (welded < 0)
    lab.fail("the repair bot never welded the object through",
             "Read work in q2_reckoning_monsters.cpp. Skill: shooter.");
}

// The map's floors from above, its monsters of the pack's kinds marked.
gl::Picture overhead(const br::Map& map, const sv::Q2Server& s) {
  constexpr int kSize = 560, kPad = 24;
  gl::Picture p = gl::create_picture(kSize, kSize, {18, 18, 22});
  if (map.models.empty()) return p;
  float lo[3], hi[3];
  for (int k = 0; k < 3; k++) {
    lo[k] = map.models[0].mins[k];
    hi[k] = map.models[0].maxs[k];
  }
  const float span = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0f});
  const auto px = [&](float x, float y) {
    return std::pair<int, int>{static_cast<int>(kPad + (x - lo[0]) / span * (kSize - 2 * kPad)),
                               static_cast<int>(kPad + (hi[1] - y) / span * (kSize - 2 * kPad))};
  };
  for (const br::Surface& f : map.surfaces) {
    if (f.kind == br::SurfaceKind::sky) continue;
    for (uint32_t t = 0; t + 2 < f.count; t += 3) {
      const br::Vertex& v = map.vertices[map.indices[f.first + t]];
      if (v.nrm[2] < 0.7f) continue;
      for (int k = 0; k < 3; k++) {
        const br::Vertex& c = map.vertices[map.indices[f.first + t + uint32_t(k)]];
        const auto [x, y] = px(c.pos[0], c.pos[1]);
        gl::fill_rect(p, x, y, 2, 2, {70, 74, 84, 255});
      }
    }
  }
  for (const sv::Q2Monster& m : s.monsters) {
    const ro::Kind& k = ro::kKinds[m.kind];
    if (k.pack != sv::Q2Pack::reckoning) continue;
    const sv::Q2Entity& e = s.entities[size_t(m.entity)];
    const auto [x, y] = px(e.origin.x, e.origin.y);
    const gl::Rgba c = m.dead        ? gl::Rgba{120, 120, 120}
                       : k.amphibian ? gl::Rgba{90, 220, 90}
                       : k.repairs   ? gl::Rgba{90, 160, 255}
                       : k.like != nullptr && std::string(k.like).starts_with("monster_soldier")
                           ? gl::Rgba{255, 200, 60}
                           : gl::Rgba{255, 80, 60};
    gl::fill_rect(p, x - 3, y - 3, 7, 7, c);
  }
  return p;
}

void pack_maps(gl::Lab& lab, const std::string& path, double seconds) {
  std::vector<uint8_t> bytes;
  std::string error;
  ge::vfs::Pak pak;
  {
    std::ifstream in(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (bytes.empty()) error = "nothing read from " + path;
  }
  if (bytes.empty() || !ge::vfs::read_pak(bytes, &pak, &error)) {
    lab.fail("the pack's pak did not open: " + error,
             "Pass --pak=<the pack's folder>/pak0.pak. Skill: vfs.");
    return;
  }
  gl::LabTable table{"the pack's maps under its switch, run with the player at the start",
                     {"map", "the pack's monsters placed for skill 1", "spawned", "standing after"},
                     {}};
  bool drawn = false;
  for (const ge::vfs::PakEntry& f : pak.entries) {
    if (!f.path.starts_with("maps/") || !f.path.ends_with(".bsp")) continue;
    const std::string name = f.path.substr(5, f.path.size() - 9);
    auto map = std::make_shared<br::Map>();
    if (!br::read_bsp(ge::vfs::pak_file(bytes, f), map.get(), &error)) continue;
    int placed = 0;
    for (const br::Entity& e : map->entities) {
      const int k = ro::kind_named(e.get_any_case("classname"), sv::Q2Pack::reckoning);
      // Those left out at skill 1 (spawn flag 512, not on medium) are not counted.
      const int flags = std::atoi(e.get_any_case("spawnflags").c_str());
      if (k >= 0 && ro::kKinds[k].pack == sv::Q2Pack::reckoning && (flags & 512) == 0) placed++;
    }
    std::unique_ptr<Run> run = start(map, name, error);
    if (run == nullptr) continue;
    int spawned = 0;
    for (const sv::Q2Monster& m : run->s().monsters)
      if (ro::kKinds[m.kind].pack == sv::Q2Pack::reckoning) spawned++;
    for (int i = 0; i < static_cast<int>(seconds * kSecond); i++) {
      run->s().players[0].health = kSturdy;
      tick(*run);
    }
    int standing = 0;
    for (const sv::Q2Monster& m : run->s().monsters)
      if (ro::kKinds[m.kind].pack == sv::Q2Pack::reckoning && !m.dead) standing++;
    table.rows.push_back(
        {name, std::to_string(placed), std::to_string(spawned), std::to_string(standing)});
    if (!drawn && placed > 0) {
      drawn = true;
      lab.shot({"pack-map",
                "THE RECKONING'S MAP " + name + " FROM ABOVE",
                "Its floors from above after " + fixed(seconds, 0) +
                    " s under the pack's switch, its own monsters marked.",
                "guards amber, gekks green, repair bots blue, the betas red, the dead grey",
                {{"map", name}}},
               overhead(*map, run->s()));
    }
  }
  lab.table(table);
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  attacks(lab, seconds);
  shields(lab);
  repair_bot(lab, seconds);
  const std::string pak = lab.arg("pak");
  if (!pak.empty()) pack_maps(lab, pak, seconds);
}

const gl::LabSpec kSpec{
    "se-reckoning-monsters",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "The Reckoning's monsters on Quake II's runner under the pack's switch "
    "(server/q2_reckoning_monsters.cpp)",
    "shooter",
    "Does each of The Reckoning's monsters attack, take blows and do its work as the pack's notes "
    "say, and do the pack's maps play with them?",
    {{"seconds", "10", "seconds each kind faces the player (and each pack map runs)"},
     {"pak", "", "the pack's pak0.pak: its maps run too (private pictures)"}},
    "--seconds=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
