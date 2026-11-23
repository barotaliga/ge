---
type: Changed
title: Quake II's server sends downloads in pieces that fill the packet
---

A Quake II server now sends each download piece as big as the packet allows (1386 bytes on a 1390-byte packet, more for a client that asked for longer packets) instead of 1024, one for each piece the client asks for, spaced by the client's rate, so files come down about a third faster on every protocol. The piece rule lives in the classic layer and Quake's server uses it too, with its own rate-sized pieces unchanged. A server running a game with the extended limits refuses clients that cannot read them, in plain words, and reports protocol 36 in its status.
