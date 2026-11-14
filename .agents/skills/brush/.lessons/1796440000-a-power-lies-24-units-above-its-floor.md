---
title: A power's box reaches 24 units below its origin; set it lower and the game removes it silently
date: 2026-11-14
scope: games/shooters_edge/content/maps/keeps.plan, games/shooters_edge/rules/common_items.cpp
concepts: [map-plan, entity, item, quad, droptofloor, se-team, item-reach]
---

The Keeps' quad stood 8 units above its platform, as a health box or a weapon may. Nothing took
it in ten minutes of `se-team`, though "items no route leads to: none" said it was in reach: the
arena's rules size a power's box from 24 below its origin to 32 above (`power_start`), lift it 6
and drop it to the floor, and an item whose box starts in solid is removed with only a developer
print. Place a power (quad, ring, pentagram, suit) at least 24 above the floor it lies on; weapons,
armour, health and ammunition start their boxes at the origin. After a map change, read
`team.<side>.quads` and `took-per-min` and the trails picture, not only the route line.
