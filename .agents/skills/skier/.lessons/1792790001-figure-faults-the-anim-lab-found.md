---
title: The placeholder figure's faults the anim lab found on its first run
date: 2026-10-22
scope: plugins/skier/app/figure.cpp
concepts: [figure, pose, anim, lean, edge, clip, pop, ski-roll]
---

`make lab LAB=anim ARGS="--scenario=turns"` on its first run found three faults in the figure,
all from how it is posed, none from the physics' state being wrong: (1) the whole upper body
POPS at every turn's change (10 to 15 m/s at the head), because the body is rolled straight by
`lean`, which swings fast between edges; (2) the downhill ski's tip and tail sink 6 cm under the
snow, because the skis roll by `edge` about the centre line under the boots instead of about
their edges; (3) the inside pole tip goes through the snow while leaning, because the tips are
placed in the rolled body frame at y = 0. The skinned figure (M8) should be posed so the lab
reads clean on all three; until then they are the baseline a change is compared against.
