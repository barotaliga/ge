---
name: sim-system
description: "Use to add or change anything in the game's simulation (sim/): the state, the fixed step's order, a new system, inputs, the seeded randomness, and the tests that hold a run to the same bits every time."
---

# A simulation system

The game is decided in `sim/`. A renderer, a sound or a HUD reads a `GameState` and never
writes it, so everything that can be tested headless is tested headless.

## The shape

- **`GameState` is the whole run.** Everything a step reads or writes lives in it: positions,
  timers, each system's state, the `ge::core::Rng`. No statics, no globals; two states never
  share anything.
- **`step(state, input)` advances it by one fixed `kDt`.** Time is `steps * kDt`. Nothing in
  `sim/` reads a wall clock.
- **The step's order is stated once**, in `step`: each system is one call, in a fixed order. A
  new system is a struct in the state, a `create` that sets it up and a `step_<system>` called
  from `step` at its place.
- **Every draw comes from `state.rng`, in step order.** Adding, removing or reordering a draw
  changes every later number: say so in the PR, and expect the digests to move.
- **Inputs are a plain struct**, the same whether a player, a bot or an agent produced them,
  so a recorded input tape replays the run.

## Adding a system

1. Find whether the engine or a plugin already models it (skill: `engine`).
2. Write the test first, in `tests/sim_<system>_test.cpp`: stage the state, script the inputs
   step by step, and check the outcome. Check bits where bits matter (`CHECK_BITS`).
3. Add the state, the create, the step call. No allocation in the step: reserve at create.
4. Keep the replay test green: two runs of the same seed and inputs have the same `digest`.
   Fold the new state into `digest`.
5. Give it a lab when it has anything to look at or measure (skill: `labs`).

## Determinism

The rules are the engine's (skill: `engine`, § Determinism): `ge::core` linked, transcendentals
from `core/libm/libm.hpp`, randomness from the state's `Rng`, no clock. `make agents-check`
fails `sim/` on a `<cmath>` transcendental, `std::chrono`, `time(`, `rand(` or `<random>`.

A run that does not replay is found by bisecting the steps: compare the two runs' digests
every N steps, then step by step inside the first window that differs, then field by field.

## Checklist

- [ ] State in `GameState`, step order in `step`, draws from `state.rng`
- [ ] A test that stages the situation and checks the outcome
- [ ] The replay test green, the new state in `digest`
- [ ] A lab when there is something to see or measure
