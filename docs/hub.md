# The hub: every live game and every demo on one website

A HUB is a website a game's community hosts: it lists every server playing that game right now,
shows each match's scoreboard as it changes, lets a visitor watch a live match or a recorded one,
and keeps a searchable archive of demos. Servers and broadcast nodes announce themselves to a
MASTER; the hub reads the masters, asks every server for its state, follows the broadcasts and
indexes the demos they keep. Row M56 ([`goals/M56.md`](goals/M56.md)). This page is the design;
nothing is built yet. The maintainer chose the engine in the browser as the way a page shows a
match (2026-11-11; § Watching in a browser).

The classic game's community runs such a site; what it does, measured and read in its public
sources, is the working note `porting/shooters-edge/hub.md`. This design takes its shape (the
pages, the polling, the advert in the status reply, demos addressed by their hash) and makes it
generic: one hub program for every game on the engine, the game supplying only its words.

## What a game's community gets

| Page | What it shows | Refreshed |
| --- | --- | --- |
| Servers | A card per server with anyone on it: the map's picture, the mode, the phase and time left, the scoreboard (players or teams), spectators and broadcast viewers, the address, the country, and buttons: Join, Spectate, Watch | Every 5 s while the page is open |
| Watch | One live match: the game's own picture in the page (the web viewer), its scoreboard, the other live matches beside it sorted by interest | The broadcast's own pace |
| Players | Every person on every server: name, playing or watching, where, and the links | Every 5 s |
| Demos | The archive: filters by mode, map, player, team, tag, server and age; 15 a page; scores hidden until asked; Play, Download, Share | On demand |
| Demo | One demo: the picture, the scoreboard and the game's stats behind a spoiler, the seek bar, the speed, whom to follow, clips by time range in the link | On demand |
| Scoreboard | One server's scoreboard alone, sized for a streamer's overlay | Every 5 s |

No login to read it: everything a hub shows is public, read-only, and cacheable for seconds. The one
exception is the Account page a player signs up and gets a login code on ([`accounts.md`](accounts.md), M59).

## The parts

```text
 game servers ──heartbeat──▶ MASTER ◀──heartbeat── TV nodes (engine/tv, the broadcast layer)
      ▲                         │                      ▲  │
      │ status query            │ list query           │  │ demo list, finished demos
      │                         ▼                      │  ▼
      └──────────────────── HUB (one process) ─────────┘  ARCHIVE (files by SHA-256 + an index)
                               │
                       HTTP: pages, JSON API, live updates
                               ▼
                           browsers ── Join / Spectate / Watch ──▶ the game's own client
```

| Part | Home | What it is |
| --- | --- | --- |
| The status record | `engine/net` (`status.hpp`), filled by `engine/server` | What any server says about itself, generic: name, game and version, map, mode, phase and time left, slots, players (name, team, score, ping, minutes, bot, colours), teams, spectators, its broadcast streams, location, every key. A game adds a blob of its own (its stats line, its extra columns) |
| The master | `engine/net` (beside the rendezvous) | One master for every kind of node: game servers, TV nodes and forwarders heartbeat the same service, told apart by a kind field. Forgets a silent one after three missed beats, answers a list query by game id, paged. The rendezvous already lists hosted sessions (`rendezvous.hpp`); the master is its list made public for a dedicated server and a TV node, one executable a community runs |
| The broadcast and the demo | `engine/tv`, `engine/replay` (the TV thread's design) | A broadcast is a replay file being written; every stream and file opens with a generic header (game, version, map, mode, players, teams, scores, start) and carries a small META track (scores and roster as they change); a finished file ends with an index and a final summary. The hub reads the header and the meta track only: it never simulates a game |
| The hub | `engine/hub`, built per game by `ge_add_hub` | One process: the poller, the index in memory, the demo archive, and an HTTP server for the pages, the JSON API and live updates. A game's hub binary links the game's `HubSpec` (its words) and nothing of its simulation |
| The classic adapter | `engine/hub/browse.hpp`, `games/shooters_edge/hub` | A game's own masters and status queries (Shooter's Edge's: the classic ones) and later its broadcasters' pages read into the same records, so Shooter's Edge's hub lists the classic servers already on the internet beside its own |

### The status record and modes

Every game says the same things about a match, so the record is the engine's: the phase is the
server core's (`engine/server`: lobby, countdown, playing, intermission) with the time left; a
team's score is the sum of its players'. What differs by game is only words and columns:

- **Modes**: a game names its modes (Shooter's Edge: 1on1, 2on2, 4on4, ffa, ctf, coop, race;
  Fall Line: race, free ride, duel) and derives one from its settings in a function of the
  `HubSpec`. The hub's filter buttons are that list.
- **The score's column**: frags, a time, a points total or a lap; how it reads and sorts.
- **Interest**: the order of the cards. A generic rule: full matches first, more players first,
  then the mode's weight the game gives (a 4on4 above an ffa), divided down by the share of bots,
  zero for a match sitting at the score screen too long. Measured on the classic hub (the note's §
  5), it is what puts the match people want at the top.
- **The title**: "tag / mode: red (a, b) vs blue (c, d) [map]", built by the hub from the record.

### How the hub learns the state

- **Masters**: each configured master's list every 4 hours, and whenever a heartbeat list changes.
- **Servers**: every known server's status every 30 s; servers with a human playing every 4 s.
  All queries of a round run at once, paced so a thousand servers do not burst; a server silent
  for the round drops out of it.
- **Broadcasts**: a server's status record names its streams (node address, stream id, delay,
  viewers). For a stream shown on the Watch page the hub subscribes to its META track over the TV
  layer and pushes each change to the browsers watching (a server-sent event stream: one
  connection, text events, reconnects by itself).
