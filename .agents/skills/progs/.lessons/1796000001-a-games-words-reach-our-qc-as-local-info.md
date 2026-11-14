---
title: A loaded game's words reach our QC as local info under the class name
date: 2026-11-04
scope: games/shooters_edge/rules/, games/shooters_edge/profiles/items.hpp, games/shooters_edge/client/game_folder.cpp
concepts: [qc, localinfo, infokey, item-name, pickup-message, profile, glue]
---

Our QC has no `strcat`, so a key built at run time is out: data a profile hands the baseline mod
goes in the server's local info under a key the QC already holds, the item's `classname`, and the
arena glue's `name_of` reads it with `infokey(world, key)` (serverinfo first, then localinfo; ""
when unset, so `== ""` picks the mod's own word). The single-player form has no `infokey`, so the
campaign glue returns the fallback. The window sends the lines (`localinfo <class> "<name>"`)
before `console` on each match server only when the baseline mod plays a game's packs. Prints
come in pieces, so a test joins `ModMatchReport::prints` and splits on '\n' before matching.
