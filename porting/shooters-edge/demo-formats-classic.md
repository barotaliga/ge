# Answers: the classic multiplayer game's client demos (qwz, protocol 26, damage, extensions)

Reader's document for the clean room. Facts only, in the reader's words. Each fact is marked
**[read]** (stated by a source the reader read), **[measured]** (the reader ran a program or
inspected corpus bytes) or **[derived]** (worked out from read or measured facts).

## Sources read and their licences

| Source | Release read | Licence |
| --- | --- | --- |
| github.com/osm/quake (Go building blocks; its compressed-demo codec, the proxy's message model, its demo readers) | master at commit ed26296 (2026) | MIT for the repository's own code; it also embeds the proxy's model file and the proxy's built-in string and name tables, which are the proxy authors' data, not MIT |
| github.com/osm/qizmo (the proxy's original 2.91 Linux and Windows release, untouched, plus patches) | master at d273bbd | the original program's terms ("For non-commercial use only. No warranty", copyright 1998 by its two authors); the repository grants nothing more |
| github.com/osm/qizmo-unpack (unpacks the protected Windows executables) | master | no licence file; read only its README (it is not a demo decoder) |
| id Software's released Quake source (its QuakeWorld tree) | github.com/id-Software/Quake, master | GPL v2 |
| ezQuake client | ezquake-source master at c633fae (Nov 2026) | GPL v2 |
| FTE (fteqw) | master at f09ac7c (Nov 2026) | GPL v2 |
| mvdsv server | master at 4e63928 (Nov 2026) | GPL v2 |
| QW-Group qwprot (the protocol header ezQuake and mvdsv share) | master at a5fce41 | no licence file; part of the GPL v2 projects above |

**Black-box runs [measured]:** the original proxy program, Linux build 2.91 (32-bit ELF,
SHA-256 `db4e91bb…e8ef`, run under the 32-bit C library), with its own model file, used only
with its documented `-C` (compress) and `-D` (decompress) switches. The reconstruction's
decoder and encoder were built and run in the scratch clone.

**Headline [measured]:** the reconstruction's decoder output is byte-identical to the
original program's `-D` output on 15 of the 16 corpus compressed demos (all 12 in `qwz/`
except one, the 2 mis-named `.qwd` files, and the 4 in `same_match/`). The sixteenth is
discussed under question 4. So the format below is the format the original program reads.

---

## A. The compressed proxy demo (.qwz)

### 1. File layout

1. **There is no header, block structure, length, checksum or trailer.** The whole file is
   one arithmetic-coded stream from its first byte to its last **[read, measured]**. The first
   four bytes are simply the coder's initial 32-bit code value (big-endian) **[read]**.
2. **The coded content is a sequence of records**, mirroring the client demo's own records
   (time, type, body), ended by a fourth record kind, "end of stream" **[read]**. For each
   record the decoder first decodes the record kind with a fixed four-symbol model (question 2,
   § Outer records), then the record's body.
3. **The end:** the stream ends with the end-of-stream record kind (symbol 3). After that
   symbol the encoder flushes its state (at most one or two bytes, question 5) and writes
   nothing more **[read]**. Reading past the last byte supplies zero bytes to the decoder
   **[read]**. Measured on 15 corpus files: the decoder meets the end symbol after consuming
   exactly the file's length, or one byte more than it (the flush can be one byte shorter
   than the decoder's look-ahead) **[measured]**.
4. **Why the first byte is always 0x80 [derived, measured]:** the first record of every demo
   is a packet record (kind 1), whose fixed interval starts at exactly 2^31 of the 2^32 code
   space, and nothing coded before it. So the coder's low bound becomes 0x8000_0000 and its
   top byte, 0x80, is settled at once. The bytes after it depend on what follows (question 6).

### 2. The coding algorithm

#### 2.1 The coder (a 32-bit range/arithmetic coder with static models) [read, verified by the bit-exact match above]

- **State:** three unsigned 32-bit values, `low`, `high` and `code`. Start: `low` = 0,
  `high` = 0xFFFF_FFFF, `code` = the file's first four bytes, big-endian. All arithmetic on
  the state is modulo 2^32.
- **A model** for an alphabet of N symbols (N = 2, 4, 256 or 512) is a non-decreasing list of
  N cumulative bounds C[0..N−1], each an unsigned 32-bit number, with the last bound the
  top of the scale. Symbol s owns the scaled interval from C[s−1] (0 for s = 0) up to C[s].
- **Renormalise before each symbol** (not after): as long as `high` − `low` is at most
  0x00FF_FFFF, shift all three left by 8 bits: `high` takes 0xFF into its low byte, `low`
  takes 0x00, `code` takes the next file byte (0 past the end).
- **Decode one symbol:** let R = `high` − `low` (no "+1"). Let V = floor(((`code` − `low`)
  × 2^32 + (2^32 − 1)) / R), computed in 64 bits and kept to 32. The symbol is the smallest s
  with C[s] > V, or N − 1 if no bound exceeds V. (The reference finds it by a binary search
  whose steps are N/2, N/4, …, 1, so it can never return more than N − 1.)
- **Narrow the interval** (using the old `low` for both ends): new `high` = `low` +
  floor(C[s] × R / 2^32) − 1; then, only for s > 0, new `low` = old `low` +
  floor(C[s−1] × R / 2^32). Products are 64-bit, results kept to 32 bits.
- **Two read modes [read]:** the symbols of the outer records, the user-command records, the
  time, the packet mode, raw packet bytes and the sequence record treat reading past the end
  as zeros. The symbols inside a compressed packet's messages are "strict": a
  renormalisation that needs a byte past the end of the file is an end-of-stream error
  there. A well-formed file never hits either.
- **No adaptation [read, measured]:** every model is fixed for the whole file. Nothing is
  updated by the data; the "context" is entirely *which* model is used for the next symbol,
  chosen by the message being decoded and the field within it (question 2.3), plus
  deterministic predictions of field values from earlier packets (deltas, XORs, second-order
  predictors). This is why different demos share long identical prefixes (proxy.md (c)).

#### 2.2 Building the models from the model file [read; derived row numbers]

- The model file is 256 rows × 256 little-endian unsigned 32-bit counts (question 3). Row r
  starts at byte r × 1024. Every count in the file is at least 1 **[measured]**.
- **An ordinary row** (256 symbols): with T = the row's total count, C[i] =
  floor((c[0] + … + c[i]) × (2^32 − 1) / T), sums in 64 bits. So C[255] = 0xFFFF_FFFF.
- **Paired rows** (512 symbols, for strings with dictionary words): rows 78+79, 80+81, 82+83,
  105+106 and 124+125 are one 512-symbol model, built as above over all 512 counts (row r
  gives symbols 0..255, row r+1 symbols 256..511).
- **Row 132** (a two-symbol flag): C[0] = floor(c[0] × (2^32 − 1) / (c[0] + c[1])), C[1] =
  0xFFFF_FFFF; the rest of the row is unused.
- **One fixed adjustment:** before building row 0, its symbol-1 count is increased by 256.
- Rows 0..131 and 180..255 are built (rows 133..179 hold the proxy's link coder tables and are
  not used by demos). The encoder must build the models identically, since the cumulative
  values, not the counts, define the code.

**The outer record model** is fixed, not from the file. Its cumulative bounds are
0x8000_0000, 0xFFFF_FC00, 0xFFFF_FE00, 0xFFFF_FFFF **[read]**. Hence the probabilities: user
command (0) one half; packet (1) one half less 2^−22; sequence record (2) 2^−23; end (3)
just under 2^−23 **[derived]**.

**Model rows the demo coder uses [derived: each row's index was computed from the
reconstruction's placement of the models, and every row below is exercised by the bit-exact
decodes].** "Lo/hi" pairs code one 16-bit delta as two bytes (question 2.4).

| Row | Codes | Row | Codes |
| --- | --- | --- | --- |
| 0 | back-reference distance of a compressed packet | 63, 64 | updatestat index, value |
| 1, 2, 3 | primary player: origin-mask, state-mask, motion-mask XOR | 65, 66 | sound channel word XOR, lo, hi |
| 4 | message opcode inside a compressed packet | 67, 68 | sound volume, attenuation |
| 5 | additional player: slot delta (0 ends the list) | 69 | sound index (remapped) |
| 6 | additional player: msec byte | 70 | sound/beam coordinate-flags byte |
| 7, 8, 9, 10 | additional player: origin, angle-move, state, motion mask XORs | 71..76 | coordinate deltas x lo, x hi, y lo, y hi, z lo, z hi (sound, beam start) |
| 11..16 | player origin deltas x lo, x hi, y lo, y hi, z lo, z hi | 77 | print level |
| 17 | player frame delta | 78+79 | print text (paired) |
| 18, 19 | player command yaw: second-order delta lo, hi | 80+81 | chat print text (paired) |
| 20, 21 | player command pitch: second-order delta lo, hi | 82+83 | stufftext text (paired) |
| 22, 23 | player command forward move delta lo, hi | 84, 85 | updatefrags value lo, hi |
| 24, 25 | player command side move delta lo, hi | 86, 87 | damage armour, blood |
| 26, 27 | player command up move delta lo, hi | 88..93 | damage source deltas x lo, x hi, y lo, y hi, z lo, z hi |
| 28 | player buttons XOR | 94 | temp entity type |
| 29 | player impulse (set) | 95, 96 | entity number XOR lo, hi (beams, muzzle flash) |
| 30 | player command msec delta | 97 | beam flags |
| 31..36 | player velocity second-order deltas x lo, x hi, y lo, y hi, z lo, z hi | 98..103 | temp entity position deltas x lo, x hi, y lo, y hi, z lo, z hi |
| 37 | player model (remapped) | 104 | gunshot/blood count |
| 38 | player skin (set) | 105+106 | centerprint text (paired) |
| 39 | player effects XOR | 107, 108 | updateping lo, hi |
| 40 | player weapon frame delta | 109..113 | updatestatlong index, bytes 0..3 |
| 41 | packet entity symbol (also the long-run extension byte) | 114, 115 | updateuserinfo user-id bytes, string bytes |
| 42 | packet entity number extension byte | 116..122 | nails count, then the six projectile byte deltas |
| 43, 44 | entity mask high byte, low byte | 123 | chokecount |
| 45 | entity model (remapped) | 124+125 | setinfo key (paired) |
| 46, 47, 48 | entity frame, colormap, skin deltas | 126 | setinfo value bytes |
| 49 | entity effects XOR | 127 | serverinfo strings |
| 50, 51 | entity origin x delta lo, hi | 128 | updatepl byte |
| 52 | entity angle 1 delta | 129 | player slot (setinfo, updateuserinfo, updatefrags) |
| 53, 54 | entity origin y delta lo, hi | 130 | player slot delta (updateping, updatepl) |
| 55 | entity angle 2 delta | 131 | plain byte (raw packets, sequence record, generic bytes) |
| 56, 57 | entity origin z delta lo, hi | 132 | damage "has a source" flag (2 symbols) |
| 58 | entity angle 3 delta | 180..213 | the proxy's voice message, one row per byte position (34 rows) |
| 224, 225 | user command mask lo, hi | 226..237 | user command deltas: pitch lo, hi, yaw lo, hi, roll lo, hi, forward lo, hi, side lo, hi, up lo, hi |
| 238 | user command buttons XOR | 239 | user command impulse (set) |
| 240 | user command msec delta | 241 | record time delta-of-delta |
| 242 | packet mode | | |

#### 2.3 What is decoded, in order [read; verified by bit-exact output]

The decoder keeps a running state across the file: the current time in whole milliseconds and
its last increment (both start at 0), the user-command predictor (2.5), the running packet
sequence number, a 64-slot ring of "command msec by sequence", 32-slot rings of the player
list and the entity list of earlier packets (keyed by sequence), entity baselines, player
names, the recorded player's slot, the player model's index, the model and sound remaps
(2.8), and the built-in dictionaries (2.7).

**Outer records.** Repeat: decode the record kind (outer model).

- **3 (end):** stop. Nothing is written.
- **Otherwise** write the qwd record header: the current time as a 32-bit float equal to
  (milliseconds / 1000) computed in double precision and rounded to float, then the kind byte.
  Then the body:
- **0, user command:** decode the command (2.5) and write its 36 bytes. Record its msec in the
  ring: the "command sequence" counter is incremented and its slot (counter mod 64) gets this
  msec. Then decode one symbol from row 241 as an unsigned byte d: the increment becomes
  (increment + d) mod 256 and the time advances by the increment. **The time changes only
  here**, so a record's time is the time coded after the last user command before it.
- **2, sequence record:** eight bytes, each from row 131, written as they are. The first four,
  little-endian, set the command sequence counter. Then the remaps are rebuilt (2.8).
- **1, packet:** decode the packet mode m from row 242.
  - **m = 0, raw packet:** length = two bytes from row 131 (low, then high; at most 65,535),
  then that many bytes from row 131. Written as the qwd length (32-bit) and the bytes,
  unchanged. If it is at least 8 bytes and its first word is not 0xFFFF_FFFF, its first word
  becomes the running sequence, and the packet is parsed as ordinary server messages to
  update the state (2.9).
  - **m ≥ 1, compressed packet:** the running sequence advances by m (modulo 2^31). Decode
  the packet (2.6) and write its length and bytes. If m > 1 and the packet holds nothing
  but the recorded player (no further message), the decoder discards it and **ends the
  output there**: this shape is the coder's end-of-file marker for a dropped tail.

#### 2.4 Conventions used by every field [read]

- **A 16-bit delta as lo/hi bytes:** two mask bits gate two bytes. The low byte, when present,
  is a signed byte added to the value; the high byte, when present, is added as (byte × 256).
  Either may be absent (contributing 0). Sum modulo 2^16. The encoder splits a delta D as
  lo = D's low byte, hi = (D − lo read as signed) / 256 **[read]**.
- **Byte deltas** add a signed byte (modulo 256). **XOR fields** XOR the byte into the previous
  value. **"Set" fields** replace the value.
- **Second-order fields** (player velocity, player command yaw and pitch, entity origins and
  angles of existing entities, user-command angles) keep a hidden per-field rate: the coded
  delta changes the rate, and the rate is added to the value. A rate is kept with the record
  it belongs to.
- **Masks are predicted:** each player and entity record carries its last mask bytes, and the
  coded mask byte is XORed into them, so "the same fields change as last time" costs little.
  After use, the bits meaning "set this once" are cleared before the mask is stored (listed
  per mask below).
- Coordinates are the wire's 16-bit eighths of a unit; angles bytes or shorts as on the wire.

#### 2.5 The user command record [read; behaviour measured]

- Mask: two bytes (rows 224, 225) form a 16-bit XOR. The new mask = (previous mask AND
  0x203F) XOR (lo + 256 × hi). Bits: 0x0001/0x0002 pitch lo/hi, 0x0004/0x0008 yaw lo/hi,
  0x0010/0x0020 roll lo/hi, 0x0040/0x0080 forward lo/hi, 0x0100/0x0200 side lo/hi,
  0x0400/0x0800 up lo/hi, 0x1000 buttons, 0x2000 msec, 0x4000 impulse (0x8000 unused).
  Previous mask starts 0.
- Fields in order: pitch, yaw, roll (each second-order: rate += delta, angle += rate, 16-bit
  angle units of 360/65536 degree), forward, side, up (first order, 16-bit), buttons (XOR,
  row 238), msec (signed byte delta, row 240), impulse (set, row 239). Rows per 2.2.
- Start state: everything 0 except **impulse = 8**.
- Written 36 bytes: msec; three zero bytes; the three angles as 32-bit floats, each the
  signed 16-bit angle × 360 / 65536 (so in −180 to just under +180 degrees); forward, side,
  up as signed 16-bit; buttons; impulse; **the same three angle floats again**.

#### 2.6 A compressed packet [read]

Written: the sequence (32-bit, high bit clear), the sequence **again** in the acknowledge
word, then opcode 42 and the recorded player's playerinfo, then the remaining messages.

**The recorded ("primary") player.**
1. Back-reference distance r (row 0). If r > 0 the packet's base is the packet with sequence
   (current − r): its entity list must be in history (else the packet is dropped, see
   below), its player list must be in history and the command-msec ring slot of that
   sequence must hold that sequence. The primary base record is the recorded player's record
   in that list, or the default record if absent. If r = 0 there is no base and the base
   record is the default record (slot = recorded player, model = player model index, all
   else 0). Prediction scale S = (command msec stored for the base sequence) × r.
2. Three bytes (rows 1, 2, 3) XORed into the base's origin mask, state mask, motion mask.
3. Origin = base origin + V × S / 125 per axis, where V is the base's velocity; the division
   rounds half away from zero (|V| × S + 62, integer-divided by 125, sign restored). Then
   origin deltas gated by origin-mask bits 0..5 (x lo, x hi, y lo, y hi, z lo, z hi; rows
   11..16), and a frame delta (row 17) if origin-mask bit 6. Bit 6 is then cleared.
4. State: model by remap (row 37) if state bit 5; skin set (row 38) if bit 6; effects XOR
   (row 39) if bit 7. Bits 3, 5 and 6 are then cleared from the stored state mask.
5. Motion: per axis, velocity rate += delta (bits 0..5, rows 31..36), velocity += rate; weapon
   frame delta (row 40) if bit 6; bit 7 means dead. Bit 6 is then cleared.
6. Written as playerinfo body: slot; flags (2 bytes) recomputed from the values: velocity
   bits for non-zero components, model bit when model ≠ player model index, skin and
   effects bits when non-zero, weapon-frame bit when non-zero, dead bit from motion bit 7; never
   the msec or command bits. Then origin, frame and the present fields in wire order.

After the primary player: "last coordinates" = primary origin; the base packet's player and
entity lists become the packet's base lists; the packet scale P = (command msec stored for
the base sequence) × (current − base sequence), or 0 without a base.

