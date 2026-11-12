# TV, proxy and forward

How a match leaves the machines that play it: watched live by many through **TV**, carried over a
better path by **Forward**, held and shaped on the player's side by **Proxy**, and kept as a
**replay file** that is the same bytes on disk and on the wire. Every game gets all four; a game
adds a face only where its wire is foreign or its fields need a model. The rows are M57a–M57f
(`docs/goals.md`). The classic game's programs these replace are described in the working area
(`porting/shooters-edge/television.md`, `forward.md`, `proxy.md`), never here.

## The four pieces

| Piece | Home | What it does | Who reaches it |
| --- | --- | --- | --- |
| **TV** | `engine/tv`, `ge_tv` | Takes a match from a source (a server, a file, another TV node, a proxy), holds it back by a delay, and fans it out to many viewers through trees of TV nodes; a late viewer starts from a keyframe | Viewers (native clients, a browser viewer over a WebSocket, the hub), other TV nodes |
| **Proxy** | `engine/proxy`, `ge_proxy` | A hop that understands the game: sends packets more than once on a lossy link, compresses the link between two proxies, records, hands the player's view to a local TV | The player's client, or a client on the player's LAN |
| **Forward** | `engine/forward`, `ge_forward` | An opaque hop to a better internet path, chained by the route carried in the connect; copies datagrams untouched after the handshake | Any client of any game |
| **Replay file, version 2** | `engine/replay` | The one demo format: tracks, blocks, keyframes, an index; compressed by the field codec | Everything above, the game's demo player, `ge replay`, the hub |

The names keep away from words already taken: "relay" is the NAT relay of `engine/net/reach`, and
"broadcast" is streaming video to a service (M23e, `docs/replays.md`).

## The layers

```
games/<game>      faces: a foreign wire (the classic stream protocol, the classic forwarder's
                  route key), a schema for the game's fields, the game's keyframe
engine/tv         sources, delay, catch-up, fan-out, chat up, viewer list, files by SHA-256,
                  heartbeat to the master, TCP and WebSocket downstream
engine/proxy      repeat per direction, link codec, record, observers into a local TV
engine/forward    route in the handshake, allow list, keepalive, rate caps, route table, STUN
engine/replay     replay file v2: header, tracks, blocks, keyframes, index; codec/: the field codec
engine/net        sockets, connections, security, the simulated network, the server list
```

Each layer uses only those below it. What stays out of every layer: the classic game's teamplay
helpers (say-team macros, locations, pointing) are Shooter's Edge's client's own console features,
and no command is smuggled through chat.

## The replay file, version 2

Its files end in `.ger`. A file and a live stream are the same bytes, so a viewer joining a
broadcast and a player opening a file run the same code, and a broadcast saved to disk is already
a demo.

- **Header.** Magic, version, the game and its behaviour version (`GameSpec::version`), the track
  table (kind, id, codec), the generic meta (map, mode, start time, players and teams) and an
  optional game blob. Readable without decoding anything else.
- **Blocks.** Chunks grouped into blocks of about one second, each compressed on its own and opened
  by a sync marker. A seek or a late joiner starts at a block holding a keyframe; a cut or damaged
  file reads up to its last whole block.
- **Tracks.**

| Track | Holds | Written by |
| --- | --- | --- |
| `meta` | Scores, roster changes, the match's phases | Every game; the hub reads this alone |
| `inputs` | The input log (`engine/play/inputs.hpp`) step by step | Deterministic games (Fall Line) |
| `state` | Snapshots and deltas of replicated entities | Games on `engine/net` replication |
| `messages` | A foreign protocol's messages (the classic multiview records) | A game's face |
| `keyframe` | What a seek or a late joiner starts from: `Run::clone` bytes, or a face's catch-up state | Every game |
| `events` | Marks and highlights (`engine/replay/highlights`) | Every game |
| `chat`, `voice` | Text lines; Opus frames | Every game |
| `radar` | Low-rate positions and headings for an overhead map (reserved, M56) | Optional |

- **Trailer.** An index of blocks by time and the final meta (duration, scores). Optional: a live
  stream has none until it ends. The file's SHA-256, which a TV node and the hub list files by, is
  taken over the whole file by whoever lists it; it is not stored in the file.
- **Version 1** (the text replay file of M23b) still loads; a game's classic formats convert to
  version 2 and back byte for byte.

## The field codec

