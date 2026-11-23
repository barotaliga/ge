// THE RULESET ON A SERVER JOINED (classic/rulesets.hpp, console/ruleset.hpp): the window's
// `ruleset` setting over the loaded game's book (Quake II's when the game is Quake II, else
// QuakeWorld's), locked while joined, its match-only refusals in force while the player plays a
// match (joined, not watching, and the server's `status` not `Standby`), and the answers to the
// chat line `f_ruleset`.
//
// - `add_remote_rulesets`: the `ruleset` setting, the `rulesets` command and `f_ruleset` (alone:
//   what it is for; `f_ruleset check`: asks everyone in chat).
// - `remote_rulesets_frame` (each frame): the book, the lock while joined, a choice taken; the
//   facts an answer says.
// - `remote_fair_step` (each frame's prints): a chat line `f_ruleset` answered in chat, at most
//   once every 20 seconds, never while watching.
//
// ge:summary The ruleset on a server joined: the book, the lock while joined, f_ruleset answers
// ge:keywords ruleset rulesets f_ruleset fair-play rsn smackdown duel playing standby
// ge:keywords add_remote_rulesets remote_rulesets_frame remote_fair_step
// ge:skill classic net console
// ge:test tests/shooters_edge_remote_rulesets_test.cpp
#pragma once

#include <vector>

#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

struct Remote;

void add_remote_rulesets(ge::console::Console& con, Remote& r);
void remote_rulesets_frame(Remote& r, ge::console::Console& con);
// Whether the player is playing a match now (the strict rulesets' match-only refusals).
bool remote_playing(const Remote& r);
void remote_fair_step(Remote& r, const std::vector<client::DemoPrint>& prints, double now_s);

}  // namespace shooters_edge::net
