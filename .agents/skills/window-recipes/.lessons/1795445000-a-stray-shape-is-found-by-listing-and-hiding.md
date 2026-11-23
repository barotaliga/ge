---
title: Stray see-through wedges on a joined map were a figure the eye stood inside
date: 2026-11-23
scope: games/shooters_edge/client/demo_video.cpp, games/shooters_edge/client/entity_debug.cpp
concepts: [recipe, entlist, enthide, translucent, spawn-marker, encloses-eye]
---

On a QuakeWorld match server the window showed see-through wedges and they looked like another
game's art. The same spot in the window's own game showed none, so it was something the server
sent. `entlist 100` named a player model at alpha 0.3 exactly at the eye (the mod's marker at the
spawn the player stood on); `enthide` on it cleared the picture. A model drawn round the eye shows
its open insides, so `demo_placed` now skips any model whose standing box holds the eye. Compare
joined against local first, then list and hide: three runs found it, guessing at render switches
found nothing.
