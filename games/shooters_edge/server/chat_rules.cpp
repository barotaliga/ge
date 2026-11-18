// The server's own chat for every runner (chat_rules.hpp states the wiring).
//
// ge:summary The runner's clients as the engine's chat seats; the phase from ge_phase
// ge:keywords chat_route chat_in_match team-report chat-seats
// ge:skill shooter
// ge:test tests/shooters_edge_chat_rules_test.cpp
#include "shooters_edge/server/chat_rules.hpp"

#include <array>

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

bool chat_in_match(const RulesCommon& s) {
  for (const auto& [k, v] : s.localinfo)
    if (k == "ge_phase") return v == "countdown" || v == "match";
  return true;
}

ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text,
                    const int* tracks) {
  std::array<ge::server::ChatSeat, sh::kMaxClients> seats;
  for (int k = 0; k < sh::kMaxClients; k++) {
    const ClientInfo& c = s.clients[k];
    ge::server::ChatSeat& d = seats[static_cast<size_t>(k)];
    d.active = c.active;
    if (!c.active) continue;
    d.spectator = c.spectator;
    d.name = info_value(c.userinfo, "name");
    d.team = info_value(c.userinfo, "team");
    d.follows = tracks != nullptr ? tracks[k] : -1;
  }
  const bool report = text.find('\r') != std::string::npos;
  return ge::server::chat_route(seats, slot, team, report, chat_in_match(s),
                                ge::server::chat_settings(s.cvars), text);
}

}  // namespace shooters_edge::server
