---
name: bots
description: "Use for anything a bot DECIDES in gameengine2 or a game on it: engine/ai (the decision record ai::Why every bot writes, and the pieces bots share as they land: the hands, thinking on a schedule, sight and memory, paths, steering, choosing), a game's Run::trace hook, ge why and the play protocol's why op, the loop that makes a bot behave more like a person, a match video or demo of the bots on request (make bot-video), why a bot did something at a moment of one (make bot-why, the se-why lab, the decision log), and Shooter's Edge's bot plan worked headless (docs/goals/M18r.md § M18r14g, the locations in a git-ignored .bots/local.md, how a change is judged, the labs). Load it before writing or changing a bot, a pilot, a rival or a pursuer, and to find out why one did what it did."
---

# Bots

A bot is the game's stand-in for a player: it reads what a player could see and writes the
controls a player would. Every game has them (a pilot, the rivals, a crowd, pursuers), and the
engine's half is `engine/ai/` (`ge::ai`, target `ge_ai`): what every bot shares, so no game
writes it twice. The roadmap is `docs/goals.md` § M16.

**Read this skill's lessons first**: `make lessons ARGS=bots`. Load `sim` (or the game's own
skill) beside it for the bot itself, `debug` for the log and the charts, `agent-interface` for
`ge why`.

## The rule: every decision is traceable

A bot that chooses records WHY, where it chooses, on an `ai::Why*` it is handed
(`engine/ai/why.hpp`):

```cpp
Controls bot_controls(const GameState& s, int32_t i, const BotPlan& plan, ge::ai::Why* why) {
  ...
  if (why) {
    why->decide("speed");               // a decision: one name, stable across steps
    why->fact("cap", cap);              // what it read and planned on, by name
    why->option("brake", v - cap - 0.5);  // each option weighed, with its score
    why->term("over_plan", v - cap);      //   and the named parts of that score
    why->option("tuck", cap - 1 - v);
    if (turning) why->bar("turning");     //   or why it was ruled out
    why->option("glide", 0);
    why->choose(chosen, because);       // the bot's own choice, and what settled it
  }
```

- **Off is a null pointer, and recording decides nothing.** Compute the choice first, record
  it after; never branch on whether a trace is on. A game's test replays a field traced and
  untraced and holds the digest equal (`tests/fall_line_why_test.cpp`; a machine's pilot the
  same, `tests/helicopter_scenarios_test.cpp` and `tests/fall_line_heli_test.cpp`).
- **A plugin's pilot takes `ai::Why*` last, defaulted to null**, and its game passes
  `why->follow(agent, step)` down from its own decision: the helicopter's `heli_pilot` records
  `way` (the four speed limits, the slowest chosen) and `height` (landed, settle, climb, sink or
  hold), and Fall Line's heli outing its `outing` (board, fly, drop, ski down) above them.
- **Names are string literals** (the record keeps the pointer), snake_case, stable across steps:
  `ge why --flips` compares choices by name, and the timeline makes a column of each fact.
- **Score options so the best one wins.** Where the bot's rule is a chain of ifs, score each
  option by how far past its threshold it is, so the chosen one is the highest and the MARGIN
  reads as how close the call was. A guard or a continuous output (a steer) is a decision with
  facts and a choice and no options.