**The message loop.** Decode an opcode from row 4. Opcode 0 ends the packet. Otherwise write
it and decode its body:

- **42, more players** (a list; may occur more than once): repeat: slot delta (row 5); 0 ends
  the list. Slot = previous slot + delta (mod 256; the previous slot starts at 255 for each
  packet and carries over between lists). Slot ≥ 32 drops the packet. Base = this slot's
  record in the base player list, else a new record with the primary's origin, the player
  model index and this slot. Then the msec byte (row 6): **bit 7 set** means "unchanged": the
  record is the base record with msec = the low 7 bits, nothing else is read. Otherwise:
  origin predicted as for the primary with scale = the smallest multiple k × c (k ≥ 1) of the
  base's command msec c that is at least (msec + P − base msec − c), or 0 when c = 0; then
  four mask XOR bytes (rows 7..10: origin, angle-move, state, motion); origin and frame as for
  the primary; angle-move mask: bits 0/1 yaw rate lo/hi (rows 18, 19), bits 2/3 pitch rate
  lo/hi (rows 20, 21), each second-order onto the command angle; bits 4/5 forward (22, 23),
  bits 6/7 side (24, 25); state mask: bits 0/1 up move lo/hi (26, 27), bit 2 buttons XOR (28),
  bit 3 impulse set (29), bit 4 command msec byte delta (30), bit 5 model by remap (37), bit 6
  skin set (38), bit 7 effects XOR (39); motion mask as for the primary. Written as one
  playerinfo per player (the first right after the opcode, each later one with its own 42):
  slot, flags, origin, frame, msec, then the command block, velocities and bytes. Flags:
  msec bit always; command bit when the command msec or any of pitch, yaw, forward, side, up,
  impulse is non-zero; the command's own flags mark the non-zero ones of those six (buttons
  and the third angle are **never** written, though buttons are tracked); then velocity,
  model, skin, effects, weapon frame and dead as for the primary.
