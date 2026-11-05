---
type: Added
title: Co-op in the campaign, with companion bots
---

Shooter's Edge's campaign plays in coop: `coopgame [bots] [novice|average|expert] [map]` starts a coop game (teamplay on, so players never harm each other) with up to three companion bots, and `addbot` and `removebot` change the party during it. A companion plays beside you rather than for you: it keeps at your shoulder, steps out of your way, kills only what comes for you or itself, holds its fire while you are in the line, presses the button you stop by, catches up when left behind, and while you wait it spins, looks at items, glances at you, hops round you, jumps and sways. The shooter plugin's senses take a game's marked non-player foes (monsters) as contacts, and `plugins/shooter/idle.hpp` is the idle repertoire any waiting bot can use. Lab `se-coop` charts it.
