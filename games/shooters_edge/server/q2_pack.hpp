// QUAKE II'S MISSION PACKS AS ONE SWITCH: which pack's rules the Quake II runner (q2_server.hpp)
// plays, so a pack's weapons, items, monsters and map things are built beside the base game's
// rules and the base game's own stay as they are (docs/porting/rules.md: a game's rule on top of
// shared rules goes behind one switch).
//
// - **The switch** is the game folder a server is started on: `xatrix` plays The Reckoning's
//   rules, `rogue` Ground Zero's, any other folder (`baseq2`, a mod's) the base game's. It is
//   read once, as the server is made (`Q2ServerOptions::pack`), and never changes in a level.
// - **The tables** (the items of q2_items.hpp, and the weapons, shots and monsters as each pack's
//   rows land) hold the base game's rows first, unchanged, then each pack's rows marked with their
//   pack. A row of another pack is never found by its name (`sq_item`), spawned or numbered, so
//   the base game's goldens, digests and item numbers do not move.
// - **The item numbers** a client reads (q2_bar.hpp) come from each pack's own list, in that
//   pack's order: the base game's list for the base game.
//
// ge:summary Quake II's mission packs as one switch on its runner: which pack's rules play
// ge:keywords q2 mission-pack pack-switch xatrix rogue reckoning ground-zero game-folder q2pack
// ge:keywords q2_pack_of_folder pack-rows
// ge:skill shooter
// ge:test tests/shooters_edge_q2_pack_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace shooters_edge::server {

// The rules a Quake II server plays: the base game's, or a mission pack's on top of them.
enum class Q2Pack : uint8_t { base, reckoning, ground_zero };

// The pack a game folder plays (`xatrix` The Reckoning, `rogue` Ground Zero, any case; a path's
// last part is the folder), else the base game.
Q2Pack q2_pack_of_folder(std::string_view folder);

// The pack's name as a player reads it ("Quake II", "The Reckoning", "Ground Zero").
std::string_view q2_pack_name(Q2Pack pack);

// Whether a table row marked `row` plays under `pack`: the base game's rows always, a pack's rows
// under that pack only.
constexpr bool q2_pack_has(Q2Pack pack, Q2Pack row) { return row == Q2Pack::base || row == pack; }

}  // namespace shooters_edge::server
