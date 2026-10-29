---
name: shooter
description: "Use for plugins/shooter in gameengine2: the first-person shooter's simulation half. The classic player move in 32-bit floats (every step in order, every move type, the server settings at the modern defaults), clip hulls (planes, clip nodes, point contents, the box hull, the line trace with its 1/32 back-off), hulls built from convex brushes, the move's list of solids and its traces, the recorded scenarios the move is held to, and the pmove lab; the classic game loop (the game-program seam, entities, the area tree, world traces, physics frames on the run clock, thinking, toss and bounce, step, pushers, touches, new missiles, a player's packet and command, the client's commands and prediction), Shooter's Edge's native program and scripted match, the thrown-entity vectors and the se-loop lab; the M18o rows."
---

# The shooter plugin

`plugins/shooter/` (`ge_shooter`, namespace `ge::shooter`) holds what any first-person shooter on
the engine moves its players with. It depends on the engine's core only
(`ge_add_plugin(shooter ENGINE core)`). The axes are the map format's (x and y across, z up);
a game converts to the engine's y-up at its edges. `docs/shooters-edge.md` is the design;
`docs/goals.md` M18o is the row.

| File | What |
| --- | --- |
| `shooter/vec.hpp/.cpp` | `Vec3f` in float: `dot`, `length` (root in double), `normalize`, `angle_vectors` (radians rounded to float, sines through `core/libm`) |
| `shooter/hull.hpp/.cpp` | Clip hulls: `Plane`, `ClipNode`, `Hull`, `point_contents`, `trace_hull`, `make_box_hull` |
| `shooter/brush_hull.hpp/.cpp` | Hulls from convex brushes as a plane chain: `build_point_hull`, `build_box_hull`, the player's box |
| `shooter/world.hpp/.cpp` | A move's solids (`MoveWorld`): `player_trace`, `box_free`, `water_contents`, `brush_contents` |
| `shooter/pmove.hpp/.cpp` | The move: `MoveCmd`, `MoveSettings`, `MoveState`, `player_move`, `run_command` (the 50 ms split) |
| `shooter/slide.hpp/.cpp` | The slide along planes, the step-slide up stairs, `clip_velocity` |
| `shooter/recording.hpp/.cpp` | The recorded scenarios: the text format, `replay` line by line, the tolerance |
| `labs/pmove_lab.cpp` | The `pmove` lab: every scenario's worst error, paths from above and the side |
| `shooter/program.hpp` | The game-program seam: hooks and functions by number, the globals |
| `shooter/entity.hpp`, `server.hpp/.cpp` | The entity record and pool, linking, trigger and impact touches, sounds, `server_hash` |
| `shooter/area.hpp/.cpp`, `trace.hpp/.cpp` | The 31-node area tree and its listing; world traces and `test_entity_position` |
| `shooter/physics.hpp/.cpp`, `push.hpp/.cpp` | The physics frame: thinking, toss, bounce, fly, step, noclip, water, new missiles; pushers |
| `shooter/frame.hpp/.cpp`, `client.hpp/.cpp` | The server's frame on the run clock; a player's packet and command, the move's solids |
| `shooter/usercmd.hpp/.cpp`, `predict.hpp/.cpp` | The client's commands; its prediction from the protocol's quantized state, smoothing |
| `shooter/toss_recording.hpp/.cpp` | The thrown-entity vectors (`tests/golden/shooter_toss_recording.txt`) and their replay |
| `games/shooters_edge/sim/` | The native program (`rules`), the test arena, the scripted match (`run_match`) |
| `games/shooters_edge/labs/se_loop_lab.cpp` | The `se-loop` lab: the match's events, counts, paths and corrections; the vectors |

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
- Still to come: the cvars bound through `engine/console` (setting `pm_airstep` sets
  `pm_pground`), a map's compiled hulls (M18b) answering `MoveWorld`, and the other entities'
  interpolation on the client (M18h).

Read this skill's lessons first: `make lessons ARGS=shooter`.
