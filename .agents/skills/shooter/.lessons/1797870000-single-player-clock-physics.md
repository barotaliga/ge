---
title: The campaign's physics rules sit behind the single-player clock; the arena host keeps its own
date: 2026-11-21
scope: plugins/shooter
concepts: [single-player-clock, push, trace, touch-order, bound-velocity, campaign, goldens]
---

Rules that make the campaign play as the original single-player engine (the push swept with
touches, a start-solid trace winning, a stuck sweep ending at its end, triggers touched node by
node, the speed bound per axis and before the player's think, currents read as water) are gated on
`ServerSettings::single_player_clock`, never applied to the arena host: its traces and pushes are
held by recorded flights and goldens (`shooter_recording.txt`, the toss recordings), even where
another release's multiplayer server reads like the original. Builtins that only a single-player
program's form reaches gate on `vm_layout(vm).single_player` (the free rule); the angles in whole
degrees gate on a bytecode program too (`Progs::natives` unset), because our own C++ campaign
rules keep the fractions: truncating them for the native campaign moved the companion tests' bot
runs (kills, how near the companion keeps) and `shooters_edge_campaign_rules_test`'s hashes.
