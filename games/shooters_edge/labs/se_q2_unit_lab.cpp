// LAB `se-q2-unit`: A UNIT OF QUAKE II'S CAMPAIGN PLAYED START TO END BY A SCRIPT
// (bots/q2_unit.hpp). A fighter plays Quake II's single-player game on its own rules from the
// unit's first map, finding each map's way out off its entities alone (the exit, else the button,
// key or trigger that opens the way, else a place not seen yet), through every level change of the
// unit until an exit leads out of it. God mode is on: the run is about the way through.
//
// THE TABLE, a map a row: how long he took and where its exit led; the keys on the map and the ones
// he took; the objectives and secrets the level counts and those found; the help messages; the
// monsters and those killed; the plan's steps done. THE PICTURES, a map each: its floors from above
// with his track in yellow, the keys (green, ringed red once taken) and the exits (blue).
//
// Without `--game` the unit is our own: three floored halls of brushes in a pack, so CI needs no
// game data and the run is a digest. The first is split by a wall whose door only opens for the
// blue key lying in the west; the second's door opens from a button and an objective lies past it;
// the third has a secret and the exit out of the unit. With `--game=<folder>` (the folder holding
// Quake II's base folder) the unit comes from its packs (`--first`, the full game's first map by
// default; `--first=base2$base1` starts on base2 as if arrived from base1), for PRIVATE pictures
// only. Built on the release preset a map's run takes seconds, not the debug build's minutes.
//
// ge:summary Lab: a unit of Quake II played start to end by a script: exits, keys, objectives
// ge:keywords lab se-q2-unit q2 unit play-through scripted-playthrough run_q2_unit
// ge:keywords exit key trigger_key objective target_goal secret target_help way-through
// ge:skill shooter labs bots
// ge:test tests/shooters_edge_q2_unit_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classic/q2_move.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/bots/q2_unit.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace bt = shooters_edge::bots;

using Pairs = std::vector<std::pair<std::string, std::string>>;

