# Quake II protocols 35 and 36: the server's side

Clean-room facts for a server that admits protocol 36 clients and protocol 35 clients without
extended limits. Read by a clean-room reader from Q2PRO upstream at commit 601a8df. Each fact is
marked [read] (the source states it) or [derived] (worked out from read facts). The message layouts
themselves (new ids, serverdata tails, frame, player state, entities, signon forms) are in
`demo-formats-q2.md` section A and are not repeated. Types: byte, short (signed 16), word (unsigned
16), long (32), little-endian.

## Handshake

- The challenge answer is always `challenge <n> p=34,35,36`; no setting drops a protocol or turns 35
  or 36 off [read]. `<n>` is random, 0 to 2^31 - 1; up to 1024 challenges are kept, one per address,
  oldest overwritten [read]. A challenge of 0 is treated as empty, so that client gets "No challenge
  for address." [derived].
- `connect <protocol> <qport> <challenge> "<userinfo>"` then a tail [read]:
  - 35: max message length (absent 1390, 0 means 4086, below 0 or above 4086 refused with "Invalid
   maximum message length.\n"), minor (absent 1903, clamped to 1903 to 1905). Always the old
   channel, zlib assumed.
  - 36: max message length (as 35), netchan type (absent 1, only 0 or 1, else "Invalid netchan
   type.\n"), zlib (absent yes, else non-zero is yes), minor (absent 1015, clamped to 1015 to 1026,
   1016 becomes 1015).
  - 34: no tail: length 1390, old channel, no zlib.
  - Protocol below 26 or above 36: "Unsupported protocol version %d.\n"; 26 to 33: "You need Quake 2
   version 3.19 or higher.\n".
- Qport [read]: stored as given, not masked. 34 packets carry 16 bits. 35 and 36 packets carry one
  byte only when the stored qport is non-zero, else none and the source port identifies the client.
  A stored qport above 255 then never matches [derived].
- Packet length [read]: start at 1390; 35 and 36 take the asked value; a non-loopback client is
  lowered to `net_maxmsglen` when that is above 0; then raised to 512. `net_maxmsglen` defaults to
  1390, is clamped to 512 to 4086 when non-zero, and 0 means no cap. The "Fixing up maxmsglen"
  admin line belongs to path-MTU discovery that this commit's build never enables [read].
- The serverdata carries the minor fixed at connect; the server never answers above what the client
  asked [derived]. Gates on the minor that matter without extended limits [read]: 35 1904 compact
  move form, 35 1905 long solid; 36 1017 beam rule, 1018 brush 16-bit angles, below 1020 no layout
  compression while recording, 1021 compressed downloads, 1022 client number as a short (below it a
  view number of 255 or more is replaced by the client's slot, and a client in slot 255 is warned),
  below 1023 picture state in place of cinematic, 1024 flags short and streamed signon.
- Answer [read]: `client_connect`, then ` nc=<asked type>` (36 only; the server's `net_chantype` is a
  client setting and is not consulted), ` ac=1` (anticheat builds only), ` dlserver=<url>` (when
  `sv_downloadserver` is set), ` map=<map>` (always).
- Movement settings, all latched [read]: `sv_strafejump_hack` (default 1: 35 and 36; 2 adds 34) removes
  the landing pause; `sv_waterjump_hack` (default 1: 36; 2 adds 34 and 35) removes the fixed upward
  speed of a jump in deep water; `sv_qwmod` (default 0, 36 only) switches on QW movement (top speed
  320, friction 4, water friction 4, water speed 0.7, QW air acceleration). 35 and 36 spectators fly
  at double time; 36 spectators also get flight friction 4 (not 9) and keep moving when no more
  acceleration is wanted.

## Channel

- New channel header [read]: long sequence (bits 0 to 29, bit 30 fragment, bit 31 reliable), long ack
  (bits 0 to 29, bit 31 reliable toggle), then from the client only one qport byte when non-zero, then
  on a fragment a word (bits 0 to 14 offset, bit 15 more follow). The old channel keeps 31-bit
  sequences; with 35 or 36 its only change from 34 is the optional one-byte qport [read].
- Fragmenting [read]: the whole datagram (reliable then unreliable) is fragmented when it exceeds the
  packet length; pieces of packet-length bytes share one sequence number, which advances after the
  last piece. Largest message 32 768 bytes. The receiver accepts pieces strictly in order; a gap or a
  repeat drops the piece, a new sequence discards an unfinished one, and nothing is resent piece by
  piece, so a lost piece loses the datagram and the reliable part comes again by the usual rule
  [read] [derived].
- Server use [read]: new-channel reliables go straight into one 32 768-byte buffer that is fragmented
  as needed; frames get the whole 32 768-byte budget, never truncate entities and write unreliables
  all or none. Old-channel reliables are queued in packet-sized messages, one in flight. While pieces
  are pending a frame is replaced by the next piece and marked suppressed. Rate (last 10 packet sizes
  against `rate`) and the 1000 ms keep-alive are the same on both.

## Client messages

- Message 2 on 35 and 36: id's three moves without the checksum byte [read]. 35 from 1904: buttons
  byte first, whose bits 2 to 6 make pitch (byte x 64), yaw (byte x 256), forward, side, up (byte x
  5) one signed byte each [read].
- Messages 10 and 11 (36): command byte low 5 bits the id, top 3 bits the extra frame count (4 or more
  drops "too many frames in packet"); 11 has a long last frame (10 means -1); a light level byte; then a
  bit stream, least significant bit first, padded to a byte: per frame 5 bits of move count, then
  each move delta-coded from the one before (first from zero): 1 bit changed, 8 bits flags (bit 7 means
  msec), pitch and yaw each 1 bit then 8-bit signed delta or 16-bit signed value, roll 16 bits,
  forward, side, up 10 signed bits each, buttons 3 bits (attack, use, any), msec 8 bits. No impulse,
  no checksum [read]. Under loss the server runs the last command for losses beyond the backups, then
  the backup frames the loss covers, then the newest frame [read].
- Message 12 (36): string key, string value, repeated while the next byte is 12; at most 8 user info
  updates per packet; drops "oversize userinfo key", "oversize userinfo value", "malformed userinfo",
  then the full user info checks run [read].
- Message 5 (35 and 36): short index, short value. Indexes 0 to 13 stored unclamped, others ignored.
  0 no gun (1 all gun fields, 2 keeps the gun index), 1 no blend, 2 recording (cancels every
  omission), 3 player updates (stored only), 4 frame rate (variable-fps builds, 36), 10 no gibs, 11
  no footsteps, 12 no prediction (36), 13 no flares (extended only) [read].
- Per packet [read]: one move ("multiple clc_move commands in packet"), 8 string commands (more
  skipped; 1024 characters or more drops "oversize stringcmd"), 8 user info updates, "unknown
  command byte", "read past end of message".

## Server writes

- Compression [read]: only to zlib clients; the signon messages, and a layout or status-bar
  configstring the game sends to one client when at least half the packet length and the client is
  not recording (or is 36 at 1020 or later). Raw deflate, window 15, memory level 9, default level,
  one stream per message; sent plain when deflate fails (warning "Error <code> compressing <n> bytes
  message for <name>") or does not shrink the message. Old-channel signon batches are cut so their
  worst-case compressed size fits the packet length.
- Signon after `new`, all reliable [read]: newline stufftext, serverdata, (36 below 1022 in slot 255:
  the warning), newline stufftext, on first `new` the version query and the admin's connect commands,
  the forced-reconnect query, cvar ban queries, then the gamestate (old channel: configstring and
  baseline messages; new channel from 1024: streams 25 then 26; new channel below 1024: message 23),
  then `precache <spawn count>`. The client answers `begin`; `configstrings` and `baselines` do
  nothing.
- Message 24 is sent only by variable-fps builds, to a 36 client that sent setting 4: long 1, long
  rate [read]. A default build never sends it, and index 0 is never sent [read].
- Frames [read]: delta only from the client's acknowledged frame when it is under 16 frames old, was
  sent, and its entities are still held; otherwise offset 31, so offsets are 1 to 15 [derived]. 35
  gets the rate-skip count (4 bits); 36 gets flags suppressed, client dropped, client predicted,
  cleared after each frame. Not recording, 35 and 36: view angles left out while alive and predicted,
  delta angles while dead, gun or blend with settings 0 and 1; 36 with setting 12 also the
  prediction fields. Omitted fields keep their previous values in the server's copy. 36 gets the
  client number when it differs from the delta frame's (or from 0). With no area bits, 35 gets one
  byte 255, 36 gets length 0.
- Entities [read]: to an optimised 36 client (not recording, alive) the own entity's origin, angles
  and old origin are never sent; a chased player's entity is hidden (model 0) from 34, 35 and
  recording 36 clients; no gibs removes gib entities; no footsteps clears footstep events; at most
  128 entities a frame unless `sv_max_packet_entities` says otherwise.

## Downloads

- Compressed download (message 22) only to a 36 client at 1021 or later with zlib, from offset 0, for
  a file stored deflated in a .pkz [read]. The data is the zip entry's raw deflate stream cut into
  pieces; size, percent and `sv_max_download_size` refer to the compressed stream [read]. Everyone
  else gets message 16 [read].
- A piece is byte id, short length, byte percent, data; length at most packet length minus the
  pending reliable bytes minus 4; one piece per `download` or `nextdl`, only with no reliable
  message unacknowledged; no frames while downloading [read].
