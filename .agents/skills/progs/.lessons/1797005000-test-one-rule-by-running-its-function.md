---
title: Test one rule of the arena by running its named function, not by playing a match for it
date: 2026-11-21
scope: tests/shooters_edge_arena_quake_test.cpp, tests/shooters_edge_qc_fixture.hpp
concepts: [quake-rules, arena-rules, fixture, door-blocked, precache, movers, ledger]
---

A rule a bot match rarely meets (a crush, a telefrag onto a pentagram, a dropped quad taken up)
is held by running the rule's own function with `self` and `other` set (`pg::find_function` plus
`call_function` inside `enter_qc` / `leave_qc`), then reading the prints and fields. Three traps
met: the fixture's arena has no movers unless asked (`Fixture(..., movers = true)` loads the door
`*1` and plat `*2`; a brush entity on a missing model faults in the hull trace); a dropped power's
model must be precached by the map, so the test map places the item too; and `ClientKill` is a
suicide that respawns at once under Quake's rules, never a death that drops the quad, so kill
with a crush (`door_blocked` with a big `dmg`) when the death itself is under test.
