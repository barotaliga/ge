---
type: Added
title: The helicopter's pilot says why it flies so
---

`heli_pilot` takes an optional `ai::Why*`, last and null by default. Handed one, it records two decisions every step:

- `way`: the speed it holds, as the slowest of four limits (the cruise, clearing the rise ahead, stopping at the aim, closing on it).
- `height`: landed, settle, climb, sink or hold, with the height over the aim and over the snow ahead.

`run_heli_scenario` passes a trace through. On the free ride, the heli outing records an `outing` decision (board, fly, drop or ski down) above the pilot's, and `SimOptions::why` traces the player's pilot in `simulate_heli_outing`. Every digest is unchanged, traced or not.
