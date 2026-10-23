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
engine/net/session        lobby, teams, chat, private messages, the player list, kicks,
                          reconnecting by token; discovery: finding a game on the LAN (M17d)
engine/net/reach          STUN, the rendezvous, hole punching, the relay (M17e1);
                          UPnP, NAT-PMP/PCP (M17e2)
engine/net/guard          input checks, interest, replay audits, cheat flags, reports (M17l)
engine/net/stress         load, soak, chaos scripts, ramps to the breaking point, fuzzing (M17m)
        │
engine/net  node          connections: handshake, keepalive, timeout, peers up to 32
            channel       unreliable, sequenced, reliable-ordered messages; acks; resends;
                          big messages in pieces, priorities, the send budget
            packet        the wire format: header, acks, messages; decoded for the tools
            transport     one interface over a UDP socket, a TCP stream, a simulated network
            sim_network   the simulated network: every packet's fate off a seeded stream
            conditions    latency, jitter, loss and its bursts, duplicates, reorder, bandwidth
            capture       pcap files any packet tool opens, read back by `ge net`
            udp_socket    the OS's UDP sockets (POSIX and Winsock)
            tcp_socket    the OS's TCP sockets, carrying the packets as length-prefixed frames
            os_socket     the socket shim both share: the only file that includes the OS's headers
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
| UDP and TCP | UDP is the default; a TCP transport carries the same packets as frames (a 16-bit length, then the bytes) on one stream a peer (M17c) | A fast game cannot wait for a lost packet's resend; a turn-based game, a chat-heavy world or a network that blocks UDP can. The layers above the transport do not care which: over a stream the acks and channels still run, and simply never see a loss, only the late packets head-of-line blocking makes |
| The clock | The caller passes `now_us` into every call; nothing in `engine/net` reads a clock | The simulated network runs on a virtual clock and a real game on the frame's; the same code, and a test is exact |
| Randomness | Salts and the simulated network's fates come from `core::Rng` streams the caller seeds; a real game seeds them from the OS's entropy (`os_entropy`) | A simulated session replays to the bit; a real one is not guessable |
| Allocation | Fixed-capacity rings sized at create time (peers, the sent-packet ring, the reliable windows) | Nothing allocates in `node_update` once a node is made (G2) |
| Packet size | At most 1200 bytes on the wire (`kMaxPacket`) | Fits the smallest MTU on the paths games use (IPv6's 1280 less its headers) with no fragmentation by IP; bigger messages are sent in pieces by the engine (M17c) |
| Big messages | Only on a reliable channel configured for them (`ChannelConfig::largest`, up to 64 KB): pieces of one packet each, reliable messages of their own with a "more follows" bit, joined in order and delivered once; queued all together or refused | The reliable channel already delivers once and in order, so joining is appending, with no fragment ids or timers of its own. An unreliable message bigger than a packet would be lost whenever any piece was: a snapshot that big is a delta's job (M17f) |
| Priority and budget | Each channel has a priority; a connection may have a send budget (kbit/s, a token bucket of 50 ms). Higher priorities ride first; with the bucket empty only acks and keepalives go, reliable messages wait and loose ones are dropped at the flush's end | A map download must not starve the inputs, and a server with 32 clients must keep each inside what its link carries. With every priority equal the order is the order queued |
| Acks | Every payload carries its sequence, the newest sequence received and 32 bits of the ones before it | One packet's loss is told by the next few, with no ack packets of their own; the round-trip time comes off the same acks |
| Reliability | Per message, not per packet: a reliable message rides in packets until one carrying it is acked, resent once a round trip has passed | The newest data goes in every packet; nothing waits behind a lost packet but the reliable messages it carried |
| Handshake | Request → challenge (a cookie only the host can make) → response → accept, all packets padded to the request's size | A spoofed address never gets past the challenge, the host keeps no state for it, and a reply is never bigger than the request (no amplification) |
| Peers | A node is both host and client: it may accept and it may connect (`NodeConfig`) | Client-server, a listen server and a peer-to-peer mesh (lockstep, rollback) are the same code; the game sets the maximum, 1 to 32 |
| Encryption | AEAD over every payload with keys from a connect token a matchmaker signs (M17i); off until then | A LAN game and the labs need none; an internet game does. Keys come from the environment or GitHub secrets, never the tree |
| Capture format | pcap (libpcap's format) with IPv4/UDP headers made up for simulated addresses | Every packet tool opens it, and `ge net` decodes our protocol inside it |
| Accounts | Our own service executable, self-hosted, over the same transport (reliable channel over TCP or UDP); passwords stored only as Argon2id hashes; a login returns a signed session token a game host checks without asking the service | One account and one friends list across every store and desktop; testable headless in the container against the simulated network |
| Sessions | A layer over a node's reliable channel (`session.hpp`): the host's process seats players and holds every rule, a client asks and mirrors; a player is a SEAT, not a peer slot, with a 64-bit token to come back to it | A dropped player keeps their seat, name, team and readiness for `reconnect_us`, and the roster survives a new peer slot; the host being the one authority is what makes the rules (rate limits, team caps, kicks) hold, and what an anti-cheat later trusts (M17l) |
| Who leads | The host's own player on a listen server; otherwise the first seated, then the longest-seated present player | A dedicated server has no player of its own, and a lobby still needs someone to pick the map and start |
| LAN discovery | Its own UDP socket on a port the game fixes: a broadcast probe (64 bytes), each host's advert (never bigger than the probe) | A node's port need not be known in advance, a probe never reaches a node, and discovery cannot amplify traffic at a third party |
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

A message inside a payload is its channel byte (the channel in the low bits; the top bit set
when a 16-bit id follows, as on a sequenced or reliable channel; the next bit set on a big
message's piece when more pieces follow), the id, its length (a varint) and its bytes. A payload with no messages is a
keepalive. `describe_packet` decodes any packet into one line for the tools.

The round-trip time is smoothed as TCP does (RFC 6298: SRTT and RTTVAR, gains 1/8 and 1/4) off
each acked packet's send time; loss is the share of the packets sent over the last window that
were never acked once they were older than a round trip and its spread.

## Sessions (M17d)

A session runs on one reliable channel the game gives it (`SessionConfig::channel`); the game
reads the node's events after the session and skips that channel. Its messages are stated once
in `engine/net/session_wire.hpp`: a client's `hello` (its token, its name), `ready`, `team`,
`chat`, `kick`, `start`, `back` and `settings`, and the host's `welcome` (the seat, the token,
the lobby's size, the phase), `player` (a seat's state and why it is sent: joined, rejoined,
away, changed, or a snapshot for a new client), `gone`, `pings`, `chat`, `settings`, `phase`,
`kicked` and `refused` (what was asked, and why not).

| Rule | What the host does |
| --- | --- |
| Seats | A hello without a token takes the lowest free seat and a fresh token; one with a token comes back to its seat. A full lobby, a started game (unless `join_in_progress`) and a banned token or address are refused, and the peer is let go once the refusal is acked |
| Away | A peer that times out leaves its seat away (unready) for `reconnect_us`, then frees it; a peer that disconnects frees it at once |
| Teams | `team_count` teams of at most `team_size`; a new player joins the smallest; a move into a full team, or once the game has started, is refused |
| Start and return | Only the leader; start needs `min_players` present and, with `need_ready`, everyone ready; return makes everyone unready |
| Chat | Control bytes become spaces; a line is cut at `chat_max` bytes on a UTF-8 boundary; each player's bucket holds `chat_burst` lines and gains one every `chat_refill_us`; a team line reaches the team, a direct line its target, and every line comes back to its sender |
| Kick | Only the leader, never the host's own player; the kicked player is told, the seat freed at once, and the token and the address refused for the rest of the session |
| Pings | Every `roster_us` the host sends each seat's round trip as it measures it |

A client whose host times out tries again every `retry_us` with its token until `reconnect_us`
has passed, then reports the session `lost`.

**Discovery** (`discovery.hpp`): a client broadcasts a probe (the protocol, a nonce) to the
game's discovery port; each host listening answers with an advert (its session's port, seated,
maximum, phase, name). Over UDP the probe goes to a broadcast address; on the simulated network
to `sim_broadcast(port)`, which reaches every socket bound on that port.

## Reaching a host (M17e1)

Most players sit behind a home router's NAT, which lets a datagram in only on a mapping one of
its own made going out. Three pieces get a client to a host anyway:

| Piece | What it does |
| --- | --- |
| STUN (`stun.hpp`) | A 20-byte Binding request; the answer's XOR-MAPPED-ADDRESS is where the server saw it come from: the node's public face. The rendezvous answers it on its own port, and so does any public STUN server |
| The rendezvous (`rendezvous.hpp`) | A small public server. A host registers its session every `register_us` (the server learns its public address and says so); a client joins by the session's id, and the server sends each the other's public address with the client's nonce and a relay port with its key. Every ask is padded to the biggest answer, so the server never amplifies a flood |
| Reach (`reach.hpp`) | The two ends, on the node's own socket (its `keep_foreign` hands them the rendezvous's datagrams). The host punches: datagrams carrying the nonce to the client's public address, which open its own NAT to it, and it binds the relay. The client connects its node to the host's public address; a punch with its nonce from another port (a symmetric NAT's port for this pair) moves the connect there (`node_retarget`). After `direct_us` without an accept it binds the relay and connects through it |

Which path a pair takes follows from RFC 4787's behaviours, and the simulated NATs
(`sim_add_nat`) have them all: a full cone lets anyone in on a mapping; a restricted NAT only a
node the inside has sent to; a port-restricted NAT only that node's port; a symmetric NAT makes
a mapping per destination and lets only that destination answer. A hole opens unless one side is
symmetric and the other port-restricted or symmetric: the symmetric side's port for the pair is
one the other side never sent to. Those pairs go through the relay, which forwards between the
two addresses that bound it with the key and nothing else. A port forward
(`sim_nat_forward`) is what M17e2's UPnP and NAT-PMP will ask a router for.

The `net-reach` lab runs all 25 pairs of the five kinds (none and the four) under each preset:
direct pairs connect in a few round trips, relayed ones a little over `direct_us`.

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
| `stream` | the link is a stream (TCP): nothing is lost, doubled or reordered; a packet the line drops arrives a resend timeout later (three latencies and two jitters, 200 ms at least) and every packet behind it waits (head-of-line blocking); a full queue makes the sender wait instead of dropping |

Every fate (dropped, delayed, duplicated) is drawn from the network's seeded stream in the
order the packets were sent, so the same seed and the same sends give the same deliveries, and
a session over it is a golden. Named presets (`lan`, `broadband`, `wifi`, `mobile`, `bad`,
`awful`) are the conditions the labs sweep.

**NATs** (`sim_add_nat`): a node can sit behind a NAT of any RFC 4787 kind, its datagrams leaving
from the NAT's public node on a mapped port and arriving only through the NAT's filter; a mapping
lives `mapping_us` after its last datagram out, and a port forward lets anyone in to one inside
address. What a NAT refuses is counted in `nat_dropped`.

**Fake traffic** is a node that sends a mix of messages at a rate (`TrafficMix`: so many bytes on
so many channels so often), so a link can be loaded to its edge and the numbers read.

**Bots over the network** (M17k): a game's bot is a pilot; over the simulated network it plays
as a remote client (its inputs up the link, the snapshots down), so the prediction, the
corrections and the bot's own decisions (`ge::ai::Why` records, M16) are measured under each
preset, headless and deterministic.

## Anti-cheat (M17l)

The engine's answer to cheating is that a client is never trusted with anything it could
lie about, and every claim it does make is checked against what the deterministic step allows.
It is built from the layers above, not bolted on:

- **The server's word.** In a server game a client sends inputs and the host steps the world
  (M17f); a client claims only what its game names (in the free ride, nothing but its controls).
  A speed hack or a teleport on the client changes only its own prediction, which the next
  snapshot corrects.
- **Inputs checked on arrival.** Every axis in range and on the control tape's grid, at most one
  input a step, none from a step the host has not reached, and a client sending faster than
  its tick is rate-limited and then dropped (the flood guards of M17i).
- **Nothing hidden is sent.** Interest management (M17f) decides what each client receives, so
  a wall hack or a map hack has nothing to read: what a player must not see never leaves the
  host.
- **The same step everywhere.** In lockstep and peer games the step's digest is the check
  (M17g): a peer whose state differs is out on the step it differs. Where peers own their own
  bodies, a position or a speed beyond what the step can reach from the last one is refused.
- **Replays as audits.** A score, a ghost or a record is accepted only after the server replays
  its input tape and reaches the same digest, which is what the engine's determinism (G1)
  buys: a forged score cannot also forge the run that would have earned it.
- **Bot-likeness.** M16's humanness numbers (reaction times, input noise, the shape of a turn)
  score how machine-like a player's inputs are; a flag goes for review, not an automatic ban.
- **Integrity and identity.** Packets are sealed with AEAD under the connect token's keys
  (M17i), so a forged or replayed packet is dropped; reports and bans are kept on the account
  in `ge-service` (M17j), not on a machine id.

Not in scope, by design: kernel drivers, scanning the player's machine, or obfuscation. They
are an arms race the engine does not enter, and the checks above hold without them.

## Pressure tests (M17m)

The simulated network runs on virtual time, so an hour of play takes seconds and every run is
seeded and replayable. `engine/net/stress` and `ge net stress` push it until something breaks:

- **Load.** 32 peers in a session, many sessions in one host process, each on a traffic mix.
- **Soak.** Hours of virtual play, held to memory and queues staying flat, sequence numbers
  wrapping cleanly and every reliable message delivered.
- **Chaos.** A script changes the links over time: a link that flaps, a partition and its
  healing, the host lost mid-session, a NAT that rebinds a client's port, clock skew, a burst of
  loss, the bandwidth collapsing. Each ends with every peer reconnected or cleanly timed out.
- **Ramps.** One knob (latency, loss, peers, message rate, bandwidth) is raised step by step
  until a bound breaks (delivery late, a queue growing, a peer timing out), and the report
  names the value where it broke for each traffic mix.
- **Fuzzing.** The packet parser is fed random and mutated datagrams under the sanitizers;
  every one is refused or read, never a crash or an overread.
- **Bots at scale.** A game's bots play as clients across many simulated servers, so the
  game's own traffic is the load, not only fake traffic.

## The tools

| Tool | Lands | Answers |
| --- | --- | --- |
| the `net-link` lab | M17a | Under each condition preset: what round trip, jitter and loss did the links measure, did every reliable message arrive once and in order, how late, at what bandwidth and how many resends; a chart of the round trip and the loss over time, and the capture attached |
| capture (`capture.hpp`) | M17a | Every packet a node sent and received, with its time, as a pcap file |
| `ge net inspect <pcap>` | M17b | Every packet decoded: type, sequence, acks, each message's channel and size |
| `ge net stats <pcap>` | M17b | Per peer: packets and bytes each way, round trip, loss, resends, bandwidth over time, charted |
| `ge net sim` | M17b | A session of fake-traffic peers over chosen conditions, its numbers and its capture, no game needed |
| traffic mixes (`traffic.hpp`) | M17b | The fake traffic shaped like a kind of game: racing, action (fast inputs, big snapshots), rpg, world (a busy shared world), lockstep (inputs and turns, no snapshots); `--mix` on the lab and on `ge net sim` |
| `net-link --transport=tcp`, `--mix=download`, `--budget` | M17c | The same sessions with every link a stream (what head-of-line blocking does to the round trip and the resends), a 64 KB message to every client every two seconds checked whole byte for byte, and every connection held to a send budget; `ge net sim` takes the same flags |
| a live node's capture | M17b | A game points its transport at a `Capture` with a size limit and saves it (`save_capture`) when a session ends or the player asks; the tools read it like a lab's |
| the `net-session` lab | M17d | A 32-player lobby on a dedicated host under each preset: everyone joins and picks a team, chats to all, to the team and to one player, one floods; the leader kicks one (whose return is refused), another is cut for six seconds and comes back to its seat, then the settings, the start and the return. The table checks every chat copy delivered, every player list the host's, the kick, the ban and the seat kept; the seats are charted and the pings drawn as boxes |
| the `net-reach` lab | M17e1 | Every pair of NAT kinds (none, full cone, restricted, port-restricted, symmetric) under each preset: the path each pair took (direct or relayed, against the one RFC 4787 predicts), how long it took, a reliable message each way; STUN from behind each kind; the connect times as a heatmap |
| the `voice` lab | M17h | A voice through each preset, metered against the clean one, the WAV attached |
| the `accounts` lab | M17j | Sign-ups, logins (a wrong password refused, a brute force throttled), friend requests, presence and an invite, against the service on the simulated network |
| the `cheats` lab | M17l | Each scripted cheat (speed hack, teleport, input flood, off-grid or future inputs, forged and replayed packets, fishing for hidden state, a planted desync, a forged score or ghost) caught or refused; honest bots under every preset raising no flag |
| `ge net stress` and the `net-stress` lab | M17m | Load and soak runs, ramps that name each knob's breaking point for a traffic mix, the session's memory and queues over time, charted |
| the `net-chaos` lab | M17m | Each chaos script over a session: who disconnected, who came back, what was lost and how late |
| the `net-fuzz` lab | M17m | The parser against random and mutated datagrams: how many read, how many refused, none crashed |
| the `net-play` lab | M17k | The game's bots as remote players under each preset: the corrections, the divergence, the bandwidth |

## Fall Line

The demo game is the pilot, once the engine's rows are done (M17k): its desktop version's free
ride played together over a dedicated server, with a lobby before the session where each
player picks the map and their skier, and the host starts it. Races are not part of it. What it
uses, so G4 holds: the free ride (up to 32 skiers on one mountain, the server authoritative,
each client predicting its own skier and interpolating the others), the lobby,
teams for the hunt, chat, private messages and voice, and an account with friends to invite. Its message ids, its snapshot's fields and
its lobby settings live in `games/fall_line/net/` (M17k).
