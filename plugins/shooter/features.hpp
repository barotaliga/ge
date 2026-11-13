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
//   walk's start (its speed check, a circle jump, a copied start); on.
// - `airwall`: the air's keys under a view not the pilot's (`airkeys`, `aimhop`) charged for a
//   wall or a body their velocity runs into within 0.4 s (bot.cpp); on.
// - `landwall`: a hop's landing whose jump would fly into a wall on every curve the air gives runs
//   on along the route and jumps once clear (pilot.cpp `land_walled`); dropped (even head to head,
//   no faster), in no preset.
// - `circlekeys`: a circle jump's curve on the ground under a view not the pilot's (the aim's, a
//   spot watched, the hand's) pressed with the key set the ground move carries furthest on round
//   the way (bot.cpp), not the curve's wish turned under that view; not kept, in no preset.
// - `circleview`: a circle jump's curve under the mouse hand as people's: the hand on the
//   velocity's heading, led into the turn, its keys the ground chooser's (bot.cpp); dropped
//   (even head to head), in no preset.
// - `circleaim`: a circle jump only under the hand's own view: while aiming the hop's first jump
//   goes at once (pilot.cpp `no_circle`), and a holder's spot watched gives way to a curve
//   (watch.cpp); dropped with `circleview`, in no preset.
// - `leadwall`: a target's predicted way slid along the walls it meets (bot.cpp `lead_slide`),
//   not run on through them (M18r.md 0s); on.
// - `leadmean`: a projectile led on the target's drift (his course averaged over 0.5 s) as the
//   flight lengthens, his swing's middle (bot.cpp); not kept (an expert's direct hits fell),
//   in no preset.
// - `leadarc`: a projectile led along the target's curve at his turn rate (intercept.hpp
//   `MovingTarget::turn`), a hopper's bend; not kept (even in the drill), in no preset.
// - `airwalk`: a walk in the air at speed (a hop's stretch ended mid-flight, a drop off an edge)
//   pressed with the air's key set that carries the velocity on toward the walk's target under
//   the view pressed (bot.cpp, as the hop's air), not the walk's forward key, which brakes;
//   on.
// - `airarc`: the air's key sets (`airkeys`, `aimhop`, `airwalk`) each flown held through the
// wall's
//   look (bot.cpp: the air move a command, traced every 0.1 s, clipped along a wall met) and
//   scored by the speed kept, not one command's velocity run straight on; judging.
// - `hopjump`: in a hop's air the jump let go on the way up and held on the way down, so it goes
//   off on the landing frame (pilot.cpp), a landing fast enough to hop on (M18r.md 0v); on.
// - `hopside`: no forward key in a hop: its air and its landing the strafe key alone, the one the
//   view turns toward (pilot.cpp, bot.cpp); on.
// - `noback`: moving, no spot watched or fired at (the game's watch, its rockets at news) and,
//   roaming, no player newly seen more than 100 degrees off the way walked: no back key under
//   them (M18r.md 0w); on.
// - `levelview`: a spot watched looked at within 15 degrees of level, and a look let go brought
//   back level under the hand (its tolerance under the pace's step); on.
// - `hopopen`: a hop's side away from a wall for every skill, and a circle jump curved out to the
//   side whose arc stays clear (pilot.cpp, M18r.md 0v); not kept (slower, no fewer hits), in no
//   preset.
// - `fightway`: in a fight, an enemy in sight who is not chasing (coming on under 100 units/s):
//   the route's back key let go, the dodge's side key alone (games/shooters_edge/bots/footwork.cpp,
//   M18r.md 0w); not kept (-4.2 dm2, -4.6 e1m2 head to head), in no preset.
// - `letgo`: the aim's hold on a player lost from sight let go once he lies more than 100
//   degrees off the way walked (bot.cpp, M18r.md 0w); not kept (-7.8, -3.6), in no preset.
// - `retreatrun`: backing off, one more than 100 degrees off the way walked who is not coming on
//   (under 100 units/s) is not aimed at: the run faces its way (bot.cpp, M18r.md 0w); not kept
//   (-1.9, -0.7, slower), in no preset.
// - `hide`: the launcher's owner (not a novice) holds the scattergun while no one is in sight, so
//   a pack dropped holds no launcher, and takes the launcher up at a sighting
//   (games/shooters_edge/bots/fighter.cpp, M18r.md 0k2), not until nobody has been seen 1.5 s; on.
// - `nogoal`: nothing worth taking, no item goal kept from before (a holder walking back to its
//   claim stood on a full armour with it as its goal) (fighter.cpp, M18r.md 3); moved nothing.
// - `tripkeep`: on a scripted way to its goal item, the goal held unless it backs off
//   (fighter.cpp, M18r.md 0c1); moved nothing (the YA taken no more).
// - `routetime`: an item's run weighed by its route's time (the reach's), not the straight line
//   (fighter.cpp best_item, M18r.md 3); the YA chosen less (lying 82 -> 88 %), in no preset.
// - `fetch`: the team's taker for its armour: a player whose own armour is worse, who believes
//   himself the nearest of his team that needs it as it comes up, says `coming` and holds the
//   trip to it (games/shooters_edge/bots/mind.cpp, M18r.md 3); not kept (head to head e1m2 -4.5,
//   dm2 -1.5: the trips fail on the way, not for want of a taker), in no preset.
// - `pokestand`: a button a shot opens is shot from a stand (the keys let go within 250 units),
//   never run into (games/shooters_edge/bots/buttons.cpp, M18r.md 3); on (even head to head).
// - `liftbrake`: walking to a lift's edge with the lift not there, the keys let go within the
//   run's stop, so the wait is at the edge and not in its shaft (shooter/pilot.cpp, M18r.md 3);
//   not kept (dm2's waits 125 -> 178 bot-seconds a match: the wait is the lift's), in no preset.
// - `scriptfresh`: a scripted way left and come back to is flown from its first step (a bug: the
//   step it was left at was kept, e1m2's YA climb aimed at the walkway from the floor)
//   (shooter/pilot.cpp, M18r.md 3); on (head to head e1m2 -1.0, inside the noise; a bug fixed).
// - `scriptentry`: a scripted way's entry had anywhere within 64 units once lined up, the keys let
//   go short of that (a run in from the side circled the entry's point) (shooter/nav_script.cpp,
//   M18r.md 3); not kept (e1m2's YA lying 77 -> 80 %, slower: the stop costs more), in no preset.
// - `calmvel`: moving past 150 units/s, the calm view on the pilot's walk within 8 degrees, not
//   on the route's point 320 units on (shooter/bot.cpp, M18r.md 6); not kept (enemies appearing
//   faced 52 -> 51 %, the run turns faster than the view follows), in no preset.
// - `lookturn`: the calm view's point on the route kept within 60 degrees of the pilot's walk
//   (past a node overshot or a switchback the route's point 320 units on lay 90+ degrees off the
//   walk in 58 % of the frames the view trailed the run's turn by 40+) (shooter/bot.cpp,
//   M18r.md 6); judging, in no preset.
// - `viewbook`: moving past 150 units/s, the calm view looks where people look at that spot moving
//   that way, off the map's view book (shooter/view_book.hpp; se-team `--viewbook=<file>`)
//   (shooter/bot.cpp, M18r.md 6); judging, in no preset.
// - `gapturn`: a gap trick near its lip with the run heading away from its jump is turned to on
//   the ground (1.5 s at most) before it is given up (shooter/pilot_gaps.cpp, M18r.md 0c);
//   judging, in no preset.
// - `gapnear`: a route is not planned through a gap trick whose takeoff is near (`kGapCurveFrom`)
//   unless the run already heads at it near its speed: it was planned and given up on the next
//   landing, off every route for 10 s (shooter/pilot.cpp, M18r.md 0c); on (head to head dm2
//   +6.0, e1m2 -0.2).
// - `placeheard`: a place a mate names is heard as a region, as a person hears it: a loc file
//   names a room by many points (dm2's `tele` 16 over 585 units), so the name is read as the
//   points of that name round where the speaker was last known, and as the item among them when
//   one stands there (the place a map's items name); off, the middle of every point of the name
//   (bots/mind.cpp `place_pos`, M18r.md 0c); on (head to head dm2 -1.25, e1m2 +0.5).
// - `gaplava`: with `gapbuild`, the curve into a gap trick and the run on the ground to its lip are
//   taken over lava and slime too (before, only over water: dm2's gaps are all over lava, so the
//   run reached the lip at a walk and the gap was given up, though a circle jump from there lands
//   across); the jump is still taken only where the move says it lands across
//   (shooter/pilot_gaps.cpp, M18r.md 0c); on since `brinkkeys` (even head to head on dm2, the
//   gaps over lava jumped ~0.8 a match a side; before it lava deaths rose 12.8 -> 18.3).
// - `brinkline`: the walk's look point is not carried past a node where the route walks a lip over
//   lava or slime (`NavLink::brink`): the walk turns at the route's corners there, not across
//   them (dm2's YA ramp: a run at 300 cut its curve off the edge into the lava) (shooter/pilot.cpp
//   `pursue`, M18r.md 0c); not kept (head to head dm2 -6.7, lava deaths no fewer), in no preset.
// - `sidebrink`: a walk whose side step falls into lava or slime (`NavLink::brink_side`: a ramp
//   or a walkway high over a pit, dm2's YA ramp) is a lip to the pilot and the brink guard as a
//   `brink` link is (shooter/nav.cpp `nav_mark_side_harm`, M18r.md 0c); judging, in no preset.
// - `fallseen`: the walk foreseen (`ahead`) flies each way of pressing and puts one whose box
// leaves
//   the floor over lava or slime after every other (a run down dm2's YA stair at 320 overshot a
//   corner 20 units from the lava's edge: the brink guard braked only at the edge, too late for
//   the keys) (shooter/look_ahead.cpp, M18r.md 0c); not kept (over `brinkkeys` head to head dm2
//   +1.5, -2.8 / +5.7, lava deaths no fewer), in no preset.
// - `liplook`: near lava or slime the walk's look point runs 0.3 s ahead at its speed (not 64
//   units): a corner by the edge is turned early and inside, not overshot (shooter/pilot.cpp,
//   M18r.md 0c); judging, in no preset.
// - `brinkkeys`: near lava or slime the keyboard's hand does not want a key whose step under the
//   view ends over it, and answers a run carried over it within a quarter second with the keys
//   against the run (bots/keyboard.cpp `keys_off_brink`; the keyboard's 45-degree sets and taps
//   weave a walk off a line 40 from the edge: se-reach --walk, M18r.md 0c); on (head to head dm2
//   +6.2, lava deaths 12.75 -> 9.0 a match).
// - `floortrap`: a floor over lava a touch button slides away (dm2's big room) is never brushed by
//   accident (the links into the nodes touching its button kept off routes), its button pressed
//   only when an enemy stands on the floor and no mate does, the presser staying put till the
//   floor returns, its teleporters shut and kept off while it is away, a `trap` call to the team
//   (bots/floor_trap.hpp, M18r.md 0c); on (head to head dm2 +5.0, the enemy's lava deaths +3).
// - `trapflee`: a player standing on a floor trap's floor when it goes (as it sees it, or a mate's
//   call) runs off it at once by the nav's shortest way to the ground round it, with the floor's
//   slide and not against it, before any goal (bots/floor_trap.hpp; dm2: 47 of 82 trap deaths
//   stood on the floor as it went and fell ~3 s later, M18r.md 0c); on (head to head dm2 -2.7,
//   inside the noise; its side's lava deaths fewer).
// - `bridgeseen`: a bridge the player is no longer sent is believed gone once one of its nodes is
//   in sight with no floor under it (a client keeps an unsent mover where it last saw it: dm2's red
//   armour's bridge stood out in the bot's mind after it went back, and routes walked off the lip
//   into the pit; and the links shut onto a bridge or a floor door still in honoured, which no
//   route did: they carry no mover; bots/fighter.cpp `bridge_seen_gone`, M18r.md 0c); on (head to
//   head dm2 +7.4, e1m2 -0.3).
// - `sidehop`: a hop's stretch ends at a walk whose side step falls into lava or slime
//   (`NavLink::brink_side`), walked, not hopped (dm2's yellow-armour stair: most of its falls came
//   out of a hop or a copied hop start; shooter/pilot.cpp `hop_stretch`, M18r.md 0c); not kept
//   (YA falls 24 -> 20 over 8 seeds, noise; the hop share 0.45 -> 0.39, people 0.46), in no preset.
// - `lostdry`: off the graph on dry land, the walk back to the nearest node never takes one in
//   lava or slime or on a mover (dm2's south pit: its trains' nodes over the lava were walked at;
//   bots/fighter.cpp, M18r.md 0c); not kept (the pit's falls 16 -> 22 over 8 seeds), in no preset.
// - `liftoff`: a jump asked for and not yet off the ground (the keyboard's jump key lands a few
//   commands late) is waited for 0.15 s, the jump still asked (on: M18r.md 0c); off, the next
//   command on the ground reads as the jump landed short and marks it missed (shooter/pilot.cpp,
//   M18r.md 0c).
// - `shotjump`: a rocket jump's shot held, the mouse still on it, until the jump key is down: a
//   person presses the two as one move; off, the shot went with the ask and burst under a player
//   still on the ground, a flat run into dm2's south pit (bots/fighter.cpp, M18r.md 0c).
// - `rjready`: a rocket jump only with the launcher in hand and ready to fire; off, the shot went
//   with whatever was in hand (shooter/pilot.cpp, bots/fighter.cpp; M18r.md 0c).
// - `paceback`: under the mouse hand, an aim lost or a look let go goes back to the way by the
//   hand's own stroke onto it (a want paced from the view gave one small stroke a look: the view
//   hung where the enemy was lost); on.
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
// ge:keywords chains goal-chain retarget goal-change uturn turn-back reversal jumpfly run-up-dance gappast lipjump airgap pilotcast aimhop airkeys airview chainhop airwall landwall circlekeys circleview circleaim leadwall leadmean leadarc airwalk airarc hopjump hopside noback levelview hopopen fightway fight-way letgo aim-hold paceback retreatrun hide weapon-hide nogoal tripkeep routetime fetch taker armour-taker pokestand button-shot liftbrake lift-shaft scriptfresh script-step scriptentry script-entry calmvel calm-view facing lookturn view-lead viewbook gapturn gap-refused gapnear placeheard place-heard loc-region gaplava gap-over-lava brinkline lip-corner sidebrink side-step fallseen fall-foreseen liplook brinkkeys floortrap floor-trap trapflee trap-flee bridgeseen bridge-seen sidehop side-hop lostdry lost-dry liftoff lift-off shotjump shot-jump rjready launcher-ready
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
  landwall,
  circlekeys,
  circleview,
  circleaim,
  leadwall,
  leadmean,
  leadarc,
  airwalk,
  airarc,
  hopjump,
  hopside,
  noback,
  levelview,
  hopopen,
  fightway,
  letgo,
  paceback,
  retreatrun,
  hide,
  nogoal,
  tripkeep,
  routetime,
  fetch,
  pokestand,
  liftbrake,
  scriptfresh,
  scriptentry,
  calmvel,
  lookturn,
  viewbook,
  gapturn,
  gapnear,
  placeheard,
  gaplava,
  brinkline,
  sidebrink,
  fallseen,
  liplook,
  brinkkeys,
  floortrap,
  trapflee,
  bridgeseen,
  sidehop,
  lostdry,
  liftoff,
  shotjump,
  rjready,
  trial,
};
constexpr size_t kFeatures = 115;
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
