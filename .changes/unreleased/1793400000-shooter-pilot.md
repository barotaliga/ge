---
type: Added
title: Shooter's Edge bots move like players: the pilot
---

`ge::shooter` gains a pilot (`shooter/pilot.hpp`): a nav route played one usercmd at a time through the player move and `ai::Hands`, jumping from a run-up at each jump's proven speed, strafe jumping long straights, waiting for a door (and planning round one that stays shut) and riding a plat, with every decision recorded for `ge why`. The nav graph now rides movers given a travel. On the server a pilot fills a bot slot through the command-source seam, and the `se-pilot` lab draws its runs on the nav course and the arena with their times and humanness.
