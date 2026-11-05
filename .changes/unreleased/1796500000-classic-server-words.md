---
type: Changed
title: A classic game's server speaks its own words
---

With a classic game's packs loaded, everything a player reads from the server is now in that game's words and colours: deaths by weapon, gib and quad ("bob rides ann's rocket", "ann rips bob a new one", "bob sleeps with the fishes"), teammates' kills (scored -1), the clock ("[19] minutes remaining" in gold, the team in the lead), "The match is over" with the teams' scores, the powers wearing off, and the lobby ("ann is ready - no matchtag set", "All players ready", "Timer started", "The match has begun!") with its countdown's centre print each second and its sounds. Nothing is told or scored before the match starts. The words are a table in the game's profile (`profiles/words.hpp`), filled in by the new `words` builtin (#640, `GE_QC_WORDS`); the weapons and armour now carry the game's own names ("Double-barrelled Shotgun", "Red Armor"). The built-in arena keeps Shooter's Edge's own words. The window shows a centre print in the middle of the screen.
