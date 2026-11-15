---
title: A meta-cvar sets values under the explicit ones; a set outside code is explicit
date: 2026-11-15
scope: engine/console/cvar.cpp, engine/console/cfg.cpp, engine/server/config.cpp, tests/console_script_test.cpp, engine/net/net_cvars.cpp
concepts: [meta-cvar, add-meta, implied, explicit, below, restore, latched, cfg-save, explain, golden, multi-line-print]
---

A meta-cvar's level never goes through `Cvars::set`: it writes members with `put`, so no server
history watch sees it and nothing marks it explicit. Every other set does go through `set`, which
records `below` (what a meta turned off restores) and marks the cvar explicit when the source is
not code. So code's own sets, such as a game's defaults, stay under the metas, and a cfg's set
beats a meta whichever ran first. Anything that reports where a value came from has to check
`implied_by` first: `--explain`, `help`, `cvarlist`, the `server-cfg` lab's picture, and
`cfg_save`, which writes `below` so a meta's value never turns into the player's own on the next
start. One more catch: `help` prints a text of several lines as one print. The
`console_script.txt` fixture runner now splits each print at its newlines. Without that, a
fixture that runs `help` writes a golden that its own reader can never match.
