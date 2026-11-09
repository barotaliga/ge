---
title: A scripted way must start as its proof did, and a person's view needs its keys scaled
date: 2026-11-09
scope: plugins/shooter/nav_script.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/item_routes.cpp, games/shooters_edge/labs/se_spot_lab.cpp
concepts: [scripted-route, nav-script, item-routes, prove-script, script-keys, entry, lined-up, facing, se-spot, treads, head-to-head]
---

e1m2's yellow armour stairs (treads 16 deep, 46 wide) as a scripted way proved in 1.94 s, yet
the first six-seed read was WORSE (YA lying 71 -> 87 %, red without it +49). A temporary trace
of the pilot on the script link showed why: the proof starts standing at the entry, while
bots arrived at a hop's 375 u/s or from the side; a "passed it toward the next step" rule let
the entry count from 43 units off the line; full forward under a view turning at a person's
pace (103 deg/s) ran them wide of the stairs, then they orbited the entry. Fixed by keying the
pilot as the proof starts: the entry left only LINED UP (under 330 u/s, standing or running
within 30 degrees of the way) and never "passed"; a step's "past" only near the line; the entry
and the last step slowed into; forward by a strict facing (full within ~14 degrees, a hop only
then). Second read: head to head +40 for the side with routes, pace unchanged.

Before scripting a place, draw it: `labs se-spot --at="x y z"` (from above by height, cuts with
every standing floor, doors, buttons, teleporters and their destinations) found the missed
tread and the doors over both stairs in one run. A denser nav (`NavSettings::treads`) found the
tread too but cost frags everywhere else (346 nodes, many standing jumps): fix a key place with
a script, not the whole graph.
