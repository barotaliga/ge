// Ground Zero's tesla in the monsters' fight (q2_monster_ai.hpp), only under the pack's switch.
//
// A tesla that hurts a monster marks its field (128 to each side, up to 128 above its floor) as a
// bad area for 30 s: a monster never steps into one, and one inside only steps away from the tesla.
// The monster it hurt turns on it (when the area is new or it had no enemy; a medic leaves its
// body), minds no other blow while it fights it, and its shots strike it (the medic's bolt for 3)
// until the tesla bursts; then it goes back to the player it fought.
//
// ge:summary Ground Zero's tesla in the monsters' fight: the bad area, the tesla as a foe
// ge:keywords q2 ground-zero rogue monsters tesla bad-area foe infighting
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_base_monsters_test.cpp
#include <iterator>
#include <string>
#include <vector>

#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"

namespace shooters_edge::server {

namespace monster_ai {

namespace {

bool inside(const Q2BadArea& a, sh::Vec3f p) {
  return p.x >= a.lo.x && p.x <= a.hi.x && p.y >= a.lo.y && p.y <= a.hi.y && p.z >= a.lo.z &&
         p.z <= a.hi.z;
}

float flat_distance(sh::Vec3f a, sh::Vec3f b) {
  return sh::length(sh::Vec3f{a.x - b.x, a.y - b.y, 0});
}

}  // namespace

bool gz_tesla_alive(const Q2Server& s, int i) {
  if (i < 0 || size_t(i) >= std::size(s.missiles)) return false;
  const Q2Missile& m = s.missiles[size_t(i)];
  return m.live && m.kind == SqShot::tesla && m.health > 0;
}

sh::Vec3f gz_tesla_at(const Q2Server& s, int i, float up) {
  if (i < 0 || size_t(i) >= std::size(s.missiles)) return {};
  return s.missiles[size_t(i)].origin + sh::Vec3f{0, 0, up};
}

bool gz_tesla_holds(Q2Server& s, Q2Monster& m) {
  return foe_missile(m.enemy) >= 0 && foe_alive(s, m.enemy);
}

bool gz_bad_area_refuses(const Q2Server& s, const Q2Monster& m, sh::Vec3f move) {
  if (s.bad_areas.empty()) return false;
  const sh::Vec3f from = s.entities[size_t(m.entity)].origin;
  const sh::Vec3f to = from + move;
  for (const Q2BadArea& a : s.bad_areas) {
    if (s.level_time > a.until || !inside(a, to)) continue;
    if (!inside(a, from)) return true;                                     // never in
    if (flat_distance(to, a.at) < flat_distance(from, a.at)) return true;  // inside: only away
  }
  return false;
}

}  // namespace monster_ai

void gz_tesla_bad_area(Q2Server& s, int i, int e, sh::Vec3f lo, sh::Vec3f hi) {
  if (s.opt.pack != Q2Pack::ground_zero || i < 0 || size_t(i) >= std::size(s.missiles)) return;
  const sh::Vec3f at = s.missiles[size_t(i)].origin;
  std::erase_if(s.bad_areas, [&](const Q2BadArea& a) { return s.level_time > a.until; });
  bool fresh = true;
  for (Q2BadArea& a : s.bad_areas)
    if (a.at.x == at.x && a.at.y == at.y && a.at.z == at.z) {
      a.until = s.level_time + 30;  // its own: kept fresh
      fresh = false;
    }
  if (fresh) s.bad_areas.push_back({lo, hi, at, s.level_time + 30});
  // The monster it hurt turns on it.
  const int mi = monster_index(s, e);
  if (mi < 0) return;
  Q2Monster& m = s.monsters[size_t(mi)];
  const int tesla = monster_ai::foe_of_missile(i);
  if (m.dead || m.enemy == tesla || !monster_ai::gz_tesla_alive(s, i) || (!fresh && m.enemy != -1))
    return;
  if (m.patient >= 0) {  // a medic leaves the body
    s.monsters[size_t(m.patient)].claimed = false;
    m.patient = -1;
    m.revive_at = 0;
  }
  if (m.enemy >= 0) m.old_enemy = m.enemy;  // a player, gone back to when the tesla is
  monster_ai::found(s, m, tesla, false);
}

}  // namespace shooters_edge::server
