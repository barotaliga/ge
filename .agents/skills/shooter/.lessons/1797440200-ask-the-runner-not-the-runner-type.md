---
title: The window asks the rules' world or a runner trait, never which runner it is
date: 2026-11-15
scope: games/shooters_edge/server/rules_world.hpp, games/shooters_edge/server/rules_runner.hpp, games/shooters_edge/client/
concepts: [rules-runner, rules-world, world-shot, own-screens, own-words, seams, shooter]
---

When client or net code needs something from the running rules, put it in the rules' world
(`RulesWorld`, filled by every runner: a shot in flight is `WorldEntity::shot`) or make it a
trait or call on `RunnerCalls` (`own_screens`, `own_words`), and have each runner fill it. A new
`sequel_of(rules)` or `qc_of(rules)` outside the server is a seam leak: a third game's runner
would need its own branch at every such place (M60d).
