---
title: A shared cvar takes its default from the game's settings, and its words follow
date: 2026-11-14
scope: engine/console/words.cpp, plugins/shooter/app/look.cpp, engine/hud/crosshair.cpp, engine/camera/cvars.cpp
concepts: [cvar-defaults, game-defaults, shared-cvars, values-for-default, stated-default, plugin-cvars]
---

When a plugin or an engine module registers cvars that several games share, a literal `.fallback`
in the spec makes every game take the first game's default, and `bind` writes it over whatever
the game set. Take the default from the field instead (its value at the call, as text), so a game
sets its own settings struct before registering. Then pass the values text through
`values_for_default(values, fallback)`. It keeps every "(the default)" mark that names that
default and removes the ones that name another, so a game's help never names some other game's
default. Before moving cvars like this, check that nothing changed for the first game. Dump each
cvar's fallback and values from the new code and diff them against the old literals.
