---
name: water
description: "Use when the game has water: a lake, the sea, a river, a brush world's volumes or water that spills (ge::water, which `ge add water` links and lays a lake with). Laying a body in one call, choosing a look by what is in the water, the one sample query, stepping, things that float and splash, a swimmer and his preset, flow regions (a breach, a flood, a waterfall), the wakes, the water's sound and muffling the world under it, and the lab that pictures it."
plugin: water
---

# Water on ge::water

`ge::water` (link `ge::water`; headers `water/*.hpp`) is water a game lays down in one call a
body. Everything anything asks of it goes through ONE query, `sample`, so the physics, a
swimmer, the sound and the picture cannot disagree about where the surface is. The waves are
the wind's (`ge::wind`), grown over each body's own fetch; a lake, a sea, a channel and a volume
are pure functions of the time, so a replay, a ghost and a peer read the same water with nothing
to step or send. Only flow regions and the game's own floating bodies and swimmers are stepped.

## Start: `ge add water`

```sh
ge add water            # sim/water.hpp, .cpp, labs/water_lab.cpp, tests/sim_water_test.cpp
ge lab water            # the lake from above (every pixel a sample) and in section
```

It links `ge::water` into the simulation and writes a lake you then move to your world's shore,
level and bed. A second name (`ge add water harbour`) writes a second set beside the first.

## Laying water

```cpp
#include "water/world.hpp"
namespace water = ge::water;

water::World world(wind_field);                                    // the air (its mean)
water::add_lake(world, shore_points, 412.0, {.look = "nordic-clear"}, bed);
water::add_ocean(world, 0.0);                                      // "adriatic", 1025 kg/m^3
water::add_river(world, centreline, 6.0);                          // m^3/s; points: level, width
water::add_volume(world, hulls, {.look = "slime"});                // convex hulls (brush water)
water::add_flow(world, water::make_flow(spec));                    // water that spills
```

- **The bed** (`water::Bed`, a function of x and z and a context) sets every depth `sample`
  reports and shallows the waves; with none a lake is `depth` deep and an ocean is deep.
- **The look is what is in the water.** Name a row of `water::looks()` (`nordic-brown`,
  `nordic-clear`, `baltic`, `adriatic`, `tropical-shallows`, `glacial`, `algal-pond`,
  `muddy-river`, `pool`, `slime`, `lava`), or state your own `water::Constituents` in
  `Stuff::water`. Every colour, the fog under the surface and the visibility are derived from
  it (`water/optics.hpp`: `reflectance`, `colour_of`, `optics`); never paint a colour by hand.
- **Density and temperature** are the body's (`Stuff`): the sea floats a body higher, cold water
  takes a swimmer's strength sooner.

## Each step, and any question

```cpp
water::step(world, kDt);                              // the flow regions; a lake costs nothing
water::Sample s = water::sample(world, x, y, z, t);   // t: steps * kDt, the run's own clock
// s.body (-1 dry), s.surface, s.nx/ny/nz, s.vx/vy/vz (waves + current), s.depth (under: > 0),
// s.bed, s.density, s.temperature, s.breaking (crest, shoal, wind), s.hs (the sea's height)
```

Hold the `World` beside the game state, built once from the seed: it is data, not run state, and
needs no digest of its own unless it has flow regions (then `water::volume(flow)` and the
cells are the game's to snapshot). The batch `sample` over a span of points costs one call for
a hull's probes or a crowd's feet.

## Floating, splashing, swimming

- **A floating thing** is a set of probes (`water/float.hpp`: spheres and slabs, a planing
  plate for a hull or a sled skipping): `step_floater` for a body that is only ever afloat, or
  `float_push` for one the game moves (it returns the force, the torque and the splashes).
  `splash_ring(world.wakes, body, splash, t)` sets the splash's ring on the surface; a hull's
  wake is a `Trail` (`water/wake.hpp`).
- **A swimmer** (`water/swim.hpp`): `step_swimmer(world, spec, input, state, t, dt)`, a wish
  direction and the jump. `default_swimmer()` is a person in a swimsuit; build a `SwimSpec` for
  your own (clothes that soak in `gear` and `garments`, boots that fill, or the kinematic regime
  of a classic first-person swim). The cold and the breath are in `SwimState`; the game decides
  what failure means.

## Moving water

A **flow region** (`water/flow.hpp`) is water on a grid: `make_flow({.nx, .nz, .cell, .x0,
.z0})`, `set_bed` for its walls and floor, `fill_to` for what it holds, its `sources` and
`drains` for what comes and goes, `open` edges for where it leaves. Break a wall with `set_bed`
and the next `water::step` empties the basin; a lip over `fall_drop` becomes a `Fall` (a
waterfall's sheet) that `sample` and the sound both read. A region's volume is held to the
quantum (`water::volume`): a regression that loses water is a test that fails.

## The sound

`water/sound.hpp` hears the same world: `make_water_bed(bus)` once, then each frame
`water_bed_update(bed, synth, world, ear_x, ear_y, ear_z, t, heading, dt)` steers the lapping,
the surf, a river's rush and babble, a fall's roar and spray, and the room under water. It
returns the `Hearing`; pass `hearing.under` to `muffle(...)` on your own beds and one-shots so
the whole world goes under together. `play_splash` takes a `Splash` from `float_push`,
`play_swim` a swimmer's `SwimEvents`.

## Labs and determinism

- `ge lab water` pictures the lake from above and across; add a view of your own water the
  same way (every pixel a `sample`), and a scenario lab for a floater or a swimmer (skill: labs).
- Everything is deterministic: the waves' phases are hashed off `World::seed` and the body's
  index, the transcendentals are `ge::core::libm`'s, and a flow region gives the same bits on
  any number of `workers`. Change the wind, a shore or a look and the lab's digest moves: rewrite
  its line with `ge digest --write` in the same change.
- `ge explain water/world.hpp` (or any header) reads its contract; the engine's
  `docs/water.md` is the design, with every model's source.
