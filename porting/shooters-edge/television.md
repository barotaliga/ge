# The classic match broadcaster and its stream protocol

**A working note of clean-room facts** about the classic match broadcaster (the relay that pulls
multiview recordings from game servers over TCP and fans them out to many viewers with a
delay), its stream protocol, and the game server's side of it, kept for the engine's game-agnostic
broadcast relay ("television"), which must speak this protocol so existing servers and clients
interoperate. A reader session (2026-11-11) read the current rewrite of the broadcaster, the
modern server and, for three facts, the modern client; the facts below are in our words, with no
code. **[R]** read, **[D]** derived from read facts; nothing is measured yet, and every **verify**
is for a capture lab to settle. The classic protocol and the multiview recording format are
`protocol.md` and the game's demo reader; this note does not repeat them. In handshake text ↵ is
one line feed (0x0A).

## 1. Shape

- [R] One process, one address (default `:28000`). TCP on it carries the stream protocol (first
  bytes `QTV`, decided within 50 ms) or HTTP (anything else); UDP on the same port answers
  `ping`/`k` and `status` and sends master heartbeats. The current relay serves **no** UDP
  game-protocol viewer: viewers play the multiview records themselves.
- [R] An **upstream** is a source (a server, another relay, a local `.mvd` file); a
  **downstream** is a viewer. The relay forwards the upstream's records unchanged except for
  their time byte, and adds its own records for catch-up, chat, user lists and downloads.
- [R] WebSocket on any HTTP path, subprotocol `faketcp`, binary messages carrying the same byte
  stream as a TCP viewer (so a browser can watch).

## 2. Source addresses

- [R] `tcp:host:port` (the default stream), `tcp:<what>@host:port` (connect to the part after
  the **last** `@`, ask it for `<what>`: a stream id in digits, a `host2:port2`, a further chain,
  or `file:name.mvd`), `file:name.mvd` (local, in the demo directory, percent-decoded, `.mvd`
  only), `file:name.mvd@host:port` (a file on another relay).
- [D] What is asked of the upstream is the string with its last `@host:port` cut off; a leading
  `tcp:` is dropped from it unless it still holds an `@`; nothing left means "default stream".
- [R] The hop limit is the count of `@` in the string: setting `maxchains`, default 1, at most 5.

## 3. Handshake (both hops)

**Request**, text lines, ended by a blank line [R]:

| Line | Notes |
| --- | --- |
| `QTV` | Bare first line; a server closes anything else |
| `VERSION: 1` | Integer value must be 1 |
| `RECEIVE` or `SOURCE: <what>` | `RECEIVE` = the default stream; a server ignores `SOURCE` |
| `SOURCELIST` / `DEMOLIST` | Relay only: list requests in place of a source |
| `USERINFO: "<info>"` | Relay to server: `name` = `<relay hostname> (<local stream id>)`; `address` and `streamid` when the relay has an advertised address. Viewer: its game userinfo (`name` used) |
| `AUTH: <method>` | May repeat; the receiver keeps the strongest it knows |
| `PASSWORD: "<response>"` | Second request only (or at once for PLAIN) |
| `QTV_EZQUAKE_EXT: <mask>` | Viewer to relay: bit 1 downloads and the step-by-step connect, bit 2 setinfo, bit 4 user lists; a viewer client sends 7 |
| `RAW: <n>` | Server only: non-zero = bare records with no reply text |
| `DEMO: <name>` | Relay only: same as `SOURCE: file:<name>` |
| `COMPRESSION` | Relay drops the connection; a server ignores it |

**Answer** [R]: `QTVSV 1`, then (relay only) `QTV_EZQUAKE_EXT: <accepted mask>` when non-zero,
then one of: `BEGIN` (server) or `BEGIN: <source>` (relay) and a blank line, after which binary
records follow; `AUTH: <method>` + `CHALLENGE: <text>`; `PERROR: <text>` (permanent),
`TERROR: <text>` (temporary), `ERROR: <text>`; list lines `ASOURCE: <id>: <server>:
<hostname>` (fields right-aligned in 15) or `ADEMO: <bytes>: <name>`; all ended by a blank line.
The relay as a client treats all three error kinds as retryable.

