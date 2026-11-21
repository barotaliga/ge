// QUAKE'S MAPS AS THE BOTS' BOOK (shooterbots/map_book.hpp): people's rocket-jump and gap tricks,
// scripted routes to key items and item rules, by map name, for every game Shooter's Edge's bots
// play on these maps.
//
// ROCKET-JUMP TRICKS: the rocket jumps people make on a map again and again,
// read off their recorded games by the `se-rj` lab (its tricks.tsv: 51 public dm2 matches, 51 of
// dm3, 51 of e1m2, read 2026-11-07), each a takeoff and a landing, the way it is flown (the flip
// or the shot at the feet), its pitch and run and the least jumper's level that makes it (the
// lowest whose players made a fifth of its jumps or more). The nav proves each one by the move
// (shooterbots/nav.hpp `nav_add_tricks`) when a map's is built (`build_map_nav`), so a trick the
// move cannot fly from the graph's nodes is left out, and one it can is a rocket link a bot of its
// level takes like any other. To add a map's, run `se-rj --dir=<its demos>` and copy its rows.
//
//
// GAP TRICKS: the plain jumps people make over a pit at a hopper's speed again
// and again (the strafe jump over dm2's lava straight to the red or the yellow armour), read off
// their recorded games by the `se-rj` lab (its gap_tricks.tsv: 51 public dm2 matches, read
// 2026-11-10), each a takeoff and a landing, the takeoff's speed people had (its quarter: most
// came faster), how often they made it, whether the pit holds lava or slime and the least
// jumper's level that makes it (the lowest whose players made a fifth of its jumps or more).
// Only those a run's jump falls short of in most of the jumps (speed jumps) and made ten times
// or more are kept. The nav proves each (shooterbots/nav_gaps.hpp `nav_add_gap_tricks`) when a
// map's is built (`build_map_nav`): one the move cannot fly from the graph's nodes, or one a run's
// jump makes too, is left out. To add a map's, run `se-rj --dir=<its demos>` and copy its rows.
//
//
// SCRIPTED ROUTES TO KEY ITEMS, per map: the last stretch to an item written as a person takes
// it, where the proven graph has no such way (the yellow armour's stairs on e1m2 hopped up in one
// flow; the grid misses a 16-unit tread and the nav's standing jumps cost a second a step). Each
// route is a run of points (a player's feet), the first its ENTRY (joined to the nav's nearest
// node), each after it walked or hopped onto; the last is the item's own node. The nav proves
// each one with the move (shooterbots/nav_script.hpp `prove_script`) and adds it as one `script`
// link (shooterbots/map_nav.cpp), so a route to the item leads to the entry and the script finishes
// it. A door over the way (e1m2's YA stairs, shut until a button is shot) bars the link while shut,
// and the fighter's button logic opens it as for any item behind a door.
//
// Read off the `se-spot` lab's pictures and se-reach's `--floors` (the treads' heights and
// depths), and people's own way up (se-rj `--spot`). A route that does not hold is tabled by
// se-reach as refused, never added.
//
// ge:summary Quake's maps as the bots' book: rocket and gap tricks, scripted routes, item rules
// ge:keywords rocket-tricks gap-tricks item-routes item-rules map-data dm2 dm3 e1m2 quake_maps
// ge:keywords tricks rocket-jump flip trick-spots gap-jump speed-jump lava-jump ya-stairs
// ge:keywords scripted-route flowing-hop rockets-before-quad
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "shooterbots/map_book.hpp"

namespace shooters_edge::bots {

using ge::shooterbots::GapTrick;
using ge::shooterbots::ItemRoute;
using ge::shooterbots::ItemRule;
using ge::shooterbots::RocketTrick;

// Each table's rows for map `name` (its base name, "dm2"); none for a map nobody read.
std::span<const RocketTrick> rocket_tricks(std::string_view name);
std::span<const GapTrick> gap_tricks(std::string_view name);
std::span<const ItemRoute> item_routes(std::string_view map);
std::span<const ItemRule> item_rules(std::string_view map);

// The four as one book.
const ge::shooterbots::MapBook& quake_maps();

}  // namespace shooters_edge::bots
