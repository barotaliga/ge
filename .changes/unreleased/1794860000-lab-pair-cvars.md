---
type: Added
title: ge lab pair draws a lab under two sets of cvars side by side
---

The labs runner takes `--cvars="<console text>"`, console commands a lab with a console runs after its own flags' cvars (`Lab::cvars()`; the world, Shooter's Edge client, world and models labs read them), and a lab given cvars it never reads fails rather than draw two runs that differ in nothing. `ge lab pair <id> --before="<cvars>" [--after="<cvars>"] [--shots=...]` (or `--switch=<cvar>`, the cvar at 0 against 1) runs the lab under each and puts every shot's two side by side in the diff's sheets, with what changed between them; `ge lab publish <id> --before=pair-before` puts the pair on one page.
