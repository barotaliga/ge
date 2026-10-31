// THE MOMENTS WORTH WATCHING IN A DEMO: Shooter's Edge's highlight rules over a scanned demo's
// kills (demo_scan.hpp), on the engine's highlights (engine/replay/highlights.hpp: the engine
// scores, windows, merges and orders them; the rules here only say what a moment is).
//
// | Kind | A moment when | Weight |
// | --- | --- | --- |
// | `multi-kill` | one player kills two or more within `kOneShotS` (one rocket, one blast) | 3 a
// kill past the first | | `long-shot` | a rocket kill whose rocket flew `kLongRocket` or more (or,
// with no rocket matched, the killer stood that far) | its flight over 500 units | | `air-kill` | a
// rocket or grenade kill of a player in the air; gibbed, an air gib | 2, 3 gibbed | |
// `annihilation` | the victim had `kBigStack` or more health and armour and lost it all within
// `kQuickS` | his stack over 100, more the quicker | | `telefrag` | the killer teleported onto his
// victim (demo_scan.hpp) | 3 | | `streak` | one player kills `kStreak` or more within `kStreakS`
// | 1.5 a kill past two |
//
// Every highlight is its killer's (`subject`), so the play bar shows the tracked player's. A game
// with other moments adds rules to the list (`demo_rules`) and kinds to `demo_kinds`.
//
// ge:summary Shooter's Edge's highlight rules over a demo's kills: multi-kills, long rockets, air
// ge:summary gibs, annihilated stacks, telefrags, streaks; their kinds' colours and windows
// ge:keywords highlights rules multi-kill double-kill long-shot long-rocket air-kill air-gib
// ge:keywords annihilation stack streak telefrag demo_kinds demo_rules demo_highlights markers
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#pragma once

#include <string>
#include <vector>

#include "replay/highlights.hpp"
#include "shooters_edge/client/demo_scan.hpp"

namespace shooters_edge::client {

inline constexpr double kOneShotS = 0.2;    // kills this close are one shot's, s
inline constexpr float kLongRocket = 1000;  // a long rocket's flight, units
inline constexpr int kBigStack = 150;       // health and armour a victim had, at least
inline constexpr double kQuickS = 2;        // lost within this, s
inline constexpr int kStreak = 3;           // kills
inline constexpr double kStreakS = 10;      // within this, s

using DemoRule = ge::replay::HighlightRule<DemoKill>;

// The kinds: their labels, colours, windows and scales.
const std::vector<ge::replay::HighlightKind>& demo_kinds();
// The rules, in the table's order; each highlight's text names the players from `names`.
std::vector<DemoRule> demo_rules(const std::array<std::string, 32>& names);

// Every highlight in a scanned demo, finished for `subject` (-1: everyone's).
ge::replay::Highlights demo_highlights(const DemoScan& scan, int subject, int most = 0);

// The text for a slot: its name, or "player n".
std::string player_name(const std::array<std::string, 32>& names, int slot);

}  // namespace shooters_edge::client
