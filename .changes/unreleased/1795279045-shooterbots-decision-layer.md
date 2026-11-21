---
type: Changed
title: "The bots' decision layer is in the shooterbots plugin"
---
`ge_shooterbots` now holds the whole fighter a shooter's bots play with: its mode, goal, weapon
and trigger, its mind and team roles, what it knows of the items and hears, the packs, the ledger
of each enemy, the map's plan, goals and nav with its movers, the footwork, the water, the co-op
companion and the way through a campaign map. A game hands its bots a `BotArsenal`: its weapons and
item rows, the words and sounds they hear, its maps' book of people's tricks and routes
(`map_book.hpp`) and its team words (`TeamWords`). `fighter_init` takes that arsenal,
`build_map_nav` and `map_goals` the book. A map's entities are read with `shooterbots/entities.hpp`,
place names with `shooter/places.hpp`, and the item tables' shapes are
`shooterbots/item_tables.hpp`.
