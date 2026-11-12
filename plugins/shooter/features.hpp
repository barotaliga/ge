// A BOT'S FEATURES: each behaviour a bot has gained, switched on or off by name, so a bot can be
// made plainer or fuller and every behaviour judged by playing with it and without it from one
// build. The traits (traits.hpp) say how WELL a player does a thing; a feature says WHETHER the bot
// does it at all: a feature on still plays at the traits' level (a rocket jump needs the moves
// trait as well), and a feature off is that behaviour gone, the bot playing on without it.
//
// THE FEATURES (`Feature`, by the name a command line or a console writes):
// - `calm`: with nobody to aim at, the view a hand on a mouse looking along the route (bot.hpp).
// - `pace`: the view at a hand's pace, not the aim's flick, back from a lost enemy and swimming.
// - `swimaim`: fighting in water with the view on the enemy, the swim's way pressed under it.
// - `screen`: a 120-degree screen of sight (16:9); off, a round cone 100 degrees across.
// - `keep`: the target kept unless another in sight is half as far; `lead`: shots led on him.
// - `lane`: walking a lane beside the route's line, as a person does; off, on the line.
// - `hop`: the bunny hop (an expert's) or the simple hop (an average player's).
// - `shortgoals`: health, armour, a missing weapon, the mega or rockets ahead taken on the way.
// - `commit`: an item goal held until news (a sighting, a pickup, a blow).
// - `danger`: where it or a mate died, and areas called lost, kept from healing at for a while.
// - `dodge`: side steps across an enemy's line in a fight; `watch`: the view on the ways in;
//   `spam`: rockets fired at the spots enemies come in by, and at news of one coming.
// - `packs`: launchers in backpacks called and fetched by who needs one; the bore for a quad mate.
// - `talk`: team lines said and read; `plan`: the map's team plan (roles, areas, the quad's rush).
// - `rj`, `gj`: rocket and grenade jumps; `airsteer`: flights steered by the side key.
// - `surface`: up for air under open water before the air runs out. `eye`: a strafer's turn read
//   a reaction late (senses.hpp `course_s`; off in every preset).
// - `glance`: the calm walker's swipe aside now and then, to the longest open line (bot.hpp).
// - `selfaim`: the aim worked out from where its own eye will be as the hands catch up.
// - `pools`: pool floors swum over (pilot.hpp); `routes`: a map's scripted ways (nav_script.hpp).
// - `corner`: an enemy lost from sight followed a reaction's worth, then held where he went;
//   `mouse`: the view a hand's strokes on a mouse, shots timed, fire clicked (bot.hpp).
// - `packinfer`: every death's pack known, its weapon guessed by level (bots/packs.hpp).
// - `listen`: a calm bot swipes its view at an enemy heard where none was known (bot.hpp).
// - `stairs`: a hop's stretch ends at a climb: stairs walked, the hop taken up at the top.
// - `locked`: a door only a button or a trigger opens, resting shut, is no way (bots/buttons.hpp).
// - `escape`: out of a pit's lava, slime or water by a rocket fired down (shooter/escape.hpp).
// - `brink`: no lip of lava or slime walked, hopped or strafed over (shooter/brink.hpp).
// - `gapjump`: a hopper takes people's speed jumps over a pit, out of a hop (shooter/nav_gaps.hpp).
// - `headroom`: no hop's jump into a lintel, nor its last one onto a stair (pilot.hpp).
// - `liphop`: a hop runs on along a lava lip, the brink guard on its feet (pilot.hpp).
// - `circlejump`: a hop's first jump out of a curve on the ground, past a run's 320 (pilot.hpp).
// - `ahead`: the walk's keys flown through the move before a bump; `bodies`: players near too.
// - `beat`: a fight mode held a person's beat (through a peek). `trial`: an A/B's, in no preset.
// - `keys`: a keyboard's hand presses the keys; `keysteer`: taps between two sets; `jumpkey`: a
//   jump held until its key lands; `curvekeys`: a hop's curve on the ground held on W and its side
//   key (bots/keyboard.hpp; all on). `lookway`, `press`, `counter`: dropped, in no preset.
// - `slopes`: a stair to 45 degrees level to the walk; `backstep`: a run-up stepped back facing
//   the jump; `gapbuild`: a gap over water curved and run into (pilot.hpp; all on).
// - `recipes`: people's hop starts copied out of a book of their recordings (pilot_recipes.hpp;
// on).
// - `chains`: an item goal planned on through its pickup to the goal after it, so the walk and the
//   hop run through the item (pilot.hpp `pilot_chain`; fighter.cpp): dropped, in no preset.
// - `retarget`: a new goal run on to from the route under way, the walk or the hop kept (pilot.hpp
//   `pilot_retarget`), not planned again from a stop's state: dropped, in no preset.
// - `uturn`: a turn back at a run pressed across the velocity, curved through at speed, not run
//   through a stop (fighter.cpp `turn_back_round`; on).
// - `jumpfly`: a run at a jump off its line or its speed jumps anyway when the jump, flown by the
//   move from where it is, lands (pilot.cpp `approach_jump`), not lined up again; on.
// - `gappast`: a gap's jump that comes down on past its far node, on its floor, lands
//   (pilot_gaps.hpp `gap_lands`); judging.
// - `lipjump`: a hop on the ground about to run off its floor jumps at the lip (pilot.cpp);
// judging.
// - `airgap`: a gap's jump forecast and flown strafed across (nav_gaps.hpp `fly_jump`); judging.
// - `pilotcast`: a hop's jump at a gap judged by a COPY of the pilot flying it on a copy of the
//   move (its hands, its strafe, its view: pilot_gaps.hpp `hop_cast`), not a forecast held forward;
//   judging.
// - `aimhop`: the hop kept while aiming (bot.cpp): the landings jumped, and in the air the key
//   set under the aim's view that best carries the velocity on round the hop's way; on.
// - `airkeys`: a hop's air under the hand's view (bot.cpp): the pilot's side key, alone or with
//   forward, pressed only while it carries the velocity on under the view pressed, else let go;
//   on.
// - `airview`: in a hop's air the mouse hand aims at the velocity's heading, a little toward the
//   turn (bot.cpp), not at the pilot's view, already late by the pilot's own hands; and the air's
//   keys chosen for the view when they land; on.
// - `chainhop`: a hop's landing with a hop stretch ahead jumps again at once (pilot.cpp), not a
//   walk's start (its speed check, a circle jump, a copied start); judging.
// - `airwall`: the air's keys under a view not the pilot's (`airkeys`, `aimhop`) charged for a
//   wall or a body their velocity runs into within 0.4 s (bot.cpp); on.
// - `runupkeys`, `underway`: judging, in no preset. The numbers: docs/goals/M18r.md.
// THE PRESETS: `features_all` (every one), each skill's (`features_of`: none `eye`, a novice no
// `glance`), `features_none` (a plain bot). Text (`apply_features`): all, none/plain, +x, -x.
// ge:summary A bot's features: each behaviour switched on or off by name, and the presets
// ge:keywords features feature-switch bot-features apply_features features_text presets keys calm
// ge:keywords pace swimaim screen keep lead lane hop shortgoals commit danger dodge watch spam
// ge:keywords packs talk plan rocket-jump grenade-jump airsteer surface glance plain-bot ab-test
// ge:keywords trial eye selfaim course-change own-motion pools pool-floor swim-under-view corner
// ge:keywords lost-enemy wall-hold mouse hand-strokes click packinfer pack-guess listen sound-swipe
// ge:keywords stairs stair-walk locked shut-door escape lava-escape brink lava-lip gapjump headroom
// ge:keywords liphop circlejump circle-jump ahead look-ahead walk-bumps corner-cut bodies beat
// ge:keywords slopes counter backstep run-up runupkeys gapbuild curvekeys underway recipes
// ge:keywords chains goal-chain retarget goal-change uturn turn-back reversal jumpfly run-up-dance gappast lipjump airgap pilotcast aimhop airkeys airview chainhop airwall
// ge:skill shooter bots
// ge:test tests/shooter_features_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "shooter/aim.hpp"

