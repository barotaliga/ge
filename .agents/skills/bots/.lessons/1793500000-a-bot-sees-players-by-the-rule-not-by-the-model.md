---
title: A bot's view must send players by their slot, not by their model, and measure aim off its own view
date: 2026-10-30
scope: plugins/shooter/senses.cpp, plugins/shooter/aim.cpp, games/shooters_edge/bots/duel_run.cpp
concepts: [senses, client-view, modelindex, sight, aim, turn-axes, quantize, skill]
---

The first client view sent every entity "with a model" and the seer saw nothing: the server's
players carry modelindex 0 until a game program sets one, so players are sent by the rule (in use,
active, not a spectator) whatever their model. Lost sightings were counted twice until a contact
kept whether it was seen the frame before (`was_visible`). Turning the movement axes under the
aim's view gave -4 for a zero axis at a right angle, float noise floored by `quantize_axis`: round
before quantizing. The truth a test holds the senses to (a clear eye-to-eye line) is not the
senses' own rule (head or middle, in the cone), so "seen per line frame" can pass 1; compare skill
by the median error over a seen frame, which kept its order (perfect, expert, average, novice) on
every seed tried, rather than by the share seen.
