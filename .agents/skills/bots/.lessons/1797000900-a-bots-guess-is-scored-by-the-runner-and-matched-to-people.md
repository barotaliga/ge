---
title: A bot's guess about what it cannot see is scored by the runner, and the want matched to people first
date: 2026-11-09
scope: games/shooters_edge/bots/packs.cpp, games/shooters_edge/net/classic_team.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/qc/common/player.qc
concepts: [packs, backpack, pack-guess, packinfer, truth, refire, weapon-switch, se-people, item-control, a-b]
---

Every death drops a pack, and the weapon in it is the one in hand at death. A bot infers it from
the shots it heard by weapon (`pack_guess`, by its ear's level). Score the guess where the TRUTH
is and the bot is not: the runner (`classic_team.cpp`) calls `packs_truth` with the server's own
world after the bot's `packs_obituary`, and fills `KnownPack::truth` for the lab
(`team.packs.guess.<skill>.right`). The bot never reads it. First read on e1m2: novice 82 %,
average 86 %, expert 86 % right (bots switch weapons by range without firing, so even "the last
weapon fired" misses).

Before tuning how much a bot wants something, measure people's want by the same rule. `se-people`
`packs-*` reads people's recordings: they take 99 % of packs, 85 % within 10 s, after 5.4 s on
average, and bots without the feature already took 100 % (5.6 s). So "go for every pack" did not
fix a gap. It pulled bots off the key items: yellow armour lying 73 -> 85 %, head to head
neutral. The pack goal yields to a key item believed up that the bot is going for.

The server holds a weapon select until the refire ends (`porting/shooters-edge/weapon-switch.md`).
The baseline mod switched at once until this change (`PlayerPostThink`). A mod rule like that
moves every match's digest. Check the notes before trusting the mod's timing.
