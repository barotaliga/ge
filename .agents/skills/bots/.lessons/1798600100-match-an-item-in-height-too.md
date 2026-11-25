---
title: A map's item matched to its entity across the floor only merges stacked items
date: 2026-11-20
scope: plugins/shooterbots/fighter_setup.cpp
concepts: [items, item-spot, entity, stacked, grenade-launcher, dm2, dm6, itemz]
---

The bots never went for dm2's grenade launcher although the nav reached it: the item list matched
each map item to the sent entity nearest ACROSS the floor, and the launcher on its ledge (z 320)
lies straight over a yellow armour (z 16): both spots took one entity, and the launcher's never
showed in a goal's options (the decision log's `item_goal` lines name every candidate: grep it for
the item before tuning its worth). dm6 stacks its launcher over a nailgun. `itemz` matches in
height too. Before blaming an item's worth, list the map's items within 48 across of each other
(`brush ents <map> --class=...`) and read the bots' item labels.
