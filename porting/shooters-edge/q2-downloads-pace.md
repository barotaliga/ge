# Quake II server: download piece size and pace (facts, clean-room reader 2026-11-23)

Source read: Q2PRO, upstream (github.com/q2pro/q2pro), default branch at commit
601a8df8433b0c50dbbe37c0716c3793fff140a7 (committed 2025-12-11). GPL-2.0. Read by a clean-room
reader; nothing below is code. Tags: [read] = stated by the source as read; [derived] = worked out
from read facts; nothing here was measured by running the program.

## Summary that corrects the porting notes

- [read] The server does NOT send several pieces a frame. It holds exactly one piece "owed" per
  client at a time. The `download` command owes the first piece; each `nextdl` command owes one
  more. When a piece is sent, nothing more is owed until the next `nextdl`. So the pace is one
  piece per client request, sent as soon as the conditions in Q2 and Q4 allow, which is usually
  much faster than one per server frame.
- [read] The piece is bigger than 1024 bytes: it fills the room left in one packet's reliable
  part (about 1386 bytes at default settings, Q3).
- [read] This holds for every protocol (34, 35, 36) and both net channel kinds alike.

## 1. Who gets the "asynchronous" pieces; what protocol 34 gets

- [read] Every client gets the same treatment, whatever its protocol (34, 35 or 36) and whatever
  its net channel kind (the old channel or the newer one with fragments). There is no setting
  that selects a stock-style 1024-byte, one-per-frame mode. The only protocol-dependent parts are
  the compressed form (Q6) and, for the old net channel, that queued reliable messages are first
  packed into the outgoing reliable buffer before the piece is added (Q3).
- [read] The client's state matters in one way: a client that is in the game ("spawned") but has
  a download open is treated as not active. It gets no game frames while the download is open and
  is served entirely by the connection-time sender (Q2, Q7). When the last piece goes out, the
  download is closed and that client gets game frames again from the next frame.
- [read] A protocol-34 client therefore gets one piece per `nextdl` it sends (plus the first
  piece for the `download` command), each sized to fill the packet (about 1386 bytes at
  defaults), not one fixed 1024-byte piece per server frame. It never gets two pieces without
  having sent `nextdl` in between.
- [read] The server makes no special allowance for a stock protocol-34 client's buffer beyond
  the 1390-byte payload limit; whether such a client accepts the larger piece is not settled by
  this source.

## 2. When a piece is sent; how often at most

- [read] Pieces are sent only by the server's connection-time sender, which runs once on every
  pass of the server's main loop, before the game-frame check. It does not run inside the
  10 Hz game-frame send. It also runs once when a map change begins.
- [read] A dedicated server's main loop sleeps on its sockets until either a packet arrives or the
  next game frame is due. So the sender runs right after any incoming packet (including the one
  that carries `nextdl`) and at least once per game frame (every 100 ms at the base 10 Hz). On a
  listen server the loop also runs every client frame.
- [read] For one client, a piece goes out on a pass only when all of these hold:
  1. a piece is owed (a `download` or `nextdl` was received since the last piece),
  2. the client's rate pause has elapsed (Q4),
  3. any fragments of an earlier large reliable message have all been sent,
  4. the reliable buffer is empty, that is, the previous reliable (the previous piece, or any
  other reliable message) has been acknowledged by the client,
  5. the reliable buffer under construction has at least 5 bytes of room left (Q3).
  If one fails, the piece stays owed and is tried on the next pass.
- [read] There is no fixed per-second cap. The ceiling is the lower of the rate (Q4) and one piece
  per round trip (the server waits for both the acknowledgement of the last piece and the next
  `nextdl`, which in practice arrive together in the client's next packet).
- [read] If an unacknowledged reliable is outstanding for more than 1000 ms since the last send,
  the sender resends it on its next pass (subject to the rate pause).
- [derived] Example: a 20 ms round trip and a 60000 rate give about 1386 bytes every
  max(20 ms, 23 ms), so about 60 kB/s; a 100 ms round trip gives about 14 kB/s whatever the rate.

## 3. "The packet's room"; the 4 bytes

- [read] The room is the client's maximum packet payload length (the net channel's limit, not
  counting the packet header), less whatever is already in the outgoing reliable buffer, less 4.
