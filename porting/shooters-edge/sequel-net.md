# The sequel's network protocol (protocol 34): the client's side of a live connection

This is a working note of clean-room facts about what a client of the sequel needs, beyond the
server's messages, to join a live dedicated server of protocol 34 as a player or a spectator, stay
connected, receive frames and send moves the server accepts. It was written by a reader who read id
Software's GPL source release of Quake II (the original release on GitHub, default branch; the 3.21
code, declaring protocol 34) and, where it settles what an unmodified modern server does, the
Yamagi Quake II source (8.71pre). Measurements were made against Yamagi's dedicated server
(`q2ded` 8.71pre, the baseq2 game library built with it) on the 3.14 demo pack, deathmatch 1, map
demo1 or demo2, over UDP on the loopback interface, with a client script of the reader's own. It
contains no code. Each fact is marked **[read]** (stated by the original release's source),
**[read, Yamagi]** (stated by Yamagi's source; "same in Yamagi" means Yamagi's source was read and
agrees), **[measured]** (observed against Yamagi's server) or **[derived]** (worked out from read or
measured facts). Where neither source settles something, it says so. The answers are numbered after
the questions.

It does not repeat `sequel-demo.md`: every server-to-client message's layout, the server packet's
8-byte header, the signon as a recording holds it and the encodings are there. The command's fields
and what the move does with them are in `sequel-movement.md` § The command.

Conventions: every multi-byte integer is little-endian; "byte" is unsigned 8 bits, "short" a signed
16-bit value, "long" a signed 32-bit value, "string" bytes up to and including a zero byte
**[read]**. Server frames are 100 ms apart (10 Hz) **[read]**; measured: 29 frames to one client
in 3 s **[measured]**.

---

## Out of band

### 1. The connectionless prefix and the out-of-band exchange

- **The prefix**: an out-of-band packet starts with the four bytes FF FF FF FF (a sequence of -1),
  then text **[read]**. Both sides tell an out-of-band packet from a connection packet by those four
  bytes alone **[read]**.
- **How the text is read**: the receiver reads one line, stopping at a newline, a zero byte or the
  end of the packet (at most 2047 characters), and splits it into words; double quotes group a word
  that contains spaces or backslashes **[read]**. A request may end in a newline or simply at the end
  of the packet; both work **[read]** **[measured]**: "ping", "info 34" and "status" without a
  newline were all answered.
- **The server's answers carry no terminator** beyond what the text itself holds: "challenge 6251"
  ends at the end of the packet, with no newline or zero byte **[read]** **[measured]**.

Requests a server answers, and its answers **[read]** (the exact text after the prefix):

| request | answer | notes |
| --- | --- | --- |
| `getchallenge` (the stock client sends it with a newline) | `challenge <n>` | n in decimal, 0 to 32767. Yamagi appends ` p=34`: measured `challenge 6251 p=34` **[read, Yamagi]** **[measured]** |
| `connect <protocol> <qport> <challenge> "<userinfo>"` + newline | `client_connect` | Q2. Yamagi appends ` dlserver=<url>` only when its download-server setting is set **[read, Yamagi]** |
| `info <protocol>` | `info` + newline + one line | see below. The original ignores it on a server with `maxclients` 1 |
| `status` | `print` + newline + the status text | see below |
| `ping` | `ack` | |
| `ack` | nothing | the server notes it on its console |
| `rcon <password> <command words>` | `print` + newline + the command's console output | refused: the text `Bad rcon_password.` |
| anything else | nothing | the server notes "bad connectionless packet" on its console |

- **Refusals and messages** come as `print` + newline + text; the client shows the text **[read]**.
  Every refusal during a connect is such a print; no other reply kind is used (Q2).
- **info's line**: the host name right-aligned in 16 characters, a space, the map name
  right-aligned in 8, a space, the client count in 2, a slash, `maxclients` in 2, a newline **[read]**.
  Measured: `info` newline `          noname    demo1  0/ 8` newline **[measured]**. With a protocol
  number other than 34: `<hostname>: wrong version` and a newline **[read]** **[measured]**. The count
  includes clients still in the signon **[read]**.
- **status's text**: the server info string (`\key\value` pairs of every server-info setting), a
  newline, then one line per client in the signon or the game: frags, a space, ping in
  milliseconds, a space, the name in double quotes, a newline. It stops before overflowing 1384
  bytes **[read]**. Measured (no players): `\cheats\0\coop\0\deathmatch\1\dmflags\16\fraglimit\0\...`
  with `maxspectators`, `needpass` and `protocol\34` among the keys **[measured]**.
- **The client's own answers** (a server may send these): `ping` gets `ack`; `echo <word>` gets the
  word back; `print` + newline + text is shown; `info` is a reply to the client's server scan, and
  `cmd <text>` is obeyed only from the client's own machine **[read]**. The server in this release
  sends none of `echo` or `cmd` **[read]**.
- **The stock client's connect sequence** **[read]**: it sends `getchallenge` (with a newline) at
  once, then again every 3 s while no `client_connect` has come, printing "Connecting to
  <server>..." each time. Each `challenge` answer makes it send `connect` at once. A second
  `client_connect` while connected is ignored ("Dup connect received. Ignored."). The default server
  port is 27910 **[read]**.

### 2. The connect line and what the server checks

- **Arguments, in order** **[read]**: `connect`, the protocol number (34), the qport, the
  challenge, the user info. The three numbers are decimal text, unquoted; the user info is in double
  quotes. The stock client writes `connect 34 <qport> <challenge> "<userinfo>"` and a newline.
- **Parsing** **[read]**: each number is read as a decimal integer (text that is not a number gives
  0). The user info is the fourth word, at most 511 characters. In the original release a quoted
  word keeps only its first 128 characters, so a longer user info is cut at 128 characters at
  connect **[derived]** from the tokenizer's 128-character word limit; Yamagi's limit is 1024
  **[read, Yamagi]**. The full user info can be sent later in a user info message (Q12), which is not
  cut **[derived]**.
- **The qport** is any value 0 to 65535 **[derived]**: the packet header carries it as 16 bits
  and the server compares the header's value, taken as 0 to 65535, with the connect line's number,
  so a number outside that range never matches and the connection's packets are all ignored. The
  stock client's qport is the setting `qport`, chosen from the millisecond clock at start-up (0 to
  65535) and not settable by the player **[read]**.
