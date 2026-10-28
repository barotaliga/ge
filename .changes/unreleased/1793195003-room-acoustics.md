---
type: Added
title: Rooms that sound like their size
---

The synth gains a room (`Synth::room`, `audio/reverb.hpp`): early echoes off the nearest walls and a damped feedback-delay tail that glides from room to room and fades the outdoor echo as the walls close in. `audio/room.hpp` probes the room round the listener with 64 rays through the game's own ray callback (a BSP, brushes, a heightfield) and turns its mean free path, open share, absorption and Eyring decay into that room. Lab: `room`.
