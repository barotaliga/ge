// The server's own chat for every runner (chat_rules.hpp states the route).
//
// ge:summary Routes a say or say_team the mod left to the server; sv_spectalk, sv_sayteam_to_spec
// ge:keywords chat_route add_chat_settings sv_spectalk sv_sayteam_to_spec chat_in_match
// ge:skill shooter
// ge:test tests/shooters_edge_chat_rules_test.cpp
#include "shooters_edge/server/chat_rules.hpp"

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace con = ge::console;

void add_chat_settings(con::Cvars& cvars) {
  if (cvars.find("sv_spectalk") == nullptr)
    cvars.add({.name = "sv_spectalk",
               .fallback = "1",
               .flags = con::kServer,
               .help = "Whether players read what spectators say during a match, a spectator's "
                       "team chat never reaching them.",
               .kind = con::CvarKind::boolean,
               .values = "0: during a match only the other spectators read it; 1: everyone reads "
                         "it (the default)"});
  if (cvars.find("sv_sayteam_to_spec") == nullptr)
    cvars.add(
        {.name = "sv_sayteam_to_spec",
         .fallback = "1",
         .flags = con::kServer,
         .help = "When spectators read the team messages of the team they follow (the "
                 "reports a player's messages send, never a line typed by hand).",
         .kind = con::CvarKind::number,
         .values = "0: never; 1: during a match (the default); 2: outside a match; 3: always"});
}

bool chat_in_match(const RulesCommon& s) {
  for (const auto& [k, v] : s.localinfo)
    if (k == "ge_phase") return v == "countdown" || v == "match";
  return true;
}

ChatLine chat_route(const RulesCommon& s, int slot, bool team, const std::string& text,
                    const int* tracks) {
  ChatLine out;
  if (slot < 0 || slot >= sh::kMaxClients) return out;
  const ClientInfo& c = s.clients[slot];
  const std::string who = info_value(c.userinfo, "name");
  const bool in_match = chat_in_match(s);
  const con::Cvar* talk = s.cvars.find("sv_spectalk");
  const bool to_specs = c.spectator && (team || (in_match && talk != nullptr && !talk->on()));
  const std::string mine = info_value(c.userinfo, "team");
  const con::Cvar* to_spec = s.cvars.find("sv_sayteam_to_spec");
  const int when = to_spec != nullptr ? static_cast<int>(to_spec->number) : 0;
  const bool specs_read = !c.spectator && team && text.find('\r') != std::string::npos &&
                          (when == 3 || (when == 1 && in_match) || (when == 2 && !in_match));
  for (int k = 0; k < sh::kMaxClients; k++) {
    const ClientInfo& d = s.clients[k];
    if (!d.active) continue;
    bool hears;
    if (to_specs) {
      hears = d.spectator;
    } else if (!team) {
      hears = true;
    } else if (!d.spectator || k == slot) {
      hears = info_value(d.userinfo, "team") == mine;
    } else {  // a spectator reading a player's team chat
      const int t = tracks != nullptr ? tracks[k] : -1;
      const bool follows = t >= 0 && t < sh::kMaxClients && s.clients[t].active;
      hears =
          specs_read && info_value(follows ? s.clients[t].userinfo : d.userinfo, "team") == mine;
    }
    if (hears) out.to |= 1u << k;
  }
  out.text = (to_specs ? "[SPEC] " + who : team ? "(" + who + ")" : who) + ": " + text + "\n";
  return out;
}

}  // namespace shooters_edge::server