The coder (`engine/replay/codec.hpp`) is an adaptive binary range coder: integers only, so it
writes the same bytes everywhere. A probability starts even and moves toward each bit it sees by a
share that shrinks as it sees more (half at first, 1/32 after fifteen bits), so a model started
over at a keyframe learns again within a few dozen values. Over it sit two models: an integer (its
bit length through a tree, its sign, the three bits under the top bit, the rest raw) and a byte
(a tree under a context of the writer's choosing, usually the byte before).

A game's face writes each field as its residual from a prediction, through a model of its own,
under a context of its own. Shooter's Edge's face for the classic multiview records
(`games/shooters_edge/client/demo_v2.hpp`) predicts:

- a player's origin and view angles from the line through the last two, carried on by the time
  since; the frame, weapon frame, skin and effects from the last;
- an entity list's words from the last list's (the same entities change the same fields frame
  after frame), and each entity's origin from the line through its last two;
- a sound's point from its entity's origin; a stat, a ping or a frag count from the last one
  written for it; a delta list's sequence as one on from the last;
- a record's kind as the one that came after the last kind last time;
- text as a template (the text with its whole numbers taken out: a team's info line, a frag
  message) from the last 256 seen, and each number from the same template's last numbers.

A record whose body does not walk whole is kept as its bytes, so every file comes back byte for
byte. A keyframe every 30 seconds (each one starts the models and predictions over) costs about 5%
against none and lets a late joiner start at most 30 seconds back; one every 10 seconds costs about
10%.

Measured by the `replay-codec` lab on 13 real classic match recordings (sizes in KB; unpacking
with the release flags):

| Recording | Raw | zstd 19 (today's `.mvdz`) | xz 6 | `.ger` | `.ger` / `.mvdz` | Unpack |
| --- | --- | --- | --- | --- | --- | --- |
| 4on4, dm2, 20 min | 7,354 | 3,876 | 3,465 | 1,947 | 0.50 | 0.34 s |
| 4on4, e1m2, 20 min | 7,125 | 3,763 | 3,366 | 1,908 | 0.51 | |
| 4on4, dm6 | 5,767 | 3,309 | 3,068 | 1,851 | 0.56 | |
| 4on4, dm3 | 4,300 | 2,493 | 2,272 | 1,392 | 0.56 | |
| 2on2, dm6 | 1,408 | 807 | 751 | 473 | 0.59 | |
| Duel, dm2 | 1,064 | 578 | 523 | 310 | 0.54 | |
| Duel, dm6 | 1,045 | 515 | 479 | 338 | 0.66 | |

Every recording came back byte for byte. Where the bytes go in a 4on4: the players' view angles
(about 27%) and origins (21%), the entities' origins (13%), text (7%); the angles are the mouse's
own noise at 1/65536 of a turn, which no prediction removes. A 4on4 broadcast is about 48 kbit/s
raw, 25 as today's packed demo and 13 as `.ger`.

## Forward

`engine/forward` (`forward.hpp`). An opaque hop between a game's client and its server, for a
player whose own path to the server is worse than a path through a forwarder near them. It knows
nothing of the game: once a route is up it copies datagrams both ways, byte for byte.

- **The route is set up first**, by the forward's own datagrams, before the game's handshake: the
  client asks the first hop with the rest of the route ("hop@hop@server"), gets a cookie only that
  hop can make from the client's address, asks again with it, and is told `routed` (or why not).
  Each hop takes the next entry off the route; a hop with more after it dials the next forwarder
  from a socket of its own for this client and answers only when the whole chain is up, so a
  refusal anywhere comes back to the client in words. Then the game's client connects to the first
  hop as if it were the server, and the game's own handshake runs end to end.
- **Allow list on by default**: a hop forwards only to the destinations it lists (a host, or one
  of its ports); `open` is an operator's explicit choice.
- **Alive both ways**: the client sends an alive every second and the hop echoes it, so the client
  knows its round trip to the hop; a client whose NAT gives it a new port is followed by the salt in
  its alive. A client silent both ways for 15 s is dropped.
- **No answer bigger than its ask**: the route request is padded past every answer, status asks go
  through `net/status.hpp`'s cookie, and STUN is answered only when the request is as big as its
  answer.

The `forward` lab gives the player's direct path each preset and puts the forwarders on good legs:

| Direct path | Direct rtt p50 / p95 (ms), lost | One forwarder | Two forwarders | Route set up (one / two) |
| --- | --- | --- | --- | --- |
| broadband | 34 / 36, 0.2% | 36 / 36, 0% | 62 / 62, 0% | 36 / 88 ms |
| mobile | 111 / 125, 2.4% | 36 / 36, 0% | 62 / 62, 0% | 36 / 88 ms |
| bad | 224 / 253, 3.0% | 36 / 36, 0% | 62 / 62, 0% | 36 / 88 ms |
| awful | 459 / 504, 18.8% | 36 / 36, 0% | 62 / 62, 0% | 36 / 88 ms |

A stranger's asks of every kind draw answers of at most a third of their size.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| Order | The format first (M57a), then TV, forward, proxy | TV first on the classic multiview stream: sooner, but its wire would change when version 2 lands |
| One format for disk and wire | A TV stream is a replay file being written | A separate wire protocol: two readers to keep agreeing |
| The demo format's name | Version 2 of the replay file | A new name: the engine already calls a kept run a replay |
| The classic compressed client demo | Not read; the field codec replaces it | Running the player's own installed program to unpack it (a later, opt-in choice); studying third-party reconstructions of unknown origin |
| Teamplay helpers | In Shooter's Edge's client | In the proxy, as the classic proxy did: they need no place on the network path |
| Forward's route | Set up by the forward's own datagrams before the game's handshake, so any game on any wire goes through unchanged | Carried in the game's connect (the classic way): every game's handshake would have to carry it; the classic face reads it there |
| The browser viewer | The WebSocket downstream lands with TV (M57b), for the hub's web viewer (M56) | Later: the hub needs it first |
