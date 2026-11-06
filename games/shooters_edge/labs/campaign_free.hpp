// Our campaign program on a FREE map (the `se-campaign` lab's `--free`): the test arena's two
// rooms with five monsters, the doorway's door, and an exit, no pack anywhere. A scripted player
// starts in the west room, turns to the nearest living monster it can see in its room and fires,
// walks the rooms' waypoints when none is left there, walks into the exit at the end and presses
// a button at the intermission until the level changes. The table
// says who was killed when, what the player took and when the level ended; the picture is the map
// from above with the player's path and where each monster fell. Every number is deterministic, so
// the lab's digest holds the whole run.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's free map: our program's monsters fought to the exit, no pack
// ge:keywords lab free-map campaign-variant monsters-to-exit scripted-player exit-trigger
// ge:keywords campaign-lab arena-campaign
// ge:skill progs shooter labs
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/file.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::labs {

namespace fr {
namespace gl = ge::lab;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

// The arena (games/shooters_edge/sim/arena.hpp): the west room x -512..128, the east 144..512,
// y -512..512, the doorway at x 128..144, y -64..64. The door is brush model *1, the exit the
// plat's brush (*2) at x 192..320, y -384..-256, taken as a trigger.
inline const char* const kFreeMap = R"({
"classname" "worldspawn"
"message" "the test arena, our campaign"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "info_intermission"
"origin" "0 0 160"
"mangle" "20 0 0"
}
{
"classname" "func_door"
"model" "*1"
"angle" "-1"
"sounds" "1"
}
{
"classname" "monster_army"
"origin" "-128 256 24"
"angle" "180"
}
{
"classname" "monster_dog"
"origin" "-128 -256 24"
"angle" "180"
}
{
"classname" "monster_army"
"origin" "416 256 24"
"angle" "180"
}
{
"classname" "monster_army"
"origin" "416 -96 24"
"angle" "180"
}
{
"classname" "monster_dog"
"origin" "256 384 24"
"angle" "270"
}
{
"classname" "trigger_changelevel"
"model" "*2"
"map" "start"
}
)";

inline bool free_load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out,
                      std::string& /*error*/) {
  out.models = sv::arena_models(true);
  out.entities = kFreeMap;
  return true;
}

struct Vec2 {
  float x, y;
};

// The waypoints the player walks when no monster is in sight: through the west room, the
// doorway, the east room, and into the exit.
inline const Vec2 kWay[] = {{-160, 0}, {136, 0}, {300, 0}, {256, -320}};

inline float yaw_to(const sh::Vec3f& from, float x, float y) {
  // Degrees, 0 along +x and 90 along +y, as the move's angles read them.
  return static_cast<float>(ge::core::libm::atan2(y - from.y, x - from.x) * 57.29577951308232);
}

inline bool west_of_door(float x) { return x < 136; }

struct Monster {
  int e = 0;
  std::string kind;
  double killed = -1;  // s, or -1
  sh::Vec3f at{};
};

}  // namespace fr