namespace ge::shooter {

enum class Feature : uint8_t {
  calm,
  pace,
  swimaim,
  screen,
  keep,
  lead,
  lane,
  hop,
  shortgoals,
  commit,
  danger,
  dodge,
  watch,
  spam,
  packs,
  talk,
  plan,
  rj,
  gj,
  airsteer,
  surface,
  glance,
  eye,
  selfaim,
  pools,
  routes,
  corner,
  mouse,
  packinfer,
  listen,
  stairs,
  locked,
  escape,
  brink,
  gapjump,
  headroom,
  liphop,
  circlejump,
  ahead,
  bodies,
  beat,
  keys,
  keysteer,
  lookway,
  press,
  slopes,
  counter,
  jumpkey,
  backstep,
  runupkeys,
  gapbuild,
  curvekeys,
  underway,
  recipes,
  chains,
  retarget,
  uturn,
  jumpfly,
  gappast,
  lipjump,
  airgap,
  pilotcast,
  aimhop,
  airkeys,
  airview,
  chainhop,
  airwall,
  trial,
};
constexpr size_t kFeatures = 68;
// The feature's name as a command line or a console writes it ("swimaim").
const char* feature_name(Feature f);

struct Features {
  uint64_t bits[2] = {0, 0};  // room for 128, a bit a feature by its number
  constexpr bool operator[](Feature f) const {
    const auto k = static_cast<uint64_t>(f);
    return (bits[k >> 6] >> (k & 63)) & 1u;
  }
  constexpr void set(Feature f, bool on) {
    const auto k = static_cast<uint64_t>(f);
    const uint64_t b = uint64_t{1} << (k & 63);
    bits[k >> 6] = on ? bits[k >> 6] | b : bits[k >> 6] & ~b;
  }
  bool operator==(const Features&) const = default;
};

// Every feature on (but `trial`).
Features features_all();
// None: a plain bot.
Features features_none();
// The skill's preset: every feature but a novice's glance (the skills differ in their traits too).
Features features_of(AimSkill skill);

// Applies a list of words to `f` ("plain,+dodge", "-swimaim"); false with `error` set (naming
// every feature) on an unknown word, `f` then as far as it got.
bool apply_features(std::string_view text, Features& f, std::string& error);
// The features as text: "all", "none", or the shorter of "all,-a,-b" and "none,+a,+b".
std::string features_text(const Features& f);

}  // namespace ge::shooter
