---
type: Fixed
title: Pickups in a classic game say its own item names
---

With a classic game's packs loaded and the baseline mod playing them, a pickup now names the item as that game does: "You got the rockets", "You got the Nailgun", "You got the Pentagram of Protection", and a backpack lists what was in it ("You get the Rocket Launcher, 20 shells, 5 rockets"). The names are a table in the game's profile (`kClassicNames`), handed to the mod as the server's local info under each item's class name; the built-in arena keeps Shooter's Edge's own words.
