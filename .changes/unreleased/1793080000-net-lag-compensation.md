---
type: Added
title: Lag compensation judged at the drawn moment, capped
---

Each input now carries the moment it was drawn on the client: `replicate_client_input` takes when within the frame the player pressed. The server keeps that moment with the input, and `replicate_server_rewind_for` rewinds any entity to it, so a hit is judged where the shooter saw the target. The rewind is capped by `rewind_max_s` (the `sv_rewindmax` cvar, 0.35 s by default), and a shot drawn further back is refused. A hit is an event (`kHitEvent`) the client shows at once and the server confirms. `racing::crossing_share` orders two finishes inside the same step by when in it each crossed the line. In the `net-predict` lab's shots at 72 Hz, a shot lands within 1 cm of where it was drawn on every link up to mobile, against 17 to 35 cm before.