**Server error texts** [R]: "You need to provide a common auth method.", "Bad password.", "You
need to provide a password.", "Incompatible version (valid version is v1)", "This map requires an
unsupported recording format." (TERROR), "Can't init stream, probably server reach a limit on the
number of proxies connected at any one time." (ERROR; also for a duplicate relay name,
case-insensitive), "This server enforces a limit on the number of proxies connected at any one
time. Please try again later" (ERROR, sent at accept when 3 over the limit). **Relay error
texts** [R]: "requested protocol version not supported", "authentication failure", "invalid
request", "multiple streams are currently playing", "no stream selected", "no sources currently
available", "no demos currently available".

**Auth** [R]:

- No password configured → accepted. Otherwise the first request carries only `AUTH` offers; the
  receiver answers with a challenge for the strongest offered method, and the sender repeats the
  **whole** request with `AUTH: <method>` and `PASSWORD`.
- Server strength order: NONE = PLAIN < CCITT < MD4 < SHA3_512. Relay as a server: PLAIN <
  SHA3_512 (it challenges only for SHA3_512). Relay as a client offers SHA3_512, SHA2_512, PLAIN,
  NONE.
- Challenge: the server writes the peer's `ip:port` text and pads with random printable bytes
  (33..126) to 63 bytes; the relay makes 63 random printable bytes.
- Responses over challenge-then-password: SHA3-512 as 128 lower-case hex; SHA-512 as standard
  base64 (another server family); MD4 as four 32-bit words in upper-case hex without padding;
  CCITT as the engines' 16-bit CRC in decimal; PLAIN the password; NONE an empty `PASSWORD:`.

**Timeouts** [R]: server pending handshake 5 s (`qtv_pendingtimeout`); relay viewer handshake
5 s; relay dial 10 s.

## 4. After BEGIN

- [R] Down: a plain concatenation of multiview records as in a file (time, type, mask for
  "multiple", length, payload). The relay refuses a payload over 8092 bytes (8192 less a 100-byte
  margin clients need) or a type outside 1..6.
- [R] Up: messages `[u16 LE total length incl. itself][u8 kind][body]`, length 3..buffer; kind 1
  = a NUL-terminated string command (at most 1023 bytes from the relay). A server drops a relay
  sending another kind.
- [R] The server catches up a new relay itself: it sends that destination alone a full initial
  game state beginning with `serverdata`. A relay sees `serverdata` as "map loading" and the
  stuffed `skins↵` as "ready", and only then feeds viewers. Each map is a new `serverdata`; the
  relay uses its own server count.
- [R] End: the server's last record is an "all" record with `disconnect` "EndOfDemo"; the relay
  forwards it and reconnects when TCP closes: back-off 2 s, doubling per failure, capped at 1 h,
  reset on reaching "ready" ([D] so an active stream that drops is retried after 4 s).

## 5. The delay (imposed by the relay)

- [R] `parse_delay` default 7 s, clamp 0..15; per stream `delay` option; local files 0.
- [R] Buffered by the records' time bytes: on a map start the release clock is set to now +
  delay; records are released while the clock is not ahead of now, each advancing it by its time.
- [R] Speed: buffered duration (sum of complete records' time bytes) ÷ target delay (target
  clamped 0.5..20 s; 0.5 s while serverinfo `status` is `Standby`), with 0.85..1.15 counted as 1
  and the result clamped 0.1..3.0; each released record's time byte is divided by it and written
  so to viewers. Delay 0 = forward at once at speed 1. Under-run with nothing released → the
  clock is pushed 2000 ms ahead.
- [R] Pause: the relay recognises a 13-byte hidden record (time 0, "multiple", mask 0, length
  3, type 0x000A, one byte of real milliseconds) and uses that byte as time. The current server
  writes a longer layout (length 7, then a body length 1, the type, the byte). [D] The relay
  therefore misses pause time from the current server; **verify** with a paused capture, and accept
  both layouts.
- [R] The viewer's client buffers a further 0.5 s by default and adjusts its own speed.

## 6. What goes up: chat and the user list

