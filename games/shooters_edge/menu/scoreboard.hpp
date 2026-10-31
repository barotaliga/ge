// THE MULTIPLAYER SCOREBOARD (porting/shooters-edge/scoreboard.md): what the scores key shows in a
// match, and what a dead player sees, drawn on the classic 320 x 200 screen (scaled by a whole
// number and centred, classic_view.hpp) in the pack's own pictures and glyph sheet
// (classic_art.hpp), or in the engine's type when the packs carry none. No background: the view
// shows through.
//
// - THE PLAYERS' BOARD: the `ranking` picture centred at the top (y 0); at y 24 the header
//   `ping pl time frags name` (with teams, `ping pl time frags team name`), at y 32 a rule under
//   each word; then a row a player from y 40, 10 px apart (8 px once the rows reach the bottom:
//   the large game, kept for the session in `BoardView`): the ping in four characters, the loss in
//   three (the sheet's second set over 25%), the minutes on the server in four, the colour box
//   (40 x 8: the top colour over the bottom, each the palette's index colour x 16 + 8, the colour
//   clamped to 0..13) with the frags in three characters inside it and gold brackets round the
//   player's own, then the team's four characters and the name. A spectator's row is its ping,
//   its loss, `(spectator)` and its name. The left edge is x 16, or x 4 with teams.
// - THE TEAMS' BOARD (teamplay on, dead): the picture, the header `low/avg/high team total
//   players` from x 36, a row a team (its pings' lowest, mean and highest, its name, its frags,
//   its players; the player's own bracketed), a blank line, then the players' board with teams.
// - THE SCOREBAR: the status bar's place taken by the wad's `scorebar` strip with `Time :` and
//   the map's minutes and seconds at (184, 180).
//
// THE ORDER: every player by frags, the most first, a tie in slot order; spectators last.
//
// ge:summary The multiplayer scoreboard: the players' and the teams' boards and the scorebar
// ge:keywords scoreboard scores showscores ranking frags ping packet-loss team teamplay spectator
// ge:keywords colour-box topcolor bottomcolor large-game scorebar classic 320x200 tab
// ge:skill ui
// ge:test tests/shooters_edge_scoreboard_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::menu {

// One player as the board lists him.
struct ScoreRow {
  std::string name{};
  int frags = 0;
  int ping = 0;             // ms
  int loss = 0;             // packet loss, 0..100
  int minutes = 0;          // on the server
  int top = 0, bottom = 0;  // the player's colours, 0..15
  std::string team{};
  bool spectator = false;
  bool self = false;  // the local player
};

struct Scoreboard {
  std::vector<ScoreRow> rows;  // by slot
  bool teamplay = false;
  int minutes = 0, seconds = 0;  // the map's clock, for the scorebar
};

// One team as the teams' board lists it.
struct TeamRow {
  std::string team{};
  int frags = 0, players = 0;
  int low = 999, mean = 999, high = 0;  // its players' pings, ms
  bool self = false;                    // the local player's team
};

// What the board keeps between frames: the large game, once on, stays on.
struct BoardView {
  bool large = false;
};

// The rows in the board's order: frags, the most first, a tie in slot order; spectators last.
std::vector<size_t> score_order(const Scoreboard& b);
// The teams of the players with a team (spectators left out), in the order they first appear,
// sorted by frags, the most first.
std::vector<TeamRow> team_rows(const Scoreboard& b);
// The palette index a player's colour fills with.
int colour_index(int colour);

// The players' board over a `width` x `height` frame.
void draw_scoreboard(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                     const Scoreboard& b, BoardView& view);
// The teams' board, then the players' (the players' alone without teamplay).
void draw_team_scoreboard(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                          const Scoreboard& b, BoardView& view);
// The scorebar in the status bar's place, with the map's clock.
void draw_scorebar(ge::ui::DrawList& dl, const ClassicArt& art, int width, int height,
                   const Scoreboard& b);

}  // namespace shooters_edge::menu
