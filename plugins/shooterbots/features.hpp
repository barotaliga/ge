// A BOT'S FEATURES: each behaviour a bot has gained, switched on or off by name, so a bot can be
// made plainer or fuller and every behaviour judged by playing with it and without it from one
// build. The traits (traits.hpp) say how WELL a player does a thing; a feature says WHETHER the bot
// does it at all: a feature on still plays at the traits' level (a rocket jump needs the moves
// trait as well), and a feature off is that behaviour gone, the bot playing on without it.
//
// THE FEATURES, each with what it does and its judgment: above `Feature` below.
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
// ge:keywords slopes counter backstep run-up runupkeys gapbuild curvekeys underway recipes chains
// ge:keywords goal-chain retarget goal-change uturn turn-back reversal jumpfly run-up-dance gappast
// ge:keywords lipjump airgap pilotcast aimhop airkeys airview chainhop airwall landwall airend
// ge:keywords circlekeys
// ge:keywords circleview circleaim leadwall leadmean leadarc airwalk airarc hopjump hopside noback
// ge:keywords levelview hopopen fightway fight-way letgo aim-hold paceback retreatrun hide weapon-
// ge:keywords hide nogoal tripkeep routetime fetch taker armour-taker pokestand button-shot
// ge:keywords liftbrake lift-shaft scriptfresh script-step scriptentry script-entry calmvel calm-
// ge:keywords view facing lookturn view-lead viewbook gapturn gap-refused gapnear placeheard place-
// ge:keywords heard loc-region gaplava gap-over-lava brinkline lip-corner sidebrink side-step
// ge:keywords fallseen fall-foreseen liplook brinkkeys floortrap floor-trap trapflee trap-flee
// ge:keywords bridgeseen bridge-seen sidehop side-hop lostdry lost-dry liftoff lift-off shotjump
// ge:keywords shot-jump rjready launcher-ready jumpcuts failed-jumps brinkfall fall-into-harm
// ge:skill shooterbots bots
// ge:test tests/shooterbots_features_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "shooterbots/aim.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots {

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
// - `packinfer`: every death's pack known, its weapon guessed by level (shooterbots/packs.hpp).
// - `listen`: a calm bot swipes its view at an enemy heard where none was known (bot.hpp).
// - `stairs`: a hop's stretch ends at a climb: stairs walked, the hop taken up at the top.
// - `locked`: a door only a button or a trigger opens, resting shut, is no way
// (shooterbots/buttons.hpp).
// - `escape`: out of a pit's lava, slime or water by a rocket fired down (shooterbots/escape.hpp).
// - `brink`: no lip of lava or slime walked, hopped or strafed over (shooterbots/brink.hpp).
// - `gapjump`: a hopper takes people's speed jumps over a pit, out of a hop
// (shooterbots/nav_gaps.hpp).
// - `headroom`: no hop's jump into a lintel, nor its last one onto a stair (pilot.hpp).
// - `liphop`: a hop runs on along a lava lip, the brink guard on its feet (pilot.hpp).
// - `circlejump`: a hop's first jump out of a curve on the ground, past a run's 320 (pilot.hpp).
// - `ahead`: the walk's keys flown through the move before a bump; `bodies`: players near too.
// - `beat`: a fight mode held a person's beat (through a peek). `trial`: an A/B's, in no preset.
// - `keys`: a keyboard's hand presses the keys; `keysteer`: taps between two sets; `jumpkey`: a
//   jump held until its key lands; `curvekeys`: a hop's curve on the ground held on W and its side
//   key (shooterbots/keyboard.hpp; all on). `lookway`, `counter`: dropped, in no preset; `press` on
//   with `presspair` (2026-11-20, the maintainer's call).
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
//   set under the aim's view that best carries the velocity on round the hop's way; OFF since
//   2026-11-19 (a fight hops no more: head to head +3.4 without it, speed 248 -> 253).
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
//   on along the route and jumps once clear (pilot.cpp `land_walled`, 0.35 s ahead). At 0.15 s it
//   was dropped (even head to head, no faster). ON at 0.35 s (2026-11-21, M18r.md P2: dm2's
//   corridor ends, where chained landings flew into the end wall): 12 2on2 dm2 seeds, speed off
//   the recordings 270.9 -> 275.1 (people 291), the hop's fast flights crashing 40.2 -> 39.3 % and
//   keeping -22.7 -> -17.9 of their speed, mid-air crashes 803 -> 729, wall bumps 4.10 -> 4.21 a
//   minute (people 2.9), head to head +1.8 (red without -2.8, blue without +6.5).
// - `circlekeys`: a circle jump's curve on the ground under a view not the pilot's (the aim's, a
//   spot watched, the hand's) pressed with the key set the ground move carries furthest on round
//   the way (bot.cpp), not the curve's wish turned under that view; not kept, in no preset.
// - `circleview`: a circle jump's curve under the mouse hand as people's: the hand on the
//   velocity's heading, led into the turn, its keys the ground chooser's (bot.cpp); on
//   (re-judged 2026-11-19: own-view circles take off at 371 not 339, head to head +1.8).
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
//   the route's back key let go, the dodge's side key alone (plugins/shooterbots/footwork.cpp,
//   M18r.md 0w); not kept (-4.2 dm2, -4.6 e1m2 head to head), in no preset.
// - `letgo`: the aim's hold on a player lost from sight let go once he lies more than 100
//   degrees off the way walked (bot.cpp, M18r.md 0w); not kept (-7.8, -3.6), in no preset.
// - `retreatrun`: backing off, one more than 100 degrees off the way walked who is not coming on
//   (under 100 units/s) is not aimed at: the run faces its way (bot.cpp, M18r.md 0w); not kept
//   (-1.9, -0.7, slower), in no preset.
// - `hide`: the launcher's owner (not a novice) holds the scattergun while no one is in sight, so
//   a pack dropped holds no launcher, and takes the launcher up at a sighting
//   (plugins/shooterbots/fighter.cpp, M18r.md 0k2), not until nobody has been seen 1.5 s; on.
// - `nogoal`: nothing worth taking, no item goal kept from before (a holder walking back to its
//   claim stood on a full armour with it as its goal) (fighter.cpp, M18r.md 3); moved nothing.
// - `tripkeep`: on a scripted way to its goal item, the goal held unless it backs off
//   (fighter.cpp, M18r.md 0c1); moved nothing (the YA taken no more).
// - `routetime`: an item's run weighed by its route's time (the reach's), not the straight line
//   (fighter.cpp best_item, M18r.md 3); the YA chosen less (lying 82 -> 88 %), in no preset.
// - `fetch`: the team's taker for its armour: a player whose own armour is worse, who believes
//   himself the nearest of his team that needs it as it comes up, says `coming` and holds the
//   trip to it (plugins/shooterbots/mind.cpp, M18r.md 3); not kept (head to head e1m2 -4.5,
//   dm2 -1.5: the trips fail on the way, not for want of a taker), in no preset. Again with the
//   door errands fixed (`errandworth`) and `arrive`, 12 2on2 dm2 seeds: -1.7 (red +1.0, blue
//   -4.3); 88 trips a match, 11 taken, 35 ended by the quad's rush, 31 by death.
// - `pokestand`: a button a shot opens is shot from a stand (the keys let go within 250 units),
//   never run into (plugins/shooterbots/buttons.cpp, M18r.md 3); on (even head to head).
// - `liftbrake`: walking to a lift's edge with the lift not there, the keys let go within the
//   run's stop, so the wait is at the edge and not in its shaft (shooterbots/pilot.cpp, M18r.md 3);
//   not kept (dm2's waits 125 -> 178 bot-seconds a match: the wait is the lift's), in no preset.
// - `scriptfresh`: a scripted way left and come back to is flown from its first step (a bug: the
//   step it was left at was kept, e1m2's YA climb aimed at the walkway from the floor)
//   (shooterbots/pilot.cpp, M18r.md 3); on (head to head e1m2 -1.0, inside the noise; a bug fixed).
// - `scriptentry`: a scripted way's entry had anywhere within 64 units once lined up, the keys let
//   go short of that (a run in from the side circled the entry's point)
//   (shooterbots/nav_script.cpp, M18r.md 3); not kept (e1m2's YA lying 77 -> 80 %, slower: the stop
//   costs more), in no preset.
// - `calmvel`: moving past 150 units/s, the calm view on the pilot's walk within 8 degrees, not
//   on the route's point 320 units on (shooterbots/bot.cpp, M18r.md 6); not kept (enemies appearing
//   faced 52 -> 51 %, the run turns faster than the view follows), in no preset.
// - `lookturn`: the calm view's point on the route kept within 60 degrees of the pilot's walk
//   (past a node overshot or a switchback the route's point 320 units on lay 90+ degrees off the
//   walk in 58 % of the frames the view trailed the run's turn by 40+) (shooterbots/bot.cpp,
//   M18r.md 6); judging, in no preset.
// - `viewbook`: moving past 150 units/s, the calm view looks where people look at that spot moving
//   that way, off the map's view book (shooterbots/view_book.hpp; se-team `--viewbook=<file>`)
//   (shooterbots/bot.cpp, M18r.md 6); judging, in no preset.
// - `gapturn`: a gap trick near its lip with the run heading away from its jump is turned to on
//   the ground (1.5 s at most) before it is given up (shooterbots/pilot_gaps.cpp, M18r.md 0c);
//   judging, in no preset.
// - `gapnear`: a route is not planned through a gap trick whose takeoff is near (`kGapCurveFrom`)
//   unless the run already heads at it near its speed: it was planned and given up on the next
//   landing, off every route for 10 s (shooterbots/pilot.cpp, M18r.md 0c); on (head to head dm2
//   +6.0, e1m2 -0.2).
// - `placeheard`: a place a mate names is heard as a region, as a person hears it: a loc file
//   names a room by many points (dm2's `tele` 16 over 585 units), so the name is read as the
//   points of that name round where the speaker was last known, and as the item among them when
//   one stands there (the place a map's items name); off, the middle of every point of the name
//   (shooterbots/mind.cpp `place_pos`, M18r.md 0c); on (head to head dm2 -1.25, e1m2 +0.5).
// - `gaplava`: with `gapbuild`, the curve into a gap trick and the run on the ground to its lip are
//   taken over lava and slime too (before, only over water: dm2's gaps are all over lava, so the
//   run reached the lip at a walk and the gap was given up, though a circle jump from there lands
//   across); the jump is still taken only where the move says it lands across
//   (shooterbots/pilot_gaps.cpp, M18r.md 0c); on since `brinkkeys` (even head to head on dm2, the
//   gaps over lava jumped ~0.8 a match a side; before it lava deaths rose 12.8 -> 18.3).
// - `brinkline`: the walk's look point is not carried past a node where the route walks a lip over
//   lava or slime (`NavLink::brink`): the walk turns at the route's corners there, not across
//   them (dm2's YA ramp: a run at 300 cut its curve off the edge into the lava)
//   (shooterbots/pilot.cpp `pursue`, M18r.md 0c); not kept (head to head dm2 -6.7, lava deaths no
//   fewer), in no preset.
// - `sidebrink`: a walk whose side step falls into lava or slime (`NavLink::brink_side`: a ramp
//   or a walkway high over a pit, dm2's YA ramp) is a lip to the pilot and the brink guard as a
//   `brink` link is (shooterbots/nav.cpp `nav_mark_side_harm`, M18r.md 0c); judging, in no preset.
// - `fallseen`: the walk foreseen (`ahead`) flies each way of pressing and puts one whose box
// leaves
//   the floor over lava or slime after every other (a run down dm2's YA stair at 320 overshot a
//   corner 20 units from the lava's edge: the brink guard braked only at the edge, too late for
//   the keys) (shooterbots/look_ahead.cpp, M18r.md 0c); not kept (over `brinkkeys` head to head dm2
//   +1.5, -2.8 / +5.7, lava deaths no fewer), in no preset.
// - `liplook`: near lava or slime the walk's look point runs 0.3 s ahead at its speed (not 64
//   units): a corner by the edge is turned early and inside, not overshot (shooterbots/pilot.cpp,
//   M18r.md 0c); judging, in no preset.
// - `brinkkeys`: near lava or slime the keyboard's hand does not want a key whose step under the
//   view ends over it, and answers a run carried over it within a quarter second with the keys
//   against the run (shooterbots/keyboard.cpp `keys_off_brink`; the keyboard's 45-degree sets and
//   taps weave a walk off a line 40 from the edge: se-reach --walk, M18r.md 0c); on (head to head
//   dm2 +6.2, lava deaths 12.75 -> 9.0 a match).
// - `floortrap`: a floor over lava a touch button slides away (dm2's big room) is never brushed by
//   accident (the links into the nodes touching its button kept off routes), its button pressed
//   only when an enemy stands on the floor and no mate does, the presser staying put till the
//   floor returns, its teleporters shut and kept off while it is away, a `trap` call to the team
//   (shooterbots/floor_trap.hpp, M18r.md 0c); on (head to head dm2 +5.0, the enemy's lava deaths
//   +3).
// - `trapflee`: a player standing on a floor trap's floor when it goes (as it sees it, or a mate's
//   call) runs off it at once by the nav's shortest way to the ground round it, with the floor's
//   slide and not against it, before any goal (shooterbots/floor_trap.hpp; dm2: 47 of 82 trap
//   deaths stood on the floor as it went and fell ~3 s later, M18r.md 0c); on (head to head dm2
//   -2.7, inside the noise; its side's lava deaths fewer).
// - `bridgeseen`: a bridge the player is no longer sent is believed gone once one of its nodes is
//   in sight with no floor under it (a client keeps an unsent mover where it last saw it: dm2's red
//   armour's bridge stood out in the bot's mind after it went back, and routes walked off the lip
//   into the pit; and the links shut onto a bridge or a floor door still in honoured, which no
//   route did: they carry no mover; shooterbots/fighter.cpp `bridge_seen_gone`, M18r.md 0c); on
//   (head to head dm2 +7.4, e1m2 -0.3).
// - `sidehop`: a hop's stretch ends at a walk whose side step falls into lava or slime
//   (`NavLink::brink_side`), walked, not hopped (dm2's yellow-armour stair: most of its falls came
//   out of a hop or a copied hop start; shooterbots/pilot.cpp `hop_stretch`, M18r.md 0c); not kept
//   (YA falls 24 -> 20 over 8 seeds, noise; the hop share 0.45 -> 0.39, people 0.46), in no preset.
// - `lostdry`: off the graph on dry land, the walk back to the nearest node never takes one in
//   lava or slime or on a mover (dm2's south pit: its trains' nodes over the lava were walked at;
//   shooterbots/fighter.cpp, M18r.md 0c); not kept (the pit's falls 16 -> 22 over 8 seeds), in no
//   preset.
// - `liftoff`: a jump asked for and not yet off the ground (the keyboard's jump key lands a few
//   commands late) is waited for 0.15 s, the jump still asked (on: M18r.md 0c); off, the next
//   command on the ground reads as the jump landed short and marks it missed
//   (shooterbots/pilot.cpp, M18r.md 0c).
// - `shotjump`: a rocket jump's shot held, the mouse still on it, until the jump key is down: a
//   person presses the two as one move; off, the shot went with the ask and burst under a player
//   still on the ground, a flat run into dm2's south pit (shooterbots/fighter.cpp, M18r.md 0c).
// - `rjready`: a rocket jump only with the launcher in hand and ready to fire; off, the shot went
//   with whatever was in hand (shooterbots/pilot.cpp, shooterbots/fighter.cpp; M18r.md 0c).
// - `jumpcuts`: a jump the pilot missed twice is kept off its routes for the match
// (shooterbots/nav.cpp
//   `nav_route`); off, only a cut through a mover or a gap trick is honoured (M18r.md 0c).
// - `brinkfall`: the brink guard flies the player's box along its run (gravity, walls, a stair's
//   steps) and brakes in the air when the flight, or the run on after it lands, ends in lava or
//   slime (dm2's YA stair: a run off its top step at 360 lands on the lip of a lower one, and the
//   guard on the ground braked 10 units too late; shooterbots/brink.cpp `fall_into_harm`, M18r.md
//   0c); on (head to head dm2 +0.9, lava deaths 10.4 -> 6.75 a match, speed +3.6).
// - `aimland`: with the view on a fight, a hop's jump whose flight straight on meets a wall within
//   0.3 s is neither held for the landing nor pressed on it; the landing runs on and jumps once
//   clear (shooterbots/pilot.cpp; the air under a fight's view turns only with the mouse, and 91 %
//   of such flights taken off 30 degrees off the hop's aim met a wall on dm2; M18r.md 0c); on (head
//   to head dm2 -3.6, e1m2 +1.2; dm2's fast flights meeting a wall 60.2 -> 57.0 %).
// - `starthead`: a copied hop start (`recipes`) is chosen among the proven ones for where its
//   takeoff heads: the nearest twelve to the pilot's state flown, the one taking off closest to
//   the hop's aim from its takeoff kept (shooterbots/pilot_recipes.cpp; copied starts' fast flights
//   met a wall 46-52 % against the hop's own 28 %, taken off past the way; M18r.md 0c); on (head to
//   head dm2 -2.3, e1m2 +0.4; dm2's expert fast flights +13 %, p95 407 -> 413).
// - `startclear`: under `starthead`, a copied start's choice also charged 200 degrees a second its
//   flight after the takeoff (straight on or turned either way at the air's most) meets a wall
//   within 0.4 s (its proof ends at the takeoff; copied starts' fast flights met a wall 38 %
//   against the hop's own 18 %, a quarter within 0.11 s; M18r.md 0c); on (head to head dm2
//   -3.8, e1m2 -2.0; dm2's fast flights meeting a wall 54 -> 49 %, keeping -111 -> -67).
// - `gaphold`: with a gap trick in a hop's stretch, the jump held in the air for the landing frame
//   when the gap's takeoff from the forecast landing would jump (it was decided only on the ground,
//   so the keyboard's late jump cost 1-3 frames of friction on 176 of 2,374 fast landings;
//   shooterbots/pilot_gaps.cpp; M18r.md 0c); on (head to head dm2 +4.3, e1m2 +1.8; 320+ 25.8
//   -> 26.5
//   %).
// - `airturn`: in a hop's air under `airview`, with the hop's aim 20+ degrees off the velocity on
//   the side held, the view wanted further into the turn (half a degree a degree past 20, up to
//   15 more): the air turned at a crawl before walls (the velocity turns only as the view
//   does, and a want on the velocity left the hand inside its tolerance; shooterbots/bot.cpp;
//   M18r.md 0c); on (head to head dm2 +2.7, e1m2 +3.9; dm2's fast flights meeting a wall 59 -> 53
//   %).
// - `paceback`: under the mouse hand, an aim lost or a look let go goes back to the way by the
//   hand's own stroke onto it (a want paced from the view gave one small stroke a look: the view
//   hung where the enemy was lost); on.
// - `keyswap`: in a hop's air under the hand (`airkeys`), the pilot's strafe key held the whole
//   flight and swapped with its side, as the mouse turns the other way (the maintainer's hop,
//   M18r.md 0b): never let go while the view crosses the velocity; on (hop course: the expert's
//   lap 25.8 -> 21.7 s, walls 5 -> 1.5; dm2 head to head +1.6 a match, speed even: neutral, more
//   human).
// - `avghop`: an average player's hop flown as an expert's (the side swapped in the air, the
//   strafe key alone, the mouse leaning into it), not the simple hop (forward and one side a
//   flight): the maintainer's hop at every level that hops (M18r.md 0b); on (dm2's averages
//   head to head -1.4 a match, speed even; the hop course's average lap 35.0 -> 25.3 s).
// - `hopsee`: a hop's arc checked for a wall 0.25 s ahead, not 0.12 (the side switched to the
//   clearer arc): at 600+ units/s the short look saw a corner's outer wall too late (the hop
//   course's expert: walls 2.2 -> 0.5 a lap, lap 22.8 -> 19.0 s; 0.4 s thrashed); on (dm2 head
//   to head -2.3 a match, inside the noise; fast hop flights meeting a wall 53.9 -> 50.9 %).
// - `hopturn`: a hop's air view led 6 degrees into the turn off the velocity, not 2: the hand
//   strokes on and the velocity turns at people's rate (dm2's people 124-141 degrees a second in
//   the air at 400-600 units/s, the bots 85; the hop course's expert 92 -> 124), gaining more a
//   flight (the maintainer: "it's in the curves you gain speed"); on, with `hoparc` (dm2 head
//   to head +5.8 a match; the hop course's average lap 23.1 -> 20.9 s, walls 1.8 -> 1.2).
// - `hoparc`: a hop's side held in the air until the aim lies 25 degrees the other way, not 12
//   (on the ground 12 still): one wide arc a jump, not a wiggle round the route; on.
// - `hopland`: a hop's side swapped only just before the landing, once the heading has come
//   round past the aim, never earlier in the air; each flight's turn paced to land as far past
//   the aim as the last swap was, less the faster it flies (the maintainer's hop: strafe the way
//   you turn, swap with the jump; speed built, turn less and keep it); judging.
// - `hoppitch`: a hop's air view pitched along the velocity a little (up on the jump, down on the
//   fall), as the maintainer looks; judging.
// - `hoprhythm`: people's rhythm in a hop's air: a swap of side kept 0.1 s before anything swaps it
//   back (the two arcs walled alike swapped back and forth every command), flown straight with
//   the side held when straight on is clearer than both arcs, the wall's check run at the jump
//   so the side is set with it; on (dm2: swaps of the side key a flight 2.5 -> 1.6, head to head
//   +0.9 a match, speed even).
// - `aimstrafe`: a hop's air under the aim strafes the way the aim turns (the maintainer: strafe
//   in the aiming direction, never turn against the strafe): the side key of the view's turn,
//   swapped as the aim swings back; let go while it would brake hard (the view far ahead of the
//   velocity on its side) or run the flight into a wall; the dodge off in a hop's air; on (dm2
//   head to head +1.0 a match; speed 239.7 -> 243.1, the curve with the aim's turn 8 -> 13 %).
// - `walkmash`: a walk at a full run on dry ground holds forward and swaps the strafe key every
//   0.1 s (the maintainer: "walk with forward and mash the strafe buttons left right, you walk
//   faster"): the wish 45 degrees off the velocity on either side lets the ground's acceleration
//   build past the run's 320 to ~395 against friction, pressed while the view lies within 25
//   degrees of the run; on (dm2 head to head +3.7 a match, speed 240.7 -> 248.9).
// - `giveup`: a jump lined up for again and again is given up whichever node beside it the plan
//   starts it from (the run-ups counted by the jump's end, not its exact link), with the jumps
//   beside it of its own kind (a rocket jump's too), kept off every route (`jumpcuts`' rule); off,
//   a flip rocket jump to dm2's quad was lined up for 40-90 s on end; on (dm2 head to head +1.5).
// - `stairbump`: the walk's look ahead counts no bump for a stair's step its box climbs; off, a
//   stair read as a wall and a fast climb let go of its keys; on (speed 247 -> 251, head to
//   head even).
// - `mashstart`: a walk mashing its strafe keys holds a hop chain's first jump until its run is
//   built (370 units/s or 0.4 s of mashing; people's chains start at 427, the bots' at 317); on
//   (speed 251 -> 257, head to head even over 24 seeds).
// - `aimground`: under a fight's view no hop jump while the run is under 360 units/s (those flights
//   lost ~48 each, 42 % into a wall; people seldom jump slow); on (head to head +1.8, speed 246 ->
//   248).
// - `straightfly`: on a straight way ahead with the aim within 8 degrees of the run the hop's air
//   flies straight (the side held, the view where its wish adds nothing across), as people fly a
//   corridor: their view on the run, the mouse almost still; under 480 units/s; on (head to head +4
//   over 24 seeds, the corridor's mouse 71 -> 23 degrees a second, people 25).
// - `midroute`: a route's walk into a node at its floor's edge costs a share more (half that a grid
//   step in), so routes and the hop's line keep to a lane's middle (dm2's pillar room: the bots'
//   fast flights met a wall 86 %, people's 20 %, along the grid's shortest way 26 units off the
//   pillars); on (head to head +2 over 24 seeds, speed +5).
// - `noroute`: an item goal the pilot finds no route to for a moment (the goal's reach counts links
//   the pilot's plan leaves off: a missed jump, a gap trick given up, a door found shut; or a
//   bridge in) is reached through the button that runs out the bridge in its way, when one does,
//   else set aside a while; without it a bot stood idle on `no route` (dm2's red armour's ledge,
//   its bridge in: 38 % of the time a bot went for it). Judging: no-route time 1.23 -> 0.85 %, the
//   items' lying even, head to head -2 a match (nobody reaches the ledge either way).
// - `itemfirst`: a key item of its own to have (its goal a key item there, or back within its run
//   and a few seconds) comes before the fight, as experts leave a fight to time an item: no chase,
//   and in a fight the feet keep the route (a retreat's: the dodge, no press, no standing), the
//   aim on him; unless he was heard firing the launcher or the arc (a key item he would drop).
//   On (head to head even; the red armour and the launcher lie less, the hub 37.5 -> 34.6 %).
// - `rushhold`: a rush for the quad, once begun, is held while the quad is due (25 s at most),
//   not counted again each think among the two nearest (a rush lasted a median 1.5 s, the goal
//   flipping, 10 of 703 trips to the quad taking it). On (the quad lying 62 -> 47 %, people 4;
//   head to head -1.2 a match, within noise).
// - `quadlead`: the quad's rush starts no earlier than 15 s before it is back (the map plan's lead
//   reached 24): holding a powerup's area before its window is no use, the other items are held
//   meanwhile (the maintainer). On (head to head even; the quad within the noise).
// - `bridgebutton`: a bridge a button runs out stays a way while it is in: a route across it sends
//   the player to its button first (dm2's plates over the lava, button 145, the way into the red
//   armour's ledge, the mega and the SSG). On (the ledge held near people's, the brawls halved;
//   head to head -4.2 a match).
// - `rushrun`: the quad's rushers are the two of the team with the shortest RUN there (its own
//   reach, a mate's route from where he is believed to be), not the nearest across (dm2's corridor
//   under the quad's room is 14 s round by its way up), and a rusher goes his run and 2 s before
//   the quad when that is longer than the plan's lead (within its 15 s window). On (the quad
//   lying 45.5 -> 38.3 %, people 4; head to head -1.25 a match, within noise).
// - `quadseen`: an enemy in sight with the quad's glow tells a player the quad is gone and back no
//   sooner than 30 s on (each later sighting moving it on), and his team ("enemy quad"): untimed,
//   the bots believed it there 97 % of its showings and long after it went. On (believed there
//   while gone 42 -> 27 % of the time; head to head +3.1 a match).
// - `doorway`: a door a button fires keeps an item (shut before it, its button pressed first) only
//   when firing it moves it away from the item; a door that moves over the item when fired (dm2's
//   crushing walls beside the pillar room's two yellow armours) leaves it open at rest; nor does
//   one with a way round all its button's doors (the nav's links from the spawns). Without it
//   the bots read both armours as shut: passing them there and wanting them they took them 5-7 %
//   of the time, people 72-77 %. On (head to head +3.3 a match on dm2, +0.75 on e1m2).
// - `trapbutton`: the nav's links whose way passes through a floor trap's touch button are shut
//   unless the player means to press it: dm2's walkway over the pillar room drops onto the
//   buttons beside its YAs, and walks along the north wall brush them (~4 floors dropped a
//   match by nobody's choice). On (head to head +1.75 a match on dm2).
// - `trappress`: `floortrap`'s press on an enemy over the floor (the rest of `floortrap`, the keys
//   and the teleporters kept off, stays): people touch dm2's two buttons once a match, presses
//   included; the bots pressed ~6.9 times. On (off: head to head -6.3 a match on dm2).
// - `blastsafe`: a rocket or grenade never bursts at its shooter: a shot whose crosshair meets a
//   wall or a player within 160 units is held, and with the enemy that near another weapon is
//   taken up (the blast weapons scored low under 160). Two 20-minute dm2 matches: 61 of 748
//   deaths the bots' own blast (people's own deaths 2.2 per 100 kills, the bots' ~13). On (head
//   to head +15.2 a match on dm2).
// - `boregive`: the bore (a weak carrier's rocket at his own feet, his launcher left in his pack)
//   for a mate near with no launcher who has better use of it, when it matters (the maintainer):
//   a powerup's carrier (the quad, now the pent too: their time is short), or a better player
//   (an average player's launcher to an expert; a novice knows no bore) while the team has just
//   lost a key area it can take back quickly. On (rare: 1-2.4 bores a 20-minute match).
// - `itemglance`: running past a key item's spot (a launcher, the red or yellow armour, the mega,
//   the quad, the pent) with it off the screen and in a clear line, a calm walker swipes its view
//   to the spot, holds it a moment and swipes back, once a pass by its map sense's odds: the item
//   learned there or gone (knowledge.cpp `item_glance_offer`). On (head to head +1.3 a match on
//   dm2, +1.55 on e1m2; the launchers' spots seen nearer people's).
// - `rlfetch`: THE LAUNCHER'S TAKER (mind.hpp): a player with no launcher, the nearest of his team
//   without one, goes to a launcher there or due within `kRlFetchAhead_s` (the timer a mate called,
//   `rl in 10`), says `coming rl` and waits on it until he has one (dm2: at a launcher's return a
//   mate without one stands on its spot 21 % of the times for people, 6.5 % for the bots).
//   Judging, in no preset.
// - `holdpoint`: THE HOLDER POINTS (the maintainer's rule): a holder of the red or yellow armour's
//   area or a launcher's does not take its item as it shows: it stands off the spot, points at it
//   (`ra at [ra]`) and waits for a mate to take it, unless its own armour is under 100 (an
//   armour), it is low on rockets with no box of them near (a launcher), or an enemy is near the
//   item: then it takes it, to deny him (mind.cpp `holder_leaves`). Judging, in no preset.
// - `errandreach`: A DOOR'S ERRAND IS A WAY THROUGH IT: the door a button errand goes round (to its
//   button) stays off the pilot's routes but not off the reach, so what lies behind it (dm2's
//   water YA and its teleporter, the way up to the quad from the east) is still reached and the
//   goal and the quad's rush hold through the errand; and a rush begun is held while the reach
//   is worked out again (mind.cpp). Judging, in no preset.
// - `fields`: THE WAYS TO EVERY ITEM FROM EVERYWHERE (shooterbots/nav_field.hpp): whether an item
// is
//   reached and how long the run is are read off the match's field to the item for the fighter's
//   jumper (built once, the same for every bot of a kind), not its reach worked out each second;
//   a door only a button opens is an errand in the field, so a way that starts with a button
//   sends the fighter to it at once (fighter_doors.cpp `field_errand`). Judging, in no preset.
// - `fieldway`: the pilot's route to its goal read off the goal's field (shooterbots/pilot.hpp
//   `PilotSettings::fields`) when nothing on it is barred, cut or shut, else searched: every bot
//   of a kind runs the same way from the same place. Needs `fields`. Judging, in no preset.
// - `chasekill`: THE CHASE IS A CHOICE (M18r.md 6f, the maintainer's rule): an enemy lost from
//   sight or heard is followed only when he is believed killable (the ledger's body low, the
//   fighter strong) or he holds the launcher or the arc and the fighter does not; else it goes on
//   with its goal. On (head to head +1.35 a match alone, even; dm2's follows 37.5 -> 36.4 %, people
//   30: the bots barely chase, M18r.md 6f).
// - `goalfeet`: THE FIGHT ON THE WAY (M18r.md 6f): in a fight the feet keep tending along the
//   route to the goal, the aim on him; the dodge jinks only on the axis the route does not need.
//   On (head to head +4.0 a match on dm2, +3.45 on e1m2, +3.45 with `chasekill`: M18r.md 6f).
// - `rlback`: THE LAUNCHER BACK AT ONCE (M18r.md 18b): with the one in sight past the blast's
//   reach the launcher, when it is the best, is asked for at once (not after the asks' gap), and a
//   lesser weapon asked for and not yet in hand (the mod changes weapon once the launcher's refire
//   is over) is asked back: people keep it up at mid range (64.6 rockets a minute, bots 38.6).
//   On (12 dm2 seeds: head to head +0.1 a match, even; the scattergun up at mid range 25 -> 18-21
//   %, rockets a minute carrying the launcher 11.1-11.6 -> 11.8-12.7, people 19.3).
// - `afterfire`: THE ROCKET AFTER HIM (M18r.md 18b): the one aimed at gone from sight a moment
//   ago (the aim still holding where he is carried), the launcher up and ready: a rocket at once
//   where the crosshair's burst lands within the blast's reach of him, never nearer the shooter
//   than the blast's (`blastsafe`) nor by a mate, two a loss: people fire 24 % of their rockets
//   within 1.5 s of losing him (bots 10 %). On (12 dm2 seeds: head to head +1.5 a match; their
//   share 10 -> 13 %, within 0.4 s 4.9 -> 7.9 %, people 10.5).
// - `earfire`: THE ROCKET AT A SOUND (M18r.md 18b): an enemy heard within kEar_s and not in
//   sight, the launcher up and ready: a rocket once the crosshair's burst lands within kEarReach
//   of where he was heard with a clear line to it (the view swiped there, `listen`), never under
//   the blast's reach of the shooter nor by a mate, one a sound: 26 % of people's blind rockets
//   burst in sight of an unseen enemy within 400 (bots 2 %). On (12 dm2 seeds: head to head +4.9 a
//   match, red +6.6, blue +3.3; frags a minute 11.5 -> 12.5; rockets with no one in view 2.5 ->
//   3.0 a minute on recordings, people 11.6).
// - `itemz`: A MAP'S ITEM MATCHED TO ITS ENTITY IN HEIGHT TOO (M18r.md 18b): of the entities within
//   48 units across, the nearest in height; across alone gave dm2's lob on its ledge and the
//   yellow armour under it one entity, and the lob was never a goal (dm6 stacks it over a
//   nailgun). On (12 dm2 seeds: head to head -0.8 a match, even; the lob carried 0.9 -> 1.7 %,
//   people 18; a belief put right).
// - `earlob`: THE LOB ROUND A CORNER (M18r.md 18b): an enemy heard or lost from sight a moment
//   ago with no straight line to him, the lob carried: its arc flown through the world and thrown
//   when its first impact lands by him (shooterbots/launcher_fire.hpp): people fire 56 % of their
//   grenades with no enemy in view, 20 % just after losing one. Judged too rare (0.005 a
//   bot-minute: the lob is not carried): off.
// - `lobtake`: THE LOB TAKEN BESIDE THE LAUNCHER (M18r.md 18b): to one carrying the launcher and
//   not the lob, the lob is worth a third of the launcher as a goal: people carry it 29 % of the
//   time they carry the launcher (bots 2.6 %). Judged too rare (2.6 -> 2.8 %): off.
// - `aimclear`: THE HEIGHT AIMED AT THAT CLEARS (M18r.md 18b): with the launcher on one in sight,
//   a height whose line bursts within the blast's reach of the shooter is raised (his middle, his
//   chest): 22 % of the bursts held past the near band had the view on the aim's own point.
//   Judged (12 dm2 seeds): runs 1.9 commands a bot-minute, those holds 23.5 -> 23.2 %, head to
//   head +2.5 in the noise: the holds are the lead's, not the height's. Off.
// - `presspair`: THE PRESS BY BOTH LAUNCHERS (M18r.md 6f): with `press`, its odds on and back by
//   the fighter's own loaded launcher and the one faced's believed (heard firing one): people
//   close in at 111 units/s without one on one with it, 55 both without, 24 with it on one
//   without. Judged with `press` (24 dm2 seeds): head to head -4.9 a match, the fights'
//   closing 26 -> 39 (people 59), deaths a minute 2.0 -> 2.2 (people 2.9). ON with `press`: the
//   maintainer's call (2026-11-20), more human at that cost.
// - `ledgerforget`: THE LEDGER FORGETS (M18r.md 0q): in a team game an enemy neither seen nor heard
//   for 3 s is believed the typical live enemy again (shooterbots/foe_ledger.hpp `foe_body`), not
//   the last count, which only ever fell by his cries (out of sight 24 points under the truth). On
//   (12 dm2 seeds: the bias out of sight -24 -> -8, the miss 51 -> 49; head to head -0.4, even).
// - `ledgerhits`: ITS OWN ROCKETS COUNTED (M18r.md 0q): each of its rockets followed from its
//   muzzle to its burst, and an enemy seen there lately takes the splash his believed place would
//   (shooterbots/foe_ledger.hpp `ledger_own_shots`); his cry right after not counted again. Judged
//   (12 dm2 seeds): the ledger's miss in sight 48 -> 50, its bias -7 -> -15, head to head -1.9:
//   off.
// - `ledgerheal`: A HEALTH BOX HEARD TAKEN HEALS HIM (M18r.md 0q): +20 to his body in the ledger,
//   to 150 at most (the ledger counted the armour and the mega heard, never a box). Judged (six
//   dm2 seeds): the miss 48.7 -> 49.6, nothing: off.
// - `ledgerobits`: THE KILL FEED READ (M18r.md 0q): an enemy's obituary sets his ledger line to a
//   fresh spawn (100, the plain weapon), every death on the map and not only the cries in
//   earshot (a death heard is the ledger's one news that pays: its miss 26 against the floor's
//   37). On (12 dm2 seeds: the spawn's news 4 -> 16 % of the ledger's samples, the miss in sight
//   48.3 -> 46.1, its bias -6.7 -> -1.3; head to head -1.1, even).
// - `spawnkept`: A FRESH SPAWN REMEMBERED LONGER (M18r.md 0q): an enemy's line read off his death
//   is forgotten after 10 s out of sight, not the fight's 3 (shooterbots/foe_ledger.hpp
//   `kSpawnKept_s`). On (12 dm2 seeds: the spawn's line 16 -> 36 % of the samples at a 25-point
//   miss, the miss out of sight 48.8 -> 46.5; head to head +1.0).
// - `rushsize`: THE QUAD'S RUSH BY THE TEAM'S SIZE (M18r.md P1): half the team rushes the quad as
// it
//   comes due, one at least (two in a 4on4 as before, one in a 2on2, where "the two nearest" were
//   the whole team and every armour lay while both waited on the quad). On (12 2on2 dm2 seeds: the
//   mega lies 39 -> 28 s after it shows, the red armour 61 -> 52, the launcher 31 -> 25, the quad
//   24 -> 30; the areas held up 3-8 points; deaths even; head to head +2.1).
// - `holdra`: THE ARMOUR HELD WITH THE ARMOUR ON (M18r.md P1(c), the maintainer's rule): the holder
//   of an armour's area stays by it while it lies there with its view on it, kills whoever comes,
//   and takes it whenever the game lets him (his protection under what it gives). Judging (12
//   2on2 seeds: contested 7 -> 15 %, deaths up, but held 45 -> 36 %, head to head -2.1: it hardly
//   engages, few bots hold the armour's area in a 2on2; M18r.md P1(c)).
// - `errandworth`: A DOOR'S ERRAND ONLY WHEN IT PAYS (M18r.md P1(c)): a shut door on the route is
//   gone round to its button only when the button and back through the door beat the way round
//   to the goal with the door shut; else the door is left off the routes a while and the goal
//   gone round to. dm2's south red armour's west door opens off a button inside the armour's own
//   room: reaching the button is reaching the armour, and the errand sent the bot over a bridge's
//   button and a lava gap's jump to it, its 12 s out before it got there. The door is barred
//   only while the goal it was weighed for is the goal (barred for every goal, the quad's only
//   way shut and bots stood with no route). On (12 2on2 dm2 seeds: the errands run out 65 -> 23
//   a match, button 4's 47 -> 4; the south armour's and mega's room contested 10 -> 26 % and 6
//   -> 17 % (people 23); deaths a minute 1.46 -> 1.55 (people 2.9); the north armour held 34 ->
//   47 %; but the south room held less and its mega lies 27 -> 60 s (people 8); head to head
//   +1.8, red +0.1, blue +3.5).
// - `nearwait`: A GOAL WAITED FOR LEAVES THE HANDS FREE (M18r.md P1): a short goal on the way is
//   looked for while the goal is near too when the goal is not there (waited for): a bot on the
//   red armour's spot waiting for it takes dm2's mega lying 96 units off (the decision log's
//   `item_passed`: "its goal near" barred 30 % of the mega's passes). On (12 2on2 dm2 seeds: the
//   south mega lies 60 -> 32 s after it shows (people 8), its area held 15 -> 25 % (58); the quad
//   27 -> 20 s, the red armours' areas held 41 -> 44 % and contested 15 -> 17 % (67, 23); deaths
//   even; head to head -1.1, red +7.4, blue -9.6: even).
// - `rushdetour`: THE RUSH BY THE CLOCK (M18r.md P1(b)): a rusher waiting on the quad takes a
//   red or yellow armour or a mega first when it is of use to him, he reaches it as it shows (or
//   it is there) and he is on the quad before it is due from there, as a person plays the items
//   by the clock (dm2: at the red armour's showings with a bot that timed it and could take it,
//   the quad's rusher waiting on the quad was the commonest). Judging, in no preset: it runs
//   rarely (21 detours to 248 waits in six matches: a red armour's round trip seldom fits the
//   quad's window); 12 2on2 seeds, head to head +1.35 (red +4.0, blue -1.3), the south armour 56
//   -> 52 s, the north 50 -> 82 s (noise at that rate).
// - `spamshow`: ROCKETS WHERE ENEMIES SHOW UP (M18r.md P1(d), the maintainer's: "more proactive
//   with shooting rockets at places where enemies often show up ... give away rocket launcher
//   locations, triggering enemies to show up"): spam at news with no news: a launcher holder with
//   rockets to spare, roaming, no one in sight, fires now and then at a show-up spot ahead of it
//   (a way into its area, a teleporter's exit, a power's spawn, a key item's spot) in sight and
//   in a rocket's range, no teammate near it (people's dm2: 11.6 rockets a minute loaded with no
//   one in view, the bots 2.1; 68 % of people's blind ones burst at a key spot). The launcher is
//   raised for the shot (`hide` had it down 64 % of the time carried) and `hide` waits
//   kRaiseHold_s after. On (12 2on2 dm2 seeds: rockets at show-up spots 7 -> 60 a match; loaded
//   with no one in view 2.1 -> 4.5 a minute (people 11.6); blind 8.5 -> 28 % of the rockets (23);
//   blind ones 600+ long 8 -> 17 % (44); frags a minute 4.47 -> 4.62; deaths even; head to head
//   +0.5, red -0.2, blue +1.2).
// - `rlhunt`: THE LAUNCHER HOLDER OVERWHELMED (M18r.md P1(e), the maintainer's: "showing up with
//   shotgun and armor, 2-3 people from different locations"): a fighter without the launcher,
//   strong (armour on), with an enemy believed to carry one placed lately within reach and a mate
//   near him too, goes for him; up close his rockets hurt him (people's launcher spells last 38 s
//   from the spot, the bots' 47). Judging, in no preset: 12 2on2 dm2 seeds, 64 hunting thinks a
//   match, head to head +0.35 (red -0.6, blue +1.3), spells 47 -> 46 s; 12 4on4 seeds, head to
//   head -3.7 (red -8.0, blue +0.6) and the spells from the spot LONGER (29 -> 32 s). Going for
//   him one by one is not overwhelming him: the 2-3 must arrive together (M18r.md P1(e)).
//   Again with `rlknown` (the carrier known 55 % of the time, not 10): 258 hunting thinks a 4on4
//   match, head to head +1.3 over 24 seeds (1-12 +5.8, 13-24 -3.2), spells from the spot 34 ->
//   31 s but from a pack 19 -> 26: noise and mixed, still off.
// - `rlgang`: THE LAUNCHER HOLDER OVERWHELMED TOGETHER (M18r.md P1(e)): a strong fighter with an
//   enemy believed to carry the launcher placed lately in an area of the map plan calls the
//   team's push on that area ("attack", mind.hpp): those strong enough gather outside it and go
//   in together, as they take back a lost base. Judging, in no preset: 12 4on4 dm2 seeds, 2.75
//   calls a match (the push often under way already), head to head -0.75 (red -6.0, blue +4.5):
//   too rare to read. What held it off (se-team `team.rlgang.<gate>`): nobody believed to carry
//   the launcher (bots knew a carrier 10 % of the time). With `rlknown` on both sides: 3.9 calls a
//   match, head to head -4.9 (red -6.0, blue -3.8): it costs.
// - `rlknown`: WHO CARRIES THE LAUNCHER, KNOWN AS A PERSON KNOWS IT (M18r.md P1(e)): an enemy heard
//   firing the launcher, or named the killer in a rocket's line of the kill feed, is believed to
//   carry it until the feed prints his death (at most kKeyKept_s), not only for the 10 s after the
//   last sound of him; the item-first and chase choices read the same belief. ON: 12 4on4 dm2
//   seeds, an enemy carrying a loaded launcher believed so 10 -> 55 % of the time and placed
//   within 3 s 6 -> 16 % (false beliefs 0.3 -> 4.7 %); head to head +0.3 (red -3.0, blue +3.7);
//   an enemy's launcher spell from the spot 29 -> 34 s, from a pack 23 -> 19, a carrier lost from
//   sight followed 39 -> 42 %; deaths and fight speed even.
// - `arrive`: A TIMED ITEM MET AS IT SHOWS (M18r.md P1(b)): a key item whose time back it knows to
//   the second is scored as if it showed kArriveSpare_s sooner, so the bot leaves for it in time
//   to be there when it shows (people take a showing red armour 3.6 s after it shows on dm2), not
//   when its wait has fallen under the run and it arrives late. ON: 12 2on2 dm2 seeds, a showing
//   mega lies 49 -> 22 s (people 8), the quad 32 -> 21 (1), the red armour 54 -> 47 (4), the
//   launcher 26 -> 31 (2); head to head +1.6 in 2on2 (red -1.9, blue +5.1) and -3.2 in 4on4 (red
//   -2.4, blue -4.0): within the sets' noise, the timing nearer people's.
// - `shareclock`: THE TEAM SHARES THE ITEMS BY THE CLOCK (M18r.md P1(b)): a taker on its way to an
//   armour or the launcher (`fetch`, `rlfetch`) does not take the quad's rush, and a mate who said
//   he is going for another key item is not counted nearer the quad, so the rush falls to a mate
//   with nothing on (in a 2on2 the rush took the armour's taker on 35 of 88 trips). Judging, in no
//   preset: 12 2on2 dm2 seeds with `fetch`, head to head -3.8 (red -3.1, blue -4.4) and the quad
//   lying 28 -> 73 s after it shows (both members on armours, nobody rushes); with `rlfetch` too
//   -2.8. The taker's trips end in its death (36 of 76), not the rush, once the rush yields.
// - `rushstrong`: THE QUAD TO THE STRONG (M18r.md P1, the fight at a showing): the quad's rush
//   ranks a mate who said he carries the launcher with 60 protection or more (100 yellow armour)
//   kRushStrongRun_s of run nearer than he is, and a strong rusher as far ahead of weak mates.
//   People bring their strong to the quad (dm2: a strong player within 600 at 56 % of its showings,
//   twice the 28 % of the time a player is strong; the bots' 19 %), and at a showing the strong win
//   (the launcher's fights 90 %, the better armour's 92 %). A mate's "coming quad" counts ahead of
//   a strong bot only past that edge, and a weak rusher's held rush yields to a strong mate's
//   said one. ON: 12 2on2 dm2 seeds, rushes begun strong 17.6 -> 20.9 %, a strong player near at
//   the quad's showing 19 -> 25 % (people 56), deaths there 0.55 -> 0.81 a showing (2.36), head to
//   head +6.0 (red +9.6, blue +2.4); 12 4on4 seeds +2.6 (red +4.1, blue +1.0). Its cost: the south
//   mega taken within 5 s 18 -> 8 of ~50 showings (its strong holder leaves the room to rush).
// - `pokelook`: THE BUTTON'S VIEW AS THE BOT'S LOOK (M18r.md P2): a door's errand or a whim's
//   button watched by the bot's own look, set before its step, so the step's hand is the one
//   writer of the view, held 0.6 s out of sight, the shot only once the view is on it. Before, the
//   poke set the view after the step, a second stroke on the same hand each command against the
//   calm view's: standing under dm2's shot button the view swung 109 -> -166 degrees and back
//   every 0.2-0.3 s (the poke's view reversed 1.54 times a second, a third of all). ON: 12 2on2
//   dm2 seeds, the view's reversals 0.25 -> 0.15 a second (the trace), off the recordings 0.54 ->
//   0.34 (people 0.12), sweeps 109 -> 99 a minute (62), lifts 13.9 -> 13.0 (6.6); the doors'
//   errands as before; head to head 2on2 +0.3 (red +4.1, blue -3.6), 4on4 -3.0 (red -2.1, blue
//   -3.8): within the noise, the hand nearer people's.
// - `circuit`: A ROUND OF THE KEY ITEMS BY THE CLOCK (M18r.md P1, the free bots' rounds): a key
//   item is scored with a share of the best key item it could have next from there, by the time
//   to have this one, the run between them and the next one's time back, so the bot goes where two
//   come due together, as people run their rounds (dm2: the red armour then the mega in its room
//   582 of 4449 of people's next key pickups, the bots 40 of 411). Judging, in no preset.
// - `unlocklook`: THE ITEM'S SHOT BUTTON AS THE BOT'S LOOK (M18r.md P2): the button that opens a
//   goal item's door (`unlocking`) watched by the bot's own look, set before its step and held
//   0.6 s out of sight, the shot only with the view on it (`pokelook`'s fix for the errands).
//   Before, the view was set straight onto it after the step each command it was in sight, a
//   second writer against the step's: the `button` writer reversed 3.3 times a second while it
//   wrote (6 % of all the reversals on dm2). ON: 12 2on2 dm2 seeds, the view's reversals 0.378 ->
//   0.336 a second (se-team `view40`), off the recordings 0.356 -> 0.325 (people 0.118); the north
//   red armour's shot-button door opened 6.3 -> 8.5 a match; the pilot's stuck count 68 -> 75 (it
//   stands while the hand turns onto the button); head to head 0.0 (red +0.5, blue -0.5).
// - `swapground`: THE HOP'S AIM SWAP AT THE LANDING (M18r.md P2): the side the aim asks for is
//   taken on the ground, not in the air, unless the aim lies `hop_swing` off; a wall's swap stays
//   in the air. People swap the lean at the landing 66 % of the time, the bots 51 %, and a swap
//   brings a reversal of the view 12 / 19 % (dm2, 25 ms frames). Judging.
// - `wallplan`: THE SIDE CLEAR OF A WALL SET WITH THE JUMP (M18r.md P2): on the ground the hop's
// arc
//   is checked half a second ahead (a flight), so the side clear of a wall is jumped with and not
//   flipped to in the air: 44 of the air view's 56 side swaps in a match were a wall's. ON: the hop
//   course's expert lap 21.9 -> 21.2 s, speed 576 -> 593, walls 1.5 -> 1.2; 12 2on2 dm2 seeds,
//   speed off the recordings 267.6 -> 271.1 (people 290.7), the hand and the air's swaps even
//   (the swaps it was meant for did not fall), head to head +0.4 (red -0.2, blue +1.0).
// - `startease`: A COPIED HOP START EASED IN FROM ITS OWN VIEW (M18r.md P2): the start's recorded
// view
//   offset to begin at the bot's view and eased to the recording's over 0.25 s, the proof flown on
//   it. People's own starts reverse the view 0.115 times a second, the bots' 0.42 (se-recipes), and
//   2.6 while a copied start writes the view (se-team `view40.recipe`): its entry view set at once
//   snapped the view in and out of every start. ON: the hop course's expert lap
//   21.2 -> 19.3 s, speed 593 -> 649, walls 1.2 -> 0.3; 12 2on2 dm2 seeds, speed off the recordings
//   271 -> 273, wall bumps 4.4 -> 4.1 a minute, the starts' reversals 0.42 -> 0.40 (people 0.115:
//   the snap is not all of it), head to head +1.9 (red +1.2, blue +2.5).
// - `airend`: A HOP'S STRETCH ENDED IN THE AIR FLOWN BY THE HOP TO THE LANDING (M18r.md P2): a
//   stretch that runs out at a climb, the goal or a plain link while the flight is up keeps the
//   hop's air (the side key, the view on the velocity, the aim along the route past the end) and
//   ends at the landing, with no jump held for it. Without it the walk took the flight over at
//   once and a glance or a look the view: 12 2on2 dm2 seeds, 840 of 2018 flights at 380-460 left
//   the hop in the air, 191 at a climb crashing 41 %; at the corridor's end into the raised cross
//   corridor 51 of 77 flights, 45 of them crashing. ON: 12 2on2 dm2 seeds, flights the hop flew to
//   their end 1050 -> 1407, speed off the recordings 275.06 -> 275.24, head to head +2.5; 4on4
//   e1m2 speed 219.9 -> 223.6, head to head -2.8 (red -5.6, blue 0.0); dm3 speed 212.1 -> 212.4,
//   head to head +2.2 (red +1.2, blue +3.3): faster on every map, the frags even (+0.6).
// - `pressface`: A TOUCH BUTTON IS WALKED INTO FROM IN FRONT OF ITS FACE (buttons.cpp
//   `into_face`): near it, the poke's keys take him to the place before its face, then in along
//   the way it is pressed (its `angle`), not straight at its middle past the wall it is set in.
//   On for Quake II's campaign (q2_unit): bunk1's walkway button, set in a post's face and run at
//   from its side for minutes, pressed at 123 s, was 414 s. Off for Quake's (coop_episode): e1m2
//   at 100% in 305 s without it, 23/24 kills in 383 s with it. Judging for the matches.
// - `shotplace`: A SHOT BUTTON HE CANNOT REACH THE PLACE OF IS SHOT FROM ONE HE HAS REACHED that
//   sees it (way_gates.cpp `way_shot_sides`). On for Quake II's campaign: bunk1's switch behind
//   its force field, shot from the corridor through the room's south opening. Off for Quake's:
//   e1m6 11/24 kills and 2/4 secrets without it, 7/24 and 0/4 with it (the shot button taken
//   first, the secret on the ledge above it then missed). Judging for the matches.
// - `runupkeys`, `underway`: judging, in no preset. The numbers: docs/goals/M18r.md.
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
  jumpcuts,
  brinkfall,
  aimland,
  starthead,
  airturn,
  startclear,
  gaphold,
  keyswap,
  avghop,
  hopsee,
  hopturn,
  hoparc,
  hopland,
  hoppitch,
  hoprhythm,
  aimstrafe,
  walkmash,
  giveup,
  stairbump,
  mashstart,
  aimground,
  straightfly,
  midroute,
  noroute,
  itemfirst,
  rushhold,
  quadlead,
  bridgebutton,
  rushrun,
  quadseen,
  doorway,
  trapbutton,
  trappress,
  blastsafe,
  boregive,
  itemglance,
  rlfetch,
  holdpoint,
  errandreach,
  fields,
  fieldway,
  chasekill,
  goalfeet,
  rlback,
  afterfire,
  earfire,
  itemz,
  earlob,
  lobtake,
  aimclear,
  presspair,
  ledgerforget,
  ledgerhits,
  ledgerheal,
  ledgerobits,
  spawnkept,
  rushsize,
  holdra,
  errandworth,
  nearwait,
  rushdetour,
  spamshow,
  rlhunt,
  rlgang,
  rlknown,
  arrive,
  shareclock,
  rushstrong,
  pokelook,
  circuit,
  unlocklook,
  swapground,
  wallplan,
  startease,
  airend,
  pressface,
  shotplace,
  trial,
};
constexpr size_t kFeatures = 192;
// The feature's name as a command line or a console writes it ("swimaim").
const char* feature_name(Feature f);

struct Features {
  uint64_t bits[3] = {0, 0, 0};  // room for 192, a bit a feature by its number
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

}  // namespace ge::shooterbots
