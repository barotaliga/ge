# Networking

How games on this engine play together over a network: the design `engine/net` is built to,
the decisions behind it, and the order it lands in (`docs/goals.md` M17). Where a part is not
built yet, this page is the plan. Change it in the same PR that changes the plan.

## What a game asks of the network

The engine serves every kind of multiplayer game a game on it may be, so the design starts from
the shapes those games take, not from one of them:

| Shape | Typical game | Players | Transport | Who decides | What crosses the wire |
| --- | --- | --- | --- | --- | --- |
| **Fast action** | an arena shooter, a survival sandbox, an open-world crime game | 2–32 a server | UDP | the server | the inputs up, snapshots (deltas) down; the client predicts its own body and interpolates everyone else |
| **Racing** | a circuit racer, a downhill race, a free ride on one mountain | 2–32 | UDP | the server (or the host) | the same as fast action; the field's standings are the server's |
| **Action RPG** | a dungeon crawler with a party | 2–8 a game | TCP or UDP | the server | commands up, world events down, reliable and ordered; little prediction |
| **Persistent world** | an MMO zone, a survival server that runs for weeks | up to the game's max a session, many sessions | TCP or UDP | a dedicated server | commands up, what is near you down (interest), the world saved to disk |
| **Turns and lockstep** | a 4X strategy game, an RTS | 2–8 | TCP or UDP | nobody: every peer runs the same deterministic step | inputs (orders) only, a turn at a time; a digest to catch a desync |
| **Rollback** | a short race or a fight between few players | 2–4 | UDP | every peer, predicted and corrected | inputs only; a peer rolls back and resimulates when a late input arrives |

What every one of them needs, and so what the engine owns:

- **A link**: send datagrams to an address and receive them, over a real UDP socket, a TCP stream
  or a simulated network, behind one interface.
- **Connections**: a handshake that a spoofed address cannot complete, keepalives, a timeout,
  a clean disconnect, and a game-set maximum of peers (at most 32).
- **Channels**: unreliable (a snapshot, a voice frame), sequenced (only the newest counts: an
  input), reliable and ordered (chat, a lobby change, an order in a turn).
- **Numbers**: round-trip time, jitter, packet loss, bandwidth each way, resends; on every link,
  every second, for the HUD's ping and for the tools.
- **Sessions round the game**: a lobby (players, ready, teams, the game's settings), chat (to
  all, to a team, privately), a scoreboard's pings, kicking, reconnecting.
- **Reaching a host**: open a port on the router (UPnP, NAT-PMP/PCP), learn the public address
  (STUN), punch through two NATs, relay when nothing else works, find a game on the LAN.
- **Replication**: the server-authoritative model (snapshots, deltas, prediction,
  interpolation, lag compensation, interest) and the lockstep model (inputs, turns, digests,
  rollback). The engine's determinism (G1) is what makes lockstep and rollback cheap here: two
  machines running the same step on the same inputs get the same bits.
- **Voice**: capture, encode (Opus), send unreliably, a jitter buffer, loss concealment, mixed
  into the synth; to all, a team, one player, or by distance.
- **Accounts**: sign up and log in (a password hashed with Argon2id, or a store's sign-in linked
  to it), a session token the game's hosts trust, a friends list with requests, presence (online,
  in a lobby, in a game) and invites to a lobby. They need a service that keeps them: the
  engine ships it as an executable built from this tree (`ge-service`: accounts, friends,
  presence, the matchmaker that signs connect tokens, the rendezvous and the relay), which a
  game's owner runs on a server of their own; nothing is hosted for them, and its keys come
  from the environment, never the tree (M17j).
- **Tools**: everything above is verified headless, on a GPU-less Linux machine, by a simulated
  network and by labs and `ge` commands that read what crossed the wire.

## The layers

```
games/<game>/net          what this game replicates, its messages, its lobby's settings
        │
engine/net/replicate      snapshots and deltas, prediction, interpolation, interest (M17f);
engine/net/lockstep       turns, input delay, desync digests, rollback (M17g)
        │
engine/net/voice          Opus frames, the jitter buffer, voice routing (M17h)
engine/net/session        lobby, teams, chat, private messages, the player list (M17d)
engine/net/reach          UPnP, NAT-PMP/PCP, STUN, hole punching, relay, LAN discovery (M17e)
        │
engine/net  node          connections: handshake, keepalive, timeout, peers up to 32
            channel       unreliable, sequenced, reliable-ordered messages; acks; resends
            packet        the wire format: header, acks, messages; decoded for the tools
            transport     one interface over a UDP socket, a TCP stream, a simulated network
            sim_network   the simulated network: every packet's fate off a seeded stream
            conditions    latency, jitter, loss and its bursts, duplicates, reorder, bandwidth
            capture       pcap files any packet tool opens, read back by `ge net`
            udp_socket    the OS's sockets (POSIX and Winsock): the only file that includes them
            address, wire IPv4, IPv6 and simulated addresses; the byte writer and reader
        │
engine/core               the seeded stream, output
```