- **Demos**: each TV node keeps a list of its finished files with their hashes; the hub pulls new
  ones every minute, verifies the SHA-256, reads the header and the closing summary, and files them.

### The JSON API

Read-only, CORS open, every response cacheable for its refresh time; the pages use nothing else,
so another community's tools can too.

| Path | Answer |
| --- | --- |
| `/v1/servers?mode=&min_clients=&type=` | The status records, sorted by interest |
| `/v1/servers/{address}` | One record; 404 when unknown |
| `/v1/servers/{address}/events` | Server-sent events: the record and its meta changes as they come |
| `/v1/streams` | Every live broadcast: node, stream, server, delay, viewers |
| `/v1/masters`, `/v1/masters/{address}` | The masters and each one's list |
| `/v1/demos?mode=&map=&player=&team=&tag=&server=&days=&page=` | Demo rows, newest first, 15 a page, with a total |
| `/v1/demos/{sha256}` | One demo's summary: the header, the final scores, the durations, the game's stats |
| `/d/{first 3 hex}/{sha256}.{ext}` | The file itself and its summary as JSON; immutable, cached a year |

### The archive

Demos are stored by the SHA-256 of their bytes under a three-character fan-out
(`d/abc/abc…`), so a demo has one name everywhere, the same demo from two nodes is stored once, and
every file is immutable and cacheable for ever. The index is held in memory and rebuilt from the
files when the hub starts; the operator's own facts (when a demo came, its tags) are one text file
beside them, `archive.tsv`. Its rows are what the header and the trailer say, never what a file name
says. A hub keeps demos for a time its operator sets (a size or an age), older ones first, a tagged
match (a league's) kept until removed by hand.

**The hub's meta words.** A recorder that wants its demos found writes these keys in the replay
file's header meta (`engine/replay/stream.hpp`; a key may repeat): `map`, `mode`, `server` (its
name), `start` (Unix seconds), `title`, `team` (a team's name), `player` (`name` or `name<TAB>team`),
`tag`; and in the trailer's final meta: `duration_ms`, `score` (`name<TAB>score`) and `team_score`
(`team<TAB>score`). Anything else is the game's own and the hub leaves it unread.

### Locations

A server's country comes from an IP-to-country table the operator drops beside the hub (a free
one, refreshed by the operator); a server may also say its own country in its record, which wins.
No lookup service is called at run time.

### Hosting it

The hub is one executable and a directory (the archive, the index, the table, the page templates).
It speaks plain HTTP on one port; TLS and a CDN are the operator's reverse proxy's job (the
`docs/networking.md` rule: no hosted service, a community runs what this tree builds). A hub, a
master and a TV node can share one small machine. The labs run all of it on the simulated network.

