// Keeping an area safe (spam.hpp): the spots, where each is fired from, the rocket.
//
// ge:summary An expert holder's rockets at the spots enemies come in by near his area
// ge:keywords spam spam_init spam_plan spam_fire teleport-destination power-spawn
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/spam.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "progs/entity_text.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/team.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;

constexpr int32_t kWRockets = 32;  // the rocket launcher's W_ bit (games/shooters_edge/qc)
constexpr float kMateClear = 300;  // never a spot a teammate is believed this near, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool clear(const server::QcServer& s, sh::Vec3f from, sh::Vec3f to) {
  return sh::trace_move(s.sv, from, {}, {}, to, sh::kTraceNoMonsters, -1).fraction >= 1;
}

// Per spot, the node within the leash of `area` that sees it from the right range, nearest the
// area (-1 where none does).
void find_from(Fighter& f, const server::QcServer& s, int32_t area) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const sh::Vec3f home = f.items[static_cast<size_t>(area)].at;
  f.spam_from.assign(f.spam_spots.size(), -1);
  for (size_t k = 0; k < f.spam_spots.size(); k++) {
    const sh::Vec3f spot = f.spam_spots[k];
    if (dist(spot, home) < kSpamNear) continue;  // the area's own door: held, not fired at
    float best = kLeash;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      const float d_home = dist(o, home);
      if (d_home >= best) continue;
      const float d = dist(o, spot);
      if (d < kSpamNear || d > kSpamFar) continue;
      if (!clear(s, {o.x, o.y, o.z + 22}, spot)) continue;
      best = d_home;
      f.spam_from[k] = static_cast<int32_t>(n);
    }
  }
  f.spam_for = area;
}

}  // namespace

void spam_init(Fighter& f, const char* entities) {
  f.spam_spots.clear();
  f.spam_from.clear();
  f.spam_for = f.spam_node = f.spam_spot = -1;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (entities && pg::parse_entity_text(entities, blocks, error)) {
    for (const pg::EntityBlock& b : blocks) {
      const char* cn = pg::block_value(b, "classname");
      sh::Vec3f at;
      if (cn && std::strcmp(cn, "info_teleport_destination") == 0 &&
          parse_vec(pg::block_value(b, "origin"), at))
        f.spam_spots.push_back({at.x, at.y, at.z - 8});  // the floor under where one comes out
    }
  }
  for (const ItemSpot& it : f.items)
    if (it.kind == ItemKind::quad || it.kind == ItemKind::pent)
      f.spam_spots.push_back({it.at.x, it.at.y, it.at.z + 8});
}

bool spam_plan(Fighter& f, const server::QcServer& s, const Stats& st, double t) {
  f.spam_node = f.spam_spot = -1;
  const int32_t area = f.mind.claim;
  if (!f.team || f.sense != MapSense::expert || f.role != Role::hold || area < 0) return false;
  if (f.mode != FightMode::roam || f.spam_spots.empty()) return false;
  if (st.rockets < kSpamRockets || (st.items & kWRockets) == 0) return false;
  const ItemSpot& it = f.items[static_cast<size_t>(area)];
  // Its item gone, timed, and not due for a while: a moment to spare.
  if (it.present || !it.timed || it.back_at - t < kSpamBack_s) return false;
  if (f.spam_for != area) find_from(f, s, area);
  // The spot whose firing place is nearest the area, and no teammate believed near it; the spots
  // taken in turn, one a few rockets, so every way in is kept.
  const size_t n = f.spam_spots.size();
  const size_t start = static_cast<size_t>(f.stats.spam_shots / 3) % n;
  for (size_t j = 0; j < n; j++) {
    const size_t k = (start + j) % n;
    const int32_t from = f.spam_from[k];
    if (from < 0 || (f.reach_at >= 0 && !f.reach.reached(from))) continue;
    bool mate = false;
    for (const Mate& m : f.mind.mates)
      mate |= t - m.at_t < kExpertMateFresh_s && dist(m.at, f.spam_spots[k]) < kMateClear;
    if (mate) continue;
    f.spam_node = from;
    f.spam_spot = static_cast<int32_t>(k);
    return true;
  }
  return false;
}

bool spam_fire(Fighter& f, const server::QcServer& s, int me, const Stats& st, double t,
               sh::MoveCmd& cmd) {
  if (f.spam_node < 0 || f.spam_spot < 0 || f.bot.aiming) return false;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  if (dist(self, g.nodes[static_cast<size_t>(f.spam_node)].origin) > 96) return false;
  if (st.weapon != kWRockets) {
    if (t - f.impulse_at >= 0.5) f.want_weapon = 7;  // the launcher's number
    return false;
  }
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  const sh::Vec3f spot = f.spam_spots[static_cast<size_t>(f.spam_spot)];
  const sh::Vec3f d = spot - eye;
  const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
  cmd.angles.y = static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
  cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
  cmd.forward = cmd.side = 0;
  if (t - f.spam_fired < kSpamGap_s) return false;
  cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  f.spam_fired = t;
  f.stats.spam_shots++;
  f.team->stats.spam_shots++;
  return true;
}

}  // namespace shooters_edge::bots
