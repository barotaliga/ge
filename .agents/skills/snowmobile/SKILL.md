---
name: snowmobile
description: "Use for plugins/snowmobile in gameengine2: the snowmobile any snow-mountain game can let a player ride. The machine's numbers (spec.hpp), the rigid body on eight sprung probes over the skier's snow (the belt's combined-slip grip, the paddles' cohesion, the carve, the roll and the tip, the yaw hold, the air and the nose set for a landing, the hull), the engine, CVT and belt (drive.hpp), the trunks, the map's edge and what throws the rider (step_sled), the bot's hands to an aim (sled_pilot), the sled scenarios and the `sled` lab."
---

# The snowmobile plugin

`plugins/snowmobile/` (`ge_snowmobile`, namespace `ge::snowmobile`) is a deep-snow mountain sled
and its rider as one rigid body on a generated (or hand-built) mountain's snow. It depends on the
core, on `mountain` (the `Level` it rides) and on `skier` (the snow's numbers it reads at its own
scale, the trunks' index): `ge_add_plugin(snowmobile ENGINE core PLUGINS mountain skier)`. It
never learns which game rides it: where the machine is parked, the press that takes a rider on
and off, what a throw does to the skier and the events a game says are the game's (Fall Line's
free ride is M13b2 in `docs/goals.md`).

| File | What |
| --- | --- |
| `snowmobile/spec.hpp` | Every number with its unit: the class, the suspension, the engine and the CVT, the grip, the snow's scale, the steer, the roll and the tip, the air, the hull, the crash; the probe table; `sled_mass`, `sled_inertia` |
| `snowmobile/sled.hpp/.cpp` | THE STEP, its order stated once in the header: `SledState`, `SledControls`, `SledEvents`, `SledWorld`, `step_sled` (the body, then the trunks, the edge and what throws the rider), `stand_sled`, `boards_at`, `sled_hash` |
| `snowmobile/body.hpp/.cpp` | The rigid body (`ride_body`): the probes' springs, the grip, the belt and the engine, the rider's weight, the air, the roll and the yaw on the snow, the levers in the air, Euler's equations, the hull's impulses; `derive_sled` |
| `snowmobile/drive.hpp/.cpp` | The engine's power curve, the CVT's hold, the belt as a mass of its own |
| `snowmobile/pilot.hpp/.cpp` | The bot's hands: `sled_pilot` to a `SledAim`, round the trunks |
| `snowmobile/scenarios.hpp/.cpp` | The sled scenarios (one table for the tests and the lab) and `run_sled_scenario` |

## The model in one paragraph

Eight probes (a ski each side, three stations down each edge of the belt) are spring-dampers
along the body's down axis against the snow's support: the ground less the sink the speed
allows, the skier's sink at the belt's 1.35 times and planing 1.7 times later, so a belt sits
some 30 cm into powder at rest and climbs out as it goes. The belt's grip is one budget spent
along its combined slip (a belt spinning under full throttle has little left to hold the tail),
the paddles shear the powder's cohesion, and a belt rolled onto its edge in powder carves toward
its low side. The engine pushes the belt as a mass of its own through a CVT that holds the
engine where the thumb asks. On the snow the roll is held toward the lean the bars ask (past the
TIP the righting fades and it goes over; in loose snow the rider balances the soft side) and the
arcade's hand holds the yaw to what the skis ask; in the air the rider's levers and the belt's
gyro pitch it and the nose is set for the landing. The hull meets the snow as impulses when the
springs run out. Nothing else on snow throws the rider but a trunk, a roll past 72 degrees held,
a landing too hard or too crooked, or the way stopped dead.

## Rules

- **State each answer once.** A number that shapes the feel goes in `spec.hpp` with its unit.
  The snow's own numbers are the skier's (`skier/tuning.hpp`): the sled reads them at its scale,
  never restates them.
- **Deterministic.** `core/libm` for every transcendental, no allocation in `step_sled`, no state
  outside `SledState`. `sled_hash` is the trajectory's digest.
- **The arcade's hands say so.** The yaw hold, the nose set for a landing and a held belt's static
  bite model nothing a real machine has; each is marked where it is applied, and each is a
  number in `spec.hpp` a game can read.
- **Test on the hand-built maps.** A scenario never reads the generator. A new situation is a row
  in `scenarios.cpp`'s table: a test (its golden row and, if it has one right answer, an
  expectation in `tests/snowmobile_scenarios_test.cpp`) and a lab run at once.
- **A model change moves the golden on purpose**: run `make lab LAB=sled ARGS=--tag=before`
  first, change, run `--compare=before`, rewrite `tests/golden/snowmobile_scenarios.txt` with
  `GE_GOLDEN_WRITE=1 build/debug/tests/snowmobile_scenarios_test`, and put the compare table and
  the pictures that moved in the PR.

## Labs

`make lab LAB=sled` (`--scenario=<id>` for one): every scenario's top and end speed, metres,
height climbed, the belt's sink and its most slip, the most it rolled, the circle it settled on,
when it stalled, its flights and its hardest landing, trunks met, whether and why the rider was
thrown, and the bot's nearest to its aim; one picture each, the line from above and the speed and
the sink under it.
