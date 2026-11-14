---
title: The scripted classic test client presses fire and jumps on its own
date: 2026-11-14
scope: tests/shooters_edge_classic_tv_udp_test.cpp
concepts: [classic-client, test, viewer, fire, brain, tv]
---

`ClassicClientEnd`'s default script presses attack two ticks in every hundred and jump now and
then, so a test viewer at a face where fire means "next view" wanders through the players by
itself and a check on its seat fails at random-looking places. A viewer that must sit still gets
`ClientConfig::brain = [](int) { return ge::shooter::MoveCmd{}; }`: hands off, no buttons.
