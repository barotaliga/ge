---
title: A habit asked for is measured off people's recordings by spot before it is built, and the gap built for
date: 2026-11-20
scope: games/shooters_edge/labs/glance_read.hpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/bots/knowledge.cpp, plugins/shooter/bot.cpp
concepts: [glance, itemglance, key-spot, people-first, by-spot, gap, maintainer-figures, look-aside, on-screen, crosshair]
---

The glance at a key item's spot was asked for as "an expert every pass". Read off people's dm2
recordings (se-people `glance.key.*`) the habit is much rarer and level-blind: of the passes
starting with the spot off the screen, the view comes within 20 degrees of it 26 % (players p25-p75
22-29 %), for 0.08 s at the median; and over all spots the bots already showed the spot as often as
people (66 / 68 % a pass). The gap was only at the launchers (the low RL 77 / 96 %). Two things
follow for any habit asked for:

- Measure it by SPOT (or by item, by place), not only overall: an overall match hides where the
  bots lag and where they already lead (the pillar YAs: bots over people, a glance moves them away).
- Measure what the habit is FOR (the spot on the screen at all: what a bot learns by), beside the
  habit's look (within 20 degrees). People bring a spot onto the screen without centring it: the
  bots' swipe that stopped 35 degrees short kept the sightings and halved the over-centring.

Also: a behaviour that fills a belief is judged by whether the belief was wrong. `team.lying.*.
unknown` (nobody believing a lying item there) was 0.0-0.4 % for every item before the glance, so
it could not move item control; it was kept as a human measure.
