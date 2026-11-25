---
title: A server-side client (the recorder's spectator) is a slot stage of its own, never an active slot without a connection
date: 2026-11-22
scope: games/shooters_edge/net/q2_mvd_dummy.cpp
concepts: [mvdspec, dummy, slot-stage, q2-slot, recorder, capture]
---

Quake II's recorder keeps a client that has no connection. Marking it `Q2Slot::active` would send
it frames, time it out, list it in `status` and count it in the browser; every loop over the
slots would need a guard. It is `Q2Slot::dummy` instead: not `q2_held`, so the send, read and
timeout loops pass it by, and only the few loops that test `!= free` (admission's counts, the
address matches, `status`) need it named. It takes a slot past `q2_net_slots` first, so it costs
no player a seat.

Whether its scoreboard is up is its layouts stat (13), not its layout string: our rules keep the
last layout text after `putaway` and clear only the screen, so a test or a timer reading the
string sees a scoreboard that is gone.

A view of no one in a multiview recording follows the lowest player that is not the recording's
observer; any count of players on a relay (`mvdchannels`, the chooser, the player list) skips the
observer through `playing()`.
