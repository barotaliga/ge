---
type: Fixed
title: The dedicated server moves on to the next map when a match ends
---

When a match on `sesv` ends (its time or frag limit, then the intermission), the server now loads the next map and takes every connected player along: each one is told the map is changing and signs on again. `sv_maprotation` picks the next map when it is set; otherwise the mod's choice stands. Before, the server stayed in the intermission for good. A map that does not load leaves the server on the one it was playing, and the status line says why. The classic client now goes back through the signon when the server says to reconnect.
