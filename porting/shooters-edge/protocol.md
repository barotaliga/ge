# The classic protocol, specified

The multiplayer protocol, version 28, in our words (README.md, the rules): the out-of-band
packets, the net channel, every message a server and a client exchange with its body field by
field, the signon, the deltas, the move and its checksum, the info strings, downloads, the first
tier of extensions and the server's lag compensation. Recorded games are `demos.md`; how the
client builds its commands and predicts is `gameloop.md` (§ The client), which this note does
not repeat.

**Where each fact came from.** A research session read the modern server, the modern client,
the shared protocol header and the 1996 release (2026-10-30), with the **modern server's
behaviour and defaults as the baseline**; where the 1996 release differs it says so. Message ids
and bit values are wire constants and are listed as such. The quantization and the move
checksum are measured (`vectors/quantize.txt`, `vectors/checksum.txt`). Anything not yet
confirmed against a capture is marked **verify**; `se-interop`'s loopback captures are the check
for every table here.

## 1. Conventions

1. **Byte order** is little-endian for every multi-byte field. `byte` is u8, `char` is i8,
   `short` is i16, `long` is i32, `float` is an IEEE 754 single.
2. **A string** is bytes up to and including a NUL. A reader drops byte 0xFF inside a string
   (it never appears in a valid one) and stops at the buffer's end if no NUL comes.
3. **Text** is the game's 8-bit character set: bit 7 set is the "bronze" (coloured) half of the
   font. Nothing in the protocol is UTF-8.
4. **Quantization** (base protocol). `vectors/quantize.txt` has worked values.

   | Field | Wire | Write | Read |
   | --- | --- | --- | --- |
   | coord | i16 | round half away from zero of `f × 8` (32-bit float arithmetic) | `n / 8` |
   | angle | u8 | `round_half_away(f × 256 / 360) & 255` | read as **signed** `char × 360 / 256` |
   | angle16 | u16 | `round_half_away(f × 65536 / 360) & 65535` | `short × 360 / 65536` |

   The 1996 release truncates the coordinate (`(int)(f × 8)`) instead of rounding; the
   difference is visible at 1.9375 (0x10 versus 0x0F). Match the modern server.
5. **Wider coordinates** are negotiated (§ 11): under the FTE float-coords bit (only when the
   server sets `sv_bigcoords`) every coord is a 4-byte float and every angle an angle16; under
   the MVD float-coords bit only entity and player **origins** become floats and angles stay
   one byte.
6. **Ports**: master 27000, client 27001, server 27500, the broadcast relay 27900.
7. **Sizes**: the largest in-band message (`MAX_MSGLEN`) and the largest datagram are both
   1450 bytes; the net channel's header is 8 bytes (10 from a client); the smallest MTU a client
   may ask for is 1350.

## 2. Connectionless packets

A packet that starts with four 0xFF bytes is out of band: a text command follows, tokenized like
a console line (`configs.md` § Lexing). Replies start with the same four bytes and a one-letter
type, unless noted.

| Request | Reply | Notes |
| --- | --- | --- |
| `ping`, or the single byte `k` | the single byte `l` (no 0xFF header) | a liveness probe |
| `status [opt]` | `n` + the status text | § 2.1 |
| `log` | the server's frag log | rarely used; verify the format before implementing |
| `rcon <pass> <cmd …>` | `n` + the command's output | § 2.3 |
| `ip <slot> <num>` | none | the real-address check during signon (§ 5, step 1) |
| `getchallenge` | `c` + decimal challenge + NUL + extension pairs | § 2.2 |
| `connect 28 <qport> <challenge> "<userinfo>"` + extension lines | `j` on acceptance, else `n` + the reason | § 2.2 |
| `lastscores`, `laststats`, `dlist…`/`demolist…`, `qtvusers`, `broadcast` | `n` + text | server extras; implement only if a tool needs them |

A print (`n`) is the text and its NUL. When a command's output is long the server flushes a
packet whenever adding more would pass `MIN_MTU − 10` bytes, so one request can draw several
replies.

### 2.1 Status

1. `opt` is a bit set (absent = 0): 1 server info, 2 players, 4 spectators, 8 spectators shown as
   players, 16 teams, 32 the broadcast relay's listeners, 64 flags, 128 the client type. `opt` 0 is
   the old style: server info and players.
2. The first line is the server info string (§ 9). Then one line per client:
   `userid frags minutes ping "name" "skin" top bottom`, followed by ` "team"` when 16 is set,
   ` "flag"` when 64 is set, and ` "b"` or ` "h"` (bot or human) when 128 is set.
3. Top and bottom colours are clamped to 0..16. A spectator shows frags −9999, a negative ping and
   the name `\s\<name>`; under bit 8 its frags field is the letter `S` instead.
