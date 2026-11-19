// THE BRIDGES AND DOORS ON THE WAY (fighter_parts.hpp): the bridges as this player sees them (a
// bridge seen gone, a floor door back), the links onto those still in shut and the way planned
// again; and a door only a button opens, shut on the route planned, its button gone to first.
//
// ge:summary The fighter's bridges and doors: seen in or gone, a shut door's button first
// ge:keywords fighter bridge bridgeseen watch_bridges bridge_seen_gone floor-door door locked
// ge:keywords door_errand door_resting locked_errands button shut-links noroute no-route
// ge:keywords bridge-errand goal-set-aside
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include <cmath>
#include <cstdint>
#include <vector>

#include "shooter/brink.hpp"
#include "shooter/nav.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/buttons.hpp"
#include "shooters_edge/bots/fighter_parts.hpp"
#include "shooters_edge/bots/floor_trap.hpp"

namespace shooters_edge::bots::fighter_parts {

namespace {

constexpr double kBridgeSpare_s = 2.5;  // a bridge left this long before it goes back, s
constexpr float kGoneSight = 800;       // `bridgeseen`: a bridge's node looked at this near, units
constexpr float kGoneDrop = 64;         // and no floor this far under it: gone
constexpr double kFloorBack_s = 10;     // `bridgeseen`: a floor door seen away, unseen since, back

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// (`bridgeseen`) Whether bridge `b`, left out of what this player is sent, is SEEN gone: one of its
// nodes in sight within `kGoneSight` with no floor under it (the lava where it stands out). A
// client keeps a mover it is no longer sent where it last saw it: dm2's red armour's bridge, seen
// out and gone back into its wall out of view, stood out in the bot's mind, and the route walked
// off the lip into the pit (22 of 25 lava deaths there in 8 matches).
bool bridge_seen_gone(const Fighter::Bridge& b, const ge::shooter::BotView& s, int me,
                      sh::Vec3f eye, const sh::NavGraph& g) {
  if (!s.world.entities[static_cast<size_t>(b.ent)].free) return false;  // sent: its place stands
  for (const int32_t n : b.nodes) {
    const sh::Vec3f o = g.nodes[static_cast<size_t>(n)].origin;
    if (dist(o, eye) > kGoneSight) continue;
    const sh::HullTrace look = sh::trace_move(s.sv, eye, {}, {}, o, sh::kTraceNoMonsters, me);
    if (look.fraction < 1) continue;
    const sh::HullTrace down =
        sh::trace_move(s.sv, o, sh::kPlayerMins, sh::kPlayerMaxs, {o.x, o.y, o.z - kGoneDrop},
                       sh::kTraceNoMonsters, me);
    if (down.fraction >= 1 || sh::over_harm(s.sv, o, o, me)) return true;
  }
  return false;
}

// A DOOR ON THE WAY (`locked`): the doors only something else opens, while they rest shut. When
// the route planned crosses one a known button opens, the button is gone to (round the door: no
// route through it meanwhile) and pressed (walked into, or shot: button_poke_cmd) before the goal,
// as a person hits the button by a shut door (dm2's yellow armour and its teleporter, behind a
// touch button's doors). A button no way reaches but through its own door (pressed from the far
// side) leaves that door off the routes a while. A door no known button opens (a trigger walked
// into opens it, or nothing does) is waited at as before (the pilot bars it after `wait_s`).
bool door_resting(const Fighter& f, const ge::shooter::BotView& s, int ent) {
  for (size_t k = 0; k < f.locked.size(); k++)
    if (f.locked[k] == ent)
      return dist(s.world.entities[static_cast<size_t>(ent)].origin, f.locked_rest[k]) < 1;
  return false;
}

}  // namespace

// The bridges as seen now (fighter.hpp `Bridge`): on a change, the links onto those still in are
// shut and the way planned again.
void watch_bridges(Fighter& f, const ge::shooter::BotView& s, int me) {
  if (f.bridges.empty()) return;
  bool changed = !f.bridges_seen;
  f.bridges_seen = true;
  for (Fighter::Bridge& b : f.bridges) {
    if (b.ent < 0 || static_cast<size_t>(b.ent) >= s.world.entities.size()) continue;
    const sh::Vec3f o = s.world.entities[static_cast<size_t>(b.ent)].origin;
    bool in = false;
    if (b.floor) {
      in = dist(o, b.spawn) >= 1;  // a floor door: there only at rest
      // (`bridgeseen`) Out of sight (not sent), seen away: back once its time out is over (a
      // client keeps it where it last saw it; one stood away in a bot's mind for 48 s).
      if (f.features[sh::Feature::bridgeseen]) {
        if (in && !s.world.entities[static_cast<size_t>(b.ent)].free) b.out_since = s.sv.time;
        if (in && s.world.entities[static_cast<size_t>(b.ent)].free && b.out_since >= 0 &&
            s.sv.time - b.out_since > kFloorBack_s)
          in = false;
      }
      // (`floortrap`) or a mate just called it gone (`trap`), unseen yet
      if (f.features[sh::Feature::floortrap] && s.sv.time - f.trap_told < kTrapTold_s) in = true;
    } else {
      // A bridge: a floor only all the way out where the map drew it, and not in the last
      // moments of its wait (it goes back from under whoever is on it: dm2's red armour's).
      const bool out = dist(o, sh::Vec3f{}) < 1;
      if (!out) b.out_since = -1;
      if (out && b.out_since < 0) b.out_since = s.sv.time;
      in = !out || (b.wait > 0 && s.sv.time - b.out_since > b.wait - kBridgeSpare_s);
    }
    if (!in && f.features[sh::Feature::bridgeseen]) {
      const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
      const sh::Vec3f eye{self.origin.x, self.origin.y,
                          self.origin.z + f.bot.senses.set.view_height};
      if (bridge_seen_gone(b, s, me, eye, *f.bot.pilot.pilot.g)) {
        in = true;
        f.stats.bridges_seen_gone += b.in ? 0 : 1;
      }
    }
    changed = changed || in != b.in;
    b.in = in;
  }
  if (!changed) return;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  std::vector<uint8_t> on(g.nodes.size(), 0);
  for (const Fighter::Bridge& b : f.bridges)
    if (b.in)
      for (const int32_t n : b.nodes) on[static_cast<size_t>(n)] = 1;
  std::vector<sh::NavExtraLink>& shut = f.bot.pilot.pilot.shut_links;
  shut.clear();
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (on[static_cast<size_t>(l.to)] && !on[a])
        shut.push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
    }
  }
  floor_trap_shut(f, shut);  // the teleporters onto a floor trap's floor while it is away
  f.goal_node = -1;          // planned again
  if (f.features[sh::Feature::bridgeseen]) f.reach_at = -1;  // and where it can get to
}