- **47 or 48, packet entities:** decoded the same way and **always written as 47** (a full
  update), 48's base byte not written (2.6.1).
- **6 sound:** channel word = (last entity × 8) XOR (lo + 256 × hi) (rows 65, 66); entity =
  (channel / 8) mod 512 becomes "last entity"; volume (row 67) if channel bit 15; attenuation
  (row 68) if bit 14; sound by remap (row 69); a flags byte (row 70). Start point = last
  coordinates; but if flags bit 7 is clear and the entity is not 0: for entity 1..32 (and only
  when the packet has a base), that player's origin from this packet's players (or from the
  base players if flags bit 6); for entity > 32, that entity's origin in this packet's
  entity list (if already decoded in this packet), else its baseline origin when the search
  passed its number and the baseline origin is not all zero. Then coordinate deltas gated by
  flags bits 0..5 (rows 71..76). The result becomes last coordinates.
- **23 temp entity:** type (row 94); start point = last coordinates. Gunshot (2) and blood
  (12): a count byte (row 104). Lightning 1, 2, 3 (types 5, 6, 9): flags (row 97); if flags
  bit 6, last entity ^= lo + 256 × hi (rows 95, 96); the entity is written; its start point is
  resolved as for sound (players only from this packet's players; entities from the base
  entity list), deltas by flags bits 0..5 (rows 71..76), written. Then for every type, all
  six position delta bytes (rows 98..103) are read and applied to the current point, which is
  written and becomes last coordinates. (For lightning the end is coded relative to the
  start.)
