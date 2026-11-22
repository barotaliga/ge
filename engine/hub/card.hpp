// A MATCH'S CARD AS A PICTURE (docs/hub.md § Match cards): the still of the match's map
// (stills.hpp: rendered once a map by the game and cached), darkened under a band, and over it the
// live board drawn with the game's own charset in the game's own look: the mode on the map and the
// clock at the top, then a row a player (ping, frags, the team when the match is played in teams,
// the name), the spectators' names at the foot. The still is pre-rendered; the board is composed
// from the status record every time it is asked for, so it is as live as the poller.
//
// THE CHARSET is a sheet of 16 x 16 glyph cells (a cell is the sheet's width / 16), glyph `c` at
// column c % 16, row c / 16, its see-through pixels alpha 0: every classic game's console font is
// one. A name's bytes are drawn as they are (a byte over 127 is the game's second set of glyphs),
// so a name in coloured letters reads as it does in the game.
//
// THE LOOK a game gives (`CardLook`): its charset, whether a player's frags sit on a box of his
// colour (Quake's board) or bare (Quake II's), and whether the column words are in the second set
// (Quake II's green). The board is drawn at the charset's own pixels and scaled up by a whole
// number (the largest at which the rows fit, at most the card's height over 90 of the
// charset's pixels: six for a duel on a 540 px card, three for a 4on4) with no filtering, so each
// texel is a crisp square as in the game; the still under it is the card's own size.
//
// THE TEAM COLUMN: shown only when the match is played in teams (two teams in the record, or two
// team names among its players), never in a duel or a free-for-all; the players are then grouped
// by team, the team with more frags first.
//
// ge:summary A match's card as a picture: the map's still, the live board in the game's charset
// ge:keywords hub card match-card picture still charset conchars scoreboard ping frags team name
// ge:keywords compose_card card_jpeg card-look score-box glyph-sheet spectators intermission-view
// ge:skill net ui
// ge:test tests/hub_card_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "hub/spec.hpp"
#include "net/status.hpp"

namespace ge::hub {

// An RGBA picture, row-major, y down.
struct CardPicture {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // width * height * 4
};

struct CardLook {
  CardPicture charset;          // 16 x 16 cells; empty: the hub draws no card picture for the game
  bool score_box = true;        // frags on a box of the player's (else his team's) colour
  bool words_alt = false;       // the column words and the clock in the charset's second set
  uint32_t box_rgb = 0x5f4f3f;  // a box's colour when neither the player nor his team has one
  // The shade of each colour row a player may pick (StatusPlayer::top and bottom), 0xRRGGBB, from
  // the game's own palette; empty: the box takes the player's or the team's colour.
  std::vector<uint32_t> rows;
};

// Whether the record's match is played in teams (the team column shown).
bool card_teams(const net::Status& s);

// The card for `s` at `width` x `height` over `still` (any size: scaled to cover, centred; empty:
// a dark ground of the map's hue).
CardPicture compose_card(const HubSpec& spec, const net::Status& s, const CardLook& look,
                         const CardPicture& still, int width, int height);

// A picture as JPEG bytes (baseline, 4:4:4 so the letters keep their edges; alpha dropped).
std::vector<uint8_t> card_jpeg(const CardPicture& picture);

}  // namespace ge::hub
