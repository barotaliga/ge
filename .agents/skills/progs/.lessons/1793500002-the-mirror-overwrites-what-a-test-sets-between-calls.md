---
title: The adapter's mirror overwrites fields a test writes between calls; clear on use wipes unjoined slots
date: 2026-10-29
scope: games/shooters_edge/sim/qc_program.cpp, tests/shooters_edge_qc_fixture.hpp
concepts: [mirror, sync-in, fixture, in-call, mirrored-fields, live]
---

Two failures had one cause. A test set `maxspeed` and `gravity` in the VM between calls. The
next call's sync copied the loop's values over them, so the move ignored them. A spectator's
`trackent`, written into a client slot that was not yet joined, was wiped too: the sync clears
an entity the first time the loop uses it. The fixture now writes mirrored fields (health,
origin, maxspeed, gravity, movement, ...) inside a call (`Fixture::in_call`). `join` syncs once
after connecting, so a slot is live before a test writes to it. When a field "will not stick",
check whether the adapter mirrors it before you suspect the builtin.
