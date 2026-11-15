---
title: ge holds no game's code, so the game's own CLI plays a mod's scenario and ge reads its JSON
date: 2026-11-14
scope: engine/cli/mod.cpp, games/shooters_edge/cli/mod_run.cpp
concepts: [check-run, mod-run, runner, secli, replay, hook-cost]
---

`ge mod check --run` cannot link Shooter's Edge's match, so the game's CLI answers
`mod run <package> --json` and `ge` parses the last JSON line of its output (`kModGames` names the
CLI). `ge` finds it by `--runner`, then `GE_CLI_BUILD_DIR` (the build root, where `ge` and `secli`
sit side by side), then the PATH; the installed package has no `secli`, so the packaging workflow
passes `--runner=build/release`. Time a mod's cost as the best of several plain runs against the
best modded run, and call anything under 2% noise: one run each read as a negative cost under ASan.
