---
type: Added
title: Door, plat and button sounds and toggle doors in our programs
---

Our programs' doors, plats and buttons now play the sounds the map's `sounds` key asks for, and a door that needs a key sounds locked when walked into and unlocking as it opens, by the world's type. A door with spawnflag 32 opens on a use and stays open until the next use shuts it. The `se-campaign` lab's `--movers=<s>` sets off every door and plat of a map in turn and tables what each plays.
