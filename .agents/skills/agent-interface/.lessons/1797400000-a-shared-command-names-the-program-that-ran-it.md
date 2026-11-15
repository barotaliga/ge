---
title: A command shared by ge and a game's CLI names the program that ran it
date: 2026-11-14
scope: engine/toolkit/, engine/gamecli/toolkit.cpp, engine/cli/main.cpp, tests/toolkit_pak_test.cpp
concepts: [toolkit, gamecli, secli, pak, next-step, usage, catalog]
---

A command body moved into `engine/toolkit/` is listed by both `ge help` and `secli help`, so
every string it writes (the `next` step, the usage refusal, the help example) is built from
`Call::program`, never a literal `ge `. Grep the moved file for `"ge ` before building: the pak
group had a dozen. A row's `example` is stored without the program; `ge`'s catalog prefixes
`ge `, the game CLI its own name. gamecli's `json` flag is the frame's, so the bridge leaves it
out of the `Call` it builds, and the frame emits the row's `Outcome` itself. A helper the moved
body used from `ge::cli` (here `split_lines`) moves down with it, and `cli/finding.hpp` keeps the
old name through a using-declaration, so no `ge` file changes.
