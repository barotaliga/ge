---
type: Fixed
title: Quake II's weapons fire as Quake II's in Quake's rules under classic remix
---

With classic remix's `cr_items q2`, Quake's rules now fire Quake II's weapons whole instead of Quake's weapons wearing Quake II's models. Each weapon steps Quake II's own hand (`plugins/classic/q2_arms.hpp`, the same code Quake II's rules step): the shotgun fires once in 1.2 s with 12 pellets, the super shotgun two halves of 10, the machinegun and chaingun their held-fire runs, the hyperblaster its loop, the rail goes through every body in its line once in 1.6 s, and the grenades and rockets fly, bounce and blast as Quake II's, with Quake II's damage, knockback and obituaries. Their samples are Quake II's under any `cr_sounds`, their hits, blasts and bolt trails are drawn as Quake II's, and the view model plays Quake II's own frames; a joined client's weapon prediction steps aside for them.
