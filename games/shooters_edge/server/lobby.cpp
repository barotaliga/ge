// Shooter's Edge's lobby, defined: a team's colours and its place in the mod (lobby.hpp).
//
// ge:summary Shooter's Edge's lobby: a team's palette row, the team put in the user info and mod
// ge:keywords lobby_apply team_colour topcolor bottomcolor userinfo
// ge:skill shooter net
// ge:test tests/shooters_edge_lobby_test.cpp
#include "shooters_edge/server/lobby.hpp"

#include <string>

#include "progs/vm.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

int team_colour(Team t) { return t == Team::red ? 4 : t == Team::blue ? 13 : 0; }

void lobby_apply(const RulesRunner& r, const Lobby& l, int slot) {
  if (slot < 0 || slot >= kLobbySeats || !l.seats[static_cast<size_t>(slot)].used) return;
  const Team t = l.seats[static_cast<size_t>(slot)].team;
  const std::string colour = std::to_string(team_colour(t));
  r.user_info(slot, "team", team_name(t));
  r.user_info(slot, "topcolor", colour);
  r.user_info(slot, "bottomcolor", colour);
  // A QC program also reads the team off its own field.
  QcServer* q = qc_of(r);
  if (q == nullptr || q->progs == nullptr) return;
  QcServer& s = *q;
  if (const ge::progs::Def* d = ge::progs::find_field(*s.progs, "team"); d != nullptr)
    *ge::progs::entity_field(s.q.vm, slot + 1, d->offset) =
        ge::progs::as_word(static_cast<float>(static_cast<int>(t)));
}

}  // namespace shooters_edge::server
