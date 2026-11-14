// Our campaign program on a FREE EPISODE (the `se-campaign` lab's `--free`): three fixture maps on
// the test arena's two rooms, no pack anywhere. `start` is the hall whose way in leads straight on
// to `e1m1`; `e1m1` has five monsters, the doorway's door and an exit; `e1m2` has three monsters, a
// door the gold key opens, the key, the episode's rune and an exit back to `start`. A scripted
// player turns to the nearest living monster it can see in its room and fires, walks the map's
// waypoints when none is left there (over the key and the rune on `e1m2`), walks into each exit,
// and presses a button at each intermission until the level changes (twice after the rune: the
// episode's closing words, then the way on). The episode is over when `start` comes round again
// with the rune's bit in the server flags. The tables say what was killed where and when, and
// each map's entry and exit; the picture is each map from above with the player's path. Every
// number is deterministic, so the lab's digest holds the whole episode.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's free episode: our program through three fixture maps, no pack
// ge:keywords lab free-map free-episode campaign-variant monsters-to-exit scripted-player
// ge:keywords exit-trigger campaign-lab arena-campaign episode-end rune finale key-door
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
#include "progs/vm.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
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
inline const char* const kStartMap = R"({
"classname" "worldspawn"
"message" "the test arena, the hall"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "func_door"
"model" "*1"
"angle" "-1"
"sounds" "1"
}
{
"classname" "trigger_changelevel"
"model" "*2"
"map" "e1m1"
"spawnflags" "1"
}
)";

inline const char* const kFirstMap = R"({
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
"map" "e1m2"
}
)";

inline const char* const kRuneMap = R"({
"classname" "worldspawn"
"message" "the test arena, the rune"
"worldtype" "2"
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
"sounds" "2"
"spawnflags" "8"
}
{
"classname" "item_key2"
"origin" "-320 -224 24"
}
{
"classname" "monster_dog"
"origin" "-160 320 24"
"angle" "270"
}
{
"classname" "monster_army"
"origin" "416 320 24"
"angle" "180"
}
{
"classname" "monster_army"
"origin" "448 -160 24"
"angle" "180"
}
{
"classname" "item_sigil"
"origin" "400 192 24"
"spawnflags" "1"
}
{
"classname" "trigger_changelevel"
"model" "*2"
"map" "start"
}
)";

inline bool free_load(void* /*ctx*/, const std::string& name, sv::MapSpec& out,
                      std::string& error) {
  out.models = sv::arena_models(true);
  if (name == "start")
    out.entities = kStartMap;
  else if (name == "e1m1")
    out.entities = kFirstMap;
  else if (name == "e1m2")
    out.entities = kRuneMap;
  else {
    error = "the free episode has no map '" + name + "' (start, e1m1, e1m2)";
    return false;
  }
  return true;
}

struct Vec2 {
  float x, y;
};

// The waypoints each map's player walks when no monster is in sight: through the west room (over
// the key on e1m2), the doorway, the east room (over the rune on e1m2), and into the exit.
inline std::vector<Vec2> way_of(const std::string& map) {
  if (map == "e1m2")
    return {{-320, -224}, {-160, 0}, {136, 0}, {300, 0}, {400, 192}, {300, 0}, {256, -320}};
  return {{-160, 0}, {136, 0}, {300, 0}, {256, -320}};
}

inline float yaw_to(const sh::Vec3f& from, float x, float y) {
  // Degrees, 0 along +x and 90 along +y, as the move's angles read them.
  return static_cast<float>(ge::core::libm::atan2(y - from.y, x - from.x) * 57.29577951308232);
}

inline bool west_of_door(float x) { return x < 136; }

struct Monster {
  int e = 0;
  size_t leg = 0;
  std::string kind;
  double killed = -1;  // s, or -1
  sh::Vec3f at{};
};

// One map as played: from its line in the campaign's history to the next.
struct Leg {
  std::string line;  // "new start", "change e1m1", "restart e1m2"
  std::string map;
  double entered = 0, left = -1, finale = -1;  // s; -1 not reached
  std::vector<sh::Vec3f> path;
};

}  // namespace fr

