---
title: A remixed monster decides by its own game's mind, lifted as pure functions both rules call
date: 2026-11-27
scope: games/shooters_edge/server/q2_monster_mind.hpp, games/shooters_edge/rules/campaign_q2_kinds.cpp
concepts: [remix, cr-monsters, monster-mind, lift-before-port, random-draw-order, sq-runs]
---

Quake II's monster AI already existed for Quake II's own rules (`server/q2_monsters.cpp`), coupled
to `Q2Server`; the classic campaign wearing those monsters ran them on Quake's `check_attack` and
`look_for_target`. The lift that keeps both honest: the decisions as header-only functions over
plain facts (`q2_mind::Sighting`, `Loop`, `Attacker`) taking the caller's draw as a callable, so
each world keeps its own random stream and the draws happen in the order the old code drew them;
Quake II's tests and every lab digest then prove the lift changed no bit. On the campaign side a
Quake II kind is told by its pain hook (`sq_runs`: `th_pain == sq_pain`), and its own
`th_check` (`sq_check`) starts the attack at once. `se-remix-campaign` measures it: hits taken,
the first hit and the monsters hunting, by skill. Not lifted yet: the chase (last sighting, the
trail), hearing, the duck and run-and-shoot, which lean on `Q2Server`'s trail and noise state.