- [read] The 4 bytes are the piece's own header: the message id byte (16 or 22), the signed
  16-bit size, and the percent byte (1 + 2 + 1).
- [read] Piece size = the smaller of (bytes left in the file) and (max payload length minus bytes
  already in the reliable buffer minus 4). If the buffer already holds max payload minus 4 bytes
  or more, nothing is sent that pass.
- [read] What else can be in the buffer: reliable messages queued for that client (prints,
  stuffed commands, config strings during signon). On the old net channel these are copied into
  the buffer first on the same pass; on the newer channel they are already there. The piece then
  takes the rest. So pieces are often smaller than the maximum while other reliable traffic is
  flowing, and the last piece is just the file's remainder.
- [read] The max payload length is settled at connect:
  - default 1390 bytes (1400 minus a 10-byte worst-case header),
  - protocols 35 and 36 may ask for another value in the connect request (the client sends its
  own `net_maxmsglen`, default 1390; 0 means "as large as possible", 4086); protocol 34 cannot
  ask and gets 1390,
  - the server then caps it at its own `net_maxmsglen` (default 1390; 0 means no server cap),
  except for a loopback client, which is not capped (a local client asks for 4086),
  - the result is raised to at least 512 and must not exceed 4086 (a request over 4086 or below 0
  is refused with "Invalid maximum message length.").
  - On builds with path-MTU discovery, a newer-channel client still in signon whose reliable
  traffic triggers an ICMP "fragmentation needed" can have its limit lowered to the reported MTU
  minus 64 (the server prints "Fixing up maxmsglen for <name>: <old> to <new>, shown with an arrow").
- [derived] At defaults with an empty buffer: 1390 minus 4 = 1386 data bytes per piece; the whole
  datagram is 1390 plus the 8-byte sequence header = 1398 bytes on the wire (UDP payload).
- [read] The piece travels in the reliable part of the net channel (it is acknowledged and resent
  like any reliable message), which is why rule 4 in Q2 holds one piece in flight at a time.

## 4. How the rate is applied

- [read] The rate is the client's `rate` userinfo in bytes per second, clamped to the server's
  `sv_min_rate` .. `sv_max_rate` (defaults 15000 and 60000; each is clamped to at least 1500). A
  client with no `rate` in its userinfo gets 5000, which is NOT clamped up to the minimum.
  A loopback client has no rate limit; with `sv_lan_force_rate` 1 (default 0) LAN clients have
  none either.
- [read] The connection-time sender does not use the game frames' rolling budget. After each
  packet it sends to a client it sets a pause of (bytes sent in that datagram, header included,
  times 1000, divided by the rate) milliseconds, integer division, and sends nothing further to
  that client on any pass until that much server time has passed. With no rate limit the pause is
  0.
- [read] When the pause has not elapsed, nothing is dropped: the piece stays owed and is sent on
  a later pass of the main loop once the pause is over.
- [derived] At defaults a full 1398-byte packet pauses 93 ms at rate 15000, 23 ms at 60000, and
  279 ms for a client with no rate set (5000).
- [read] For an in-game (spawned) client the size of each such packet is also written into the
  game frames' 10-entry rate window, so the first frames after the download finishes may be
  suppressed if the window is still over the rate.
- [read] There is no separate download minimum or maximum rate.

## 5. Does the client still send `nextdl`

- [read] Yes. The client sends `nextdl` after every piece whose percent is not 100, and the
  server needs it: each `nextdl` owes exactly one more piece. A `nextdl` with no open download is
  ignored. A second `nextdl` before the owed piece is sent does not owe a second piece (the owed
  mark is a flag, not a count).
- [read] The first piece is owed by the `download` command itself. When the last piece is sent
  the server closes the download at once (it does not wait for a final `nextdl`).

