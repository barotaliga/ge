// A party of bots splitting a campaign map's work (party.hpp).
//
// ge:summary The party's share: each stop to its nearest bot, what one knows told to all
// ge:keywords party party_share mates split nearest holder leash seen ever walk route-cost
// ge:keywords party_stay party_work helper exit_held
// ge:skill shooterbots bots
// ge:test tests/shooterbots_party_test.cpp
#include "shooterbots/party.hpp"

#include <algorithm>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooterbots/companion.hpp"
#include "shooterbots/features.hpp"
#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

namespace {

float dist2(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return d.x * d.x + d.y * d.y + d.z * d.z;
}

// A member's time to `at` (node `node`): his walk's from his last plan where it reached the node;
// else -1 (unknown: the straight distance stands in for everyone).
double walk_s(const PartyMember& m, int32_t node) {
  const ge::ai::Search& sr = m.way->search;
  if (node < 0 || static_cast<size_t>(node) >= sr.nodes()) return -1;
  return sr.reached(node) ? sr.cost(node) : -2;  // -2: his search does not reach it
}

// The member nearest `at` (node `node`) among the living (-1 none): by the walk when every living
// member's search has a time for it (one whose search does not reach it is out, unless none
// does), else by straight distance; the last holder kept while he is no more than kPartyKeep
// (kPartyKeep_s) further than the nearest (so two bots level with a stop do not trade it).
int nearest(std::span<const PartyMember> party, const std::vector<sh::Vec3f>& where, sh::Vec3f at,
            int32_t node, int was, bool& walked) {
  std::vector<double> cost(party.size(), 0);
  bool by_walk = true, any = false;
  for (size_t k = 0; k < party.size(); k++) {
    if (!party[k].alive) continue;
    cost[k] = walk_s(party[k], node);
    by_walk = by_walk && cost[k] != -1;
    any = any || cost[k] >= 0;
  }
  by_walk = by_walk && any;
  for (size_t k = 0; k < party.size(); k++)
    if (party[k].alive && !by_walk) cost[k] = sh::length(where[k] - at) / kPartyRun;
  int best = -1;
  for (size_t k = 0; k < party.size(); k++) {
    if (!party[k].alive || cost[k] < 0) continue;
    if (best < 0 || cost[k] < cost[static_cast<size_t>(best)]) best = static_cast<int>(k);
  }
  walked = by_walk;
  if (best >= 0 && was >= 0 && static_cast<size_t>(was) < party.size() &&
      party[static_cast<size_t>(was)].alive && cost[static_cast<size_t>(was)] >= 0) {
    const double keep = by_walk ? kPartyKeep_s : kPartyKeep / kPartyRun;
    if (cost[static_cast<size_t>(was)] <= cost[static_cast<size_t>(best)] + keep) return was;
  }
  return best;
}

bool near_people(const PartyOptions& opt, sh::Vec3f at) {
  if (opt.leash <= 0 || opt.humans.empty()) return true;
  for (const sh::Vec3f& h : opt.humans)
    if (dist2(h, at) <= opt.leash * opt.leash) return true;
  return false;
}

}  // namespace

