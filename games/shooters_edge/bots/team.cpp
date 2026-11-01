// A team of fighters (team.hpp): the roster's bits, a member's message, the lab's counts.
//
// ge:summary A team of fighters: the roster, a member's message said, the role counts
// ge:keywords team_init team_say team_count roster friends key-areas
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/team.hpp"

#include <string>

namespace shooters_edge::bots {

bool team_say(Fighter& f, double t, const std::string& text) {
  if (!f.team || t - f.said_at < kSayGap_s) return false;
  for (const HeardLine& h : f.mind.heard)
    if (h.text == text && t - h.t < kRepeat_s) return false;  // a mate just said it
  Team& tm = *f.team;
  f.said_at = t;
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
  if (t.members.empty()) return;
  const std::vector<ItemSpot>& items = t.members[0]->items;
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

void team_count(Team& t) {
  for (Fighter* f : t.members) t.stats.role_commands[static_cast<int>(f->role)]++;
}

}  // namespace shooters_edge::bots
