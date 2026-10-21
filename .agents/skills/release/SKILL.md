---
name: release
description: Cut, dry-run or verify a release of the engine (GitHub Release + GitHub Packages), and record a change as a changeset fragment. Load before touching .changes/, VERSION, CHANGELOG.md, cmake/Packaging.cmake or the release/packaging workflows.
---

# Release

The procedure behind `docs/publishing.md` (read it first: what a release is,
how a game consumes it, which credentials exist).

## Record a change (every PR that ships something)

1. A PR touching `engine/`, `cmake/` or `CMakeLists.txt` adds
   `.changes/unreleased/<unix-ts>-<slug>.md` (`date +%s` for the timestamp).
   Format: `.changes/unreleased/README.md`. Write the body for a GAME that
   consumes the engine, one sentence. A change no consumer can notice
   (internal refactor, tests, docs) carries the `no-changelog` label instead.
2. `breaking: true` when a consumer's code or results must change: an API
   renamed or removed, a header moved, a determinism digest moved.
3. `python3 tools/release.py check` must pass.
4. Never edit `CHANGELOG.md` or `VERSION` — the release workflow writes both.

## A new public target or header directory

- A target a game links: add it to `GE_EXPORTED_TARGETS` in
  `cmake/Packaging.cmake` (target, export name). Its include directories must
  be `$<BUILD_INTERFACE:...>`; Packaging adds the install side for `ge_core`.
- What a consumer inherits: `ge_fp` (the determinism flags) travels with
  `ge_core`; `ge_flags` (warnings, `-Werror`, sanitizers) is linked only
  `$<BUILD_INTERFACE:...>` and only when this tree is the top-level project,
  so neither an installed package nor a FetchContent consumer gets it.
- Headers: anything under `engine/` matching `*.hpp|*.h|*.inl` installs
  automatically. `tests/package/consumer` includes every installed header in
  one translation unit, so a header that does not stand alone fails CI.

## Cut a release

1. `main` is green and `python3 tools/release.py bump` prints the version you
   expect. No fragments → nothing to release; stop.
2. Prove it first: dispatch with `dry_run: true`. GitHub MCP:
   `actions_run_trigger` `{method: "run_workflow", owner: "niclaslindstedt",
   repo: "gameengine2", workflow_id: "release.yml", ref: "main",
   inputs: {bump: "auto", dry_run: "true"}}`.
3. Green → dispatch again with `dry_run: "false"`.
4. Verify: `get_latest_release` shows `vX.Y.Z` with 3 platform archives, the
   `-src` archive and `SHA256SUMS`; the run summary lists the ghcr.io ref.
5. A run that failed in `plan` or `package` left nothing behind: fix forward
   on `main` and dispatch again. One that failed inside `publish` (after the
   tag push) is finished by re-running its failed jobs (`actions_run_trigger`
   `method: rerun_failed_jobs`): every publish step reuses what an earlier
   attempt pushed. Never delete a pushed tag. A re-run uses the workflow file
   of the original run, so a fix to the workflow itself does not reach it.
6. The ghcr.io copy failed or is missing: dispatch `ghcr.yml` on `main` with
   `inputs: {tag: "vX.Y.Z", latest: "true"}`. It mirrors the GitHub Release's
   own files, so it works for any past release with the current workflow.

## Secrets (public repository)

Nothing sensitive is ever written to the tree, an Actions variable, a
workflow file or a log. A release needs only the built-in `GITHUB_TOKEN`.
`RELEASE_TOKEN` (optional repository secret) is only for pushing to a
protected `main`. An agent never handles a token's value: a maintainer sets
secrets in the repository settings (the agent proxy refuses the secrets API).
