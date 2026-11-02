---
title: A recorded rocket carries no owner: its shooter is the player nearest where it first shows
date: 2026-11-01
scope: games/shooters_edge/client/demo_scan.hpp, games/shooters_edge/client/demo_scan.cpp
concepts: [demo, rockets, grenades, flights, shooter, kills, scan-demo, gold-shots]
---

A projectile in a recording is an entity with a model and an origin, nothing more: no owner, no
velocity. `scan_demo` follows each missile and grenade from the frame it appears to the frame it
goes (`DemoScan::flights`), and gives it the player nearest its first origin within
`kShooterReach` (80 units: it appears a frame's flight in front of the muzzle) that frame; in real
1 on 1 and 2 on 2 recordings about two in three flights find their shooter this way. A kill's
rocket is still matched from the victim's end (nearest landing within `kRocketReach` and
`kRocketWindowS`), since the victim's spot is known and the shooter's guess is not needed for it.
Bots learning "where rockets go" read whole flights, hit or miss, not only the kills.
