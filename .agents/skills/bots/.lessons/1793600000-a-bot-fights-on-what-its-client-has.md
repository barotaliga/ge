---
title: A fighter's item knowledge comes from the map file and the entities sent, and its goal needs hysteresis
date: 2026-10-30
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/fight_run.cpp
concepts: [fighter, items, item-timing, entity-text, hysteresis, pick-best, retreat, dead-flag]
---

A client loads the map file, so a fighter may read the items' classnames and places off the map's
entity text, and an item taken is no longer sent (its model is gone), which times it fairly; no
item kind is guessed from a model name (the armours share one). Two things the lab's pictures
showed at once: the goal re-picked every tenth of a second flipped between near-equal items and
the pilot replanned without arriving, fixed by holding the current goal's score up by half; and a
retreat that would wait for a health box's return camped beside the mega health until killed
there, fixed by a retreat going only for health that is there within three seconds. Keep names
copied into the bot's own strings: an entity block's text is freed when the parse's vector goes.
The senses must drop a dead player (the protocol's dead flag), or a fighter empties its gun into
a body.
