---
name: helicopter
description: "Use for plugins/helicopter in gameengine2: the light helicopter any snow-mountain game can fly. The machine's numbers (spec.hpp), the step (step_heli: the rotor's thrust by momentum theory with ground effect, the disc tilted by the cyclic and the fuselage hung under it, the pedals against the torque, the lever's lag, the skids' landing, the strikes of the rotor's tips, the body and a crown, the map's edge), the downwash (wash_at), the bot's hands to an aim (heli_pilot, heli_hands), the heli scenarios and the `heli` lab."
---

# The helicopter plugin

`plugins/helicopter/` (`ge_helicopter`, namespace `ge::helicopter`) is a light single-rotor
helicopter on a generated (or hand-built) mountain. It depends on the core, on `mountain` (the
`Level` it flies over and lands on) and on `skier` (the trunks' index and the hand-built maps):
`ge_add_plugin(helicopter ENGINE core PLUGINS mountain skier)`. It never learns which game flies
it: where the machine stands, the press that takes a rider on and off, what a crash does to the
people in it, the wreck and the restart are the game's (Fall Line's free ride:
`games/fall_line/sim/heli_ride.hpp`, the pad, the press, the drop, flown home, the wreck; and
`heli_outing.hpp`, the bot's outing on it, both held by `tests/fall_line_heli_test.cpp` and the
`free` lab's `--heli` table; the `sim` skill owns them).

| File | What |
| --- | --- |
| `helicopter/spec.hpp` | Every number with its unit: the class, the rotor and the tail rotor, the body's strike points, the skids, the power, the blades, the drag, the flight's gains, the bot's, the crash, the map's edge, the wash |
| `helicopter/heli.hpp/.cpp` | THE STEP, its order stated in the header: `HeliState`, `HeliControls`, `HeliEvents`, `HeliWorld`, `HeliLoad`, `thrust_most`, `step_heli` (the spool, the lever's lag, the flight, the skids, the strikes), `stand_heli`, `heli_turned_back` (the edge's air), `heli_hash` |
| `helicopter/pilot.hpp/.cpp` | The bot's hands: `heli_pilot` to a `HeliAim` (the way, slowed for a rise ahead it cannot climb at speed and for the sink down to the aim; the height over the snow it will be over in the next seconds; the landing's settle), `heli_hands` (a wanted velocity, turn and climb as the four controls) |
| `helicopter/wash.hpp/.cpp` | The downwash: the column under the disc and the wall jet along the snow (`wash_at`) for a game's snow, spray and people |
| `helicopter/scenarios.hpp/.cpp` | The heli scenarios (one table for the tests and the lab) and `run_heli_scenario` |

## The model in one paragraph

The rotor's most thrust is momentum theory's (Glauert's induced flow at the speed through the
disc, the power less the parasite drag's, capped by the blades' own lift), raised by ground
effect near the snow; the lever asks a share of it through a lag. The thrust is along the disc,
which the cyclic tilts against its damping and its flapback, and the fuselage hangs under the
disc as a damped pendulum, rolled toward a load sat off its centre. The pedals turn it against
the main rotor's torque (more lever, more turn to the left) and the fin's weathervane. On the
snow the skids judge a landing: too fast down, too fast across, too tilted or on too steep a
face is a crash. In the air the rotor's tips, the body's strike points and the trees' crowns are
tested every step. Near the map's edge an air band turns it back toward the middle.

## Rules

- **State each answer once.** A number that shapes the flight goes in `spec.hpp` with its unit.
- **Deterministic.** `core/libm` for every transcendental, no allocation in `step_heli`, no state
  outside `HeliState`. `heli_hash` is the trajectory's digest.
- **The wind is the caller's.** `HeliWorld::air_x/air_z` is the air it flies through; the plugin
  models no weather.
- **A crash is reported, never handled.** `HeliEvents::crash` says why; the machine is left where
  it came down and the game owns the wreck, the fire and the people.
- **Test on the hand-built maps.** A new situation is a row in `scenarios.cpp`'s table: a golden
  row and, if it has one right answer, an expectation in `tests/helicopter_scenarios_test.cpp`.
- **A model change moves the golden on purpose**: `make lab LAB=heli ARGS=--tag=before`, change,
  `--compare=before`, rewrite `tests/golden/helicopter_scenarios.txt` with
  `GE_GOLDEN_WRITE=1 build/debug/tests/helicopter_scenarios_test`, and put the compare table in
  the PR.

## Labs

`make lab LAB=heli` (`--scenario=<id>` for one): every scenario's top speed, metres, the height
climbed and the fastest climb, when it lifted off, how far the torque turned it, the most roll
and disc pitch, its landings and the hardest, whether and why it crashed and how fast it fell,
the bot's nearest to its aim and the least height over the snow; one picture each, the line from
above and the height and speed under it.