- **The server's order of checks, with the exact text after the prefix** **[read]** (the same texts
  in Yamagi unless noted, **[measured]** where shown):
  1. Wrong protocol: `print\nServer is version 3.19.\n` in the original (its version constant reads
     3.19, formatted with two decimals). Yamagi: `print\nServer is protocol version 34.\n`
     **[read, Yamagi]** **[measured]**.
  2. A server playing back a recording or cinematic refuses remote clients:
     `print\nConnection refused.\n`.
  3. The challenge (not checked for the server's own machine's internal loopback): an entry for the
     sender's IP address with another number gives `print\nBad challenge.\n` **[measured]**; no entry
     for that address gives `print\nNo challenge for address.\n` **[measured]** (from a second
     loopback address that had never asked).
  4. An existing slot for this address (Q3), else a free slot. None free: `print\nServer is full.\n`
     **[measured]** with `maxclients` 2 and two clients in.
  5. The game's own checks (the stock baseq2 game, the same in Yamagi's): a banned IP address gives
     `print\nBanned.\nConnection refused.\n`. A spectator (Q17) with a wrong spectator password:
     `print\nSpectator password required or incorrect.\nConnection refused.\n` **[measured]**. Too many
     spectators: `print\nServer spectator limit is full.\nConnection refused.\n`. A player whose
     `password` key does not match the server's `password` setting:
     `print\nPassword required or incorrect.\nConnection refused.\n` **[measured]** (also with no
     password key at all). A refusal by a game that gives no reason: `print\nConnection refused.\n`.
  6. Accepted: `client_connect` **[measured]**.
- The server's `password` and `spectator_password` settings are off when empty or `none`; the ban
  list is the game's (`addip`, `removeip`, `listip`, `writeip` server commands; `filterban` 1, the
  default, makes the list a ban list, 0 an allow list) **[read]**.
- The game also reads the user info at connect (Q4) and broadcasts nothing until the player enters
  (Q9) **[read]**.
- **Right after `client_connect`** the server opens the connection and, at its next frame, sends a
  connection packet with an empty body because nothing has been sent for over a second **[read]**.
  Measured: the 8 bytes 01 00 00 00 00 00 00 00 (sequence 1, acknowledging 0) about 0.1 s after the
  accept **[measured]**.

### 3. Challenges and repeated connects

