// THE NAMES A BOT PLAYS UNDER on a server with people: a handle as a person picks one (a word, a
// short made-up name, a letter swapped for a digit now and then), never a team's name and a number,
// which tells everyone on the server what it is. Drawn without repeats off a seed from a list of
// made-up handles (none is a known player's), so two teams seeded apart rarely share one.
//
// ge:summary The names bots play under among people: made-up handles drawn off a seed
// ge:keywords bot-names handles nicknames names bot_handles realistic-names turing human-like
// ge:skill bots
// ge:test tests/shooters_edge_footwork_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace shooters_edge::bots {

// `count` handles, no two alike, drawn off `seed` (the same seed, the same handles).
std::vector<std::string> bot_handles(uint32_t seed, int count);

}  // namespace shooters_edge::bots
