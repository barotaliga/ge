---
name: build
description: "Use for CMake, presets, the Makefile, compiler flags, adding a dependency, or a CI workflow in gameengine2. The determinism flags, FetchContent pinning, the CI matrix, and the public-repo rule for workflows."
---

# Build and CI

## Layout

- Root `CMakeLists.txt`: the project, `ge_fp` (the **determinism flags**, linked by
  `ge_core` and published as `ge::fp`), `ge_flags` (warnings, `-Werror`, sanitizers;
  this tree only, never published), `ge_core`, `add_subdirectory(tests)` when
  top-level, and `include(cmake/Packaging.cmake)` (the installed package, `release` skill). Each
  later layer adds a library target (`ge_platform`, `ge_gpu`, …) linking the layers
  below it, and nothing above it.
- `CMakePresets.json`: `debug` (ASan + UBSan) and `release`. Ninja generator,
  `build/<preset>/`.
- `Makefile`: names the everyday calls. The logic lives in CMake.

## Adding a dependency

Only when a milestone needs it (G4). Use `FetchContent_Declare` with `GIT_TAG` set to
a **release tag or full commit hash**, never a branch. Prefer a prebuilt release
archive with `URL_HASH SHA256=…` for heavy dependencies (wgpu-native) so CI does not
spend minutes compiling them. Record why it was chosen in `porting/research.md`. Each
dependency is linked only by the layer that owns it: SDL only by `ge_platform`, and
webgpu only by `ge_gpu`.

## Flags

- `ge_fp` (determinism, travels to consumers) and `ge_flags` (hygiene, this tree only)
  are the only places compile options are set. Never add a flag on one
  target to work around something; fix the code.
- Never add `-ffast-math`, `-Ofast` or `-march=native` (they break cross-platform bits; see the
  `determinism` skill).

## CI (`.github/workflows/ci.yml`)

Jobs: `fmt` (clang-format), `agents` (`tools/check-agents.sh`), `lint` (clang-tidy),
`test` (Linux x64 GCC debug, GCC release and Clang release; Linux arm64 GCC; macOS
Clang), and `windows` (MSVC). `fail-fast: false`, so one row's golden failure shows
alongside the others.

- **Kept fast**: ctest runs four cases at
  once (`CTEST_PARALLEL_LEVEL`; `make test` uses every core), the labs' smoke pass is eight
  CTest cases (`labs_smoke_1`..`_8`, `labs --all --smoke --shard=I/N`) and the slow cases carry
  a `COST` so they start first. The Linux and macOS rows compile through ccache, restored on
  every run and saved only from main (a pull request never writes the cache). On a pull
  request `lint` checks only what the change reaches (`tools/lint.sh BUILD BASE`: each changed
  `.cpp`, and each `.cpp` whose object includes a changed header, read off Ninja's deps); a
  change to `.clang-tidy`, `tools/lint.sh` or the CMake lints everything, and so does every
  push to main. A new slow test: give it a `COST`; never cut a row to save time without
  saying what it covered and where that coverage went.

- **Public repo**: workflows use `permissions:` scoped to the minimum (`contents: read`
  by default). Credentials are only `${{ secrets.NAME }}`. Never echo a secret, never
  put one in `env:` at workflow level, and never take secrets in a `pull_request_target`
  job that checks out PR code.
- Pin third-party actions to a major version tag from a trusted publisher. Prefer
  `actions/*`.
- A new OS or compiler row is a promise of the same bits there too.