- **The table** **[read]**: 1024 entries, each an address, a number (a random 0 to 32767) and the
  time it was made. Entries are matched on the IP address alone, not the port. A `getchallenge` from
  an address that has an entry gets that entry's number again; otherwise the oldest entry is
  overwritten. Same in Yamagi **[read, Yamagi]**. Measured: two requests, and a request from another
  port of the same address, all got the same number **[measured]**.
- **Lifetime** **[read]**: nothing expires an entry and a connect does not use it up. It lasts until
  1024 newer addresses have pushed it out, and it can be used for any number of connects. Every
  machine behind one IP address shares one number **[derived]**.
- **Repeated connects from one address** **[read]**: a connect whose IP address matches a slot that
  is not free (connecting, in the game, or just disconnected) and whose qport or port matches that
  slot's is a reconnect. In the original, a reconnect within `sv_reconnect_limit` seconds (default
  3) of that slot's last accepted connect is ignored with no reply; after that, the slot is taken
  over and the connect is handled as new. In Yamagi, a remote reconnect to a slot that is connecting
  or in the game is always ignored with no reply, and slots that are free or just disconnected are
  not matched, so a client that has just disconnected gets a fresh slot **[read, Yamagi]**.
  Measured: a second connect from the same address and qport while in the game got no reply
  **[measured]**.
- There is no other throttle on out-of-band requests in the original **[read]**.

### 4. The user info

- **Form** **[read]**: `\key\value\key\value...`, at most 511 characters. Keys and values must be
  under 64 characters each; a value holding a backslash, a double quote or a semicolon cannot be set
  by the stock client. Empty values are left out of the string. The server adds `ip` (the sender's
  address and port, text) at connect so the game can filter on it; when the string is too full to
  take it, it is not added.
- **Validation by the game** **[read]**: a user info holding a double quote or a semicolon is
  replaced whole by `\name\badinfo\skin\male/grunt`.
- **Keys and what reads them** **[read]**:

| key | stock client default | who reads it | effect, limits |
| --- | --- | --- | --- |
| `name` | `unnamed` | engine and game | the engine keeps 31 characters with the top bit of each cleared (console, status); the game keeps 15 (prints, scoreboard) |
| `skin` | `male/grunt` | game | model/skin; the game sets configstring 1312 + slot to name, a backslash, skin |
| `rate` | 25000 (original), 8000 (Yamagi **[read, Yamagi]**) | engine | Q19; clamped to 100 to 15000; absent or empty means 5000 |
| `msg` | 1 | engine | prints below this level are not sent to the client (0 low, 1 medium, 2 high, 3 chat); absent means 0 |
| `hand` | 0 | game | gun hand: 0 right, 1 left, 2 centre |
| `fov` | 90 | game | 1 to 160; below 1 or not a number gives 90; with the deathmatch fixed-fov flag always 90 |
| `gender` | `male` | game | the wording of death messages |
| `spectator` | 0 | game | Q17 |
| `password` | empty (so absent) | game | Q2 |
| `ip` | (added by the server) | game | the ban filter |
| `rejmsg` | — | engine | set by a game refusing a connect; becomes the refusal text |

- The stock client resends its whole user info in a user info message (Q12) whenever one of these
  settings changes **[read]**.

## The connection's packet header

### 5. The client-to-server header

Ten bytes **[read]**:

| bytes | field |
| --- | --- |
| 0–3 | the packet's sequence number in the low 31 bits; the top bit set when the packet carries a reliable payload |
| 4–7 | the highest server sequence received, in the low 31 bits; the top bit is the client's reliable even/odd acknowledgement (the even/odd bit of the last reliable payload received from the server) |
| 8–9 | the qport (16 bits) |

- The server-to-client header is the same less the qport (8 bytes) **[read]**.
- **Sequences** start at 1 for each side's first connection packet; the receiver's acknowledged
  sequence starts at 0, and both reliable bits start at 0 **[read]** **[measured]**.
- **Matching** **[read]**: the server takes a connection packet as a client's when its IP address
  (ignoring the port) and its qport match that client's. When the port differs, the server adopts the
  new port (with a console note), so a client keeps its connection
  across a router changing its port. Packets that match no client are ignored without a reply. The
  client accepts only packets from the exact server address and port it connected to, and ignores a
  server packet under 8 bytes.
- **Sizes** **[read]**: a packet is at most 1400 bytes with its header; the reliable payload at most
  1384.

