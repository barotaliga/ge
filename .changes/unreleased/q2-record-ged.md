---
type: Added
title: games on Quake II's maps record as .ged
---

`record <name>` on one of Quake II's maps writes `<name>.ged`: Quake II's own recording from the player's view, written from the rules' state by the server-side encoder (`games/shooters_edge/server/q2_wire.hpp`), packed into the replay file. A `.dm2` unpacks from it byte for byte.