- **39 muzzle flash:** entity = last entity XOR (lo + 256 × hi) (rows 95, 96).
- **19 damage:** armour (86), blood (87), flag (row 132): 0 writes six zero bytes; 1 reads all
  six deltas (88..93) against the primary player's origin.
- **43 nails:** count (116); for each projectile its six packed bytes are byte deltas (rows
  117..122) from the previous projectile; the first is predicted from the primary origin
  packed like a projectile: byte 0 = x bits 4..11; byte 1 = y bits 4..7 in its high nibble
  and x bits 12..15 in its low nibble; byte 2 = y bits 8..15; byte 3 = z bits 4..11; byte 4 =
  z bits 12..15 sign-extended into a byte; byte 5 = 0.
- **8 print:** level (77); then symbols of the paired model (78+79, or 80+81 for level 3,
  chat) until symbol 0, which is written as the terminator. Symbols 0..255 are literal bytes.
  For chat: 256..287 write "(" name "): ", 288..319 the name alone, 320..511 a dictionary
  entry. Otherwise 256..287 the name, 288..511 a dictionary entry. The name is player
  (symbol mod 32)'s current name, "unnamed" when none is known.
- **9 stufftext** (82+83), **26 centerprint** (105+106): literals and dictionary entries
  256..511, until 0.
- **51 setinfo:** slot (129), key (paired 124+125 with dictionary), value bytes (126) to 0.
  **40 updateuserinfo:** slot (129), four user-id bytes (114), string bytes (115) to 0.
  **12 lightstyle:** one byte and a string (131). **52 serverinfo:** two strings (127).
- **36 updateping:** slot = previous + delta (row 130; previous starts at 255 per packet),
  lo (107), hi (108). **53 updatepl:** slot by its own delta chain (130), one byte (128).
- **Fixed bodies:** 1, 2, 27, 28, 33, 34, 35 none; 3 updatestat index (63) value (64); 14
  updatefrags slot (129), lo (84), hi (85); 38 updatestatlong index (109) and four bytes
  (110..113); 29 spawnstaticsound and 30 intermission nine bytes, 49 maxspeed and 50
  entgravity four bytes, 10 setangle three, 16 stopsound two, 24 setpause, 32 cdtrack one,
  37 updateentertime five, all from row 131; 44 chokecount one byte (123).
- **The proxy's private messages:** 81 a text string (row 131 bytes to 0); 82 a 162-byte block
  (row 131); 83 a 34-byte voice frame, byte i from row 180 + i.
- Any other opcode is an error (the format cannot code it).
- **A dropped packet** (slot ≥ 32, a missing base list, a copy past the base): the original
  program's behaviour differs from the reconstruction's (question 4, the odd file).

**2.6.1 Packet entities [read].** Base list = the base packet's entity list (in number order),
or empty. Current number starts at 32. Repeat: symbol (row 41).