### 6. The reliable channel

**[read]** unless marked:

- **Sender**: reliable data is queued (up to 1384 bytes) while a previous reliable payload is in
  flight. When nothing is in flight and data is queued, the queue becomes the in-flight payload, the
  sender's reliable bit flips, and the payload goes at the front of the next packet, with that
  packet's reliable flag set. The unreliable part follows when it fits; when it does not, it is
  dropped (with a console note) and the packet still goes.
- **Acknowledgement**: when a received packet's reliable-ack bit equals the sender's current
  reliable bit, the in-flight payload is done and the next can go.
- **Resending**: the payload is sent again, whole, only when the other side has acknowledged a
  sequence more than one past the packet that last carried it, and its reliable-ack bit still does
  not match. It is never put in two packets in a row; a lost payload costs at least a round trip
  and one more packet **[derived]**.
- **Receiver**: every packet accepted in order (Q7) whose reliable flag is set flips the receiver's
  reliable bit, which it reports in the top bit of its acknowledgement word. The receiver does not
  check whether the payload is new: the sender's resend rule guarantees that a resent payload was
  not received **[derived]**. A client must follow the same resend rule, or the server will run its
  string commands twice **[derived]**.
- The reliable payload and the unreliable part are read as one message; nothing marks the boundary.
- **Overflow**: when more than 1384 bytes are queued while a payload is in flight, the server drops
  that client at its next frame, broadcasting "<name> overflowed" **[read]**. The stock client
  stops sending, printing "Outgoing message overflow" **[read]**.
- A server sends packets to a client still in the signon only when it has reliable data queued or
  in need of resending, or when a second has passed since its last packet (an empty keep-alive)
  **[read]**.

### 7. Out of order, duplicates and dropped packets

**[read]**:

- A packet whose sequence is not greater than the highest received is dropped whole: stale and
  duplicate packets alike, with nothing acknowledged and no reliable bit flipped. The connection is
  not harmed. With the setting `showdrop` 1, an "Out of order packet" line is printed.
- The number of packets dropped is the jump in sequence minus one, worked out afresh for each packet
  accepted; nothing accumulates it. The server uses it to run commands that would otherwise be lost
  (Q13). A client may skip sequence numbers freely; the server only counts them as dropped
  **[derived]**.
- In a frame, the server writes the count of frames it held back for rate (Q19) into the frame's
  suppress byte; that is the only drop count it reports.

### 8. Timeouts

- **The server's timeout** **[read]**: the setting `timeout`, default 125 s, counted in real
  time since the server last accepted a connection packet from that client (in order, whatever its
  content, even a header with no body). The same in the signon and in the game. A client past it is
  dropped and the others get "<name> timed out". Same in Yamagi.
- **Measured** with `timeout` 3: header-only packets once a second kept a client in the game for 8
  s; after it went silent the server logged "<name> timed out" **[measured]**.
- **A no-op**: a packet with only the no-op message, or with no body at all, keeps the client
  alive **[read]** **[measured]**.
- **After a drop** the slot is a zombie for the setting `zombietime` (default 2 s) after the last
  packet received from the client, during which the server keeps sending its reliable payload (the
  disconnect) but ignores the client's packets; then the slot is free **[read]**.
- **The client's timeout** **[read]**: the setting `cl_timeout`, default 120 s, past which on more
  than five client frames in a row the stock client prints "Server connection timed out." and
  disconnects.

## The signon from the client's side

### 9. The string commands of the signon

The commands a live client sends are string commands (Q12) on the reliable channel; their text has no
newline **[read]**. The server's prompts are stufftext messages ending in a newline; the stock client
runs them as console commands, where `cmd <rest>` sends `<rest>` to the server as a string command
**[read]**. The numbers below are the server's spawn count, a number that changes with every level
**[read]**: random at server start in the original **[read]**; measured 1232907478 on Yamagi, and one
more after a level change **[measured]**.

| step | client sends | server answers (reliably) |
| --- | --- | --- |
| 1 | (on `client_connect`) `new` | serverdata, then stufftext `cmd configstrings <count> 0` + newline |
| 2 | `configstrings <count> <i>` | configstrings from index i upward, then stufftext `cmd configstrings <count> <next>` + newline, or after the last, `cmd baselines <count> 0` + newline |
| 3 | `baselines <count> <i>` | spawnbaselines from entity i upward, then `cmd baselines <count> <next>` + newline, or after the last, `precache <count>` + newline |
| 4 | `begin <count>` | the player enters the game: frames start |

