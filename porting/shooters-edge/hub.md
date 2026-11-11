# The classic community's live-game and demo hub: facts for our hub (M56)

**WORKING NOTE** (clean room): written by a reader agent from the public sources and the live
services on 2026-11-11, screened for code. The design built from it is `docs/hub.md`, which
names none of this. The reader's own preamble follows.


Reader's facts document. No source is quoted. Each fact is tagged:
[R] read in the public source named below, [M] measured against the live service on 2026-11-11,
[D] derived from read or measured facts. "Open" means neither the source nor a measurement settles it.

## Projects read (name, licence, commit)

| Project | What it is | Licence | Commit read |
| --- | --- | --- | --- |
| quakeworldnu/hub.quakeworld.nu | The hub's web front end | MIT | 2a8524c (2026-11-11) |
| vikpe/qw-hub-api | The earlier hub API (still live at hubapi.quakeworld.nu) | MIT | a77f258 (2026-08-23) |
| vikpe/serverstat | Go library: query one QuakeWorld server (used by qw-hub-api) | MIT | e787de0 (2026-08-23) |
| vikpe/masterstat | Go library: query master servers | MIT | 11dd8c7 (2024-04-25) |
| vikpe/udpclient | Go UDP request/response helper used by both | MIT | 2f926c0 (2023-04-26) |
| quakeworld/serverstat | Rust crate: query one server; its JSON shape matches today's api.quake.world | MIT | 946ca2f (2025-04-11) |
| quakeworld/masterstat | Rust crate: query master servers | MIT | 09789b1 (2025-08-13) |
| vikpe/qw-servers-geoip | Daily IP-to-location table the hub reads | MIT | d41da40 (2026-11-11) |
| quakeworld/fteqtv.quake.world | Deployment of the FTE QTV proxy that browsers reach over websocket | MIT | b003736 (2025-03-20) |
| QW-Group/qtv | QTV proxy, Go rewrite ("QTVGO") | BSD-2-Clause | 1326095 (2026-11-10) |
| QW-Group/qtv-obsolete | The older C QTV (only its README was read; it is marked obsolete) | GPL-2.0 | a9efad0 (2025-11-17) |
| vikpe/fteqw, branch newtube | FTE fork built to WebAssembly for the hub (only its hub README and file list were read) | GPL-2.0 (FTE) | fe02987 (2026-11-05) |
| vikpe/fte-web-assets | Old copy of the web player's content files (listing only) | none stated | 19191b3 |
| vikpe/qw-data, vikpe/go-qwhub | Cloned, not needed for any answer | MIT | 297994b, 6e440a2 |

Not public (searched, not found): the source of today's api.quake.world service itself, and the
indexer that fills the demo database and the demo file store. Where answers depend on them they come
from the front end's use of them and from live measurement.