## Watching in a browser

A hub page shows a match with the ENGINE ITSELF: a viewer built from the same C++ as the game,
compiled to WebAssembly, drawing through WebGPU, loaded by the Watch and Demo pages. It is the one
web build the engine has (G5): a viewer, never the game, and nothing in it plays.

| Piece | What it is |
| --- | --- |
| The web viewer | A game's viewer target, `ge_add_web_viewer(<game>)`: its renderer, its demo reader and the TV client, nothing of input beyond the camera and the transport. Built by Emscripten, drawing through the browser's WebGPU with the same WGSL the native renderer uses (Dawn ships the browser's binding) |
| Its stream | The TV node's stream carried over a WebSocket (the TV layer's browser carriage), the same messages a native spectator reads: the viewer says hello, turns the messages back into the replay file as it grows (`tv::Watch`) and unpacks each whole block into the recording as it comes (Shooter's Edge's `unpack_more`); it plays two blocks behind and holds its last frame when the next block is late. A demo is fetched by its hash from the archive, decoded in the viewer |
| Its content | A demo's own content pack (`engine/hub/content.hpp`): the game names the files the demo draws from (`HubSpec::demo_files`: Shooter's Edge's maps, precached models and sounds, and the basics every picture needs), the hub takes them out of the packs beside the viewer (a later pack wins, as a game folder's numbered packs), writes them as one pack, zstd-packs it and keeps it in the archive's `content/` named by its own SHA-256, served at `/content/<sha>.pak.zst` and cached by the browser for a year. It is made when the demo arrives, so no visitor waits; two demos that need the same files share one pack. A 20-minute 4on4 on the classic game's third level takes 1.6 MB instead of the two packs' 52 MB. A game that names no files hands the viewer every pack beside it |
| Its controls | Seek, speed, pause, whom to follow and free camera, through the transport the native replay player already has (`engine/replay`); clip links (`?t=from-to:who`) |
| The page glue | The few lines of script a page needs to start the viewer, hand it the stream's address and wire the buttons: platform glue, kept as small as the platform allows, as Apple's and Android's app shells are |
| Content rights | A hub serves only content its operator may share. For the classic game that is the free pack and the operator's own; a level whose content the hub may not serve is shown in the installed game instead |

