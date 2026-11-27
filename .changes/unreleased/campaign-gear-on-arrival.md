---
type: Added
title: Campaign bots can enter each map with the gear a playthrough brings
---

`se-q1-campaign --gear=arrival` and `se-q2-unit --gear=arrival` start each map with the weapons, ammunition and armour a player would carry into it: every pickup the earlier maps of the episode (Quake, Scourge of Armagon, Dissolution of Eternity) or of the campaign (Quake II, The Reckoning, Ground Zero) place, taken by the game's own pickup rule, less the ammunition their monsters cost to kill and a further 20%. Quake's gear reaches the map by the program's own level change. `se-q2-unit --each=1` plays every map of a Quake II campaign on its own, in the order a playthrough first enters them.