Important context [R][M]: the hub front end no longer talks to qw-hub-api. Since its current version it
reads live servers and streams from api.quake.world/v1 (a separate service whose JSON shape is exactly
the Rust serverstat crate's), recorded games from a Supabase (PostgREST) database, and demo files from a
CDN. qw-hub-api still answers at hubapi.quakeworld.nu/v2 [M] and is described below because it is the
readable reference for scraping, demos from QTV and streams.

---

## The site

### 1. Pages and what each shows

The site is a set of separate pages (one build entry per page), each a single-page React app [R]:

- **/ (Servers)** [R]. A grid of server cards, only servers with at least one player or spectator. Each card
  shows: the map's screenshot as background, a title, the mode, the map, a status name and description,
  a progress bar of elapsed against total minutes when the game is "Started" and has a timelimit, the
  scoreboard (players with name, team, frags, ping, top/bottom colours; team rows in teamplay), the
  spectator names (server spectators plus QTV viewers, cut to 88 characters with ".."), the address shown
  as "host:port" with a copy button and a country flag, the KTX version, and buttons: Join (a qw:// link
  to the address), Spectate (qw://address/observe), QTV (qw://N@qtvhost:port/qtvplay), Watch QTV in the
  browser (/qtv/?address=...), a pop-out scoreboard (/scoreboard/?address=...), and "lastscores" (recent
  results on that server; offered only when the server's version string is MVDSV 0.36 or later).
  Players beyond 8 (teamplay) or 10 (otherwise) are hidden behind a "+N" count. Cards get extra styling
  for a match tag, an official league server, free player slots and a LAN-party host.
  Beside the grid: a league queue box, a donation goal, upcoming scheduled games, and live Twitch streams.
  A filter panel (stored in the browser) chooses modes 1on1, 2on2, 4on4, ffa, race, fortress, other,
  regions Africa, Asia, Europe, North America, Oceania, South America; and whether bot-only servers show.
  The header shows counts: servers, QTV streams, "N players, M spectators".
- **/qtv/** [R]. A live in-browser player (FTE in WebAssembly) on the left and, unless embedded, a sidebar
  of servers that have a QTV stream and players, sorted by score (detailed or brief view). `?address=` picks
  the server; otherwise the highest-scored one. A footer under the player shows map, status, title and
  Join/Spectate/QTV links. The old /qtv-popout/ path redirects to /qtv/ keeping the query.
- **/players/** [R]. A table of every client on the (filtered) servers: name, status ("Playing",
  "Spectating", "Spectating (QTV)"), server address and title, and links QTV / Spectate / Join. Bots
  among players are left out. Sorted by name, case-insensitive.
- **/games/** (Demos / Recent games) [R]. A browser of recorded games from the database, grid or list,
  15 per page. Filters: mode (All, 1on1, 2on2, 4on4, Wipeout, CTF), player search, team names, map,
  match tag, server hostname, and timeframe (last day, week, month (default), 3 months, year, all time),
  plus a "show scores" spoiler switch. Each row shows timestamp, mode, map, the teams/players and
  scores (hidden until revealed), a Play link (/games/?gameId=N) and a Download link when a demo exists.
  With `?gameId=` the same page shows the demo player (see 9), title, date and server, a scoreboard and
  KTX stats (both behind a spoiler click), a clip editor, Share and Download.
- **/demo-player/** [R]. A bare player for embedding: `?demo_sha256=` plus optional `width` and `height`
  in pixels.
- **/scoreboard/** [R]. A 480 by 400 pixel embeddable scoreboard for `?address=`; shows "Server has no
  players." when the API has no game server there.

Per server fields available to the page: see 2 (api.quake.world shape). Per demo/game fields: database
row id, timestamp, mode, matchtag, map, teams, players, demo_sha256, server_hostname; and from the
per-demo info file: filename, timestamp, mode, map, matchtag, teams, players, the recorder's point of
view, demo, countdown and match durations, server hostname/address/ip/port/serverinfo, source filename
and source URL (see 9 and 11) [R].

Not on the current hub [R]: news, forum posts, wiki changes and events. qw-hub-api still has scrapers
for these (see 13) but its routes for them are commented out; they return nothing [R].

### 2. How the front end gets data

All polling, no push for hub data; one SSE stream for a third party [R]:

- **Servers**: GET `https://api.quake.world/v1/servers?server_type=game&min_clients=1`, every 5 s while
  the page is visible, paused when hidden, refetched at once on focus [R]. The client also drops servers
  whose players plus spectators is 0 [R]. Response: JSON array; each game server has `server_type`
  ("game"), `software_type` (mvdsv, fte, unknown, z_quake seen), `address` ("ip:port"), `ip`, `port`,
  `player_mode` (e.g. 1on1, 2on2, 2on2on2, 4on4, ffa, coop, or null), `game_mod` (e.g. fortress, ctf,
  tot, wipeout, smash, instagib, arena, or null), `info` (the full serverinfo as string pairs),
  `player_limit`, `spectator_limit` (or null), `total_spectator_count`, `score`, `teams` (name, frags,
  top_color, bottom_color, ping (may be null)), `players` (name, team, frags, top_color, bottom_color,
  user_id, ping, time (minutes), bot (null or an object with skill), auth (null or a string)),
  `spectators` (name, ping, time, bot, auth), `qtv_stream` (null or id, name, number, address, url,
  client_names). Every name is an object with `ascii` and `unicode` renderings [M][R].
- **One server**: GET `/v1/servers/{address}`; 404 with empty body when unknown, 400 for a malformed
  address, accepts a hostname ("quake.se:28501" resolved and answered with the ip address) [M]. The
  scoreboard page polls it every 5 s and keeps polling in the background [R].
- **Lastscores**: GET `/v1/servers/{address}/lastscores` once per open, no polling [R]. Returns an array
  of KTX stats records for the whole host (all ports); the client keeps entries whose `port` equals the
  server's port and that have players [R][M]. Fields used: date, mode, map, demo, players (name, team,
  top-color, bottom-color, ping, stats.frags, bot) [R]. Measured: 12 entries, KTX stats version 3 [M].
- **Twitch streams**: GET `/v1/vendor/twitch-streams`, every 60 s, refetch on focus. Response wraps
  Twitch Helix "streams" records under `data` (id, user_id, user_login, user_name, game_id, game_name,
  type, title, viewer_count, started_at, language, thumbnail_url, tags, ...). Sorted by viewers; then
  "highlighted", then "featured" (a login-to-in-game-name list kept in the front end), then the rest [R][M].
- **Other api.quake.world paths seen** [M]: `/v1/masters` (list of master addresses),
  `/v1/masters/{host:port}` (that master's server list, 300 addresses measured),
  `/v1/servers` unfiltered (all types: 652 entries measured: 437 mvdsv, 37 fte, 79 qtv, 93 qwfwd, 1 other
  proxy, 5 other), `server_type` filter accepting game, proxy, qtv, generic (anything else: 400 with a
  text error naming the allowed values), `min_clients`. Unknown parameters (limit, has_client,
  player_mode, software_type) are ignored. QTV entries carry info (hostname, maxclients, *version),
  client_count, client_limit, clients; proxy entries the same. `/v1/vendor/ko-fi?project=` feeds the
  donation goal [R]. No `/v1/demos` or `/v1/streams` (404) [M].
- **Geo table**: the raw JSON file from the qw-servers-geoip repository, fetched at most once an hour
  per page [R] (see 7).
- **Games index**: Supabase PostgREST, view `v1_games` (columns id, timestamp, mode, map, matchtag,
  teams, players, team_names, players_fts, server_hostname, demo_sha256, demo_source_url) with the public
  "anon" key in the `apikey` and `Authorization: Bearer` headers [R]. Searches: equality on mode and map,
  "contains" on team_names (lower-cased, space separated), full-text on players_fts (each word quoted,
  joined with AND), case-insensitive substring on matchtag and server_hostname, timestamp at least now
  minus maxAge days; ordered by timestamp descending; pages of 15 via a row range; a separate exact
  count request with no body for the pager [R]. One game by id for the player page [R]. Fetched on demand,
  never polled; cached 60 s after last use, no retries [R].
- **Demo files and metadata**: the CDN (see 9).
- **League queue**: an EventSource (SSE) on `https://qwleague.com/api/public/stream`, event
  `queue_stats` (waiting, by_mode, by_mode_region, by_mode_continent, matchmaking_enabled). Reconnects
  on a closed stream with backoff from 2 s doubling to 60 s [R].
- **Scheduled games**: GET a cloud function `getScheduledGames?limit=10` with `status=upcoming` (and
  optional teamTag); reply `success` plus `matches` (teamA, teamB with tag and logo_url, gameType,
  scheduledDateTime) [R].
- Images: map screenshots `https://media.quake.world/mapshots/{640|1280|1920}/{map}.webp`, fallback
  `https://a.quake.world/mapshots/default.jpg`; flags `https://flagcdn.com/h24/{cc}.webp` [R].

Front-end query defaults: data never goes stale by itself (only polling refreshes it), unused data is
kept 60 s, no retry, no refetch on reconnect [R].

### 3. Stacks

- **Front end** [R]: React 19 with TypeScript, built by Vite into several HTML entry points, Tailwind CSS,
  TanStack Query for fetching, Biome lint, Vitest. Deployed by GitHub Actions on every push to main to
  **Firebase Hosting**; all paths rewrite to the page HTML; HTML is served "max-age=0, must-revalidate",
  hashed JS/CSS and fonts "max-age=31536000, immutable", other assets 1 hour [R]; measured responses carry
  Fastly cache headers [M]. Environment values: the demo CDN URL, the asset CDN URL, the Supabase URL and
  the anon key [R].
- **api.quake.world** [M][D]: behind Cloudflare; JSON errors in the text form of the Rust axum
  framework; `cache-control` max-age 5 s on server lists, 30 s on lastscores, 60 s on masters and on
  Twitch streams, no-store on a single server; CORS allows any origin and exposes all headers [M]. Its
  per-server JSON matches the Rust serverstat crate field for field [D]. Source not public (open).
- **qw-hub-api** [R]: Go, Fiber web framework; middleware for panic recovery, permissive CORS, response
  compression, and an in-memory response cache keyed by full URL, 2 s by default, overridable per route
  (servers and server groups 60 s, demos 5 min, lastscores/laststats 1 min, masters 6 h, streams none).
  No database: everything lives in memory. Listens on the configured port; on port 443 it serves TLS
  from a certificate and key file. Config is a JSON file; Twitch credentials come from the environment.
- **Games database**: Supabase (PostgreSQL behind PostgREST 13). Tables visible in the generated schema:
  demos, events, games_v2, maps, matches, names, profiles, series, stages, tournaments, and the view
  v1_games; trigram and unaccent extensions are installed [R].
- **Demo store**: d.quake.world (CloudFront per the variable names); assets a.quake.world (Amazon S3,
  90 day max-age) and assets.quake.world (Cloudflare, one year) [R][M].
- **QTV websocket gateway**: nginx in front of FTE's QTV proxy built from FTE source, in Docker (see 8).

---

## Live servers

### 4. Finding servers and querying them

**Masters.** Configured lists: qw-hub-api sample uses master.quakeworld.nu, master.quakeservers.net,
qwmaster.fodquake.net, all on UDP port 27000 [R]; api.quake.world reports master.quake.world,
master.quakeservers.net, qwmaster.fodquake.net, port 27000 [M]. QTVGO registers itself on
"master.quake.world master.quakeservers.net" by default [R].

**Master query** [R]: one UDP datagram of 3 bytes: 0x63 ('c'), 0x0A (newline), 0x00. Reply begins with
the 6-byte header 0xFF 0xFF 0xFF 0xFF 0x64 ('d') 0x0A, followed by records of 6 bytes each: 4 bytes of
IPv4 address in network order, then a 2-byte port, big-endian. Parsing stops at a short record. Example
from the crate's tests: bytes 192 168 1 1 0x75 0x30 is 192.168.1.1:30000. Addresses from several masters
are merged, de-duplicated and sorted as text [R].

Edge cases [R][D]:
- The Go client reads datagrams until an 800 ms read timeout, concatenates them, and checks and strips the
  header only at the very start; any later datagram's own 6-byte header would then be read as a bogus
  address (255.255.255.255:25610) [D]. The Rust crate reads with a 64 KiB buffer and a caller timeout
  (2 s in its examples) [R]. The Rust tests expect at least 300 addresses per major master [R]; measured
  300 from master.quake.world through the API [M].
- Go client defaults: 8192-byte buffer, 3 attempts, 800 ms timeout per write and per read; a retry
  happens only if nothing at all was received [R].
- UDP from this container was blocked, so the bytes above are not measured here.

**Cadence in qw-hub-api** [R]: a 1 s ticker. Masters are asked at tick 0 and again every
`master_interval` seconds (sample: 14400 = 4 h). Every `server_interval` seconds (30) every known address
is queried and the index replaced; on other ticks every `active_server_interval` seconds (4) only
"active" servers (at least one client who is a non-spectator non-bot) are re-queried and merged. All
queries of a round run concurrently; non-answering servers are dropped from that round.
api.quake.world's cadence is open; its own cache max-age is 5 s [M].

**Status query** (two variants) [R]:
- Go library: the out-of-band packet four 0xFF bytes then the text "status 215" and a newline. The reply
  must start with four 0xFF, 'n', backslash [R].
- The number is a bit set. The Rust crate documents the bits as 1 serverinfo, 2 players, 4 spectators,
  8 spectators shown as players, 16 teams, 32 the QTV line, 64 flags [R]. 119 is all of these except 8,
  215 is 1, 2, 4, 16, 64 and a bit 128 the sources do not describe [D].
- Rust crate: four 0xFF then "status 119" (no newline), 64 KiB read buffer. Reply must start with four 0xFF
  and 'n' (0x6E) [R].
- Reply body: lines separated by newline. Line 1 is the serverinfo string: backslash-separated key/value
  pairs (a reply whose first line is shorter than "hostname\x" is rejected). Each further line is one
  client; a line beginning with "qtv " is the QTV advertisement (see 6) [R].
- Client line: space-separated tokens with double-quoted strings: user id, frags, connect time (minutes),
  ping, name, skin, top colour, bottom colour, then optionally team, then optionally a flag/auth country
  code, then (Go library) optionally a client type "b" bot or "h" human [R]. Example line:
  63 43 41 25 "ToT_Oddjob" "" 4 4 "red" "" [R].
- Spectators: the ping field is negative (Go: below 0; Rust: below 1, that is 0 or less) and the name is
  prefixed by the three characters backslash s backslash, which is stripped; the Rust crate zeroes a
  spectator's frags [R].
- Bots: Go library: client type "b" if given, else ping exactly 10, else a name in a short list of known
  bot names. Rust crate: a ping outside 12..600 ms counts as a bot [R]. api.quake.world instead reports a
  `bot` object with a `skill` number, and some bots show real-looking pings (56, 68) [M]; how it learns
  bot skill is open.
- Settings read: hostname, map, maxclients, maxspectators, teamplay, deathmatch, timelimit, fraglimit,
  status, mode, ktxmode, matchtag, *gamedir, *version, ktxver, hostport, needpass, and on proxies city,
  coords, countrycode [R][M].
- Server software from the first word of `*version`, case-insensitive: mvdsv, fte, fo (FortressOne) are
  game servers; qtv and qtvgo are QTV; qwfwd is a proxy; anything else unknown [R]. Measured examples:
  "MVDSV HBE 1.2.1", "MVDSV 1.20-dev-ezcsqc", "QTVGO 1.16-dev", "qwfwd 1.40-dev" [M].
- The address shown: if serverinfo has `hostport`, the Rust crate uses it as the server's address; the Go
  library adds `hostname_parsed`: the first word of the hostname if it looks like a domain (with a port
  kept to 5 digits, or the query port appended) [R].
- Per MVDSV server, extra queries: "status 32" (QTV line, Go) and "qtvusers" (see 6); and on demand
  "laststats N" (N default 10, at most 50) whose reply, after four 0xFF and "nlaststats ", carries a JSON
  array of KTX stats; datagram headers inside a multi-datagram reply are removed and trailing-comma
  glitches cleaned before parsing; buffer 800 KiB [R].

**Teams** [R][M]: built only when teamplay is above 0, by grouping players by team name: frags summed,
ping averaged (Go: over humans only; Rust: over all), colours = the most common top/bottom pair (Rust:
with fewer than 3 players, the first player's pair), sorted by name.

### 5. Modes and match state

**Mode, Go library** [R], in order:
1. `*gamedir` (default "qw"): "ktx-ffa" means ffa; any other non-"qw" gamedir is itself the mode
   (e.g. fortress).
2. `mode` key: if it ends in "race" it is race; if it contains "-" the part before is the mode and the
   part after the submode; else lower-cased as is.
3. `ktxmode` key, lower-cased.
4. Otherwise from settings: teamplay above 0 with teamplay 2 and maxclients 26, 24 or 12 is coop,
   teamplay 4 with maxclients 16 is ctf; teamplay 4 with deathmatch 5 is clan arena; else "NonN" with
   N = maxclients halved. Teamplay 0 with maxclients 2 is 1on1, else ffa.

**Mode, api.quake.world** [M]: splits it into `player_mode` (1on1, 2on2, 2on2on2, 4on4, ffa, coop, or null)
and `game_mod` (fortress, ctf, tot, wipeout, smash, instagib, arena, x76, ext, or null). The hub takes
game_mod, else player_mode, else the gamedir unless it is empty, qw, id1 or ktx [R]. Its own filter
buckets are 1on1, 2on2, 4on4, ffa, race, fortress, other [R].

**Match state** (same rules in the Go library and in the hub) [R], from the serverinfo `status` key that
KTX keeps ("Standby", "Countdown", "N min left"), the mode, the players and free slots:
- race: Standby, "Racing".
- status Standby, mode NonN or ffa, some player with frags above 0: if bots present and humans present:
  Standby "Score screen"; bots only: Standby "Waiting for players"; no bots: Started "Score screen".
- status Standby otherwise: "Waiting for K player(s)" when NonN and K free player slots, else
  "Waiting for players to ready up".
- status Countdown or containing " min left": Started; description "Game in progress" for coop and
  clan arena (hub: coop and ca), else the status text itself.
- empty status (non-KTX servers, measured on FTE servers): Unknown, "Playing" if players else
  "Waiting for players" (hub); any other text: Unknown with that text.

**Time** [R]: total = timelimit (minutes); remaining = the number before " min left" when present, else
total; elapsed = total minus remaining. The hub draws a bar only when Started and total above 0.

**Score** (ranking value, Go library; api.quake.world returns a `score` field whose values 0, 4, 10, 40,
60 were measured) [R][M]: 0 for no players or bots only, and 0 when the server is "idle" (no players, or
Standby, not race, not NonN with free slots, and every player connected at least clamp(1.5 times players,
3, 10) minutes). Otherwise a per-mode maximum (1on1 50, 2on2 70, 2on2on2 75, 3on3 75, 4on4 130,
wipeout 110, ffa 35, clan arena 40, ctf 100, coop 20, race 15, other 5), reduced when the server is not
full (two or more missing: 5 per present player; one missing: times the fill fraction cubed), divided
by the players' average "division" (from a built-in list of known names, 2.5 for unknown), then scaled
down by the bot fraction. The hub sorts by score, then Europe first, then address [R].

**Title** [R]: "matchtag / mode: participants [map]". Participants: for teamplay (not 1on1, not coop, at
most 2 teams) "team (p1, p2)" per team, or just the team name past 4 players, joined by " vs "; for 1on1
names joined by " vs "; otherwise by ", "; ffa shows no names. A matchtag is cleaned to letters, digits,
spaces and dashes, dropped when shorter than 3 or when it is a plain mode word or contains pause,
pausable or test.

### 6. QTV advertisement and the watch link

- **In the status reply** [R][M]: with the "show QTV" bit (32) an MVDSV server adds a line beginning
  "qtv " with four tokens: stream id, quoted stream title, quoted "N@host:port", and viewer count.
  Example from the Go library's notes: qtv 1 "qw.foppa.dk - qtv (3)" "3@qw.foppa.dk:28000" 0.
  N is the stream's number on that QTV, host:port the QTV's address. The Go library sends "status 32"
  separately and expects the reply to begin four 0xFF then "nqtv ".
- Misconfigured servers send an empty address: the Go library then takes the number in the last
  parentheses of the title and builds N@serverhost:28000 (28000 = the default QTV port) [R].
- Viewer names: the "qtvusers" packet (four 0xFF then "qtvusers"); the reply starts four 0xFF then
  "nqtvusers " then the stream id and the viewers' quoted names, ending at a newline. It is asked only
  when a stream exists (Go: only when the count is above 0) [R]. The API exposes them as
  `qtv_stream.client_names` [M].
- What QTVGO tells the server: its external `address` setting (host:port) is handed to MVDSV, which
  advertises it [R].
- **Watch links built by the hub** [R]: in-browser /qtv/?address={server ip:port}; desktop client
  qw://{N@host:port}/qtvplay; for past games on a server ("lastscores") a QTV on the same host at port
  28000 is assumed: download http://host:28000/dl/demos/{file} and stream qw://qtvplay file:{file}@host:28000.
  qw-hub-api's v1 endpoint built http://{qtv address}/watch.qtv?sid={id} [R].

### 7. Geo

- The table [R]: qw-servers-geoip runs daily at 06:00 UTC (GitHub Actions). It takes every `ip` from
  api.quake.world/v1/servers plus every server of qwleague.com's server API, aborts if fewer than 150
  IPs, looks them up in batches of 100 at ip-api.com (fields continent, country, countryCode, city, lat,
  lon) and writes a JSON object keyed by IP with region (the continent name), country, cc, city,
  coordinates [lat, lon]; missing fields become "unknown" or 0. League servers override region (from
  their continent code: af, as, eu, na, oce, sa), country code and name, and city. 187 IPs at the commit
  read [R].
- Use [R]: the hub looks a server's `ip` up in that table (empty location if absent); a LAN-party host
  name prefix gets a fixed Polish location; league hostnames "xx.qwleague.com" give the flag's country
  code. The Go library resolves hostnames to IPv4 first and refreshes its copy every 6 h; the hub every
  hour. Region drives the hub's region filter and sort.
- Ping regions: none; the site computes no ping from the viewer. The `ping` shown is each player's ping
  to the server as the server reports it [R].
- Proxies (qwfwd) may announce `city`, `coords`, `countrycode` in serverinfo [R].

---

## Watching live and demos in the browser

### 8. Live in the browser

- **Player** [R]: FTE QuakeWorld compiled to WebAssembly, built from the fork's "newtube" branch with
  emscripten 2.0.12, plus a CSQC add-on (`csaddon.dat`) for the hub's on-screen features; build folder
  named by date and short hash, older builds kept. Files: ftewebgl.js 216,451 bytes; ftewebgl.wasm
  6,405,447 bytes (2,072,584 over the wire with brotli); csaddon.dat 258,826 bytes; all "max-age one
  year, immutable", CORS any origin [M].
- **Connection** [R][M]: when a server is selected the page issues the engine command `qtvplay` with the
  target "tcp:{N@qtvhost:port}@wss://qtv.quake.world". The browser opens a websocket to
  wss://qtv.quake.world (path /) with subprotocol **faketcp**; inside it the bytes are the plain QTV TCP
  stream. The gateway forwards to the named QTV over TCP. Switching servers re-issues qtvplay (the engine
  swaps when the new stream is ready; no disconnect step); the engine reconnects by itself when a stream
  ends [R].
  Measured: a websocket upgrade with subprotocol faketcp gets 101 and echoes faketcp; another
  subprotocol gets 400; a plain GET gets 404; served through Cloudflare [M].
  The published gateway setup (fteqtv.quake.world) is nginx on 443 that redirects any non-websocket
  request, or any request whose subprotocol is not faketcp, with 302 to quake.world, and otherwise
  proxies to FTE's QTV proxy (built from FTE's source, listening on TCP/UDP 27599, run as an
  unprivileged user, restarted daily at 09:00 by cron because of an unexplained fault) [R].
  That older host name did not answer from here (502 at the egress proxy) [M].
- QTVGO itself also accepts websocket upgrades with subprotocol faketcp on its normal port (28000),
  binary messages, then treats the connection exactly like a TCP viewer [R].
- **QTV stream handshake** (what travels inside the websocket) [R]: text lines "Name: value", first
  "QTV", then VERSION (major must match, 1.x), one of SOURCE (stream id, or "file:" plus a demo name),
  RECEIVE (last stream), SOURCELIST, DEMOLIST (or legacy DEMO), optional PASSWORD or AUTH (with a
  challenge reply), USERINFO, COMPRESSION, ending with an empty line. Reply starts "QTVSV {version}",
  then e.g. "BEGIN: {server}" and the MVD stream, or "PERROR: ..." and close; SOURCELIST answers lines
  "ASOURCE: id: server: name"; DEMOLIST lines "ADEMO: size: name". The command buffer accepts at
  most one "@" chain hop by default (maxchains 1).
- **What is downloaded first** [R][M]: the engine script and wasm; a manifest
  a.quake.world/fte/default.fmf (103 bytes: game quake, base game id1, no home dir, default font); a
  hub config from the site (/assets/fte/config_v11.cfg, which ends by executing the user's saved
  settings file); csaddon.dat; a few crosshair and charset images from the site; 450 content files from
  a.quake.world/fte/... (sounds 142, status-bar and HUD pictures 120, textures 80, models 63, skins 18,
  item brush models 12, particle scripts 9, gfx.wad, a shader, frag message files) totalling 12,220,564
  bytes uncompressed, 90-day cache; and the current map as
  https://assets.quake.world/maps/{map}.bsp.gz, or maps/packaged/{map}.bsp.gz (lighting and locations
  appended) for a fixed list of about 50 maps (stock id maps and some community maps). Example sizes:
  dm3 packaged 640,504 bytes; povdmm4 55,735 plain, 76,482 packaged; one-year cache. Later maps come
  through the engine's download source cl_download_mapsrc = https://assets.quake.world/maps/ [R][M].
  Total first visit is therefore about 15 MB plus the map [D]. The loading screen shows "Loading assets
  (P%)" until 80% of the listed files are counted as loaded, then "Initializing.." [R]. The browser's
  own HTTP cache is what caches; user settings live in localStorage, including a generated config file
  the engine reads [R].
- **Delay** [R]: the source QTV adds `parse_delay` seconds before relaying a live game (QTVGO default 7 s,
  to prevent ghosting); the hub's engine config sets `qtv_buffertime` 1 (s) and offers it as a setting.
  The websocket gateway's own delay is open.
- Sound starts muted; volume cap 0.2 [R]. Pointer idle after 2 s hides controls; a wake lock keeps the
  screen on [R].

### 9. Demos in the browser

- **Format** [R]: the hub plays only MVD demos stored gzip-compressed. Files are addressed by the SHA-256
  of the demo: base `https://d.quake.world/{first 3 hex chars}/{sha256}` with suffixes `.mvd.gz` (the
  demo, also the Download link), `.mvd.info.json` (metadata, see 1 and 11) and `.mvd.ktxstats.json`
  (KTX stats; the player now prefers the stats embedded in the demo, read from the engine). The demo is
  mounted in the engine's file system as qw/match.mvd.gz and started with "playdemo match"; the map is
  fetched as in 8.
  QWD and NQ demos: the fork has QW and NQ timeline code, but the hub never sends anything but .mvd.gz
  (open whether others work).
- **Seeking** [R]: by demo time in seconds; the engine clamps to the length, ignores jumps shorter than
  1 s, and keeps speed and the followed player across a rewind. Buttons to start and to end, a time
  slider. Duration comes from the engine once known, else from the info file's `demo_duration`
  (fallback 610 s). The match clock subtracts a countdown (from the engine, else 10 s, or 15 s on league
  servers).
- **Speed** [R]: a percentage (the engine's cl_demospeed as a fraction): pause/resume restores the last
  positive speed; slow motion toggles between 20% and 100%.
- **Whom you follow** [R]: autotrack on/off/toggle; track a player by user id or by name (which turns
  autotrack off). For CTF demos the page clears enemy/team skins and gives each player a ctf skin by team.
- **Clips and sharing** [R]: `/games/?gameId=N` (legacy `demoId`), with either legacy `from`, `to`,
  `track` or up to 20 `clip=FROM-TO[:TRACK]` parameters (seconds; TRACK is a user id, a player name or
  auto; FROM must be below TO).
- **Embedding** [R]: /demo-player/?demo_sha256=... posts to the parent window: "seek" on every jump,
  "clip" on clip change, every 100 ms "current_time", and "set_speed" and "track" when they change.

### 10. The QTV proxy over HTTP (QTVGO; the hub's demo list scraper also handles the old C QTV's pages)

QTV and HTTP share one TCP port (default 28000); a connection starting with the bytes "QTV" is a viewer,
anything else HTTP (HTTP can be disabled) [R]. Routes [R]:
- `/` redirects (301) to `/nowplaying/`.
- `/nowplaying/`: HTML "QuakeTV: Now Playing on {hostname}": per stream a "Watch now!" link
  qw://{id}@{address}/qtvplay, the server address, upstream status, map long and short name,
  "(password protected)"; when players are present a level shot /levelshots/{map}.jpg (144 by 108),
  team and player frags, match status, observer count. Sorted by stream id.
- `/demolist/`: HTML table id "demos" with columns Stream (qw://file:{name}@{address}/qtvplay for .mvd),
  Download (/demos/{name}), Demoname, Size in kB, and "Total: N demos"; an upload form.
- `/demo_filenames`: plain text, one demo file name per line, newest first; with `?hash=xxh3` each line is
  "{xxh3 hex} {name}".
- `/dl/demos/{file}`: 301 to `/demos/{file}` (compatibility with the old QTV).
- `/demos/...`: static files from the demo directory (hidden files and .cfg/.key refused with 403).
- `/upload/` (POST multipart field "file"): only when uploads are enabled; one upload at a time, at most
  one per minute, name must end .mvd, size limit (default 32 MB, clamp 1 MB to 128 MB), first 100 KB
  must look like MVD with at least 500 ms of demo time; saved as "upload-*-{sanitised name}.mvd",
  oldest uploads deleted once a minute past a total (default 64 MB).
- Anything else: static files from the QTV directory (style.css, script.js, icons).
- Websocket upgrade on any path with subprotocol faketcp (see 8).
- The demo list is rebuilt every 60 s from the demo directory: extensions .mvd, .gz, .zip, .bz2,
  non-empty regular files, sorted by modification time newest first, XXH3 hashes reused while file
  metadata is unchanged.
- Timeouts: read 45 s, write 600 s, idle 60 s by default; HTTPS (and HTTP/2) when a certificate and key
  are set.
- The old C QTV, as captured in the scraper's test files, served /nowplaying/, /demos/ (the same table
  with id "demos" and cells class "name"), /dl/demos/{file}, /watch.qtv?demo={file}, /rss, /admin/ and
  `/demo_filenames.txt` [R].

---

## Demos

### 11. Where demos come from, indexing, search, retention

Two systems:

**A. The current hub's games index (Supabase + CDN).**
- Rows (view v1_games) [R]: id, timestamp, mode, map, matchtag, teams (JSON: name, name_color, color pair,
  frags, ping, players), players (JSON: name, name_color, team, team_color, color pair, frags, is_bot),
  team_names (text array), players_fts (full-text vector), server_hostname, demo_sha256,
  demo_source_url. Underlying table "demos" holds sha256, matchtime, mode, map, matchtag,
  server_hostname, players, teams, has_bots, has_ktxstats, source_url, uploaded_at, uploaded_by,
  event_id, game_id; games_v2, matches, stages, tournaments, series and events model competitions [R].
- Per-demo info file (`.mvd.info.json`) fields [R]: version, sha256, filename, timestamp, mode, map,
  matchtag, teams (name, name_raw, color, ping, frags), players (name, name_raw, team, team_raw, color,
  ping, frags, auth_username, auth_cc, is_bot), pov (id, number, name, name_raw, team, team_raw),
  demo_duration, countdown_duration, match_duration (seconds), server (hostname, address, ip, port, info
  as the serverinfo string), source_filename, source_url. The demo's displayed name drops a leading
  "yyyymmdd-hhii " from filename, so stored filenames start with the date and time [R].
- Search: see 2 (mode, map, team names, players full-text, matchtag, hostname, age; 15 per page).
- Where the demos come from and how long they are kept: open (the indexer is not public). The presence
  of source_url, source_filename and has_ktxstats suggests demos fetched from servers' or QTVs' lists
  and parsed for KTX stats [D, unconfirmed].

**B. qw-hub-api's demo list (`/v2/demos`)** [R][M]:
- Sources: a configured list of QTV addresses (15 in the sample config, e.g. de.quake.world:28000,
  quake.se:28000, troopers.fi:28000), each with a date format and a time zone.
- Fetch per QTV: first `http://{qtv}/demo_filenames.txt` (one name per line); if that fails, the HTML
  `http://{qtv}/demos/` page's demo table name cells. All QTVs queried in parallel. Cached 5 min; the
  list is rebuilt on the first request after expiry.
- Filename convention parsed: "{mode}_{participants}[{map}]{date}-{time}" plus an optional "_N" and the
  extension, e.g. 4on4_red_vs_blue[dm3]20261019-1608.mvd. Mode = text before the first underscore,
  submode midair or bf after duel, 2on2, 3on3, 4on4; participants = between mode (and submode) and the
  last "[", split once on "_vs_"; map = between the last "[" and the last "]"; date-time = after "]" up
  to the next "_" or ".". Date formats per source: "ymd" (yymmdd, default), "Ymd" (yyyymmdd), "dmy",
  time hhmm; read in the source's time zone, converted to UTC.
- Kept only if: it has a date-time; mode 4on4 always; otherwise not when a participant is a known bot
  name (bro, timber, goldenboy, tincan); mode must be duel, 1on1, 2on2, ctf or wipeout; map not
  containing dmm4 and not in a list of arena/midair/race-style maps; not older than 30 days.
- Output per demo: qtv_address, time (UTC, RFC 3339), filename, download_url
  (http://{qtv}/dl/demos/{filename}), qtvplay_url (file:{filename}@{qtv}); newest first.
- Query: `q` (space-separated words, each must be a substring of the file name, case-sensitive),
  `mode` (exact), `qtv_address` (substring), `limit` (default 100 per README; the code applies no limit
  when absent).
  Measured live: `/v2/demos?limit=1` returns exactly those five fields [M].

### 12. Parsed or taken from names?

- qw-hub-api: names only. No demo file is opened; everything comes from the file name and the QTV's
  configured time zone [R].
- Server "lastscores": from the server's own KTX stats JSON over the "laststats" UDP command (date with
  offset, map, hostname, ip, port, mode ("team" or other), tl, fl, dm, tp, duration, demo file name,
  teams, players with stats) [R][M]; the mode shown is the demo file name's prefix when present [R].
- Current hub: the per-demo info file has durations (demo, countdown, match), the recorder's point of
  view and per-player auth fields, which cannot come from a file name, and the player reads KTX stats
  out of the demo itself in the engine [R]; so the indexer parses demo files [D]. How: open.

---

## Other

### 13. Streamers and events

- **Twitch** [R]: qw-hub-api asks Twitch Helix every 60 s for up to 20 live streams of game id 7348
  (Quake), then the users of those streams for profile images (scaled from 300x300 to 70x70 in the URL).
  Output per stream: id, channel, url (https://twitch.tv/{login}), title, viewers, language,
  client_name (in-game name from a configured login map, else the Twitch display name),
  server_address (the first MVDSV server where that name is a player or spectator, including QTV
  viewers, case-insensitive wildcard match), started_at, duration_minutes, is_featured (login in the
  map), game_name, profile_image_url. Credentials: client id and an app access token from the
  environment (client-credentials flow). The current hub reads api.quake.world's Twitch passthrough
  instead (see 2) and keeps the featured-streamer map in the front end [R].
- **Events, news, forum, wiki** (qw-hub-api scrapers, routes disabled) [R]: events from the wiki page
  quakeworld.nu/wiki/Overview, tables with ids upcoming, ongoing, completed (title, status, date, wiki
  URL, 32 px logo), at most 10; "games in spotlight" from the same page; news from
  quakeworld.nu/feeds/news.php (RSS: title, date, URL); forum posts from the quakeworld.nu front page's
  recent-activity box (title, forum, author, date, URL); wiki recent changes from the MediaWiki
  recentchanges RSS (100 days, 20 items, bots hidden, de-duplicated by title). Cached 15 min.
- **Scheduled games and league**: scheduler.quake.world's cloud function and qwleague.com's SSE (see 2).

### 14. Auth, rate limits, CORS, caching, keys

- No user login anywhere on the hub [R].
- api.quake.world: public, no key, CORS any origin, Cloudflare in front, max-age 5 s to 60 s by route
  (see 3) [M]. Rate limits: none observed; open.
- qw-hub-api: permissive CORS, URL-keyed response cache (see 3); no auth, no rate limit [R]. Parameters
  validated: lastscores/laststats limit 0..50 (default 10), the address must be host:port; server group
  host at least 4 characters; mvdsv filter `empty` include/exclude/only (default exclude), `has_player`,
  `has_client`, `hostname` (at least 2 characters each), `limit` (default 100) [R].
- Supabase: the public anon key ships in the front end and is sent on every request; what it may read is
  governed by the database's row-level policies (open) [R].
- Front end HTML revalidates every load; assets immutable (see 3) [R]. CDN files: one year
  (assets.quake.world), 90 days (a.quake.world), 1 day (map shots) [M].
- Twitch credentials and the deploy service account are secrets in the environment/CI [R].
- QTVGO: optional viewer password (PLAIN or challenge AUTH), chat flood limits (4 messages per 2 s,
  then 2 s mute), at most 1000 viewers and 100 upstreams by default, upload limits (see 10) [R].

### 15. Registering a server

- No registration in any of the hub projects: servers are found only through the master servers'
  lists, and the hub lists those with clients [R]. A server operator gets listed by having the game
  server (and QTV, which registers itself on its configured masters) heartbeat to a master [R][D].
- What an operator controls is the serverinfo: `hostname` (whose first word, if it is a domain with an
  optional port, becomes the displayed address), `hostport` (the advertised address; api.quake.world
  then reports the server under it), `matchtag`, `mode`/`ktxmode`, and for QTV the stream advertisement
  and the QTV's external `address` setting [R][M].
- The demo list (system B) only reads QTVs written into its config file; system A's sources: open.
- The geo table can be corrected for league servers through qwleague.com's server data (see 7) [R].