// A hall 2048 by 1024 (x -1024..1024, y -512..512) split at x 0 by a wall with a doorway, the
// doorway shut by a door `door` names (none: left open), and `extra` entities; `extra_models` brush
// boxes, each an entity's model `*k` in order after the door's.
struct Hall {
  br::BrushWorld world;
  int models = 1;
  Hall() {
    world.models.resize(1);
    box(0, -1024, -512, -16, 1024, 512, 0);
    box(0, -1040, -512, -16, -1024, 512, 256);
    box(0, 1024, -512, -16, 1040, 512, 256);
    box(0, -1040, 512, -16, 1040, 528, 256);
    box(0, -1040, -528, -16, 1040, -512, 256);
    box(0, -8, -512, 0, 8, -64, 256);  // the dividing wall, the doorway at y -64..64
    box(0, -8, 64, 0, 8, 512, 256);
    box(0, -8, -64, 128, 8, 64, 256);  // above the doorway
  }
  void box(int model, float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[static_cast<size_t>(model)].push_back(
        br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  }
  // A brush entity: a box as model `*k`, its pairs.
  void brush(float x0, float y0, float z0, float x1, float y1, float z1, Pairs pairs) {
    world.models.emplace_back();
    box(models, x0, y0, z0, x1, y1, z1);
    pairs.emplace_back("model", "*" + std::to_string(models++));
    put(std::move(pairs));
  }
  void put(Pairs pairs) {
    br::Entity e;
    e.pairs = std::move(pairs);
    world.entities.push_back(e);
  }
  bool write(std::vector<uint8_t>& out, std::string& error) {
    return br::write_brush_world(world, &out, &error);
  }
};

void start(Hall& h) {
  h.put({{"classname", "info_player_start"}, {"origin", "-800 0 24"}, {"angle", "0"}});
}

// The doorway's door, named `name`, sliding up out of the way.
void door(Hall& h, const char* name) {
  h.brush(-8, -64, 0, 8, 64, 128,
          {{"classname", "func_door"}, {"targetname", name}, {"angle", "-1"}, {"wait", "-1"}});
}

// A trigger box at x0..x1 across the hall's width firing `target` once.
void trigger(Hall& h, float x0, float x1, const char* target) {
  h.brush(x0, -512, 0, x1, 512, 96, {{"classname", "trigger_once"}, {"target", target}});
}

void exit_to(Hall& h, const char* map) {
  h.brush(900, -96, 0, 1000, 96, 96, {{"classname", "trigger_multiple"}, {"target", "out"}});
  h.put({{"classname", "target_changelevel"}, {"targetname", "out"}, {"map", map}});
}

bool unit(std::vector<uint8_t>& pak, std::string& error) {
  std::vector<uint8_t> a, b, c;
  {  // the first: the blue key in the west, the door its lock opens
    Hall h;
    start(h);
    h.put({{"classname", "key_blue_key"}, {"origin", "-600 400 24"}});
    door(h, "gate");
    h.brush(-120, -64, 0, -40, 64, 96, {{"classname", "trigger_multiple"}, {"target", "lock"}});
    h.put({{"classname", "trigger_key"},
           {"targetname", "lock"},
           {"item", "key_blue_key"},
           {"target", "gate"}});
    trigger(h, -400, -380, "first_help");
    h.put({{"classname", "target_help"},
           {"targetname", "first_help"},
           {"message", "Find the blue key."},
           {"spawnflags", "1"}});
    exit_to(h, "unit2");
    if (!h.write(a, error)) return false;
  }
  {  // the second: a button opens the door, an objective past it
    Hall h;
    start(h);
    door(h, "gate");
    h.brush(-500, 496, 32, -440, 512, 96,
            {{"classname", "func_button"}, {"target", "gate"}, {"angle", "90"}});
    trigger(h, 300, 320, "goal");
    h.put({{"classname", "target_goal"}, {"targetname", "goal"}});
    exit_to(h, "unit3");
    if (!h.write(b, error)) return false;
  }
  {  // the third: a secret, and the exit out of the unit
    Hall h;
    start(h);
    h.brush(-600, 300, 0, -500, 400, 96, {{"classname", "trigger_once"}, {"target", "found"}});
    h.put({{"classname", "target_secret"}, {"targetname", "found"}});
    exit_to(h, "*next");
    if (!h.write(c, error)) return false;
  }
  return ge::vfs::write_pak({{"maps/unit1.bsp", a}, {"maps/unit2.bsp", b}, {"maps/unit3.bsp", c}},
                            &pak, &error);
}

struct Loader {
  const cl::SearchPath* files = nullptr;
  std::vector<std::shared_ptr<const br::Map>> read;  // each map read, by the order asked
};

bool load(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  Loader& l = *static_cast<Loader*>(ctx);
  std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(*l.files, name, error);
  if (!m) return false;
  auto read = std::make_shared<br::Map>();
  if (!br::read_bsp(m->file, read.get(), &error)) return false;
  out = sv::MapSpec();
  out.name = name;
  out.brushes = read;
  l.read.push_back(read);
  return true;
}

// The map from above: its floors, his track, the keys and the exits.
gl::Picture overhead(const br::Map& map, const bt::UnitMap& um) {
  constexpr int kSize = 560, kPad = 24;
  gl::Picture p = gl::create_picture(kSize, kSize + 40, {18, 18, 22});
  if (map.models.empty()) return p;
  float lo[3], hi[3];
  for (int k = 0; k < 3; k++) {
    lo[k] = map.models[0].mins[k];
    hi[k] = map.models[0].maxs[k];
  }
  const float span = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0f});
  const auto px = [&](float x, float y) {
    return std::pair<double, double>{kPad + (x - lo[0]) / span * (kSize - 2 * kPad),
                                     kPad + (hi[1] - y) / span * (kSize - 2 * kPad)};
  };
  const float zspan = std::max(hi[2] - lo[2], 1.0f);
  for (const br::Surface& f : map.surfaces) {
    if (f.kind == br::SurfaceKind::sky) continue;
    for (uint32_t t = 0; t + 2 < f.count; t += 3) {
      const br::Vertex* v[3];
      for (int k = 0; k < 3; k++) v[k] = &map.vertices[map.indices[f.first + t + uint32_t(k)]];
      if (v[0]->nrm[2] < 0.7f) continue;  // floors only
      const float z = (v[0]->pos[2] - lo[2]) / zspan;
      const auto g = static_cast<uint8_t>(50 + 120 * std::clamp(z, 0.0f, 1.0f));
      for (int k = 0; k < 3; k++) {
        const auto [x0, y0] = px(v[k]->pos[0], v[k]->pos[1]);
        const auto [x1, y1] = px(v[(k + 1) % 3]->pos[0], v[(k + 1) % 3]->pos[1]);
        gl::draw_line(p, x0, y0, x1, y1, {g, g, static_cast<uint8_t>(g + 20)});
      }
    }
  }
  for (const sh::Vec3f& n : um.nodes) {  // where he could stand: the nav's nodes
    const auto [x, y] = px(n.x, n.y);
    gl::set_pixel(p, static_cast<int>(x), static_cast<int>(y), {70, 75, 90});
  }
  // His track yellow; a party's partners (`--partners`) each in a colour of their own.
  static const gl::Rgba kPartner[3] = {
      {90, 170, 255, 255}, {240, 90, 220, 255}, {120, 230, 120, 255}};
  for (size_t k = 0; k < um.party.size(); k++)
    for (size_t i = 1; i < um.party[k].size(); i++) {
      const auto [x0, y0] = px(um.party[k][i - 1].x, um.party[k][i - 1].y);
      const auto [x1, y1] = px(um.party[k][i].x, um.party[k][i].y);
      gl::draw_line(p, x0, y0, x1, y1, kPartner[k % 3], 1.5);
    }
  for (size_t i = 1; i < um.track.size(); i++) {
    const auto [x0, y0] = px(um.track[i - 1].x, um.track[i - 1].y);
    const auto [x1, y1] = px(um.track[i].x, um.track[i].y);
    gl::draw_line(p, x0, y0, x1, y1, {255, 210, 60}, 1.5);
  }
  for (const sh::Vec3f& e : um.exits) {
    const auto [x, y] = px(e.x, e.y);
    gl::fill_disc(p, x, y, 7, {70, 140, 255});
  }
  for (size_t k = 0; k < um.keys_at.size(); k++) {
    const auto [x, y] = px(um.keys_at[k].x, um.keys_at[k].y);
    gl::fill_disc(p, x, y, 5, {90, 220, 110});
    if (static_cast<int>(k) < um.keys_taken) {
      gl::draw_ring(p, x, y, 8, {255, 70, 70});
      gl::draw_ring(p, x, y, 9, {255, 70, 70});
    }
  }
  if (!um.track.empty()) {
    const auto [x, y] = px(um.track[0].x, um.track[0].y);
    gl::draw_ring(p, x, y, 6, {255, 255, 255});
  }
  gl::draw_line(p, 10, kSize + 14, 24, kSize + 14, {255, 210, 60}, 2.0);
  gl::draw_text(p, 30, kSize + 10, "HIS WAY", {220, 220, 220});
  gl::fill_disc(p, 100, kSize + 14, 5, {90, 220, 110});
  gl::draw_text(p, 110, kSize + 10, "KEY", {220, 220, 220});
  gl::fill_disc(p, 150, kSize + 14, 6, {70, 140, 255});
  gl::draw_text(p, 160, kSize + 10, "EXIT", {220, 220, 220});
  gl::draw_text(p, 12, kSize + 26,
                um.name + ": " + ge::core::num::to_fixed(um.seconds, 1) + " s, " +
                    (um.finished ? "out to " + um.next : um.end),
                {230, 230, 230});
  return p;
}

