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

A file and a live stream are the same bytes, so a viewer joining a broadcast and a player opening
a file run the same code, and a broadcast saved to disk is already a demo.

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

- **Trailer.** An index of blocks by time, the final meta (duration, scores) and the file's
  SHA-256. Optional: a live stream has none until it ends.
- **Version 1** (the text replay file of M23b) still loads; a game's classic formats convert to
  version 2 and back byte for byte.

## The field codec

A game declares each field of a track once (its type, its quantum and its predictor: none, delta
from the last value, linear from the last two, or an angle that wraps), and the codec writes each
value as its residual from the prediction through an adaptive binary range coder with a context per
field. Text, chat and anything without a schema go through zstd. Measured on 15 real classic match
recordings with a throwaway prototype (residuals handed to xz instead of a range coder):

| Recording | Raw | zstd -19 (today's packed demo) | xz -9e | Prototype field model |
| --- | --- | --- | --- | --- |
| Duel, 10.3 min | 1,064 KB | 577 KB | 524 KB | 281 KB |
| 4on4, 20.3 min | 7,354 KB | 3,869 KB | 3,461 KB | 1,929 KB |
| 4on4, 20.2 min | 4,978 KB | 2,725 KB | 2,512 KB | 1,509 KB |

So a 4on4 broadcast is about 48 kbit/s raw, 25 as today's packed demo and 12.7 with the model;
M57a's `replay-codec` lab holds the real codec to at least half of today's packed size.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| Order | The format first (M57a), then TV, forward, proxy | TV first on the classic multiview stream: sooner, but its wire would change when version 2 lands |
| One format for disk and wire | A TV stream is a replay file being written | A separate wire protocol: two readers to keep agreeing |
| The demo format's name | Version 2 of the replay file | A new name: the engine already calls a kept run a replay |
| The classic compressed client demo | Not read; the field codec replaces it | Running the player's own installed program to unpack it (a later, opt-in choice); studying third-party reconstructions of unknown origin |
| Teamplay helpers | In Shooter's Edge's client | In the proxy, as the classic proxy did: they need no place on the network path |
| The browser viewer | The WebSocket downstream lands with TV (M57b), for the hub's web viewer (M56) | Later: the hub needs it first |
