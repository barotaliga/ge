---
type: Changed
title: Console commands need no +
---

Only a button (a held action such as `+attack` or `+jump`) takes a `+`. Every other command is typed bare in the console (`coopgame 2 expert`), and a `+` typed on one is dropped, so `+coopgame 2 expert` still works and the release a key bound to it sends runs nothing. On the command line of `shooters_edge`, `fall_line` and a dedicated server, the first command needs no `+` either (`coopgame 2 expert +map e1m2`); a `+` still starts each command after it.
