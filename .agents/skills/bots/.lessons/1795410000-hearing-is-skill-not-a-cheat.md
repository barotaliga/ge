---
title: Hear through the client's sounds by skill, and snap a sound only to spots the listener knows
date: 2026-11-01
scope: plugins/shooter/hearing.cpp, games/shooters_edge/bots/ears.cpp, games/shooters_edge/bots/buttons.cpp, games/shooters_edge/qc/common/player.qc
concepts: [hearing, pickup-sound, spawn-sound, telefrag, buttons, team-map]
---

The senses used to hear every enemy sound exactly where it was, for every skill: a cheat that made
novices hear like experts. Turn that off (`SenseSettings::hear_players = false`) and hand in what
the bot's own ears take in (`sense_heard`): the direction true, the distance off by the ear's
error. A listener who knows the map snaps a pickup to the item of that pickup nearest the guess
and a spawn to the nearest spawn point; one who does not, keeps the guess.

The classic armour pickups share one sample, so which armour went is known only by where it was
heard: put the sample's word on the item kind (`hears_as`), never on one item.

A run where two players stood frozen at one spot for minutes was two spawns on one point: with
every point taken the spawn picks one anyway. The classic answer is the telefrag (the spawn kills
whoever stands on its spot); without it a symmetric 4v4 can freeze the two mirrored players
together. Look for identical positions across players in the lab's last lines when frags collapse.

A button with `"health" "0"` is touched, not shot: read the number, not the key's presence.

Only a UNIQUE pickup sound says which item went. The ammunition box's sound is the backpack's too,
and three health boxes share one, so letting a common sound mark the nearest such item gone left
experts believing half the arena empty. A common sound places the player and no more.

A scarcity weight (the map's lone big armour) is a TEAM plan. Applied to a weak player's or a lone
deathmatch player's choice it kept an expert walking at an item on a lift held up top for the whole
match. Weigh only the team's wants, and let go of a goal held 30 s in vain.
