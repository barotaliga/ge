// Keeping an area safe (spam.hpp): the spots, where each is fired from, the rocket.
//
// ge:summary Rockets at the ways in: a holder's at the spots near his area, anyone's at news
// ge:keywords spam spam_init spam_plan spam_fire teleport-destination power-spawn
// ge:keywords spam_news_look spam_news_fire
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
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/bots/team.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;

constexpr int32_t kWRockets = 32;  // the rocket launcher's W_ bit (games/shooters_edge/qc)
constexpr float kMateClear = 300;  // never a spot a teammate is believed this near, units
constexpr float kNewsOn = 0.995f;  // the view this near the way in (the cosine: about 6 degrees)

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool clear(const server::RulesCommon& s, sh::Vec3f from, sh::Vec3f to) {
  return sh::trace_move(s.sv, from, {}, {}, to, sh::kTraceNoMonsters, -1).fraction >= 1;
}

// Per spot, the node within the leash of `area` that sees it from the right range, nearest the
// area (-1 where none does).
void find_from(Fighter& f, const server::RulesCommon& s, int32_t area) {
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

bool spam_plan(Fighter& f, const server::RulesCommon& s, const Stats& st, double t) {
  f.spam_node = f.spam_spot = -1;
  const int32_t area = f.mind.claim;
  if (!f.team || f.camp != MapSense::expert || f.role != Role::hold || area < 0) return false;
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

bool spam_fire(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
               sh::MoveCmd& cmd) {
  if (f.spam_node < 0 || f.spam_spot < 0 || f.bot.aiming) return false;
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
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
  f.bot.set_by = "spam";
  if (t - f.spam_fired < kSpamGap_s) return false;
  cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  f.spam_fired = t;
  f.stats.spam_shots++;
  f.team->stats.spam_shots++;
  return true;
}

void spam_news_look(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t) {
  f.news_on = false;
  if (!f.plan || f.mode != FightMode::roam || f.bot.aiming || f.escort.leader >= 0) return;
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  if (self.health <= 0 || self.waterlevel >= 2 || st.weapon != kWRockets) return;
  const float spare = kNewsRocketsMost - static_cast<float>(f.traits[sh::Trait::aggression]);
  if (st.rockets < spare) return;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const sh::Vec3f me_at = self.origin;
  const int32_t area = plan_area_of(*f.plan, sh::nav_nearest(g, me_at));
  if (area < 0) return;
  // The news: where an enemy was seen, heard or called out lately, or a teammate died, and when.
  struct News {
    sh::Vec3f at;
    double t;
  };
  std::vector<News> news;
  const Mind& mn = f.mind;
  if (t - mn.sighted_t < kNews_s) news.push_back({mn.sighted_at, mn.sighted_t});
  if (t - mn.enemy_t < kNews_s) news.push_back({mn.enemy_at, mn.enemy_t});
  for (const sh::Contact& c : f.bot.senses.contacts)
    if (ge::ai::known(c.heard) && ge::ai::age(c.heard, t) < kNews_s)
      news.push_back({{static_cast<float>(c.heard.x), static_cast<float>(c.heard.y),
                       static_cast<float>(c.heard.z)},
                      c.heard.at});
  for (const auto& [where, when] : f.danger)
    if (t - when < kNews_s) news.push_back({where, when});
  if (news.empty()) return;
  const sh::Vec3f eye{me_at.x, me_at.y, me_at.z + 22};
  // The way in nearest the freshest news beyond it, seen from here at a rocket's range.
  float best = 0;
  bool found = false;
  sh::Vec3f spot;
  double spot_t = 0;
  for (const int32_t e : f.plan->areas[static_cast<size_t>(area)].entries) {
    const sh::Vec3f o = g.nodes[static_cast<size_t>(e)].origin;
    const sh::Vec3f aim{o.x, o.y, o.z - 20};  // the floor there: the blast reaches the feet
    const float d = dist(aim, eye);
    if (d < kSpamNear || d > kSpamFar) continue;
    bool mate = false;
    for (const Mate& m : mn.mates)
      mate |= t - m.at_t < kExpertMateFresh_s && dist(m.at, o) < kMateClear;
    if (mate) continue;
    int seen = -1;  // the line to it: worked out once, when some news makes it worth it
    for (const News& n : news) {
      const float beyond = dist(n.at, o);
      if (beyond > kNewsNear || dist(n.at, me_at) <= dist(o, me_at)) continue;
      if (found && beyond >= best) continue;
      if (seen < 0) seen = clear(s, eye, aim) ? 1 : 0;
      if (seen == 0) break;
      found = true;
      best = beyond;
      spot = aim;
      spot_t = n.t;
    }
  }
  if (!found) return;
  if (spot_t != f.news_t) {
    f.news_t = spot_t;
    f.news_shots = 0;
  }
  if (f.news_shots >= kNewsShots) return;
  f.news_on = true;
  f.news_spot = spot;
  f.bot.look = true;
  f.bot.look_at = spot;
}

bool spam_news_fire(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                    sh::MoveCmd& cmd) {
  if (!f.news_on || st.weapon != kWRockets || t - f.spam_fired < kSpamGap_s) return false;
  const sh::Vec3f o = s.world.entities[static_cast<size_t>(me)].origin;
  const sh::Vec3f to = f.news_spot - sh::Vec3f{o.x, o.y, o.z + 22};
  const float d = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
  if (d < 1) return false;
  const sh::Vec3f fwd = sh::angle_vectors(cmd.angles).forward;
  // On it within a few degrees: the blast is wide, the hands need not be exact.
  if ((fwd.x * to.x + fwd.y * to.y + fwd.z * to.z) / d < kNewsOn) return false;
  cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  f.spam_fired = t;
  f.news_shots++;
  f.stats.spam_shots++;
  if (f.team) f.team->stats.spam_shots++;
  return true;
}

}  // namespace shooters_edge::bots
