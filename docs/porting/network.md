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
| Classic plugin | `plugins/classic/protocol/wire.hpp`, `directions.hpp` | Sizes, quantization, fields, CRC-16; Quake II's 162 directions |
| Engine | `engine/net/http_fetch.hpp` | An HTTP GET for any game: https, proxy (a tunnel for https), redirects, chunks, failure words |
| Engine | `engine/net/tls_stream.hpp` | TLS 1.2 and 1.3 over caller-carried bytes (Mbed TLS): the system's roots, the name checked, a test server |
| Classic plugin | `plugins/classic/content/web_downloads.hpp` | Downloads from a server's web address or a GitHub repository, several at once, file lists, the channel behind: Quake II's `dlserver`, our QuakeWorld server's `*dlserver`, the `cl_http_*` settings in the classic layer |
| Shooter kit | `plugins/shooter/usercmd.hpp`, `predict.hpp` | The client's commands and prediction (`movement.md`) |
| Shell | `games/shooters_edge/net/protocol.hpp` | The family named in the game's net namespace |
| Shell | `games/shooters_edge/net/qw_antilag.hpp`, `qw_hits.hpp` | Lag compensation for hitscan (origin history, rewinds); shots and hits per weapon |
| Shell | `games/shooters_edge/net/qw_browse.hpp` | The classic status query and master list for the engine's server list |
| Shell | `games/shooters_edge/net/qw_trial.hpp` | One classic session over the simulated network, measured (`se-net`) |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Wire, version, ports | `net/qw_wire.hpp`, `qw_svc.hpp`, `qw_delta.hpp` | `net/q2_net.hpp`, `server/q2_wire.hpp` (protocol 34) | its own (Huffman-coded, delta snapshots: a reader's facts first) | its own (peer-to-peer lockstep in the original: `engine/net/lockstep.hpp` exists) |
| Server end | `net/qw_server.hpp` | `net/q2_net_server.hpp` | its own | its own |
| Client end | `net/qw_client.hpp`, `qw_remote.hpp`; Quake's own protocol 15 (a server on port 26000) by `net/q1_remote.hpp` | `net/q2_client.hpp`, `q2_remote.hpp`; protocols 35 and 36 read by `net/q2_proto_in.hpp`, which rewrites each message into protocol 34 so one reader (`client/demo_q2.cpp`) serves every server and a recording stays 34 | its own | its own |
| Extensions | `net/qw_ext.hpp`: the `x`, `x2`, `z` families and our `ge` set in `*ge_ext` | the `ge` set in `*ge_ext`, its own message ids in the unused range | the `ge` set by its own carrier | ours |

## Our extensions

Our improvements ride the wire as EXTENSIONS: a stock client or server never sees one, and a test
proves it. The `ge` set travels in user info and server info (`*ge_ext`), one bit a feature
(`kExtBits` in `net/qw_ext.hpp`):

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
| 256 | `every-frame` | Every rules frame of Quake II's server at `sv_fps` past 10, where a stock client gets the tenths (`server.md`) |

The checklist for a new extension, in every game that gets it:

1. A bit in `kExtBits` (offered or not) and the code that honours it; negotiation never changes.
2. Its own message id in the game protocol's high unused range (Quake II's server messages 121
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
| Extensions negotiated by bit, stock ends untouched | Quake's later clients and servers | `qw_ext.hpp`, both games |
| Lag compensation for hitscan and missiles (levels 1 and 2, the cap, the opt-out) | Quake's later servers; Quake II's modern servers (the cap, the box, the opt-out, `sv_antilag_interp`) | `qw_antilag.hpp` |
| Chunked downloads into archives | Quake's later protocol | `content.md` |
| Downloads from a web address, a GitHub repository too | Quake II's modern clients | `web_downloads.hpp`, both games |
| Moves sent between the 10 Hz frames | ours, for Quake II | Quake II's server, by `other-moves` |

| Drawback left behind | How |
| --- | --- |
| A stock client sees nothing new, so nothing improves for it | our client asks by bit and gets the improvement; the stock one plays as it always did |
| Quake II's 10 Hz frames as the only clock | the frames stay at 10 Hz for stock clients; our client gets moves between them |
| Downloads lost when a connection drops | kept in a partial archive, resumed |

## Gaps

- Prediction and the inventory on a joined Quake II server (#1112), a per-client PVS cut, the
  normals table for a spark's direction (Quake II's effects carry no surface normal).
- The third game's protocol; the sector family's lockstep.
- A Quake server of protocol 15 is joined without prediction: the window draws what it is sent,
  as a demo of that protocol plays (`net/q1_remote.hpp`; the transport's facts are
  `porting/shooters-edge/netgame-transport.md`).

## Labs and tests

`se-net`, `se-q2-net`, `se-interop`, `se-parity`, `se-remote`, `se-reach`, `net-link`, `cheats`;
`tests/shooters_edge_qw_wire_test.cpp`, `tests/shooters_edge_q2_net_test.cpp`.

## Lessons

- Lift a feature's logic into a game-neutral core and make each game's server a thin wrapper that
  supplies only what differs (its protocol ids, what a team is): voice and `say_near` reached the
  Quake II that way.
- Adding user info grows the connect packet and moves the net labs' digests: prove that is the
  only cause by taking the line out and running the labs again.
- Check what reaches the rules layer: a server that passes part of the user info breaks features
  silently.
