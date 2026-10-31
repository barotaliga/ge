---
name: shooter
description: "Use for plugins/shooter in gameengine2: the first-person shooter's simulation half. The classic player move in 32-bit floats (every step in order, every move type, the server settings at the modern defaults), clip hulls (planes, clip nodes, point contents, the box hull, the line trace with its 1/32 back-off), hulls built from convex brushes, the move's list of solids and its traces, the recorded scenarios the move is held to, and the pmove lab; the classic game loop (the game-program seam, entities, the area tree, world traces, physics frames on the run clock, thinking, toss and bounce, step, pushers, touches, new missiles, a player's packet and command, the client's commands and prediction), Shooter's Edge's native program and scripted match, the thrown-entity vectors and the se-loop lab; the M18o rows. Also the bots' navigation proven by the move (nav.hpp: floor nodes off the hulls, walk, drop and jump links proven by player_move, movers tagged, routes on ai/path, plats ridden), the nav course and the se-nav lab; M18r1. Also the pilot (pilot.hpp: a route played as usercmds through ai/hands, jumps from a run-up, the strafe jump, doors waited for and barred, plats ridden), its runs on the course and the server's arena (games/shooters_edge/bots) and the se-pilot lab; M18r2. Also the bot's senses and aim (senses.hpp: what a client is sent, sight in a cone by the trace, hearing, memory; aim.hpp: skill profiles on ai/hands, turn_axes; bot.hpp: the pilot with senses and aim, a slot's command source), the watched duel and the se-senses lab; M18r3. Also the fighter on the baseline mod (games/shooters_edge/bots/fighter.hpp: stats, item timing, mode, goal, weapon and trigger on ai/choose; fight_run.hpp: four fighters on the arena), the se-bots lab; M18r4. Also the engine mover's classic row over brush worlds (`row.hpp`, `hull_world.hpp`) and its parity; M18e. Also the server's host, its bot slots and command sources, the input log and its replay, the stand-in bot, the shooters_edge_server binary and the se-server lab; M18f. Also the loop's settings a QC server uses (first_dynamic, a full pool reusing the last entity, the command's movement); M18p's loop changes."
---

# The shooter plugin

`plugins/shooter/` (`ge_shooter`, namespace `ge::shooter`) holds what any first-person shooter on
the engine moves its players with, and how its bots find their way. It depends on the engine's
core and `ai` (`ge_add_plugin(shooter ENGINE core ai ...)`: the nav graph searches on `ai/path`).
The axes are the map format's (x and y across, z up); a game converts to the engine's y-up at its
edges. `docs/shooters-edge.md` is the design;
`docs/goals/M18o.md` is the row.

