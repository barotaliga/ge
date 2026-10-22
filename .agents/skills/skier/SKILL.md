---
name: skier
description: "Use for plugins/skier in gameengine2: the skier on the snow any skiing game links. The snow under a pair of skis (sink, base friction, plough, grip, brake), what a skier can do (limits: edge, carve curvature, corner and terminal speed), one step of the skier (step_skier: the contact impulse, the carve and the pivot, the tuck and the push, the jump, flight, landing, trunks, wipeout), the hand-built maps and the ride scenarios, and the `ride` lab."
---

# The skier plugin

`plugins/skier/` (`ge_skier`, namespace `ge::skier`) is a skier and his pair of skis on a
generated (or hand-built) mountain's snow. It depends on the core and on `mountain`, whose
`Level` it skis (`ge_add_plugin(skier ENGINE core PLUGINS mountain)`); it never learns which
game rides it, so the modes, the course's rules, the bot and the field stay in the game.

| File | What |
| --- | --- |
| `skier/skier.hpp/.cpp` | THE MODEL, its order stated once in the header: `SkierState`, `SkierInput`, `SkierEvents`, `World`, `step_skier`, `stand_skier`, `skier_hash` |
| `skier/snow.hpp` | The snow under the skis by its packed share: `sink_target`, `base_friction`, `powder_drag`, `side_grip`, `brake_grip` |
| `skier/limits.hpp` | What a skier can do: `edge_most`, `carve_curvature`, `edge_for_curvature`, `corner_speed`, `carve_speed`, `terminal_speed`, `top_speed` |
| `skier/skis.hpp` | `SkiSpec` and `kReferenceSkis`, the pair every shared number was tuned on |
| `skier/tuning.hpp` | Every shared number, with its unit |
| `skier/trees.hpp/.cpp` | The trunks on a grid once a map: `build_tree_index`, `tree_overlap` |
| `skier/synthetic.hpp/.cpp` | The hand-built maps: `synthetic_slope()`, `flat_strip(pitch, packed)` |
| `skier/scenarios.hpp/.cpp` | The ride scenarios (one table for the tests and the lab) and `run_scenario` |

Its app half, `plugins/skier/app/` (`ge_skier_app`, no GPU), is the skier as seen and heard:
`figure.hpp` (the placeholder figure), the beds (`wind_bed.hpp`, `snow_bed.hpp`, `ride_bed.hpp`,
the `audio` skill) and `tracks.hpp`: the PRINTS a skier's skis lay each frame, two capsules a
stance apart from the pen (where they last touched) to where they touch now; the outside ski of a
turn deeper by `ski_load`, a carve narrower, a skid as wide as the ski's footprint across the
travel and shallower with a berm, a scuff on the piste and a hand's furrow in loose snow; none in
the air, none for a reset, one wide gouge for a body down. The game hands them to its trail map
(`mountain/app/trails.hpp`, the `mountain` skill); the `trails` lab measures the two together.

## The model in one paragraph

A body of `SkiSpec::mass`, integrated semi-implicitly at the caller's step. On the snow a
VELOCITY-LEVEL CONTACT takes the velocity into the snow out, and that normal impulse is the
currency every friction is paid from: across the skis the edge's grip, along them the base,
the brake and the powder. So a sideways load can never exceed what the snow holds, and a skid
costs speed because the slide it takes out is energy lost. The skis yaw at the carve's rate
(the speed times `tan(edge) / sidecut`) held to the grip, plus the pivot, plus their tracking
of their own way. A flight is the ground curving away faster than gravity follows; a landing
is the support met again, its load `1 + v_n^2 / (2 stroke g)`. There is no mode.

## Rules

- **State each answer once.** A pilot (the game's bot) plans on `limits.hpp`, never on a number
  of its own; the physics reads the same functions. A number that shapes the feel goes in
  `tuning.hpp` with its unit; a pair's own goes in `SkiSpec`.
- **Deterministic.** `core/libm` for every transcendental, no allocation in `step_skier`, no
  state outside `SkierState`. `skier_hash` is the trajectory's digest.
- **Test on the hand-built maps.** A scenario never reads the generator, so the plugin's tests
  pass with it deleted. A new situation is a row in `scenarios.cpp`'s table: it is a test (its
  golden row and, if it has one right answer, an expectation in
  `tests/skier_scenarios_test.cpp`) and a lab run at once.
- **A model change moves the golden on purpose**: run `make lab LAB=ride ARGS=--tag=before`
  first, change, run `--compare=before`, rewrite `tests/golden/skier_scenarios.txt` with
  `GE_GOLDEN_WRITE=1 build/debug/tests/skier_scenarios_test`, and put the compare table and
  the pictures that moved in the PR. A game's `sim` digests move with it (the `sim` skill).

## Labs

`ride` (scenario): every scenario run and measured (top speed against the terminal, distance,
the hardest sideways load and the tightest path, flights and landings, the stop, the wipeout),
with one picture each: the line from above coloured by what he was doing, and his speed.
`make lab LAB=ride ARGS=--scenario=kicker` runs one. The `labs` skill owns how to read it.

## Still to come

What the game does not ask for yet stays out (G4): the catalog of pairs, the wind, crust and
ice, the poles and the legs as stations, the tricks, the ragdoll and the body. Each lands with
the milestone row that needs it.
