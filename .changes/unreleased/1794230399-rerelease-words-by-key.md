---
type: Fixed
title: The re-release's messages show their words, not their keys
---

Playing the full game's re-release, every pickup, door, trigger, obituary and finale message showed its raw text key (`$map_skill_normal`, `$qc_got_item`). The game now reads the release's English text from its own archive beside the game folder and puts the words in, arguments and all, before a message leaves the server; the release's program also gets the builtins it calls by name or by number, and its achievement message no longer stops the client reading the level's messages.
