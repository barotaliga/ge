---
type: Added
title: ge precheck and ge ci
---

`ge precheck [--preset=<p>]` runs a game repo's CI gates here, in CI's order, for both presets (or the one named): `ge check`, then the pilot over seeds 1, 2 and 3 and every input log it wrote replayed to its digest; the scaffold's `ci.yml` now runs exactly `ge precheck --preset=<p>`, so a green precheck is a green job. `ge ci [<pr>] [--log=<file>]` lists the checks on a pull request (or the checkout's commit) through `gh api` and reads each failed job's log back as findings: ge's own error lines first, then compiler, CMake, test and lab failures, then the log's last words; `--log` reads a saved log with no network.
