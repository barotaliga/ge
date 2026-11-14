# Answers: the sequel's demo variants beyond protocol 34 client recordings

Written by a clean-room reader. Sources read (current public releases, default branches):

- **q2pro** (GitHub q2pro/q2pro, head of 2025-12-11),
- **r1q2** (GitHub tastyspleen/r1q2-archive, head of 2011-09-12),
- **Yamagi Quake II** (GitHub yquake2/yquake2, head of 2026-11-11),
- **id's GPL release** (GitHub id-Software/Quake-2, the 3.21 code),
- **the 2023 re-release's open-sourced game code and its published protocol notes** (GitHub
  id-Software/quake2-rerelease-dll, head of 2023-10-03).

Measured: the full game's `demo1.dm2` (132 293 bytes) and `demo2.dm2` (140 969 bytes), both
protocol 26, and the demo release's `q2demo1.dm2` (138 511 bytes, protocol 31), walked byte by
byte with a script of the reader's own.

Every fact is marked **[read]** (the source states it), **[measured]** (counted in the recordings)
or **[derived]** (worked out from read or measured facts). Where nothing settles a question, the
answer says so. The answers assume the protocol 34 note already in the tree and do not repeat it.
The types are the ones that note uses: byte, char, short, long, coord, pos, angle8, angle16, dir,
string, all little-endian. Two more types appear here. "word" is an unsigned 16-bit value.
"int23" is the variable-length coordinate described in A.1.6.

---

## A. Protocol 35 (r1q2) and protocol 36 (q2pro)

### 2. Neither client writes its own protocol into a .dm2 (answered first, because it decides how much of question 1 matters)

- **q2pro's client never writes protocol 35 or 36 into a .dm2** **[read]**. It does not save the
  received packets. It saves each frame again from its parsed state, as a protocol 34 frame (header,
  playerinfo, packetentities, all in protocol 34 layout). The serverdata it writes says protocol
  34. If the connection's protocol is below 34, it writes that lower number; this can only happen
  while it re-records a protocol 26 demo during playback, and then it also leaves out the
  suppress-count byte. There is one exception. When the server runs with "extended limits" (the
  re-release game module on a q2pro server), the demo is written in q2pro's own extended demo
  protocol, numbered 3436 (see 1.7). No cvar selects 35 or 36 for a demo **[read]**.
- **q2pro's client refuses to play a .dm2 whose serverdata says 35 or 36.** It accepts 26 to 34 and
  3434 to 3436; any other number ends playback with "Demo uses unsupported protocol version"
  **[read]**.
- **r1q2's client also writes protocol 34** **[read]**. With a protocol 34 server it saves each
  received packet minus its 8-byte header, as id's release does. With a protocol 35 server it saves
  one block per received packet:
  - every non-frame message is copied byte for byte, in the order it was parsed,
  - in the frame's place it writes a protocol 34 frame rebuilt from its own state. That frame keeps
  the server's frame number and deltas from the last frame written to the demo, or carries -1
  when that frame is too old (16 or more frames) or its entities have been overwritten,
  - a block that would be empty, or that overflowed its 1400-byte buffer, is written as a single
  nop byte (id 6), so that one block is still written per packet.

  Its serverdata always says protocol 34, with 0x10000 plus the server count **[read]**. The code
  that would have written the protocol 35 serverdata fields is commented out **[read]**. Its
  playback accepts protocol 35 serverdata from a demo, because a demo's attract-loop byte is
  non-zero **[read]**. It refuses to start recording while it is playing a demo **[read]**.
- Both clients tell a protocol 35 or 36 server that a recording is in progress, with the "recording"
  client setting (setting 2, value 1 or 0). The server then stops leaving out data the client could
  predict: view angles, the player's own entity, gun and blend fields the player switched off
  **[read]**. q2pro also stops compressing large layouts while an old client records **[read]**.
- So a protocol 35 or 36 .dm2 is not produced by any of the clients read. Question 1 is answered
  for the wire messages, so that such a stream can be decoded if one is met (a third-party client,
  or a packet capture) **[derived]**.

### 1. How protocol 35 and 36 differ from 34

#### 1.1 New message ids

The ids continue after frame (20) **[read]**:

| id | r1q2 (35) | q2pro (36) | payload |
| --- | --- | --- | --- |
| 21 | zpacket | zpacket | word compressed length, word uncompressed length (at most 32 768), then the compressed bytes |
| 22 | zdownload | zdownload | as download (short size, byte percent); r1q2 adds a short uncompressed size before the data; then size bytes of compressed data |
| 23 | player update | gamestate | r1q2: long frame number, then one pos per player entity in that frame (see below). q2pro: configstrings then baselines (1.4) |
| 24 | setting | setting | long setting index, long value |
| 25 | - | configstring stream | 1.4 |
| 26 | - | baseline stream | 1.4 |

- **zpacket** **[read]**: the data is a raw deflate stream (no zlib header; window 15 bits). The
  bytes it inflates to are parsed as ordinary server messages. A zpacket inside a zpacket is
  refused. The server compresses a layout only when it is at least half the packet size, and the
  signon (configstrings, baselines) only when the client said it has zlib. A recording q2pro client
  writes the inflated messages into the demo one by one, never the zpacket itself.
- **zdownload** **[read]**: with protocol 35 each piece inflates on its own to the stated size. With
  36 the pieces form one continuous deflate stream and no size is sent. Neither is ever written to
  a demo.
- **player update (r1q2 only, 23)** **[read]**: extra player positions between frames. It carries no
  count. The reader walks the entities of its current frame, and for each one numbered at most max
  clients, other than the viewer, reads a pos (3 coords). So it cannot be parsed without the
  current frame's entity list **[derived]**. q2pro gives id 23 another meaning and rejects it on
  protocol 35 **[read]**.
- **setting (24)** **[read]**: index 0 is the player-update rate, index 1 the server's frames per
  second; q2pro uses the second to set its frame time. It is never copied into a demo.
- **Command byte high bits** **[read]**: ids fit in 5 bits. On 35 and 36 the top 3 bits of the
  command byte are used only on frame (20), where they carry player-state flags (1.3). On any other
  message, set high bits are an error.

