---
title: A proven map's intent starts as a draft from its items; bots on it read rough
date: 2026-11-15
scope: plugins/shooter/intent_draft.cpp, games/shooters_edge/labs/map_intent_lab.cpp, games/shooters_edge/labs/gpu_map_overview_lab.cpp
concepts: [intent, draft-intent, reference-maps, proven-maps, map-intent, flood-fill, slice]
---

Writing eleven proven maps' intents by hand from their pictures was slow and unrepeatable, and
the overview has no coordinate labels to read boxes off. `--intent=auto` drafts one: each armour,
power and key weapon's room found by flooding the open cells of `slice_map` at the item's height
(capped at 352 units each way), rooms that overlap by 60 % merged under the stronger intent. A
room that opens wide (dm6's launcher hall) spills into its neighbours up to the cap: edit the box.
The draft knows nothing of a map's middle or its routes; those are the study's to add.

Bots on a built map (`map-intent --game= --map=`) read rougher than on our plans: on dm6 one bot
held the red armour while the other stuck in the ring's corridor, 0 to 1 in three minutes. Judge
a proven map's areas from people's demos (`--demos=`) before trusting the bots' table there.
