---
title: The protocol family's names reach the game's net code through net/protocol.hpp
date: 2026-11-14
scope: plugins/shooter/protocol/, games/shooters_edge/net/protocol.hpp
concepts: [protocol-family, wire, net-channel, info-string, using-declarations, move-to-plugin]
---

Moving the wire, channel and info strings into `ge::shooter::protocol` would have meant
qualifying about 270 calls across 45 files that other threads edit. Instead one game header,
`games/shooters_edge/net/protocol.hpp`, names them in `shooters_edge::net` with using-declarations,
and the moved headers' includers include it: no call site changed. A new name added to the
plugin's protocol headers is added to that list too, or the game's files will not see it.
