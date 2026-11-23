# Quake II protocols 35 and 36: the client's side

Clean-room facts, read from Q2PRO upstream at commit 601a8df (2025-12-11), client and common code.
No code. [read] = the source states it; [derived] = worked out from read facts. Complements
`demo-formats-q2.md` section A and `q2-proto36-server.md`; their facts are not repeated. Where an error
text below starts with "<routine>: ", the real text starts with the name of the parsing routine
that raised it. Every "drop" is a disconnect with that text shown as the error.

Corrections to the earlier notes: none found. One addition to section A: the client accepts message 24
(setting) on 35 as well as on 36 [read].

## 1. Choosing the protocol

- `cl_protocol`: default 0, no range clamp, no change callback [read]. It is read into the "asked"
  protocol whenever a connection starts: `connect`, `reconnect` typed at the console, and a passive
  connect [read].
- Challenge answer [read]: the client looks at every argument after the challenge number. An argument
  starting with `p=` is read as a comma-separated list of decimal numbers. Only 35 and 36 are noted;
  34 and anything else is ignored. Several `p=` arguments are merged.
- Choice [read], run once on the challenge:
  - asked 0 becomes 36;
  - asked 36: 36 if the list has 36, else go on as asked 35;
  - asked 35: 35 if the list has 35, else 34;
  - anything else (34, or any other number such as 33 or 99): 34.
  So a forced value the server does not list falls DOWN (36 to 35 to 34), never up; a value outside
  35/36 always means 34 [derived].
- No `p=` at all (an id-style server answering just `challenge <n>`): 34 [derived from read].
- Debug line only: "Selected protocol %d" [read].
- Local server (loopback, no challenge): asked = `cl_protocol`, but if outside 34 to 36 it becomes
  36 (so 0 gives 36) [read].
- Serverdata check [read]: as soon as serverdata arrives, if its protocol number differs from the
  chosen one and this is not demo playback, the client drops with "Requested protocol version %d,
  but server returned %d." (first the chosen, then the received). It happens on every serverdata
  (first connect and every map change). Only demo playback tolerates a mismatch.
- After `client_connect` the client prints "Connected to %s (protocol %d).\n" (address, chosen
  protocol) [read].
- `connect` with more than one argument [read]: prints "Second argument to `%s' is now ignored.
  Set protocol via `cl_protocol' variable.\n" (the command's name in place of %s; the quotes are a
  backquote and a straight quote), then connects anyway with the first argument.
- The `connect` address may be written `quake2://<address>` with an optional trailing slash, which is
  stripped [read]. A bad address prints "Bad server address\n".

## 2. The connect line