// `seconds` of the scripted player on the free map; `root` the repository (our program's sources).
inline void free_campaign(ge::lab::Lab& lab, const std::string& root, double seconds) {
  using namespace fr;
  const std::filesystem::path src =
      std::filesystem::path(root) / "games/shooters_edge/qc/campaign/progs.src";
  const pg::QcProject built = pg::compile_project(src);
  if (!built.result.ok) {
    lab.fail("se-campaign --free: " + src.generic_string() + " does not compile",
             "next: `ge qc build " + src.generic_string() + "` from the repository root");
    return;
  }
  sv::Campaign c;
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 256;
  o.load = &free_load;
  o.settings = {"deathmatch 0", "skill 1"};
  std::string error;
  if (!sv::campaign_new_game(c, built.result.progs, o, "arena", error)) {
    lab.fail("se-campaign --free: the free map did not start: " + error,
             "next: read the error; skill: progs");
    return;
  }
  sv::campaign_join(c, 0, "player");
  sv::QcServer* s = sv::qc_of(c.server);
  const pg::Progs& p = built.result.progs;
  const auto field = [&](const char* name) {
    const pg::Def* d = pg::find_field(p, name);
    return d ? d->offset : -1;
  };
  const int f_health = field("health"), f_class = field("classname");
  const auto num = [&](int e, int ofs) { return pg::as_float(*pg::entity_field(s->q.vm, e, ofs)); };
  std::vector<Monster> monsters;
  for (int e = 1; e < s->sv.num_edicts; e++) {
    if (s->sv.edicts[static_cast<size_t>(e)].free) continue;
    const char* cn =
        pg::vm_string(s->q.vm, static_cast<int32_t>(*pg::entity_field(s->q.vm, e, f_class)));
    if (cn && std::string(cn).rfind("monster_", 0) == 0) monsters.push_back({e, cn, -1, {}});
  }
  std::vector<sh::Vec3f> path;
  double left_at = -1, taken = 0;
  float last_health = num(1, f_health);
  int deaths = 0;
  size_t way = 0;
  uint32_t sequence = 0;
  const int frames = static_cast<int>(seconds / sv::kCampaignStep);
  const size_t maps = c.history.size();
  for (int i = 0; i < frames && c.server; i++) {
    s = sv::qc_of(c.server);
    const sh::Vec3f me = s->sv.edicts[1].origin;
    const double t = i * sv::kCampaignStep;
    if (i % 18 == 0 && me.z < 100) path.push_back(me);  // not from the intermission's spot
    // The nearest monster alive on this side of the door.
    const Monster* target = nullptr;
    float best = 1e9f;
    for (Monster& m : monsters) {
      if (m.killed >= 0) continue;
      const sh::Edict& ed = s->sv.edicts[static_cast<size_t>(m.e)];
      if (num(m.e, f_health) <= 0 || ed.free) {
        m.killed = t;
        m.at = ed.origin;
        continue;
      }
      if (west_of_door(ed.origin.x) != west_of_door(me.x)) continue;
      const float d =
          (ed.origin.x - me.x) * (ed.origin.x - me.x) + (ed.origin.y - me.y) * (ed.origin.y - me.y);
      if (d < best) best = d, target = &m;
    }
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++sequence;
    float yaw = 0, forward = 0;
    uint8_t buttons = 0;
    if (me.z > 100) {
      buttons = static_cast<uint8_t>(i & 1);  // at the intermission's spot: a button leaves it
    } else if (target != nullptr) {
      const sh::Vec3f at = s->sv.edicts[static_cast<size_t>(target->e)].origin;
      yaw = yaw_to(me, at.x, at.y);
      buttons = static_cast<uint8_t>(i & 1);  // the attack, every other frame
      forward = best > 300.0f * 300.0f ? 200.0f : 0.0f;
    } else if (way < std::size(kWay)) {
      const Vec2 w = kWay[way];
      yaw = yaw_to(me, w.x, w.y);
      forward = 320;
      if ((w.x - me.x) * (w.x - me.x) + (w.y - me.y) * (w.y - me.y) < 24 * 24) way++;
    }
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.angles = {0, yaw, 0};
      m.forward = static_cast<int16_t>(forward);
      m.buttons = buttons;
    }
    if (!sv::campaign_frame(c, &pk, 1, error)) break;
    if (!c.server) break;
    s = sv::qc_of(c.server);
    const float h = num(1, f_health);
    if (h < last_health) taken += last_health - h;
    if (h <= 0 && last_health > 0) deaths++;
    last_health = h;
    if (c.history.size() != maps) {  // the level changed: the run is over
      left_at = t;
      break;
    }
  }
  gl::LabTable kills{"our program's monsters on the free map, fought by the scripted player",
                     {"monster", "kind", "killed at (s)", "where"},
                     {}};
  int killed = 0;
  for (const Monster& m : monsters) {
    killed += m.killed >= 0;
    kills.rows.push_back({std::to_string(m.e), m.kind,
                          m.killed >= 0 ? ge::core::num::to_fixed(m.killed, 2) : "alive",
                          m.killed >= 0 ? ge::core::num::to_fixed(m.at.x, 0) + " " +
                                              ge::core::num::to_fixed(m.at.y, 0)
                                        : "-"});
  }
  lab.table(kills);
  gl::LabTable run{"the free map's run", {"what", "value"}, {}};
  run.rows = {
      {"monsters killed", std::to_string(killed) + " of " + std::to_string(monsters.size())},
      {"health the player took", ge::core::num::to_fixed(taken, 0)},
      {"the player's deaths (each restarts the map)", std::to_string(deaths)},
      {"the level left at (s)", left_at < 0 ? "not left" : ge::core::num::to_fixed(left_at, 2)},
      {"the level change", c.history.size() == maps ? "-" : c.history.back()},
      {"faults", s ? std::to_string(s->q.faults) : "-"}};
  lab.table(run);
  lab.metric("free.killed", killed, "count");
  lab.metric("free.left", left_at < 0 ? 0 : 1, "bool");
  lab.metric("free.faults", s ? static_cast<double>(s->q.faults) : -1, "count");
  // The map from above: 1 pixel 2 units, x across and y up.
  gl::Picture pic = gl::create_picture(512, 512, {24, 26, 30});
  const auto px = [](float x) { return (x + 512) / 2; };
  const auto py = [](float y) { return (512 - y) / 2; };
  gl::fill_rect(pic, px(-512), py(512), 320, 512, {58, 62, 70});
  gl::fill_rect(pic, px(144), py(512), 184, 512, {58, 62, 70});
  gl::fill_rect(pic, px(128), py(64), 8, 64, {120, 96, 60});     // the doorway's door
  gl::fill_rect(pic, px(192), py(-256), 64, 64, {60, 110, 80});  // the exit
  for (size_t k = 1; k < path.size(); k++)
    gl::draw_line(pic, px(path[k - 1].x), py(path[k - 1].y), px(path[k].x), py(path[k].y),
                  {120, 190, 255});
  for (const Monster& m : monsters) {
    if (m.killed < 0) continue;
    gl::draw_line(pic, px(m.at.x) - 5, py(m.at.y) - 5, px(m.at.x) + 5, py(m.at.y) + 5,
                  {240, 80, 70});
    gl::draw_line(pic, px(m.at.x) - 5, py(m.at.y) + 5, px(m.at.x) + 5, py(m.at.y) - 5,
                  {240, 80, 70});
  }
  gl::fill_disc(pic, px(-416), py(0), 4, {255, 255, 255});
  lab.shot(
      {"free-map",
       "OUR CAMPAIGN ON THE FREE MAP",
       "The test arena from above: the west room, the doorway's door (brown) and the east "
       "room with the exit (green). Blue is the scripted player's path from its start (white); "
       "a red cross is where a monster fell.",
       "every monster crossed out and the path ending in the exit",
       {{"seconds", ge::core::num::to_fixed(seconds, 0)}, {"skill", "1"}}},
      pic);
}

}  // namespace shooters_edge::labs