Everything in `engine/net` is generic: it speaks of peers, channels, messages, teams and
snapshots, never of a skier. A game's half (what it replicates, its message ids, its lobby's
settings) lives in `games/<game>/net/`. A genre's shared half (a racing field's standings over
the wire) would be a plugin's.

## Decisions

| Part | Decision | Why |
| --- | --- | --- |
| Own protocol or a library | Our own, small, over UDP | The pieces are small and well understood (one header of acks, three channel kinds, a challenge handshake); a library (ENet, GameNetworkingSockets, yojimbo) brings its own allocator, threads, clock and logging, none of which a deterministic simulated network can drive. Ours runs on a virtual clock, so the same seed is the same packets, and every lab is a determinism test |
| UDP and TCP | UDP is the default; a TCP stream transport carries the same packets length-framed (M17c) | A fast game cannot wait for a lost packet's resend; a turn-based game, a chat-heavy world or a network that blocks UDP can. The layers above the transport do not care which |
| The clock | The caller passes `now_us` into every call; nothing in `engine/net` reads a clock | The simulated network runs on a virtual clock and a real game on the frame's; the same code, and a test is exact |
| Randomness | Salts and the simulated network's fates come from `core::Rng` streams the caller seeds; a real game seeds them from the OS's entropy (`os_entropy`) | A simulated session replays to the bit; a real one is not guessable |
| Allocation | Fixed-capacity rings sized at create time (peers, the sent-packet ring, the reliable windows) | Nothing allocates in `node_update` once a node is made (G2) |
| Packet size | At most 1200 bytes on the wire (`kMaxPacket`) | Fits the smallest MTU on the paths games use (IPv6's 1280 less its headers) with no fragmentation by IP; bigger messages are fragmented by the engine (M17c) |
| Acks | Every payload carries its sequence, the newest sequence received and 32 bits of the ones before it | One packet's loss is told by the next few, with no ack packets of their own; the round-trip time comes off the same acks |
| Reliability | Per message, not per packet: a reliable message rides in packets until one carrying it is acked, resent once a round trip has passed | The newest data goes in every packet; nothing waits behind a lost packet but the reliable messages it carried |
| Handshake | Request → challenge (a cookie only the host can make) → response → accept, all packets padded to the request's size | A spoofed address never gets past the challenge, the host keeps no state for it, and a reply is never bigger than the request (no amplification) |
| Peers | A node is both host and client: it may accept and it may connect (`NodeConfig`) | Client-server, a listen server and a peer-to-peer mesh (lockstep, rollback) are the same code; the game sets the maximum, 1 to 32 |
| Encryption | AEAD over every payload with keys from a connect token a matchmaker signs (M17i); off until then | A LAN game and the labs need none; an internet game does. Keys come from the environment or GitHub secrets, never the tree |
| Capture format | pcap (libpcap's format) with IPv4/UDP headers made up for simulated addresses | Every packet tool opens it, and `ge net` decodes our protocol inside it |
| Accounts | Our own service executable, self-hosted, over the same transport (reliable channel over TCP or UDP); passwords stored only as Argon2id hashes; a login returns a signed session token a game host checks without asking the service | One account and one friends list across every store and desktop; testable headless in the container against the simulated network |
| Voice codec | Opus (BSD-licensed), 48 kHz like the synth, 20 ms frames, in-band FEC | The standard for voice at 16–32 kbit/s; loss concealment built in |
| NAT | UPnP IGD and NAT-PMP/PCP to open a port, STUN to learn the public address, hole punching through a rendezvous, a relay last | No hosted service: the rendezvous and the relay are executables built from this tree that a game's owner runs (G5: GitHub hosts only code and releases) |

## The wire (M17a)

Every packet starts with the game's 32-bit protocol id (its name and version hashed: two builds
that cannot play together drop each other's packets) and a type byte. All integers are little
endian; `engine/net/wire.hpp` writes and reads them.

| Type | Body | Sent by |
| --- | --- | --- |
| `request` | the client's salt, then zeros to 1200 bytes | a node connecting |
| `challenge` | the client's salt, the host's cookie (a keyed hash of the client's address, salt and the host's secret) | the host, stateless |
| `response` | the client's salt, the cookie, zeros to 1200 bytes | the node connecting |
| `accept` | the client's salt, the session key (the two salts mixed), the slot, the maximum | the host |
| `deny` | the client's salt, the reason (`full`, `closed`) | the host |
| `payload` | the session key, sequence (16 bits), ack, ack bits (32), then messages | both, once connected |
| `disconnect` | the session key | either, three times |

A message inside a payload is its channel (a byte), its reliable id when the channel is
reliable (16 bits), its length (a varint) and its bytes. A payload with no messages is a
keepalive. `describe_packet` decodes any packet into one line for the tools.

The round-trip time is smoothed as TCP does (RFC 6298: SRTT and RTTVAR, gains 1/8 and 1/4) off
each acked packet's send time; loss is the share of the packets sent over the last window that
were never acked once they were older than a round trip and its spread.

## The simulated network (M17a)

`SimNetwork` is a network of nodes with simulated addresses, on a virtual clock in
microseconds. Every directed link has its `LinkConditions`:

| Field | What |
| --- | --- |
| `latency_ms` | one way |
| `jitter_ms` | added to each packet, uniform in [0, jitter] |
| `loss` | the long-run share of packets dropped |
| `burst` | the mean length of a run of drops, packets (1 = independent drops; more = a Gilbert–Elliott chain with the same long-run loss) |
| `duplicate` | the share of packets delivered twice |
| `reorder` | the share of packets held back `reorder_ms` so later ones overtake them; with none, jitter never reorders (a link is a queue) |
| `bandwidth_kbps` | the link's rate (0 = unlimited): a packet waits for the ones before it to be clocked out |
| `queue_ms` | how long the queue may grow before a packet is dropped at its tail |

Every fate (dropped, delayed, duplicated) is drawn from the network's seeded stream in the
order the packets were sent, so the same seed and the same sends give the same deliveries, and
a session over it is a golden. Named presets (`lan`, `broadband`, `wifi`, `mobile`, `bad`,
`awful`) are the conditions the labs sweep.

**Fake traffic** is a node that sends a mix of messages at a rate (`TrafficMix`: so many bytes on
so many channels so often), so a link can be loaded to its edge and the numbers read.

**Bots over the network** (M17k): a game's bot is a pilot; over the simulated network it plays
as a remote client (its inputs up the link, the snapshots down), so the prediction, the
corrections and the bot's own decisions (`ge::ai::Why` records, M16) are measured under each
preset, headless and deterministic.

## The tools

| Tool | Lands | Answers |
| --- | --- | --- |
| the `net-link` lab | M17a | Under each condition preset: what round trip, jitter and loss did the links measure, did every reliable message arrive once and in order, how late, at what bandwidth and how many resends; a chart of the round trip and the loss over time, and the capture attached |
| capture (`capture.hpp`) | M17a | Every packet a node sent and received, with its time, as a pcap file |
| `ge net inspect <pcap>` | M17b | Every packet decoded: type, sequence, acks, each message's channel and size |
| `ge net stats <pcap>` | M17b | Per peer: packets and bytes each way, round trip, loss, resends, bandwidth over time, charted |
| `ge net sim` | M17b | A session of fake-traffic peers over chosen conditions, its numbers and its capture, no game needed |
| traffic mixes (`traffic.hpp`) | M17b | The fake traffic shaped like a kind of game: racing, action (fast inputs, big snapshots), rpg, world (a busy shared world), lockstep (inputs and turns, no snapshots); `--mix` on the lab and on `ge net sim` |
| a live node's capture | M17b | A game points its transport at a `Capture` with a size limit and saves it (`save_capture`) when a session ends or the player asks; the tools read it like a lab's |
| the `net-session` lab | M17d | 32 peers join a lobby, chat to all, to a team and privately, one is kicked, one reconnects |
| the `voice` lab | M17h | A voice through each preset, metered against the clean one, the WAV attached |
| the `accounts` lab | M17j | Sign-ups, logins (a wrong password refused, a brute force throttled), friend requests, presence and an invite, against the service on the simulated network |
| the `net-play` lab | M17k | The game's bots as remote players under each preset: the corrections, the divergence, the bandwidth |

## Fall Line

The demo game is the pilot, once the engine's rows are done (M17k): its desktop version's free
ride played together over a dedicated server, with a lobby before the session where each
player picks the map and their skier, and the host starts it. Races are not part of it. What it
uses, so G4 holds: the free ride (up to 32 skiers on one mountain, the server authoritative,
each client predicting its own skier and interpolating the others), the lobby,
teams for the hunt, chat, private messages and voice, and an account with friends to invite. Its message ids, its snapshot's fields and
its lobby settings live in `games/fall_line/net/` (M17k).
