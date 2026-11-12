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
| `radar` | Low-rate positions and headings for an overhead map (`engine/replay/radar.hpp`: rosters, positions, an outline) | Optional; Shooter's Edge's `demos pack` |

- **Trailer.** An index of blocks by time and the final meta (duration, scores). Optional: a live
  stream has none until it ends. The file's SHA-256, which a TV node and the hub list files by, is
  taken over the whole file by whoever lists it; it is not stored in the file.
- **Version 1** (the text replay file of M23b) still loads; a game's classic formats convert to
  version 2 and back byte for byte.

## The TV node

`engine/tv` (`node.hpp`, `net_link.hpp`). A node takes a source's replay file bytes in any pieces
(a game's server writing them, a file read out, or the node above it), splits them into the header
and whole blocks, and sends them on:

- **The delay** is in game time: the first block anchors the source's clock to the node's, and a
  block goes out once its start plus the delay has come, so a source that runs ahead (a file) is
  paced to the match. With no delay a block goes out as soon as it is whole.
- **A viewer** says hello, gets the header, then the blocks from the newest keyframe block out,
  then each block as it goes out, then `ended`. A link that will take no more keeps the viewer's
  place, so a slow viewer slows no one else. Blocks before the newest keyframe that no viewer still
  needs are let go.
- **A relay** is a viewer of the node above that feeds a node of its own. Chat goes up to the first
  node and comes back down to every viewer; each relay reports how many watch under it, and every
  node tells its viewers the whole tree's count.
- **The wire** is one reliable, ordered channel of engine/net: hello, header, block (in pieces under
  engine/net's biggest message), chat, viewers, ended, refused.

The `tv` lab runs a source, a relay and six viewers under every simulated network preset:

| Preset | Viewers whole | Lag p50 / p95 / max (ms) | Down a viewer (kB a match second) |
| --- | --- | --- | --- |
| lan | 6 of 6 | 20 / 20 / 20 | 2.1 |
| wifi | 6 of 6 | 80 / 90 / 180 | 2.1 |
| mobile | 6 of 6 | 140 / 302 / 500 | 2.1 |
| bad | 6 of 6 | 280 / 590 / 1040 | 1.8 |
| awful | 6 of 6 | 1145 / 3425 / 4320 | 1.6 |

On the bad and awful links some viewers connect late and start at a later keyframe block, so they
take fewer bytes.

### Browsers, finished files and the master

- **WebSocket viewers** (`websocket.hpp`): a host's `TvWeb` listens on TCP, takes the HTTP upgrade
  and carries every TV message as one binary frame each way, beside the net node's viewers in the
  same node (link numbers from `kWebLinks` up). Pings are answered, a close is answered and the link
  dropped, and a broken handshake or a frame over 1 MiB drops the link. This is the wire the hub's
  web viewer watches by (`docs/hub.md`).
- **Finished files** (`library.hpp`): `library_scan` lists a folder's `.ger` files by the SHA-256
  of their bytes, with name, size, game and header meta. A viewer sends `list` and gets `files`, and
  sends `get` with a hash and gets the file in pieces, or a "not here". A viewer whose hello says
  *files* gets no live stream and is not counted as watching (the hub's archive fetching a file).
- **The master** (`tv_host_list`): the host beats to masters as a TV node from its own socket and
  answers status asks there with its name, the match's game and map, its viewers and the delay.

## Spectating by inputs

`engine/replay/spectate.hpp`. A deterministic game on the play contract (Fall Line) broadcasts
what its player holds, not the world: each one-second block holds the block's runs of held inputs
(each axis's grid index) and the run's digest after it, and every few seconds a keyframe block also
holds the run saved (`save_run`). A viewer loads the first keyframe it gets and steps its own run
through every block, checking the digest at each; a run that parts stops there with the step.
Fall Line through a source node and a relay reaches the player's digest under every network preset
(the `spectate` lab). Its inputs cost about 230 bytes a second; a whole save every 10 s is about
10 kB a second packed, so the keyframes are what is left to make small.

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

The face also writes the hub's words (`docs/hub.md` § The archive) into the meta, read off the
recording before it is packed (`demo_hub_meta`): the map, the mode, the server's name and every
player and team in the header; the length, each player's frags and each team's total in the
trailer. A caller adds its own (`title`, `tag`, `start`).

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

## The classic face

Shooter's Edge's server offers the classic broadcaster's stream port
(`games/shooters_edge/net/classic_qtv.hpp`, M57c). A classic relay connects over TCP, asks in a few
lines of text (`QTV`, `VERSION: 1`, `RECEIVE`, its user info, its auth offers) and is answered
`QTVSV 1` and `BEGIN`, a challenge, or an error with the classic words; then the match follows as
a multiview recording written as it is played. Each relay has its own recorder hung on the server's
`streams`, started when it is let in, so its first frame is the whole signon (its catch-up) and
every map change another; a stream's signon also stuffs the full server info and the `skins` line a
relay waits for. With `qtv_password` set the server asks for the strongest method both sides know:
SHA3-512 of the challenge and the password (written here from FIPS 202), or the plain password.
Up from a relay come chat (`say`, `say_team`, `say_game`, printed to the spectators, or everyone with
`qtv_sayenabled`, and into every relay's stream) and its viewer list (`qul`: joins, renames and
leaves told to the players, and stuffed to clients with `qul 1`). A relay that takes nothing for
`qtv_streamtimeout` seconds, or lets 16 MiB pile up, is dropped.

| Setting | Default | What |
| --- | --- | --- |
| `qtv_streamport` | 0 | The TCP port (0: none) |
| `qtv_maxstreams` | 1 | Relays at once (0 or less: no limit); three over it are told so and closed |
| `qtv_password` | empty | The password a relay gives |
| `qtv_pendingtimeout` | 5 | Seconds to finish the request |
| `qtv_streamtimeout` | 45 | Seconds a relay may take nothing |
| `qtv_sayenabled` | 0 | 1: viewers' chat reaches the players too |

The `se-tv` lab serves a two-player match to relays that join at 0, 4 and 8 seconds: a relay's
catch-up on the test arena is under 1 KB and the match about 4.5 KB a second, and every stream
plays to its end in the demo reader with the viewer's chat and join in it. A stream's signon is
cut into records under 8000 bytes, which is what a relay takes.

Our TV node's classic face (`games/shooters_edge/net/classic_tv.hpp`) is a relay of its own: it
dials a stream port (ours or any server's), answers its challenge, reads the records after `BEGIN`
and holds each for the delay (7 s by default) before it goes to the viewers, who ask the node as a
relay asks a server, with the node's own password. A connection that falls is dialled again after
2 s, doubling to an hour. A viewer who joins mid-match is caught up from the match as the node keeps
it (`classic_mv_state.hpp`): the level's signon as it came, the state messages since (light styles,
server info, user info, frags, pings, statics), every player in the recorder's form with all his
fields, each one's stats and a full entity list from the baselines, so the next record forwarded
reads on from it. A viewer's `say` stays on the node; its `say_game` goes up to the server, and
the viewer list goes up as `qul` lines. The `se-tv-node` lab relays a two-player match with a 2 s
delay to viewers who join at 0, 4 and 8 seconds: a late viewer's catch-up is 0.7 KB, and every
viewer's stream plays to the same end (every player, his frags and name, every entity) with the
chat in it.

Our client watches a broadcast with `watch <host>:<port> [password]` in the console
(`games/shooters_edge/net/classic_watch.hpp`): it asks as a relay asks, answers a challenge, and
hands the records to the demo reader a whole frame at a time, so the window draws the match as it
draws a game it joined (the same view, sounds and lines). Still to come in M57c: the node writing
its own name into the server info.

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

**The classic face** (`games/shooters_edge/net/classic_forward.hpp`) plugs into the forward's face
seam (`ForwardFace`: the datagrams the forward does not take, a client admitted with
`forward_admit` and connected when the face's handshake upstream is done) so classic clients and
forwarders chain through ours. The route is the connect's `prx` user info key, the rest of the route
after this hop with the server last; the hop takes its destination off it, writes the rest back,
adds `*qwfwd` and runs the challenge and the connect upstream with the client's qport. It answers the
client's `j` only once the destination has accepted, and passes a refusal on in its words, so a
chain of classic hops connects end to end or says why not. It answers `ping`, `status` (with
`*version` "qwfwd ge 1", which is how a classic browser tells a forwarder) and `pingstatus` (each
server it knows with its ping, for a client's route finder) itself, once a second a host; it makes
challenges from the client's address and a secret, so it keeps nothing before a good connect; and it
follows a client whose NAT gives it a new port by its qport on the same host.
`tests/shooters_edge_classic_forward_test.cpp`: our classic client plays through one hop and through
a chain of two to our classic server.

**The route finder** (`engine/forward/routes.hpp`) picks the path with the lowest summed round trip
from what a server browser already measured: the player's own ping to every server and forwarder,
and each forwarder's list of the servers it reaches with its own ping (the game's `RouteQuery`
asks for it; Shooter's Edge's is the classic `pingstatus`). Hosts are matched without their ports,
so a forwarder beside a server is that host, and ties go to the fewer hops. `<game>_cli route
<address>` answers the route and the chain to give the game.

## Proxy

`engine/proxy` (`proxy.hpp`, `trial.hpp`). Two hops that understand the slow link between them:
the player runs the near proxy beside the game's client and a far proxy runs near the server. The
client connects to the near one as if it were the server; the far one opens a socket of its own to
the server for that client. The game's hops to its proxies stay as the game sent them; between the
proxies each datagram travels in a link frame that each sending end may:

- **repeat**: send it up to five times, the copies a few milliseconds apart, the receiver passing
  on the first copy only (a window by number);
- **pack**: XOR it with the newest datagram the other end said it has, then write its runs of zero
  bytes as counts. A lost frame costs only itself, because the reference is always one the other
  end confirmed; a frame that would not get smaller goes plain.

The session opens like forward's route (a padded open, a cookie, then opened or a refusal in
words), and the far proxy reaches only the servers on its allow list. Lag and loss for practice are
the transport's own fake conditions.

The `proxy` lab runs a 60 Hz client and a 20 Hz server through the pair at each loss on the link
between them:

| Link loss | Plain | Twice | Three times | Packed | Three times packed |
| --- | --- | --- | --- | --- | --- |
| 2% | 1.45% lost | 0.06% | 0% | 1.45% | 0% |
| 10% | 10.30% | 0.91% | 0.12% | 10.30% | 0.12% |
| 20% | 21.03% | 4.42% | 0.91% | 21.03% | 0.91% |
| Link bytes down a second | 8284 | 16564 | 24844 | 2514 | 7532 |

Three copies cut the loss at 10% about eighty times; packing sends 30% of the plain bytes down, so
three copies packed cost less than one plain.

## The sequel's recordings

The sequel's client recording (a `.dm2`: blocks of a 32-bit length and one server packet's messages,
ended by a length of -1) goes on the replay file through Shooter's Edge's face
(`games/shooters_edge/client/sequel_demo.hpp`, M57f), on a `sequel` track of field-coded chunks, ten
blocks (a second of its 10 Hz frames) a chunk and a keyframe every 30. Every message is walked and
every field coded as the residual from its prediction: origins from the line through their last two,
angles, frames and stats from their last, the entity number from the one before it in the list, the
frame number from the last plus one, a sound's point from its entity's origin. An entity's flag bytes
are coded after its number, under that entity's own last flags, so they are learned per entity while
the bytes still come out in the file's order. A block that does not walk whole is kept as its bytes.

The `se-sequel-pack` lab: the demo pack's own recording (640 blocks, 138 511 bytes) walks whole and
comes back byte for byte at 40 418 bytes, 29% of the recording and 77% of zstd at level 19; a
made-up one of a minute packs to 39% of zstd. The format's facts are in the working area
(`porting/shooters-edge/sequel-demo.md`).

The sequel's TV is built here on the native format, since the sequel has no broadcaster of its own
(`games/shooters_edge/client/sequel_tv.hpp`): a broadcast takes the recording's blocks as they are
played and writes the replay file as it goes, the same bytes `pack_demo` writes for the finished
recording (it is built on the broadcast). A TV node takes it as any game's stream. The sequel's
frames are deltas against the frames before them and its level starts once, so a broadcast
keyframes only its first block and a late viewer is sent the match from its start; at a few hundred
bytes a second that is small. The `se-sequel-tv` lab plays the demo pack's recording as if live
through a node with a 2 s delay: it is broadcast at 617 bytes a second against the recording's
2164, and a viewer who joins at 30 s gets the whole recording back byte for byte, as the first does.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| Order | The format first (M57a), then TV, forward, proxy | TV first on the classic multiview stream: sooner, but its wire would change when version 2 lands |
| One format for disk and wire | A TV stream is a replay file being written | A separate wire protocol: two readers to keep agreeing |
| The demo format's name | Version 2 of the replay file | A new name: the engine already calls a kept run a replay |
| The classic compressed client demo | Not read; the field codec replaces it | Running the player's own installed program to unpack it (a later, opt-in choice); studying third-party reconstructions of unknown origin |
| Teamplay helpers | In Shooter's Edge's client | In the proxy, as the classic proxy did: they need no place on the network path |
| A late viewer's catch-up | Written from the match the node keeps, record by record | Replaying everything since the level began: it grows with the match and plays the past at once |
| A relay's auth | NONE, PLAIN and SHA3-512 | The older 16-bit check and MD4: weaker, and every current relay offers SHA3-512 |
| Forward's route | Set up by the forward's own datagrams before the game's handshake, so any game on any wire goes through unchanged | Carried in the game's connect (the classic way): every game's handshake would have to carry it; the classic face reads it there |
| The proxy's packing | XOR against the newest datagram the other end confirmed, zero runs counted: generic, a lost frame costs only itself | A stream compressor across frames (the classic "quality line"): smaller, but one loss damages what follows; the field codec on the link: needs the game's schema, a later face |
| The sequel's late viewer | The match from its start (the broadcast keyframes once): small, and its frames need what came before | A keyframe that repeats the level's start: the frames after it are deltas against frames the viewer never had |
| The browser viewer | The WebSocket downstream lands with TV (M57b), for the hub's web viewer (M56) | Later: the hub needs it first |
