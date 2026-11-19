# Movement: each game's player move, prediction and the aids

How a player moves: each classic game's own move, bit for bit; the arena move every shooter can
pick; the client's prediction of its own moves and of other players; the feel a server tells its
clients; and the aids that make a game's hard moves reachable (the easy jump, the blast-jump
helper). The player meets it every step; remix meets it as "this game played with that game's
move". The move's knobs for any genre are `docs/movement.md`.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/move/` | The mover for any genre: knobs and presets (`classic_arena`, `strafe_arena`, ...), the step over a trace callback (`docs/movement.md`) |
| Shooter kit | `plugins/shooter/pmove.hpp`, `framemove.hpp`, `slide.hpp`, `vec.hpp` | The first game's move in 32-bit floats over clip hulls, every move type; the original frame move; the slide and step |
| Shooter kit | `plugins/shooter/row.hpp`, `row_recording.hpp` | The classic numbers as rows of the engine's mover, held to the recordings |
| Shooter kit | `plugins/shooter/settings.hpp` | The move's and the loop's server settings by name (`sv_playermove`, the `pm_` switches), ranges, server info |
| Shooter kit | `plugins/shooter/usercmd.hpp`, `client.hpp` | The client's commands (rate, msec carry, key shares, the angle grid); the server's speed check on them |
| Shooter kit | `plugins/shooter/predict.hpp` | The client's prediction: the protocol's player state, replayed commands, error smoothing, other players run forward |
| Shooter kit | `plugins/shooter/mouse_feel.hpp`, `controls.hpp` | The mouse's feel (`m_filter`, `m_accel`, raw input); the move buttons and mouse look |
| Shooter kit | `plugins/shooter/speedjump_assist.hpp`, `rjump_assist.hpp`, `circle_jump.hpp` | The easy jump, the blast-jump helper, the circle jump |
| Shooter kit | `plugins/shooter/recording.hpp`, `frame_recording.hpp`, `toss_recording.hpp` | The recorded moves the goldens hold every move to |
| Classic plugin | `plugins/classic/sequel_move.hpp` | The sequel's move: packed integer state over brushes, its own rules (`SequelFeel`: top speed, friction, air acceleration, swim, landing pause, air step) |
| Classic plugin | `plugins/classic/predict_original.hpp`, `sequel_view.hpp` | The client's prediction on the original move; the sequel's view by the server (bob, run tilt, kicks) |
| Engine | `engine/camera/first_person.hpp` | View height, step smoothing, bob, strafe roll, kick, landing dip |
| Shell | `games/shooters_edge/net/sequel_predict.hpp` | The player's own moves predicted ahead of a sequel server, over its brushes and movers |
| Shell | `games/shooters_edge/server/rjump_rules.hpp` | The blast-jump helper on a server: `sv_rjump`, the grant |

## Each game's

| | The first game | The sequel | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Move | `pmove.hpp` (floats, hulls, split commands) | `sequel_move.hpp` (integers in eighths, brushes, never split) | its own move, held to its own recordings (a reader's facts first) | its own (2D with heights), plus the converted map played by the first game's move |
| Prediction | `predict.hpp` | `sequel_predict.hpp`, bit for bit on our server | its own | its own |
| View | the engine's camera (`cl_bob` and kin) | its rules' view (`sequel_view.hpp`), eased between 10 Hz frames | its own client half | its own bob |
| Settings | `sv_` and `pm_` (`settings.hpp`) | gravity and air acceleration only, plus ours by `move-feel` | its own | none but ours |

## Adding a game

1. A clean-room reader writes the move's facts step by step (each rule and its precision) under
   `porting/shooters-edge/`; recordings of the stock program are the black-box truth.
2. The move as a pure, allocation-free function over a world callback, beside `sequel_move.hpp` in
   `plugins/classic`, held to its own bits by a golden.
3. It runs over `MoveWorld` (`maps.md`), so it plays on every map family: the remix slot for the move.
4. Its prediction for a joined server; on our server, the other players' states and moves sent
   by an extension bit (`other-moves`) so they run forward.
5. The aids: the easy jump and the blast-jump helper run their commands through the game's move;
   the share a server allows travels by an extension bit (`speedjump-share`).
6. A per-client move feel goes only to clients that announced the `move-feel` bit; a stock client
   given it would mispredict.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Client prediction of the own player | the first game's later protocol | `predict.hpp`; the sequel's own `sequel_predict.hpp` |
| Other players run forward to now | the first game's later clients | `other-moves` bit, both games |
| The easy jump and the blast-jump helper | ours, on the first game's move | the kit; the sequel's air law too (`cl_speedjump`) |
| The landing dip | the sequel's fall kick | `engine/camera` (`cl_landdip`, 0 in the first game) |
| Any move on any map | ours | `sv_playermove`, one trace |

| Drawback left behind | How |
| --- | --- |
| Other players seen where the last 10 Hz frame had them | moves sent between frames and run forward: 10.6 units off a circling runner against 17.1 |
| A server's own move mispredicted by its clients | the feel told (`move-feel`): no corrections where a stock move corrects 2.9 units |
| Hard moves out of reach of new players | the aids, granted per player by an admin or a vote |

Not a drawback: a game's own numbers. A move told another game's numbers feels like that game only
with ALL of them, the landing pause too; the move is never "improved".

## Gaps

- The first game's move on a 38 map for the bots (M18v2c); the third game's move.

## Labs and tests

`pmove`, `move-strafe`, `move-trace`, `se-moves`, `se-hop`, `se-rj`, `se-remix-physics`,
`se-speedjump`, `se-sequel-speedjump`, `se-rjump`; `tests/classic_sequel_move_test.cpp`.

## Lessons

- A setting that replaces a constant keeps the constant's arithmetic, or the goldens move.
- The stair double jump lives in the air's slide, not the jump.
- A per-client feel only to clients that understand it (an extension bit), or stock clients
  mispredict.
