// A team of fighters (team.hpp): the key areas off the map's items, and the roles each plan.
//
// ge:summary A team of fighters: the key areas and the roles planned each half second
// ge:keywords team_init team_plan roles hold rush carry free quad-timer claims strength
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/team.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::bots {

namespace {

constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit (games/shooters_edge/qc/common/defs.qc)

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool alive(const server::QcServer& s, const Fighter& f) {
  return f.slot >= 0 && s.sv.clients[static_cast<size_t>(f.slot)].active &&
         s.sv.edicts[static_cast<size_t>(f.slot) + 1].health > 0;
}

sh::Vec3f origin_of(const server::QcServer& s, const Fighter& f) {
  return s.sv.edicts[static_cast<size_t>(f.slot) + 1].origin;
}

}  // namespace

void team_init(Team& t) {
  t.friends = 0;
  for (const Fighter* f : t.members)
    if (f->slot >= 0 && f->slot < 32) t.friends |= 1u << f->slot;
  t.hold_item = t.quad_item = -1;
  t.claimed.clear();
  if (t.members.empty()) return;
  const std::vector<ItemSpot>& items = t.members[0]->items;
  t.claimed.assign(items.size(), -1);
  // The held area: the best armour's.
  int best = -1;
  for (size_t i = 0; i < items.size(); i++) {
    const ItemKind k = items[i].kind;
    const int rank = k == ItemKind::armor3   ? 3
                     : k == ItemKind::armor2 ? 2
                     : k == ItemKind::armor1 ? 1
                                             : 0;
    if (rank > best && rank > 0 && items[i].node >= 0) {
      best = rank;
      t.hold_item = static_cast<int32_t>(i);
    }
    if (k == ItemKind::quad && items[i].node >= 0 && t.quad_item < 0)
      t.quad_item = static_cast<int32_t>(i);
  }
}

void team_plan(Team& t, const server::QcServer& s) {
  const double now = s.sv.time;
  for (Fighter* f : t.members) t.stats.role_commands[static_cast<int>(f->role)]++;
  if (now < t.next_plan || t.members.empty()) return;
  t.next_plan = now + kPlan_s;
  t.stats.plans++;
  std::fill(t.claimed.begin(), t.claimed.end(), -1);

  std::vector<Fighter*> live;
  for (Fighter* f : t.members) {
    if (alive(s, *f))
      live.push_back(f);
    else
      f->role = Role::free;
  }
  if (live.empty()) return;
  const std::vector<ItemSpot>& items = t.members[0]->items;
  ge::ai::Why* why = nullptr;
  for (Fighter* f : live)
    if (f->bot.pilot.why) why = f->bot.pilot.why->follow(f->slot, f->bot.pilot.command);

  // The carrier: whoever holds the quad.
  Fighter* carrier = nullptr;
  for (Fighter* f : live)
    if (static_cast<int32_t>(read_stats(s, f->slot + 1).items) & kItQuad) carrier = f;

  // The holder: the strongest, the one holding now kept on a near tie.
  Fighter* holder = nullptr;
  float top = -1;
  for (Fighter* f : live) {
    if (f == carrier) continue;
    float v = fighter_strength(read_stats(s, f->slot + 1));
    if (f->role == Role::hold) v *= 1.25f;
    if (v > top) {
      top = v;
      holder = f;
    }
  }

  // The rush: the quad due soon (or there) and nobody on the team carrying it.
  std::vector<Fighter*> rushers;
  if (t.quad_item >= 0 && !carrier) {
    const ItemSpot& q = items[static_cast<size_t>(t.quad_item)];
    // Every member times it; the team takes the earliest it is due back.
    double due = 1e30;
    for (const Fighter* f : t.members) {
      const ItemSpot& mine = f->items[static_cast<size_t>(t.quad_item)];
      due = std::min(due, mine.present ? now : mine.back_at);
    }
    if (due - now <= kRushAhead_s) {
      std::vector<std::pair<float, Fighter*>> by;
      for (Fighter* f : live)
        if (f != holder || live.size() <= 2) by.push_back({dist(origin_of(s, *f), q.at), f});
      std::sort(by.begin(), by.end(), [](const auto& a, const auto& b) {
        return a.first < b.first || (a.first == b.first && a.second->slot < b.second->slot);
      });
      for (size_t i = 0; i < by.size() && i < 2; i++) rushers.push_back(by[i].second);
    }
  }

  for (Fighter* f : live) {
    Role r = Role::free;
    if (f == carrier)
      r = Role::carry;
    else if (std::find(rushers.begin(), rushers.end(), f) != rushers.end())
      r = Role::rush;
    else if (f == holder)
      r = Role::hold;
    if (r == Role::rush && f->role != Role::rush) t.stats.rushes++;
    if (r == Role::carry && f->role != Role::carry) t.stats.quads++;
    if (r != f->role && why) {
      why->decide("role");
      why->fact("slot", f->slot);
      why->choose(role_name(r), r == Role::carry  ? "carries the quad"
                                : r == Role::rush ? "nearest the quad as it comes due"
                                : r == Role::hold ? "the strongest: holds the area"
                                                  : "gathers what he lacks");
    }
    f->role = r;
  }
}

}  // namespace shooters_edge::bots