- **How much per answer** **[read]**: the server adds messages to the reliable queue while it holds
  fewer than 700 bytes, so one answer is roughly 700 to 1400 bytes; only non-empty configstrings are
  sent, and only baselines with a model, a sound or effects (entities 0 to 1023).
- **Measured** on demo1 **[measured]**: configstrings asked from 0, 9, 94, 318, 379, then baselines
  from 0, 165, 295 (138 baselines), then `precache`. On demo2 after a level change: configstrings
  from 0, 8, 109, 294, 358, 1062, then baselines from 0, 98, 171. The whole signon took 10 packets
  each way with the reader's client.
- **What the stock client does between `precache` and `begin`** **[read]**: it loads the map and
  checks its checksum (Q10), asks for any files it lacks when downloads are on (Q11), loads models,
  sounds and pictures, then sends `begin <count>` with the count from the `precache` line.
- **Entering the game** **[read]**: on `begin` with the right count, the client is in the game; the
  game's begin runs (in deathmatch, "<name> entered the game" to everyone, measured with print level
  2 **[measured]**), and from the next server frame the client gets one packet per frame. A client is
  sent frames only once in the game. Moves before then are read and ignored (Q13).
- **A wrong count** in `configstrings`, `baselines` or `begin` (a level changed during the signon):
  the server prints "from different level" on its console and starts the signon over by answering
  as for `new` **[read]**.
- **Out of turn** **[read]**: `new`, `configstrings` and `baselines` from a client already in the
  game are ignored ("not valid -- already spawned" on the server console). `begin` is not checked:
  sent again in the game with the right count, it runs the game's begin again **[read]**.
- **Never sending `begin`** **[read]**: the client stays in its slot in the signon state, counted in
  `info` and `status`, sent a keep-alive packet a second, never sent frames; its moves are ignored. It
  stays as long as it sends packets within the timeout.
- **The stock client while in the signon** sends at most one packet each 100 ms, and only when it
  has reliable data queued or a second has passed **[read]**.
- **Other string commands the engine handles** **[read]**: `disconnect` (Q18), `info` (the server
  info, printed to the client), `download` and `nextdl` (Q11), `nextserver <count>` (after a
  cinematic). Any other command, in the game, goes to the game (`say`, `say_team`, `score`, `help`,
  `inven`, `use`, `drop`, `kill`, `wave`, `players`...). The server runs at most 7 string commands
  from one packet; the rest are read and ignored **[read]** **[measured]**: of 10 in one packet, 7
  ran. The game also limits chat by its flood settings (`flood_msgs` 4 in `flood_persecond` 4 s,
  then a `flood_waitdelay` of 10 s) **[read]** **[measured]**: of 10 `say` lines, 4 were heard.

### 10. Checks of the client's files

- **The map checksum** **[read]**: configstring 31 holds, as signed decimal text, the server's
  checksum of the map file: the MD4 digest of the whole .bsp file, its four 32-bit little-endian
  words XORed together. Measured: demo1.bsp (1 991 772 bytes in pak0) gives -1076406445, the value the
  server sent in configstring 31 **[measured]**.
- **Only the client checks it** **[read]**: the stock client computes the same over its own copy and,
  when they differ, drops itself with "Local map version differs from server" and the two numbers.
  Nothing is sent to the server, and the server never checks a client's files, models or skins
  **[read]**.
- The challenge saved at connect is not used in any check in this release **[read]**.

### 11. Downloads

- **The server sends a file only when asked** **[read]**: the client sends `download <path>` or
  `download <path> <offset>` to resume. The server answers with a download message (sequel-demo.md
  Q7) carrying up to 1024 bytes, and each `nextdl` from the client gets the next piece; the piece
  with percent 100 is the last.
