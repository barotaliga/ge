---
title: Save the whole server and check it by playing on beside the game never saved
date: 2026-10-31
scope: games/shooters_edge/server/campaign_save.cpp, games/shooters_edge/labs/se_campaign_lab.cpp
concepts: [save, load, campaign, digest, area-links, strings, static-assert, continuation]
---

A campaign save that writes only the program's fields by name reloads a game that looks right
and then drifts: the area tree's list order decides the touch order, the engine's and the zone's
string stores decide what a string value means, and the loop's client slots (the last command,
the sequence) decide what the next packet does. Save the running state whole (every entity slot
with its links, the area lists, the string stores, the generator) and start the map again only
for what the map rebuilds. Hold it by the contract, not by field checks: save at a frame, load
into a second campaign, run both on with the same commands and compare `campaign_digest` every
frame (`se-campaign --save=20` on a real map: a death and restart on the way still matched).
`static_assert` the sizes of the plain structs walked field by field, so a field added later
fails the build at the walk instead of drifting a loaded game. A listen client resets its
command sequence on entering a map; after a load it must take the restored slot's
`last_sequence`, or the server drops its packets as old.
