// A GAME'S HUB SPEC: the only thing a game gives the engine's hub (docs/hub.md § One hub for every
// game). Its words (the title, the score's column), its protocol (which nodes are its own), its
// modes with the weight each carries in the order of the cards, and, for a game whose servers
// already run a status protocol of their own, that protocol's query (the browse). Everything else,
// the poller, the index, the API and later the pages and the archive, is the engine's. A game's hub
// binary is a three-line main that calls `hub_main` with its spec (`ge_add_hub`, cmake/Hub.cmake).
//
// THE INTEREST RULE (§ The status record and modes): matches full of humans first, more humans
// first, the mode's weight, divided down by the share of bots, and nothing for a match that has sat
// in its intermission too long or a node that has not answered.
//
// ge:summary A game's hub spec (words, protocol, modes and weights) and the interest rule
// ge:keywords hub spec hubspec mode weight interest order cards score-column game words
// ge:skill net
// ge:test tests/hub_api_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hub/poller.hpp"
#include "net/server_list.hpp"

namespace ge::hub {

struct HubMode {
  std::string name;  // as the game's record says it ("4on4", "race")
  int weight = 1;    // its place among the cards: higher first
};

struct HubSpec {
  std::string game;              // the game's name, on every answer ("Shooter's Edge")
  std::string id;                // a short lower-case name for files and links ("shooters_edge")
  uint32_t protocol = 0;         // the game's (net/packet.hpp's protocol_id)
  std::vector<HubMode> modes;    // the filter buttons, in this order
  std::string score = "score";   // what a player's score column is called ("frags", "time")
  bool score_low_first = false;  // a time sorts lowest first
  uint16_t http_port = 28080;    // the hub's web port when none is given
  // The game's own status protocol (net/server_list.hpp), when its servers speak one the engine's
  // status record does not: `--browse-master` and `--browse-server` list them (browse.hpp). Empty:
  // the hub reads only the engine's masters.
  net::ServerQuery browse;
};

// How long a match may sit in its intermission before it drops to the bottom.
constexpr int64_t kStaleIntermissionUs = 300000000;

// The order of the cards: higher first; 0 for a node with no record.
int64_t interest(const HubSpec& spec, const HubNode& node, int64_t now_us);
// The weight the spec gives `mode` (1 when it names no such mode).
int mode_weight(const HubSpec& spec, std::string_view mode);

}  // namespace ge::hub
