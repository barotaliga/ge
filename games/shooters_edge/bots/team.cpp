// A team of fighters (team.hpp): the key areas off the map's items, and the roles each plan.
//
// ge:summary A team of fighters: the key areas and the roles planned each half second
// ge:keywords team_init team_plan roles hold rush carry free quad-timer claims strength
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/team.hpp"

#include <algorithm>
#include <cmath>
#include <string>

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

bool team_say(Fighter& f, double t, const std::string& text) {
  if (!f.team || t - f.said_at < kSayGap_s) return false;
  Team& tm = *f.team;
  if (text == tm.last_text && t - tm.last_text_at < kRepeat_s) return false;
  f.said_at = t;
  tm.last_text = text;
  tm.last_text_at = t;
  tm.stats.messages++;
  tm.log.push_back({t, f.slot, text});
  f.outbox.push_back(text);
  return true;
}

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
  Fighter* timer = nullptr;  // the member whose knowledge set the quad's time
  double quad_due = 0;
  if (t.quad_item >= 0 && !carrier) {
    const ItemSpot& q = items[static_cast<size_t>(t.quad_item)];
    // When it is due back: the earliest a member knows of (one who saw it there since it was last
    // gone, or one who timed it going); a quad nobody can time is not rushed.
    double due = 1e30;
    for (Fighter* f : t.members) {
      const ItemSpot& mine = f->items[static_cast<size_t>(t.quad_item)];
      const double at = mine.present && mine.confirmed ? now : mine.timed ? mine.back_at : 1e30;
      if (at < due) {
        due = at;
        timer = f;
      }
    }
    if (due - now <= kRushAhead_s) {
      std::vector<std::pair<float, Fighter*>> by;
      for (Fighter* f : live)
        if (f != holder || live.size() <= 2) by.push_back({dist(origin_of(s, *f), q.at), f});
      std::sort(by.begin(), by.end(), [](const auto& a, const auto& b) {
        return a.first < b.first || (a.first == b.first && a.second->slot < b.second->slot);
      });
      for (size_t i = 0; i < by.size() && i < 2; i++) rushers.push_back(by[i].second);
      quad_due = due - now;
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

  // What the plan says out loud: the rush called by whoever timed the quad, the carry and its loss.
  if (!rushers.empty() && !t.rushing) {
    Fighter* caller = timer && alive(s, *timer) ? timer : rushers[0];
    const int in = static_cast<int>(std::lround(quad_due));
    team_say(*caller, now,
             in >= 2 ? "quad in " + std::to_string(in) + ", get quad!" : std::string("get quad!"));
    if (rushers[0] != caller) team_say(*rushers[0], now, "going quad");
  }
  t.rushing = !rushers.empty();
  if (carrier && carrier != t.last_carrier) team_say(*carrier, now, "have quad!");
  if (!carrier && t.last_carrier && !alive(s, *t.last_carrier))
    team_say(*t.last_carrier, now, "lost quad");
  t.last_carrier = carrier;
}

}  // namespace shooters_edge::bots