| File | What |
| --- | --- |
| `shooter/vec.hpp/.cpp` | `Vec3f` in float: `dot`, `length` (root in double), `normalize`, `angle_vectors` (radians rounded to float, sines through `core/libm`) |
| `shooter/hull.hpp/.cpp` | Clip hulls: `Plane`, `ClipNode`, `Hull`, `point_contents`, `trace_hull`, `make_box_hull` |
| `shooter/brush_hull.hpp/.cpp` | Hulls from convex brushes as a plane chain: `build_point_hull`, `build_box_hull`, the player's box |
| `shooter/bsp_world.hpp/.cpp` | A read brush map (29, BSP2) as the server's brush models: hull 0 from the node tree, hulls 1 and 2 from the clip nodes |
| `shooter/world.hpp/.cpp` | A move's solids (`MoveWorld`): `player_trace`, `box_free`, `water_contents`, `brush_contents` |
| `shooter/pmove.hpp/.cpp` | The move: `MoveCmd`, `MoveSettings`, `MoveState`, `player_move`, `run_command` (the 50 ms split) |
| `shooter/slide.hpp/.cpp` | The slide along planes, the step-slide up stairs, `clip_velocity` |
| `shooter/recording.hpp/.cpp` | The recorded scenarios: the text format, `replay` line by line, the tolerance |
| `shooter/hull_world.hpp/.cpp` | The engine mover's world over brush models' clip hulls (`mover_world`, the axes `to_engine`/`to_map`, `contents_at`) and a hull 0's liquids laid as `engine/water` volumes (`add_liquids`) |
| `shooter/row.hpp/.cpp` | The classic row on the engine's mover and swimmer: `classic_rows` from the settings, `row_body`, `row_command` |
| `shooter/row_recording.hpp/.cpp` | The row against the recordings, one command at a time and as runs (`replay_row`, `row_covers`) |
| `labs/pmove_lab.cpp` | The `pmove` lab: every scenario's worst error, paths from above and the side |
| `shooter/program.hpp` | The game-program seam: hooks and functions by number, the globals |
| `shooter/entity.hpp`, `server.hpp/.cpp` | The entity record and pool, linking, trigger and impact touches, sounds, `server_hash` |
| `shooter/area.hpp/.cpp`, `trace.hpp/.cpp` | The 31-node area tree and its listing; world traces and `test_entity_position` |
| `shooter/physics.hpp/.cpp`, `push.hpp/.cpp` | The physics frame: thinking, toss, bounce, fly, step, noclip, water, new missiles; pushers |
| `shooter/frame.hpp/.cpp`, `client.hpp/.cpp` | The server's frame on the run clock; a player's packet and command, the move's solids |
| `shooter/usercmd.hpp/.cpp`, `predict.hpp/.cpp` | The client's commands; its prediction from the protocol's quantized state, smoothing |
| `shooter/nav.hpp/.cpp` | Navigation proven by the move: floor nodes off the hulls, walk, drop and jump links proven by `player_move`, movers tagged, a mover given a travel ridden (top nodes, ride links, the ways off proven with it there), `nav_route` on `ai/path` |
| `shooter/pilot.hpp/.cpp` | The pilot: a route played one command at a time (pursuit, the jump's run-up, the strafe jump, waits and bars, the ride), hands, the stuck and lost checks, a slot's command source |
| `games/shooters_edge/bots/pilot_run.hpp`, `labs/se_pilot_lab.cpp`, `labs/nav_top.hpp` | The pilot's runs on the nav course and the server's arena, and the `se-pilot` lab: trails from above, times against proven, humanness |
| `shooter/senses.hpp/.cpp` | The bot's senses: what a client is sent (`client_view`), sight in the view's cone by the hull trace, the sounds heard, contacts on `ai/memory` |
| `shooter/aim.hpp/.cpp`, `shooter/bot.hpp/.cpp` | Aim by skill on `ai/hands`, `aim_angles`, `turn_axes`; the bot: the pilot with senses and aim, a slot's command source |
| `games/shooters_edge/bots/duel_run.hpp`, `labs/se_senses_lab.cpp` | The watched duel (a seer and a runner on the arena, the truth kept beside) and the `se-senses` lab |
| `games/shooters_edge/bots/fighter.hpp/.cpp`, `fight_run.hpp/.cpp`, `labs/se_bots_lab.cpp` | The fighter on the baseline mod (stats, items timed, mode, goal, weapon, trigger) and four of them played on the arena; the `se-bots` lab |
| `shooter/toss_recording.hpp/.cpp` | The thrown-entity vectors (`tests/golden/shooter_toss_recording.txt`) and their replay |
| `games/shooters_edge/sim/` | The native program (`rules`), the test arena, the scripted match (`run_match`) |
| `games/shooters_edge/sim/nav_course.hpp`, `labs/se_nav_lab.cpp` | The nav course (stairs, a crate, a pit, two ramps) and the `se-nav` lab: the graph from above, routes as proven |
| `games/shooters_edge/labs/se_loop_lab.cpp` | The `se-loop` lab: the match's events, counts, paths and corrections; the vectors |
| `shooter/cmd_source.hpp` | The bots' seam: a slot's command source, read-only on the server |
| `games/shooters_edge/server/` | The server's host (slots, bots' packets, the frame), the input log and its replay, the stand-in bot, the `shooters_edge_server` binary (`bin/main.cpp`) |
| `games/shooters_edge/client/` | The `shooters_edge` window (M18h's first slice): the search path over a game folder's packs, the listen server (the baseline mod, the player as slot 0, map changes), the keys and raw mouse as commands, the HUD's status and the camera's subject, the eyes and markers, the world's frame; `tests/shooters_edge_listen_test.cpp` (a rocket jump, golden) and the `se-client` lab |
| `games/shooters_edge/client/demo*.hpp`, `timedemo.hpp`, `labs/gpu_se_timedemo_lab.cpp` | Recorded games read frame by frame (the single-player demo, protocol 15, the multiplayer client's recording and the server's multiview recording, protocol 28, told apart by their first bytes, gzip unpacked; `demo_single.cpp`, `demo_client.cpp`, `demo_multiview.cpp`), written in code for tests (`demo_write.hpp`), and the demo BENCHMARK (`timedemo <demo>`, `-timedemo <demo>`: one demo frame a drawn frame, vsync off; average, max, min, 1% and 0.1% low), playback at the demo's pace (`playdemo`, `cl_demospeed`, `demo_track`); a demo turned into a video (`demo_video.hpp`, `-demovideo <demo> <file>`: the frame clock, the demo's sounds `demo_sound.hpp` and voices `demo_voice.hpp`); the voice formats `.qwdv` and `.mvdv` (the voice header) and our recorders (`net/classic_record.hpp`); `tests/shooters_edge_demo_test.cpp`, `tests/shooters_edge_classic_record_test.cpp`, the `se-timedemo`, `se-demo` and `se-demo-video` labs |
| `games/shooters_edge/labs/se_server_lab.cpp` | The `se-server` lab: a full server of stand-ins, its players and counts, the log replayed, the trails |

