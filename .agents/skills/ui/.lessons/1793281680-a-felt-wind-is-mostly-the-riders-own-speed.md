---
title: Alarm on the weather's wind, not the felt one, for anything that moves fast
date: 2026-10-29
scope: engine/hud/instruments.cpp
concepts: [hud, wind, felt-wind, gale, alarm, instruments]
---

A skier at 87 km/h feels a 24 m/s wind in still air, past a gale. A wind meter that turns red when
the felt wind passes a gale is therefore red for the whole run and means nothing. The felt wind is
what the arrow shows, but the alarm colour must read the weather's wind (the air over open
ground), and use the felt speed only when the game gives no weather. The `hud-instruments` lab's
game frame showed this at once. Stage a lab's snapshot at the speed players really ride, not at
rest.
