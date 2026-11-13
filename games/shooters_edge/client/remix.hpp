// CLASSIC REMIX: one classic game played with another's parts, a feature of classic-enhance
// (docs/goals/M18u.md). Off unless the player turns it on, and then only with two classic games
// loaded as FULL games (a shareware or demo install never counts), so a game loaded on its own
// keeps its own assets alone.
//
// - **The settings** (`RemixSettings`, `bind_remix`): `cr_enabled` (0 off, the default; 1 on)
//   and `cr_items` (0 the played game's own items; N the Nth classic game's: 1 the classic game's,
//   2 the sequel's), both kept in the player's config.
// - **The check** (`remix_check`): the installs the window knows, each with its edition; the
//   remix is on when it is enabled and a second full game is there, else the refusal says what
//   is missing in the game's words.
// - **Another game's map** (`map_game`, `remix_entities`): a map found only in the other game's
//   packs is played in the played game, its items made the played game's (or `cr_items`'s) by
//   the remix table (profiles/remix.hpp).
// - **Another game's items** (`items_game`): with `cr_items` naming the other game, a game's own
//   maps are played with that game's items, and the HUD that shows them is that game's.
//
// ge:summary Classic remix: cr_enabled and cr_items, the two-full-games check, items swapped
// ge:keywords remix classic-remix cr_enabled cr_items remix_check remix_entities items_game
// ge:keywords other-game-map item-swap full-game entity-text map-items
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/cvar.hpp"
#include "shooters_edge/client/installs.hpp"

namespace shooters_edge::client {

struct RemixSettings {
  bool enabled = false;  // cr_enabled
  int32_t items = 0;     // cr_items: 0 the played game's; N the Nth classic game's
};

// `cr_enabled` and `cr_items`, writing `out`.
void bind_remix(ge::console::Cvars& cvars, RemixSettings* out);

// A classic game the window knows: its content profile id, its install and its edition.
struct KnownGame {
  std::string id;
  std::string basedir;
  Edition edition = Edition::unknown;
};

struct RemixState {
  bool on = false;
  std::string other;          // the other full game's profile id ("" when off)
  std::string other_basedir;  // its install
  std::string why;            // when off but enabled: what is missing, in the game's words
};

// Whether the remix is on while `played` is played, from the games the window knows.
RemixState remix_check(const RemixSettings& s, std::string_view played,
                       std::span<const KnownGame> games);

// The profile id of the game whose items a map gets while the remix is on: `cr_items`'s game
// when it names a content profile, else `played`'s.
std::string_view items_game(const RemixSettings& s, std::string_view played);

// What a rewrite did, for the console and the lab.
struct RemixSwap {
  std::string from, to;  // class names; `to` empty: dropped
  int count = 0;
};
struct RemixReport {
  std::vector<RemixSwap> swaps;  // in the order first met
  int items = 0;                 // items met
  int swapped = 0;               // items given another class name
  int dropped = 0;               // items left out
};

// The entity text `text` of a map whose items are `from`'s, with every item made `to`'s by the
// remix table, into `out`. The same text when the games are the same. False with `error` when
// the text does not parse.
bool remix_entities(std::string_view text, std::string_view from, std::string_view to,
                    std::string& out, RemixReport* report, std::string& error);

// The line the console prints for a rewrite, in the game's words.
std::string remix_line(const RemixReport& r, std::string_view to);

}  // namespace shooters_edge::client