void run(gl::Lab& lab) {
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  const bool own = lab.arg("game").empty();
  std::string first = "unit1";
  if (own) {
    std::vector<uint8_t> pak;
    if (!unit(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture unit should write and mount. Skill: brush.");
      return;
    }
  } else {
    if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
      lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                          "/pak0.pak>. Skill: shooter.");
      return;
    }
    first = "base1";
  }
  if (!lab.arg("first").empty()) first = lab.arg("first");
  Loader loader;
  loader.files = &files;
  bt::UnitOptions opt;
  opt.load = &load;
  opt.load_ctx = &loader;
  opt.first = first;
  opt.maps = static_cast<int>(lab.arg_num("maps"));
  opt.map_seconds = lab.arg_num("seconds");
  opt.plan = !lab.arg("plan").empty();
  opt.complete = !lab.arg("complete").empty();
  opt.partners.assign(
      static_cast<size_t>(std::clamp(static_cast<int>(lab.arg_num("partners")), 0, 3)),
      ge::shooterbots::AimSkill::expert);
  opt.beside = !lab.arg("beside").empty();
  const bt::UnitReport r = bt::run_q2_unit(opt);
  if (!r.ok) {
    lab.fail(r.error, "Read the reason above. Skill: bots.");
    return;
  }
  const gl::Det det = own ? gl::Det::deterministic : gl::Det::measured;
  gl::LabTable t;
  t.title = "The unit from " + first + (r.unit_done ? " out to " + r.out_to : ", not finished");
  t.columns = {"map",     "time (s)", "led to",   "keys",      "objectives",
               "secrets", "helps",    "monsters", "steps done"};
  int keys = 0, keys_taken = 0, goals = 0, goals_found = 0, finished = 0;
  for (const bt::UnitMap& m : r.maps) {
    t.rows.push_back({m.name, ge::core::num::to_fixed(m.seconds, 1), m.finished ? m.next : m.end,
                      std::to_string(m.keys_taken) + " of " + std::to_string(m.keys),
                      std::to_string(m.goals_found) + " of " + std::to_string(m.goals),
                      std::to_string(m.secrets_found) + " of " + std::to_string(m.secrets),
                      std::to_string(m.helps),
                      std::to_string(m.killed) + " of " + std::to_string(m.monsters),
                      std::to_string(m.way.steps_done)});
    keys += m.keys;
    keys_taken += m.keys_taken;
    goals += m.goals;
    goals_found += m.goals_found;
    finished += m.finished ? 1 : 0;
  }
  lab.table(t, det);
  if (opt.beside) {
    gl::LabTable h;
    h.title = "Helpers beside him (a person's stand-in)";
    h.columns = {"map",         "time (s)", "near share",  "farthest",
                 "back to him", "gave way", "exits turned"};
    for (const bt::UnitMap& m : r.maps)
      h.rows.push_back({m.name, ge::core::num::to_fixed(m.seconds, 1),
                        ge::core::num::to_fixed(m.helper_near_share, 3),
                        ge::core::num::to_fixed(m.helper_farthest, 0),
                        std::to_string(m.helper_stays), std::to_string(m.helper_gave_way),
                        std::to_string(m.helper_exits_turned)});
    lab.table(h, det);
  }
  lab.metric("unit.done", r.unit_done ? 1 : 0, "bool", det);
  lab.metric("unit.maps", static_cast<double>(r.maps.size()), "count", det);
  lab.metric("unit.exits_taken", finished, "count", det);
  lab.metric("unit.keys_taken", keys_taken, "count", det);
  lab.metric("unit.keys", keys, "count", det);
  lab.metric("unit.objectives_found", goals_found, "count", det);
  lab.metric("unit.objectives", goals, "count", det);
  lab.metric("unit.digest_low", static_cast<double>(r.digest & 0xffffffu), "hash", det);
  for (const bt::UnitMap& m : r.maps)
    for (const std::string& line : m.plan) lab.note(m.name + ": " + line);
  if (!lab.arg("track").empty()) {
    // The track holds a point every 0.2 s (bots/q2_unit.hpp); --track=1 notes one a second.
    const size_t every = static_cast<size_t>(std::max(1.0, lab.arg_num("track") * 5 + 0.5));
    for (const bt::UnitMap& m : r.maps)
      for (size_t i = 0; i < m.track.size(); i += every)
        lab.note(m.name + ": track " + ge::core::num::to_fixed(0.2 * static_cast<double>(i), 1) +
                 " s " + ge::core::num::to_fixed(m.track[i].x, 0) + " " +
                 ge::core::num::to_fixed(m.track[i].y, 0) + " " +
                 ge::core::num::to_fixed(m.track[i].z, 0));
  }
  if (own && (!r.unit_done || keys_taken != keys || goals_found != goals))
    lab.fail("the script did not play the fixture unit through: " + std::to_string(finished) +
                 " exits, " + std::to_string(keys_taken) + " of " + std::to_string(keys) +
                 " keys, " + std::to_string(goals_found) + " of " + std::to_string(goals) +
                 " objectives",
             "Run with --plan to see his changes of plan, then check shooterbots/way_through.cpp's "
             "steps for Quake II's words. Skill: bots.");
  for (size_t k = 0; k < r.maps.size() && k < loader.read.size(); k++) {
    const bt::UnitMap& m = r.maps[k];
    lab.shot({m.name,
              m.name + (m.finished ? " played through" : " not finished"),
              "The floors of " + m.name +
                  " from above with the way the script took in yellow from "
                  "his start (ringed white), the keys in green (ringed red once he took them) "
                  "and the exits in blue.",
              "a yellow line from the white ring to a blue exit",
              {{"map", m.name}, {"game", own ? "fixture" : "local packs"}}},
             overhead(*loader.read[k], m), det);
  }
}

