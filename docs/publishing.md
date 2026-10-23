# Publishing

How the engine is released and how a game consumes a release. GitHub is the
only host: every release is a **GitHub Release**, mirrored to **GitHub
Packages** as an OCI artifact. Agents cut releases; nobody needs to.

## What a release is

A release `vX.Y.Z` is one annotated git tag on `main` plus these files, the
same set in both places:

| File | What it is |
| --- | --- |
| `gameengine2-X.Y.Z-linux-x64.tar.gz` | the installed CMake package, built on Linux |
| `gameengine2-X.Y.Z-macos-arm64.tar.gz` | the same, built on macOS (Apple silicon) |
| `gameengine2-X.Y.Z-windows-x64.tar.gz` | the same, built with MSVC |
| `gameengine2-X.Y.Z-src.tar.gz` | `git archive` of the tag |
| `SHA256SUMS` | the checksums of every archive above |

Each package archive holds one directory, `gameengine2-X.Y.Z-<platform>/`,
with `include/gameengine2/` (the headers, laid out as `engine/` is),
`lib/` (the static libraries, built in Release), `lib/cmake/gameengine2/`
(the package config, its version file and the exported targets) and `bin/ge`
(the command line game repos are scaffolded and checked with).
Every archive has a signed build-provenance attestation.

- **GitHub Release:** `https://github.com/barotaliga/ge/releases/tag/vX.Y.Z`
- **GitHub Packages:** `ghcr.io/barotaliga/ge:X.Y.Z` (and `:latest`), an
  OCI artifact (`application/vnd.gameengine2.release.v1`) whose layers are the files above.

## Consuming a release

