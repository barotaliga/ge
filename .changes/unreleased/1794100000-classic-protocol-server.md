---
type: Added
---
Shooter's Edge serves the classic protocol, version 28, with its first tier of extensions (`games/shooters_edge/net/classic_*`). The server answers status, ping and challenge queries and checks each connect. It runs the net channel, signs clients on and sends skin downloads in chunks. Each frame it sends player records and entity deltas against the frame the client acknowledged. It verifies every move's checksum, and it can rewind players for a hitscan shot (`antilag`). The `se-net` lab measures scripted clients under every network preset. `se-interop` reports "not run" until existing programs are at hand.
