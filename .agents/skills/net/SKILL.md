---
name: net
description: "Use for engine/net in gameengine2: multiplayer networking. Addresses, the wire, UDP and TCP sockets, the simulated network (stream links too) and its conditions, pcap captures, the packet format, connections (acks, round trip, loss, unreliable/sequenced/reliable channels, big messages in pieces, priorities, the send budget), nodes (host, client, peer; up to 32), sessions (the lobby: seats, teams, chat, kick, start, reconnect by token), LAN discovery, NATs (the simulated kinds, STUN, the rendezvous, hole punching, the relay), fake-traffic sessions, the net-link, net-session and net-reach labs; and the M17 plan (lobby, chat, NAT, replication, lockstep, voice, accounts)."
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
| `net/session.hpp` | Sessions: the lobby on a node's reliable channel; `SessionHost` seats players and holds the rules, `SessionClient` asks and mirrors |
| `net/session_wire.hpp` | The session's messages, stated once for both sides (not installed) |
| `net/discovery.hpp` | LAN discovery: a broadcast probe on a fixed port, each host's advert, the hosts found |
| `net/stun.hpp` | STUN's Binding request and answer: the public address a NAT gave a socket |
| `net/rendezvous.hpp` | The public server: sessions registered, a client introduced to its host with a nonce and a relay port, STUN answered, relays forwarding |
| `net/reach.hpp` | The two ends on the node's socket: the host registers and punches, the client joins, connects, follows a punch's port (`node_retarget`), falls back on the relay; `reach_wire.hpp` the messages (not installed) |
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

- **A session's rules are the host's.** A client only asks; every rule (seats, team caps, the
  chat bucket, the leader, kicks and bans) is decided in `session_host.cpp` and answered with
  `refused` and why. Add a rule there and its `SessionWhy`, never in the client.
- **A player is a seat, not a peer.** Code that keys anything per player uses the seat
  (`session_seat_of_peer`); a reconnect gives the same seat a new peer slot.
- **A script acts as the leader the links chose.** On a dedicated server the first SEATED
  player leads, and jitter decides who that is: a lab or test asks the clients' mirrors who
  leads (`players[me].leader`) rather than assuming the first to join.
- **A NAT is the simulated network's, a path is reach's.** NAT behaviour (mapping, filtering,
  expiry, forwards) lives in `sim_network.cpp`'s `map_out` / `map_in` and nowhere else; which
  path a pair should take follows from RFC 4787 (a hole opens unless one side is symmetric and
  the other port-restricted or symmetric), and the `net-reach` lab and `net_reach_test` hold every
  pair to it. The rendezvous never answers bigger than it was asked: pad every new ask.

## Labs and tests

- `make lab LAB=net-link` (`--presets=bad,awful --clients=32 --seconds=30`; `--transport=tcp`
  makes every link a stream, `--mix=download` adds 64 KB messages checked whole, `--budget` caps
  every connection): fake traffic under
  each preset; the table (joined, rtt beside the model's range, loss beside the configured,
  reliable events delivered and in order, latencies, delivery of snapshots and inputs, the host's
  bandwidth, resends), the round trip and loss charts, the reliable latency boxes, and one
  preset's session as `previews/net-link/<preset>.pcap` (opens in any packet tool).
- `make lab LAB=net-session` (`--presets=bad,awful --clients=32`): a dedicated host's lobby
  filled and scripted under each preset (joins, teams, three chat scopes and a flood, a kick and
  its ban, a six-second cut and the return to the seat, settings, start, return); the table
  checks every chat copy, every roster, the kick, the ban and the seat, the seats are charted
  and the pings boxed.
- `make lab LAB=net-reach` (`--presets=lan,wifi,bad`): every pair of NAT kinds (none, full cone,
  restricted, port-restricted, symmetric) reaching a host through the rendezvous: the path each
  took against the one predicted, its connect time, a reliable message each way; STUN from
  behind each kind; the connect times as a heatmap.
- `ge net` (`build/debug/ge net ...`; `engine/cli/net.hpp`): `inspect <pcap>` decodes every
  datagram, `stats <pcap>` measures each connection from the capturing node (rtt, loss each way,
  dups, reorders, resends) and charts its kbps, rtt and loss, `sim` runs the same sessions as the
  lab from the command line (`--presets`, `--mix`, `--link=latency=80,loss=3%`, `--capture`,
  `--transport=tcp`, `--budget`).
  Read a lab's capture with `ge net stats previews/net-link/bad.pcap` before guessing.
- Tests: `tests/net_*_test.cpp`, `tests/cli_net_test.cpp`. `net_tcp_socket_test` runs frames
  and two nodes (a 64 KB message) over real loopback TCP. `net_traffic_test` holds the guarantees under every preset;
  `net_connection_test` holds the stream's rules on a scripted wire; `net_node_test` the
  handshake, the 32-peer limit, denial and timeouts; `net_session_test` every session rule;
  `net_discovery_test` probes and adverts on the simulated network and real loopback UDP;
  `net_sim_network_test` each NAT kind's filter, expiry and forwards; `net_stun_test` the RFC's
  layout; `net_reach_test` every pair of kinds, an unknown session and a missing relay.
- Before and after a change here: `make lab LAB=net-link ARGS=--tag=before`, then
  `--compare=before`, both in the PR.