#### 1.2 serverdata

The common part is unchanged: long protocol, long server count, byte attract loop, string game
directory, short player number, string level name **[read]**. Then:

**Protocol 35 (r1q2)** **[read]**:

| field | type | meaning |
| --- | --- | --- |
| enhanced | byte | must be 0 (q2pro refuses "enhanced" r1q2 servers) |
| minor version | short | 1903, 1904 or 1905 |
| (formerly advanced deltas) | byte | ignored |
| strafe-jump hack | byte | non-zero turns on the movement hack in prediction |

- r1q2's own client reads the last two bytes only for minor 1903 or later **[read]**.
- r1q2 servers report the highest minor they support, which may be above the one the client asked
  for. q2pro then clamps it into 1903 to 1905 and warns **[read]**.
- Minor versions **[read]**:
  - 1903 is the base,
  - 1904 changes only the client-to-server move encoding,
  - 1905 makes the entity solid a long (1.5).

**Protocol 36 (q2pro)** **[read]**:

| field | type | meaning |
| --- | --- | --- |
| minor version | short | 1015 to 1026 |
| server state | byte | 0 dead, 1 loading, 2 game, 3 picture, 4 broadcast (an MVD relay), 5 cinematic; used from 1019 on, read but ignored before |
| then, minor 1024 or later: protocol flags | short | bit 0 strafe-jump hack, bit 1 "QW" movement, bit 2 water-jump hack, bit 3 extended limits, bit 4 extended limits 2 (only together with bit 3) |
| or, minor below 1024: three bytes | byte, byte, byte | strafe-jump hack, QW movement, water-jump hack |

- A server sends server state 3 (picture) in place of 5 (cinematic) to clients older than 1023
  **[read]**.
- A player number of -1, or a server state of 3 or 5, means a cinematic or picture, not a level
  **[read]**.

What each q2pro minor version changes **[read]**:

| minor | change |
| --- | --- |
| 1015 | base: long solid, the 16-bit-mask rule (1.5) |
| 1016 | reserved, no format change |
| 1017 | beam old-origin rule (1.5) |
| 1018 | 16-bit angles for brush models (1.5) |
| 1019 | server state byte meaningful |
| 1020 | "extended layout": compressed layouts allowed while recording |
| 1021 | compressed downloads |
| 1022 | client number in the frame is a short (was a byte) |
| 1023 | cinematic server state |
| 1024 | protocol flags short; extended limits possible; signon by the configstring and baseline streams |
| 1025 | extended limits 2 possible |
| 1026 | player fog (a third player-flag byte) |

#### 1.3 frame (20)

**Protocol 35 and 36** **[read]**:

| field | type | meaning |
| --- | --- | --- |
| command byte | byte | low 5 bits 20; top 3 bits are extra player flags 4 to 6 |
| frame word | long | bits 0 to 26: server frame number; bits 27 to 31: how many frames back the delta frame is (31 means no delta; 0 to 30 means delta frame = frame number minus that value) |
| suppress byte | byte | low 4 bits: r1q2, the suppress count (0 to 15); q2pro, frame flags (bit 0 suppressed, bit 1 client dropped, bit 2 client predicted, bit 3 reserved). High 4 bits: extra player flags 0 to 3 |
| area bits length | byte | |
| area bits | bytes | |
| player state | see below | **no** playerinfo id byte before it |
| client number (36 only) | byte before minor 1022, short from 1022 | present only when extra flag 6 is set; otherwise the previous frame's value is kept |
| entity list | as protocol 34's packetentities list | **no** packetentities id byte before it |

- The extra player flags are 7 bits: bits 0 to 3 come from the suppress byte's high nibble, bits 4
  to 6 from the command byte's top 3 bits **[read]**. Their meanings **[read]**:

  | extra bit | meaning |
  | --- | --- |
  | 0 | gun offset follows |
  | 1 | gun angles follow |
  | 2 | velocity z follows |
  | 3 | origin z follows |
  | 4 | view angle roll follows |
  | 5 | stats follow |
  | 6 | client number follows (36 only) |

- The client number is the player whose view this is. A q2pro server sends it for chase cameras
  and MVD spectators; it can differ from the serverdata player number **[read]**.
- Because the delta is a 5-bit back-offset, it can only reach 30 frames back; frame numbers wrap
  at 2^27 **[derived]**.

**Player state on 35 and 36** **[read]**: a short of flags with the same bit numbers as protocol
34. On q2pro with player fog (1026, extended limits 2), a set bit 15 is followed by a byte of
flags 16 to 23; only flag 16, fog, is defined. r1q2 names bit 15 a bounding box, but its code for
it is commented out and it is never sent. The fields follow in this order:

| order | present when | field | type |
| --- | --- | --- | --- |
| 1 | flag 0 | movement type | byte |
| 2 | flag 1 | origin x, y | 2 shorts |
| 3 | extra 3 | origin z | short |
| 4 | flag 2 | velocity x, y | 2 shorts |
| 5 | extra 2 | velocity z | short |
| 6 | flag 3 | movement time | byte |
| 7 | flag 4 | movement flags | byte |
| 8 | flag 5 | gravity | short |
| 9 | flag 6 | delta angles | 3 shorts |
| 10 | flag 7 | view offset | 3 chars |
| 11 | flag 8 | view angles pitch, yaw | 2 angle16 |
| 12 | extra 4 | view angle roll | angle16 |
| 13 | flag 9 | kick angles | 3 chars |
| 14 | flag 12 | gun model index | byte |
| 15 | flag 13 | gun frame | byte (alone: offset and angles are not carried with it) |
| 16 | extra 0 | gun offset | 3 chars |
| 17 | extra 1 | gun angles | 3 chars |
| 18 | flag 10 | screen blend | 4 bytes |
| 19 | flag 16 | fog | 1.7 |
| 20 | flag 11 | field of view | byte |
| 21 | flag 14 | render flags | byte |
| 22 | extra 5 | stats | long mask, then a short per set bit |

- Unlike protocol 34, the stats mask is absent when extra flag 5 is clear (no stat changed)
  **[read]**.