**A new game repo** is one command, with the `ge` of the release it should pin
(from a package's `bin/`, or `make ge ARGS="new <dir>"` in this tree):

```sh
ge new snow-racer --engine-sha256=<the line for -src.tar.gz in SHA256SUMS>
```

It writes a repo that fetches that release's source (the FetchContent form below), builds its
own `ge`, and passes its own `make test` and `make agents-check` (`ge doctor`) from the start
(the `agent-interface` skill).

The CMake package is `gameengine2`; its targets are `ge::core` (the
deterministic pool, the libm and the run clock), `ge::audio` (the synth), `ge::lab` (the lab
contract and runner a game's labs plug into), one `ge::<plugin>` per plugin
(`ge::racing`: the control tape, records and standings; `ge::mountain`: the generated
ski mountain; `ge::skier`: the skier on the snow; `docs/architecture.md` § Plugins), a
`ge::<plugin>_app` for a presentation half whose engine targets are all published
(`ge::skier_app`: the skier's wind and snow beds, over `ge::audio`) and `ge::fp` (the floating-point determinism flags, which `ge::core`
carries to the consumer so a game computes what the engine's tests did), and `ge::isa` (the
instruction-set baseline, AVX2 on x86-64, which `ge::core` carries too).

**From source, pinned to a tag** (what a game in CI should do — no install
step, no credentials, the repository is public):

```cmake
include(FetchContent)
FetchContent_Declare(gameengine2
  URL https://github.com/barotaliga/ge/releases/download/vX.Y.Z/gameengine2-X.Y.Z-src.tar.gz
  URL_HASH SHA256=<the line for -src.tar.gz in SHA256SUMS>)
FetchContent_MakeAvailable(gameengine2)
target_link_libraries(game PRIVATE ge::core)
```

**From an installed package:**

```sh
gh release download vX.Y.Z -R barotaliga/ge -p '*-linux-x64.tar.gz' -p SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
gh attestation verify gameengine2-X.Y.Z-linux-x64.tar.gz -R barotaliga/ge
tar -xzf gameengine2-X.Y.Z-linux-x64.tar.gz
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/gameengine2-X.Y.Z-linux-x64"
```

```cmake
find_package(gameengine2 X.Y REQUIRED)
target_link_libraries(game PRIVATE ge::core)
```

**From GitHub Packages** (same files, any OCI client):

```sh
oras pull ghcr.io/barotaliga/ge:X.Y.Z
```

The packages hold Release builds only. With MSVC, a Debug game cannot link a
Release library (the runtimes differ): build the game in Release against a
package, or take the engine from source with FetchContent.

While the engine is on `0.x`, `find_package(gameengine2 0.N)` accepts only
`0.N.*` — a minor bump may break. From `1.0` on, any later `1.*` satisfies `1.N`.

## Recording a change

Every PR that changes what a release ships (`engine/`, `cmake/`,
`CMakeLists.txt`) adds a fragment to `.changes/unreleased/`, or carries the
`no-changelog` label when no consumer could notice. The `packaging`
workflow's `changeset` job holds it. Format: `.changes/unreleased/README.md`.
`CHANGELOG.md` and `VERSION` are written only by the release workflow —
never by hand.

```sh
python3 tools/release.py check   # every fragment valid
python3 tools/release.py bump    # the version the next release would get
```

## Cutting a release

Dispatch `.github/workflows/release.yml` on `main`. An agent does it through
the GitHub API (the GitHub MCP server's `actions_run_trigger` with
`method: run_workflow`, `workflow_id: release.yml`, `ref: main`, and
`inputs: {"bump": "auto", "dry_run": "false"}`), or with
`gh workflow run release.yml -f bump=auto`. The `release` skill
(`.agents/skills/release/SKILL.md`) is the checklist.

The run:

1. **plan** — reads the fragments, works out `X.Y.Z` (`tools/release.py bump`),
   refuses a tag that exists.
2. **package** — on Linux, macOS and Windows: writes `VERSION`, builds,
   runs the tests, installs, builds `tests/package/consumer` against the
   installed package (every installed header included in one translation
   unit), archives the install.
3. **publish** — only when every package passed: collates `CHANGELOG.md`,
   commits `VERSION` + `CHANGELOG.md`, tags `vX.Y.Z`, pushes (refused if
   `main` moved since the plan), then the GitHub Release and the attestation.
4. **mirror** — `.github/workflows/ghcr.yml` downloads the release's files,
   checks them against `SHA256SUMS` and pushes them to ghcr.io.

A run that fails in plan or package leaves no tag and no release: fix the
cause and dispatch again. One that fails inside publish is finished by
re-running its failed jobs — every publish step reuses what an earlier
attempt pushed. A release whose ghcr.io copy is missing or stale is mirrored
again by dispatching `ghcr.yml` with its tag (`tag: vX.Y.Z`); that works for
any past release, from the current workflow file.
`dry_run: true` runs plan and package only — the way to prove a release
would succeed without publishing anything.

## Credentials — this repository is public

Everything in this repository, its workflow files, its Actions variables and
its workflow logs can be read by anyone. **No token, key, password or other
sensitive value is ever committed, put in an Actions variable, or echoed in a
log.** Credentials live only in **GitHub repository secrets** and reach a
workflow only as `${{ secrets.NAME }}` mapped into an `env:` entry (never
interpolated into a `run:` script, never printed).

| Credential | Where | Used for |
| --- | --- | --- |
| `GITHUB_TOKEN` | built in, per run | the release, the attestation, the ghcr.io push, the commit and tag push |
| `RELEASE_TOKEN` | optional repository secret, a fine-grained PAT with Contents: read and write on this repository | replaces `GITHUB_TOKEN` for the commit and tag push only — needed once `main` is protected against Actions, or if the tag push should trigger other workflows |

A release needs no secret today. To add `RELEASE_TOKEN`, a maintainer enters
it under *Settings ▸ Secrets and variables ▸ Actions ▸ New repository
secret*, or runs `gh secret set RELEASE_TOKEN -R barotaliga/ge`
and pastes the value at the prompt (never as an argument, which lands in
shell history).

The first push to ghcr.io creates the package linked to this repository
(`org.opencontainers.image.source`). If it comes up private, a maintainer
makes it public once under the package's *Package settings*; later releases
keep that.
