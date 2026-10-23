---
name: paramotor
description: "Use for plugins/paramotor in gameengine2: the powered speed wing a skier flies on any snow-mountain game. The rig's numbers (spec.hpp), the step (step_para: the hands' lag, the engine's thrust, the wing on its lines by a polar with the brakes, the risers and the toggles, the swing damped, the lines pulling only, the stall and the surge, the fold and its refill, the pilot hung in the harness, the wing brought down onto the snow or a crown, the drop and the pieces falling; the acro: the dive, the swing-up, the loop, the slack lines and the tangle), the air it flies in (para_air_at: the wind aloft, ridge lift, the lee, the eddies), the bot's hands (para_pilot: the launch, a height held toward an aim, the glide in and the flare), the para scenarios and the `para` lab."
---

# The paramotor plugin

`plugins/paramotor/` (`ge_paramotor`, namespace `ge::paramotor`) is a motorised speed wing flown
by a skier: a small ram-air wing on short lines over a foot-launch motor on his back. The pilot
IS the skier (`skier::SkierState`): on the snow his skis are the skier's in every respect, and in
the air he hangs under the wing. It depends on the core, on `mountain` (the `Level` it flies over)
and on `skier` (the skier, his step, the trunks' index and the hand-built maps):
`ge_add_plugin(paramotor ENGINE core PLUGINS mountain skier)`. It never learns which game flies
it: where the ride begins, the press that drops the rig, the restart and the wind it is handed are
the game's (Fall Line's free ride: `games/fall_line/sim/para_ride.hpp`, begun on the highest run's
head, the press, the held skis, the restart; and `para_outing.hpp`, the bot's outing off the
summit onto a gentle stretch of a run and down on skis, both held by
`tests/fall_line_para_test.cpp` and the `free` lab's `--para` table; the `sim` skill owns them).

| File | What |
| --- | --- |
| `paramotor/spec.hpp` | Every number with its unit: the wing, the polar, the trim, the brakes, the risers, the lean, the turn, the swing's damping, the engine, the launch, what brings it down, the drop, the flare, the air, the fold, the map's edge |
| `paramotor/para.hpp/.cpp` | THE STEP, its order stated in the header: `ParaState`, `ParaControls`, `ParaEvents`, `ParaWorld`, `ready_para`, `step_para` (the hands, the engine, the wing on its lines, the fold, the hang, the wing brought down), `drop_para`, `para_hangs`, `para_hash` |
| `paramotor/acro.hpp/.cpp` | The acro `step_para` calls: `kept_way` (a wing met from behind collapses whole), `riser_rig` (the dive), `bite_rears`, `riser_lift`, `held_share`, `hold_pitch` (the rears bending the pair's path round and holding the wing over him), `count_swing` (the loops), `fallen_in` (the slack lines), `read_load` (the pull in g), `step_tangled` (falling in the bundle) |
| `paramotor/air.hpp/.cpp` | The air the wing flies in: `para_air_at` (the 10 m wind up the log law and the mountain's exposure, ridge lift and the lee at two scales, the eddies frozen into the wind), `eddy_up` (one tip's), `make_air_field` (the eddies dealt off the map's seed, once a map), `level_wind` (the map's weather as a wind) |
| `paramotor/pilot.hpp/.cpp` | The bot's hands: `para_pilot` to a `ParaAim` (the launch down the fall line on full throttle; the toggles to the aim; the throttle holding a height over the highest snow under and ahead of him, damped by how fast it closes; the glide in, the flare; a fold pumped out) |
| `paramotor/scenarios.hpp/.cpp` | The para scenarios (one table for the tests and the lab) and `run_para_scenario` |

## The model in one paragraph

Two bodies on one inextensible line, solved after the skier's own step. The wing is a 10 kg point
mass flying by a polar: its angle of attack is the air through it against the plane square to its
lines, plus the trim the brakes and the risers move, so its pitch is the pendulum's own and the
surge, the dive after a stall and the swing under a hard turn all fall out of it. Past the stall
its lift falls away and its drag jumps. The toggles put a side force on the canopy, righted by its
arc as it banks, so a toggle held is a steady banked turn. The engine pushes the pilot along his
skis on the snow and along the wing's way aloft, its thrust falling with the air through the
propeller. The lines only pull, and the swing about the pilot is damped against the air. The air
is the caller's wind grown up the log law, rising up a face (ridge lift), sinking and churning in
its lee, with eddies read at both tips: one tip pushed under its angle folds that side, the middle
pushed under folds the whole leading edge, and a fold refills by itself, faster pumped.

THE ACRO is the bar, forward and back. Forward pulls the fronts: a dive, faster the longer it is
held. Back pulls the rears, biting as hard as he was fast when he pulled: from a cruise they only
slow and lift the wing, but from a big dive they bend the pair's path up and hold the wing over
him on the inside of the curve, so it goes on round over him and down behind, a LOOP (the lines'
whole turn about the level pitch axis, so a spiral counts nothing). The rears' extra lift carries
no induced drag; that is the one liberty taken, because without it a soft wing cannot close a
loop. Pulled up without the speed to go over, the lines go slack at the top with the wing level
with him or below, and he falls into it: TANGLED, the motor run down, falling in the bundle until
the press cuts it away or the snow stops him (hard enough, the skier's own landing wipes him out).
A wing met by the air from behind cannot fly backwards and collapses whole; a wing pulled past
1.8 g is held full whatever its angle.

## Rules

- **State each answer once.** A number that shapes the flight goes in `spec.hpp` with its unit.
- **Deterministic.** `core/libm` for every transcendental, no allocation in `step_para`, no state
  outside `ParaState` (the eddies' field is built once a map and read-only). `para_hash` is the
  trajectory's digest.
- **The skier steps first.** The caller runs `skier::step_skier` (handing it no input while
  `para_hangs`), then `step_para`. The plugin moves the skier's body (the thrust, the lines, the
  hang) and never his snow.
- **The wind is the caller's.** `ParaWorld::wind` is the 10 m wind over open snow; still air has
  no lift, no lee and no eddy.
- **A figure is the hand's, never the bot's.** `para_pilot` never touches the bar; the acro is
  flown by script in the scenarios (`dive`, `swing-up`, `loop`, `tangle`, `cut-tangle`) and by
  the player. The figures are sensitive to the dive's length: a loop wants about 5 s of dive,
  and 2 s of dive swung up tangles him.
- **A cut is reported, never handled.** `ParaEvents::cut` says why the rig came off; the pieces
  fall and lie, and the game owns the restart.
- **Test on the hand-built maps.** A new situation is a row in `scenarios.cpp`'s table: a golden
  row and, if it has one right answer, an expectation in `tests/paramotor_scenarios_test.cpp`.
- **A model change moves the golden on purpose**: `make lab LAB=para ARGS=--tag=before`, change,
  `--compare=before`, rewrite `tests/golden/paramotor_scenarios.txt` with
  `GE_GOLDEN_WRITE=1 build/debug/tests/paramotor_scenarios_test`, and put the compare table in
  the PR.
- **Not here yet**: the acro (the dive, the swing up, the loop, the slack lines and the pilot
  fallen into his wing) is docs/goals.md's M13d3.

## Labs

`make lab LAB=para` (`--scenario=<id>` for one): every scenario's launch and take-off, top speed,
metres, the height climbed and the fastest climb, the mean airspeed and sink aloft over its second
half, the most bank, the turn, whether it stalled, its folds and the deepest, the touches and the
hardest landing, whether the skier went down, why the rig came off and when, and the bot's nearest
to its aim; one picture each, the line from above and the height, speed and folds under it.
