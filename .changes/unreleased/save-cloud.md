---
type: Added
title: Saves on every device
---

`save::sync` keeps a save store's slots in step with the player's cloud: each slot compared with the cloud's and with what both held at this device's last sync, a manifest in the cloud so a sync reads one small file, a slot changed on two devices won by the one played longer and the other kept as `<slot>.conflict` on every side, and removals followed. `services::SaveCloud` puts a store's cloud files (the simulated, offline or a real store's) behind it. The `save-cloud` lab.