- [R] Server accepts from a relay only `say`, `say_team`, `say_game`, `qul`.
- [R] Viewer `say_game <text>` (or `say`/`say_team` whose text starts `say_game `) → up as
  `<cmd> "[say_game ]#<viewer id>:<name>: <text>"`, only with 4096 bytes free toward the server;
  "say_game failed" otherwise. Server prints `#0:qtv_<say|say_team>_game:#<id>:<relay name>:
  <text>`; while a game is in progress (the mod decides; skipped in `Countdown` or with
  `qtv_sayenabled` 1), and always for `say_team`, only spectators get it. The server also writes it
  into the stream as a chat print in an "all" record, so every relay's viewers see it.
- [R] Plain `say`/`say_team` stays in the relay: every viewer of that stream gets a chat print
  `#0:qtv_<cmd>:#<id>:<name>: <text>`.
- [R] User list: `qul <action> <id> "<name>"` (1 joined, 2 renamed, 3 left without a name) sent
  up on every viewer change and in full at each map start; the server keeps it per relay (over
  2048 drops the relay), prints `QTV: <relay>: <name> join` / `left` / `changed name to <new>`,
  and stuffs `//qul <action> <id> "<name>"` to game clients whose userinfo has `qul` 1 (the whole
  list as action 4 when they connect or set it). Viewers with mask bit 4 get the same stuffed
  text from the relay.

## 7. Viewers

- **Requests** [R]: `SOURCE` by id or by a full `tcp:`/`file:` string (opened on the viewer's
  behalf, named `(user)<string>`, shared with later viewers, closed when the last one leaves);
  `RECEIVE` (the single operator stream); `SOURCELIST`; `DEMOLIST`. The password is the stream's
  own or `qtv_password`.