- **Refused** **[read]**: size -1 and percent 0, when `allow_download` is 0 (the original's
  default; Yamagi's default is 1 **[read, Yamagi]**), when the path holds `..`, starts with `.` or
  `/`, or has no `/`, when the file is missing, or when a `maps/` file comes from a pack. Further
  switches: `allow_download_players` (default 0), `allow_download_models`, `allow_download_sounds`,
  `allow_download_maps` (default 1). In the original, the prefix tests compare only the first six
  characters, so `allow_download_players` 0 refuses any path starting `player` and
  `allow_download_maps` 0 refuses only the bare path `maps/`, never a real map **[derived]**.
- **Declining** **[read]**: a client that does not want files simply never sends `download`; it
  goes on to `begin`. The stock client asks only when its `allow_download` is on, and its downloads
  are only for files it lacks.

## Client-to-server messages

### 12. Message ids

| id | message | payload |
| --- | --- | --- |
| 0 | bad | none valid: the server drops the client |
| 1 | no-op | nothing |
| 2 | move | Q13 |
| 3 | user info | string: the whole user info |
| 4 | string command | string |

**[read]** Several messages may share a packet; the server reads them in order to the end of the
packet. Any id other than 1 to 4 drops the client (with a console note on the server), and
reading past the end of the packet inside a message drops it too. A user info message
replaces the user info and runs the same checks as at connect (name, rate, msg; the game's skin,
fov, hand, spectator), but adds no `ip` key **[read]**.

### 13. The move message

Fields in order **[read]**:

| field | size | |
| --- | --- | --- |
| id 2 | 1 | |
| checksum | 1 | Q14 |
| acknowledged frame | 4 (long) | Q15 |
| command 1 (oldest) | 2 to 17 | delta from an all-zero command |
| command 2 | 2 to 17 | delta from command 1 |
| command 3 (newest) | 2 to 17 | delta from command 2 |

Each command delta **[read]**: a byte of flag bits, then the fields whose bits are set, then two bytes
always present:

| bit | value | field | size |
| --- | --- | --- | --- |
| 0 | 0x01 | pitch | short |
| 1 | 0x02 | yaw | short |
| 2 | 0x04 | roll | short |
| 3 | 0x08 | forward move | short |
| 4 | 0x10 | side move | short |
| 5 | 0x20 | up move | short |
| 6 | 0x40 | buttons | byte |
| 7 | 0x80 | impulse | byte |
| — | — | msec | byte, always |
| — | — | light level | byte, always |

- **The values** **[read]**: angles are 65536ths of a turn (the view angle in degrees times 65536
  over 360, truncated, low 16 bits), moves in units per second, buttons 1 attack, 2 use, 128 any key,
  msec 0 to 255; the light level of where the player stands, 0 to 255 (`sequel-movement.md` § The
  command). A bit is set when the field differs from the base; a field whose bit is clear keeps the
  base's value.
- **There is no separate light level byte**: each command carries its own **[read]**.
- **Which commands** **[read]**: the stock client makes one command per packet and keeps them in a
  ring by sequence. Command 3 is the one made for this packet, 2 the one for the packet before, 1
  for the packet before that. Size: 12 to 57 bytes for the whole message **[derived]**.
- **What the server runs** **[read]**: command 3 always. When packets were dropped before this one
  (Q7) and fewer than 20: with one drop also command 2, with two also command 1, with more, first
  the last command it ran, once per drop beyond two, then 1, then 2. Oldest first, then command 3. The
  newest command is kept as "the last command". With 20 or more drops only command 3 runs. Nothing
  runs while the server is paused.
- **One move per packet** **[read]**: a second move message in the same packet makes the server
  ignore it and everything after it in the packet **[measured]**: a string command placed after a
  second move did not run.
- **Before the client is in the game** the move is read, the acknowledged frame is reset to -1 and
  nothing runs **[read]**.
- **When moves run**: the server runs each move as soon as it reads the packet, between frames,
  not at the 10 Hz frame **[read]**.

### 14. The move's checksum byte

- **What it covers** **[read]**: the bytes after the checksum byte up to the end of command 3: the
  acknowledged frame and the three deltas, not the id, not the checksum itself, nor any message
  after the move. At most the first 60 of those bytes are used; the longest move has 55, so all are
  used **[derived]**.
- **How it is computed** **[read]**: append to those bytes four bytes taken from a constant table at
  an offset given by the packet's own sequence number (the low 31 bits of the header's first word,
  which the stock client sets before sending); compute over the result the 16-bit CRC with polynomial
  0x1021, initial value 0xFFFF, no reflection and no final XOR (the CCITT CRC of the XMODEM family,
  usually called CRC-16/CCITT-FALSE); add every byte of the same result (with the four appended) as
  an unsigned integer sum; XOR the CRC with the sum and keep the low 8 bits.
