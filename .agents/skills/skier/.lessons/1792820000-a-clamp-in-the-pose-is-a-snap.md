---
title: A hard clamp anywhere in a pose is a snap; one lag on the lean is a pop; a ski rolled about its waist cuts the snow
date: 2026-10-22
scope: plugins/skier/app/rig_pose.cpp, plugins/skier/labs/anim_lab.cpp
concepts: [pose, poser, ik, clamp, snap, pop, lag, smooth-max, sidecut, edge-pivot]
---

The first poser on the skier model fixed the placeholder's pops and clips but showed 99 snaps
and 10 angle snaps in the `anim` lab: every one came from a `min` or `max` in the pose (the
arms' and legs' reach held short of straight, the pelvis lowered to the LONGER of two legs).
A clamp keeps a joint's place continuous but kinks its speed. Each became smooth: a reach is
eased onto its most by a tanh knee (`soft_reach`), and the longer leg is a log-sum-exp smooth
maximum, which brought every count to 0. The pops came from one lag on the lean: its output's
SPEED jumps when the input steps at a change of turn; two lags in turn start the speed from
zero (`tests/skier_rig_pose_test.cpp` fails one lag). And a ski rolled onto its edge about its
waist's half-width sank its wider shovel and tail 10 mm into the snow; rolled about the widest
line instead, nothing goes under. Ease only a reach that is a wish (the hands): easing the legs
moved the foot off the ankle the skis had placed, a bone that stretched.
