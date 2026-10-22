---
name: engine
description: "Use when the game uses the engine: the pin in cmake/engine.cmake, the targets to link (ge::core, ge::lab, ge::<plugin>), how headers are spelled, the determinism contract it carries, the output channel, the ge command line, upgrading to a new release, and reporting an engine bug upstream."
---

# The engine

The game is built on one release of the engine, pinned in `cmake/engine.cmake`
(`GE_ENGINE_VERSION`, and `GE_ENGINE_SHA256` for the download's checksum). CMake fetches that
release's source and builds it with the game, so the libraries, the headers and the `ge` command
line all come from the same release.

## What it gives

| Target | What | Headers |
| --- | --- | --- |
| `ge::core` | The deterministic pool: the PRNG, math, the libm, noise, the heightfield, quaternions, the sun, the run clock, the output channel, `parallel.hpp` (a bake's rows on threads); carries `ge::fp` and `ge::isa` | `core/*.hpp`, `loop/*.hpp` |
| `ge::lab` | The lab contract and runner (skill: `labs`) | `lab/*.hpp` |
| `ge::<plugin>` | A genre's shared code (for example `ge::racing`, `ge::mountain`, `ge::skier`) | `<plugin>/*.hpp` |

Headers are spelled as the engine's tree lays them out: `#include "core/prng.hpp"`,
`#include "racing/tape.hpp"`. Each file's leading comment says what it owns and carries `ge:`
tags, so `grep -rn "ge:keywords.*<word>" build/debug/_deps/gameengine2-src/` finds the API for a
concept. Read a header's comments before calling it.

## Determinism

The engine's simulation code computes the same bits on every compiler and OS, and so must the
game's `sim/`:

- **Link `ge::core`**: it carries `ge::fp` (no FMA contraction, no fast-math) to every target,
  and `ge::isa` (on x86-64 the game requires AVX2, as the engine does).
  Never add `-ffast-math`, `-Ofast`, `-ffp-contract=fast` or `/fp:fast`.
- **Threads only in a bake** whose every output is a pure function of its index
  (`core/parallel.hpp`'s `parallel_bands`), never in a step.
- **Transcendentals from `core/libm/libm.hpp`** (`ge::core::libm::sin`, `pow`, ...), never
  `<cmath>`'s, whose bits differ between platforms. Exact functions (`sqrt`, `floor`, `fabs`,
  `fmod`) are fine.
- **Randomness from `ge::core::Rng`** in the state (`core/prng.hpp`). Time is the step count.
- **The output channel** (`core/output.hpp`) is how the game prints; the host attaches a sink.

`ge doctor` holds all four in `sim/` and `labs/`.

## The ge command line

`ge` is how the repo is built, tested, grown and checked (skill: `ge`). The pinned release
builds its own `ge` into `build/<preset>/ge`; a `ge` from another release run here hands the
command to that one, so the version always matches the pin.

| ge | What |
| --- | --- |
| `ge use <plugin>` | Link one of the engine's plugins (`racing`, `mountain`, `skier`) and install its skill |
| `ge find <words>`, `ge explain <x>` | The engine's API, read from the pinned release's own `ge:` tags and header comments |
| `ge doctor` | The repo against the engine's rules; every finding says what, where and next |
| `ge skills sync` | The engine's skills, rewritten to the pinned release; the repo's own skills and every lesson untouched |

An engine skill (one listed in the `ge:skills` block of `AGENTS.md`) is rewritten by
`ge skills sync`. Never edit one in place: write a lesson in its `.lessons/` (skill:
`skill-reflection`). `ge doctor` reports an edited one.

## Upgrade

1. Read the release notes (`CHANGELOG.md` in the engine) between the pinned version and the new
   one. While the engine is on `0.x`, a minor release may break the API.
2. Run the labs you care about with `--tag=before` on the old pin.
3. Set `GE_ENGINE_VERSION` to the new release, and `GE_ENGINE_SHA256` to the `-src.tar.gz` line
   of its `SHA256SUMS`.
4. `ge build` (it fetches the new release and builds its `ge`), then `ge skills sync` (the new
   release's skills).
5. `ge check`, and every lab again with `--compare=before`. A digest that moved is a behaviour
   the release changed: find it in the notes before you accept it.
6. One commit: the pin, the synced skills, any fixes, and the lab comparison in the PR.

## An engine bug

Never patch `build/*/_deps/`: the next build overwrites it. Reduce it to the smallest repro (a
test or a lab with a seed), and open an issue on the engine's repository with the release, the
repro and what it printed. Work around it in the game, with a comment naming the issue, until a
release fixes it.

To work on the engine and the game at once, point the build at a local engine checkout:
`ge build --engine=<path>` (the preset keeps it; `ge find` and `ge explain` read it too).
