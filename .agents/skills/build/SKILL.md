---
name: build
description: "Use for CMake, presets, the Makefile, compiler flags, adding a dependency, or a CI workflow in gameengine2. The determinism flags, FetchContent pinning, the CI matrix, and the public-repo rule for workflows."
---

# Build and CI

## Layout

- Root `CMakeLists.txt`: the project, `ge_fp` and `ge_isa` (the **determinism flags**, in
  `cmake/Fp.cmake`, which `cmake/Mods.cmake` includes too; linked by `ge_core` and published as
  `ge::fp`), `ge_flags` (warnings, `-Werror`, sanitizers;
  this tree only, never published), the modules, the plugins and the games, `add_subdirectory(tests)`
  when top-level, and `include(cmake/Packaging.cmake)` (the installed package, `release` skill).
- **An engine module builds itself**: `engine/<module>/module.cmake` defines its target (linking
  the layers below it, nothing above) and ends with one `ge_module(ge_<module> [EXPORT <name>]
  [SIMULATION] ...)` call (`cmake/Modules.cmake`). The root includes every such file, and the
  package, the tests, the labs, `tools/check-plugins.mjs` and `tools/check-libm.sh` read the
  registration, so a new module is a new directory plus its AGENTS.md row, and no shared list is
  edited (parallel pull requests adding modules do not conflict). A change to a module's,
  plugin's or game's own build file reaches only its directory's code in `ge test --affected`,
  `ge digest --affected` and precheck; a change to the root's, `cmake/`'s or `tests/`'s reaches
  everything.
- `CMakePresets.json`: `debug` (ASan + UBSan) and `release`. Ninja generator,
  `build/<preset>/`.
- `Makefile`: names the everyday calls. The logic lives in CMake.

## The session-start build

`tools/session-start.sh` is the cloud session's SessionStart hook (registered in
`.claude/settings.json`, the one Claude-specific config file; the script lives in `tools/` with the
other checks). It returns at once and leaves a detached worker that runs `tools/setup-ubuntu.sh --https-mirrors`
(the container's proxy passes HTTPS alone) and then `make build` into `build/debug/`. One worker at a time (`build/session-start.lock`); the log is
`build/session-start.log`, the state `build/session-start.status`; `--wait` blocks until it ends.
It does nothing outside a cloud session unless `GE_SESSION_BUILD=1`. 

## The Ubuntu packages

`tools/setup-ubuntu.sh` is the ONE list of Linux packages (`LIB_PKGS`, checked by dpkg, and
`TOOLS`, checked by the command on PATH and its version), for a fresh Ubuntu Server 24.04 or 22.04
(which gets Kitware's, the toolchain PPA's, apt.llvm.org's, NodeSource's and GitHub's sources), a
machine that has some of it, CI's Linux jobs and the session hook. It installs or upgrades only what
is missing or too old and is a sub-second no-op with no network when nothing is, so callers run it
unconditionally. CI passes `--cache-dir=.cache/apt` under an `actions/cache` keyed on the image, the
script's hash and the week. `--check` reports without installing; `--verify` builds and runs the
whole suite as CI's Linux x64 row. A new system package a build needs goes in `LIB_PKGS` (a library)
or `TOOLS` (a command and its minimum), and CI and the hook pick it up.

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
- **The instruction-set baseline** is `ge_isa` (`cmake/Fp.cmake`), linked by `ge_core` and
  published as `ge::isa`: on x86-64, `-march=x86-64-v3` (MSVC `/arch:AVX2`), so AVX2 is
  required; on ARM64, the compiler's default (NEON is baseline). It moves no bits because
  `ge_fp` keeps contraction off. Raising it is a decision for the maintainer: it drops CPUs.

## CI (`.github/workflows/ci.yml`)

Jobs: `gates` (clang-format, `tools/check-agents.sh`, libm, release-check, the changeset rule,
and the decision what else runs), `lint` (clang-tidy), `test` (macOS Clang; under `full-ci` also
Linux x64 GCC debug, GCC release and Clang release, and Linux arm64 GCC), `windows` (MSVC), `packaging`
(`packaging.yml`, called), and `required`, the ONE check the ruleset requires. `fail-fast:
false`, so one row's golden failure shows alongside the others.

- **Only what can differ across platforms**: Linux is `make precheck`'s, run in the container
  before every push, so a pull request runs `gates`, `windows` and the macOS row; the merge
  queue adds `packaging` (the package on Windows and macOS, `scaffold` only when the change
  reaches `engine/cli`, `engine/play`, `engine/lab`, the templates or the build); a push to main
  builds the macOS row only (to save its cache). A change to prose alone runs `gates` only.
  `full-ci` on a pull request adds the Linux rows, `lint` and the whole packaging. `required` passes when every job passed or was
  skipped by the rules, so a skipped row never leaves a check pending: a new job joins its
  `needs:`, never the ruleset. A failure only the queue sees ejects the pull request from the
  queue; fix it, push, and auto-merge queues it again.
- **Kept off the runners when it cannot change the answer** (`ci.yml`'s header): a newer push
  cancels a PR's older run; a draft runs nothing until it is marked ready; a label other than
  `full-ci` runs the gates alone, in its own concurrency group, so it never cancels the run going;
  each queue run's gates cancel the queue runs whose group ref is gone (the queue rebuilt), so
  nobody cancels stale queue runs by hand any more; the heavy jobs time out at about twice their
  usual time. GitHub cannot cap a job kind by count (a concurrency group cancels its pending
  job), so there is no cap on macOS or Windows jobs.
- **Kept fast**: ctest runs four cases at
  once (`CTEST_PARALLEL_LEVEL`; `make test` uses every core), the labs' smoke pass is eight
  CTest cases (`labs_smoke_1`..`_8`, `labs --all --smoke --shard=I/N`) and the slow cases carry
  a `COST` so they start first. The Linux and macOS rows compile through ccache, restored on
  every run and saved only from main (a pull request never writes the cache). On a pull
  request `lint` checks only what the change reaches (`tools/lint.sh BUILD BASE`: each changed
  `.cpp`, and each `.cpp` whose object includes a changed header, read off Ninja's deps); a
  change to `.clang-tidy`, `tools/lint.sh` or the CMake lints everything (main is never linted
  again after the queue). A new slow test: give it a `COST`; never cut a row to save time without
  saying what it covered and where that coverage went.

- **Public repo**: workflows use `permissions:` scoped to the minimum (`contents: read`
  by default). Credentials are only `${{ secrets.NAME }}`. Never echo a secret, never
  put one in `env:` at workflow level, and never take secrets in a `pull_request_target`
  job that checks out PR code.
- Pin third-party actions to a major version tag from a trusted publisher. Prefer
  `actions/*`.
- A new OS or compiler row is a promise of the same bits there too.
