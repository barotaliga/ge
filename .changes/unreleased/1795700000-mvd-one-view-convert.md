---
type: Added
title: A multiview demo converts to one player's view for the oldest clients
---

`shooters_edge_cli demos convert <name>.mvd <file>` now writes a plain client recording of one player's view (the lowest playing slot) that the oldest multiplayer clients play. The stream's view can be chosen or switched at any frame (`convert_set_pov`): each start gives a whole signon and a whole frame, so a broadcast converts a view only while someone watches it.