- **0:** end; the remaining base entities are kept unchanged.
- **Bit 7 set, a new entity:** n = symbol bits 0..5. If n = 0: number += 64, then extension
  bytes (row 42): each 255 adds 255 and another is read; any other value is added and ends.
  Else number += n. Base entities below the number are kept; one equal to it is an error. The
  record starts as the number's baseline (or all zero), rates cleared. If symbol bit 6: a mask
  high byte (row 43, as is, not XOR); if its bit 6 (the mask's bit 14) is set, a low byte (row
  44) whose bits 0..7 select, in order, model by remap (45), frame, colormap, skin deltas
  (46, 47, 48), effects XOR (49), angle 1, 2, 3 deltas (52, 55, 58), all against the baseline; then
  origin deltas against the baseline origin by high-byte bits 0..5 (x lo 50, x hi 51,
  y lo 53, y hi 54, z lo 56, z hi 57). The stored mask of a new entity is 0.
- **Bit 7 clear, base entities:** if bit 6 is clear, k = symbol (1..63): keep k − 1 base
  entities, then **remove** the next one. If bit 6 is set, k = bits 0..4, and k = 0 means
  k = 32 + one more byte (row 41): keep k − 1, then update the next one: mask = its stored
  mask; XOR a high byte (row 43, shifted up 8) if symbol bit 5; XOR a low byte (row 44) if the
  mask's bit 14 is set. Model, frame, colormap, skin, effects as for a new entity but against
  the entity's current values. Each angle: if its bit is set, its rate += delta; the rate is
  **always** added to the angle. Each origin axis: lo/hi deltas (by high-byte bits 0..5) go
  into the axis rate, and the rate is **always** added to the origin. The mask is stored with
  the entity. (Kept entities are copied as they are: their rates are not applied.)
- **Written:** a full packet-entities list: for every entity, the header word carries the
  number (bits 0..8) and the bits of the fields that differ from that entity's baseline (model,
  frame, colormap, skin, effects, each origin, each angle); the more-bits flag and the low
  byte follow the wire's rule; then the fields in wire order; terminated by a zero word.

#### 2.7 The built-in dictionaries and name lists [read]

The decoder needs, besides the model file, tables built into the proxy program: five string
dictionaries (print, chat print, stufftext, centerprint, setinfo key), each indexed by the
symbol 256..511, and two sorted name lists (61 model names, all beginning "progs/", and 189
sound names, each list preceded by an empty entry). They are in the program's data, not in
the model file. The reconstruction carries copies; their contents are not given here. Without
them a decoder cannot expand dictionary symbols or remapped model and sound numbers.

#### 2.8 The model and sound remaps [read]

Two permutations of 0..255 map a coded number to the server's model or sound number; they
start as the identity. They are rebuilt on each sequence record, and on a raw packet's
stufftext beginning "cmd prespawn" or "cmd spawn" while not yet built since the last list
change. Building: walk the stored list messages (up to 8 chunks per list, a chunk with
cursor 0 restarting the list, a chunk with an already stored cursor ignored) name by name,
the k-th name being server number k (from 1). For each name found in the built-in list
(models: only names beginning "progs/", compared without that prefix; sounds: the whole name)
at index c, swap so that code c decodes to server number k and the server number that code c
used to decode to takes k's former code. A quirk: the model walk moves from its first chunk to
chunk index (number of sound chunks + 1), not to its second chunk, so on a map with more than
one model chunk most later models stay unmapped.

#### 2.9 What a raw packet teaches the decoder [read]

Parsed as protocol 28 messages (the proxy's 81..83 accepted). It records: the recorded
player's slot from serverdata (the low 7 bits of the slot byte), clearing names; names from
updateuserinfo, setinfo "name" and updatename; baselines from spawnbaseline; the player model
index (the position of "progs/player.mdl" in the model list); a modellist with cursor 0
clears baselines and entity history; list chunks for the remaps; the packet's players and
entities (deltas against the raw entity history keyed by the low byte of the base sequence).
These lists are stored in history under the packet's sequence, or under (command sequence +
1) when the packet holds players and the command sequence is not 0.

### 3. The model file (compress.dat)

1. **Needed to decode: yes [read, measured].** Every symbol except the outer record kinds is
   coded with a model built from it; a different file decodes to garbage.
2. **Layout [read, measured]:** 262,144 bytes = 256 rows × 256 unsigned 32-bit little-endian
   counts; entry (r, s) at byte 1024 r + 4 s is the training count of symbol s under row r's
   context (rows per 2.2). Measured: every count is ≥ 1 (add-one smoothing); 49 rows are all
   ones, i.e. never trained (among them rows 28 and 29, player buttons and impulse, and rows
   243..255); the largest count is 2,203,463; rows 0..3 all total 1,859,627 (one count per
   trained packet).
3. **Derivable or rebuildable: no [derived].** The counts come from training on an unknown
   demo collection; nothing in the format records them. A model trained on other demos would
   produce a different, incompatible format. A reader needs the exact file (or the exact
   cumulative bounds built from it).
4. **Where to obtain it [read, measured]:** in the proxy's 2.91 distribution (the release
   zip, still mirrored; proxy.md lists it at 262,144 bytes deflating to 50,603), in the
   repository preserving that release (github.com/osm/qizmo), and embedded in
   github.com/osm/quake. All three copies are identical: SHA-256
   `d5013d85ad4a0df70b34cb292078ef51be05a67935dbefeaddcadb0e483546e0` **[measured]**. Its
   terms are the proxy's ("non-commercial use only"), not MIT **[read]**.
