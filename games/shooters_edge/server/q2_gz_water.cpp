// Ground Zero's smart water (q2_gz.hpp states the rules, the numbers are the clean-room note's,
// porting/shooters-edge/gz-world.md § Smart water): a `func_water` with flag 2 whose middle is in a
// liquid rises after the lowest living player once used, instead of its plain move, until its top
// reaches the height its `health` key names. The map's movers (q2_map.cpp) hand it its uses and
// its blocks; the frame (q2_gz_things.cpp) steers it.
//
// ge:summary Ground Zero's smart water: lava that rises after the lowest player once used
// ge:keywords q2 ground-zero rogue mission-pack smart-water rising-lava func-water rmine2
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_things_test.cpp
#include <algorithm>
#include <cstdlib>
#include <string>

#include "brush/map.hpp"
#include "shooter/mover_motion.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

namespace {

constexpr sh::MoverFrame kFrame{0.1f, 10};
constexpr int kRising = 4;   // `count`'s bit: the water rises (its plain move set aside for good)
constexpr float kCreep = 5;  // the slowest it rises, units a second
constexpr int kBlockHarm = 100;

std::string key(const Q2Server& s, const Q2Entity& e, const char* name) {
  if (e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get(name);
}
float number(const Q2Server& s, const Q2Entity& e, const char* name, float fallback) {
  const std::string v = key(s, e, name);
  return v.empty() ? fallback : std::strtof(v.c_str(), nullptr);
}

int index_of(const Q2Server& s, const Q2Entity& e) {
  return static_cast<int>(&e - s.entities.data());
}

bool smart(const Q2Server& s, const Q2Entity& e) {
  return s.opt.pack == Q2Pack::ground_zero && e.classname == "func_water" &&
         (e.spawnflags & 2) != 0;
}

float top(const Q2Entity& e) { return e.origin.z + e.maxs.z; }

// Holds the water where it stands.
void hold(Q2Entity& e) {
  e.arrive = q2map::kNoAct;
  e.velocity = {};
  e.frames = 0;
}

}  // namespace

bool gz_rising_liquid(const Q2Server& s, const Q2Entity& d) {
  if (d.gone || (d.spawnflags & 2) == 0) return false;
  if (d.classname != "func_door" && d.classname != "func_water") return false;
  const sh::Vec3f mid = d.origin + (d.mins + d.maxs) * 0.5f;
  return (map_contents(s, mid) & (sh::kSqWater | sh::kSqSlime | sh::kSqLava)) != 0;
}

bool gz_smart_water_use(Q2Server& s, Q2Entity& e, int activator) {
  if (!smart(s, e) || !gz_rising_liquid(s, e)) return false;
  if ((e.count & kRising) != 0) return true;  // rising already: a use does nothing more
  e.count |= kRising;
  e.message.clear();
  e.activator = activator;
  for (const Q2Entity& p : s.entities)  // the area portals it names open as it starts
    if (!e.target.empty() && p.targetname == e.target && p.classname == "func_areaportal" &&
        p.state >= 0 && size_t(p.state) < s.portals.size())
      s.portals[size_t(p.state)] = 1;
  map_use_targets(s, index_of(s, e), activator);
  return true;
}

void gz_smart_water_frame(Q2Server& s, Q2Entity& e) {
  if ((e.count & kRising) == 0) return;
  const float stop = number(s, e, "health", 0);
  if (stop > 0 && top(e) >= stop) return hold(e);  // up to its height: it stays there
  bool any = false;
  float feet = 0;  // the lowest living player's feet
  for (int p = 0; p < sh::kMaxClients; p++) {
    if (!s.clients[p].active || !s.players[p].alive) continue;
    const float z = float(s.players[p].move.origin[2]) * 0.125f + s.players[p].mins.z;
    feet = any ? std::min(feet, z) : z;
    any = true;
  }
  if (!any) return hold(e);
  const float accel = number(s, e, "accel", 20);
  const float most = number(s, e, "speed", 25);
  const float gap = feet - top(e);
  float speed = gap < accel ? kCreep : gap / accel;
  speed = std::clamp(speed, kCreep, std::max(kCreep, most));
  e.speed = e.accel = e.decel = speed;
  e.arrive = q2map::kSmartRise;
  e.moved = true;
  e.split = false;
  sh::begin_move(e, e.origin + sh::Vec3f{0, 0, 4096}, kFrame);  // straight up, steered each frame
}

bool gz_smart_water_blocked(Q2Server& s, Q2Entity& e, int slot) {
  if ((e.count & kRising) == 0) return false;
  if (slot >= 0)  // a player or a monster in its way burns
    fight_hurt(s, slot, -1, {}, kBlockHarm, 0, kSqByLava);
  else
    monster_damage(s, -2 - slot, -1, kBlockHarm, {});
  return true;
}

}  // namespace shooters_edge::server
