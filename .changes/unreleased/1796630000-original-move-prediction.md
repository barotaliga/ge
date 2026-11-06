---
type: Added
title: The original movement predicted on a client
---

A client joined to a server that plays the original movement (`sv_playermove original`) now predicts its own player with that movement instead of the arena's, so what it shows matches the server. The `se-loop` lab's `--move=original` plays its scripted match on the original movement with the clients predicting it.