5. **The same in every release? Open.** Only 2.91 was obtained (its Linux and Windows builds
   ship the same file). Earlier releases (2.00 to 2.71 exist, per the unpacker's README) were
   not found; whether they carry this file or another is not settled.

### 4. Is the decoded file the original .qwd, byte for byte?

**No. The format is lossy and canonicalising [read, and measured by round trips through the
original program on three corpus qwd files].** Measured sizes: 1,523,712 to 126,960 (12.0:1); 2,841,046
to 296,120 (9.6:1); 15,099,131 to 2,212,337 (6.8:1); back to 1,522,973, 2,838,290
and 15,092,325 bytes. Every transform, with its inverse (where none exists, said so):

| What the original compressor does | Measured on the round trip | Inverse |
| --- | --- | --- |
| Times rounded to whole milliseconds (time × 1000 + 0.5, truncated) and made relative: the record after the first user command becomes 0.000 s; everything before it is 0 | first record 4651.285 s became 0.000 s; every later time within 1 ms of (original − offset) | none (absolute time and sub-ms lost) |
| A record's time is coded only after a user command; records after other records repeat the last time | in the measured files the source already did this (one exception in 16,433 records) | none |
| The time step after a user command is clamped to 255 ms; a longer gap makes all later times fall behind | no such gap in the measured files | none |
| User command angles stored as signed 16-bit angle units | angles above 180° came back as negative equivalents (359.74° became −0.258°) | equal modulo 360° |
| User command: the second angle triple is replaced by the first | 8,364 of 8,365 commands changed there | none |
| User command: impulse is sticky; a zero impulse is written as the last coded one (8 at the start) | 8,334 of 8,365 commands changed impulse 0 to 7, 8 or 2 | none |
| User command: three padding bytes written as zero | | none |
| Packet header (compressed packets): the reliable bit dropped, the acknowledge word replaced by the sequence | 3,866 acknowledge words and 236 sequence words changed in one file | none |
| User-info messages moved after all other messages of the packet (in raw packets too) | a signon packet came back with its six updateuserinfo messages moved to the end | none needed by a reader |
| Messages before or between player records moved after the trailing messages; all playerinfo gathered right after the header, the recorded player first | a temp entity before the players came back after them | none |
| Consecutive high-level prints merged into one message | | none |
| The proxy's private string message (81) dropped | | none |
| Delta packet entities (48) written as full packet entities (47) | | equivalent state |
| Playerinfo flags recomputed from the values (zero fields dropped, a missing msec flag added) | a zero third velocity was dropped from a player | equivalent state |
| Playerinfo command: buttons and the third angle never written | | none |
| A packet holding a disconnect stored raw with the disconnect replaced by a no-op; after it, playerinfo messages removed from later packets | | none |
| A last packet the compressor cannot code is lost | a final 89-byte packet whose entity list was cut short was missing | none |
| A zero-length packet ends the original compressor; packets over 65,535 bytes cannot be coded | | none |

**The odd file [measured].** `exo_4on4_hpr_vs_x_dm3_000.qwz` (lead `80 01 cd`): the original
program's `-D` writes only 6,986 bytes, 17 records, ending with a packet record of length 0.
The reconstruction writes 4,319,247 bytes whose first 6,986 are the same, then about 157,000
more records, many of them zero-length packets, with times reaching 10,081 s, and stops on
the dropped-tail marker at byte 561,386 of 1,221,217. A zero-length packet is something the
original compressor cannot write, and the original's decoder produces it at the first packet
it treats as dropped, so this file was not made by 2.91's `-C` from a sound qwd (made by
another release, another program, or damaged); which one is not settled.

**Not settled by any source:** whether the original `-D` ever reproduces a sound qwd exactly; on
the three measured it did not.

### 5. How encoding works

**[read; the reconstruction's encoder matches the original's output bit for bit on its
3,653 test cases (its own record), but refuses the three corpus files above because they need
the lossy transforms of question 4; the original compressed all three, measured.]**

- **The coder:** start `low` = 0, `high` = 0xFFFF_FFFF. For symbol s with bounds L = C[s−1]
  (0 for s = 0) and U = C[s] and range R = `high` − `low`: new `high` = `low` + floor(U × R
  / 2^32) − 1, and for s > 0, new `low` = `low` + floor(L × R / 2^32). A symbol whose scaled
  interval is empty cannot be coded. Then as long as `high` − `low` is at most 0x00FF_FFFF,
  renormalise: if `low` ≤ `high` (no wrap), output the cached byte (plus one if `low` went
  below its value at the last shift, a carry) followed by the pending 0xFF bytes (0x00 on a
  carry), and cache the top byte of `low`; if `low` > `high` (the interval wrapped through
  2^32), count one more pending byte instead. Then shift `low` and `high` left 8, `high`
  taking 0xFF. At the end: without a wrap, flush the cache and the pending bytes as above,
  then write the top byte of `high` unless `low` is 0; with a wrap, write the cached byte plus
  one and drop the pending bytes.
- **The order** is the decoder's: per qwd record, the kind symbol, then the body. Before
  each record that follows a user command, the time symbol (increment − previous increment,
  mod 256) is coded at the end of that command; after the last record, if it was a user
  command, a 0 time symbol; then the end symbol 3.
- **Raw or compressed:** a packet record is compressed only when it is not right after a
  sequence record, holds no disconnect, has a sequence between 1 and 255 ahead of the
  previous packet's, contains the recorded player's playerinfo and a packet-entities
  message, and holds only messages the format codes. Everything else (connection packets,
  the signon) is stored raw.
- **The back reference** is always the previous packet (r = its distance), and only when
  r ≤ 30, its lists are in history, the command-msec ring holds its sequence, history is
  valid (more than 32 packets after the last serverdata, or after a missing delta base), and
  the previous packet's acknowledge word did not carry the reliable bit without the
  sequence word carrying it. Otherwise r = 0.
- **Field choices:** each predicted value's delta is coded; a mask bit is set exactly when its
  byte is non-zero; mask bytes are coded as XOR with the stored masks. An additional player is
  coded as "unchanged" (msec bit 7) when its visible fields equal the base and its msec is
  larger than the base's; after such a player, if more follow, the list is closed and a new 42
  list opened (the slot chain continues). Dictionary strings: centerprint, chat and stufftext
  match greedily against fixed sub-ranges of their dictionaries (centerprint symbols 256..292,
  chat 320..509, stufftext 256..409); print and setinfo keys pick the cheapest parse.
- **Entity runs:** unchanged base entities are kept by runs and implicit keeping at the end,
  removed ones by the remove form, changed ones by the update form, new ones by number delta
  from the previous number (starting at 32).

