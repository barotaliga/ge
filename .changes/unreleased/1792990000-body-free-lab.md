---
type: Added
title: The free ride's body table and the INJURIES switch on the headless run
---

The headless free ride takes the INJURIES switch (`SimOptions::injuries`), and its report carries what the player's body took: the injuries, the severity score, the hardest blow and the time of death. A dead player ends the run. The `free` lab skis every piste again with the switch on and prints the body table. It fails a run that he lived through but that moved with the switch on.
