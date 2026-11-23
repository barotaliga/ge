// A party of bots splitting a campaign map's work (party.hpp).
//
// ge:summary The party's share: each stop to its nearest bot, what one knows told to all
// ge:keywords party party_share mates split nearest holder leash seen ever
// ge:skill shooterbots bots
// ge:test tests/shooterbots_party_test.cpp
#include "shooterbots/party.hpp"

#include <algorithm>
#include <cstddef>

namespace ge::shooterbots {

namespace {

float dist2(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return d.x * d.x + d.y * d.y + d.z * d.z;
}

// The member nearest `at` among the living (-1 none), the last holder kept while he is no more
// than kPartyKeep further than the nearest (so two bots level with a stop do not trade it).
int nearest(std::span<const PartyMember> party, const std::vector<sh::Vec3f>& where, sh::Vec3f at,
            int was) {
  int best = -1;
  float best_d = 0;
  for (size_t k = 0; k < party.size(); k++) {
    if (!party[k].alive) continue;
    const float d = dist2(where[k], at);
    if (best < 0 || d < best_d) {
      best = static_cast<int>(k);
      best_d = d;
    }
  }
  if (was >= 0 && static_cast<size_t>(was) < party.size() &&
      party[static_cast<size_t>(was)].alive) {
    if (sh::length(where[static_cast<size_t>(was)] - at) <=
        sh::length(where[static_cast<size_t>(best)] - at) + kPartyKeep)
      return was;
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
  int64_t leashed = 0;
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
    if (holder < 0) holder = nearest(party, where, at, p.step_holder[i]);
    if (way_exit(st)) holder = -1;  // the way out is everyone's
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
      if (holder < 0) holder = nearest(party, where, d.origin, p.prey_holder[e]);
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
}

}  // namespace ge::shooterbots