void party_share(Party& p, std::span<PartyMember> party, const MapNav& nav, const BotView& s,
                 const PartyOptions& opt) {
  if (party.empty() || party[0].way == nullptr) return;
  const NavGraph& g = nav.graph;
  const size_t steps = party[0].way->steps.size();
  const size_t ents = s.sv.edicts.size();
  p.stats.shares++;
  std::vector<sh::Vec3f> where(party.size());
  for (size_t k = 0; k < party.size(); k++)
    where[k] = s.sv.edicts[static_cast<size_t>(party[k].ent)].origin;

  // What one knows, all know: the places seen, the secrets and triggers walked through.
  for (size_t k = 0; k < party.size(); k++) {
    WayThrough& a = *party[k].way;
    for (size_t j = 0; j < party.size(); j++) {
      if (j == k) continue;
      const WayThrough& b = *party[j].way;
      const size_t n = std::min(a.seen.size(), b.seen.size());
      for (size_t i = 0; i < n; i++)
        if (b.seen[i] && !a.seen[i]) {
          a.seen[i] = 1;
          p.stats.seen_told++;
        }
      const size_t m = std::min(a.steps.size(), b.steps.size());
      for (size_t i = 0; i < m; i++) {
        WayStep& st = a.steps[i];
        if (st.ever || !b.steps[i].ever) continue;
        if (st.kind == WayKind::secret || (st.kind == WayKind::trigger && !st.keyed)) {
          st.ever = true;
          p.stats.ever_told++;
        }
      }
    }
  }

  // Who takes what.
  p.step_holder.resize(steps, -1);
  p.prey_holder.resize(ents, -1);
  int64_t leashed = 0, walk_count = 0;
  for (size_t i = 0; i < steps; i++) {
    const WayStep& st = party[0].way->steps[i];
    const sh::Vec3f at = st.node >= 0 && static_cast<size_t>(st.node) < g.nodes.size()
                             ? g.nodes[static_cast<size_t>(st.node)].origin
                             : (st.lo + st.hi) * 0.5f;
    int holder = -1;
    for (size_t k = 0; k < party.size() && holder < 0; k++) {
      const WayThrough& w = *party[k].way;
      if (party[k].alive && w.step == static_cast<int>(i) && w.kind != WayKind::explore &&
          w.kind != WayKind::exit)
        holder = static_cast<int>(k);
    }
    bool walked = false;
    if (holder < 0) holder = nearest(party, where, at, st.node, p.step_holder[i], walked);
    walk_count += walked ? 1 : 0;
    // The way out is everyone's; with people playing, theirs alone.
    if (way_exit(st)) holder = opt.exit_held ? static_cast<int>(party.size()) : -1;
    if (holder >= 0 && !near_people(opt, at)) {
      holder = static_cast<int>(party.size());  // no one's: too far from the people
      leashed++;
    }
    p.step_holder[i] = static_cast<int8_t>(holder >= static_cast<int>(party.size()) ? -1 : holder);
    for (size_t k = 0; k < party.size(); k++) {
      WayThrough& w = *party[k].way;
      w.mates_steps.resize(steps, 0);
      w.mates_steps[i] = holder == static_cast<int>(party.size())       ? uint8_t{2}
                         : holder >= 0 && holder != static_cast<int>(k) ? uint8_t{1}
                                                                        : uint8_t{0};
    }
  }
  for (size_t e = static_cast<size_t>(s.sv.set.maxclients) + 1; e < ents; e++) {
    const sh::Edict& d = s.sv.edicts[e];
    const bool monster = (d.flags & sh::kFlagMonster) != 0 || d.solid == sh::kSolidSlidebox;
    int holder = -1;
    if (!d.free && monster && d.health > 0 && d.solid != sh::kSolidNot) {
      for (size_t k = 0; k < party.size() && holder < 0; k++)
        if (party[k].alive && party[k].way->prey == static_cast<int>(e))
          holder = static_cast<int>(k);
      bool walked = false;
      if (holder < 0) {
        const int32_t node = sb::nav_nearest(g, d.origin);
        holder = nearest(party, where, d.origin, node, p.prey_holder[e], walked);
      }
      walk_count += walked ? 1 : 0;
      if (holder >= 0 && !near_people(opt, d.origin)) {
        holder = static_cast<int>(party.size());
        leashed++;
      }
    }
    p.prey_holder[e] = static_cast<int8_t>(holder >= static_cast<int>(party.size()) ? -1 : holder);
    for (size_t k = 0; k < party.size(); k++) {
      WayThrough& w = *party[k].way;
      w.mates_prey.resize(ents, 0);
      w.mates_prey[e] = holder == static_cast<int>(party.size())       ? uint8_t{2}
                        : holder >= 0 && holder != static_cast<int>(k) ? uint8_t{1}
                                                                       : uint8_t{0};
    }
  }
  p.stats.leashed = leashed;
  p.stats.walked = walk_count;
}

bool party_stay(Fighter& f, const MapNav& nav, const BotView& s, int me, const PartyOptions& opt) {
  if (opt.leash <= 0 || opt.humans.empty()) return false;
  const NavGraph& g = nav.graph;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  const sh::Vec3f goal = f.objective >= 0 && static_cast<size_t>(f.objective) < g.nodes.size()
                             ? g.nodes[static_cast<size_t>(f.objective)].origin
                             : self;
  if (f.objective >= 0 && near_people(opt, goal)) return false;
  size_t h = 0;
  for (size_t k = 1; k < opt.humans.size(); k++)
    if (dist2(opt.humans[k], self) < dist2(opt.humans[h], self)) h = k;
  const float to = sh::length(self - opt.humans[h]);
  // Out of his way: a spot kPartyNear behind where he looks (he looks where he goes), else short
  // of him on this side. Never onto him, never standing at his elbow in a doorway.
  sh::Vec3f spot = opt.humans[h] + (self - opt.humans[h]) * (kPartyNear / std::max(to, 1.0f));
  if (h < opt.facing.size()) {
    const double yaw = opt.facing[h] * ge::core::kPi / 180.0;
    spot = opt.humans[h] - sh::Vec3f{static_cast<float>(ge::core::libm::cos(yaw) * kPartyNear),
                                     static_cast<float>(ge::core::libm::sin(yaw) * kPartyNear), 0};
  }
  int32_t n = sb::nav_nearest(g, spot);
  if (n < 0) n = sb::nav_nearest(g, opt.humans[h]);
  f.objective = n;
  return f.objective >= 0;
}

