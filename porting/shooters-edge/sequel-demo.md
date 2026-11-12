# The sequel's client recording (.dm2) and protocol 34's server messages

This is a working note of clean-room facts about the sequel's client recording (.dm2) and the
server-to-client messages of network protocol 34, written by a reader who read id Software's GPL
source release of Quake II (the original release on GitHub, default branch; it is the 3.21 code and
declares protocol 34) and measured the one recording in the 3.14 demo pack
(`baseq2/pak0.pak`, `demos/q2demo1.dm2`, 138 511 bytes). It contains no code. Each fact is marked
**[read]** (stated by the source), **[measured]** (counted by walking the recording with a script of
the reader's own) or **[derived]** (worked out from read or measured facts). Where the source does
not settle something, it says so. The answers are numbered after the questions.

Conventions used below: every multi-byte integer is little-endian **[read]**. "byte" is an unsigned
8-bit value, "char" a signed 8-bit value, "short" a signed 16-bit value, "long" a signed 32-bit value.
"coord" is a short holding 1/8 unit; "pos" is three coords (x, y, z; 6 bytes); "angle8" is one byte
holding 1/256 of a turn; "angle16" is a short holding 1/65536 of a turn; "dir" is a byte index into
the table of 162 normals; "string" is bytes up to and including a zero byte (see Q9 and Q10).

---

## 1. The recording's framing

- The file is a sequence of blocks. Each block is a 4-byte little-endian signed length followed by
  exactly that many bytes of message data. There is no header before the first block: the file's
  first four bytes are the first block's length **[read]**. Measured: the recording's first four bytes
  give 1334, and the first block starts with the serverdata message **[measured]**.
- A length of -1 (bytes FF FF FF FF) ends the recording; the client writes it when recording stops
  **[read]**. Playback also ends, without error, at end of file or when a block cannot be read whole
  **[read]**. Measured: 640 blocks, then the -1 marker, then end of file; 4 x 641 + 135 947 = 138 511
  bytes, the whole file **[measured]**.
- Playback refuses a block longer than 1400 bytes (the protocol's largest message) and drops the
  game with an error **[read]**. A block of length 0 ends playback in this release, because the read
  of zero bytes is treated as a failed read **[derived]** from what playback does; no zero-length
  block occurs in the demo **[measured]**.
- A block is one server packet's message area: the packet as received, minus its 8-byte connection
  header (a 32-bit outgoing sequence whose top bit flags a reliable payload, and a 32-bit acknowledged
  sequence whose top bit is the reliable even/odd acknowledgement). The recorder strips those 8
  bytes, so a block carries no sequence numbers or any other per-packet header **[read]**. Inside a
  packet, the reliable part comes first and the unreliable part after it, with nothing marking the
  boundary **[read]**.
- Exception: the first blocks of a client recording are not received packets but synthesised by the
  recorder (the signon, Q8); they use the same framing **[read]**.
- Block sizes: the largest a live packet can give is 1400 - 8 = 1392 bytes **[derived]**. Measured:
  smallest block 106 bytes, largest 1363 **[measured]**.
- One block is consumed per server frame during playback, i.e. every 100 ms; while the game is
  paused no block is read **[read]**.

## 2. Protocol 34's server message ids and their payloads

Each message is one id byte followed by its payload. A message area ends when the bytes run out
(there is no end-of-message id) **[read]**. All ids **[read]**:

| id | message | payload | length |
| --- | --- | --- | --- |
| 0 | bad | never valid; the reader drops the game | - |
| 1 | muzzleflash | short entity, byte weapon flash | 3 |
| 2 | muzzleflash2 | short entity, byte flash number | 3 |
| 3 | temp_entity | byte kind, then a payload by kind (Q7) | variable |
| 4 | layout | string | variable |
| 5 | inventory | 256 shorts | 512 |
| 6 | nop | nothing | 0 |
| 7 | disconnect | nothing | 0 |
| 8 | reconnect | nothing | 0 |
| 9 | sound | byte flags, byte sound index, optional fields (Q7) | 2 to 13 |
| 10 | print | byte level, string | variable |
| 11 | stufftext | string | variable |
| 12 | serverdata | long, long, byte, string, short, string (Q3) | variable |
| 13 | configstring | short index, string | variable |
| 14 | spawnbaseline | entity header and delta (Q4) | variable |
| 15 | centerprint | string | variable |
| 16 | download | short size, byte percent, then size bytes (Q7) | variable |
| 17 | playerinfo | only inside a frame (Q6) | variable |
| 18 | packetentities | only inside a frame (Q4) | variable |
| 19 | deltapacketentities | defined but never written; reader rejects it | - |
| 20 | frame | header, then playerinfo, then packetentities (Q5) | variable |

- Ids 17, 18 and 19 standing alone (not inside a frame) make the reader drop the game ("out of
  place frame data") **[read]**. Any id above 20 also drops it **[read]**.
- Ids 1 to 5 are the ones a game module writes itself; the rest are written only by the engine
  **[read]**.

## 3. serverdata and configstring

**serverdata (12)**, fields in order **[read]**:

| field | type | size | meaning |
| --- | --- | --- | --- |
| protocol | long | 4 | 34 in this release |
| server count | long | 4 | the server's spawn count; a client recording writes 0x10000 plus it |
| attract loop | byte | 1 | 0 live game, 1 client recording, 2 server recording |
| game directory | string | var | empty for the base game |
| player number | short | 2 | the viewer's client slot (entity number minus 1); -1 for a cinematic or picture |
| level name | string | var | the full level name (configstring 0); a cinematic's or picture's name when player number is -1 |

Measured in the demo: protocol 31, server count 0x13CBE (so 0x10000 + 0x3CBE), attract loop 1, empty
game directory, player number 0, level name "Installation" **[measured]**. On receiving serverdata
the client wipes its whole per-level state first **[read]**.

**configstring (13)**: short index, then string **[read]**. The reader drops the game if the index
is below 0 or at least 2080 **[read]**. Slots are 64 bytes each (the longest path) **[read]**, but a
value longer than 63 characters set into one slot runs on into the following slots; a recording
re-sends each non-empty slot separately, so such a slot's message carries the tail of the long value
from that slot's 64-byte offset onward **[read]** **[measured]**: the status bar program in slot 5 is
276 characters, and slots 6, 7, 8, 9 carry its last 212, 148, 84 and 20 characters **[measured]**.

Index ranges, 2080 slots in all **[read]**:

| index | what |
| --- | --- |
| 0 | level name |
| 1 | CD track |
| 2 | sky name |
| 3 | sky axis, three numbers as text |
| 4 | sky rotation speed |
| 5 | status bar program (may run on through 28) |
| 6 to 28 | no own meaning; room for the status bar's run-on |
| 29 | air acceleration |
| 30 | max clients |
| 31 | map checksum |
| 32 to 287 | models (256; index 1 is the world, "*n" is an inline brush model) |
| 288 to 543 | sounds (256) |
| 544 to 799 | images (256) |
| 800 to 1055 | light styles (256) |
| 1056 to 1311 | items (256) |
| 1312 to 1567 | player skins, one per client (256) |
| 1568 to 2079 | general (512) |

Model, sound and image numbers in other messages are the offset into their range (sent as a byte)
**[read]**. Measured: 329 configstring messages, 328 of them in the signon and one mid-game (block 78,
model slot 128, a grenade model); by range: 12 in 0 to 31 (slots 0 to 9, 30, 31), 96 models, 150
sounds, 12 images, 17 light styles, 41 items, 1 player skin, 0 general **[measured]**. The signon's
configstrings come in ascending index order **[measured]** (the recorder walks the slots in order
**[read]**).

## 4. spawnbaseline and the entity delta in packetentities

**The entity header** **[read]**:

1. One byte of flag bits 0 to 7. If bit 7 is set, a second byte gives bits 8 to 15. If bit 15 is
   set, a third byte gives bits 16 to 23. If bit 23 is set, a fourth byte gives bits 24 to 31. So the
   header is 1, 2, 3 or 4 bytes, and each "more bits" flag sits in the top bit of the byte before.
2. The entity number: a short if bit 8 is set, else a byte. The writer sets bit 8 for numbers 256 and
   above; numbers are 1 to 1023 **[read]**.

A writer sets every "more bits" flag below the highest non-empty byte, so the number of bytes is
fixed by the highest set flag **[read]**.

**Flag bits** **[read]**:

| bit | value | meaning |
| --- | --- | --- |
| 0 | 0x00000001 | origin x |
| 1 | 0x00000002 | origin y |
| 2 | 0x00000004 | angle 2 (yaw) |
| 3 | 0x00000008 | angle 3 (roll) |
| 4 | 0x00000010 | frame as a byte |
| 5 | 0x00000020 | event |
| 6 | 0x00000040 | remove |
| 7 | 0x00000080 | more bits: a second header byte |
| 8 | 0x00000100 | entity number is a short |
| 9 | 0x00000200 | origin z |
| 10 | 0x00000400 | angle 1 (pitch) |
| 11 | 0x00000800 | model index |
| 12 | 0x00001000 | render effects, low size bit |
| 13 | 0x00002000 | unused |
| 14 | 0x00004000 | effects, low size bit |
| 15 | 0x00008000 | more bits: a third header byte |
| 16 | 0x00010000 | skin, low size bit |
| 17 | 0x00020000 | frame as a short |
| 18 | 0x00040000 | render effects, high size bit |
| 19 | 0x00080000 | effects, high size bit |
| 20 | 0x00100000 | model index 2 |
| 21 | 0x00200000 | model index 3 |
| 22 | 0x00400000 | model index 4 |
| 23 | 0x00800000 | more bits: a fourth header byte |
| 24 | 0x01000000 | old origin |
| 25 | 0x02000000 | skin, high size bit |
| 26 | 0x04000000 | sound |
| 27 | 0x08000000 | solid |
| 28 to 31 | - | unused |

**Field order after the number, each present only when its flag is set** **[read]**:

| order | field | type |
| --- | --- | --- |
| 1 | model index | byte |
| 2 | model index 2 | byte |
| 3 | model index 3 | byte |
| 4 | model index 4 | byte |
| 5 | frame (bit 4) | byte |
| 6 | frame (bit 17) | short |
| 7 | skin | low bit only: byte; high bit only: short; both: long |
| 8 | effects | low bit only: byte; high bit only: short; both: long |
| 9 | render effects | low bit only: byte; high bit only: short; both: long |
| 10 | origin x | coord |
| 11 | origin y | coord |
| 12 | origin z | coord |
| 13 | angle 1 (pitch) | angle8 |
| 14 | angle 2 (yaw) | angle8 |
| 15 | angle 3 (roll) | angle8 |
| 16 | old origin | pos |
| 17 | sound | byte (sound index) |
| 18 | event | byte |
| 19 | solid | short |

- The field order is not the flag-bit order (origin z's flag is bit 9 but it is read with x and y,
  and pitch's flag is bit 10 but it is read before yaw) **[read]**.
- Size choice by the writer **[read]**: frame below 256 a byte, else a short. Skin, compared as
  unsigned: below 256 a byte, below 65536 a short, else a long. Effects and render effects: below 256
  a byte, below 32768 a short, else (including any negative value) a long. The reader accepts both
  frame flags at once and then reads a byte and then a short, the short winning **[read]**.
- The solid short packs a bounding box: 0 not solid, 31 a brush model, otherwise bits 0 to 4 are the
  half-width / 8, bits 5 to 9 the depth below the origin / 8, bits 10 to 15 (the height above the
  origin + 32) / 8 **[read]**.
- **The base a delta applies to** **[read]**: a spawnbaseline is a delta from an all-zero state. In a
  packetentities list, an entity present in the frame being deltaed from is a delta from its state
  there; an entity not present there is a delta from its baseline.
- **State rules a field predictor must copy** **[read]**: before applying a delta, the new state's old
  origin is set to the base state's origin; the old origin flag overrides that. The event is not
  delta-coded: it is zero unless the event flag is present in this delta. Every other field keeps
  the base's value when its flag is absent.
- **When the writer sets the old origin flag** **[read]**: for every spawnbaseline, for an entity
  entering a frame from its baseline, for every delta of an entity whose number is at most the
  server's max clients (players), and for any entity with the beam render effect (render effects
  bit 7, value 128). Because the flag counts as a change, players are sent every frame even when
  nothing else changed **[derived]**. Measured: only 131 of the 179 baselines in the 3.14 recording
  carry the old origin flag, so the 3.14 writer did not set it for every baseline; a byte-exact
  reader must take the flag as found **[measured]**.
- **Which baselines are sent** **[read]**: a live server sends one for every entity whose baseline
  has a model, a sound or effects; a client recording writes one for every entity whose baseline has
  a model index other than 0.
- **The list in packetentities (18)** **[read]**: entries in ascending entity number. An entity that
  is unchanged from the frame deltaed from is omitted and carried over unchanged. An entity in the old
  frame but absent from the new is removed by an entry whose flags have only the remove bit (plus bit 7
  and bit 8 when the number is 256 or more), then the number; no fields follow. The reader ignores
  any other flags on a remove entry. The list ends with an entry whose number is 0; the writer writes
  it as two zero bytes (header 0, number 0) **[read]**. Measured: all 632 lists end with two zero
  bytes **[measured]**. The reader drops the game for a number of 1024 or more **[read]**.
- The 3.21 writer never sends an entry with no flags except the end marker and forced baseline
  entries; the reader, given one, would treat it as "no change" for an existing entity or as a
  fresh copy of the baseline for a new one **[derived]**.
- Measured flag counts over every entity entry (baselines and frames) in the demo **[measured]**:
  bit 0 1837, bit 1 1835, bit 2 2587, bit 3 693, bit 4 3734, bit 5 86, bit 6 369 (removes),
  bit 7 12613, bit 8 11635, bit 9 631, bit 10 637, bit 11 83, bit 12 7, bit 13 0, bit 14 82,
  bit 15 2538, bit 16 38, bit 17 262, bit 18 0, bit 19 6, bit 20 2, bit 21 0, bit 22 0,
  bit 23 2334, bit 24 2314, bit 25 0, bit 26 39, bit 27 54. 179 baselines, entity numbers 10 to 371,
  in ascending order. Largest number of entity entries in one frame: 37.

## 5. frame (20)

Fields in order **[read]**:

| field | type | size | meaning |
| --- | --- | --- | --- |
| server frame | long | 4 | the server's frame number; frames are 100 ms apart |
| delta frame | long | 4 | the frame this one is deltaed from; -1 (or any value 0 or less) for none |
| suppress count | byte | 1 | packets the server held back for rate since the last one sent; absent only for protocol 26 |
| area bits length | byte | 1 | number of area bytes that follow |
| area bits | bytes | length | one bit per map area, set when the area is visible |

- Directly after the area bits must come the playerinfo id (17) and its payload, and directly after
  that the packetentities id (18) and its list; anything else drops the game **[read]**. So a frame is
  one message to a reader, with two id bytes inside it **[read]**.
- Both playerinfo and the entity list are deltas from the frame named by delta frame; with no delta
  frame they are deltas from zero (player state) and from baselines (entities) **[read]**. A client
  keeps the last 16 frames, indexed by frame number modulo 16 **[read]**; a server stops deltaing (sends
  -1) when the requested base is 13 or more frames old **[read]**.
- The area bits are at most 32 bytes (256 areas) in this release; the reader does not check the
  length against that **[read]**.
- In a recording, messages carried reliably (prints, configstrings, stufftext) come before the frame
  in the same block, and the frame's unreliable companions (sounds, temp entities, muzzle flashes)
  come after the entity list **[read]** **[measured]**: of the 632 frame blocks, 630 begin with the
  frame and 2 have one message before it (a print in block 581, a configstring in block 78)
  **[measured]**.
- Measured: 632 frames, server frames 137 to 768 consecutively, one frame per block from block 9 to
  block 640; the first frame has delta frame -1, each of the other 631 deltas from the frame just
  before it; every suppress count is 0; every area bits length is 1 **[measured]**.

## 6. playerinfo (17), the player state delta

A short of flag bits, then the fields, then the stats **[read]**. The base is the player state of the
delta frame, or all zeros **[read]**.

**Flag bits** **[read]**:

| bit | value | field |
| --- | --- | --- |
| 0 | 0x0001 | movement type |
| 1 | 0x0002 | origin |
| 2 | 0x0004 | velocity |
| 3 | 0x0008 | movement time |
| 4 | 0x0010 | movement flags |
| 5 | 0x0020 | gravity |
| 6 | 0x0040 | delta angles |
| 7 | 0x0080 | view offset |
| 8 | 0x0100 | view angles |
| 9 | 0x0200 | kick angles |
| 10 | 0x0400 | screen blend |
| 11 | 0x0800 | field of view |
| 12 | 0x1000 | gun model index |
| 13 | 0x2000 | gun frame (with offset and angles) |
| 14 | 0x4000 | render flags |
| 15 | 0x8000 | unused |

**Field order** (not the bit order: gun index and gun frame come before blend and field of view)
**[read]**:

| order | field | type | size | scale |
| --- | --- | --- | --- | --- |
| 1 | movement type | byte | 1 | 0 normal, 1 spectator, 2 dead, 3 gibbed, 4 frozen |
| 2 | origin | 3 shorts | 6 | 1/8 unit |
| 3 | velocity | 3 shorts | 6 | 1/8 unit per second |
| 4 | movement time | byte | 1 | 8 ms per unit |
| 5 | movement flags | byte | 1 | 1 ducked, 2 jump held, 4 on ground, 8 time is water jump, 16 time is landing, 32 time is teleport, 64 no prediction |
| 6 | gravity | short | 2 | units per second squared |
| 7 | delta angles | 3 shorts | 6 | angle16 |
| 8 | view offset | 3 chars | 3 | 1/4 unit |
| 9 | view angles | 3 shorts | 6 | angle16 |
| 10 | kick angles | 3 chars | 3 | 1/4 degree |
| 11 | gun model index | byte | 1 | model number |
| 12 | gun frame, gun offset, gun angles | byte, 3 chars, 3 chars | 7 | frame; 1/4 unit; 1/4 degree |
| 13 | screen blend | 4 bytes | 4 | red, green, blue, alpha, each 1/255 |
| 14 | field of view | byte | 1 | degrees |
| 15 | render flags | byte | 1 | 1 underwater, 2 no world model, 4 infrared goggles, 8 ultraviolet goggles |
| 16 | stats mask | long | 4 | bit n set: stat n follows |
| 17 | stats | one short per set bit, ascending | 2 each | raw value |

- The stats mask and its values are always present, even when the flag short is 0 **[read]**.
  There are 32 stats **[read]**.
- The gun frame flag is set by the writer when the gun frame changes, and it carries the gun offset
  and gun angles with it whether or not they changed; a change of offset or angles alone is not sent
  **[read]**.
- The 3.21 writer sets the gun model index flag in every playerinfo **[read]**; measured: set in all
  632 of the demo's **[measured]**.
- The writer scales by multiplying and truncating toward zero: view offset, kick and gun offset and
  angles by 4 into a char, blend by 255 into a byte **[read]**.
- When the client is playing a recording (attract loop non-zero), it forces the movement type to
  frozen after reading, whatever the recording says; this does not change the bytes **[read]**.
- Measured flag counts over the 632 playerinfos: bit 0 1, bit 1 512, bit 2 513, bit 3 3, bit 4 11,
  bit 5 1, bit 6 1, bit 7 541, bit 8 466, bit 9 548, bit 10 170, bit 11 1, bit 12 632, bit 13 466,
  bits 14 and 15 0. Stats sent: stat 0 1, stat 1 76, stat 2 10, stat 3 114, stat 6 2, stat 7 7,
  stat 8 7, stat 12 2, stat 15 54 times **[measured]**.

## 7. The other messages

**sound (9)** **[read]**: byte flags, byte sound index, then in this order:

| flag bit | value | field | type | meaning |
| --- | --- | --- | --- | --- |
| 0 | 1 | volume | byte | volume x 255; absent means 1.0 |
| 1 | 2 | attenuation | byte | attenuation x 64; absent means 1.0 |
| 4 | 16 | time offset | byte | milliseconds after the frame's start, 0 to 255 |
| 3 | 8 | entity and channel | short | entity number x 8 + channel (0 to 7) |
| 2 | 4 | position | pos | where the sound plays |

The field order is volume, attenuation, offset, entity, position: not the bit order **[read]**. A
sound with neither entity nor position is local to the listener **[read]**. The 3.21 server always
sets the entity flag, and sets the position flag for brush models, for entities not sent to clients
and when an origin is given **[read]**. The reader drops the game for an entity above 1024 **[read]**.
Measured: 159 sounds, flag bytes 8 (88 times), 10 (37), 14 (31), 15 (3) **[measured]**.

**temp_entity (3)**: byte kind, then a payload by kind **[read]**. Layouts as read:

| kind | effect | payload |
| --- | --- | --- |
| 0 | gunshot | pos, dir |
| 1 | blood | pos, dir |
| 2 | blaster impact | pos, dir |
| 3 | rail trail | pos start, pos end |
| 4 | shotgun impact | pos, dir |
| 5 | explosion 1 | pos |
| 6 | explosion 2 | pos |
| 7 | rocket explosion | pos |
| 8 | grenade explosion | pos |
| 9 | sparks | pos, dir |
| 10 | splash | byte count, pos, dir, byte splash kind (0 unknown, 1 sparks, 2 blue water, 3 brown water, 4 slime, 5 lava, 6 blood) |
| 11 | bubble trail | pos start, pos end |
| 12 | screen sparks | pos, dir |
| 13 | shield sparks | pos, dir |
| 14 | bullet sparks | pos, dir |
| 15 | laser sparks | byte count, pos, dir, byte colour |
| 16 | parasite attack | short entity, pos start, pos end |
| 17 | rocket explosion in water | pos |
| 18 | grenade explosion in water | pos |
| 19 | medic cable attack | short entity, pos start, pos end |
| 20 | BFG explosion | pos |
| 21 | BFG big explosion | pos |
| 22 | boss teleport | pos |
| 23 | BFG laser | pos start, pos end |
| 24 | grapple cable | short entity, pos start, pos end, pos offset |
| 25 | welding sparks | byte count, pos, dir, byte colour |
| 26 | green blood | pos, dir |
| 27 | blue hyperblaster | pos, pos (the second is a direction sent as a position, not a dir index) |
| 28 | plasma explosion | pos |
| 29 | tunnel sparks | byte count, pos, dir, byte colour |
| 30 | blaster 2 impact | pos, dir |
| 31 | rail trail 2 | not handled: the reader drops the game |
| 32 | flame | not handled: the reader drops the game |
| 33 | lightning | short source entity, short target entity, pos start, pos end |
| 34 | debug trail | pos start, pos end |
| 35 | plain explosion | pos |
| 36 | flashlight | pos, short entity |
| 37 | force wall | pos start, pos end, byte colour |
| 38 | heat beam | short entity, pos start, pos end |
| 39 | monster heat beam | short entity, pos start, pos end |
| 40 | steam | short id; then byte count, pos, dir, byte colour, short magnitude; then, only when id is not -1, long duration in ms |
| 41 | bubble trail 2 | pos start, pos end |
| 42 | more blood | pos, dir |
| 43 | heat beam sparks | pos, dir |
| 44 | heat beam steam | pos, dir |
| 45 | chainfist smoke | pos |
| 46 | electric sparks | pos, dir |
| 47 | tracker explosion | pos |
| 48 | teleport effect | pos |
| 49 | deathball goal | pos |
| 50 | widow beam out | short id, pos |
| 51 | nuke blast | pos |
| 52 | widow splash | pos |
| 53 | explosion 1 big | pos |
| 54 | explosion 1 without particles | pos |
| 55 | flechette impact | pos, dir |
| 56 and up | - | the reader drops the game |

Kinds 30 to 55 are the mission pack's additions, carried in the same release **[read]**. Measured: 459
temp entities, kinds 0 (148), 1 (243), 2 (16), 4 (47), 5 (2), 7 (1), 8 (2) **[measured]**.

**muzzleflash (1)**: short entity number (1 to 1023, else the reader drops the game), byte weapon
flash: the low 7 bits pick the player weapon's flash (0 to 39 defined), bit 7 (value 128) means
silenced **[read]**. Measured: 101 **[measured]**.

**muzzleflash2 (2)**: short entity number (1 to 1023), byte flash number picking one of the monster
flashes (0 to 146 defined in this release; the number indexes a table of muzzle offsets from the
monster's origin, so the client draws the flash and plays the sound) **[read]**. Measured: 203
**[measured]**.

**layout (4)**: one string, the layout program the client draws (scoreboards, help) **[read]**. The
client keeps at most 1023 characters of it **[read]**. Measured: none **[measured]**.

**inventory (5)**: 256 shorts, the count of each item in item-slot order; 512 bytes, fixed **[read]**.
Measured: none **[measured]**.

**print (10)**: byte level (0 low: pickups, 1 medium: deaths, 2 high: critical, 3 chat, which also
plays the chat sound), then the string **[read]**. Measured: one, level 1, "Player died." with its
newline, in block 581 **[measured]**.

**centerprint (15)**: one string shown in the middle of the screen **[read]**. Measured: none.

**stufftext (11)**: one string added to the client's console command buffer; it should end with a
newline **[read]**. Measured: one, "precache" with a newline, in block 8 **[measured]**.

**nop (6)**: no payload **[read]**. **disconnect (7)**: no payload; the client disconnects
**[read]**. **reconnect (8)**: no payload; the client reconnects to the same server **[read]**.
Measured: none of these three **[measured]**.

**download (16)**: short size, byte percent (0 to 100), then size bytes of file data. A size of -1
means the server does not have the file, and then no data follows; percent 100 marks the last piece
**[read]**. Measured: none **[measured]**.

Measured totals over the whole recording **[measured]**: frame 632, playerinfo 632 (inside frames),
packetentities 632 (inside frames), temp_entity 459, configstring 329, muzzleflash2 203, spawnbaseline
179, sound 159, muzzleflash 101, serverdata 1, stufftext 1, print 1; entity removes 369. No other ids
occur.

## 8. The recording's first blocks (the signon) and starting mid-game

A client can only start a recording while it is in a level, so every client recording starts
mid-game **[read]**. On the record command the client **[read]**:

1. Writes synthesised signon blocks from its own state: a serverdata (protocol 34 in this release,
   0x10000 plus the server count, attract loop 1, the game directory, the player number, the level
   name); then a configstring for every non-empty slot in index order; then a spawnbaseline for every
   entity whose baseline has a model; then a stufftext "precache" with a newline. It starts a new
   block when adding a configstring would bring the block past 1400 bytes with a 32-byte margin
   (current size plus the string's length plus 32), or a baseline would bring it past 1400 with a
   64-byte margin.
2. Then waits: it asks the server for frames that are not deltaed, and records nothing until a
   packet arrives whose frame has no delta frame. That packet is the first recorded one; from then
   on every packet the client parses is written as a block after it is parsed. Packets received
   between the record command and that first full frame are not recorded, with any reliable
   messages they carried.

Measured in the demo **[measured]**:

| block | contents |
| --- | --- |
| 1 (1334 bytes) | serverdata, 73 configstrings |
| 2 | 39 configstrings |
| 3 | 66 configstrings |
| 4 | 63 configstrings |
| 5 | 80 configstrings |
| 6 | 7 configstrings, 66 baselines |
| 7 | 71 baselines |
| 8 | 42 baselines, stufftext "precache" |
| 9 | the first frame (server frame 137, delta frame -1), then a sound |
| 10 to 640 | one frame each, deltaed from the previous frame |

A live server's own signon differs from a recording's **[read]**: its serverdata carries the bare
server count and the attract loop flag of the server (0 for a game), the configstrings and baselines
come over several reliable packets that the client asks for in turn by console commands the server
stuffs, the baselines sent are those with a model, a sound or effects, and the final stufftext is
"precache" followed by the server count.

A second kind of recording exists, the server recording (attract loop 2) **[read]**: one large first
block (up to 32 768 bytes) with serverdata (player number -1) and every configstring and no
baselines; then per server frame a block of a frame id, a long server frame, then the packetentities
id and every entity deltaed from an all-zero state, the two-zero-byte end, and the frame's multicast
messages. It has no delta frame, no suppress count, no area bits and no playerinfo, and stopping it
writes no -1 end marker. The client's reader in this release would reject it (it requires playerinfo
after a frame header) and playback would refuse its first block for being over 1400 bytes; it is for
merging recordings with other tools **[derived]** from the read facts.

## 9. Coordinate, angle and direction encodings

- **Coordinates** **[read]**: a short holding the value x 8, so 1/8 unit, range -4096 to +4095.875.
  The writer truncates toward zero. The player's origin and velocity in playerinfo are already held
  as whole 1/8 units and sent as shorts unchanged.
- **Entity angles** **[read]**: one byte. The writer takes the angle in degrees x 256 / 360,
  truncates toward zero, and keeps the low 8 bits. The reader reads the byte as signed (-128 to 127)
  and multiplies by 360 / 256, so it gives -180 to +178.59 degrees.
- **Player angles** (view angles; delta angles are kept in this form) **[read]**: a short. The
  writer takes degrees x 65536 / 360, truncates toward zero and keeps the low 16 bits; the reader
  reads a signed short and multiplies by 360 / 65536.
- **Directions** **[read]**: a byte index into a table of 162 unit normals. The writer picks the entry
  with the largest positive dot product with the direction; a missing direction, or one with no
  positive dot product with any entry, gives index 0. The reader drops the game for an index of 162
  or more. The table is the same 162 vertex normals the sequel's model format uses **[read]**.
  Measured on the table: every entry has length 1 within 5e-7; every entry's opposite is also in the
  table; each entry's nearest neighbour is 14.5 to 17.2 degrees away; 12 entries have five nearest
  neighbours and 150 have six **[measured]**. That is the vertex set of an icosahedron with each edge
  divided into four (10 x 4 x 4 + 2 = 162 vertices) projected onto the unit sphere **[derived]**.
  The exact values, and their order, cannot be regenerated from this description alone, so a
  byte-exact reader needs the table itself or a derivation checked against measured indices
  **[derived]**.

## 10. What a byte-exact reader must watch

- **The protocol number** **[read]**: the reader refuses a serverdata whose protocol is not 34 from
  a remote server, but accepts any protocol when the game itself is running the server, which is
  how recordings are played; so 3.14-era recordings play. The only layout difference the reader
  knows is protocol 26, whose frame header has no suppress count byte. Measured: the demo says
  protocol 31, and walking it with protocol 34's layouts consumes every block exactly, with no byte
  left over in any of the 640 blocks **[measured]**.
- **Field sizes that depend on flags**: the entity header's length (1 to 4 bytes) depends on its own
  "more bits" flags; the entity number's size on bit 8; frame, skin, effects and render effects
  sizes on their pairs of flags (skin, effects and render effects become a long when both flags
  are set) **[read]**.
- **Fields that depend on earlier state** **[read]**: every delta needs its base: the delta frame's
  entities and player state (frames are kept by number modulo 16) or the baselines. The old origin
  defaults to the base's origin, and the event resets to 0 when not sent. A packetentities list is
  merged with the old frame's list by entity number, so to reproduce bytes a writer must know which
  entities were in the old frame. The byte lengths alone never depend on earlier state, except the
  protocol number (frame header) and the temp entity kind (its payload) **[derived]**.
- **Orders that are not bit orders**: the entity fields (origin z with x and y, pitch before yaw),
  the playerinfo fields (gun index and frame before blend and field of view) and the sound fields
  (offset before entity and position) **[read]**.
- **Strings** **[read]**: the reader stops a string at a zero byte or at the end of the block, and
  keeps at most 2047 characters; a longer string would leave its remaining bytes to be read as the
  next messages. Configstring messages in a recording can be longer than one 64-byte slot (Q3).
- **No padding** anywhere: messages follow each other byte for byte, and a block ends exactly at its
  last message **[read]** **[measured]**.
- **Sizes** **[read]**: the largest message (and largest playable block) is 1400 bytes; a packet's
  reliable part is at most 1384 bytes; entity numbers are below 1024; model, sound and image indices
  are bytes (below 256); configstring indices are below 2080; there are 32 stats and 256 inventory
  slots.
- **Reading past the end** **[read]**: when a read runs past the block's end the reader returns -1
  for that field and carries on; a message id read past the end ends the block. A packetentities
  list that runs past the end drops the game.
- **Timing**: the server frame number times 100 gives the frame's server time in milliseconds
  **[read]**; the demo runs 632 frames, 63.2 s of play **[derived]**.
- **Write-side rules that make bytes reproducible** **[read]**: an entity is sent in a frame only when
  something in its delta is set (players always, Q4); the gun index flag is always set; the stats
  mask is always written; the end of the entity list is always two zero bytes; a remove entry for a
  number of 256 or more has a two-byte header (remove, more bits; number is a short). What the 3.14
  writer did differently (the old origin flag on baselines, Q4) is known only from the measurement,
  not from source.
