// CLASSIC REMIX'S WORN BAR: with `cr_items 2` Quake's rules wear Quake II's weapons,
// and the HUD that shows them is Quake II's own status bar (docs/goals/M18u.md, M18u4). The
// player's classic status (hud/status.hpp) is put into Quake II's 32 stats and its program
// (server/q2_bar.hpp's `sq_bar_program`) is run over them as Quake II's own bar is
// (client/q2_bar.hpp), its pictures read from the remix's files:
//
// - **Health** (stats 0, 1) and the blow just taken (15, bit 1).
// - **Ammunition** (2, 3) by the weapon in hand, as Quake II's weapon it wears holds it: the
//   shotguns' shells, the nailguns' bullets (the machinegun and chaingun), the grenade
//   launcher's grenades, the rocket launcher's rockets, the beam's slugs (the railgun); none for
//   the melee weapon.
// - **Armour** (4, 5) by its class: the jacket, combat and body armour.
// - **The weapon picked up** (7, 8) for 3 s: its icon and its name.
// - **A power-up running** (9, 10): quad damage, invulnerability or the environment suit, and its
//   whole seconds left (the classic ring has no picture of Quake II's and is not shown).
// - **The frags** (14) in a match.
//
// And the reverse (M18u4d): while Quake II's rules wear Quake's things (`sv_wear`),
// the classic status bar is drawn over Quake II's player (`worn_status`): each of its weapons
// as the classic gun it wears (by `profiles::kQ2AsQ1`'s roles), its ammunition in the
// classic counters (bullets the nails, grenades or rockets the rockets and slugs or cells the
// cells by the gun in hand), its armour by class, its power-ups' seconds, its frags; the
// pictures read from the remix's files (`WornHud::art`).
//
// ge:summary Classic remix's worn bar: Quake II's status bar drawn over the classic status
// ge:keywords remix classic-remix cr_items worn-bar q2-hud status-bar stats remix-hud
// ge:skill ui shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "shooters_edge/client/q2_bar.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/hud/art.hpp"
#include "shooters_edge/hud/status.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::server {
struct Q2Server;
}

namespace shooters_edge::client {

// The HUD while the rules wear the other game's things: Quake II's bar (`bar`) over the classic
// status, the classic status bar's pictures (`art`) over Quake II's.
struct WornHud {
  Q2Bar bar;
  hud::HudArt art;
};

// Ready for what `remix` now wears: the bar's pictures read again, the classic pictures read from
// the remix's files while Quake II wears Quake's things (else none).
void worn_hud_reset(WornHud& h, const RemixFiles& remix);

// The classic status for Quake II's player 0 at time `t` s (`sv_wear q1`).
hud::Status worn_status(const server::Q2Server& sq, double t);
// The status of the player on a server of Quake II joined (the client's recording `s`, read as
// it comes) at time `t` s: health and armour off its stats, and Quake II's own weapon keys
// (`keyed`, as `worn_status` fills them) so the weapon groups and the quick weapon ask for its
// weapons. The weapon in hand is the one whose view model is held; the weapons carried and their
// ammo are the last inventory the server sent, and while none came every weapon is taken as carried
// and ready (the server answers a weapon not carried with its own words).
hud::Status q2_joined_status(const DemoState& s, double t);

// Quake II's 32 stats for the classic status `st` at time `t` s (`st.t`); `match` puts the
// frags in.
std::array<int16_t, hud::kLayoutStats> worn_stats(const hud::Status& st, bool match);

// The picture image `index` (1..) names, or "" (the stats' picture numbers index this list).
std::string_view worn_image(int index);

// Runs Quake II's bar over `st` on a `width` x `height` window into `bar.ops`, as
// `run_q2_bar` does for Quake II's own world (draw it with `draw_q2_bar`).
void run_worn_bar(Q2Bar& bar, const hud::Status& st, bool match, int64_t frame, int width,
                  int height);

// Runs and draws Quake II's bar over `st` while the rules wear Quake II's things (`worn`);
// false, drawing nothing, otherwise (the game's own HUD is drawn instead).
bool draw_worn_bar(ge::ui::DrawList& dl, Q2Bar& bar, const profiles::RemixArsenal* worn,
                   const hud::Status& st, bool match, int width, int height);

// The world whose own bar the window draws, and its frame: Quake II's rules', or a hosted
// game library's (a mod of Quake II's, on either game's maps: its layout program and its
// players' layouts); null for any other rules.
const server::RulesWorld* q2_bar_world(const server::RulesRunner* rules, int64_t& frame);

// QUAKE II'S BARS, the one choice the window and the labs make: Quake II's own bar (`own`,
// run over `rules`' world (`q2_bar_world`) at time `t` s, its pictures from `files`, or from
// the remix's files while a hosted mod of Quake II's plays this game's maps, with the crosshair
// as the player sets it, `crosshair` (null: none), and the centre print `centre`) while its rules
// run in their own things; Quake II's bar over the classic status `st` while the classic rules
// wear Quake II's (`draw_worn_bar`, `worn`); false, drawing nothing, otherwise (the game's own
// HUD is drawn instead: the classic status bar, over Quake II's player while it wears the classic
// game's things).
bool draw_q2_bars(ge::ui::DrawList& dl, Q2Bar& own, Q2Bar& worn, const server::RulesRunner* rules,
                  const SearchPath& files, const RemixFiles& remix, const hud::Status& st,
                  bool match, const ge::hud::CrosshairSettings* crosshair, std::string_view centre,
                  double t, int width, int height);

}  // namespace shooters_edge::client
