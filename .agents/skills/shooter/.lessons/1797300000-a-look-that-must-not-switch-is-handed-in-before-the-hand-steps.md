---
title: A look that must not show a weapon switch picks the hand's weapon before the hand steps
date: 2026-11-10
scope: plugins/shooter/quick_weapon.cpp, games/shooters_edge/client/quick_fire.cpp, games/shooters_edge/client/held.cpp
concepts: [quick-weapon, ghost-weapon, weapon-switch, hand, fade, listen-server, tap]
---

The hand (view/hand.hpp) animates a switch whenever its input weapon changes, so a look laid over
its finished frame can recolour the weapon but never hide the put-away and raise. The quick weapon's
preview hands its weapon in through `HeldState::show` before `step_held`, and a shot counts only
when the shown weapon is the game's. A fade judged only by "the key is held" barely moves on the
listen server: a tap lets go after one 13 ms frame, so the look holds solid for `kHoldS` after the
press, whatever the tap's length. Default `QuickState::pressed_at` far in the past, or that hold
fires at start-up.
