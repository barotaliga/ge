---
type: Added
title: Starter maps, one a mode
---

Four starter maps for Shooter's Edge beside The Keeps (4on4), each a plan in `games/shooters_edge/content/maps/` built with `secli map build`: The Cistern (duel), The Twin Forge (2on2), The Gallery (free-for-all) and Updraft (midair). Every trick a plan writes is made on the move: the `map-intent` lab now calculates a plan's rocket jumps (flown only when no arc lands), says where a missed arc came down, proves a head jump off a teammate standing under the jumper (`prove_head_jump`), and counts a jump that lands on the target's floor when the rest is a walk.
