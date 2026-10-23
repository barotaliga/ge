---
title: A state change made between steps has its event wiped by the next step; read it at once
date: 2026-10-23
scope: games/fall_line/labs/gpu_hud_lab.cpp
concepts: [hud, news, events, set-wanted, lab, step]
---

The `hud` lab raised the wanted level with `set_wanted` between two steps, and the news never
showed WANTED: `step` clears `GameState::events` before it runs, so the event pushed outside it
was gone before `note_step` read the step's events. A lab or a developer key that changes the
state outside a step calls `note_step` right after the change, so the HUD hears what it did.
