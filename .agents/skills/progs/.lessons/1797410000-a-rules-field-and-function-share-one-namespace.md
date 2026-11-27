---
title: A native program's field and function share one name space; a lab aims through the listen's view
date: 2026-11-27
scope: games/shooters_edge/rules/campaign_decl.hpp, games/shooters_edge/labs/se_remix_campaign_lab.cpp
concepts: [native-program, x-list, field-name, function-name, lab-aim, listen-view, dodge]
---

The X-lists in `rules/campaign_decl.hpp` give a field `f_<name>` and a function `fn_<name>`, so C++
compiles a field and a think of the same name, but the program's loader refuses it at the first
map ("function sq_duck is declared twice"): name a think's state field apart from it
(`sq_duck_frame` beside the think `sq_duck`). In a lab driving the window's listen, a `MoveCmd`'s
`angles` and `impulse` are overwritten by `listen_frame` (it takes `l.view` and `l.impulse`): aim
and pick a weapon through those, else every shot goes the way he faced at the start. A lab that
fires at monsters picks only those in his sight (`sh::trace_move` with `kTraceNoMonsters`).