- **Agent ids**: 0 is the game's pilot (the player's stand-in), then whatever the game's other
  bots are (Fall Line: each rival's racer index). `Why::follow(agent, step)` hands out the
  recorder or null; pass the result straight to the bot.
- A game opts in with `Run::trace(ai::Why*)` (`engine/play/game.hpp`): store the pointer, hand
  it to the pilot and to every other bot, return true.

## Reading why a bot did it

`ge why <log>` (the play executable's `why`, `engine/play/why.hpp`) replays an input log with
the trace on. Nothing is kept in a normal run: a run replays to the bit, so the records are
rebuilt when asked for.

| Command | What it answers |
| --- | --- |
| `ge why <log> --step=S --around=K` | Agent 0's decisions at steps S-K..S+K: what it saw, every option's score and terms, the choice and its margin |
| `ge why <log> --agent=N --decision=speed` | One bot, one decision |
| `ge why <log> --flips [--close=X]` | Every step it changed its mind over the whole run, and the close calls under margin X: dithering and odd choices are there |
| `ge why <log> --timeline` | `runs/<log>-why-<agent>.tsv`: one row a step, each decision's choice, choice index, margin and facts; `ge graph` it |
| `ge shot <log> --step=S --view=bot` | Fall Line: the pilot's mind drawn at a step: its line coloured by the plan's speed, the stretch it reads its cap off, the point it aims at; the caption its line and speed calls (the aim is the `line` decision's `aim_x`, `aim_z`) |
| `{"op":"why"}` in `ge play` | What the pilot would do now and why, without stepping: an agent asks before taking over |
| `--log=why=debug` on a lab | Each decision as a log line, by step, beside every other channel |

**The pilot is asked at every step of the replay.** On a log the pilot played (`ge sim`) that
is what it did. On a log a person or an agent played it is what the bot WOULD have done, and the
answer names the first step it would have played otherwise (`first_disagree`): the quickest way
to see where a bot and a person part.

## The hands: how a decision becomes controls

A person does not press what they decide. `ai::Hands` (`engine/ai/hands.hpp`) turns what a bot
WANTS into what it PRESSES, per axis (`ai::AxisHands`), in this order: a reaction delay (a fixed
ring, up to 0.8 s), a slow wander off what was meant (faded near an end and near rest, so a hand
let go or pressed to a stop sits still), a damped spring (`settle_s`, `damping`: under 1
overshoots a quick move), a slew limit (travel a second), the reach (`lo`, `hi`). A button is
`pass`. Everything is fixed-size and copies cheaply, its noise off its own `core::Rng` stream.

- **The bot records its intent before its hands**: `ai::record_intent(why, names, wanted)`
  writes a decision called `hands`, one fact an axis by the axis's name. The humanness report
  measures the lag against it.
- **A game names its profiles** (Fall Line: `games/fall_line/sim/hands.hpp`, `none` and
  `human`; `--hands=` on the play contract, `SimOptions::hands` in the sim). The default profile
  presses what the bot wants, so no digest moves until a run asks for hands.
- **A pilot asked off the step works on a COPY of its hands** (`ge why` asks the pilot at every
  step, the protocol's `pilot` op asks without stepping): pilot() presses on a copy, and the
  step commits it only when it was asked at that step. Otherwise asking moves the run.
- **Late hands need a bot that looks further ahead.** The bot steers from where it will be when
  its hands catch up (`BotHands::lead()`, Fall Line's `bot_controls(..., lead_s)`): without it a
  0.2 s delay turns a pursuit into a limit cycle from one end of the stick to the other.
  Lower the loop's gain (a longer look); predicting the body a delay ahead feeds the same cycle.
- **A spring settled in two commands or less rings or sticks** (it is explicit Euler): a move that
  needs every command exact (a hop's view on the velocity) takes settle 0 and no delay.
- **A look-ahead or a rollout presses through a COPY of the bot's own hands** (and keyboard): they
  copy cheaply and replay the same wander, so it picks only what the hands can do.

## How human it looks

`ai::axis_humanness` and `ai::decision_humanness` (`engine/ai/human.hpp`) measure a run;
`kHumanBands` holds each metric to a person's band, every band with the assumption it rests on.
`check_axis` / `check_decision` list what is outside as `OutOfBand`.

| Metric | What it is | A person's band |
| --- | --- | --- |
| `saturated` | the share of the steps in use spent at an end of the travel | a stick 0..0.35 |
| `jumps` | steps that cross more than half the travel at once | 0..0.01 |
| `reversals_min` | changes of direction a minute (0.05 hysteresis) | 0..120 |
| `hold_s` | how long a stick sits part-way (a time-weighted median) | a stick 0.05..3 s |
| `lag_s` | the delay at which the pressed best follows the wanted (`hands`) | 0.15..0.4 s |
| `flips_min` | a decision's changes of mind a minute | 0..20 |

`ge sim --human` (the play executable's `sim --human`, `engine/play/human.hpp`) prints them for
every run and warns on each one out of band; it reads a person's log the same way (the wanted is
then what the bot would have meant there). A signed axis is a stick, a lever a trigger, a button
is left out. Fall Line's `humanness` lab sweeps seeds with a field of four, every profile side by
side, and charts the steer meant against pressed.

## The loop: a bot that behaves more like a person

1. Run the bot's labs with `--tag=before` (Fall Line: `make sim`, `make lab LAB=free`,
   `make lab LAB=humanness`), and `ge sim --human` on the seed.
2. Find a robotic moment: a flagged metric, a line too perfect, a stop too late. `ge why --flips`
   on that seed lists where the bot changed its mind; `--timeline` and `ge graph` show the facts
   it chose on round there.
3. Read the decision at that step (`ge why --step=S --around=12`): which term settled it, how
   close the call was.
4. Change the bot (its decisions) or its hands (how a decision becomes controls: the delay,
   the spring, the slew, the wander), never both in one round. An axis out of band is the
   hands'; a decision flipping is the bot's.
5. Run with `--compare=before`, `ge graph` both timelines on one chart, put both in the PR.

**Every behaviour a switch.** A shooter bot's behaviours are named features
(`plugins/shooter/features.hpp`: `calm`, `dodge`, `shortgoals`, `rj` ...), on by default, off with
`se-team --features=-dodge` (a side's own: `--features-red=plain`, an A/B by team in one match), the
bot team's `--features=` and the console's `features -dodge`. A new behaviour lands with its switch
and gates its code on `f.features[...]`, so its A/B is one flag on one build, not an env variable.
The traits say how WELL a bot does a thing; a feature says WHETHER it does it.

## Shooter's Edge's bot plan, worked headless

The shooter bots' plan is a checklist in `docs/goals/M18r.md` § M18r14g: what is done with its
numbers, what is open in order with its yardstick, and what was tried and dropped (never tried
again blind). "Continue the bot work" means: the first open item there, built, judged as below,
its entry updated with the before and after.

**A SESSION KEEPS GOING.** One checkpoint is not a session: after each one (pushed and its
entry updated), take the next open item and go on, checkpoint after checkpoint. Never estimate
the context used (the guesses run twice the real figure): work on until the system compacts the
conversation, keeping the plan's entries, the pushed branch and the handoff current at every
checkpoint so nothing is lost. Stop earlier only for a question that is the maintainer's to
answer.

**The locations: `.bots/local.md`.** Nothing about a machine is in the tree. The game's packs,
people's recordings and the scratch output live where `.bots/local.md` says (git-ignored, one per
checkout; write it on first use, the defaults below when nothing else is known):

```
basedir: .bots/game    # holds the game folder with the packs: every lab's --basedir=
demos: .bots/demos     # people's recordings, one folder a map: <demos>/<map>/*.mvd
work: .bots/work       # traces, A/B runs, samples: anything a run writes
```

A path in it is absolute or relative to the checkout. A machine without the full game plays the
shareware pack's maps (e1m2 is one); dm2 to dm6 and povdmm4 need the player's own full game.

**A MATCH VIDEO OR A DEMO, ON REQUEST.** When the maintainer asks for a video, a demo or "a match
to watch", play one with `make bot-video` (`tools/bot-match-video.sh`) in the background (a
10-minute match takes about 10-20 minutes on the Mac) and answer with the file's path and the
score. It plays a 4on4 with this build's bots on our server, keeps the recording (the demo,
`<work>/videos/<name>.mvd`) and renders it with the camera on the hottest player as
`~/Downloads/bots_<commit>_<map>_s<seed>[_<features>].mkv`. Map the ask onto its flags:

| Asked for | Flags (`make bot-video ARGS="..."`) |
| --- | --- |
| a map, a seed, a length | `--map=dm2 --seed=3 --minutes=5` (defaults e1m2, 1, 10) |
| a feature shown off, or the bots without it | `--features=-hop` (every bot), `--features=+trial` |
| one team with it, one without (a head to head) | `--features-red=-hop` (or `--features-blue=`) |
| the video somewhere else | `--out=<folder>` |
| the demo alone, no video | `--no-video`: the `.mvd` only, played in the window with `-playdemo <mvd>` |

Rendering needs a GPU adapter, so on a machine without one say so and offer the demo alone.

Every match `make bot-video` plays also writes the bots' DECISION LOG beside the demo
(`<work>/videos/<name>.why.tsv`, `se-team --why=<file>`; `games/shooters_edge/labs/why_log.hpp`).

**WHY DID THAT BOT DO THAT, THEN?** When the maintainer asks why a bot did (or stopped doing)
something at a moment of a video or a demo, ANSWER FROM THE LOG, never by guessing: `make bot-why
ARGS="--bot=red2 --at=4:12 [--around=10] [--video=<name>] [--name=mode,trigger]"`
(`tools/bot-why.sh`, the `se-why` lab; the newest log by default). It lists every decision that
bot changed within the window (the fight mode, the trigger, the aim, the pilot's link, the role
itself (`role`: the quad's due, its reach, a door's errand, the bot's place and node) and the role's
goal, sight, hearing...: each with what settled it, the margin and the facts it read) and the
events (a speed lost to a wall or a player, a death), on the video's clock, and how often each
decision is made and changes a bot-minute (several changes a second is jitter, not a mind). Quote
the rows that answer, then say what to change. A question the log cannot answer is a decision
not yet recorded: add it where the bot chooses (§ The rule) so the next match can. A hop's turn
against the move's rules is the `turn` decision (ok / too-tight: the way ahead against the
tightest circle the air turns at that speed; se-team's trace column `tight`).

**THE LOG AND ITS TOOLS ARE OURS TO KEEP RIGHT.** The decision log (`games/shooters_edge/labs/
why_log.hpp`), its reader (`se-why`), `tools/bot-why.sh`, `tools/bot-match-video.sh` and what
`se-team` records are young and imperfect. Whenever one gets in the way (a question it cannot
answer, a row that misleads, a moment that will not line up with the video, a decision missing,
the log too big or too thin, a flag that reads wrong), fix it in the same session, before the
answer, and record a lesson: a missing event or column, a filter, a better count, a clock fixed.
Keep its output honest: when a number is a client's guess and not the server's (the bots
lesson on client-fed state), say so in the row or measure it off the server. A tool fixed once
saves every later question; a tool worked round is asked again. A
video is never a checkpoint's step: checkpoints are judged headless (below).

**NO WISHLIST: THE WISH IS BUILT** (bot work only: everywhere else the router's "say what `ge`
lacked" and its `ge wishlist` stand). Bot work ends with no `ge wishlist`. Whatever would have made the session quicker (a
measure, a column, a filter, a count by spot, a join of the trace to the recording, a flag) is
built into the bot tools in the same session: the labs (`se-team`, `se-people`, `se-why`,
`se-reach`, `se-spot`, `se-moves`, `se-recipes`), the decision log, `tools/bot-why.sh`,
`tools/bot-match-video.sh`, and this skill's lab table updated with the new flag or metric. The
signs that one is owed: a scratch script written to count something, the same script run twice,
rows matched by hand (a bot by its position, a clock by its offset), a number read off a picture.
Build it before the next item, or at the latest before the session's last checkpoint. What is
too big for the session goes into the plan (`docs/goals/M18r.md` § M18r14g) as an open tool item
(the question it answers, the steps it saves, where it goes) and into the handoff, never as a list
at the end of the report. The report says what was built ("se-team now counts passes by item
spot").

**A checkpoint** is a commit, a push, and the plan's entry updated, judged by the simulations
(the sets, head to head, the staging). The build gates (`make precheck`, the lint, every lab's
smoke digests) are NOT run at a checkpoint: the handoff names the last commit they ran on, and the
agent asked for a pull request runs them first (a golden or digest a change moves on purpose
rewritten then, and the commit says why). A change in `plugins/shooter` or
`games/shooters_edge/bots` moves labs precheck does not reach: run EVERY lab's smoke digests
(`labs --all --smoke --shard=i/8 --quiet --check-digests=tests/digests`, eight at once) and rewrite
moved ones one lab at a time (parallel writers keep one file). A test held by a few short matches
that flips is fixed by measuring what it says or by more seeds; never lower its floor, and name
the outlier runs in the commit. No video and no live server: every match runs on our
own server, in process (`se-team`), headless.

**How a change is judged.**

- **Two axes, always both**: frags head to head, and a HUMAN measure against people's (se-people's
  `hand.*`, sighting, speed, item control, wet). A feature that brings a human measure closer to
  people's is kept at a small frag cost (within a set's noise); it is dropped only when it is
  costly and no more human, or when it moves a human measure away from people's. For movement,
  SPEED decides first: a person moves fast, a bot that crawls is spotted at once.
- **Head to head is the verdict**: one team with the feature, the other without, in the same 4on4
  (`se-team --features-red=-<name> --sides=1`), AND AGAIN WITH THE SIDES SWAPPED
  (`--features-blue=-<name>`), each read against the same build's full-against-full set (the
  sides are not even, and their skew moves with the seeds); the effect is the mean of the two.
  One side alone is a draw (dm2: -7.4 on blue, -0.3 on red, for a change even over 24 seeds).
  10-minute matches, 12 seeds. Both sides with it only says whether the game got faster or more
  violent. Each side's speed and bumps: `team.side.<side>.*`.
- **Six seeds, never one, in one call**: `se-team --seeds=1-6 --variants="on:--sides=1;off:
  --features=-<name> --sides=1;h2h:--features-red=-<name> --sides=1" --client-fed=1 --minutes=10
  --basedir=<basedir> --map=e1m2` plays every variant on every seed in parallel (`--jobs`, one a
  hardware thread) and reports the A/B table (red-blue, frags a minute, damage, accuracy, the key
  items lying, speed, wet, stuck, waits), a row a run, and every metric's mean as
  `<variant>.<metric>`; a match in a set gives the same numbers as alone. A single match swings
  by more than most changes move it. A quick read first: `--minutes=3` with
  `--trace=<work>/<tag>/t-{variant}-{seed}.tsv --trace-every=1` (`{variant}` and `{seed}` are
  replaced per run).
- **People and bots by one rule**: `se-team --save=<file>.mvd` records the bots' match and
  `se-people --file=<it>` measures it exactly as it measures people's (`--dir=<demos>/<map>`).
- **What it newly triggers**: a change that should help and hurts is read for what it set off;
  break a number down by its cause before tuning a knob. **A behaviour that rarely runs cannot
  move a number**: count how often it runs, and what holds it off, first. **Test a guessed cause
  against its base rate**: the bad outcome's odds with and without the state, and the same state
  in people's recordings, before building for it.
- **Ask people's recordings before guessing**: where the bots lag people, measure how people play
  that place, item or moment (se-people `--samples`, se-moves, se-rj) and copy it. When the
  maintainer describes how a move is done or names a fault seen in a match, build or check it at
  once; the measures then judge it.
- **The recording is the yardstick, the trace finds the cause**: speed, keys, bumps, stops and
  facing are compared off recordings by one rule (se-people, se-moves, se-recipes on `se-team
  --save` matches; experts against people's experts, `--skills=expert`), at the finest rate both
  sides have (a 0.1 s sample hid the brief stops); never off the trace (its speed counts blasts, it
  sees through walls). The trace (`--trace --trace-every=1`) splits a number by cause: who turned
  the view (`view`), who wrote the keys (`keys`), the pilot's `because`.
- **A small set is read for the behaviour's own measure**: a six-seed 5-minute set swings frags
  ±30 %; a fight change shows first in `team.accuracy`, a projectile's lead in `--drill=rockets`.
  A variant on the same seeds as an earlier set repeats its base bit for bit: compare against it. A
  collapsed match is often one bot stuck in one cell or players stacked at one point: read "where
  they stand still" first.
- **A switch is judged before a preset gets it**: `kAll` is every bit under `trial`, so a new
  switch sits after `trial` (out of `features_all`) until its verdict; bit-identical sets mean the
  variant never ran. `--features=all,+x` also turns on what a preset keeps off: write `+x`. A
  pilot fix not yet a switch is gated whole on `trial` (`Bot::trial`) for its A/B.
- **Rare moves are staged, not played**: gap tricks, escapes, a pit's falls (`se-reach
  --gaps=fly`, `--escapes=fly`, `--walk`), under the keyboard and the trailing view matches press
  with (`keys` in `--gap-features`, `--walk-view-lag`). A staging that disagrees with the match is
  the staging's bug first: say which switches it holds under.

**The pilot's rules** (a first-person bot):

- **Test under human hands and the keyboard from the first line.** Plain hands hide orbits,
  overruns and late jumps.
- **Keys and views land late** (a hand's delay; the keyboard's 1-3 commands): choose keys for the
  view when they land (`view_when_keys_land`), never the view now or wanted. A pair a person presses
  as one move (fire and jump) waits for the slower path (`shotjump`); "landed" after a jump ask
  waits a lift-off grace (`liftoff`), or every late jump reads as a miss.
- **A bot knows how its body meets the room**: predict by flying the box through `player_move`
  (gravity, landings, the slide along walls) for the keys it will press, never a swept line or a
  guess. A guard looks as far as the flight carries and runs in the air too.
- **A proof is a move the pilot can start and reproduce**: one keys function and the move's water
  level for both, a start it reaches (lined up, room to run up and to turn), no view a hand cannot
  have (it stops at 80 degrees pitch). A refusal on a route's first command is the planner's.
- **Every view goes through the one hand** (`engine/ai/mouse.hpp`; `bot_hand_override` for a view
  set after the step), and a writer turns the wish under its view (`turn_axes`, `turn_axes_swim`).
  A one-command view (a rocket jump's shot) is a contract: announced (`Pilot::shot_*`), fired at
  `shot_ready`. After a change to who writes the view, read a move's OUTCOME, not its count.
- **A swimmer is never `onground`**: ask what each check gated on it does in water.
- **The planner reads what it reads**: a shut link is honoured by `nav_route` AND `nav_reach`, or
  the goal stays "reachable" and the bot stands planning. A fix to a belief that moves no bit means
  the planner never saw it: print what it is handed. State kept per link resets when the pilot
  comes back to the link.

**COPYING MOVEMENT OFF RECORDINGS** (M18r.md § 0r). A hard move is copied whole from people's
recordings, never rebuilt by hand from their averages: `plugins/shooter/recipes.hpp` cuts a track
into instances (`read_hop_starts`: frames from the low, the keys read off the move as rocket jumps
read them), and `play_recipe` plays an instance's keys and view back through `player_move` (the
PROOF: people's keys played on an open floor reach their recorded speed, or the reader is wrong).
`se-recipes --dir=<demos>/<map> --basedir=<basedir>` tables them by level and spot, times every
stop to 400 (people and bots by one rule: run it on `se-team --save` matches too), `--prove=N`
checks the reader, `--book=<file>` writes the rows a game compiles in
(`games/shooters_edge/bots/hop_start_book.cpp`; splice the rows into `kRows`, the format is in
its header). The pilot (`pilot_recipes.hpp`) chooses the nearest entry to its state, turns it to
its way, FLIES it by the move from where it stands on the map and commits only a proven one, then
plays it closed loop against the proof. Three things learned getting it to fly in a match:
- the recorded VIEW is already a person's mouse: play it as it is (`bot.cpp` `recipe_on`); a
  watch or the mouse hand over it chased the sweep and lost the start (takeoff 341 -> 416);
- the keyboard presses late, and W and S share a finger: ask keys a press early
  (`recipe_keys_s`) and keep starts that press back on the ground out of the book;
- keep the book to starts a bot can use on its way: off the ground soon (0.9 s), set off near the
  way (75 degrees); people's loops away and back read as made starts too.

**The labs it is worked with** (`build/release/labs <name> ...`, `--basedir=<basedir>`):

| Lab | What it answers |
| --- | --- |
| `se-team` | a 4on4 of our bots on our server (a set: `--seeds`, `--variants`, `--jobs`; `--size=2` a 2on2, as people's dm2 recordings are: compare in the same size): `--map`, `--seed`, `--minutes`, `--client-fed=1`, `--sides=1` (each side's numbers), `--features`, `--features-red`, `--features-blue`, `--trace=<tsv>` (`--trace-every`: every bot's command as a row: feet, speed, mode, goal, pilot mode, link kind, the view and who turned it, the keys sent and the velocity), `--save=<mvd>`; the hop by skill always (`team.hop.<skill>.share`, `speed-p50`, `fast-share`, `flights`, `crash-share`, `gain-p50`, `takeoffs-360`, `chained-share`), the deaths in lava or slime (`team.deaths-in.*`) and the view's swipes at a key item's spot (`team.item-glances-per-min`, `itemglance`); every death as a row in `deaths.tsv` (where, the liquid, the mode, the way in; a set's behind its variant and seed: read it before writing a script); why and where they stand still (`team.still.mover-share`, `no-route-share`, `walk-share` and the top spots); the view's reversals by what turned it (`team.view.<writer>.*`, `team.view.reversals-per-s`, people 0.09); on a real map, every fast flight (380-460 at takeoff) read off the recording by se-moves' rule and put down to the pilot mode that flew it (`team.air.s380.<mode>.gain-p50`, `crash-share`) |
| `se-people` | people and bots measured alike off recordings: in view, faced, speed, the hand's sweeps (`hand.*`, `hop.*`), fire in view, item control, wet, wall and player bumps (`bumps.*`, a minute and a minute at speed); the glance at a key item's spot passed (`glance.key.*`: of the passes starting with the spot off the screen, the share it comes onto the screen (`shown`, `shown-aside` off the run) or within 20 degrees of the view (`glanced`), by item; `seen` of every pass; a table by spot); a key item AS IT SHOWS (`show.<item>.*`: the last taker's mates and the enemy on and near the spot, who takes it, the taker there waiting, the wait p50/p75, taken within 1 s, taken full; a table by spot: where the bots lag people's timing) and the armour worn by type (`armour.wear.*`); `--samples=<tsv>` every sample alive, `--events=<tsv>` pickups, showings (`shown`), deaths, frags |
| `se-moves` | who moves fastest and the weave by speed, per player, off a folder of recordings (`--dir`); the ground between jumps (`jumps.ground-p50`, what a landing costs) and the chains (`jumps.chain-3plus-share`, `jumps.chain-first-off-p50`) |
| `se-rj` | rocket, grenade and gap jumps read off recordings and clustered into tricks |
| `se-recipes` | people's hop starts read off recordings (by level, the spots, `hop_starts.tsv`, `--frames`), every stop timed to 320 and 400 (`stop.*`: people's 35 % at 400 within 2.5 s), `--prove` the keys played back, `--book` the rows the bots copy |
| `se-reach` | a map's nav asked: `--route` node by node, `--floors`, `--escapes=fly`; `--gaps=fly` stages every gap trick with an expert's pilot from 160-640 behind its lip (planned, jumped, takeoff speed, landed, refused; `--gap-trace=<tsv>` every command with the pilot's reason; pressed through an expert's keyboard as matches are while `keys` is in `--gap-features`, `-keys` the analog pilot): a gap jump judged in seconds, not in matches; `--walk='x y z > x y z'` walks a route through an expert's keyboard under a view trailing its heading (`--walk-view-lag`), already running at 250/320/400 on five headings, the runs into lava or slime tabled per `--walk-variants`: a pit's falls judged in seconds (a match has ~5, too few to judge) |
| `se-spot` | a place drawn every way (top, cuts, inside views, a route); `--clip-top=<z>` cuts the map away over a height and steps through the roofs under it, to study a level under another |
| `se-trick` | one bot down a trick map's course (ztrain, zjumps, ztricks, hoppa2 in the game folder's maps; `.bots/local.md`): the sections off the teleporters, every try's fails and stalls, each section from above and what the nav reaches (roofs cut away; `--clip-top`), `--trace` every command, `--backup`, `--spacing`; defaults to the trick maps' switches `+gappast,+lipjump` |
| `se-bhop` | THE HOP JUDGED FIRST: one fighter a skill down the hop course (`labs/hop_course.hpp`, a practice map compiled in the run, no packs needed), section by section: lap time, speed in and out, takeoffs and their gain, landing jumps, walls, forward in the air, reversals, the mouse with the key, the lean; the line from above coloured by speed over the nav's route; `--features`, `--seeds`, `--trace=<tsv>` (every frame: keys, view, the hop's aim, side and air set), `--write-plan=<file>` (~4 s) |
| `se-duel` | 3-minute duels, deathmatch mode 4 on povdmm4 by default |

## What lives where

| Piece | Home | Status |
| --- | --- | --- |
| The decision record and its forms | `engine/ai/why.hpp` | built (M16a) |
| `ge why`'s engine half: replay with the trace, timeline, flips | `engine/play/why.hpp`, `play why` in `main.cpp`, `ge why` in `engine/cli/play.cpp` | built (M16a) |
| The hands, the humanness metrics and bands | `engine/ai/hands.hpp`, `human.hpp` | built (M16b) |
| The report off a log, `ge sim --human` | `engine/play/human.hpp`, `sim --human` in `main.cpp`, `ge sim` in `engine/cli/play.cpp` | built (M16b) |
| A skier's pilot (pure pursuit, the speed plan on the limits) | `plugins/skier/pilot.hpp` | built (M16c) |
| Paths (one heap, grid and graph search), thinking on a schedule | `engine/ai/path.hpp`, `think.hpp` | built (M16d) |
| Sight and memory, steering (pursuit along a line, a fan of headings, separation over a grid, the stuck check), choosing (a utility selector, a flat state machine, both decision records) | `engine/ai/sight.hpp`, `memory.hpp`, `steer.hpp`, `choose.hpp` | built (M16e) |
| A first-person map's navigation, every link proven by running the player move (the first plugin user of `ai/path`'s graph search on its own) | `plugins/shooter/nav.hpp` | built (M18r1) |
| A first-person pilot: a nav route played one usercmd at a time through `ai/hands` (pursuit, jumps at their proven speed from a run-up, the strafe jump, waiting for doors and riding a plat), a server slot's command source; its runs on the nav course and the arena | `plugins/shooter/pilot.hpp`, `games/shooters_edge/bots/pilot_run.hpp`, `se-pilot` | built (M18r2) |
| A first-person bot's senses and aim: what its client is sent, sight in a cone by the hull trace, hearing, memory, aim on `ai/hands` by skill (perfect, expert, average, novice), the route kept while aiming | `plugins/shooter/senses.hpp`, `aim.hpp`, `bot.hpp`, `games/shooters_edge/bots/duel_run.hpp`, `se-senses` | built (M18r3) |
| A shooter bot's behaviours as switches, the plain bot and the presets | `plugins/shooter/features.hpp` | built (M18r14) |
| A deathmatch fighter on the baseline mod: its own stats, the map's items timed off what is sent, the mode (`ai::enter`), the item goal and the weapon (`ai::pick_best`), the trigger; four of them played on the arena | `games/shooters_edge/bots/fighter.hpp`, `fight_run.hpp`, `se-bots` | built (M18r4; the rocket jump M18r4b) |

A piece lands with its first user, never before (G4). A bot a game writes before its piece exists
still records its decisions, and moves onto the piece when it lands.
