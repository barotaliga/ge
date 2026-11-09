// A BOT'S FEATURES: each behaviour a bot has gained, switched on or off by name, so a bot can be
// made plainer or fuller and every behaviour judged by playing with it and without it from one
// build. The traits (traits.hpp) say how WELL a player does a thing; a feature says WHETHER the bot
// does it at all: a feature on still plays at the traits' level (a rocket jump needs the moves
// trait as well), and a feature off is that behaviour gone, the bot playing on without it.
//
// THE FEATURES (`Feature`, by the name a command line or a console writes):
// - `calm`: the calm view: walking with nobody to aim at, the view a hand on a mouse looking along
//   the route (bot.hpp `calm`); off, the view is the route's heading.
// - `pace`: the view at a person's pace: back from a lost enemy and while swimming, turned at a
//   hand's speed rather than the aim's flick.
// - `swimaim`: fighting in water with the view on the enemy, the swim's way pressed under it.
// - `screen`: a 120-degree screen of sight (16:9); off, a round cone 100 degrees across.
// - `keep`: the target in hand kept unless another in sight is half as far.
// - `lead`: shots led at where the target will be and along his arc in the air.
// - `lane`: walking a lane beside the route's line, as a person does; off, on the line.
// - `hop`: the bunny hop (an expert's) or the simple hop (an average player's).
// - `shortgoals`: health, armour, a missing weapon, the mega or rockets ahead taken on the way.
// - `commit`: an item goal held until news (a sighting, a pickup, a blow).
// - `danger`: where it or a mate died, and areas called lost, kept from healing at for a while.
// - `dodge`: the side steps across an enemy's line in a fight.
// - `watch`: the view on the ways in where it stands, nobody in sight.
// - `spam`: rockets fired at the spots enemies come in by, and at news of one coming.
// - `packs`: launchers in backpacks called and fetched by who needs one; the bore for a quad mate.
// - `talk`: team lines said and read (status, calls, help, plans).
// - `plan`: the map's team plan: roles, areas held, the opening, the quad's rush.
// - `rj`: rocket jumps (the nav's rocket links and tricks).
// - `gj`: grenade jumps.
// - `airsteer`: flights steered in the air with the side key and the view on the velocity; off,
//   the view at the target and forward.
// - `surface`: up for air under open water before the air runs out.
// - `glance`: the calm walker's swipe aside now and then, to the longest open line (bot.hpp).
// - `eye`: a strafer's turn read a reaction late (senses.hpp `course_s`); off in every preset.
// - `selfaim`: the aim worked out from where its own eye will be as the hands catch up.
// - `pools`: pool floors swum over, the swim's wish pressed in 3D (pilot.hpp, bot.hpp `pools`).
// - `routes`: a map's scripted ways to key items taken (nav_script.hpp, bots/item_routes).
// - `corner`: an enemy lost from sight followed a reaction's worth, then held where he went.
// - `mouse`: the view a hand's strokes on a mouse, shots timed, fire clicked (bot.hpp).
// - `packinfer`: every death's pack known, its weapon guessed by level (bots/packs.hpp).
// - `listen`: a calm bot swipes its view at an enemy heard where none was known (bot.hpp).
// - `stairs`: a hop's stretch ends at a climb: stairs walked, the hop taken up at the top.
// - `locked`: a door only a button or a trigger opens, resting shut, is no way (bots/buttons.hpp).
// - `trial`: read by an A/B now; off in every preset (`all` too): `--features-red=+trial`.
//
// THE PRESETS: `features_all` (every one), each skill's (`features_of`: every one but what that
// level of player does not do: none `eye`, a novice no `glance`; the rest of a skill's differences
// are its traits'), `features_none` (a plain bot: it still roams, fights, heals and takes items).
// The text form (`apply_features`) reads `all`, `none` (or `plain`), `+name` or `name` (on),
// `-name` (off), separated by commas, spaces or '/', applied in order: "plain,+dodge",
// "-swimaim,+shortgoals".
//
// ge:summary A bot's features: each behaviour switched on or off by name, and the presets
// ge:keywords features feature-switch bot-features apply_features features_text presets routes
// ge:keywords calm pace swimaim screen keep lead lane hop shortgoals commit danger dodge watch
// ge:keywords spam packs talk plan rocket-jump grenade-jump airsteer surface glance plain-bot
// ge:keywords ab-test trial eye selfaim course-change own-motion pools pool-floor swim-under-view
// ge:keywords corner lost-enemy wall-hold mouse hand-strokes click packinfer pack-guess listen
// ge:keywords sound-swipe stairs stair-walk locked shut-door
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
  trial,
};
constexpr size_t kFeatures = 33;
// The feature's name as a command line or a console writes it ("swimaim").
const char* feature_name(Feature f);

struct Features {
  uint64_t bits = 0;  // room for 64 (8 bytes: a fighter's struct packs tight with it)
  bool operator[](Feature f) const { return (bits >> static_cast<uint64_t>(f)) & 1u; }
  void set(Feature f, bool on) {
    const uint64_t b = uint64_t{1} << static_cast<uint64_t>(f);
    bits = on ? bits | b : bits & ~b;
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
