// The server's own chat for every runner (chat_rules.hpp states the route).
//
// ge:summary Routes a say or say_team the mod left to the server; registers sv_spectalk
// ge:keywords chat_route add_chat_settings sv_spectalk
// ge:skill shooter
// ge:test tests/shooters_edge_chat_rules_test.cpp
#include "shooters_edge/server/chat_rules.hpp"

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace con = ge::console;

void add_chat_settings(con::Cvars& cvars) {
  if (cvars.find("sv_spectalk") != nullptr) return;
  cvars.add({.name = "sv_spectalk",
             .fallback = "1",
             .flags = con::kServer,
             .help = "Whether players read what spectators say, a spectator's team chat never "
                     "reaching them.",
             .kind = con::CvarKind::boolean,
             .values = "0: only the other spectators read it; 1: everyone reads it (the default)"});
}

ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text) {
  ChatLine out;
  if (slot < 0 || slot >= sh::kMaxClients) return out;
  const ClientInfo& c = s.clients[slot];
  const std::string who = info_value(c.userinfo, "name");
  const con::Cvar* talk = s.cvars.find("sv_spectalk");
  const bool to_specs = c.spectator && (team || (talk != nullptr && !talk->on()));
  const std::string mine = info_value(c.userinfo, "team");
  for (int k = 0; k < sh::kMaxClients; k++) {
    const ClientInfo& d = s.clients[k];
    if (!d.active) continue;
    if (to_specs ? d.spectator : (!team || info_value(d.userinfo, "team") == mine))
      out.to |= 1u << k;
  }
  out.text = (to_specs ? "[SPEC] " + who : team ? "(" + who + ")" : who) + ": " + text + "\n";
  return out;
}

}  // namespace shooters_edge::server
