---
title: A kind's own attack test in the campaign is a th_check returning by ret_f, its mode a state
date: 2026-11-27
scope: games/shooters_edge/rules/campaign_monsters.cpp, games/shooters_edge/rules/campaign_ai.cpp, games/shooters_edge/rules/campaign_decl.hpp
concepts: [campaign, scrag, attack-test, slide, th-check, anonymous-namespace, digests]
---

`ai_run` calls a kind's `th_check` and reads its answer with `q.result_f()`, so the kind's test
ends with `q.ret_f(1 or 0)`. A hunting mode the test sets (the scrag's slide) is an
`attack_state` value (`kAsSlide`) that `ai_run` reads after the test fails; the frames that mode
shows are swapped by rewriting `fr_run`, since `monster_run` loops whatever it holds. Helpers in
`campaign_ai.cpp` live in an anonymous namespace: a new one there must be defined before its first
use in the same namespace, not declared in `campaign_decl.hpp` (the call then binds to the
undefined outer declaration and the inner one is "defined but not used"). A change to Quake's
own monsters moves the campaign's lab digests on purpose: rewrite them in the same PR and tell
whoever keeps baselines on the campaign (the campaign bots).