## Rules

- **Floats where the move is float.** Positions, velocities and every sum are 32-bit floats in
  the order the code writes them (no contraction, the build's flags). Only a length's square
  root and the view's sines are taken in double and rounded to float; the radians are rounded
  to float before the sine. A double literal in a float expression promotes it: write `0.1f` or
  cast on purpose, and say why.
- **Held two ways.** `tests/shooter_recording_test.cpp` holds every recorded line within the
  tolerance (today all 4033 agree in every printed digit); `tests/shooter_pmove_test.cpp` holds
  the move's own bits per scenario in `tests/golden/shooter_pmove.txt`. A deliberate change
  rewrites the second (`GE_GOLDEN_WRITE=1`) and must keep the first green; say why in the PR.
- **The recordings are measurements.** `tests/golden/shooter_recording.txt` is never edited: a
  scenario runs on the scene printed before it.
- **Run the lab before and after** a change to the move or the hulls:
  `make lab LAB=pmove ARGS="--tag=before"`, then `--compare=before`; `--draw=<names>` picks the
  scenarios drawn. Rewrite its digest with `make ge ARGS="digest pmove --write"` when it moves.
- **The loop is the plugin's; the rules are the program's.** Nothing in `plugins/shooter` knows a
  weapon, a door or a score: those are a program behind `Program::call` (Shooter's Edge's native
  one in `games/shooters_edge/sim/rules.cpp`; M18p's bytecode one later). Entities are addressed
  by NUMBER, never by pointer: the pool never reallocates, but any program call may free or
  reuse a slot, so check `free` after every `call_program` (and after anything that can call
  one: a link with touches, an impact, a push).
- **The server's clock is the run clock.** `server_frame` advances time as `start_time + frame x
  sv_mintic` (multiplied out, never summed), runs the packets in the order given, then physics;
  nothing reads a wall clock. The match's `server_hash` golden
  (`tests/golden/shooters_edge_match.txt`) and the `se-loop` digest hold a run to the bit.
- **Held three more ways.** `tests/shooter_toss_recording_test.cpp` holds every recorded thrown
  flight (all 454 frames exact today); `tests/shooter_physics_test.cpp` holds the spec's thinking
  examples and the physics' bits (`tests/golden/shooter_physics.txt`); the match test holds the
  whole loop. Run `make lab LAB=se-loop` before and after a change to any loop file.
- **A server replays from its input log alone.** The host logs, per frame, the slot events and
  every packet it handed `server_frame` (a bot's as well as a person's) and the hash after it;
  `replay_input_log` runs the log with no bot at all. So a command source READS the server and
  never writes it, and draws only off its own seeded stream (`core::Rng`), never the server's.
  `tests/golden/shooters_edge_host.txt` and the `se-server` digest hold a server of eight
  stand-ins; run `make lab LAB=se-server` before and after a change to the host, the log or a
  bot.
- **A QC program's server sets the pool's rules.** `ServerSettings::first_dynamic` (33: after all
  32 client slots, as a mod's entity numbers expect) and `full_reuses_last` (a full pool clears
  and reuses the last entity, counted in `spawns_full`) default off, so the native program's
  digests do not move; the mod server (`games/shooters_edge/server`, the `progs` skill) turns
  both on. `Edict::movement` carries the command's moves to the program before each command.
- **The engine mover's row is held two ways too.** `tests/shooter_row_recording_test.cpp` holds
  every covered scenario's lines within the recordings' own tolerance, step by step and as runs;
  `tests/shooter_row_test.cpp` holds the row's own bits (`tests/golden/shooter_row.txt`). A
  change to `engine/move/step.cpp` or `engine/water/swim.cpp` can move the second: rewrite it
  only when the first stays green. A classic behaviour the row lacks is a new engine KNOB, off
  by default (the `move` skill), never a branch on the classic in `engine/`.
- Still to come: the cvars bound through `engine/console` (setting `pm_airstep` sets
  `pm_pground`), a map's compiled hulls (M18b) answering `MoveWorld`, and the other entities'
  interpolation on the client (M18h).

Read this skill's lessons first: `make lessons ARGS=shooter`.
