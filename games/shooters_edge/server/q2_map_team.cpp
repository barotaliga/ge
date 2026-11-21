// A team of Quake II movers moved as one (q2_map_kinds.hpp states the rule).
//
// ge:summary Quake II's mover teams: one part blocked undoes the whole team's move this frame
// ge:keywords q2 movers team-movers door-team train-team blocked undo
// ge:skill shooter
// ge:test tests/shooters_edge_q2_map_test.cpp
#include <tuple>

#include "shooters_edge/server/q2_map_kinds.hpp"

namespace shooters_edge::server::q2map {

namespace {
constexpr float kStep = 0.1f;  // one 10 Hz frame
}  // namespace

void team_begin(const Q2Server& s, TeamFrame& t, int master) {
  t = {};
  t.master = master;
  bool moving = false;  // a team at rest has nothing to undo
  for (const Q2Entity& e : s.entities)
    moving = moving || (e.team_master == master && e.arrive != kNoAct);
  if (!moving) return;
  for (const Q2Entity& e : s.entities) {
    t.where.emplace_back(e.origin, e.angles);
    if (e.team_master == master) t.parts.push_back(e);
  }
  for (const Q2Player& p : s.players) {
    t.players.emplace_back(p.move, p.view_angles);
    t.ground.push_back(p.ground);
  }
}

void team_undo(Q2Server& s, TeamFrame& t) {
  for (size_t i = 0; i < s.entities.size() && i < t.where.size(); i++)
    std::tie(s.entities[i].origin, s.entities[i].angles) = t.where[i];
  size_t k = 0;
  for (Q2Entity& e : s.entities)
    if (e.team_master == t.master && k < t.parts.size()) e = t.parts[k++];
  for (size_t i = 0; i < t.players.size(); i++) {
    std::tie(s.players[i].move, s.players[i].view_angles) = t.players[i];
    s.players[i].ground = t.ground[i];
  }
  for (Q2Entity& e : s.entities)
    if (e.team_master == t.master && e.think_at > 0) e.think_at += kStep;
  t.blocked = true;
}

}  // namespace shooters_edge::server::q2map
