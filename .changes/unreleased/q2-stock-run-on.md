---
type: Added
title: A stock Quake II server's players run on from its frames
---

Joined to a stock server of Quake II, which sends only its 10 Hz frames, the window now draws the other players run on from the newest frame at the speed the last two frames show, to the server's present (at most 100 ms, never through a wall), instead of a frame and a round trip behind. `cl_predict_players 0` turns it off.
