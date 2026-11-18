// The server's chat route (chat.hpp states it).
//
// ge:summary The chat route's settings and who reads a say or a say_team
// ge:keywords chat_route add_chat_settings chat_settings sv_spectalk sv_sayteam_to_spec
// ge:skill net
// ge:test tests/server_chat_test.cpp
#include "server/chat.hpp"

namespace ge::server {

void add_chat_settings(console::Cvars& cvars) {
  if (cvars.find("sv_spectalk") == nullptr)
    cvars.add({.name = "sv_spectalk",
               .fallback = "1",
               .flags = console::kServer,
               .help = "Whether players read what spectators say during a match, a spectator's "
                       "team chat never reaching them.",
               .kind = console::CvarKind::boolean,
               .values = "0: during a match only the other spectators read it; 1: everyone reads "
                         "it (the default)"});
  if (cvars.find("sv_sayteam_to_spec") == nullptr)
    cvars.add(
        {.name = "sv_sayteam_to_spec",
         .fallback = "1",
         .flags = console::kServer,
         .help = "When spectators read the team reports of the team they follow, never a "
                 "line typed by hand.",
         .kind = console::CvarKind::integer,
         .min = 0,
         .max = 3,
         .values = "0: never; 1: during a match (the default); 2: outside a match; 3: always"});
}

ChatSettings chat_settings(const console::Cvars& cvars) {
  ChatSettings s;
  if (const console::Cvar* c = cvars.find("sv_spectalk")) s.spectalk = c->on();
  if (const console::Cvar* c = cvars.find("sv_sayteam_to_spec"))
    s.sayteam_to_spec = static_cast<int>(c->number);
  return s;
}

ChatLine chat_route(std::span<const ChatSeat> seats, int slot, bool team, bool report,
                    bool in_match, const ChatSettings& s, const std::string& text) {
  ChatLine out;
  const int n = static_cast<int>(seats.size() < 32 ? seats.size() : 32);
  if (slot < 0 || slot >= n) return out;
  const ChatSeat& c = seats[static_cast<size_t>(slot)];
  const bool to_specs = c.spectator && (team || (in_match && !s.spectalk));
  const int when = s.sayteam_to_spec;
  const bool specs_read = !c.spectator && team && report &&
                          (when == 3 || (when == 1 && in_match) || (when == 2 && !in_match));
  for (int k = 0; k < n; k++) {
    const ChatSeat& d = seats[static_cast<size_t>(k)];
    if (!d.active) continue;
    bool reads;
    if (to_specs) {
      reads = d.spectator;
    } else if (!team) {
      reads = true;
    } else if (!d.spectator || k == slot) {
      reads = d.team == c.team;
    } else {  // a spectator reading a player's team report
      const int t = d.follows;
      const bool follows = t >= 0 && t < n && seats[static_cast<size_t>(t)].active;
      reads = specs_read && (follows ? seats[static_cast<size_t>(t)].team : d.team) == c.team;
    }
    if (reads) out.to |= 1u << k;
  }
  out.text = (to_specs ? "[SPEC] " + c.name
              : team   ? "(" + c.name + ")"
                       : c.name) +
             ": " + text + "\n";
  return out;
}

}  // namespace ge::server