## 6. The compressed form (message 22)

- [read] It is used only when all hold: the client is protocol 36, its minor version is 1021 or
  more, it said at connect that it has zlib (the connect field; absent means yes), the request
  has no resume offset (offset 0), and the file is found as an entry stored deflated inside a zip
  archive (a .pkz or other zip pack). Then the server sends the entry's raw deflate stream, as it
  sits in the archive, without inflating it. Otherwise (loose file, pak entry, stored zip entry,
  any resume offset, protocol 34 or 35) the plain form, message 16, is used. The server never
  compresses a file on the fly for downloads.
- [read] Protocol-35 clients never get message 22 for downloads from this server.
- [read] In message 22 all counts are in compressed bytes: the size short is the compressed bytes
  in this piece; the file size used for the percent, for the end test and for the
  `sv_max_download_size` check is the entry's compressed length; percent is (compressed bytes
  sent so far, including this piece) times 100 divided by the compressed length, rounded down.
- [read] The byte layout of message 22 from this server is the same as message 16: id byte,
  signed 16-bit size, percent byte, then the data. There is NO uncompressed-size field for
  protocol 36. The client takes the pieces as one continuous raw deflate stream and inflates as
  they arrive. (The protocol-35 variant of message 22, which carries an extra 16-bit
  uncompressed size after the percent, is understood by the client but never sent by this
  server for downloads.)
- [read] Piece sizing and pace are the same as for message 16.

## 7. In game as well as during signon; shared packets or their own

- [read] Downloads are accepted in any client state; the `download`, `nextdl` and `stopdl`
  commands are handled whatever the client's state.
- [read] While a spawned client has a download open it gets no game frames at all; its packets
  carry only reliable traffic and the download pieces. When the download ends the frames resume
  and the frame numbering is realigned for that client.
- [read] So a piece never shares a packet with a game frame. It does share the reliable buffer
  (and so the packet) with any other reliable messages queued for that client (Q3).

## 8. Settings, with defaults

- [read] `net_maxmsglen` (server side): caps each client's max payload length; default 1390,
  0 = no cap; otherwise 512..4086. Loopback clients are not capped. It sets the piece size.
- [read] `net_maxmsglen` (client side): the payload length a protocol 35 or 36 client asks for,
  default 1390; 0 = 4086.
- [read] `rate` (userinfo, bytes per second), `sv_min_rate` (15000), `sv_max_rate` (60000),
  `sv_lan_force_rate` (0): the pace, as in Q4. No-`rate` clients get 5000.
- [read] `sv_max_download_size`: the largest file served, in bytes; default 8388608 (8 MiB),
  0 or less means the built-in ceiling of 134221824 bytes (128 MiB plus 4 KiB). A larger file is
  refused (the client gets the failure form: size -1, percent 0).
- [read] `allow_download` (default 0 on a dedicated server, 1 otherwise) and the per-kind
  `allow_download_players`, `_models` (also covers sprites/), `_sounds`, `_maps`, `_textures`
  (also covers env/), `_pics` (all default 1) and `_others` (default 0): whether a file may be
  downloaded at all. Paths must be inside a folder, with no `..` and no leading or trailing dot
  or slash.
- [read] `sv_downloadserver` (default empty): an HTTP address advertised to clients for HTTP
  downloads instead; not part of the in-band pace.
- [read] There is no setting for a fixed piece size or a download-only rate.
- [read] Related edge cases a player sees:
  - `download <file> <offset>` resumes at a byte offset (plain form only). An offset equal to the
  file's size gets an immediate reliable message 16 with size 0 and percent 100; an offset past
  the size gets the print "File size differs from server." and "Please delete the
  corresponding .tmp file from your system." and then the failure form.
  - Any refusal (not allowed, missing, empty, too large, bad path) is message 16 with size -1 and
  percent 0, sent reliably.
  - `stopdl` sends message 16 with size -1 and the current percent, and closes the download.
  The client treats size -1 with percent 0 as a failure and any other percent as a stop.
