// A game's hub spec, defined: the weight of a mode and the interest rule that orders the cards.
//
// ge:summary The interest rule defined: full, humans, the mode's weight, bots and stale scores
// ge:keywords hub interest order mode weight bots intermission
// ge:skill net
// ge:test tests/hub_api_test.cpp
#include "hub/spec.hpp"

namespace ge::hub {

namespace gn = ge::net;

int mode_weight(const HubSpec& spec, std::string_view mode) {
  for (const HubMode& m : spec.modes) {
    if (m.name == mode) return m.weight;
  }
  return 1;
}

int64_t interest(const HubSpec& spec, const HubNode& node, int64_t now_us) {
  if (!node.have) return 0;
  const gn::Status& s = node.status;
  int people = 0, bots = 0;
  for (const gn::StatusPlayer& p : s.players) {
    if (p.flags & (gn::kPlayerSpectator | gn::kPlayerViewer)) continue;
    people++;
    bots += (p.flags & gn::kPlayerBot) ? 1 : 0;
  }
  const int humans_in = people - bots;
  if (s.phase == gn::StatusPhase::intermission && node.phase_us >= 0 &&
      now_us - node.phase_us > kStaleIntermissionUs) {
    return 1;  // listed, at the bottom
  }
  const bool full = s.max_players > 0 && humans_in >= s.max_players;  // bots fill nothing
  int64_t score = (full ? 1000000 : 0) + int64_t{humans_in} * 10000 + int64_t{people} * 100 +
                  int64_t{mode_weight(spec, s.mode)} * 10 + 2;
  // Divided down by the share of bots: a field of bots alone keeps a tenth of its place.
  if (people > 0) score = score * (int64_t{10} * humans_in + (people - humans_in)) / (int64_t{10} * people);
  return score < 2 ? 2 : score;
}

}  // namespace ge::hub
