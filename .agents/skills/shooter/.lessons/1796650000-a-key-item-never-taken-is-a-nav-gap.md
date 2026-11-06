---
title: An item people take every minute that bots never take is a gap in the nav: find the node, then the proof that refused its way in
date: 2026-11-07
scope: plugins/shooter/nav.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [nav, prove-link, standing-jump, run-up, stairs, pocket, dive, swim-exit, reach, real-map]
---

On e1m2 people take the yellow armour 54 times a 20-minute match and the launcher about 60; the
bots took neither. Both were nav gaps, found the same way: print the item's node, its links out,
every node within 200 units and the links into each (a temporary `fprintf` in the fighter, behind
an environment variable, taken out before the commit). The armour's pedestal had only links down:
its stairs (28 to 32 units high, 32 deep) failed every jump, because a moving jump needs 32 units of
level run-up behind it and a stair's tread has none. Then `prove_link` called directly with the
path printed showed the standing jump landing on the tread 13 to 17 units short of the node and
refused as "touched the ground before the target". The fix: a standing jump tried last onto a
neighbour, allowed to land 24 units short on the target's floor and step on. Apply that landing
rule to the standing jump alone: on moving jumps it let a full-speed jump overshoot a crate and
count as reached, and the pilot ran off the far side (`shooter_pilot_test`'s human hands). The
launcher's pocket had no node of the main part within 72 units (`nav_reach` from the item against
from a spawn): only the dive reaches it, and the dive did reach it; what failed was after, the
"escape a small pocket" rule sending the bot back into the water the moment it climbed out.
