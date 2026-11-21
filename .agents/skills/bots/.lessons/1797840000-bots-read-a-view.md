---
title: Bots read the match through ge::shooter::BotView, never a game's server type
date: 2026-11-15
scope: plugins/shooterbots/bot_view.hpp, plugins/shooter/map_spec.hpp, games/shooters_edge/bots
concepts: [bot-view, bots, seams, rules-world, map-spec]
---

`BotView` is five references (the loop `sv`, the rules' `world`, `models`, `sounds`, `map_name`)
and converts from any rules' state with members of those names, so a call site passes its server
unchanged. A bot file names `ge::shooter::WorldEntity`, `PlayerNumbers` and `MapSpec`, never the
game's `server::` spelling; only the match runners (`fight_run`, `duel_run`, `coop_run`,
`pilot_run`, `q2_unit`, `coop_episode`) and Quake II's arsenal binding may include a game's
server, and they stay in the game when the bot core moves to the plugin. A header that stops
including the game's server breaks files that got its types through it: include what they use.