- **Catch-up** [R], each in a "read" record with time 0: `serverdata` (extension words, 28, the
  relay's server count, game dir, time 0, map, ten movement values) with `fullserverinfo`
  stuffed; sound list then model list in ~725-byte chunks (short forms past index 255); pre-spawn
  in ~768-byte records (per slot userinfo, frags, ping, loss; baselines; light styles; static
  sounds; static entities); a `playerinfo` per player; a stats record per player (type 5, player
  in the high 5 bits) with the first also carrying a synthesised match start and game time; a full
  `packetentities` from baselines; stuffed `skins`; `setpause` when paused; an empty "all"
  record with time 5 (a client workaround). Mask bit 1 viewers instead get `serverdata`, aliases
  for `lastscores` and `follow`, then step through `qtvsoundlist <count> <start>`,
  `qtvmodellist <count> <start>`, `qtvspawn <count>`, restarted on every map; a wrong count is
  ignored, a step out of order restarts.
- **State kept per map** [R]: everything the catch-up writes, plus 64 frames of up to 300
  entities; limits 2048 entities, 4096 sounds, 512 static sounds, 64 light styles, 32 slots, 32
  stats. Across maps: the last 32 distinct `//finalscores` stuffed texts (date, type, map, two
  sides and scores) for `lastscores`. Server `hostname` changes are ignored; the relay rewrites
  it to `<own hostname> (live: <server hostname>)` or `(recorded from: …)`, replacing a parent
  relay's name when the upstream already had `*qtv`; it sets `*qtv` to `1`.
- **Commands** [R]: `qtvspawn`, `qtvsoundlist`, `qtvmodellist`, `download`, `stopdl`,
  `stopdownload`, `say`, `say_team`, `say_game`, `users`, `setinfo`, `ptrack [slot]`,
  `follow [name|id]` (the relay stuffs `track <userid>` to followers), `lastscores`, `qul`
  (ignored).
- **Names** [R]: high-bit spaces made plain, trimmed; empty, `console`, or holding `#`, `:`,
  `&c`, `&r` → `unnamed`; a duplicate gets `(` + 8 random characters + `)` and is cut to 25 after.
- **Flood** [R]: `fp_messages` 4 within `fp_persecond` 2 s silences for `fp_secondsdead` 2 s.
- **Downloads** [R]: 1350-byte pieces while the output queue is under two thirds full; per
  directory switches (skins, models in `progs/`, sounds, maps, demos, other); `.cfg`/`.key`,
  dot-leading and top-level names refused.
- **Limits** [R]: `maxclients` 1000 (cap 2048, extra connections closed silently); `maxservers`
  100 (cap 1024); viewer queues 32 KiB in, 192 KiB out (a 4095-sound map needs 512 KiB);
  upstream queue 4 MiB (files 320 KiB); a record that does not fit a viewer's queue disconnects
  that viewer, chat and user-list records are skipped below 16 KiB room; `dstream_timeout` 30 s,
  `ustream_timeout` 60 s; one wake-up per `tick_time` 100 ms (100..1000).

## 8. Server side settings and lists

- [R] `qtv_streamport` 0 (off; 0..65534; the same port also takes a `qizmo↵` TCP game tunnel),
  `qtv_maxstreams` 1 (≤ 0 unlimited; up to 3 extra connections are held only to see if they are
  the tunnel), `qtv_password` empty, `qtv_pendingtimeout` 5, `qtv_streamtimeout` 45 (no bytes
  accepted for that long, or the oldest queued bytes waiting twice that, drops the relay),
  `qtv_stream_max_queue_size` 16384 KiB (overflow drops; applied live), `qtv_sayenabled` 0.
  Commands `qtv_list`, `qtv_close <id|all>`, `qtv_status`; the player command `qtvusers`.
- [R] Connectionless `status <opt>` with bit 32: `qtv <id> "<relay name>" "<streamid>@<address>"
  <viewers>` per relay (`""` without an address); `qtvusers`: `qtvusers <id> "<name>"…` per relay.
- [R] A game client whose userinfo has `*qtv` gets no protocol extensions.

## 9. The relay's own discovery, web and files

- [R] Masters (default `master.quake.world master.quakeservers.net`, port 27000): `a↵<seq>↵<viewer
  count>↵` every 5 minutes, names re-resolved daily.
- [R] UDP `status`: `n` + serverinfo (`*version`, `hostname`, `maxclients`) + one line per viewer
  `<id> 0 <minutes> 9 "<name>" "" 0 0`.
- [R] HTTP: `/` → 301 `/nowplaying/` (streams, `qw://<id>@<address>/qtvplay` links, map, scores,
  match status, observer count; data 5–10 s old); `/demolist/` (upload form,
  `qw://file:<name>@<address>/qtvplay` links); `/demo_filenames` (plain names, or `<xxh3 16 hex> <name>` with
  `?hash=xxh3`); `/dl/demos/<f>` → 301 `/demos/<f>`; `/demos/<f>` files; `/upload/` POST field
  `file`; other paths static files. Hidden names and `.cfg`/`.key` → 403.
- [R] Files: only `.mvd` plays (`.gz`, `.zip`, `.bz2` list and download only), at 1× with no
  delay; [D] a finished file restarts after the back-off. No playlists, no recording by the
  relay; uploads `.mvd` only, one at a time and one a minute, the first 100 KiB must hold at least
  500 ms of whole records, stored as `upload-<random>-<cleaned name>.mvd`, oldest deleted over the
  total limit.

## 10. Versions

- [R] One stream protocol version, 1. Additions are negotiated by name instead: the viewer
  extension mask, auth method names, `RAW`, and the recording's own extension word (bit 5 hidden
  messages, bit 0 float entity coordinates). Hidden record type ids: 0x0000 anti-lag positions,
  0x0001 user commands, 0x0002 and 0x0008 weapon preferences, 0x0003 demo info, 0x0004–0x0006
  commentary, 0x0007 damage done, 0x0009 weapon instructions, 0x000A paused duration, 0xFFFF
  extended type follows. Their dates are not settled.

## 11. What the engine's relay should do better (derived)

- Size catch-up from the state, not a fixed queue; time out on progress, not on socket writes.
- Carry real elapsed time and the game clock in the stream rather than guessing from stats.
- Accept both paused-time layouts; let a viewer pick its delay above the operator's minimum.
- Challenge-based auth on every hop; optional compression; a UDP or browser path for viewers
  without a multiview player.
