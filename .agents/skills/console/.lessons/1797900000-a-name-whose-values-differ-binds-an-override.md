---
title: A game's setting whose values mean other things binds an override field, not a synonym
date: 2026-11-21
scope: engine/console/view.cpp, games/shooters_edge/client/cvar_layers.cpp
concepts: [synonym, override, game-chat, con-auto-chat, bindings-reset, layer-switch]
---

Quake II's `con_auto_chat` 2 means "said to the team" where our `con_chat` 2 means "a command when
the first word is known", so a synonym would silently change meaning. Bind the game's cvar to an
override field the engine reads over its own (`ConsoleView::game_chat`, -1 unset; its order
mapped in `chat_mode()`), pass the field through `GameBindings`, and reset it to unset in
`add_game_cvars` before every game's rows: a bound field keeps its last value when the layer
drops, so the next game would inherit it otherwise.