// `seconds` of the scripted player on the free episode, on the campaign in C++.
inline void free_campaign(ge::lab::Lab& lab, double seconds) {
  using namespace fr;
  pg::Progs progs;
  if (std::string error; !sv::campaign_mod(progs, error)) {
    lab.fail("se-campaign --free: the campaign: " + error, "next: fix the campaign's rules");
    return;
  }
  sv::Campaign c;
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 256;
  o.load = &free_load;
  o.settings = {"deathmatch 0", "skill 1"};
  std::string error;
  if (!sv::campaign_new_game(c, progs, o, "start", error)) {
    lab.fail("se-campaign --free: the free episode did not start: " + error,
             "next: read the error; skill: progs");
    return;
  }
  sv::campaign_join(c, 0, "player");
  const pg::Progs& p = progs;
  const auto field = [&](const char* name) {
    const pg::Def* d = pg::find_field(p, name);
    return d ? d->offset : -1;
  };
  const pg::Def* finale_def = pg::find_global(p, "finale_words");
  const int f_health = field("health"), f_class = field("classname");
  sv::QcServer* s = sv::qc_of(c.server);
  const auto num = [&](int e, int ofs) { return pg::as_float(*pg::entity_field(s->q.vm, e, ofs)); };
  const auto finale_up = [&] {
    if (finale_def == nullptr) return false;
    const char* w = pg::vm_string(s->q.vm, pg::global_int(s->q.vm, finale_def->offset));
    return w != nullptr && w[0] != 0;
  };
  std::vector<Monster> monsters;
  std::vector<Leg> legs;
  std::vector<Vec2> way;
  size_t next_way = 0;
  // A map entered: its monsters listed, its waypoints from the first.
  const auto enter = [&](double t) {
    s = sv::qc_of(c.server);
    const std::string& line = c.history.back();
    legs.push_back({line, line.substr(line.find(' ') + 1), t, -1, -1, {}});
    for (int e = 1; e < s->sv.num_edicts; e++) {
      if (s->sv.edicts[static_cast<size_t>(e)].free) continue;
      const char* cn =
          pg::vm_string(s->q.vm, static_cast<int32_t>(*pg::entity_field(s->q.vm, e, f_class)));
      if (cn && std::string(cn).rfind("monster_", 0) == 0)
        monsters.push_back({e, legs.size() - 1, cn, -1, {}});
    }
    way = way_of(legs.back().map);
    next_way = 0;
  };
  enter(0);
  double taken = 0, episode_done = -1;
  float last_health = num(1, f_health);
  int deaths = 0;
  uint32_t sequence = 0;
  const int frames = static_cast<int>(seconds / sv::kCampaignStep);
  size_t maps = c.history.size();
  for (int i = 0; i < frames && c.server; i++) {
    s = sv::qc_of(c.server);
    const sh::Vec3f me = s->sv.edicts[1].origin;
    const double t = i * sv::kCampaignStep;
    if (episode_done >= 0 && t >= episode_done + 1.0) break;        // a second on the hall again
    if (i % 18 == 0 && me.z < 100) legs.back().path.push_back(me);  // not from the intermission
    // The nearest monster of this map alive on this side of the door.
    const Monster* target = nullptr;
    float best = 1e9f;
    for (Monster& m : monsters) {
      if (m.killed >= 0 || m.leg + 1 != legs.size()) continue;
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
    } else if (next_way < way.size()) {
      const Vec2 w = way[next_way];
      yaw = yaw_to(me, w.x, w.y);
      forward = 320;
      if ((w.x - me.x) * (w.x - me.x) + (w.y - me.y) * (w.y - me.y) < 24 * 24) next_way++;
    }
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.angles = {0, yaw, 0};
      m.forward = static_cast<int16_t>(forward);
      m.buttons = buttons;
    }
    const bool finale_before = finale_up();
    if (!sv::campaign_frame(c, &pk, 1, error)) break;
    if (!c.server) break;
    s = sv::qc_of(c.server);
    if (c.history.size() != maps) {  // the level changed (or restarted)
      maps = c.history.size();
      legs.back().left = t;
      enter(t);
      if (legs.back().map == "start" && (static_cast<int>(c.serverflags) & 1)) episode_done = t;
      last_health = num(1, f_health);
      continue;
    }
    if (finale_before && !finale_up()) legs.back().finale = t;
    const float h = num(1, f_health);
    if (h < last_health) taken += last_health - h;
    if (h <= 0 && last_health > 0) deaths++;
    last_health = h;
  }
  const auto fixed = [](double v, int d) { return ge::core::num::to_fixed(v, d); };
  gl::LabTable kills{"our program's monsters on the free episode, fought by the scripted player",
                     {"map", "monster", "kind", "killed at (s)", "where"},
                     {}};
  int killed = 0;
  for (const Monster& m : monsters) {
    killed += m.killed >= 0;
    kills.rows.push_back({legs[m.leg].line, std::to_string(m.e), m.kind,
                          m.killed >= 0 ? fixed(m.killed, 2) : "alive",
                          m.killed >= 0 ? fixed(m.at.x, 0) + " " + fixed(m.at.y, 0) : "-"});
  }
  lab.table(kills);
  gl::LabTable maps_table{"the free episode's maps, as the campaign's history has them",
                          {"map", "entered at (s)", "left at (s)", "closing words at (s)"},
                          {}};
  for (const Leg& l : legs)
    maps_table.rows.push_back({l.line, fixed(l.entered, 2), l.left < 0 ? "-" : fixed(l.left, 2),
                               l.finale < 0 ? "-" : fixed(l.finale, 2)});
  lab.table(maps_table);
  gl::LabTable run{"the free episode's run", {"what", "value"}, {}};
  run.rows = {
      {"monsters killed", std::to_string(killed) + " of " + std::to_string(monsters.size())},
      {"health the player took", fixed(taken, 0)},
      {"the player's deaths (each restarts the map)", std::to_string(deaths)},
      {"the episode over at (s) (the hall again, the rune held)",
       episode_done < 0 ? "not over" : fixed(episode_done, 2)},
      {"the server flags", std::to_string(static_cast<int>(c.serverflags))},
      {"faults", s ? std::to_string(s->q.faults) : "-"}};
  lab.table(run);
  lab.metric("free.killed", killed, "count");
  lab.metric("free.maps", static_cast<double>(legs.size()), "count");
  lab.metric("free.episode", episode_done < 0 ? 0 : 1, "bool");
  lab.metric("free.faults", s ? static_cast<double>(s->q.faults) : -1, "count");
  // Each map from above, side by side: 1 pixel 2 units, x across and y up.
  const int panel = 512, gap = 8;
  const int shown = static_cast<int>(legs.size() < 3 ? legs.size() : 3);
  gl::Picture pic = gl::create_picture(panel * shown + gap * (shown - 1), panel, {24, 26, 30});
  for (int k = 0; k < shown; k++) {
    const Leg& l = legs[static_cast<size_t>(k)];
    const float ox = static_cast<float>(k * (panel + gap));
    const auto px = [&](float x) { return ox + (x + 512) / 2; };
    const auto py = [](float y) { return (512 - y) / 2; };
    gl::fill_rect(pic, px(-512), py(512), 320, 512, {58, 62, 70});
    gl::fill_rect(pic, px(144), py(512), 184, 512, {58, 62, 70});
    const gl::Rgba door =
        l.map == "e1m2" ? gl::Rgba{220, 180, 60, 255} : gl::Rgba{120, 96, 60, 255};
    gl::fill_rect(pic, px(128), py(64), 8, 64, door);              // the doorway's door
    gl::fill_rect(pic, px(192), py(-256), 64, 64, {60, 110, 80});  // the exit
    if (l.map == "e1m2") {
      gl::fill_disc(pic, px(-320), py(-224), 5, {220, 180, 60});  // the gold key
      gl::fill_disc(pic, px(400), py(192), 6, {200, 120, 255});   // the rune
    }
    for (size_t j = 1; j < l.path.size(); j++)
      gl::draw_line(pic, px(l.path[j - 1].x), py(l.path[j - 1].y), px(l.path[j].x), py(l.path[j].y),
                    {120, 190, 255});
    for (const Monster& m : monsters) {
      if (m.killed < 0 || m.leg != static_cast<size_t>(k)) continue;
      gl::draw_line(pic, px(m.at.x) - 5, py(m.at.y) - 5, px(m.at.x) + 5, py(m.at.y) + 5,
                    {240, 80, 70});
      gl::draw_line(pic, px(m.at.x) - 5, py(m.at.y) + 5, px(m.at.x) + 5, py(m.at.y) - 5,
                    {240, 80, 70});
    }
    gl::fill_disc(pic, px(-416), py(0), 4, {255, 255, 255});
  }
  lab.shot({"free-episode",
            "OUR CAMPAIGN'S FREE EPISODE",
            "Three fixture maps on the test arena from above, left to right in the order played "
            "(the hall, the first map, the rune's map): the west room, the doorway's door (brown, "
            "gold when the gold key opens it) and the east room with the exit (green). Blue is the "
            "scripted player's path from its start (white); a red cross is where a monster fell; "
            "the gold dot is the key and the violet one the rune.",
            "every monster crossed out, the key and the rune walked over, each path ending in its "
            "exit",
            {{"seconds", fixed(seconds, 0)}, {"skill", "1"}}},
           pic);
}

}  // namespace shooters_edge::labs