// Whether this fighter runs the doors' errands: `locked`, and its own player (a companion and
// the campaign's leader keep their own doors: companion.cpp, way_through.cpp).
bool locked_errands(const Fighter& f) {
  return f.features[sh::Feature::locked] && f.escort.leader < 0;
}

void door_errand(Fighter& f, const ge::shooter::BotView& s, double t) {
  sh::Pilot& p = f.bot.pilot.pilot;
  p.shut.clear();
  if (f.door_button >= 0) {
    const bool open = !door_resting(f, s, f.door_ent);
    // No way to the button but through its door: that door off the routes a while.
    const bool no_way = p.mode == sh::PilotMode::idle &&
                        f.goal_node == f.button_spots[static_cast<size_t>(f.door_button)].node &&
                        p.route.empty();
    if (!open && !no_way && t < f.door_until) {
      p.shut.push_back(f.door_ent);  // round it to the button
      return;
    }
    if (no_way) {
      f.door_avoid = f.door_ent;
      f.door_avoid_until = t + kDoorAvoid_s;
      f.reach_at = -1;  // where it can get to, worked out again without the door
    }
    f.door_button = -1;
    f.door_ent = -1;
    f.poke = -1;
    f.goal_node = -1;  // the goal planned again, through the door now open (or round it)
  }
  if (f.door_avoid >= 0 && t < f.door_avoid_until && door_resting(f, s, f.door_avoid))
    p.shut.push_back(f.door_avoid);
  // A shut one on the route planned: its button first.
  const sh::NavGraph& g = *p.g;
  for (size_t i = p.k; i + 1 < p.route.size(); i++) {
    const sh::NavNode& n = g.nodes[static_cast<size_t>(p.route[i])];
    int32_t mover = -1;
    for (int32_t j = 0; j < n.links; j++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(j)];
      if (l.to == p.route[i + 1]) mover = l.mover;
    }
    if (mover < 0 || !door_resting(f, s, mover)) continue;
    for (size_t b = 0; b < f.button_spots.size(); b++) {
      const ButtonSpot& bs = f.button_spots[b];
      bool opens = false;
      for (const int d : bs.doors) opens |= d == mover;
      if (!opens || bs.node < 0 || (f.reach_at >= 0 && !f.reach.reached(bs.node))) continue;
      f.door_button = static_cast<int32_t>(b);
      f.door_ent = mover;
      f.door_until = t + kDoorErrand_s;
      f.poke = f.door_button;
      f.poke_until = f.door_until;
      f.stats.door_errands++;
      p.shut.push_back(mover);
      return;
    }
  }
}

