---
title: The muzzle-flash bit on a monster is set by casters too; key a gun's fire to the model
date: 2026-11-13
scope: games/shooters_edge/client/world_fx.cpp
concepts: [monster, muzzle-flash, effects-bit, caster, ce_monsters, replace-table]
---

The entity's muzzle-flash effect bit is set when a soldier or an enforcer shoots, but also
when a flier, a knight or the big caster throws. Drawing the modern shotgun fire for every
flash put a gun's burst in a caster's hands. The classic draws a light for each, so the light
stays for all; the fire is drawn only for a model the replacement table names `monster.gun`
(`plugins/classicenhance/replace.hpp`).