- With extended limits (1.7) the same order holds, with wider field types **[read]**.
- A q2pro client that is not recording may get a player state with the view angles or the
  prediction fields left out, and its own entity with no origin or angles **[read]**. That is why a
  recording client says so (question 2).

#### 1.4 The protocol 36 signon

Which form the server uses **[read]**:

- before minor 1024: one gamestate message (23),
- from 1024: one or more configstring stream messages (25), then one or more baseline stream
  messages (26),
- to a client on the old packet format: plain configstring and baseline messages, as protocol 34.

Layouts **[read]**:

- **gamestate (23)**: repeated (word index, string), ended by a word equal to the number of
  configstring slots (2080, or 13 630 with extended limits). Then repeated baselines in the entity
  header and delta form, ended by an entity number of 0; the writer writes that end as two zero
  bytes.
- **configstring stream (25)**: the configstring half alone, with the same end marker. When a
  message fills up, the writer ends it with the marker and starts another message 25.
- **baseline stream (26)**: the baseline half alone, ended by two zero bytes. It is split the same
  way.

All of these are sent compressed (zpacket) when the client has zlib **[read]**. None of them ever
appears in a .dm2 that q2pro writes **[read]**.

#### 1.5 Entities on 35 and 36

The header (1 to 4 bytes, the more-bits chain, the number as byte or short) and the field order are
protocol 34's **[read]**. Differences:

- **Long solid** **[read]**: on r1q2 1905 and later, and on every q2pro connection, the solid field
  is a long, not a short. Its packing:
  - bits 0 to 7: half-width in whole units, 1 to 255,
  - bits 8 to 15: depth below the origin, 0 to 255,
  - bits 16 to 31: height above the origin plus 32 768.

  31 still means a brush model. A box whose packing happens to equal 31 is sent as 0. A q2pro client
  repacks long solids into protocol 34's 16-bit form when it writes a protocol 34 demo.
- **16-bit mask rule** **[read]**: skin, effects and render effects choose byte, short or long by
  their value. q2pro uses a long only when the value has a bit at 16 or above. To protocol 34 and
  35 clients it also uses a long when bit 15 is set, because old readers read the short as signed.
  The bytes differ, not the meaning.
- **Beam old origin** **[read]**: for an entity with the beam render effect, protocol 34 sends the
  old origin in every delta. Beam-aware connections (r1q2, q2pro 1017 and later) send it only when
  it changed. In either form, the reader must not overwrite a beam's old origin from the base state.
- **16-bit angles** **[read]**: on q2pro 1018 and later, header bit 13 (unused in 34) means "angles
  are angle16". The writer sets it only for brush models (solid 31) whose angles changed. With bit
  13, each present angle is a short; otherwise a byte as in 34.