THE BUILD. The `web` preset (`cmake --preset web`, with `EMSDK` set: Emscripten's toolchain)
sets `GE_WEB` and builds only `GE_WEB_GAME`'s viewer (`shooters_edge` by default) and what it
links: no host tools, no packaging, no encoders, no window layer. Dawn's own browser binding
(Emscripten's `emdawnwebgpu` port) stands in for the native Dawn library, so `engine/gpu` and
`engine/render` compile unchanged but for the surface, which a web build makes on the page's
canvas (`NativeWindow::Kind::canvas`). `engine/web` holds what every viewer shares: its arguments
(`viewer_args.hpp`, tested natively) and the page's calls (`browser.hpp`: a file fetched from the
hub, the next paint, the canvas's size, the status line). Every wait (a fetch, a paint, the GPU's)
suspends the C++ stack through JSPI, so a viewer is a plain loop in `main`. `ge_add_web_viewer`
writes `<game>_viewer.js` and `.wasm` into `build/web/<game>_viewer/`, the folder a hub's
`--viewer=<dir>` serves; the hub's operator puts the content packs it may share beside them
(`.pak`, `.pk3`); the Watch page hands them to the viewer by name order, the Demo page the demo's own content pack.

THE LIMITS. A browser offers WebGPU's default limits unless the machine's GPU earns it more:
16 sampled textures a shader stage, 8 storage buffers and 256 layers an array texture, among
others. The software adapter in the container offers exactly those. A pipeline layout counts every
entry its stages can see, used or not, so the frame's group 0 (17 entries, 11 of them textures)
is built whole for the passes that read most of it and in lean parts for the rest
(`render::FrameGroup`: `block` for post, `surface` for the classic map, `water` for the water),
each keeping its bindings' own numbers so one WGSL source serves both; the parts come from what
each entry point statically reads (Tint's binding inspector). The shadow cubes are as many as the
device's array layers hold (42 in a browser, 48 elsewhere), and the cull reads its chunks' sizes
and states from one buffer. Two browser refusals the native device lets pass are taken out at the
source: a render pass's NaN depth clear value (`gpu::begin_render_pass` gives it 0) and timestamp
queries (not asked for on the web). A native run asks for exactly the defaults with
`GE_GPU_LIMITS=default` (validation on), so a pipeline over them names itself in the log without
a browser.

Two cheaper ways stay in the plan beside it: Watch and Play buttons that open the installed game
(`<game>://tv/<node>/<stream>`, `<game>://demo/<sha256>`, a link each client registers), and the
live overhead map (a RADAR track the hub draws), for phones and slow machines. Video from a hub
camera was considered and not taken: a CPU-rendered client per stream costs more than the
browser's own GPU.

## The plan

| Phase | Row | What lands | Shown by |
| --- | --- | --- | --- |
| 0 | M56 | This design and the clean-room note | The plan page |
| 1 | M56a | The status record in `engine/server`; the master (heartbeats from game servers, TV nodes and forwarders, one service told apart by kind; the list query, expiry) | `hub-master` lab: 40 servers and 6 TV nodes on the simulated network, one dying, the list each second |
| 2 | M56b | `engine/hub`: the poller, the index, an HTTP/1.1 server with server-sent events, the JSON API, `ge_add_hub`; Fall Line's and Shooter's Edge's `HubSpec` | `hub-api` lab: a scripted evening, the API's answers checked, digests |
| 3 | M56c | The pages, rendered by the hub as HTML from templates (no build step): Servers, Players, Scoreboard; the game's words and the map pictures | `hub-pages` lab: each page screenshotted headless in Chromium at phone and desktop width |
| 4 | M56d | The archive: pulled from TV nodes, SHA-256 names, the SQLite index and search, the Demos page, downloads, retention | `hub-archive` lab: 500 generated demos, searches answered and timed |
| 5 | M56e | Open in the game: each client's link scheme and `+tv`/`+demo` start; Watch and Play buttons | A Shooter's Edge client opened from a hub link, watching a bot match |
| 6 | M56f | The classic adapter: the classic masters, status queries and broadcasters read into the records; Shooter's Edge's hub lists the internet's classic servers | `se-hub` lab against the simulated classic servers, and once by hand against the real masters |
| 7 | M56g | The web viewer: `ge_add_web_viewer`, the Emscripten and WebGPU build, the stream over a WebSocket, demos by hash, the content folder, the transport; the Watch and Demo pages; Shooter's Edge's and Fall Line's viewers | `hub-viewer` lab: the viewer in headless Chromium on a software WebGPU adapter, a live bot match and a demo screenshotted, seek checked, first-visit bytes measured |
| 7b | M56g2 | The renderer within WebGPU's default limits (16 sampled textures a stage, 256 array layers) on the web build: the frame's group split by what each stage reads, the shadow cubes capped by the device; the picture on the canvas | `hub-viewer` lab: the match drawn on the canvas in headless Chromium |
| 8 | M56h | The live map for phones and slow machines: the RADAR track, the hub's SVG view, live updates | `hub-radar` lab: a bot match drawn frame by frame |

### Built so far

- **M56a** (`hub-master`): `engine/net/status.hpp` is the record (version 1, rows left out from
  the end with a `truncated` flag when it outgrows the datagram) and its ask: a padded 64-byte ask,
  answered first with an 8-byte-heavy cookie (a keyed hash of the asker's address and a 10 s
  bucket) and only then with the record, so a forged source address gets nothing bigger than its
  ask. `engine/net/master.hpp` is the master (`master_init`, `master_update`), the beat a node
  sends (`MasterBeat`: every 30 s, an ack, three byes on close) and the paged list query
  (`MasterQuery`: 20 entries a page, the ask padded to 512 bytes, at most 64 entries per host).
  A dedicated server beats to every master in `sv_master` (a list, default port 27900) and
  answers status asks on its own port with `server_public_status` plus the game's
  `ServerGame::fill_status`.

- **M56b** (`hub-api`): `engine/hub` is the hub. `poller.hpp` reads every master's list (each
  60 s) and asks each listed node for its record (each 30 s, each 4 s while a human plays there),
  keeping one `HubNode` a node with its last record and a count of its changes; a node no master
  lists is dropped once its record is 90 s old. `spec.hpp` is a game's `HubSpec` (its name,
  protocol, modes and their weights, the score's column) and the interest rule. `api.hpp` answers
  `/v1`, `/v1/servers` (filters `mode`, `min_players`, `has_players`, `kind`), `/v1/servers/{address}`,
  `/v1/servers/{address}/events`, `/v1/streams`, `/v1/nodes` and `/v1/masters` as JSON, a pure
  function of the index. `http.hpp` carries it: HTTP/1.1 GET and HEAD (and POST to the Account page's `/account/...` alone, a form of at most 2 KB), CORS open, the cache time
  of the busy period, server-sent events for one node's record with a comment line every 15 s.
  `main.hpp` is the process (`ge_add_hub`, cmake/Hub.cmake); `--serve-master` runs the master in
  the same process, so one binary is a community's whole service. Shooter's Edge's is
  `shooters_edge_hub`; Fall Line has no dedicated server yet, so no hub.

- **M56b, part two**: a server whose game speaks its own protocol (`ServerGame::serve`, Shooter's
  Edge's classic one) beats and answers too. The core sets the socket's public hook
  (`Transport::on_public`, net/transport.hpp): every datagram is offered to the core first, which
  takes the master's acks and answers the hub's status asks, so the game never sees either.
  Shooter's Edge's record has the map, the mode its rules make (coop, 2on2 or 4on4 with teamplay,
  1on1 or ffa), the score screen, the frag and time limits and every client (bots and spectators
  flagged). `shooters_edge_server +set sv_master <hub>` appears on `shooters_edge_hub`.

- **M56c** (`hub-pages`): `pages.hpp` writes the pages from the same index, as plain HTML with
  one stylesheet and no script, refreshed every 5 s by a meta refresh. `/` is the Servers page: a
  card for each server with anyone on it in the interest rule's order (the map and phase with the
  time left, the name, mode, playing over slots, bots, country, the scoreboard with teams first,
  spectators and watchers, and Join, Spectate and Watch links in the game's scheme), the mode
  filters (`/?mode=`) above and the empty servers listed after. `/players` lists every person, people
  before bots; `/scoreboard/{address}` is one server's scoreboard for a streamer's overlay;
  `/style.css` is dark by default and light when the reader's system asks. Every text a server sends
  is escaped. `hub_answer` routes a path to the API or a page.

- **M56d** (`hub-archive`): `archive.hpp` is the demo archive: replay files kept by SHA-256 under
  `d/abc/`, each row read from the hub's meta words, the same file from two places kept once,
  `archive.tsv` for the operator's tags. `--archive=<dir>` turns it on; a recorder drops `.ged`
  files in `<dir>/inbox/` (a refused one goes to `inbox/refused/`), and `--keep-gb` and
  `--keep-days` prune the oldest untagged demos. `/v1/demos` searches by player (any part, any
  case), mode, map, team, tag, server and age, newest first, 15 a page, with the scores left out;
  `/v1/demos/{sha256}` is one demo's summary with them; `/d/abc/{sha256}.ged` is the file, cached a
  year. `/demos` is the page: a search form, Play (`<id>://demo/<sha256>`) and Download, the
  scores folded until asked. Pulling demos from TV nodes waits for the TV node (M57b).

- **M56e**: the buttons open the game. `link.hpp` reads and writes the links (`<id>://join/`,
  `spectate/`, `tv/<node>/<stream>`, `demo/<sha256>?hub=<host>`, the hub named from the request's
  Host header), fetches a demo by its hash over HTTP and plans the scheme's registration per
  system. The game CLI's `link register` and `link open` (docs/gamecli.md § A hub's links) carry
  them out: Shooter's Edge joins, spectates (`set spectator 1`) and plays a hub's demo; Watch waits
  for the game's TV client (M57b/M57c).

- **M56f** (`se-hub`): the classic adapter. A game whose servers already speak a status protocol
  of their own gives the hub its query (`HubSpec::browse`, the engine's `net::ServerQuery`), and
  `browse.hpp` reads them the way the game's server browser does: `--browse-master` (the game's
  masters) and `--browse-server` (servers by hand) are asked every 30 s on a second UDP socket,
  and each answer becomes a record (`status_of`: the name, map, mode and slots, the roster with
  spectators flagged, the teams' frags added up, the phase from what the server says of its match). A status query that says nothing of
  bots (the classic one) lists a bot as a player; the engine's own record flags them.
  Such a node is `browsed`: the poller never sends it the engine's ask, and it is dropped once no
  refresh has heard it for 5 minutes; if the engine's master lists the same address it becomes the
  poller's, and its own record wins. Shooter's Edge's hub hands it the classic query, so
  `shooters_edge_hub --browse-master=<a classic master>` lists the classic servers on the internet
  beside the game's own. The classic broadcasters' pages wait for the TV node (M57b).

- **M56h** (`hub-radar`): the map. The radar track (`replay/radar.hpp`) holds a roster chunk,
  every player's place, heading and death four times a second, and the level's outline when the
  game has one; `stream_add_track` adds it to a finished file without touching its other tracks.
  `/radar/{sha256}` (`hub/radar.hpp`) draws it as SVG: the outline, the faint trail of every player
  over the whole match, and the dots in their team's colour, one minute animated by SVG's own
  animation (no script) or still at a moment (`?at=`) with links a second and five seconds either
  way; the Demos page links Map where the file has a radar. Shooter's Edge reads the radar off a
  multiview recording (`client/demo_radar.hpp`), with the outline from the level's map sliced at the
  height played most, and `shooters_edge_cli demos pack` writes it. A two-minute 4on4 of bots adds
  26 KiB (11 %) to its file. A live match's map waits for the TV node to carry the track.

- **M56g, the pages** (`hub/watch.hpp`, in `hub-pages`): `/watch/{tv-node}/{stream}` holds the
  viewer's canvas fed by the TV node's WebSocket (its status record names the port, key `web`), the
  match's scoreboard from the server whose record lists the stream, the other live broadcasts, and
  Open in the game; `/demo/{sha256}` holds the viewer on the demo's `/d/...` file, with Play in the
  game, Download and Map. The cards' Watch and the Demos page's Watch open them. `/viewer/{file}`
  serves the viewer's `.js`, `.wasm` and data from `--viewer=<dir>`. A hub with no viewer, or a node
  that takes no browser, shows the reason in the picture's place. The script is the few lines
  that start the viewer. The viewer itself (the engine built by Emscripten) is next.

Phase 2 and later depend on the TV layer's header and meta track (M57a, the replay file v2, and M57b, the native TV node); the
master can land before them, since a dedicated server's heartbeat needs nothing of TV.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| One hub for every game | `engine/hub` with a game's `HubSpec` of words and columns, built per game | A hub per game written again: Fall Line and Shooter's Edge would differ only in words |
| Where the state comes from | The servers' own status records, polled; the broadcasts' meta tracks for the watched ones | A hub that joins as a spectator: it would need each game's simulation |
| The master | Its own small heartbeat and list protocol next to the rendezvous, plus the classic adapter for the classic game | Only the classic master protocol: a list of addresses with nothing of game, version or streams |
| Demo metadata | Read from the file's header and closing summary | File names, as the classic hub's older list did: they hold no durations, no points of view, and break when renamed |
| Demo names | SHA-256 of the bytes, a fan-out directory | Names by date and players: two nodes' copies differ, and a URL is not stable |
| The index | In memory, rebuilt from the files at start (M56d measured 500 demos searched in under a millisecond); SQLite with full-text search when an archive outgrows memory | A database server: one more process for a community to run; SQLite from the start: a dependency before an archive needs it |
| Pages | HTML rendered by the hub from templates, CSS, a few lines of script only where a page moves or starts the viewer | A single-page app with a build step: a second language and toolchain in the tree |
| A match in the browser | The engine's own viewer in WebAssembly and WebGPU (the maintainer's choice, 2026-11-11) | Only a link to the installed game: no picture without installing. Video from a hub camera: one CPU-rendered client per stream. The live map alone: no real picture |
| TLS | The operator's reverse proxy | TLS in the hub: certificates are an operator's job and every proxy already does it |
| Locations | An operator's IP-to-country table file, or the server's own word | A lookup service called at run time: a dependency and a privacy leak |
