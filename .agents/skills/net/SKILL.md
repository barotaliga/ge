---
name: net
description: "Use for engine/net in gameengine2: multiplayer networking. Addresses, the wire, UDP and TCP sockets, the simulated network (stream links too) and its conditions, pcap captures, the packet format, connections (acks, round trip, loss, unreliable/sequenced/reliable channels, big messages in pieces, priorities, the send budget), nodes (host, client, peer; up to 32), fake-traffic sessions, the net-link lab; and the M17 plan (lobby, chat, NAT, replication, lockstep, voice, accounts)."
---

# Networking

`engine/net/` (`ge_net`, namespace `ge::net`) is how games on the engine play together. The
design and the plan are [`docs/networking.md`](../../../docs/networking.md); the milestone rows
are `docs/goals.md` M17a–M17k. Everything here is generic: peers, channels, messages, never a
skier. A game's half (what it replicates, its message ids, its lobby's settings) lives in
`games/<game>/net/`.

| File | What |
| --- | --- |
| `net/address.hpp` | IPv4, IPv6 and simulated (`sim:<node>:<port>`) addresses; parse, print, hash |
| `net/wire.hpp` | The bounded byte writer and reader, varints, 16-bit sequence arithmetic |
| `net/os_socket.hpp` | The socket shim (POSIX, Winsock): the only file with the OS's socket headers; not installed |
| `net/udp_socket.hpp` | The OS's non-blocking UDP sockets; `os_entropy` |
| `net/tcp_socket.hpp` | The OS's non-blocking TCP sockets: listen, accept, connect, packets as length-prefixed frames |
| `net/conditions.hpp` | A link's conditions and each packet's fate (a `stream` link: nothing lost, a drop a resend late, head-of-line blocking); the presets `lan` … `awful` |
| `net/sim_network.hpp` | The simulated network: conditioned directed links on a virtual clock, seeded |
| `net/capture.hpp` | pcap captures (link type RAW; sim nodes as 198.18.0.0/15), written and read back |
| `net/transport.hpp` | One send/receive over a UDP socket, TCP streams (a host's, or a client's one) or the simulated network, captured |
| `net/packet.hpp` | THE WIRE FORMAT, stated once: packet types, the payload header, messages, `describe_packet` |
| `net/connection.hpp` | One connection: sequences, acks, RTT (RFC 6298), loss, the three channel kinds, big messages in pieces (`largest`), `priority`, `budget_kbps`, `LinkStats` |
| `net/node.hpp` | Host, client or peer: the challenge handshake, up to 32 peers, timeouts, events, flush |
| `net/traffic.hpp` | Fake traffic: a whole session over the simulated network, measured end to end |

## Rules

- **No clock, no OS randomness inside.** Every call takes `now_us` from the caller; salts and
  secrets come off a `NodeConfig::seed` (a real game passes `os_entropy()`). That is what makes a
  session over the simulated network a pure function of its seed: the labs' digests and the
  tests depend on it. `make libm-check` scans `engine/net`.
- **The wire format is `packet.hpp`'s.** A change to a packet's layout changes every capture's
  meaning and `describe_packet`; change both together and say so in the PR.
- **Nothing allocates after `node_init`** except the event buffer's rare growth; every ring is
  sized up front (`kSentRing`, `kReliableInFlight`, `kReliableSpan`).
- **Verify on the simulated network first.** A real socket test (`tests/net_udp_socket_test.cpp`)
  proves the OS path; everything else is proven under the presets, headless.
- **Big messages are a reliable channel's, and configured.** Set `ChannelConfig::largest[ch]`
  (up to `kMaxBigMessage`, 64 KB) on a reliable channel; each connection keeps a buffer that size
  for the channel to join pieces in. Never raise `kMaxMessage` or send pieces by hand. A
  message's pieces take that many reliable slots at once: a 64 KB message needs 56 of the 128.
- **Priorities change the packing only when room is short.** With every priority equal the
  order is the order queued, packet for packet, so adding the fields moved no digest; keep it so.
- **A node polls once a tick.** The round trip it measures includes each side's wait for its
  next update: the `net-link` lab's `want` column states the model. Poll faster (or update
  before and after the step) to shrink it, never subtract it in the stats.

## Labs and tests

- `make lab LAB=net-link` (`--presets=bad,awful --clients=32 --seconds=30`; `--transport=tcp`
  makes every link a stream, `--mix=download` adds 64 KB messages checked whole, `--budget` caps
  every connection): fake traffic under
  each preset; the table (joined, rtt beside the model's range, loss beside the configured,
  reliable events delivered and in order, latencies, delivery of snapshots and inputs, the host's
  bandwidth, resends), the round trip and loss charts, the reliable latency boxes, and one
  preset's session as `previews/net-link/<preset>.pcap` (opens in any packet tool).
- `ge net` (`build/debug/ge net ...`; `engine/cli/net.hpp`): `inspect <pcap>` decodes every
  datagram, `stats <pcap>` measures each connection from the capturing node (rtt, loss each way,
  dups, reorders, resends) and charts its kbps, rtt and loss, `sim` runs the same sessions as the
  lab from the command line (`--presets`, `--mix`, `--link=latency=80,loss=3%`, `--capture`,
  `--transport=tcp`, `--budget`).
  Read a lab's capture with `ge net stats previews/net-link/bad.pcap` before guessing.
- Tests: `tests/net_*_test.cpp`, `tests/cli_net_test.cpp`. `net_tcp_socket_test` runs frames
  and two nodes (a 64 KB message) over real loopback TCP. `net_traffic_test` holds the guarantees under every preset;
  `net_connection_test` holds the stream's rules on a scripted wire; `net_node_test` the
  handshake, the 32-peer limit, denial and timeouts.
- Before and after a change here: `make lab LAB=net-link ARGS=--tag=before`, then
  `--compare=before`, both in the PR.
