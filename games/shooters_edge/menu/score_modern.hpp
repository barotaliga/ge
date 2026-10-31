// THE SCORES IN SHOOTER'S EDGE'S OWN LOOK: what the scores key and the level's end show when the
// packs carry no classic pictures (no glyph sheet), drawn in screen pixels in the engine's type
// rather than on the classic 320 x 200 screen, in the modern menu's colours (a dark warm slab, the
// rust accent). Every size is scaled by the frame's height (720 px is 1).
//
// - THE BOARD: one card centred near the top: a header with SCOREBOARD, the rules (free for all
//   or teams) and the map's clock; with teams, a tile a team (its name, frags, players and mean
//   ping, the player's own outlined); then the column heads (#, PLAYER, TEAM, FRAGS, TIME, PING,
//   LOSS) and a row a player by frags: the rank, the colour chip (the top colour over the bottom),
//   the name, the team, the frags large, the minutes, the ping tinted by how good it is, the loss;
//   rows striped, the player's own tinted with the accent and barred at the left; spectators named
//   on one line under the rows. Rows shrink to fit a large game.
// - THE LEVEL CARD (the intermission, the level's statistics): a card in the middle with a heading
//   (LEVEL COMPLETE, or the map's title), the map's title under it, and three tiles: KILLS and
//   SECRETS as found of total over a bar, TIME as minutes and seconds.
// - THE SOLO STRIP (the scores key in a campaign without the level card): one bar above the
//   bottom with the map's title and the three figures, each kill and secret bar filled.
//
// ge:summary The scores key and the level's end in the game's own modern look, without a pack
// ge:keywords scoreboard modern card rows colour-chip ping-tint level-card solo-strip progress-bar
// ge:keywords no-pack fallback engine-type intermission tiles
// ge:skill ui
// ge:test tests/shooters_edge_scoreboard_test.cpp
#pragma once

#include <string_view>

#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/scoreboard.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// A player colour (0..13, clamped) in the game's own hues: what the board fills with without the
// pack's palette.
ge::ui::Color player_colour(int colour);

// The board: with `teams` and teamplay on, the teams' tiles over the players.
void draw_modern_board(ge::ui::DrawList& dl, int width, int height, const Scoreboard& b,
                       bool teams);
// The level card: `heading` over `title` and the three tiles; `darken` dims the frame first.
void draw_modern_level_card(ge::ui::DrawList& dl, int width, int height, std::string_view heading,
                            std::string_view title, const LevelStats& s, bool darken);
// The solo strip above the bottom of the frame.
void draw_modern_solo_strip(ge::ui::DrawList& dl, int width, int height, std::string_view title,
                            const LevelStats& s);

}  // namespace shooters_edge::menu
