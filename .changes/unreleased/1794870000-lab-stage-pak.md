---
type: Added
title: Labs stage a fixture pack of stand-in players with --stage-pak
---

The labs runner takes `--stage-pak=<name>`, a pack a game's labs stage over the files they read (`Lab::stage_pak()`), filling only what the game folder lacks; a lab given a name it never stages fails. Shooter's Edge's `fixture` (`client/stage.hpp`) stages stand-in players, heads, rockets, grenades and gibs made from the morph fixture body, so `se-highlights`, `se-demo-video`, `se-timedemo` and `se-client` draw a demo's players without the classic pack.
