// FALL LINE'S HUD AS DRAWN: a snapshot (snapshot.hpp) laid out over the running picture with
// the engine's HUD kit (engine/hud), in the game's own look (`hud_style`: the rounded charset,
// white over a navy drop shadow, the signals green and red).
//
//   top left      the map's SEED and where he stands; the run under his skis and the metres
//                 skied; the run's best air once he has landed one
//   top centre    the AIR CLOCK while a flight lasts (red while it beats the run's best); on a
//                 phone held upright it drops under the corners' columns
//   top right     the WANTED LEVEL while the army hunts him: five stars (the lit ones blinking
//                 while they search), the search draining under them, the survival timer
//   upper centre  the PROMPT: down, stuck, waiting for a chair, dead, arrested
//   bottom left   the SPEED in km/h (or the machine's, captioned with its name) with the EDGE
//                 bar over it, centred: left edges fill left, right edges right
//   bottom right  the NEWS: a landing's air and load, a wipeout, a tree brushed, onto a run,
//                 the top of a lift, the hunt's turns
//
// The HUD is DATA (engine/hud/layout.hpp): `hud_layout` declares every element above with its
// place, `fill_hud` writes their content from the snapshot once a frame, and the player moves,
// resizes, hides and fades any of them through its `hud_<id>_` cvars (`add_hud_cvars`). The ids:
// seed run best_air stars search timer air lift speed edge height news prompt.
//
// Every box it draws is returned by id for the layout check (`ge::hud::layout_problems`); the
// `hud` lab fails a HUD whose boxes overlap or leave the screen at any of the three reference
// viewports. Every word comes from words.hpp.
//
// ge:summary Fall Line's HUD drawn: the free ride's figures laid out in the game's look
// ge:keywords hud view draw_hud hud_style layout seed air-clock wanted-stars speed edge-bar
// ge:keywords prompt news portrait landscape hud_layout fill_hud elements cvars
// ge:skill ui
// ge:test tests/fall_line_hud_test.cpp
#pragma once

#include <vector>

#include "fall_line/hud/snapshot.hpp"
#include "hud/hud.hpp"
#include "hud/layout.hpp"

namespace fall_line::hud {

ge::hud::Style hud_style();
NewsInk news_ink(const ge::hud::Style& style);

// Fall Line's elements, by index, in the order they are declared (and stacked).
struct HudLayout {
  ge::hud::Layout layout;
  int seed, run, best_air, stars, search, timer, air, lift, speed, edge, height, news, prompt;
};

// The game's HUD: every element in its default place.
HudLayout hud_layout(const ge::hud::Style& st);
// Each element's content from the snapshot; `run`'s feed must outlive the next draw.
void fill_hud(HudLayout& h, const ge::hud::Style& st, const HudSnapshot& s, const HudRun& run);

// A HUD in the game's default places, filled and drawn: for a lab or a test.
std::vector<ge::hud::Placed> draw_hud(ge::ui::DrawList& dl, const ge::hud::Frame& f,
                                      const ge::hud::Style& st, const HudSnapshot& s,
                                      const HudRun& run);

}  // namespace fall_line::hud