- **The table** **[read]**: it is a table of constants the source defines (it is not derived from
  anything in the format). It is declared as 1024 bytes, of which the first 960 are given values and
  the last 64 are zero. The offset is the sequence modulo 1020, and four consecutive bytes from that
  offset are taken, so offsets 0 to 1019 read bytes 0 to 1022, and from offset 957 upward some or all
  of the four are the zero tail. Its contents are not given here. Yamagi uses the same rule **[read,
  Yamagi]**, and the reader's own computation matched the server's (every move accepted)
  **[measured]**.
- **Could it be measured?** **[derived]** Yes, without reading it. (a) From a stock client's
  traffic: each of its moves gives one 8-bit equation in the four table bytes at offset sequence mod
  1020; because neighbouring offsets share three bytes, after the first offset each new offset adds
  one unknown byte with 256 candidates, so a few packets at each offset (a stock client at tens of
  packets a second cycles all 1020 offsets every 10 to 30 s) pin the whole table. (b) By probing a
  server: a client may choose any higher sequence number (Q7), so it can aim at any offset, and a
  move with the right checksum visibly moves the player in the next frame while a wrong one does
  nothing; at most 256 tries per message and offset.
- **When it is wrong** **[read]**: checked only when the client is in the game. The server ignores
  the move and the rest of the packet silently: no reply, no count, no drop; only a developer-level
  line on the server console naming the client, the computed and the sent values and the sequence.
  Messages before the move in the same packet have already run, and the acknowledged frame was
  taken before the check, so it still counts. **[measured]**: 40 moves with a wrong checksum left the
  player where it stood, 40 with the right one moved it, and the server logged a failure line for
  each wrong one with developer 1 on.

### 15. The acknowledged frame

**[read]** unless marked:

- It is the number of the last frame the client received and parsed. The server deltas the next
  frame from it.
- The server keeps the last 16 frames per client. It deltas from the acknowledged frame when that is
  greater than 0 and at most 12 frames old (current frame minus acknowledged at most 12); otherwise it
  sends a full frame, whose header gives delta frame -1. **[measured]**: acknowledging frames 0, 1 and
  5 behind gave deltas; 11 to 20 behind (13 or more by the time the next frame was built) gave -1.
- -1 or 0 asks for a full frame; the stock client sends -1 when its setting `cl_nodelta` is 1,
  when it has no valid frame, and while waiting to start recording **[read]**. Measured: 0 gave a full
  frame **[measured]**.
- The server does not check that the number is one it sent: a number from the future within the
  window would be deltaed from whatever that slot of the ring holds **[derived]**.
- A connection starts with 0 (full frames) and a level change sets -1 **[read]**, so a client that
  never sends a move is sent a full frame every time **[derived]**.
- **Ping**: when the acknowledged frame changes and is above 0, the time between that frame's
  sending and now is one of 16 samples; the client's ping (the `status` reply, the scoreboard) is
  the mean of the samples above 0, in milliseconds.

### 16. How often a client sends; msec and the time check

- **The stock client** **[read]**: in the game, one packet per client frame, each with one move, at
  most `cl_maxfps` (default 90) a second. The command's msec is that frame's length; a frame over 250
  ms gives 100. In the signon, see Q9.
- **The server** **[read]**: it runs every move it accepts (Q13), with that command's msec, however
  many come in a frame; there is no merging and no splitting. Fewer moves simply mean the player
  moves less; frames go on at 10 Hz whether or not moves come.
- **The time check** **[read]**: each client has a budget, set (not added) to 1800 ms every 16
  server frames (1.6 s); each command run subtracts its msec. With the setting `sv_enforcetime`
  nonzero (default 0, off), a command that brings the budget below 0 is dropped, with only a
  developer-level line naming the client on the server console. Nothing tells the client.
  Same in Yamagi **[read, Yamagi]**. **[measured]**: 80 commands of 100 ms in 2 s with
  `sv_enforcetime` 1: 46 dropped. With it 0: none dropped.

## Spectators and disconnects

### 17. Spectators