- Form [read]: `connect <protocol> <qport> <challenge> "<userinfo>"<tail>\n`, all decimal.
- Tail [read]:
  - 34: empty.
  - 35: ` <maxmsglen> 1905` (the minor is always the client's newest, 1905).
  - 36: ` <maxmsglen> <net_chantype> <zlib> 1026` (minor always 1026).
- maxmsglen [read]: the `net_maxmsglen` value; to a local address (loopback or the machine's own)
  always 4086. `net_maxmsglen` default 1390; when non-zero it is clamped to 512 to 4086 on change; 0
  is allowed and is sent as 0 (the server reads 0 as 4086).
- `net_chantype` [read]: default 1, no clamp on the client; sent as typed (the server refuses other than
  0 or 1 with "Invalid netchan type.").
- zlib flag [read]: 1 in a build with zlib, 0 in a build without it. Never tied to a setting, so a
  normal build always sends 1.
- qport [read]: cvar `qport`, default the low 16 bits of the millisecond clock at start-up (random
  0 to 65 535). 34: the full value is sent in the connect line and as a 16-bit field in every
  packet. 35 and 36: the value is masked to its low 8 bits for both the connect line and the
  channel; when that byte is 0 no qport byte is written in packets at all [read].
- None of `net_maxmsglen`, `net_chantype` or the zlib flag reaches a 34 server [derived].
- The client's own outgoing channel [read]: its packet length is fixed at 1024 bytes for every
  protocol (not `net_maxmsglen`). On the old channel the client's reliable buffer is 1024 bytes; on
  the new channel it is 32 768 and the client fragments its own datagrams at 1024 bytes.
  `net_maxmsglen` only tells the SERVER how big its packets to the client may be.
- `client_connect` answer [read]:
  - default channel type: new if the chosen protocol is 36, else old;
  - ` nc=<n>` overrides it on ANY protocol: the client uses the server's answer, not its own ask.
    A value other than 0 or 1 disconnects with "Server returned invalid netchan type". A 36 server
    that omits `nc=` gets the new channel whatever `net_chantype` was [derived];
  - ` ac=<n>`: anticheat; in a build without it and n of 2 or more, prints "Anticheat required by
    server, but no anticheat support linked in.\n" and goes on;
  - ` map=<name>`: kept for the loading screen;
  - ` dlserver=<url>`: the first one sets the HTTP download server; none clears it.
  - Then the channel is set up and the client sends the `new` string command.
- Duplicate or stray answers are ignored silently (debug lines "Dup challenge received.  Ignored.",
  "Dup connect received.  Ignored.", "... from different address.  Ignored.") [read].

## 3. Minor versions

- 35: supports 1903 to 1905, asks 1905 [read]. In serverdata: a value inside 1903 to 1905 is taken
  as is, even if lower than asked, silently. A value outside is clamped into 1903 to 1905 with the
  warning (three lines) "R1Q2 server reports unsupported protocol version %d.\nAssuming it really
  uses our current client version %d.\nThings will break if it does not!\n" (received, 1905)
  [read]. The non-zero "enhanced" byte drops with "'Enhanced' R1Q2 servers are not supported"
  [read]. The two bytes after the minor (old advanced-deltas byte, strafe-jump byte) are read for
  every minor [read].
- 36: supports 1015 to 1026, asks 1026 [read]. Any value 1015 to 1026 is taken as is, silently, even
  if lower than asked. A value outside drops with "Q2PRO server reports unsupported protocol version
  %d.\nCurrent client version is %d." (received, 1026) [read]. No clamp.
- On 35 the beam old-origin rule is on for every minor; the long solid from 1905 [read]. On 36 the
  long solid and the 16-bit-mask rule are always on, beam from 1017, short angles from 1018 [read].

## 4. Moves on 35 and 36

- Choice [read]: on 36 with `cl_batchcmds` non-zero (default 1) the batched message (10 or 11);
  otherwise (34, 35, or 36 with `cl_batchcmds` 0) the classic message 2.
- Message 2 [read]: the three last moves (two before, then the newest), each followed by the light
  level byte, after a long last-frame number (-1 when `cl_nodelta` is set or the current frame is
  not valid). 34 puts the checksum byte right after the id; 35 and 36 have none.
  - 35 at minor 1904 or later: the compact form, and only then [read]. The client writes the buttons
    byte right after the bits byte, and only when the buttons changed (bit 6 of the bits). In that
    byte, bits 2, 3, 4 are set for forward, side, up when that move changed and is a multiple of 5;
    those moves are then one signed byte of value/5, the others a short. Angles are always shorts
    (the client never sets the angle bits 5 and 6 of the buttons byte). Impulse and msec as 34. At 1903,
    and always on 36, the 34 move layout without checksum [read].
- Batched (36) [read]: id 11 with a long last frame, or 10 (no-delta) under the same conditions
  as -1 above; the top 3 bits of the id byte = `cl_packetdup`; one light level byte; then the bit
  stream. Frames carried = `cl_packetdup` + 1, oldest first: for each of the last packets sent, the
  moves made since the packet before it (so a frame may hold 0 moves). The delta chain runs across all
  frames of the packet, the first move from zero. If a frame would hold 32 or more moves, the client
  warns "<routine>: MAX_PACKET_USERCMDS exceeded" and sends the packet with no move at all [read].
- `cl_packetdup` [read]: default 1, clamped 0 to 3 each time a batched packet is built. 36 batched only.
- `cl_maxpackets` [read]: default 30. 0 = no limit; a non-zero value below 10 is set to 10. The gap is
  1000/value ms, then rounded down to a whole division of 100 (100 / (100 / gap)): 30 gives 33 ms, 60
  gives 16 ms. A packet goes early anyway when one is forced, when reliable data waits, or when a
  reliable ack is pending. It throttles the batched path. On the classic path (34, 35, unbatched 36)
  it applies only with `cl_fuzzhack` (default 0): then a held-back packet still increments the outgoing
  sequence (it looks dropped to the server), but no more than 2 moves are ever held back in a row
  [read]. With defaults, the classic path sends one packet per move [derived].
- Moves are made at `cl_maxfps` (default 62) per second in the default asynchronous mode [read], so
  with defaults a 36 packet carries about 2 moves per frame and 2 frames [derived].
- `cl_instantpacket` [read]: default 1. Pressing attack or use (key down) while in the game and not
  playing a demo makes a move at once and forces a packet out now, bypassing `cl_maxpackets`. It
  works on every protocol (on the classic path the extra move goes out in its own packet).
- `cl_batchcmds` default 1, read only on 36 [read]. `cl_nodelta` default 0 [read].
- The 34 path is unchanged by any of this, apart from `cl_instantpacket` and `cl_fuzzhack`, which
  are not protocol-specific [read].
- When no new move exists the client sends nothing on the move path. While not yet active (or
  paused) it sends only reliables and a keep-alive [read] (section 13).

## 5. Client settings (message 5) sent by the client

All are reliable: id 5, short index, short value [read]. Each is skipped unless the channel's
protocol allows it (checked on the live channel, so nothing is sent while not connected) [read].

| index | sent on | value | cvar (default) |
| --- | --- | --- | --- |
| 0 no gun | 35, 36 | 2 when `cl_gun` is -1; 1 when `cl_gun` is 0, or `cl_gun` is 1 and `hand` is 2; else 0 | `cl_gun` (1), `hand` (0) |
| 1 no blend | 35, 36 | 1 when `cl_blend` is 0 | `cl_blend` (1) |
| 2 recording | 35, 36 | 1 while recording a demo or relaying to GTV viewers, else 0 | - |
| 4 frame rate | 36, variable-fps builds only | `cl_updaterate` as is (frames per second wanted) | `cl_updaterate` (0) |
| 10 no gibs | 36 | 1 when `cl_gibs` is 0 | `cl_gibs` (1) |
| 11 no footsteps | 36 | 1 when `cl_footsteps` is 0 | `cl_footsteps` (1) |
| 12 no prediction | 36 | 1 when `cl_predict` is 0 | `cl_predict` (1) |
| 13 no flares | 36 with extended limits | 1 when `cl_flares` is 0 | `cl_flares` (1) |

- Index 3 (player updates) is never sent [read].
- When [read]: all of them at begin (after precache, once loading is done), in the order
  frame rate (before the `begin <spawncount>` string command), then after it gun, blend, gibs,
  footsteps, predict, recording, flares. Then each again when its cvar changes (gun also when `hand`
  changes), at any time while connected, even before begin. Recording: on record start (after the
  demo header is set up), on record stop (after "Stopped demo (%s).\n"), and on GTV relay start and
  stop.
- `variable-fps` is a build option, off by default, so a default client never sends index 4 [read].

## 6. Message 24 (setting) on the client

- Accepted on 35 and 36 (refused on 34 as "illegible server message") [read]. Two longs, index and
  value.
- Index 0 (player-update rate): read and ignored [read]. Every index other than 1: ignored.
- Index 1 (server frame rate): acted on only in a variable-fps build [read]. Then: divisor =
  rate / 10, clamped 1 to 6; frame time = 100 / divisor ms (100, 50, 33, 25, 20, 16). The client's
  server time becomes (frame number minus a base) times the frame time. A "key frame" is every frame
  whose number is a multiple of the divisor, and the 10 Hz things (entity events and their lerp, the
  demo frame writer) tick only on key frames. When the client is already in the game, the base is
  realigned to a multiple of the divisor so that time stays continuous. Every serverdata resets the
  rate to 10 Hz (divisor 1) [read].
- In a default build (no variable fps) the client stays at a fixed 100 ms frame [read]. A server
  sending faster frames to it anyway would make every frame count as 100 ms of server time, so the
  game would run fast and lerp wrong; nothing on the client guards against it [derived]. So a server
  must send faster frames only to a client that asked with setting 4 and only after telling it with
  message 24 index 1, as the server notes say [derived].

## 7. The frame as the client handles it

- 36 frame flags [read]: the client keeps them for display only (they change no game state).
  - suppressed (bit 0): the netgraph draws one "suppressed" mark, the lag meter marks that sample in
    the warning colour; the "turtle" shows SUPPRESSED (`scr_showturtle` 2 or more).
  - client predicted (bit 2): shown as CLIENTPRED by the turtle; when set, client dropped is
    cleared, so the two are never shown together.
  - client dropped (bit 1): shown as CLIENTDROP (`scr_showturtle` 2 or more).
- 35's 4-bit count [read]: the netgraph draws that many suppressed marks; any non-zero count also
  counts as the suppressed flag. 34 reads its byte the same way.
- The client adds its own flags: a gap in incoming sequences (SERVERDROP), and the delta checks below.
  All flags are cleared at the start of each frame message [read].
- Delta frame (35 and 36: frame number minus the 5-bit offset; 31 = none) [read]:
  - none: the frame is complete (flag NODELTA);
  - equal to the current frame: bad, the frame is not used (debug "delta from current frame");
  - the stored slot (16 frames kept) holds another number: too old, not used ("delta frame was
    never received or too old");
  - the stored frame is marked invalid: not used ("delta from invalid frame");
  - its entities have been overwritten in the entity ring: not used ("delta entities too old").
  - A frame not used is still parsed to the end; the client marks its current frame invalid, so the
    next move asks with -1 (message 2) or message 10 (batched) [read]. Same rules as 34; only the
    offset encoding is new [derived].
- Client number (36, extra bit 6) [read]: byte before 1022, short from 1022; a value out of range
  drops with "<routine>: bad clientNum". Without the bit it is copied from the delta frame, or 0 when
  there is no delta frame. On 34 and 35 it is always the serverdata player number.
- What the frame's client number changes [read]: it names the entity treated as "me": that entity is
  left out of the prediction's solid list; it is drawn at the predicted origin and angles (and hidden
  in first person); the sound listener; muzzle-flash and beam special cases (own gun beam from the
  view); the scoreboard highlight; the origin repair below. When it changes from one frame to the
  next, the player state is not interpolated across the change. The serverdata number stays as
  the player's own slot [read].
- Optimised own entity (36 only) [read]: while the player state's move type is alive (below dead), the
  entity named by the frame's client number takes its origin (and its angles and the rest of what the
  player state carries) from the player state, because the server leaves them out.
- View angles on 35 and 36 [read]: while alive the view uses the predicted angles (the client's own
  angles plus the delta angles), also on the first frame after loading and with prediction off; on the
  frame the player dies the view lerps from the predicted angles to the server's angles. 34 lerps the
  server's angles when not predicting.

## 8. Serverdata tails in the client's prediction

All start from the default parameters at each serverdata: top speed 300, friction 6, water friction 1,
fly friction 9, water speed factor 0.5, spectator time factor 1 [read].

- Strafe-jump hack (35 byte, 36 flag bit 0) [read]: landing no longer sets the "time land" flag and
  its 144 or 200 ms jump block when falling faster than 200 units/s.
- QW movement (36 only) [read]: top speed 320, friction 4, water friction 4, water speed factor 0.7,
  and air acceleration: in the air, the wished speed counted against the velocity is capped at 30
  units/s (the full wished speed still scales the gain); without it, air acceleration is id's normal
  acceleration with factor 1.
- Water-jump hack (36 only) [read]: jumping with water level 2 or more no longer sets an upward speed
  (100 water, 80 slime, 50 lava); the jump then just leaves the ground.
- Always on 35 and 36 [read]: spectators move at double time (time factor 2). 36 also always: fly
  friction 4, and the fly move keeps moving at the current speed when no more acceleration is
  wanted (id's returns before moving).
- 36 server state byte (used from 1019; read and ignored before; 10 Hz game assumed) [read]: 3
  (picture) or 5 (cinematic) plays the level name as a picture or cinematic instead of loading a map
  (the same as player number -1). 4 (broadcast, an MVD relay): prints and center prints do not fire
  the client's text triggers, the automatic version reply to chat is off, and chat lines starting
  with "[MVD] " make no sound. 0, 1, 2: no visible effect beyond being stored [read].
- Flag bits 3 and 4 (extended limits) switch the configstring table, wider fields and the extended
  collision rules; bit 4 without bit 3 drops with "Q2PRO_PF_EXTENSIONS_2 without Q2PRO_PF_EXTENSIONS"
  [read].

## 9. zdownload on the client

- Refused on 34 [read]. A build without zlib drops with "Compressed server packet received, but no
  zlib support linked in." [read].
- Piece [read]: short size, byte percent; on 35 a short uncompressed size follows; then the data.
  Size -1 ends the download: percent 0 prints "[UDP] <file> [FAIL] [<n> remaining file(s)]", other
  percent "[STOP]". Other negative sizes drop with "<routine>: bad size: %d". A piece with no download
  requested drops with "<routine>: no download requested".
- Inflating [read]: one raw-deflate stream object for the whole download. Each piece is fed in and
  inflated with sync flushes into a 64 KiB scratch buffer, repeated until the buffer is not full,
  every output written to the file as it comes. When the stream ends it is reset, so 35's
  per-piece streams and 36's single stream share one path. On 35 a total out that differs from the
  stated size only warns "[UDP] Decompressed size mismatch: expected %d, got %lu\n" (the count is
  the stream's running total) and goes on. 36 sends no size; the result size is just whatever the
  stream produces [read].
- Progress [read]: the server's percent byte is shown as is; the position counter adds the
  compressed piece size. Every piece below 100 percent is answered with the string command `nextdl`;
  100 percent closes the file, renames the temporary `<file>.tmp` to its name and prints
  "[UDP] <file> [DONE] [...]".
- Inflate error [read]: "[UDP] Error %d decompressing download\n" (the inflate code); the file is
  closed and abandoned (no FAIL line; the `.tmp` remains) and the next queued file starts.
- Asking [read]: on 35 (zlib build) the client asks explicitly: `download "<file>" <offset>
  udp-zlib`, with offset 0 for a new file. On 36 it never asks: `download "<file>"` or
  `download "<file>" <offset>` to resume; compression is the server's choice by minor (1021) and the
  zlib flag.

## 10. zpacket on the client

- Refused on 34 (illegible) [read]. Word compressed length, word output length. Output length above
  32 768 drops with "<routine>: invalid output length" [read]. The input is the next compressed-length
  bytes of the message (reading past the end drops with "<routine>: read past end of message").
- Raw deflate inflated in one go to exactly the stated output buffer; anything but a clean stream end
  drops with "<routine>: inflate() failed with error %d" [read]. The output is then parsed as server
  messages.
- Nested zpacket drops with "<routine>: recursively entered" [read]. No zlib in the build: the
  same "Compressed server packet received, but no zlib support linked in." [read].

## 11. The teleport bits (RDF 16, pm flag 128)

- With extended limits (36 at 1024 or later with flag bit 3, and the extended demos) the client watches
  render-flag bit 4 (16); otherwise, on every protocol including 34, it watches player-move flag
  bit 7 (128) [read]. Only a CHANGE of the bit between the previous and the current frame counts.
- Effect [read]: the previous frame's player state is replaced by the current one, so the view does
  not interpolate this frame: origin, view offset, angles where lerped, kick, gun offset and angles,
  field of view and the blends snap. Entities are not affected. The same snap happens when the
  previous frame is missing or invalid, when the origin moved more than 256 units on any axis, when
  the own entity carries a teleport event, when the frame's client number changed, and with
  `cl_nolerp` 1.

## 12. User info on 36, and other 35/36-only sends

- Changed user-info cvars [read]: not connected, demo playback, or a private cvar: nothing. On 34 and
  35 any change sends the full user-info string (message 3) at the next send. On 36 each distinct
  changed cvar is queued (the same cvar twice counts once); up to 7 go as message 12 pairs (name,
  value; a cvar no longer in user info is sent with an empty value); the 8th distinct change in one
  window switches to one full message 3 instead. Sent before the next move or keep-alive.
- No other 35/36-only string commands [read]: the version reply, `nextdl`, `begin`, `new` are the
  same on every protocol; there is no `nextserver` in the client. The only per-protocol requests are
  35's `udp-zlib` download argument and the settings of section 5.

## 13. What a 34-only client lacks

- Accept up to 4096-byte datagrams (the client receives into a 4096-byte buffer); a sequenced packet
  under 8 bytes is a runt and is ignored [read].
- The new channel (section A of the server notes) on 36 with nc=1: 30-bit sequences, fragment
  reassembly into a 32 768-byte buffer, the client's own fragmenting at 1024 [read]. Drops on receive
  are silent (debug only).
- The optional one-byte qport on 35/36 packets (none when 0) instead of 34's 16-bit one [read].
- Sequence limit [read]: when the outgoing sequence reaches 2^31 - 256 (old channel) or 2^30 - 256
  (new), "Outgoing sequence too big" (drop). Reliable buffer overflow: "Reliable message overflowed"
  (drop).
- Keep-alive [read]: while not yet active or paused, a packet (possibly empty) goes out when
  reliable data waits, a reliable ack is pending, fragments are pending, or more than 1000 ms have
  passed since the last send. Not protocol-specific, but on the new channel the pending fragments
  must be pushed out this way.
- Timeouts [read]: "Server connection timed out." after the `cl_timeout` period (more than 5
  checks in a row); with an ICMP error and 5 s of silence "Server connection was reset.".
- Message-id rule [read]: top 3 bits of the id byte allowed only on 35/36 and only on message 20,
  else "<routine>: illegible server message: %d"; 21, 22, 24 refused on 34; 23, 25, 26 refused on
  anything but 36 (so r1q2's player update 23 is refused on 35).
- 35 and 36 frames carry no playerinfo or packetentities id bytes (34 drops with "<routine>: not
  playerinfo" / "not packetentities" when they are missing) [read]. A frame whose field of view is 0
  drops with "<routine>: bad fov"; area bits longer than the buffer: "<routine>: invalid areabits
  length" [read].
- Serverdata: "Requested protocol version %d, but server returned %d.", the minor-version texts of
  section 3, "Oversize gamedir string", and the warning "Serverdata has invalid playernum %d\n"
  (the player number is then set to -1, not dropped) [read].
- There is no "Server returned version %d, not %d" text in this client [read].
- Predicted view angles on 35/36 (section 7), because the server leaves them out while alive [read].
