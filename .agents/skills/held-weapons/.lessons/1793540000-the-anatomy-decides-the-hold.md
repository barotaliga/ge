---
title: An IK with only a pole bends elbows upward; the joints' ranges, the grip's obliquity and the body's stance fixed the arms
date: 2026-10-31
scope: games/shooters_edge/content/models/view/weapon_kit.py, engine/viewmodel/arm.cpp
concepts: [ik, elbow, wrist, forearm-twist, power-grip, cradle, stance, hold, anatomy, arm-poses]
---

The first two-bone IK took a pole and the nearest straight forearm, and the shotgun's right elbow
pointed up. Measuring every joint (`engine/viewmodel/arm.hpp`) showed four separate causes, each
with its own fix. The search never had a range to keep to, so the kit now tries the elbow's whole
circle under a comfort cost. Each hand's turn on its grip was a guess, so the kit now searches it.
The grip crossed the palm at a right angle, which forced 35 to 58 degrees of ulnar deviation on
every arm, so a power grip now crosses obliquely and a cradle more so. And the support arm reached
across to a gun held high and to the right, so long guns get a stance and sit low and central.

Two traps cost the most time. glTF's y-up turns a node's local axes: the builder's hand +y arrives
in the engine as the node's -z (and +x as -x), so `arm_poses` read the wrist 90 degrees wrong
until the kit's and the engine's angles were compared on one model. Compare them whenever either
changes. A linear cost ran single joints to their limits; a squared distance outside an easy band
spreads the strain the way a body does.