const gl::LabSpec kSpec{
    "se-q2-unit",
    gl::LabKind::drive,
    "game:shooters_edge",
    "bots/q2_unit.hpp: a unit of Quake II's campaign played start to end by a script on "
    "Quake II's own rules",
    "bots",
    "Can a script with nothing but each map's entities play a unit of Quake II's campaign from "
    "its first map to the exit out of it, taking every key and reaching every objective on the "
    "way?",
    {{"game", "", "the folder holding Quake II's base folder (none: a fixture unit of three)"},
     {"first", "", "the unit's first map (default unit1; base1 with --game)"},
     {"maps", "12", "at most this many maps entered"},
     {"seconds", "600", "a map's time before it is called unfinished, s"},
     {"plan", "", "any value: note every change of the script's plan"},
     {"track", "",
      "seconds: note where he was every so often (0.2 at the finest), to read beside --plan"},
     {"complete", "", "any value: the 100% run (every monster and secret, then the exit)"},
     {"partners", "0",
      "0..3 partners beside him in co-op, the map's work split among them "
      "(plugins/shooterbots/party.hpp); each partner's track in its own colour"},
     {"beside", "",
      "any value: he plays as a person would and the partners are helpers beside him (a hosted "
      "game's `addbot helper`: leashed to him, the exit his); a table of how they kept to him"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
