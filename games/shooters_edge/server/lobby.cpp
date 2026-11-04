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

void lobby_apply(QcServer& s, const Lobby& l, int slot) {
  if (slot < 0 || slot >= kLobbySeats || !l.seats[static_cast<size_t>(slot)].used) return;
  const Team t = l.seats[static_cast<size_t>(slot)].team;
  const std::string colour = std::to_string(team_colour(t));
  set_user_info(s, slot, "team", team_name(t));
  set_user_info(s, slot, "topcolor", colour);
  set_user_info(s, slot, "bottomcolor", colour);
  if (s.progs == nullptr) return;
  if (const ge::progs::Def* d = ge::progs::find_field(*s.progs, "team"); d != nullptr)
    *ge::progs::entity_field(s.q.vm, slot + 1, d->offset) =
        ge::progs::as_word(static_cast<float>(static_cast<int>(t)));
}

}  // namespace shooters_edge::server
