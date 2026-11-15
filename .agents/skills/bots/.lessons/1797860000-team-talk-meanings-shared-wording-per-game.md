---
title: A team message's meaning is ge::shooter's; its wording and places are the game's
date: 2026-11-15
scope: plugins/shooter/team_talk.hpp, games/shooters_edge/bots/talk.hpp
concepts: [team-talk, bots, seams, say-kind, wording]
---

`SayKind`, `Strategy`, `Said` and `SayStatus` are `plugins/shooter/team_talk.hpp`: what a message
means and who acts on it. How a game words a message, parses a chat line back to a meaning, and
names places (its macros, its loc files) stays in the game's `bots/talk` beside the using-
declarations, so a later game writes its own `say_text` and `parse_say` over the same meanings.
Bot movement, item worths, routes and weapon choice stay per game too; lift a bot part only when
the next game would share it unchanged.
