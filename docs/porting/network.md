# Network: the protocol family, each game's protocol, our extensions

How a classic game talks over the network: the channel and info strings both classic protocols
share, each game's own messages and version, how our client joins stock servers and stock clients
join ours, and how our own improvements ride the wire without a stock end ever seeing them. The
server's behaviour is [`server.md`](server.md); recordings and TV are [`demos.md`](demos.md).

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/net/` (`transport.hpp`, `udp_socket.hpp`, `sim_network.hpp`, `capture.hpp`, `conditions.hpp`) | Sockets, the simulated network and its conditions, pcap captures: every classic trial runs on them |
| Engine | `engine/net/guard.hpp` | Anti-cheat on what a client sends: off range, off grid, flood, future views, scored |
| Engine | `engine/net/voice.hpp`, `engine/vfs/partial.hpp` | Voice, the download origin's wire body: one body every protocol carries alike |
| Engine | `engine/net/replicate.hpp`, `session.hpp`, `node.hpp` | The engine's OWN protocol (replication, sessions, up to 32 peers) for a game that needs no stock interop |
| Classic plugin | `plugins/classic/protocol/channel.hpp` | The classic net channel: sequences, reliable bit and resend, qport, rate, choke |
| Classic plugin | `plugins/classic/protocol/info.hpp` | Info strings: get, set, remove, star keys, names cleaned and unique |
| Classic plugin | `plugins/classic/protocol/wire.hpp`, `directions.hpp` | Sizes, quantization, fields, CRC-16; the sequel's 162 directions |
| Shooter kit | `plugins/shooter/usercmd.hpp`, `predict.hpp` | The client's commands and prediction (`movement.md`) |
| Shell | `games/shooters_edge/net/protocol.hpp` | The family named in the game's net namespace |
| Shell | `games/shooters_edge/net/classic_antilag.hpp`, `classic_hits.hpp` | Lag compensation for hitscan (origin history, rewinds); shots and hits per weapon |
| Shell | `games/shooters_edge/net/classic_browse.hpp` | The classic status query and master list for the engine's server list |
| Shell | `games/shooters_edge/net/classic_trial.hpp` | One classic session over the simulated network, measured (`se-net`) |

## Each game's

| | The first game | The sequel | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Wire, version, ports | `net/classic_wire.hpp`, `classic_svc.hpp`, `classic_delta.hpp` | `net/sequel_net.hpp`, `server/sequel_wire.hpp` (protocol 34) | its own (Huffman-coded, delta snapshots: a reader's facts first) | its own (peer-to-peer lockstep in the original: `engine/net/lockstep.hpp` exists) |
| Server end | `net/classic_server.hpp` | `net/sequel_net_server.hpp` | its own | its own |
| Client end | `net/classic_client.hpp`, `classic_remote.hpp` | `net/sequel_client.hpp`, `sequel_remote.hpp` | its own | its own |
| Extensions | `net/classic_ext.hpp`: the `x`, `x2`, `z` families and our `ge` set in `*ge_ext` | the `ge` set in `*ge_ext`, its own message ids in the unused range | the `ge` set by its own carrier | ours |

## Our extensions

Our improvements ride the wire as EXTENSIONS: a stock client or server never sees one, and a test
proves it. The `ge` set travels in user info and server info (`*ge_ext`), one bit a feature
(`kExtBits` in `net/classic_ext.hpp`):

| Bit | Name | What it carries |
| --- | --- | --- |
| 1 | `named-effects` | The rules' effects by name, drawn by our client's own effects |
| 2 | `aim-bend` | The aim help's bent shot |
| 4 | `download-origin` | The archive a downloaded file came from (`content.md`) |
| 8 | `other-moves` | Every other player's move state, so our client runs them forward |
| 16 | `speedjump-share` | The easy jump's share the server allows |
| 32 | `account-key` | The logins' key tag (`online.md`) |
| 64 | `move-feel` | The move's feel the server runs (`movement.md`) |
| 128 | voice | Voice messages |

The checklist for a new extension, in every game that gets it:

1. A bit in `kExtBits` (offered or not) and the code that honours it; negotiation never changes.
2. Its own message id in the game protocol's high unused range (the sequel's server messages 121
   to 127, its client message 83), never a stock id reused.
3. Sent only to an end that announced the bit.
4. A server feature the game never had defaults OFF (the voice setting `sv_voip 0`, as in the
   first game).
5. A test that a stock end gets no byte of it, and the net labs' digests rewritten in the same PR
   (protocol bytes move them).

## Adding a game

1. A reader's facts on the protocol (`porting/shooters-edge/<game>-net.md`), tagged original or
   later fork.
2. The shared family first: does the game's channel, info strings or wire match the family? Use
   it; differences are fields, the rest is the game's own `net/<game>_*` files.
3. Both ends, each measured black-box against an unmodified program in a lab (`se-interop`,
   `se-parity`): our client on its stock server, its stock client on our server.
4. Its extension carrier (the `ge` set) and, feature by feature, the checklist above.
5. Its browse query for the server list, its downloads, its logins, its TV face (`demos.md`).

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Extensions negotiated by bit, stock ends untouched | the first game's later clients and servers | `classic_ext.hpp`, both games |
| Lag compensation for hitscan | the first game's later servers | `classic_antilag.hpp` |
| Chunked downloads into archives | the first game's later protocol | `content.md` |
| Moves sent between the 10 Hz frames | ours, for the sequel | the sequel's server, by `other-moves` |

| Drawback left behind | How |
| --- | --- |
| A stock client sees nothing new, so nothing improves for it | our client asks by bit and gets the improvement; the stock one plays as it always did |
| The sequel's 10 Hz frames as the only clock | the frames stay at 10 Hz for stock clients; our client gets moves between them |
| Downloads lost when a connection drops | kept in a partial archive, resumed |

## Gaps

- Prediction and the inventory on a joined sequel server (#1112), a per-client PVS cut, the
  normals table for a spark's direction (the sequel's effects carry no surface normal).
- The third game's protocol; the sector family's lockstep.

## Labs and tests

`se-net`, `se-sequel-net`, `se-interop`, `se-parity`, `se-remote`, `se-reach`, `net-link`, `cheats`;
`tests/shooters_edge_classic_wire_test.cpp`, `tests/shooters_edge_sequel_net_test.cpp`.

## Lessons

- Lift a feature's logic into a game-neutral core and make each game's server a thin wrapper that
  supplies only what differs (its protocol ids, what a team is): voice and `say_near` reached the
  sequel that way.
- Adding user info grows the connect packet and moves the net labs' digests: prove that is the
  only cause by taking the line out and running the labs again.
- Check what reaches the rules layer: a server that passes part of the user info breaks features
  silently.
