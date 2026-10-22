---
type: Added
title: Play a game through ge, replay it and find where two runs part
---

A game now fills the engine's play contract (`ge::play`: its axes, its options, and a run that steps, observes, reports its state and its digest, and can pilot itself) and gets a `play` executable from it. `ge play` puts an agent at the controls over a JSON line protocol; `ge sim` has the game's pilot play each seed and writes each run as a plain-text input log; `ge replay` plays a log back and fails when it misses its recorded digest; `ge state` prints everything that decides the next step at any step; `ge diverge` names the first input and the first step two runs part, and the fields that differ. A scaffolded repo plays from its first commit, its CI replays its runs, and the new `play-test` consumer skill says how to use all of it.