### 6. The second byte: 01, 02, 03, 08

**Not a version, option or model id: it is coder output [read, measured].** After the first
symbol fixes the first byte (question 1), the next symbols are the packet mode (0, raw) and
the low byte of the first packet's length, coded with row 131; the second byte is mainly the
position of that length byte in row 131's distribution. Measured on the corpus (first packet
length, its low byte, the second file byte):

| First packet | Length (low byte) | Second byte | Files |
| --- | --- | --- | --- |
| a 5-byte connection-accept packet (recorded through the proxy) | 5 (0x05) | 01 | 11 |
| serverdata | 520 (0x08) | 01 | 1 (the odd file) |
| serverdata | 301 (0x2D) | 02 | 1 |
| serverdata | 323 (0x43) | 03 | 1 |
| serverdata | 457 (0xC9), 486 (0xE6) | 08 | 2 |

The mapping is monotonic in the low byte, as an arithmetic code is **[measured]**. The common
16-byte prefix in proxy.md (c) is the coding of that identical 5-byte first packet and the
signon after it.

---

## B. Old client recordings, protocol 26 (1998)

### 7. Protocol 26 against 28 in a client recording

Sources: the released QuakeWorld source is protocol 28 only (it defines no 26 behaviour)
**[read]**; FTE and ezQuake accept versions 24 to 28 in demo playback only **[read]**, and FTE
keeps the only version-dependent branches, listed here. The two corpus protocol 26 demos
(`Thresh-QW-1_qw4.qwd`, 15,553,996 bytes, 218,757 records; `repqwd8-1_qwd8-5.qwd`,
17,353,600 bytes, 241,727 records) were parsed with a measuring parser.

1. **Demo records: identical [measured].** Same 4-byte float time, kind byte, 36-byte user
   command record (msec, 3 pad, 3 float angles, 3 shorts move, buttons, impulse, 3 floats),
   32-bit packet length, 8-byte sequence record. Both files walk to their exact end.
2. **Serverdata (11): identical to 28's layout [measured, read].** Version long 26 (0x1A),
   server count, game directory string, player slot byte, level name, then the ten movement
   floats (gravity 800, stop speed 100, max speed 320 … as in 28). FTE reads the movement
   floats only for version ≥ 25 and substitutes the defaults (800, 100, 320, 500, 10, 0.7, 10,
   6, 1, 1) below 25 **[read]**.
3. **Soundlist (46) and modellist (45): identical [measured, read].** They have the start
   byte before the names and the next-start byte after the empty name. FTE reads both bytes
   only for version ≥ 26 (below 26 there are none) **[read]**.
4. **The command inside playerinfo (42, flag 0x02) differs [read in FTE, confirmed by
   measurement].** In protocol 26 and below: one flags byte; angle 1 (pitch) as a short if bit
   0x01; **angle 2 (yaw) as a short always**; angle 3 if bit 0x02; forward, side and up each as
   **one byte, multiplied by 8**, if bits 0x04, 0x08, 0x10; buttons if 0x20; impulse if 0x40; **msec
   as a byte only if bit 0x80** (in 28 bit 0x80 is the yaw flag and msec is always
   present; moves are shorts). Measured: with the 26 rule, 107,390 of 107,396 and 116,609 of
   116,611 game packets parse to their exact ends (the few failures are one serverdata
   packet per file that the measuring parser does not handle); with the 28 rule only 45,004
   and 61,373 parse.
5. **Everything else measured is the same as 28 [measured]:** with the 26 usercmd rule, the
   message numbers 2 to 52 and the layouts of playerinfo's other fields, packet entities (47,
   48), sound, temp entities, nails (43), chokecount (44), updatestatlong, setinfo and
   serverinfo all parse cleanly. Not seen in either file: download (41) and updatepl (53); whether
   26 had them is not settled.
6. **The recorded player's own command records** (kind 0) use the client's full shorts in both
   versions; in the 26 files 79% of move triples are multiples of 8 **[measured]**.

### 8. Protocol 27

**Open.** No corpus file carries version 27 **[measured]**. FTE and ezQuake accept 27 in demo
playback; FTE's version branches (≥ 25, ≥ 26, ≤ 26) treat 27 exactly like 28 **[read]**, but
no source read states what 27 changed or whether a release ever recorded it.

---

## C. A damaged-looking client recording

### 9. A record type outside 0..2