// (`noroute`) THE GOAL NO ROUTE LEADS TO. The goal's reach (fighter.cpp, `reach`) counts links the
// pilot's plan leaves off (a jump missed twice, a gap trick given up, a door found shut), so a goal
// can be chosen that the pilot has no route to: it stood idle on `no route` until the goal changed
// (dm2's red armour's ledge, its bridge in: 38 % of the time a bot went for it). Found idle that way
// at a think (the pilot had planned and failed since the last): a bridge still in on the way the
// map would give (no link shut) whose button a way reaches is run out first, as a person hits the
// button by the pit; else the item is set aside as a goal for kNoRouteRest_s and chosen again.
void noroute_watch(Fighter& f, sh::Vec3f me, double t, int32_t no_route_to, int32_t item,
                   ge::ai::Why* why) {
  constexpr double kNoRouteRest_s = 10;  // the item set aside this long
  constexpr double kBridgeErrand_s = 8;  // a bridge's button gone for at most this long
  sh::Pilot& p = f.bot.pilot.pilot;
  if (f.bridge_button >= 0) {
    const bool out = f.bridge_k >= 0 && !f.bridges[static_cast<size_t>(f.bridge_k)].in;
    if (!out && t < f.bridge_until) return;
    f.bridge_button = -1;
    f.bridge_k = -1;
    f.poke = -1;
    f.goal_node = -1;  // the goal planned again, over the bridge now out
  }
  if (item < 0 || no_route_to < 0) return;
  f.stats.noroute++;
  const sh::NavGraph& g = *p.g;
  const int32_t from = sh::nav_nearest(g, me);
  int bridges_on = 0, buttons = 0;
  size_t way_n = 0;
  const auto note = [&](const char* chosen, const char* because) {
    if (!why) return;
    why->decide("noroute");
    why->fact("item", item);
    why->fact("to", no_route_to);
    why->fact("way", static_cast<double>(way_n));
    why->fact("bridges_in", bridges_on);
    why->fact("buttons", buttons);
    why->choose(chosen, because);
  };
  if (from >= 0) {
    // The way the pilot would plan with the bridges' links open: every door it bars and every link
    // it leaves off kept (pilot_plan.cpp `plan_way`), the bridges' shut links alone lifted.
    std::vector<int32_t> barred;
    for (const int32_t b : p.barred)
      if (b >= 0) barred.push_back(b);
    for (const int32_t b : p.shut)
      if (b >= 0) barred.push_back(b);
    std::vector<sh::NavExtraLink> cut = p.cut;
    for (size_t q = 0; q < p.gaps_off.size(); q++)
      if (p.gaps_off_s[q] > p.clock_s) cut.push_back(p.gaps_off[q]);
    const sh::NavRoute way = sh::nav_route(g, from, no_route_to, p.search, 1, barred, cut, p.set.jumper);
    way_n = way.nodes.size();
    for (size_t k = 0; k < f.bridges.size() && !way.nodes.empty(); k++) {
      const Fighter::Bridge& br = f.bridges[k];
      if (!br.in || br.floor) continue;
      bool on = false;
      for (const int32_t n : way.nodes)
        for (const int32_t bn : br.nodes) on = on || n == bn;
      if (!on) continue;
      bridges_on++;
      for (size_t b = 0; b < f.button_spots.size(); b++) {
        const ButtonSpot& bs = f.button_spots[b];
        bool runs = false;
        for (const int d : bs.doors) runs = runs || d == br.ent;
        if (runs) buttons++;
        if (!runs || bs.node < 0 || (f.reach_at >= 0 && !f.reach.reached(bs.node))) continue;
        f.bridge_button = static_cast<int32_t>(b);
        f.bridge_k = static_cast<int32_t>(k);
        f.bridge_until = t + kBridgeErrand_s;
        f.poke = f.bridge_button;
        f.poke_until = f.bridge_until;
        f.stats.bridge_errands++;
        note("bridge", "a bridge in on the way: its button first");
        return;
      }
    }
  }
  note("set-aside", "no route and no bridge's button to run one: another goal a while");
  f.noroute_item = item;
  f.noroute_until = t + kNoRouteRest_s;
  if (f.goal_item == item) {
    f.goal_item = -1;
    f.goal_node = -1;
  }
  f.decided_at = -1e9;  // chosen again now
}

}  // namespace shooters_edge::bots::fighter_parts
