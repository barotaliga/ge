---
title: Beside race photographs, the poser's carve reads shallower than a racer's
date: 2026-10-23
scope: plugins/skier/app/rig_pose.cpp, plugins/skier/labs/skier_views_lab.cpp
concepts: [poser, carve, lean, angulation, reference, photographs, stance]
---

The first reference sheet (`skier-views --refs`, two giant slalom racers carving, the angle swept
until the skis lined up) put the model at a state lean of 0.9 rad and an edge of 1.0 rad beside
racers whose hips nearly touch the snow. The model's hips stayed well above its boots and its
torso nearly upright. A racer's inclination and hip drop at speed are far beyond what the poser
draws for the same state numbers.

Before tuning the carve's look, put photographs beside it the same way. Read the inclination
off the photograph's snow line rather than trusting the state's lean, which is the body's
inclination as the physics sees it, not the pose's.
