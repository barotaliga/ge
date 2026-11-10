---
type: Added
title: Shooter's Edge on a joined server draws the other players and missiles smoothly
---

On a server joined with `connect`, the other players are run ahead from their last command to the drawn time every window frame, so they move between the server's frames, and missiles, movers and gibs are drawn between the last two frames received. `cl_predict_players 0` and `cl_nolerp 1` turn each off; `netstatus` reports them and the round trip. The new `se-remote` lab charts it off and on.
