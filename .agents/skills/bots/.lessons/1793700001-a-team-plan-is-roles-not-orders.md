---
title: A team plan sets roles and claims, and each fighter keeps its own choices under them
date: 2026-11-01
scope: games/shooters_edge/bots/team.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [team, roles, hold, rush, carry, claims, callouts, held-fire, statistical-test]
---

The team plan (every half second) only names roles (hold, rush, carry, free), the key items and
the claims; the role's goal is tried first and everything else (the mode, retreats, the weapon,
the trigger) stays the fighter's. That keeps a duel's fighter unchanged where a test holds it,
except for what really changed for every fighter (reach filters, the walk onto an item).

A test that a better aim outscores a worse one on one seed is a coin toss once anything in the
bot moves: sum the frags over four seeds (140 to 99 here) rather than adjusting seeds until it
passes.
