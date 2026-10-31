---
title: Check a lossy text save by its reread and the game's end state, and read the box from the program
date: 2026-10-31
scope: games/shooters_edge/server/classic_save.cpp, games/shooters_edge/labs/se_campaign_lab.cpp
concepts: [classic-save, text-save, round-trip, absmin, mirror, renumber, end-state]
---

A text save that keeps only the program's fields cannot hold the digest contract: the loaded
game parts at the first frame (no random stream, no area-list order, no string store). Hold it
two other ways: written, read back and written again must give the same text (every value's
text reads back to its bits: the shortest `%.*f` that `strtof` returns exactly), and the loaded
game walked on beside the original must end the same game (`se-campaign --save=20`: 0 faults,
the player 0 units apart, the same entities live). On load, decide whether an entity was linked
from the VM's `absmin`/`absmax` fields as the text set them, not the loop's copies: the mirror
copies the loop's box out only after a link, so after the reset the loop's box is zero for
every entity and nothing would be linked. A save numbered by a one-player engine is spotted by
a non-player block below the first dynamic slot; shift every entity value from 2 up with it.
