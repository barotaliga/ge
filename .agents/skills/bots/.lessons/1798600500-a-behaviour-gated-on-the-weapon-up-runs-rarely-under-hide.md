---
title: A behaviour gated on the launcher being up runs rarely while `hide` keeps it down
date: 2026-11-20
scope: games/shooters_edge/bots/spam.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [spam, spamshow, hide, holds-launcher, has-launcher, gate, calm-rockets, raise, rare]
---

- `spamshow` (rockets at show-up spots) first doubled the spam and moved nothing: of the commands
  carrying the launcher, 64 % had it DOWN (`hide`: the shotgun up while no one is in sight, so a
  death drops no launcher). A gate on `holds_launcher` reads "carried" as "never".
- Count the first failing gate per command in the lab (`team.spamshow.<gate>`) before tuning a
  rate: the fix was to raise the launcher for the shot (as people do) and keep `hide` off for
  `kRaiseHold_s` after the raise, or the two switch the weapon back and forth.
- Then: rockets at spots 7 -> 60 a match, calm rockets 2.1 -> 4.5 a minute, the blind share 8.5 ->
  28 % (people 23), head to head even.
- A switch "turned on" by a text edit of `kJudging` (plugins/shooter/features.cpp) can silently
  miss once clang-format has wrapped the list: `spamshow` was committed as on and stayed off. Prove
  a switch is in the preset by a run: the preset's set must repeat the variant's "on" row bit for
  bit (or the switch's own counter is non-zero in a plain run) before the commit says "on".