**[read]** (the stock baseq2 game; Yamagi's game agrees):

- **Deathmatch only.** A user info `spectator` that is present and not `0` asks for a spectator.
  When the server's `spectator_password` is set (not empty or `none`), the `spectator` value must
  equal it: the key is the password. A spectator skips the player `password` check. The server
  setting `maxspectators` (default 4, in the server info) caps them.
- **What it gets**: frames like any client in the game. Its player state has movement type 1
  (spectator) and no gun; it flies through walls; its entity is not sent to others. **[measured]**:
  a spectator's playerinfo showed movement type 1; "<name> entered the game" was still broadcast.
- **Moves**: not needed to stay connected (any packet will do, Q8), but they are what moves the
  spectator. Attack toggles following a player (chase camera); jump (up move 10 or more) picks a
  target or the next one; the inventory next/previous commands (`invnext`, `invprev`) cycle targets.
- **Switching in the game**: change `spectator` in the user info. At a frame at least 5 s after the
  client's last spawn, the game checks the passwords and the cap. On failure it prints
  "Spectator password incorrect." or "Server spectator limit is full." (with no newline), or, going
  back to playing with a wrong `password`, "Password incorrect.", and stuffs `spectator 0` or
  `spectator 1` with a newline to reset the client's setting. On success it broadcasts "<name> has
  moved to the sidelines" or "<name> joined the game".

### 18. Disconnects and level changes

- **The stock client's disconnect** **[read]**: a string command `disconnect`, sent unreliably in
  three packets in a row, each with its own sequence; its string has no terminating zero (the server
  reads to the end of the packet). The first one received drops the client; the server sends the
  disconnect message (id 7) reliably and the game broadcasts "<name> disconnected" **[measured]**.
  Later packets from the client are ignored (zombie, Q8).
- **The server drops a client** **[read]**: it queues the disconnect message (id 7) reliably, after
  any print it sends with it. Kicked: "<name> was kicked" to the others and "You were kicked from the
  game" to the client, then the disconnect. Timed out (Q8) and overflowed (Q6) likewise. The stock
  client, on id 7, drops with "Server disconnected".
- **A level change** (the game's level end, or the `gamemap` command) **[read]** **[measured]**: the
  server stuffs `changing` + newline to every client reliably and sends it before loading; after
  loading it stuffs `reconnect` + newline reliably. On `changing` the stock client shows the loading
  plaque; on `reconnect`, still connected, it sends `new` on the same connection, with no new
  challenge or connect. The server has put every client in the game back into the signon state with
  acknowledged frame -1 and a new spawn count, so the signon of Q9 runs again. Measured: `changing`
  and `reconnect` arrived, `new` brought serverdata with the spawn count one higher, and the signon
  ended in frames on demo2 **[measured]**.
- **A restart** (the `map` command on a running server, which starts the game afresh) **[read]**
  **[measured]**: the server sends every client a print "Server restarted" and the reconnect message
  (id 8), unreliably, twice. The stock client, on id 8, prints "Server disconnected, reconnecting"
  and starts a fresh connect (getchallenge, connect). Measured: two packets each with print level 2
  "Server restarted" and id 8 **[measured]**.
- **The server shutting down** **[read]**: the same two unreliable packets with "Server quit",
  "Server was killed." or a crash text, and the disconnect message (id 7).
- **What a client must send after a level change**: `new` in answer to `reconnect`, then the
  signon as in Q9, ending with `begin` and the new spawn count **[read]** **[measured]**.

## Rate

### 19. Rate

- **The key** **[read]**: user info `rate`, in bytes per second; clamped to 100 to 15000; absent or
  empty means 5000. A stock client's default is above the cap (Q4), so it gets 15000 **[derived]**.
- **The test** **[read]**: the server records the size of the frame message it sent each frame
  (the frame and the frame's unreliable messages; not the header or the reliable payload) in a ring
  of 10 slots, one per frame (one second at 10 Hz). Before sending a frame it sums the 10 slots, and
  when the sum exceeds the rate it sends nothing that frame: no frame is built, no packet goes (so
  no reliable data either), the slot for that frame is set to 0, and a suppress counter goes up. The
  next frame sent carries the count in its suppress byte and resets it. Clients on the server's own
  internal loopback are never held back.
- **Sequence numbers**: a held-back frame consumes none; the next packet takes the next sequence
  **[read]** **[measured]**: every packet received had a sequence one more than the last at every
  rate tried.
- **Measured** with moves sent every 16 ms for 3 s **[measured]**: rate 15000: 29 packets (every
  frame), suppress 0; rate 1000 (packets about 230 bytes): 14 packets, suppress byte 6 at times, and
  rate 100: 2 packets, suppress byte 10. When one frame's message alone exceeds the rate, a frame
  goes out once every 11 frames **[derived]** from the ring's rule.
