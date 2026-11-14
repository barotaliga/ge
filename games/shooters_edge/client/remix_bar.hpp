// CLASSIC REMIX'S WORN BAR: with `cr_items 2` the classic game's rules wear the sequel's weapons,
// and the HUD that shows them is the sequel's own status bar (docs/goals/M18u.md, M18u4). The
// player's classic status (hud/status.hpp) is put into the sequel's 32 stats and its program
// (server/sequel_bar.hpp's `sq_bar_program`) is run over them as the sequel's own bar is
// (client/sequel_bar.hpp), its pictures read from the remix's files:
//
// - **Health** (stats 0, 1) and the blow just taken (15, bit 1).
// - **Ammunition** (2, 3) by the weapon in hand, as the sequel's weapon it wears holds it: the
//   shotguns' shells, the nailguns' bullets (the machinegun and chaingun), the grenade
//   launcher's grenades, the rocket launcher's rockets, the beam's slugs (the railgun); none for
//   the melee weapon.
// - **Armour** (4, 5) by its class: the jacket, combat and body armour.
// - **The weapon picked up** (7, 8) for 3 s: its icon and its name.
// - **A power-up running** (9, 10): quad damage, invulnerability or the environment suit, and its
//   whole seconds left (the classic ring has no picture of the sequel's and is not shown).
// - **The frags** (14) in a match.
//
// ge:summary Classic remix's worn bar: the sequel's status bar drawn over the classic status
// ge:keywords remix classic-remix cr_items worn-bar sequel-hud status-bar stats remix-hud
// ge:skill ui shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "shooters_edge/client/sequel_bar.hpp"
#include "shooters_edge/hud/status.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

// The sequel's 32 stats for the classic status `st` at time `t` s (`st.t`); `match` puts the
// frags in.
std::array<int16_t, hud::kLayoutStats> worn_stats(const hud::Status& st, bool match);

// The picture image `index` (1..) names, or "" (the stats' picture numbers index this list).
std::string_view worn_image(int index);

// Runs the sequel's bar over `st` on a `width` x `height` window into `bar.ops`, as
// `run_sequel_bar` does for the sequel's own world (draw it with `draw_sequel_bar`).
void run_worn_bar(SequelBar& bar, const hud::Status& st, bool match, int64_t frame, int width,
                  int height);

// Runs and draws the sequel's bar over `st` while the rules wear the sequel's things (`worn`);
// false, drawing nothing, otherwise (the game's own HUD is drawn instead).
bool draw_worn_bar(ge::ui::DrawList& dl, SequelBar& bar, const profiles::RemixArsenal* worn,
                   const hud::Status& st, bool match, int width, int height);

}  // namespace shooters_edge::client