bool party_give_way(const BotView& s, int me, const std::vector<sh::Vec3f>& humans,
                    sh::MoveCmd& cmd) {
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  for (const sh::Vec3f& h : humans) {
    const float dx = self.x - h.x, dy = self.y - h.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d >= kPartyClear || self.z - h.z > 56 || h.z - self.z > 56) continue;
    // Straight away from him (square on him: to the side his view puts on the right).
    const double kDeg = 180.0 / ge::core::kPi;
    const double away =
        d > 1 ? ge::core::libm::atan2(dy, dx) : (cmd.angles.y / kDeg) - ge::core::kPi / 2;
    const double rel = away - cmd.angles.y / kDeg;
    cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    return true;
  }
  return false;
}

void party_work_init(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s,
                     const sh::MapSpec& spec) {
  w.way = WayThrough();
  w.wedge = Wedge();
  // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
  f.features.set(Feature::locked, false);
  hunt_init(f, s);
  f.ledge_guard = true;  // a fall into a pit with no way out ends his part
  w.way.complete = true;
  w.way.complete_until = 1e9;  // the people decide when the map ends
  way_complete_init(f, nav, s, spec);
  way_init(w.way, f, nav, s, spec);
}

void party_work_plan(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s, int me,
                     double t, const PartyOptions& opt) {
  const int64_t plans = w.way.stats.plans;
  way_plan(w.way, f, nav, s, me, t);
  // Kept to the people each frame, counted once a plan.
  if (party_stay(f, nav, s, me, opt) && w.way.stats.plans != plans) w.stays++;
}

void party_work_command(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s, int me,
                        double t, const PartyOptions& opt, sh::MoveCmd& cmd) {
  way_dive(w.way, nav, s, me, cmd);
  way_reach_in(w.way, nav, s, me, cmd);
  if (way_touch(w.way, s, me, cmd)) {
    f.bot.yaw = cmd.angles.y;  // he turned to it: his keys follow his view
  } else {
    // A climber is held to the ladder by his pilot: a shove aside takes him off it.
    if (!f.bot.pilot.pilot.climbing) unwedge(w.wedge, s, me, t, cmd, way_held(f.bot.pilot.pilot));
    way_edge(nav, f, s, me, cmd);
  }
  if (party_give_way(s, me, opt.humans, cmd)) w.gave_way++;
  if (exits_keep_off(f, s, me, cmd)) w.exits_turned++;
}

void party_round(PartyRound& r, const MapNav* nav, const BotView& s, std::span<const bool> bot,
                 std::span<WayThrough* const> helpers) {
  r.members.clear();
  r.people.humans.clear();
  r.people.facing.clear();
  r.people.leash = kPartyLeash;
  r.people.exit_held = true;
  const int slots = std::min({static_cast<int>(s.sv.clients.size()), static_cast<int>(bot.size()),
                              static_cast<int>(helpers.size())});
  for (int i = 0; i < slots; i++) {
    if (!s.sv.clients[static_cast<size_t>(i)].active) continue;
    const sh::Edict& e = s.sv.edicts[static_cast<size_t>(i) + 1];
    if (!bot[static_cast<size_t>(i)]) {
      r.people.humans.push_back(e.origin);
      r.people.facing.push_back(s.world.entities[static_cast<size_t>(i) + 1].view_angles.y);
    } else if (WayThrough* way = helpers[static_cast<size_t>(i)]; way != nullptr) {
      r.members.push_back({way, i + 1, e.health > 0});
    }
  }
  if (!r.members.empty() && nav != nullptr) party_share(r.share, r.members, *nav, s, r.people);
}

}  // namespace ge::shooterbots
