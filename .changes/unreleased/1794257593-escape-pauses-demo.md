---
type: Fixed
title: Escape pauses a played demo behind the menu, and no longer crashes
---

In Shooter's Edge, Escape during a played demo now pauses it and opens the main menu with the demo's frame behind it, instead of dropping back to the title; Escape again plays on. Stopping a demo (or its sounds' folder changing) no longer frees its recordings while the audio thread is still fading them, which crashed the window. The synth says when a recording may be freed (`Synth::reads`).