- **Old origin rule** **[read]**: q2pro's writer sets the old-origin flag differently from id's (for
  new entities only when the old origin differs from the base's origin; always when the "frame
  lerp" render effect is set). The reader rule is unchanged: the old origin is taken from the base
  state unless the flag is present.
- r1q2's 35 entities differ from 34 only by the long solid at 1905 **[read]**.

#### 1.6 Other messages on 35 and 36

- sound, temp_entity, muzzleflash, print, configstring, layout, inventory and the rest keep their
  protocol 34 layouts **[read]**. The q2pro reader accepts temp entity kinds beyond 55 on any
  protocol:

  | kind | effect | payload |
  | --- | --- | --- |
  | 31 | rail trail 2 | two pos |
  | 56 | corrected blue hyperblaster | pos, dir |
  | 57 | BFG zap | two pos |
  | 58 | berserker slam | pos, dir |
  | 59 | grapple cable 2 | short entity, two pos |
  | 60 | power splash | short entity, byte |
  | 61 | lightning beam | short entity, two pos |
  | 62 | explosion 1 without light | pos |
  | 63 | explosion 2 without light | pos |
  | 128 | damage dealt | short |

  Because q2pro copies temp entities into its protocol 34 demos unchanged, a demo recorded on a
  server running a newer game can contain kinds a 3.21 client rejects **[derived]**.
- **The int23 coordinate** (extended limits 2 only) **[read]**: a word is read first.
  - When its bit 0 is clear, bits 1 to 15, sign-extended from 15 bits, are a delta added to the base
  value (in 1/8 unit). That form is used when the delta is between -16 384 and 16 383.
  - When bit 0 is set, one more byte is read as bits 16 to 23, and bits 1 to 23, sign-extended from
  23 bits, are the absolute value in 1/8 unit.
  - A pos written this way (temp entities, sound positions, old origin) always uses a base of 0.

#### 1.7 q2pro's extended demo protocol (3434, 3435, 3436)

This is the only non-34 protocol q2pro writes into a .dm2 **[read]**:

- It is written when the server used extended limits (3436 is always written); 3434 and 3435 are
  accepted on playback.
- The layout is protocol 34's (playerinfo and packetentities ids present, suppress byte present),
  with these changes **[read]**:
  - **Configstring layout**: 13 630 slots:
  - 0 to 58 misc, with the status bar program allowed to run from 5 to 58,
  - 59 air acceleration, 60 max clients, 61 map checksum,
  - models 62 to 8253 (8192),
  - sounds 8254 to 10 301 (2048),
  - images 10 302 to 12 349 (2048),
  - light styles 12 350 to 12 605,
  - items 12 606 to 12 861,
  - player skins 12 862 to 13 117,
  - general 13 118 to 13 629 (512).

  Derived by adding up the range sizes the source states. Slots are 64 bytes, as before.
  - **Entity header** (3434 on): a fifth header byte follows when bit 31 is set, giving bits 32 to
  39. The new bits:

  | bit | meaning |
  | --- | --- |
  | 28 | model indices are shorts |
  | 29 | "more effects", low size bit |
  | 30 | alpha byte (value / 255) |
  | 32 | scale byte (value / 16) |
  | 33 | "more effects", high size bit |

  Entity numbers go to 8191.
  - **Entity sound**: a word. Bits 0 to 13 are the sound index. Bit 14 means a loop-volume byte
  follows (value / 255), bit 15 a loop-attenuation byte (value / 64; 192 means "no attenuation").
  - **New entity fields** come after solid, in this order: more effects (sized like effects), alpha,
  scale. Solid uses the second long packing: half-width x, half-width y, depth below, and height
  above plus 32, one byte each.
  - **Player state** (3434 on): the gun model index is a short.
  - **More player state** (3435 on, "extensions 2"):
  - origin and velocity are int23 deltas,
  - movement time and movement flags are words,
  - stats are 64, with the mask written as a little-endian base-128 varint,
  - the blend is a flag byte (bits 0 to 3: blend channels present; bits 4 to 7: damage-blend
   channels present), then the bytes present,
  - entity origins are int23 deltas, and every pos is int23 from 0.
  - **Fog** (3436): a set player flag 15 adds a third flags byte. Flag 16 is fog: a flag byte, then
  colour 3 bytes (bit 0), density and sky factor 2 words (bit 1), height density word (bit 2),
  height falloff word (bit 3), height start colour 3 bytes (bit 4), height end colour 3 bytes
  (bit 5), height start distance int23 (bit 6), height end distance int23 (bit 7).
  - **Muzzleflash and sound**: in a monster muzzleflash, the short's top 3 bits are bits 8 to 10 of
  the flash number and the low 13 bits the entity. A sound's flag bit 5 means the index is a word.
  - **Prints**: print levels 4 (typewriter) and 5 (centre) print in the middle of the screen.

### Other facts about q2pro's .dm2 writer

These are not asked, but a byte-exact reader of q2pro demos needs them **[read]**:

- **Frames are renumbered.** The first written frame is numbered 1 and each later one is the
  previous plus 1, whatever the server's numbers were. The delta frame is the previous written
  frame (the first one says -1). The reason given: a frame numbered 0 cannot be used.
- **The server count is written as is**, not plus 0x10000 as id and r1q2 write it.
- **The demo buffer is 1390 bytes by default.** It is set by the `cl_demomsglen` cvar and clamped to
  512 to 32 768; `record -e` makes it 4086 and `record -s` 1390. Extended demos use 32 768. A frame
  that does not fit is dropped and counted. Other messages that do not fit are dropped and counted.
  The reliable messages, which come first in a packet, are the ones most likely kept.
- **Frames are written only on 10 Hz frames.** When the server runs faster than 10 frames per second,
  only every n-th frame, the one on the 10 Hz boundary, is written.
- **What the demo keeps.** Messages are copied in the order parsed; the frame takes the place where
  the server's frame was. Never copied: serverdata, frame, download, zpacket, zdownload, gamestate
  and the streams, setting.
- **The signon.** Recording starts at once from the current state; it does not wait for a full
  frame. The signon block (or blocks) holds a serverdata (attract loop 1), every non-empty
  configstring and every baseline, then the stufftext "precache" with a newline. The first frame
  after it has no delta frame.
- **Pausing.** Recording can be paused and resumed (`suspend`, `resume`). On resume the writer emits
  every configstring that changed meanwhile, then a frame. Pausing writes nothing to the file.
- `record -z` writes the file gzip-compressed (`.dm2.gz`); playback reads gzip transparently.
- The end is the same -1 length as id's.

---

## B. The 2023 re-release (KEX engine)

### 3. Protocol numbers and differences from 34

- **Numbers** **[read]**: the re-release game code declares three:
  - 34: the original 3.xx protocol,
  - 2022: the protocol "for demos",
  - 2023: the live protocol.

  The published notes say the server repeats the protocol number in its connect reply "mainly for
  backward compatibility with demos". The notes also mark two fields as different "in protocol
  2022", the one in the shipped demos. From both, the re-release's own .dm2 files carry 2022, and
  2023 is what a live connection uses **[derived]**. No re-release recording was available to
  measure, so the number actually written by its recorder is **open**.
- **Public documentation** **[read]**: the re-release's open-sourced game code ships notes that
  document protocol 2023's changes from the original. The engine itself is not open source. The
  game code also defines a configstring remap from the old layout to the new one, which the notes
  say "is used in our engine to provide old demo support". So the re-release plays old protocol 34
  demos by remapping configstring indices **[read]**.
- **No open client among those read implements 2022 or 2023** **[read]**. Neither q2pro nor Yamagi
  Quake II at the heads read has a parser for them. q2pro runs the re-release *game module* on its
  own server, with its own extended limits (1.7), but speaks its own protocols 34, 35, 36 and 3434
  to 3436 to clients. Whether some q2pro fork implements 2023 is not settled by what was read.

**Differences from 34, from the published notes and the game code's constants** **[read]**. The
notes give these as lists of reads; restated here in words.

- **Limits**:
  - 8192 entities (the game code's comment ties that limit to a 15-bit sound encoding, while the
  notes' large-entity sound form gives the entity 29 bits),
  - 8192 models, 2048 sounds (the notes' text says 1024 in one place and 2048 in another; the
  constant is 2048), 512 images, 256 light styles, 256 shadow lights, 256 items,
  - 256 clients, 512 general strings, 64 stats,
  - configstrings up to 96 characters (was 64), with a stated theoretical maximum of 32 767
  entries,
  - the server ticks at 40 Hz (25 ms) in place of 10 Hz.
- **Configstring layout** (in slots):
  - 0 name, 1 CD track, 2 sky, 3 sky axis, 4 sky rotation,
  - 5 to 58 status bar,
  - 59 air acceleration, 60 max clients, 61 map checksum,
  - models from 62, sounds from 8254, images from 10 302,
  - light styles from 10 814, shadow lights from 11 070, items from 11 326,
  - player skins from 11 582, general from 11 838,
  - then weapon-wheel weapons (32), wheel ammo (32), wheel powerups (32), CD loop count, game style.

  Derived by adding the stated range sizes to the stated start of models (62).
- **serverdata** adds a tick-rate byte after the attract-loop byte. The player number becomes a
  list: a first short of -2 means "split screen", and is followed by a short count, then that many
  shorts of entity numbers. Any other first value is the single player number; -1 is a cinematic.
- **frame**: long frame, long delta frame, byte suppress count. Then, once per split-screen player:
  area bits length byte, area bits, the playerinfo id, the player state. Then the packetentities id
  and the entity list. Entity events are cleared before the list is read.
- **Player state** flags:
  - a word of flags; when bit 15 is set, a second word gives flags 16 to 31,
  - flag 16 is damage blend, flag 17 team id.

  Fields, by flag (bit numbers as in protocol 34):

  | flag | field and type |
  | --- | --- |
  | 0 | movement type, byte |
  | 1, 2 | origin and velocity: full-precision positions (floats) |
  | 3, 4 | movement time and flags: words |
  | 5 | gravity: short |
  | 6 | delta angles: a full-precision position |
  | 7 | view offset: 3 shorts in 1/16 unit, then a char view height (the char is absent in 2022) |
  | 8 | view angles: a full-precision position |
  | 9 | kick angles: 3 shorts in 1/1024 degree |
  | 12 | gun index: a word; bits 13 to 15 are the gun skin, the rest the model |
  | 13 | gun frame: a word; bits 9 to 15 are 7 flags (offset x, y, z; angles x, y, z; gun rate), the rest the frame; each flagged offset or angle follows as a float, and the gun rate as a byte |
  | 10 | blend: 4 bytes |
  | 11 | field of view: byte |
  | 14 | render flags: byte |
  | always | stats 0 to 31: a long mask and shorts, then stats 32 to 63: a second long mask and shorts |
  | 16 | damage blend: 4 bytes |
  | 17 | team id: byte |

- **Entity bits**: protocol 34's, plus these:

  | bit | meaning |
  | --- | --- |
  | 28 | model indices are shorts |
  | 29 | effects are 64-bit |
  | 30 | alpha |
  | 31 | a fifth header byte follows |
  | 32 | scale |
  | 33 | instance bits (split-screen visibility) |
  | 34 | owner |
  | 35 | old frame |

  The field order is **not** protocol 34's:
  1. the four model indices (bytes, or shorts with bit 28),
  2. frame (byte or short),
  3. skin (byte, word or long),
  4. effects: with bit 29 a long of the low 32 bits comes first, then the usual byte, word or long,
  which holds the high 32 bits when bit 29 is set,
  5. render effects,
  6. solid, always a long, placed **before** the origin,
  7. origin x, y, z: floats, except that in 2022, when solid is 0, they are shorts in 1/8 unit,
  8. old origin as a position,
  9. the three angles as floats,
  10. sound as a word (bits 0 to 13 the index; bit 14: a loop-volume byte follows; bit 15: a
   loop-attenuation byte follows),
  11. event byte,
  12. alpha byte (value / 255),
  13. scale byte (value / 16),
  14. instance byte,
  15. owner short,
  16. old frame word.

  The solid long packs, one byte each: half-width x, half-width y, depth below, height above plus
  32. 31 still means a brush model.
- **Positions** (pos in temp entities, sounds and the rest) are full floats.
- **sound (9)**:
  - the flag bits gain bit 5 (an explicit position) and bit 6 (large entity),
  - the index is a word,
  - the entity-and-channel field is a long when bit 6 is set, else a short, with the channel in the
  low 3 bits,
  - the position flag is always set by the server.
- **New messages** (numbered by the game code's message list):

  | id | message | payload |
  | --- | --- | --- |
  | 21 | split client | byte: the split-screen player the following unicasts are for, plus 1 |
  | 22 | config blast | short compressed size, short uncompressed size, zlib data that inflates to repeated (word index, string) |
  | 23 | baseline blast | the same, inflating to spawnbaseline payloads |
  | 24 | level restart | repeated (short index, string), ended by index -1 |
  | 25 | damage | byte count, then per item a byte (bits 0 to 4 damage / 3, bit 5 health, bit 6 armour, bit 7 shield) and a dir |
  | 26 | localised print | byte flags, string base, byte argument count, that many strings |
  | 27 | fog | a flags byte and, when its bit 7 is set, a second byte; then the fields each flag selects |
  | 28 | waiting for players | byte count |
  | 29 | bot chat | string name, short client (256 for none), string |
  | 30 | point of interest | word key, word time, position, word image, byte palette index, byte flags |
  | 31 | help path | byte start, position, dir |
  | 32 | muzzleflash 3 | as muzzleflash2, but the flash number is a word |
  | 33 | achievement | string |

  The notes' headings number muzzleflash 3 and achievement both 32; the message list in the game
  code puts muzzleflash 3 at 32 and achievement at 33 **[derived]** from the list's order.
- **New enum values**:
  - muzzleflash: 19, 20, 31, 32,
  - monster flashes 211 to 255, and 256 to 288 through muzzleflash 3,
  - temp entities 31 (lighter rail trail), 56 to 63 (the same kinds as in 1.6's table), and splash
  colour 7 (electric),
  - print levels 4 (typewriter), 5 (centre), 6 (text to speech), plus flag bit 3 (broadcast) and
  bit 4 (no notify).
- **Out of band**: there are no challenges. The connect line carries the protocol, the number of
  split-screen players, a social id per player, and the userinfo in pieces of at most 510
  characters.

---

## C. Server-side recordings

### 4. The stock server's `serverrecord`

What the note already holds is not repeated. New facts:

- **Same in three releases** **[read]**: id's 3.21, r1q2 and Yamagi Quake II write the same layout.
  The serverdata says protocol 34 in all three. q2pro has removed the command (its MVD recording,
  question 5, replaces it).
- **Commands** **[read]**: `serverrecord <name>` writes `demos/<name>.dm2` under the game directory,
  and only while a level runs. `serverstop` closes the file. No end marker is written, so the file
  ends at its last block. Shutting the server down closes it the same way.
- **Which entities go into each frame** **[read]**: every entity in use, with a number, with a model,
  effects, a sound or an event, and not flagged "no client". Each is written as a delta from an
  all-zero state, sent only when some field is non-zero, with the old-origin flag.
- **What follows the entity list** **[read]**: everything the server multicast during that frame,
  appended byte for byte. That covers:
  - every temp entity and muzzle flash the game sends through multicast,
  - every sound,
  - every configstring change made after loading,
  - the server's broadcast stufftexts.

  It does **not** cover:
  - prints, which go to each client separately (so no obituaries or chat),
  - centre prints, layouts, inventories and other unicasts,
  - anything else addressed to one client.
- **Map change** **[read]**: a map change neither closes nor restarts the recording, and no second
  signon is written. Frames of the new map follow directly. Configstrings set while the new map
  loads are not multicast, so they are missing from the file. Only later changes appear.
- **Playback in stock clients** **[derived]**:
  - id's and Yamagi's clients reject the first block (above 1400 bytes).
  - q2pro's client takes a first block of up to 32 768 bytes. But a player number of -1 makes it
  treat the level name as a cinematic to play, and its frame reader then reads the long frame
  number and the packetentities id as a delta frame, so the frame is misparsed and play stops.
  - No client read plays a server recording.
- **What a server recording lacks for one player's view** **[derived]**: no player state (no view
  angles, view height, blend, gun, stats), no area bits, and the unicast text.
  - A player's position and angles could be taken from that player's entity (origin in 1/8 unit,
  angles as angle8).
  - Everything else would have to be made up or left at defaults.

### 5. q2pro's multi-view recording (MVD2, .mvd2) and its GTV stream

**File** **[read]**:

- The file starts with the 4-byte magic `MVD2` (0x4D 0x56 0x44 0x32).
- Then come blocks, each a word length (little-endian, 1 to 65 535; in practice under 32 768)
  followed by that many bytes. A length of 0 ends the file. `.mvd2.gz` is read gzip-compressed.
- q2pro's client, given a demo file whose first four bytes are the magic, hands it to the MVD
  player in place of the .dm2 player.
- The server writes it with `mvdrecord` (stopped by `mvdstop`), which needs `sv_mvd_enable`. The
  options are `sv_mvd_maxsize` (in units of 1000 bytes, 0 for no limit), `sv_mvd_maxtime`
  (minutes, counted as 600 frames each, 0 for no limit), `sv_mvd_maxmaps` (default 1) and
  `sv_mvd_autorecord`.
- A file can hold several levels: each new level starts with a new serverdata block.
- **Blocks**:
  - the first block is the gamestate,
  - after it, exactly one block per 10 Hz server frame,
  - a frame's block holds, in this order: the frame's "reliable" buffer, the frame message, the
  frame's "unreliable" buffer.

**Message ids** (5 bits; the top 3 bits of the command byte carry extra data where stated)
**[read]**:

| id | message | payload |
| --- | --- | --- |
| 0 | bad | - |
| 1 | nop | nothing |
| 2, 3 | disconnect, reconnect | reserved, never written |
| 4 | serverdata | below |
| 5 | configstring | word index, string |
| 6 | frame | below |
| 7 | frame without delta | reserved, never written |
| 8 | unicast | top bits: bits 8 to 10 of the length; byte length low bits; byte client number; then that many bytes of ordinary server messages |
| 9 | reliable unicast | as 8 |
| 10, 11, 12 | multicast to all, to the hearable set, to the visible set | top bits: length bits 8 to 10; byte length low bits; word leaf number (absent for "all"); then that many bytes of ordinary server messages |
| 13, 14, 15 | the same, reliable | as 10 to 12 |
| 16 | sound | top bit 5: no hearable-set culling; top bit 6: reliable. byte flags, byte sound index (a word when flag bit 5 is set on an extended stream), volume byte (flag 1), attenuation byte (flag 2), offset byte (flag 16), then always a short of entity times 8 plus channel. No position |
| 17 | print | byte level, string (a broadcast print) |
| 18 | stufftext | reserved, never written |

- Unicast and multicast payloads are at most 2047 bytes; the writer drops larger ones with a
  warning **[read]**.
- **Which buffer each message lands in** **[read]**:
  - Reliable multicasts are written into the frame's unreliable buffer, and unreliable multicasts
  into the reliable one, which looks inverted but is what the writer does. Configstrings, prints
  and reliable unicasts go to the reliable buffer; unreliable unicasts and sounds to the
  unreliable buffer.
  - So in a block, configstring changes, prints, reliable unicasts and unreliable multicasts come
  before the frame; reliable multicasts, unreliable unicasts and sounds come after it
  **[derived]**.
- **Unicasts are filtered before writing** **[read]**:
  - a stufftext is kept only when it starts with "play ",
  - layouts are kept only for the MVD's own observer,
  - with `sv_mvd_nomsgs` (default 1), prints to anyone but that observer are dropped,
  - data for players not in the game is dropped.

**serverdata (4)** **[read]**:

1. long major protocol, always 37,
2. word minor version: 2010 normally, 2013 with extended limits; 2009 to 2013 are accepted,
3. stream flags:
   - minor 2012 or later: a word after the minor,
   - otherwise: the top 3 bits of the command byte,
   - the flags: bit 0, other players' messages left out; bit 1, single point of view (a client's
  own stream); bit 2, extended limits; bit 3, extended limits 2,
4. long server count,
5. string game directory,
6. short client number of the MVD observer, or -1 for none,
7. configstrings: repeated (word index, string), ended by a word equal to the number of slots (2080,
   or 13 630 extended),
8. a baseline frame: the frame message's body (portal bits, players, entities) with no frame id.

- Each configstring slot may hold at most its size; an overflow is fatal **[read]**.
- The reader then reads max clients from the max-clients configstring (1 to 256) and loads the map
  named by the world model's configstring, for visibility only. It refuses the map's visibility
  when the map checksum differs from the checksum configstring **[read]**.

**frame (6)** **[read]**:

- **No frame number is sent**; frames are implicit, one per block, numbered by counting.
- The body:
  1. byte portal-bits length, then that many bytes of area-portal states (replacing protocol 34's
  area bits),
  2. the player list,
  3. the entity list.
- Everything is a delta from the previous frame of the same stream; the baseline frame is a delta
  from zero.

**Players in a frame** **[read]**:

- **Entry header**: a byte client number (0 to 254), a short of flags, and, on extended-limits-2
  streams of minor 2013 or later, a third flags byte when flag 15 is set. The list ends with
  client number 255.
- **Flags**:

  | bit | field |
  | --- | --- |
  | 0 | movement type, byte |
  | 1 | origin x, y: 2 shorts, or int23 deltas on extended-2 streams |
  | 2 | origin z |
  | 3 | view offset, 3 chars |
  | 4 | view angles pitch, yaw: 2 angle16 |
  | 5 | view roll, angle16 |
  | 6 | kick angles, 3 chars |
  | 7 | blend: 4 bytes, or q2pro's flag-byte form on extended-2 streams |
  | 8 | field of view, byte |
  | 9 | gun model index: byte, or short when extended |
  | 10 | gun frame, byte |
  | 11 | gun offset, 3 chars |
  | 12 | gun angles, 3 chars |
  | 13 | render flags, byte |
  | 14 | stats: long mask and shorts, or varint mask on extended-2 streams |
  | 15 | more flags |
  | 16 | removed |
  | 17 | fog (as in 1.7) |

- **Field order**: movement type, origin x y, origin z, view offset, view angles, roll, kick, gun
  index, gun frame, gun offset, gun angles, blend, fog, field of view, render flags, stats.
- **What is never sent**: velocity, movement time, movement flags, gravity and delta angles.
- **Removing a player**: the number, then the flags short with only bit 15. On streams with the
  third byte, that byte is 1 (flag 16). In older streams, and in any stream without the third byte,
  **bit 15 itself means "removed"**.
- **Sending**: a player is sent only when something changed, or in full (from the remembered state)
  when it re-enters.

**Entities in a frame** **[read]**:

- **Format**: protocol 34's entity header and fields, with these flags:
  - every stream: q2pro's 16-bit-mask rule and the beam rule,
  - extended: long solid in the second packing, 16-bit brush-model angles, the extended fields of
  1.7,
  - extended 2: int23 delta origins.

  Non-extended streams keep the 16-bit solid and byte angles. The list ends with two zero bytes.
- **Players' entities**: a player entity whose player is in the game with movement type normal is
  sent with **no origin and no angles**. The reader rebuilds them from that player's state:
  - origin = player origin / 8,
  - pitch = view pitch / 3, with pitch above 180 first wrapped to below 0,
  - yaw = view yaw,
  - roll = 0.

  The writer then keeps that origin as the base for the next delta.
- **Removing an entity**: a header with the remove bit, as in protocol 34.
- **The gamestate's entity list** may hold, for slots no longer in use, the slot's last state with
  the remove bit set **and** its fields. The reader parses an entry's fields even when the remove
  bit is set.
- **Which entities are in the stream**: those not flagged "no client" that have a model, effects, a
  sound or an event.
- **No visibility culling**: every such entity and every player is in the stream every frame.

**GTV stream** (TCP, the live form of the same data) **[read]**:

- **Magic**: both sides send the 4-byte magic `MVD2` first; the server echoes it.
- **Framing**: messages of a word length (counting the op byte), an op byte, then the payload.
  - A length of 0 ends the stream.
  - Client messages are at most 256 bytes; server messages at most 32 768.
- **Client ops**: 0 hello, 1 ping, 2 stream start, 3 stream stop, 4 string command.
  - The client hello: word protocol 0xED04, long flags (bit 0 deflate, bit 1 string commands), long
  reserved, string name, string password, string version.
  - Stream start: a short of how many messages may be buffered before a flush (at least 10).
- **Server ops**: 0 hello (long accepted flags), 1 pong, 2 stream start (ack), 3 stream stop,
  4 stream data, 5 error, 6 bad request, 7 no access, 8 disconnect, 9 reconnect.
- **Deflate**: when accepted, everything the server sends after its hello is one zlib stream (with
  header), flushed with sync flushes.
- **Stream data**:
  - The first stream-data message after a stream start carries the gamestate.
  - Each later one carries exactly the block a .mvd2 file would hold for that frame.
  - A stream-data message with no payload marks the stream as suspended (no active players).
- **A q2pro client can serve its own view** as a GTV stream with the single-point-of-view flag. It
  holds only that client's player and what that client received, multicasts written as "to all"
  **[read]**.

**How one player's view is extracted** **[read]**. q2pro's MVD player runs as a special server
(server state 4, broadcast) to which ordinary clients of protocols 34 to 36 connect and pick a
player to follow:

- **Player state**: the client gets a copy of the followed player's state, with:
  - movement type set to frozen and the "no prediction" flag added,
  - the field of view replaced by the spectator's own unless they ask for the player's,
  - optionally some stats taken from the MVD observer.

  With no one followed, the spectator flies freely, and the blend and underwater flag come from the
  map contents at the eye.
- **Entities** are culled for that viewer by the normal server visibility code. It uses the map
  loaded from the world model, the portal states of each frame and the viewer's origin. The
  followed player's own entity is hidden (model 0) from clients that cannot hide it themselves.
- **Multicasts** are forwarded to viewers whose position is in the visible or hearable set of the
  stored leaf (and whose area connects).
- **Sounds** are rebuilt as protocol 34 sound messages with an explicit position:
  - the position is the entity's origin, or the centre of its box for brush models,
  - they are hearable-set culled unless flagged otherwise,
  - reliable ones are sent reliably.
- **Unicasts** addressed to the followed player are forwarded to its followers:
  - configstrings at or above the general range are kept per player,
  - layouts are kept only for the observer,
  - prints are routed by the spectator's message level and mute settings,
  - stufftexts only when they start with "play ".
- Such a client can then record an ordinary protocol 34 .dm2 of that view with its normal `record`
  command **[derived]**.

### 6. Other common demo formats

- **r1q2**: no multi-view or multi-point-of-view format. Its server has only the stock
  `serverrecord`, and its client writes protocol 34 .dm2 (question 2) **[read]**.
- **Yamagi Quake II**: only the stock client recording (raw packets, serverdata protocol 34) and the
  stock `serverrecord` **[read]**.
- **q2pro**, beyond the .dm2 and .mvd2 above **[read]**:
  - its client writes gzip-compressed demos on request,
  - it keeps seek snapshots in memory while playing (every `cl_demosnaps` seconds, default 10). They
  are not a file format.
- **OpenTDM, Q2Admin and other "multi-POV" .dm2 tools**: not among the sources read; **open**.

---

## D. Protocol 26 (the full game's demos) against 34

### 7. Every difference, and the rewrite to 34

**What the sources say** **[read]**:

- **id's 3.21, Yamagi Quake II and q2pro know one layout difference only**: protocol 26's frame
  header has no suppress-count byte. Everything else is read with protocol 34's layouts.
- **Which clients play 26 directly**:
  - id's 3.21 and Yamagi accept any protocol number in a demo, because the game runs its own server
  while playing one,
  - q2pro accepts 26 to 34,
  - r1q2's reader takes any protocol from a demo (its attract-loop byte is non-zero).
- **So a stock protocol 34 client plays a protocol 26 recording as it is**, with no rewrite
  **[derived]**. A rewrite is only needed for a reader that insists on the number 34.

**What the two recordings show** **[measured]**:

- **The layouts match.** Walking demo1 and demo2 with protocol 34's layouts for every message, minus
  the suppress byte, consumes every block exactly: no byte left over or missing in any block.
  - demo1: 696 blocks, then the -1 end, then end of file; 4 × 697 + 129 505 = 132 293 bytes.
  - demo2: 625 blocks; 4 × 626 + 138 465 = 140 969 bytes.

  So the message ids, the serverdata, configstring, spawnbaseline, the entity header and field
  order, playerinfo with its field order and stats, sound, temp entity payloads, muzzleflashes,
  print and stufftext are byte-for-byte protocol 34's. That holds at least for every message kind
  and flag that occurs in them.
- **serverdata**:
  - demo1: protocol 26, server count 0x105C4, attract loop 1, empty game directory, player 0, level
  "Installation".
  - demo2: protocol 26, server count 0x15A36, attract loop 1, player 0, level "Pumping Station 1".

  The recorder added 0x10000 to the server count, as 3.21's does.
- **Signon**: demo1 has 8 signon blocks before the first frame (block 8, server frame 195, delta
  -1). demo2 has 8 too (first frame at block 8, server frame 82). The last signon block ends with
  baselines and the stufftext "precache" with a newline.
- **Frames**:
  - demo1 has 688 frames, server frames 195 to 882 consecutively; demo2 has 617 frames, 82 to 698.
  - Each later frame deltas from the one before.
  - The area bits are always 1 byte long.
  - Every playerinfo sets the gun model index flag, as 3.21's writer does.
- **Block sizes**: largest block 1370 bytes (demo1) and 1368 (demo2); smallest 72.
- **Message counts**:

  | message | demo1 | demo2 |
  | --- | --- | --- |
  | temp entity | 526 | 627 |
  | configstring | 331 | 323 |
  | baseline | 178 | 131 |
  | sound | 150 | 125 |
  | muzzleflash | 148 | 97 |
  | muzzleflash2 | 108 | 33 |
  | print | 1 | 1 |
  | stufftext | 1 | 1 |

  No layout, inventory, centerprint, nop, download, disconnect or reconnect occurs.
- **Kinds that occur**:
  - temp entities: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 14,
  - sound flag bytes: 8, 10, 14, 15,
  - muzzleflash weapons: 1, 2, 3, 4, 5, 6, 13,
  - monster flash numbers: 26, 39 to 42, 45 to 56, 61, 88, 94, 97, 99, 100.
- **Old origin on baselines**: 128 of 178 baselines in demo1 and 103 of 131 in demo2 carry the
  old-origin flag. Like 3.14's, these recorders did not set it on every baseline.
- **Configstring indices are protocol 34's**:
  - model 1, the world, at slot 33,
  - sounds from 288, images from 544, light styles from 800, items from 1056, player skins from
  1312,
  - the map checksum at 31,
  - the status bar program in 5, running on through 9.
- **Slots 29 (air acceleration) and 30 (max clients) are absent** from both protocol 26 recordings,
  the protocol 31 one has slot 30. A 3.21 client takes an empty max-clients string as single player,
  as it does "1" **[read]**.
- **Player sounds are named by full path** ("player/male/death1.wav"). The protocol 31 recording
  uses the "*death1.wav" form that a 3.21 client resolves per player model. Both forms load in a
  3.21 client, the first one as an ordinary sound **[derived]**.
- **Item order differs**: in the item slots (1056 and up) the 26 recordings list "Grenade
  Launcher" at 1068 where 31's has "Grenades" (one position shift from there on). The image
  numbers differ too. A recording is self-consistent, because its configstrings travel with it
  **[measured]**, so this matters only to a writer that hard-codes item or image numbers
  **[derived]**.
- **Movement flags never show "on ground"**:
  - protocol 26: the movement-flags byte only ever takes 1 (ducked, while dead or crouched, view
  offset z -7 or -8) and 0,
  - protocol 31: it takes 4, 5, 20 and 0, so "on ground" (4) is set most of the time.

  So in protocol 26 the on-ground bit, and the landing timer, are not sent. A 34 client playing
  back forces movement to frozen, so this has no visible effect **[derived]**.
- **Unsettled**: whether the numbering of monster flashes, temp entity kinds, effects and render
  effects bits is the same in 26 as in 34. The bytes fit 34's payloads exactly, and the source read
  has only 34's tables. **Open**, beyond the payload fit.

**The rewrite to 34** **[derived]** from the above:

1. In the serverdata, change the long 26 to 34. The block length is unchanged.
2. In every frame message, insert one byte 0 (suppress count) directly after the delta-frame long,
   and add 1 to that block's length prefix. Frames sit at the start of a block or after a reliable
   message (a print in demo1's block 653 and demo2's block 584 precede their frames, as in 3.14's
   recording), so the walk must parse every message to find the frame.
3. Nothing else changes byte-wise. The largest block grows to at most 1371 bytes, under the 1400
   limit.
4. Optional: add configstring 30 "1" (and 29 "0") to the signon. A 3.21 client does not need them.

**Cross-check of the existing note** **[measured]**: walking q2demo1 with the same script gives
entity flag counts over baselines and frames that differ from the note's for some bits. For
example, model index: 262 here against 83 there, and 262 = 83 + 179 baselines; render effects high
bit: 59 here against 0 there. The note's counts for those bits appear to leave out the baselines or
some entries. The byte-exact walk itself agrees: every block is consumed exactly.
