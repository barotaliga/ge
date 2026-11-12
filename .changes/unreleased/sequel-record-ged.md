---
type: Added
title: games on the sequel's maps record as .ged
---

`record <name>` on one of the sequel's maps writes `<name>.ged`: the sequel's own recording from the player's view, written from the rules' state by the server-side encoder (`games/shooters_edge/server/sequel_wire.hpp`), packed into the replay file. A `.dm2` unpacks from it byte for byte.
