---
type: Added
title: The cheats lab refuses forged and replayed packets and never leaks hidden players
---

The guard trial now runs on sealed connections with interest. A replayer, a forger and a fisher join the cheaters. The server refuses every forged or replayed packet before it reaches the game, and counts them for each peer (`Peer::forged`, `Peer::replayed`). No player is ever sent a player out of its sight. The `cheats` lab shows both on every link.
