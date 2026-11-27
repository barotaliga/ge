---
title: The window's Quake campaign runs the packs' own program, not the engine's campaign rules
date: 2026-11-27
scope: games/shooters_edge/client/game_folder.cpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/labs/se_remix_campaign_lab.cpp
concepts: [campaign, pack-progs, own-campaign, remix-campaign, campaign-program, cr-monsters, cr-items, recipe]
---

NEW GAME in the window starts Quake's campaign on the packs' `progs.dat` (GameContent's
`campaign`); the engine's own campaign rules (`campaign_mod`) ran only in tests, labs and the bots'
coop runs. A lab built on `own_mod("campaign")` therefore passed while the window failed: the packs'
program has no spawn for Quake II's monsters and ignores the arsenal's rows. Anything the remix asks
of the campaign's rules goes through `remix_campaign`, and a lab of it starts on the packs' program
(`se-remix-campaign --start=pack`). When a lab passes and the player still sees the bug, drive the
real window with a recipe (`window-recipes`) before guessing: `newgame`, the cr_ setting, `map`,
`+showscores` and a screenshot answered it in one run. A recipe saves the config on quit: copy
`settings/config.cfg` first and put it back.