**What clients do [read]:**
- **ezQuake and FTE keep only the low 3 bits of the kind byte.** 0 is a user command, 1 a
  packet, 2 a sequence record, 3 to 6 are the multiview kinds (3 "to several players" with a
  32-bit player mask, 4 "to one player" and 5 "stats", the player in the upper 5 bits, 6 "to
  all"), each followed by a packet length and body; **7 stops playback**: FTE prints
  "Corrupted demo." and stops, ezQuake raises "Corrupted demo". FTE applies the multiview
  kinds even when playing a client demo. A packet longer than the client's message limit
  also stops playback, with a console message that the demo message is too long.
- So kind **40** (binary 101000, low bits 0) is read **as a user command**, and playback goes
  on reading what follows as records until a length or kind stops it.
- ezQuake's probe of an unknown file (deciding qwd or multiview) gives up on low bits 7.

**The corpus file [measured]** (`Locktar_-_Locktar_vs_[tVS]Ihminen_-_[dmm4]_-_[end]_000.qwd`,
3,188,708 bytes):
- Bytes 0 to about 16,410 are a sound protocol 28 recording: 150 records, game time 1,696.10
  to 1,696.99 s, ending inside a user command record that starts at byte 16,375 and whose
  angle bytes are already wrong.
- From there to byte 69,634 (about 53,200 bytes) the bytes are random-looking: 7.99
  bits/byte over 16 KiB windows (the sound parts measure 5.2 to 5.9), no record structure.
- From byte **69,635** a clean record chain resumes and runs 28,914 records to the exact end of
  the file, at game time 1,701.69 s onwards (188.4 s more).
- So the high entropy is a damaged region, not a change of coding to the end of the file. The
  gap is 4.7 s of game time; at this file's measured rate (16.5 KB/s) that would hold about
  78 KB, not 53 KB, so the region neither exactly overwrites nor exactly replaces the lost
  stretch.

**Known writers that switch coding mid-file or append data: none found [read].** The proxy's
compressed format is a whole-file code with no embedded plain parts (question 1), and the
client demo writers read (ezQuake, FTE) write only kinds 0, 1 and 2 to a client demo. Who or
what damaged this file is not settled.

---

## D. Extension bits a client recording may carry

### 10. The extensions and how each changes the layout

**How they are recorded [read]:** before the version 28 in serverdata, pairs of (32-bit
family magic, 32-bit bit set) for each family in force: FTE (`FTEX`), FTE second set
(`FTE2`), multiview (`MVD1`). ezQuake writes into a client demo the FTE bits it negotiated
(leaving the pair out when the only bits are chunked downloads and 256 packet entities),
and the FTE2 and MVD1 bits it negotiated. The ZQuake bits are not in serverdata: they are the
`*z_ext` key of the server info (a stufftext of the full server info) **[read]**.

**What the corpus carries [measured]:** none of the 22 client demos (6 qwd, 16 decoded qwz) nor the 15 multiview recordings carries an extension family in serverdata.
So "commonly carried today" below rests on the current server's recording code **[read]**,
not on a measured modern file.

**FTE family (`FTEX`) [read]:**

| Bit | Name a player sees in settings | Layout change |
| --- | --- | --- |
| 0x00000008 | transparency | Packet entities: when the low byte's bit 7 ("even more") is set, a third flags byte follows (and a fourth if its bit 7 "yet more" is set); its bit 1 adds an alpha byte after the angles. Playerinfo: flags bit 15 now means "an extra flags byte follows"; the ZQuake on-ground and solid bits (14 and 15) move to bits 22 and 23 of the widened flags; bit 17 adds an alpha byte after the weapon frame (bit 16 a scale byte first, under scale 0x2) |
| 0x00000080 | sound-double | New message 55 "extended sound": flags byte (volume 0x01, attenuation 0x02, more flags 0x04, large entity 0x08, large sound 0x10, time offset 0x40, pitch 0x80); optional volume and attenuation bytes; entity+channel as a short, or with large entity a separate entity reference and a channel byte; sound index a byte or (large sound) a short; then coordinates. Message 56 a soundlist with a 16-bit cursor; message 94 a static sound with a 16-bit index |
| 0x00001000 | model-double | Packet entities: "even more" byte bit 3 adds 256 to a model index, or alone reads the model as a short. Message 60 a modellist with a 16-bit cursor |
| 0x00002000, 0x00004000 | entity-double, entity-double2 | "Even more" byte bits 5 and 6 add 512 and 1024 to the entity number (up to 2,048 entities); in a delta list a removal can carry the extra byte too |
| 0x00008000 | float coords | Every coordinate a 4-byte float (not a 2-byte eighth), every angle 2 bytes (not 1); only when the server runs big coordinates |
| 0x00080000 | colour modulation | bit 2 of the "yet more" (fourth) byte adds three colour bytes after alpha; playerinfo bit 19 likewise |
| 0x00400000 | spawnstatic2 | Messages 21 and 66: static entity and baseline as an entity delta from the null state (66: a short entity number first) |
| 0x01000000 | 256 packet entities | Only the limit: up to 256 entities in one packet-entities list (64 without it) |
| 0x20000000 | chunked downloads | Download (41) messages carry chunk numbers (protocol.md § 12.3) |
| 0x40000000 | client-side programs | Messages 76, 83/90, 92 for client-side game code; layout per those messages, not detailed here |
| 0x00000400 | bullet temp entities | Temp entity types 14, 15 and 17..21 exist (layouts not detailed here) |
| 0x00000040, 0x00000200, 0x80000000 | accurate timings, half-life maps, DarkPlaces flags | No layout ezQuake reads (DarkPlaces flags defines further bits of the fourth flags byte, which ezQuake's entity parser does not read) |

**FTE second family (`FTE2`) [read]:** 0x02 voice chat: message 84 = sender byte, generation
byte, sequence byte, 16-bit length, then that many bytes. 0x08 replacement deltas: replaces
packet entities and playerinfo with a different persistent delta format (message 86, a float
time, entity records, a terminating zero short) and widens sound flags; not detailed here.
Other FTE2 bits (cursor, angle deltas, more players, prediction info, size encoding, info
blobs, STUN-aware netchan, VR input, lerp time) belong to FTE's own client and server; ezQuake
does not parse them.

**Multiview family (`MVD1`) [read]:** 0x01 float coords: only entity and player origins
become 4-byte floats (other coordinates stay 2 bytes). 0x02 high-lag teleport: no recorded
layout change. 0x08 debug weapon and 0x10 debug antilag: client-to-server only. 0x20 hidden
messages: in a multiview demo, a "to several players" record with mask 0 holds hidden blocks
(length, type id, data, repeated). 0x04/0x40 server-side weapon, 0x80 weapon prediction, 0x100
simple projectiles: commented out in the shared header today.

**ZQuake bits (`*z_ext`) [read]:** pm type 0x01 (playerinfo flags bits 11..13 hold a move-type
code), new pm types 0x02, view height 0x04, server time 0x08, pitch limits 0x10, join/observe
0x20, on-ground valid 0x40 (flags bit 14 meaningful for every player), view weapons 0x80,
solid valid 0x100 (flags bit 15). Only the flag meanings change; no bytes are added. The
corpus's ezQuake userinfo shows `*z_ext` 127 **[measured]**.

**Commonly carried today [read, from the current server's recording code]:** a multiview
recording of the current server always carries FTE 256 packet entities, model-double,
entity-double, entity-double2, spawnstatic2, transparency and colour modulation; sound-double
only when the map has 256 or more sounds; float coords only with big coordinates; client-side
programs only when the mod uses them; bullet temp entities and replacement deltas only in its
"rd" format (chosen automatically only when the map needs more than 2,048 entities); FTE2
voice chat always; MVD1 hidden messages always. A client demo recorded by ezQuake carries
whatever its connection negotiated (protocol.md § 11 lists what the current server offers).