4. Ping is 999 while the client is still connecting, 9999 when it has no samples, otherwise the
   mean of its frame pings in milliseconds. A bot reports its skill (or 10).

### 2.2 Challenge and connect

1. The server keeps a table of challenges by the sender's address (ignoring the port), replacing
   the oldest. A challenge is a random 31-bit number.
2. The `c` reply is followed, after the challenge's NUL, by binary pairs: a u32 extension family
   magic and a u32 bit set. Families: `FTEX` = 0x58455446, `FTE2` = 0x32455446, `MVD1` =
   0x3144564D (the four letters read little-endian). The modern server offers the sets in § 11.
3. The client answers `connect 28 <qport> <challenge> "<userinfo>"\n` followed by one line per
   family it accepts: `0x<magic> 0x<bits>\n` in hex. It also puts `*z_ext` in its userinfo: the
   ZQuake bits it understands (511, all nine; § 11).
4. The server checks, in order, and refuses with an `n` print naming the reason:
   1. the version is 28, else "Server is version …";
   2. the challenge matches the table (loopback is exempt), else "Bad challenge." or
      "No challenge for address.";
   3. the userinfo has no `&c` or `&r` and no malformed backslash pairs;
   4. passwords: a `spectator` key may carry the spectator password, the `password` key the
      player password, and "none" means no password is set; both keys are removed, and a
      spectator gets `*spectator 1`;
   5. a reconnect from the same address with the same qport (or port) drops the old slot
      (`sv_reconnectlimit`, default 0, can throttle it);
   6. a free slot of the wanted kind, else "server is full"; `sv_forcespec_onfull` may seat the
      client as a spectator instead.
5. On success: `j`. The channel is set up with the client's `mtu` key (clamped to 1350..1450,
   minus the header). User ids run 1..99 and wrap. `*z_ext` is copied out of the userinfo and
   removed from it. Protocol extensions are offered later (§ 5, the `pext` step) unless the
   userinfo marks a proxy (the Qizmo key or `*qtv`).

### 2.3 Remote console

1. `rcon <pass> <command …>`. With `sv_crypt_rcon` (default 1) the password argument is a SHA-1
   digest in hex followed by the client's time as hex: the digest covers the command name, a
   space, the password, the time string, a space, then each further argument followed by a space.
   A time more than `sv_timestamplen` (default 60) seconds off is refused. **verify** the exact
   concatenation against a capture before relying on it.
2. At most `sv_rconlim` (default 10) attempts a second per server.
3. Some first tokens are always refused (file removal and listing, `chmod`, the admin and
   logging cvars, `if`, `localcommand`, the rcon security cvars, the command line). Replies:
   "Bad rcon_password." or "Command not valid.".
4. Output is redirected and returned as `n` prints (§ 2).

## 3. The net channel

1. **Header** (every in-band packet): u32 sequence, bit 31 set when the packet carries the
   reliable message; u32 acknowledged sequence, bit 31 the even/odd bit of the last reliable
   message received. A client adds a u16 qport after it, so the server finds it when a NAT moves
   its port. Out-of-band packets are the same header with sequence −1 (§ 2).
2. **One reliable message in flight.** The reliable message is resent while the peer has acked
   something newer than the packet that last carried it but its reliable bit does not yet match
   ours. When the bit matches, the reliable buffer is cleared. A new reliable message is taken
   from the pending buffer only when none is in flight, and taking it flips our reliable bit.
3. **Packet order**: the reliable part first, then as much of the unreliable datagram as fits.
4. **Receiving**: a sequence not newer than the last is dropped (stale or duplicate). The count of
   packets lost is `sequence − (last + 1)`.
5. **Rate**: each channel keeps a clear time. Sending `n` bytes adds `n × copies / rate` seconds;
   a packet may go when the clear time is less than now plus 400 bytes' worth. A packet held back
   counts as choked (§ 6, `svc_chokecount`). Loopback has no limit; while paused the clear time is
   reset to now.
6. **The server's rate** for a client: 0 → 2500; no `rate` key → 99999; otherwise the key,
   capped by `sv_maxrate` (by `sv_maxdownloadrate` and the `drate` key while downloading), then
   clamped to 500..300000. A new channel starts at 2500. The `dupe` key (0..3) sends that many
   extra copies of each packet. The client's own `rate` cvar defaults to 25000.
