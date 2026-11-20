---
title: Re-judge an old switch on today's build; split a loss by who holds the view before building
date: 2026-11-19
scope: plugins/shooter/features.cpp, plugins/shooter/bot.cpp, plugins/shooter/pilot.cpp
concepts: [features, judging, re-judge, aimhop, circleview, aimground, verdict, head-to-head, flights-by-takeoff, view-writer]
---

Two switches judged long ago flipped when judged again on the current build: `circleview` (dropped
on an even head to head before speed decided first) gave +1.8 a match and own-view circles 339 ->
371; `aimhop` (on since early) was -3.4 a match against its absence, its aimed flights losing at
every takeoff speed. Other switches had changed what they sat on. When a measure points at a part a
switch already covers (in judging or on), re-judge that switch first: one set and a head to head,
before writing anything new. The fault was found by splitting the flights by takeoff speed AND by
who held the view (the trace's `view` three commands in): own-view hops held their speed where the
aimed ones lost; seven new switches built on guesses about the walls before that split all came
back even or worse.
