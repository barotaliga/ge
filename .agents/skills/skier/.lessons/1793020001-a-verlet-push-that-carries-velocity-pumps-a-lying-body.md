---
title: A Verlet push that carries velocity, fought by a joint limit every pass, pumps a lying body up
date: 2026-10-23
scope: plugins/skier/ragdoll.cpp, plugins/skier/skier.cpp
concepts: [ragdoll, verlet, constraints, snow, pump, energy, trunk, tips, glance]
---

Giving the tips their own collision threw `tree-fast`'s body from a metre short of the trunk, and
two seconds after it lay down it rose 1.5 m off the snow, twice. The cause was older than the
tips: inside the solver's passes the snow put a point back by moving its position alone (a
Verlet push is a velocity), while `hold_joints` moved a knee back into the snow carrying none
(position and last together). Eight passes a step, each round of that fight left the knee, and
through the thigh the hip, a little more upward way; the pass along the snow afterwards clamps
only the planted points, so the hip kept it. Keep the first pass's push (it is the step's fall
onto the snow, and the static hold on a slope rests on it) and make every later pass's carry no
velocity. Moving every push's last position too lets a body slide off any slope steeper than
its friction, which moved the drop scenarios 20 m.

To find a pump, print each point's velocity after every stage of every pass for the few steps
around the jump: the stage whose output grows pass after pass is the one fighting another.

Measure a trunk across the skier's whole line, not dead on: a head-on hit is the worst case of
a curve whose mean is what real collisions report.