7. **The server sends to a client only after hearing from it**: a reply goes out when the
   client's newest incoming sequence has reached the server's outgoing sequence, so the server
   answers one packet per client packet (the client's packet rate is the frame rate it sees).

## 4. Messages

Every in-band payload is a run of messages: a byte id, then its body. A reader that meets an
unknown id must drop the connection (the classic programs do: the stream cannot be resynced).
`(R)` marks messages always sent on the reliable channel.

### 4.1 Server to client (svc)

| Id | Name | Body | When sent |
| --- | --- | --- | --- |
| 0 | bad | none | never; reading it is an error |
| 1 | nop | none | keepalive, and a spare byte |
| 2 | disconnect | none (an MVD adds the string "EndOfDemo") | the server drops the client or shuts down |
| 3 | updatestat | byte stat, byte value | a stat (§ 10) changed and fits a byte (R) |
| 6 | sound | § 4.3 | a sound in the client's hearing set |
| 8 | print | byte level, string | text for the console: level 0 low, 1 medium, 2 high, 3 chat; the client's `msg` key filters lower levels (R) |
| 9 | stufftext | string | a console line for the client to run (`configs.md` § What a server may stuff) (R) |
| 10 | setangle | [byte kind], angle × 3 | the server sets the view; the kind byte is present under the MVD high-lag-teleport bit: 0 unknown, 1 teleport, 2 respawn (in an MVD it is the player number) (R) |
| 11 | serverdata | § 5 | signon |
| 12 | lightstyle | byte style, string pattern | signon, and when a style changes (64 styles; letters `a`..`z` are 0..25 of brightness) (R) |
| 14 | updatefrags | byte slot, short frags | a player's frags changed (R) |
| 16 | stopsound | short `entity << 3 | channel` | a looping or long sound is stopped |
| 19 | damage | byte armour, byte blood, coord × 3 from | the client's player took damage this frame |
| 20 | spawnstatic | byte model, byte frame, byte colormap, byte skin, then (coord origin, angle) × 3 | signon: a static entity (no number, never updated) |
| 21 | spawnstatic2 (FTE) | an entity delta from the null state (§ 7) | signon, under the spawnstatic2 bit |
| 22 | spawnbaseline | short entity, then the spawnstatic body | signon: the baseline new entities are deltas from |
| 23 | temp_entity | byte kind, then § 4.4 | an effect |
| 24 | setpause | byte 0/1 | pause toggled (R) |
| 26 | centerprint | string | a message in the middle of the screen (R) |
| 27 | killedmonster | none | single player (R) |
| 28 | foundsecret | none | single player (R) |
| 29 | spawnstaticsound | coord × 3, byte sound, byte volume (0..255), byte attenuation (× 64) | signon: an ambient loop |
| 30 | intermission | coord × 3 origin, angle × 3 | the level ended: the camera goes there (R) |
| 31 | finale | string | the episode ended: text on screen (R) |
| 32 | cdtrack | byte | signon: the music track (ignored by us: no CD audio) |
| 33 | sellscreen | none | the shareware advert (ignored) |
| 34 | smallkick | none | the view kicks a little (a hit) |
| 35 | bigkick | none | the view kicks more |
| 36 | updateping | byte slot, short ms | every couple of seconds, per player (R) |
| 37 | updateentertime | byte slot, float seconds since the player entered | signon and on entering (R) |
| 38 | updatestatlong | byte stat, long value | a stat changed and needs 32 bits (R) |
| 39 | muzzleflash | short entity | a player fired |
| 40 | updateuserinfo | byte slot, long userid, string info | a player's visible info changed; keys starting `_` are not sent (R) |
| 41 | download | § 12 | a download block |
| 42 | playerinfo | § 6 | every frame, per visible player |
| 43 | nails | § 8 | every frame when nails are sent this way |
| 44 | chokecount | byte | how many packets the server held back since the last one |
| 45 | modellist | § 5 | signon |
| 46 | soundlist | § 5 | signon |
| 47 | packetentities | § 7 | every frame, a full list |
| 48 | deltapacketentities | byte from-sequence, § 7 | every frame, a delta list |
| 49 | maxspeed | float | the client's movement maxspeed changed (R) |
| 50 | entgravity | float | the client's gravity scale changed (R) |
| 51 | setinfo | byte slot, string key, string value | one key of a player's info changed (R) |
| 52 | serverinfo | string key, string value | one key of the server info changed (R) |
| 53 | updatepl | byte slot, byte loss % | every couple of seconds (R) |
| 54 | nails2 | § 8 | MVD only: nails with their numbers |
| 55 | soundextended (FTE) | § 4.3 | a sound under the sound-double bit |
| 56 | soundlistshort (FTE) | as soundlist with a short cursor | signon, when the cursor passes 255 |
| 60 | modellistshort (FTE) | as modellist with a short cursor | signon, when the cursor passes 255 |
| 66 | spawnbaseline2 (FTE) | short entity, then a delta from the null state | signon, under the spawnstatic2 bit |
| 78, 79 | updatestatstring, updatestatfloat (FTE) | byte stat, string / float | extended stats; not needed by the classic game |
| 83 | voice (proxy) | a proxy's voice data | never from a server; skip only if a proxy feeds us |
| 84 | voicechat (FTE) | voice data | under the voice bit; not built |
| 94 | spawnstaticsound2 (FTE) | coord × 3, byte flags, short sound, byte volume, byte attenuation | signon, sound index above 255 (verify the field order) |
| 76, 90, 92 | client-side-program packets (FTE) | — | not built (the CSQC bit is never accepted by our client) |
| 100, 101 | simple projectiles (MVD extension) | — | only under the simple-projectile bit; not built |

Ids 4, 5, 7, 13, 15, 17, 18 and 25 are the single-player protocol's and never appear.

### 4.2 Client to server (clc)

| Id | Name | Body | When sent |
| --- | --- | --- | --- |
| 1 | nop | none | spare |
| 3 | move | § 4.5 | every client packet once connected |
| 4 | stringcmd | string (the server cuts it to 1023 bytes) | a console command for the server (§ 5, § 13) |
| 5 | delta | byte sequence | the frame the client holds, so entities come as a delta from it (§ 7) |
| 6 | tmove | coord × 3 | a spectator teleports |
| 7 | upload | short size, byte percent, data | a file the server asked for (rare; we refuse) |
| 50 | ackframe (FTE) | long | not built |
| 81 | qcrequest (FTE) | — | not built |
| 83 | voicechat (FTE) | — | not built |
| 200 | mvd weapon | byte mode bits, then weapon order (server-side weapon switch) | only under that MVD bit; not built |
| 201 | mvd debug | — | not built |

The server drops a client that sends an unknown id. It accepts at most **one** move a packet.

### 4.3 Sounds

1. **Base form (6).** short `packed` = `entity << 3 | channel` (entity in bits 3..12, channel
   in bits 0..2); bit 15 set → a volume byte follows (0..255, default 255); bit 14 set → an
   attenuation byte follows (× 64, default 1.0 → 64). Then byte sound index, coord × 3 origin.
   Order: packed, [volume], [attenuation], sound, origin.
2. **Channel 8 and up** (the reliable flag in the channel argument) means: send reliably and to
   everyone, skipping the hearing test.
3. **Extended form (55)**, under the sound-double bit: byte flags (1 volume, 2 attenuation, 8 the
   entity needs more than 13 bits, 0x10 the sound index needs 16 bits), [byte volume],
   [byte attenuation], then either short packed or (short entity, byte channel) by flag 8, then
   the sound index (short under 0x10, else byte), coord × 3. **verify** the order against a
   capture.
4. **Origin**: a brush entity's sound comes from the centre of its box.
5. **Who hears it**: everyone within 1024 units, and beyond that anyone whose potentially
   hearable set (PHS) holds the source (`sv_phs`, default 1). The `rsnd` userinfo key asks for
   sounds on the reliable channel when `sv_reliable_sound` allows.

### 4.4 Temporary entities (23)

| Kind | Id | Body after the kind byte |
| --- | --- | --- |
| spike, superspike | 0, 1 | coord × 3 |
| gunshot | 2 | byte count, coord × 3 |
| explosion, tarbaby explosion | 3, 4 | coord × 3 |
| lightning 1, 2, 3 | 5, 6, 9 | short entity, coord × 3 start, coord × 3 end |
| wizard spike, knight spike | 7, 8 | coord × 3 |
| lava splash, teleport | 10, 11 | coord × 3 |
| blood | 12 | byte count, coord × 3 |
| lightning blood | 13 | coord × 3 |

Temporary entities go out through the multicast (§ 4.6) by the potentially visible set.

### 4.5 The move (clc 3)

1. Body: byte checksum, byte packet loss in percent, then three usercmds: the first as a delta
   from the null command, the second from the first, the third from the second. The three are
   the commands of the last three client frames (newest last), so one lost packet loses no input
   (`gameloop.md` § The server's frame says how the server uses them).
2. **A delta usercmd**: a bits byte, then the changed fields in this order, then msec:

   | Bit | Field | Wire |
   | --- | --- | --- |
   | 0x01 | pitch | angle16 |
   | 0x80 | yaw | angle16 |
   | 0x02 | roll | angle16 |
   | 0x04 | forward move | short |
   | 0x08 | side move | short |
   | 0x10 | up move | short |
   | 0x20 | buttons | byte (attack 1, jump 2, use 4, attack2 8) |
   | 0x40 | impulse | byte |
   | always | msec | byte |

   The field order is pitch, yaw, roll, forward, side, up, buttons, impulse, msec, whatever the
   bits' numeric order. A field is sent when it differs from the previous command.
3. **The checksum** is § 4.7. A wrong one makes the server ignore the rest of that packet.

### 4.6 Multicast

A message the game program writes for many clients goes to everyone, or by the PHS, or by the
PVS of a point, each reliably or not; plus a demo-only "hidden" target (`demos.md`). Gunshot and
blood particles become temporary entities on the way.

### 4.7 The move checksum

1. Covered bytes: from the loss byte to the end of the third usercmd, at most the first 60.
2. Append four bytes from a fixed 1024-byte key table, starting at offset `s mod 1020`, where
   `s` is the packet's own outgoing sequence as an unsigned 32-bit number: the first key byte
   XOR the sequence's low byte, the second key byte, the third key byte XOR the sequence's second
   byte, the fourth key byte.
3. Take CRC-16 over the result: polynomial 0x1021, initial value 0xFFFF, most significant bit
   first, no reflection, no final XOR (CRC-16/CCITT-FALSE). The checksum byte is the CRC's low
   byte. Test values: the empty input gives 0xFFFF, the ASCII `123456789` gives 0x29B1.
4. **The key table is not in this note** (it is an arbitrary constant, not something the format
   defines in words). **Decision for the lead**: our server may simply not verify the byte (it
   protects nothing a modern threat cares about); our client needs the exact table to be accepted
   by existing servers, so it must be supplied by a decision (treat the table as a wire constant
   and take it, or interoperate only with servers that skip the check). `vectors/checksum.txt`
   holds 72 black-box vectors and a 2048-sequence digest that any table we adopt must reproduce.

## 5. Signon

The client drives the signon with string commands; the server answers each with reliable data
and a stuffed command that asks for the next step. `sc` is the server's spawn count, which
changes with every map; a command carrying a stale count is ignored (the client is being
reconnected).

1. **`new`.** If the client may get protocol extensions, the server first stuffs `cmd pext\n`;
   the client answers `pext <magic> <bits> …` (one pair a family), the server keeps the AND of
   those bits and what it offers, and stuffs `cmd new\n` again. Before the server data, with
   `sv_getrealip` (default 1), the server may stuff `packet <server> "ip <slot> <num>"\ncmd new\n`
   (up to 10 times within 3 s) so the client sends the out-of-band `ip` packet that reveals its
   real address behind a proxy. Then the reply:
   - **serverdata (11)**: for each accepted extension family, u32 magic and u32 bits; then
     long 28; long spawn count; string game directory (default `qw`); byte player number (bit 7
     set for a spectator; an MVD writes a float time here instead); string level name; ten
     floats of movement settings in this order: gravity, stopspeed, maxspeed, spectator
     maxspeed, accelerate, airaccelerate, wateraccelerate, friction, waterfriction, entity
     gravity. (The FTE float-coords bit is dropped unless `sv_bigcoords` is set.)
   - **cdtrack (32)**: byte.
   - **stufftext** `fullserverinfo "<serverinfo>"\n`.
2. **`soundlist <sc> <n>`.** Reply: soundlist (46), byte start index (or 56 with a short when it
   passes 255), then the names of precache slots `n + 1` onward as strings (slot 0 is always empty),
   an empty string, and a byte: where the next request should start (low byte), or 0 when the
   list is done. A chunk is at most half a message and never
   ends on a multiple of 256, so the client's next request is `(count & 0xFF00) + next`. Names are
   paths under `sound/`.
3. **`modellist <sc> <n>`.** The same with 45 / 60. A client without the model-double bit gets
   at most 256 models. Under the ZQuake view-weapon bit, the first chunk is preceded by a
   stuffed `//vwep <names>` line: the view-weapon model names without `progs/` and `.mdl`. Index
   1 is the world (`maps/<name>.bsp`), then the brush submodels `*1`, `*2`, …, then the rest.
4. **Model checks.** The client sends `setinfo pmodel <crc>` and `setinfo emodel <crc>`: CRC-16
   (§ 4.7, step 3) of the whole `progs/player.mdl` and `progs/eyes.mdl` files. The server warns
   when they are not the standard files' values.
5. **`prespawn <sc> <n> [<map checksum>]`.** The first request carries the map's second checksum:
   for every lump except the entities, the four 32-bit words of its MD4 digest XORed together,
   and all of those XORed together; the second checksum also skips the visibility, leaf and node
   lumps. The server compares it when `sv_mapcheck` is 1. The reply walks one index space: the
   static entities (20 or 21), then the baselines (22 or 66), then the signon buffers (static
   sounds, 29 or 94, and the game program's signon writes). Each reply fills about half a message
   and stuffs `cmd prespawn <sc> <next>`; the last stuffs `cmd spawn <sc> 0`.
6. **`spawn <sc> <n>`.** For every slot: updatefrags, updateping, updatepl, updateentertime and
   updateuserinfo (paged with `cmd spawn <sc> <i>` when a message fills). Then the 64 light styles,
   then stats as updatestatlong: total secrets, total monsters, secrets found, monsters killed.
   Then stuffs `skins\n`.
7. **`skins`** (a client command, not sent): the client checks each player's skin, downloads the
   missing ones (§ 12), then sends **`begin <sc>`**. The server places the player in the world
   (the game program's connect and spawn hooks), and sends setpause when paused. The client is
   now active and moves start.

A map change: the server stuffs `changing\n` then `reconnect\n` (during a download it waits);
`reconnect` makes the client send `new` again.

## 6. Players (svc 42)

1. Body: byte slot, short flags (under the FTE transparency bit, when flag bit 15 is set, an
   extra byte follows that extends the flags to 24 bits), coord × 3 origin (floats under the MVD
   float-coords bit), byte frame, then the optional fields below in this order.

   | Flag | Bit | Field |
   | --- | --- | --- |
   | msec | 0x1 | byte: ms since that player's last command, `1000 × (now − last)`, capped at 255 |
   | command | 0x2 | a delta usercmd from the null command (buttons sent as 0; impulse carries the view weapon under the ZQuake bit); a dead player's pitch is 0 and yaw the model's |
   | velocity x, y, z | 0x4, 0x8, 0x10 | short each (units a second, not quantized) |
   | model | 0x20 | byte: sent when the model is not the player model |
   | skin number | 0x40 | byte (bit 7 adds 256 to the model index) |
   | effects | 0x80 | byte |
   | weapon frame | 0x100 | byte: only to the player himself or a spectator tracking him |
   | dead | 0x200 | none |
   | gib | 0x400 | none (the player's box is not the standing one) |
   | move type | bits 11..13 | 0 normal, 1 normal with jump held, 2 old spectator, 3 spectator, 4 fly, 5 none, 6 lock |
   | on ground, solid | bits 14, 15 (bits 22, 23 in the extended form) | none: valid under the ZQuake on-ground and solid bits |
   | transparency | bit 17, extended form only | byte alpha × 254, 1..254 |

2. The player's own entry carries no msec and no command (he predicts himself), except under the
   lock move type, when the command carries his view angles. A spectator gets only the velocity
   for the player he tracks. **verify** the exact rule for the extended flag word against a
   capture.
3. **Who is sent**: players whose origin is in the client's PVS; spectators are never sent.
   Game-program fields can hide players and entities from a client.
4. Velocity is sent only when non-zero; a missing component reads as 0. Origin, frame and
   (where present) msec are always current.

## 7. Packet entities (svc 47 and 48)

1. Each frame the server builds the list of visible non-player entities with a model, sorted by
   number, at most 64 (256 under the extension, 300 in an MVD); over the limit it keeps the nearest.
2. **Full or delta.** If the client's last clc_delta names a frame the server still holds (a
   ring of 64, and not the slot being written now) the server writes 48 with that sequence's low
   byte, and each entity as a delta from that frame; otherwise 47 and each entity as a delta from
   its baseline. A client asks for a delta when it holds a valid frame no more than 63 behind and
   `cl_nodelta` is 0; on a mismatch of the from-sequence it drops its valid frame and asks for a
   full update.
3. **A record**: a u16 word with the entity number in bits 0..8 and flags in bits 9..15:
   origin x 1<<9, origin y 1<<10, origin z 1<<11, angle y 1<<12, frame 1<<13, remove 1<<14,
   more bits 1<<15. With "more bits", a byte: angle x 1, angle z 2, model 4, colormap 8, skin
   0x10, effects 0x20, solid 0x40 (no field), even more 0x80. With "even more" (FTE), a byte:
   transparency 2, model-double 8, entity +512 0x20, entity +1024 0x40, yet more 0x80; with "yet
   more", a byte whose bit 2 (1<<10 of the combined flags) is the colour modulation.
4. **Field order**: model (byte; a short under model-double when the model bit is not also set:
   **verify**), frame, colormap, skin, effects (bytes), origin x, angle x, origin y, angle y,
   origin z, angle z, transparency (byte), colour modulation (3 bytes).
5. **Removal**: the word with the remove bit (and for an entity above 511, more bits, even more and
   the number's high bits). The list ends with a u16 0.
6. An entity in the from-frame but not in this list is removed; a new one comes as a delta from
   its baseline. The modern server may defer a removal or an entity that does not fit to a later
   frame.

## 8. Nails

1. `sv_nailhack` (default 1) sends nails as ordinary entities. With it off, svc 43: byte count,
   then 6 bytes a nail packing x, y, z as 12 bits each (`(int(o + 4096 + 1) >> 1) & 4095`, so 2
   units of precision), a 4-bit pitch and an 8-bit yaw.
2. svc 54 (MVD only) is the same with a leading byte per nail: its number, so a player can follow
   it between frames.
3. Nails are always entities when coordinates are floats.

## 9. Info strings

1. **Shape**: `\key\value\key\value…`. No key or value may contain `\` or `"`; keys are
   case-sensitive; at most 64 bytes a key. Server info is limited to 512 bytes, a client's user
   info to 1024 (the extended limit), at most 128 keys a client.
2. **Star keys** (`*name`) are set only by the server: `*spectator`, `*z_ext`, `*client`, `*VIP`,
   `*version`, `*admin`… A client's `setinfo` on a star key is refused.
3. **`setinfo <key> <value>`** (a string command): refused for star keys, for backslashes, `&c` or
   `&r`; the game program may veto. The server then sends svc 51 to everyone for the keys others
   see: `name`, `team`, `skin`, `topcolor`, `bottomcolor`, `gender`, `*auth`, `*flag`, `*skill`.
4. **Names**: any of `\`, CR, LF, `$`, `#`, `"`, `;` gives the default name (`sv_default_name`);
   leading and trailing spaces are trimmed; "console" is refused; a duplicate becomes `(N)name`
   cut to 10 characters; more than 4 changes in 5 s kicks; more than 5 top-colour changes in 8 s
   kicks.
5. **Client keys** with their defaults: `name` "player", `rate` 25000, `msg` 1, `noaim` 1, plus
   `team`, `topcolor`, `bottomcolor`, `skin`, `spectator`, `password`, `mtu`, `w_switch`,
   `b_switch`, `railcolor`, `gender`. Server-read keys: `rate`, `drate`, `dupe`, `msg`, `sp`,
   `rsnd`, `mtu`.
6. **Server info** carries `*version`, `*z_ext` (511), `maxclients`, `maxspectators`, `deathmatch`,
   `teamplay`, `timelimit`, `fraglimit`, `maxfps`, `pm_*` movement switches (`movement.md`),
   `watervis`, `maxpitch`/`minpitch` (ZQuake pitch limits), `sv_antilag`… Changes go out as
   svc 52. The client asks again with the `serverinfo` command.

## 10. Stats

| Stat | Id | Notes |
| --- | --- | --- |
| health | 0 | |
| weapon model | 2 | the view model's index |
| ammo | 3 | of the current weapon |
| armour | 4 | |
| shells, nails, rockets, cells | 6, 7, 8, 9 | |
| active weapon | 10 | an item bit |
| total secrets, total monsters, secrets, monsters | 11, 12, 13, 14 | |
| items | 15 | item bits, with the server flags in bits 28.. and the rune bits from 23 |
| view height | 16 | under the ZQuake view-height bit |
| time | 17 | server time in ms, every 5 s, under the ZQuake server-time bit |

At most 32 stats. A stat goes out as 3 when it fits a byte (0..255), else 38; only when the
reliable channel can take it this frame.

## 11. Extensions (first tier)

1. **ZQuake** (the `*z_ext` keys, no handshake beyond the userinfo and the server info): pm type
   1, pm type new 2 (fly and spectator move types), view height 4, server time 8, pitch limits
   0x10, join/observe commands 0x20, on-ground valid 0x40, view weapons 0x80, solid valid 0x100.
   All nine (511) are offered and used.
2. **FTE** (`FTEX`): transparency 0x8, sound-double 0x80, model-double 0x1000, entity-double 0x2000,
   entity-double2 0x4000, float coords 0x8000, colour modulation 0x80000, spawnstatic2 0x400000,
   256 packet entities 0x1000000, chunked downloads 0x20000000, client-side programs 0x40000000.
   The modern server offers exactly these (sum 0x6148F088); half-life maps 0x200 and
   DarkPlaces flags 0x80000000 exist but are not offered; accurate timings 0x40 is offered only
   by clients. `FTE2`: voice chat 0x2.
3. **MVD** (`MVD1`): float coords 1; high-lag teleport 2 (only when the game program has a
   `teleported` field); server-side weapon 4 and 0x40 (only with a mod that supports it);
   debug weapon 8 and debug antilag 0x10 (by cvars); hidden messages 0x20 (demos). Clients also
   know weapon prediction 0x80 and simple projectiles 0x100.
4. **Our first tier** (M18g): the ZQuake set; FTE transparency, model-double, entity-double(2),
   256 packet entities, spawnstatic2, chunked downloads and sound-double; MVD float coords and
   high-lag teleport. Everything else is refused in the `pext` answer. **verify** with
   `se-interop` that the modern server and client behave with that subset.

## 12. Downloads

1. The client sends `download <path>` (backslashes become slashes). The server refuses: a leading
   `/` or `.`, any `..` component, `.log` files, and anything not in a subdirectory. Switches, all
   default 1: `allow_download_skins`, `_models` (progs/), `_sounds`, `_maps` (and maps in paks),
   `_demos`; `allow_download_other` defaults to 0. Names are lowercased.
2. **Classic transfer** (svc 41): short size of this block (−1 = refused, followed by a byte 0),
   byte percent done, the data. The client answers `nextdl` for each block. Block size comes from
   the rate (at least 10, at most 1350 bytes, verify the formula).
3. **Chunked** (FTE bit): the start is svc 41 with long −1, long total size (−1 not found, −2
   refused, −3 cancelled) and the name. Then chunks: long chunk number, 1024 bytes. The client
   asks with unreliable `nextdl <chunk> <percent> <download number>` for up to
   `cl_chunksperframe` chunks a frame (1..30); the server sends at most
   `sv_downloadchunksperframe` (default 30) a frame, the first inside the datagram and the rest as
   out-of-band packets whose text is `\chunk`, a long download number, then the svc 41 body.
   The client ends with `nextdl -1 <percent> <num>` or `stopdownload`.
4. A finished file is written under a temporary name and renamed. Skins are fetched during
   signon (§ 5, step 7), maps and models when the lists name a missing one. The client refuses
   downloads by extension (`configs.md`, `cl_allow_downloads`).

## 13. String commands the server knows

`new`, `modellist`, `soundlist`, `prespawn`, `spawn`, `begin`, `drop`, `pings`, `rate`*, `kill`*,
`pause`*, `say`*, `say_team`*, `setinfo`, `serverinfo`, `download`, `nextdl`, `dl`, `ptrack`
(spectator tracking), `upload`, `snap`, `stopdownload`/`stopdl`, `dlist…`, `demoinfo`,
`lastscores`, `minping`*, `airstep`*, `maps`*, `ban`*…, `join`*, `observe`*, `god`*/`give`*/
`noclip`*/`fly`*, `pext`, `techlogin`, the voice commands. A starred command can be overridden:
the game program's command hook sees it first. Anything unknown goes to the game program, else
the client gets "Bad user command". Output comes back as svc_print level 2.

The client's `cmd <text>` sends the text verbatim as a string command; console commands the
client registers without a function (`kill`, `god`, `give`, `noclip`, `fly`) and any client
command it does not know are forwarded the same way (`configs.md` § The command buffer).
`say` and `say_team` are expanded on the client before sending (`configs.md` § Team messaging)
and always sent wrapped in quotes.

## 14. Lag compensation

1. `sv_antilag`: 0 off, 1 on, 2 also rewinds traces made by the game program, and with
   `sv_antilag_projectiles` missiles. The default is the empty string in the cvar (off):
   **verify** what a fresh server reports in its info. `sv_antilag_no_pred` turns off the
   client-side hint.
2. Each player keeps a ring of 128 (time, origin) samples, recorded after each of his commands.
3. **The target time** for a shooter is the server time of the frame his last packet
   acknowledged, plus 0.02 s (or one client frame, `1 / maxfps`, when his ping is under 20 ms;
   a `maxfps` outside 20..1000 counts as 77), capped at the current time.
4. **Each other live, spawned player** is placed at the target time: later than his last command
   → extrapolate from the last sample by velocity × min(gap, 0.02); one sample → that sample;
   otherwise interpolate between the samples on either side.
5. A trace marked lagged (move flag 64, set when the shooter or the shooter's owner is a player)
   skips the live player boxes and tests the rewound ones. Bots are never rewound.

## 15. Small messages, summarized

- **Prints and centre prints**: § 4.1; chat (level 3) is what message triggers and the chat
  filters read on the client.
- **Light styles**: 64 strings; `a` is dark, `m` normal, `z` double; the client steps through a
  style's letters 10 times a second.
- **Intermission and finale**: both freeze the player; the client shows scores (intermission) or
  the text (finale) and stops predicting.
- **`stufftext`**: the client runs it from its server command buffer under the rules in
  `configs.md` § What a server may stuff.

## 16. What is ours, not theirs

1. Our server's clock is the run clock (`gameloop.md`), so a reply's timing follows our frame, not
   a wall clock; the wire format does not change.
2. Our client never offers or accepts client-side programs, voice, or the server-side weapon
   extension.
3. The checksum key table is a decision (§ 4.7).
