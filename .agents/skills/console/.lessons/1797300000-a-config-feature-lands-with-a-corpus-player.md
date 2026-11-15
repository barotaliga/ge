---
title: A config feature lands with a player in the corpus that uses it
date: 2026-11-14
scope: engine/console/script.cpp, engine/console/expr.cpp, engine/console/binds.cpp, tests/console_configs_test.cpp, tests/golden/console_configs/
concepts: [config-corpus, players-configs, if-without-then, expression-if, old-key-names, golden]
---

A unit fixture in `console_script.txt` shows one command at a time. It cannot show what players'
configs need together: a startup that execs other files, keys pressed and released, lines heard
from the game, and the config the session saves. The corpus does
(`tests/golden/console_configs/<player>/`: the player's cfgs, a `play.txt`, a `golden.txt`). When
the language grows for players' configs, write the feature into one player's files as a player
would use it, and regenerate with `GE_GOLDEN_WRITE=1 build/debug/tests/console_configs_test`.
Read the whole golden before you commit it: every printed line and the saved config. Two things
the corpus found: older configs leave out `then` and write keys as `UPARROW` or `LCTRL`. Writing
the expression form of `if` over the lexed words failed, because quotes and brackets came back
wrong. Take the raw text instead (`Args::rest(0)` up to the first bare `then`) and expand it.
