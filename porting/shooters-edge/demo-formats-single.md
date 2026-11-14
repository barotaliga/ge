# Answers: netgame demos (.dem) beyond protocol 15, and Dzip

Reader's answer document. Facts only, in the reader's words. Every fact carries a tag:

- **[read]**: settled by reading a named project's current public source.
- **[measured]**: observed by running a program or parsing a public file in the scratch area.
- **[derived]**: worked out from read or measured facts; the reasoning is given.
- **[doc]**: taken from public prose documentation (cited).

All multi-byte numbers are little-endian. "i16" is a signed 16-bit integer, "u16" unsigned, "i32"
signed 32-bit, "f32" an IEEE 754 single, "string" a NUL-terminated byte string, "byte" 8 bits
unsigned, "char" 8 bits signed.

## What was read and measured

| Project | Release read |
| --- | --- |
| Quakespasm | main branch at 2026-10-29, version 0.97.0 |
| Ironwail | master at 2026-09-10, version 0.8.3-dev |
| vkQuake | master at 2026-11-07 |
| DarkPlaces | master at 2026-11-05 |
| ProQuake | the unofficial public mirror (QuakeEngines), version 4.93, last commit 2020-08-08 |
| Dzip | the maintained public repository (kugelrund), version 3.2, last commit 2025-08-04 |

FitzQuake itself was not available as a current public release; its protocol 666 is described
here as the three FitzQuake descendants above implement it. They agree on every 666 fact below
except where a difference is named. RMQ's own engine was not read; 999 is described as the same
three descendants implement it.

Public documentation read: the Steam Community guide "[2021 Re-Release] Netcode" by JPiolho
(posted 12 Dec 2021, updated 20 Aug 2022), and the Steam discussion thread for app 2310 about
recording demos in the re-release.

Samples measured (all public, from the Speed Demos Archive's Quake pages, fetched into scratch):
six .dz archives (one run of ad_blasphemy, four runs of 2024 jam maps and one of dope_spoot4).
They were extracted with Dzip 3.2 built from source in scratch, and every message of two of them
was parsed by a scratch parser written from the facts in this document. Synthetic demos were also
made in scratch and fed to Dzip as a black box.

---

## A. Protocol 666 and 999

### 1. Every difference from protocol 15 in the server messages

**The protocol number.** The server info message (11) starts with an i32 protocol number: 15, 666
or 999 [read]. A client that knows 666 and 999 refuses any other number. The same number also
appears in the version message (4, an i32) when one is sent [read].

**Server info (11).** The fields are the same as in protocol 15, with one addition [read]:

1. i32 protocol (15, 666 or 999).
2. Only when the protocol is 999: a u32 protocol-flags word (see question 2). Protocols 15 and 666
   carry no flags word.
3. byte maxclients (the client refuses anything outside 1..16).
4. byte game type (0 coop, 1 deathmatch).
5. string level name.
6. model names, one string each, ended by an empty string. Model slot 1 is the world.
7. sound names, the same way.

Limits [read]: a 666 or 999 server may list up to 4096 models and 2048 sounds in these clients
(the receiving client's own limits; it warns past 256 and, for models, past 2048). A protocol-15
server sends at most the first 255 names of each list.

The text printed just before the server info names the server engine, but only as a hint [read]
[measured]. The FitzQuake line prints a version and the progs CRC. vkQuake prints its own engine
name and version. ProQuake prints a boxed "ProQuake Server Version" banner. All six measured
speed-run demos carried the classic "VERSION 1.09 SERVER (n CRC)" line, the 666 one included.

**New message ids in 666** (they are also used in 999) [read]:

| Id | Name a mapper or modder knows it by | Layout after the id byte |
| --- | --- | --- |
| 37 | skybox | string: the sky name. An empty string clears it |
| 40 | bonus flash ("bf") | nothing. It flashes the screen as for an item pickup |
| 41 | fog | byte density, byte red, byte green, byte blue, i16 fade time in hundredths of a second. That is 6 bytes of payload, 7 with the id. Each byte is divided by 255. A negative time counts as 0 |
| 42 | spawn baseline 2 | i16 entity, byte flags, then as for baseline (below) |
| 43 | spawn static 2 | byte flags, then as for baseline (below) |
| 44 | spawn static sound 2 | 3 coords origin, u16 sound index, byte volume, byte attenuation |

Notes on the table:
- The fog layout is the one all three clients read. An older informal description says that fog's
  last field is an f32. It is not: the clients read an i16 [read]. None of the three servers sends
  fog, skybox or bonus flash on its own; game code (QuakeC) writes them byte by byte [read]. So
  what a demo holds is whatever that game code wrote.
- The ids 35, 36 and 38, 39 are unused by 666 [read]. Nehahra's own protocol-15 messages use
  35..37 (show picture, hide picture, skybox). Of those, only 37 matches 666's skybox.

**Baseline layout (22, 42; static 20, 43)** [read]:

- Spawn baseline (22): i16 entity, then the body with flags 0.
- Spawn baseline 2 (42): i16 entity, byte flags, then the body.
- Spawn static (20): the body with flags 0, with no entity number.
- Spawn static 2 (43): byte flags, then the body.

The body is: model index (u16 when flag bit 0, "large model", is set, otherwise a byte), frame
(u16 when flag bit 1, "large frame", is set, otherwise a byte), byte colormap, byte skin, and then
three pairs of (coord, angle) in the order x, y, z. Then a byte alpha when flag bit 2 is set, then
a byte scale when flag bit 3 is set.

Which form a server picks [read]:
- It sends form 42 only when at least one flag is set; otherwise it sends the plain 22. It does the
  same for statics.
- A 666 server sets the scale flag on baselines never. A 999 server sets it when the entity's
  scale differs from 1.
- In protocol 15 the server sends only the plain forms. Before sending, it zeroes any model or
  frame number above 255.

Values carried by baselines and updates [read]:
- **Alpha byte**: 0 means "default" (opaque, except water, which follows the client's water-alpha
  setting). 1 means invisible. 255 means opaque. Any other value v means opacity (v − 1) / 254.
  The server encodes an alpha a in (0, 1] as round(a × 254 + 1), clamped to 1..255.
- **Scale byte**: the scale times 16, so 16 means 1.0. A scale of 0 in the game code is sent as 16.
  The default when no scale is sent is 16.

**Entity update (id byte with bit 7 set).** The first byte is bits 0..6 of the update's bit set,
with bit 7 as the marker [read]. Bit 0 says a second byte follows; it carries bits 8..15. In 666
and 999, bit 15 says a third byte follows (bits 16..23), and bit 23 says a fourth (bits 24..31).
The bytes come in that order, right after the first.

In protocol 15, bit 15 has no continuation meaning. A client that sees it in a 15 stream treats it
as Nehahra-style transparency (below) [read]. Measured: one of the five protocol-15 speed-run
demos used it 7,150 times.

Bit assignments [read]. The low 15 bits are the same as in protocol 15:

| Bit | Meaning |
| --- | --- |
| 0 | more bits (a second byte follows) |
| 1, 2, 3 | origin x, y, z |
| 4 | angle y (yaw) |
| 5 | "step" (no interpolation of movement; a step-moving monster) |
| 6 | frame |
| 8, 9 | angle x (pitch), angle z (roll) |
| 10 | model |
| 11 | colormap |
| 12 | skin |
| 13 | effects |
| 14 | long entity (the entity number is an i16, otherwise a byte) |
| 15 | 666 and 999: extend 1 (a third byte follows). Protocol 15: Nehahra transparency |
| 16 | alpha, byte |
| 17 | frame high byte |
| 18 | model high byte |
| 19 | lerp finish, byte |
| 20 | scale, byte |
| 21, 22 | unused |
| 23 | extend 2 (a fourth byte follows; no bits are defined in it) |

Field order after the bit bytes [read] [measured on the 666 sample, 60,674 blocks parsed with no
error]:

1. The entity number (a byte, or an i16 when bit 14 is set).
2. model (byte), frame (byte), colormap (byte), skin (byte), effects (byte).
3. origin x, angle x, origin y, angle y, origin z, angle z. Coords and angles are in the stream's
   encoding (question 2).
4. In 666 and 999 only: alpha, scale, frame high byte, model high byte, lerp finish, one byte each
   in that order. Note that scale (bit 20) comes before the two high bytes (bits 17 and 18).

What the extra 666 fields mean [read]:
- **Frame and model high bytes**: these are bits 8..15 of the frame or model number. The low byte
  comes from the ordinary frame or model field. Servers send a high byte only together with its
  low-byte bit, and only when the high byte is non-zero.
- **Values not sent**: alpha and scale fall back to the baseline's value. Frame and model fall
  back to the baseline's full 16-bit value. A high byte that is not sent leaves the low-byte
  result as it is.
- **Lerp finish**: a byte b means the entity's next animation step is due b / 255 seconds after
  the update's time. The server writes round((next think − server time) × 255). It sends lerp
  finish whenever the entity's think interval is non-standard. When the field is absent, the
  client assumes 0.1 s.
- **Scale in 666**: the 666 servers send scale in updates whenever it differs from the baseline's
  scale. Because 666 baselines never carry scale, a scaled entity in a 666 demo has the scale
  field in every update [read]. vkQuake says so outright: it compares against 1.0 in 666 and
  against the baseline in 999.

Nehahra transparency in a protocol-15 stream (bit 15) [read] [measured]: three or two f32s follow
the angle fields.
- The first f32 is a mode: 1 or 2.
- The second f32 is the alpha in 0..1. The client converts it to the alpha byte as above.
- Only when the mode is 2, a third f32 follows: a fullbright value.

Other facts about updates [read]:
- An entity whose alpha is "invisible" (1) and which has no effects is not sent at all.
- Entity numbers are limited by the client's entity limit, which is far above 255 in all three
  clients. Updates of entities above 255 always use the i16 form.
- The 666 sample shows how common the wide fields are on a big map [measured]. Of about 18.9
  million updates, 15.8 million carried the model high byte (that map has more than 255 models),
  459,245 carried alpha and 10,101 carried lerp finish. None carried scale or frame high bytes.

**Client data (15).** In protocol 15 the bit set is a u16 [read]. In 666 and 999 the same u16
comes first. Bit 15 then says a byte with bits 16..23 follows, and bit 23 says a byte with bits
24..31 follows. Both bytes come right after the u16.

Bits 0..14 are unchanged from protocol 15:

| Bit | Meaning |
| --- | --- |
| 0 | view height (char; the default is 22 when absent) |
| 1 | ideal pitch (char; 0 when absent) |
| 2, 3, 4 | punch x, y, z (char each) |
| 5, 6, 7 | velocity x, y, z (char each, times 16) |
| 8 | unused |
| 9 | items present (the items word is read whether or not this bit is set) |
| 10 | on ground (no data) |
| 11 | in water (no data) |
| 12 | weapon frame (byte) |
| 13 | armour (byte) |
| 14 | weapon model (byte) |
| 15 | 666 and 999: extend 1 (bits 16..23 follow) |

The new bits in 666 and 999 [read]. Bits 16..22 and 24 are each the high byte (bits 8..15) of a
value whose low byte is in the protocol-15 part:

| Bit | High byte of, or meaning |
| --- | --- |
| 16 | weapon model |
| 17 | armour |
| 18 | current ammo |
| 19 | shells |
| 20 | nails |
| 21 | rockets |
| 22 | cells |
| 23 | extend 2 (bits 24..31 follow) |
| 24 | weapon frame |
| 25 | weapon alpha (byte, same encoding as entity alpha; "default" when absent) |
| 26..30 | unused |
| 31 | extend 3 (reserved; no further byte is read) |

Field order [read] [measured]:
1. view height, ideal pitch.
2. Then, for each axis x, y, z in turn: punch, then velocity. So punch x, velocity x, punch y,
   velocity y, and so on.
3. i32 items (always present).
4. weapon frame, armour, weapon model (each when its bit is set).
5. i16 health, byte current ammo, byte shells, byte nails, byte rockets, byte cells, byte active
   weapon. These are always present.
6. Then, in 666 and 999, the high bytes in this order: weapon model, armour, ammo, shells, nails,
   rockets, cells, weapon frame. Then weapon alpha.

What the servers do [read]:
- The server sets each high-byte bit only when that high byte is non-zero.
- Bit 25 is set when the player entity's alpha is not "default".
- Quakespasm's server always sets bit 14 (weapon model). Measured: it was present in every client
  data message of both parsed samples.

**Sound (6).** The first byte after the id is a field mask [read]:

| Bit | Meaning |
| --- | --- |
| 0 | volume byte follows (the default is 255) |
| 1 | attenuation byte follows (the value is the byte / 64; the default is 1.0) |
| 2 | "looping": named in the protocol but carries nothing in these engines' sound message |
| 3 | 666 and 999: large entity |
| 4 | 666 and 999: large sound |

The fields follow in this order:
1. Volume, then attenuation, when their bits are set.
2. The entity and channel:
   - Without large entity (as in protocol 15): one u16. The top 13 bits are the entity number and
   the low 3 bits the channel (0..7).
   - With large entity: a u16 entity number and then a byte channel.
3. The sound index: a u16 with large sound, otherwise a byte.
4. Three coords: the sound's origin, which is the entity's bounding-box centre.

When the server sets the large bits [read]: large entity when the entity number is 8192 or more,
large sound when the sound index is 256 or more. In protocol 15 the server drops such a sound
rather than send it.

Measured masks in the 666 sample: 0, 2, 3, 16, 17, 18 and 19. Large sound was used; large entity
was not.

**Other messages whose size depends on the coordinate encoding** [read]: the set-angle message
(10, three angles), particle (18: three coords, three chars of direction × 16, byte count, byte
colour), damage (19: byte armour, byte blood, three coords), temp entities (23), static sound (29:
three coords, byte sound, byte volume, byte attenuation), and the sound origin. In 666 these are
the same as protocol 15 (16-bit coords, 8-bit angles); only 999 changes them.

Temp entity sizes after the id and type bytes, with c the size of one coord [derived from the
layouts the clients read; agrees with the measured parse]:
- Types 0..4, 7, 8, 10 and 11 (spikes, gunshot, explosions, wizard and knight spikes, lava splash,
  teleport): 3 × c.
- Types 5, 6, 9 and 13 (the three lightning bolts and the beam): an i16 entity plus 6 × c.
- Type 12 (explosion 2): 3 × c plus a byte colour start and a byte colour length.

**2021 re-release message ids that 666 and 999 clients now recognise** (see question 4) [read]:
- **Achievement (52)**: the client reads a string and ignores it.
- **Local sound (56)**: a byte mask, then a sound index (a u16 when mask bit 4 is set, otherwise a
  byte). The sound is played to this client only. The Quakespasm and Ironwail servers write 56
  when game code asks for a local sound.
- Every other re-release id is an unreadable message to these clients, and playback stops with an
  error.

### 2. Protocol 999's flags and which are used in recorded demos

The flags word follows the protocol number in the server info message, only when the protocol is
999 [read]. Its bits:

| Bit | Value | Meaning |
| --- | --- | --- |
| 1 | 2 | short angles: angles are i16, value × 360 / 65536 degrees |
| 2 | 4 | float angles: angles are f32 degrees (this wins over short angles) |
| 3 | 8 | 24-bit coords |
| 4 | 16 | float coords: coords are f32 |
| 5 | 32 | edict scale (listed as supported by the clients, but no client behaviour depends on it, and no server sets it) |
| 6 | 64 | "alpha sanity" (not supported by the three clients; they warn) |
| 7 | 128 | 32-bit integer coords: an i32 in units of 1/16 |
| 31 | — | "more flags" (reserved, not supported) |

How each coord encoding is read and written [read]:
- **Precedence**: float coords, then 32-bit integer coords, then 24-bit coords, then protocol 15's
  16-bit coords.
- **Protocol 15's coord**: an i16 in units of 1/8, so a range of ±4096 units. The writer rounds
  value × 8 to the nearest integer.
- **32-bit integer coord**: the writer rounds value × 16. Reading divides by 16.
- **24-bit coord**: an i16 holding the integer part, then a byte holding the fraction × 255. The
  reader adds byte / 255 to the i16. The writer takes the integer part by truncation, and the
  fraction byte as the remainder of truncate(value × 255) divided by 255. So for negative values
  the two halves do not combine into the original value; no current server picks this encoding.
- **8-bit angle**: the writer rounds value × 256 / 360 and keeps the low 8 bits. The client reads
  the byte as signed, so the angle runs from −180 to just under 180 degrees.
- **16-bit angle**: the same at 65536 steps.

Which flags servers actually send [read]:
- **Quakespasm**: defaults to protocol 666. Under 999 it sends flags 130 (32-bit integer coords
  plus short angles).
- **Ironwail**: defaults to protocol 999 with flags 130.
- **vkQuake**: defaults to protocol 999. With its protocol extensions on (the default), it sends
  flags 18 (float coords plus short angles); with them off, flags 130.

So the flag values a demo reader will meet in practice are 130 and 18 [derived from the defaults].
No public 999 demo was found to measure: all six speed-run archives were protocol 15 or 666.
Derived: the speed-run archive requires Dzip, and Dzip refuses 999 (question 7).

### 3. Other demo-level differences

**The block layout and the header line are unchanged** [read] [measured]:
- The file starts with a text line holding the forced CD track, written as a decimal integer and
  "\n". It is "-1" when no track was forced (all six samples: "-1\n").
- Then come blocks, each an i32 message length, three f32 view angles (pitch, yaw, roll, in
  degrees) and the message bytes.

Reading the header [read]: the three clients read an integer and then require the very next byte
to be "\n". Anything else (for example a space before the newline) makes them refuse the file as
invalid.

Block length limit [read]: the three clients stop with a fatal error when a block's length is over
64,000 bytes (protocol 15's original limit was 8,000). A block that ends early (a truncated file)
ends playback quietly. The largest measured block in the 666 sample was 16,215 bytes.

**What a recording contains when it starts mid-game** [read]. The three clients keep a copy of
every message received before the level finished loading (signon stage 2). When "record" is typed
during a game, they write:
1. Those kept messages, one block each.
2. Then one made-up block holding, in this order:
   - for every player slot: name (13), frags (14) and colours (17),
   - all 64 light styles (12),
   - the four statistics total secrets, total monsters, secrets found and monsters killed, as
   update-stat messages (3),
   - set view (5),
   - signon number 3 (25).

vkQuake instead rebuilds the server info and the baselines from its current state, so its
mid-game recordings start with a server info message it wrote itself [read].

**End of a recording** [read]: "stop" writes a final block whose message is a single disconnect
(2), with the current view angles.

**vkQuake's protocol extensions** [read]: vkQuake can put extension words before the protocol
number in server info. That is an i32 marker followed by an i32 extension mask, repeated:
- the marker is the four bytes "FTEX" (value 0x58455446) for the first extension set, or "FTE2"
  (0x32455446) for the second,
- when the second set includes its "prediction info" extension, a game-directory string follows
  the 999 flags word, before maxclients.

Its demos record whatever was negotiated, so a vkQuake demo can begin its server info with "FTE2"
rather than 15, 666 or 999. With "replacement deltas" negotiated, entities then come in extension
messages (ids 66, 78, 79, 86 and others) instead of the classic updates. A reader that wants
vkQuake demos must at least recognise the markers. This document does not cover the extension
messages' layouts (open).

---

## B. The 2021 re-release

### 4. The re-release's protocol, its differences, and public documentation

**Recording** [doc, second-hand]: the re-release's own developer is quoted, in a Steam discussion
for the game, as saying that recording was removed from the re-release. It is "currently
impossible to play back our own demos", because the netcode changed for split screen. The
re-release plays demos back but cannot record them. Derived: no demos recorded by the re-release's
own engine exist in normal use. Re-release .dem files in circulation are therefore recordings made
by other engines.

**Protocol number**: open. The source read does not settle it, and no re-release-recorded demo
exists to measure. The community guide does not give the number either.

**Public documentation** [doc]: the Steam Community guide "[2021 Re-Release] Netcode" (JPiolho,
Dec 2021 / Aug 2022). Its author says it may be incomplete or wrong. It says:
- The netcode is based on the classic one, adds messages, and wraps the UDP traffic in DTLS 1.2
  with a pre-shared key (OpenSSL). That is the network, not a demo.
- Its "WriteCoord" is listed as a 4-byte float, where the classic one is a 2-byte integer.
  Derived: that matches a float-coordinate stream, not protocol 15's.
- Server-to-client message ids 0..34 are as in protocol 15. The re-release then adds or names:

| Id | Name in the guide | Layout given in the guide |
| --- | --- | --- |
| 37 | skybox | (not given) |
| 38 | bot chat | byte player, i16 string count, then that many strings. The first string is a format in which "{}" is replaced by the following strings |
| 39 | spawned monster | (not given) |
| 40 | bonus flash | (not given) |
| 41..44 | fog, baseline 2, static 2, static sound 2 | (not given; same ids as 666) |
| 45 | set views | (not given) |
| 46 | update ping | (not given) |
| 47 | update social | (not given) |
| 48 | update player info | (not given) |
| 49 | raw print | (not given) |
| 50 | server vars | (not given) |
| 51 | sequence | (not given) |
| 52 | achievement | (the 666 clients read one string) |
| 53 | chat | byte name colour (0 white, 1 red, 2 light blue, 3 yellow), byte message colour (0 white, 1 light blue), string name, string message. The guide says it appears to work only in single player |
| 54 | level completed | (not given) |
| 55 | back to lobby | (not given) |
| 56 | local sound | (the 666 clients read a byte mask and a byte or u16 sound) |
| 57 | prompt | byte choice count n, string prompt text, then n pairs of (string choice text, byte impulse) |

The guide names more differences:
- **Centre print (26)** in the re-release is not a bare string. It is an i16 string count and then
  that many strings, with the same "{}" replacement as bot chat. Derived: a 666 reader would
  misparse a re-release centre print.
- **Entity update**: the guide gives three or four bit bytes. Bits 0..15 are as in the classic
  update, and bit 15 continues to the third byte as in 666. In the third byte: bit 16 alpha, bit 17
  frame (the guide is unsure), bit 18 model index (unsure), bit 19 next think (unsure), bit 21
  "origin offset" (the guide says this makes the origins i16 instead of floats), bit 22 solid, bit
  23 more. In the fourth byte: bit 24 "flags", bit 25 "health" and bit 26 "model indexes", each
  "appears to send multiple values".
- **Field order in the guide**: model, frame, colormap, skin, effects, origin x (f32, or i16 with
  the origin-offset bit), angle x, origin y, angle y, origin z, angle z (angles as one byte each),
  then alpha, frame, model, next think and solid (one byte each), then the fourth byte's fields
  (layouts unknown).
- The guide also lists two client-to-server ids, 5 and 6 (the first "appears to be sent every
  frame"). They are not in demos.

The 666 clients' notes add one thing [read]: id 52 is also an extension id in another family
(DarkPlaces' "effect"), and id 56 is DarkPlaces' "spawn static 2". So the meaning of 50..62 depends
on which family the stream is.

**Achievements, localised strings, bot data**: bot chat, chat, prompt, achievement and centre
print with replacement strings are listed above. Localised text is carried as strings: Ironwail,
which loads the re-release's localisation table, looks up any string that begins with "$" as a
key [read], and the guide's "{}" placeholders are filled from the following strings [doc]. That is
a fact about the strings, not about the wire layout. Bot data: only bot chat (38) is documented. Open beyond the guide.

### 5. Whether open clients play re-release demos, and what they skip

[read] Quakespasm, Ironwail and vkQuake accept only protocols 15, 666 and 999 in server info.
vkQuake also accepts its own extension markers. Of the re-release's ids they handle only two:
- 52 (achievement): the string is read and ignored, with a developer-only console note.
- 56 (local sound): the sound is played.

Ids 38, 45..51, 53..55 and 57 make playback stop with an "Illegible server message" error, which
names the previous message. These clients do not read the string-count form of centre print
either.

So none of them plays a stream recorded from a re-release server [derived]. Since the re-release
cannot record demos (question 4), the practical case is different: an open client records a demo
while running the re-release's game data and its game code. Such a demo is ordinary 15, 666 or
999 and can contain ids 52 and 56. Those ids are written by game code or by the local-sound call
[read]. Other open clients play such a demo only when they know 52 and 56, and Dzip does not know
either of them (question 7) [measured].

---

## C. Other .dem variants

### 6. ProQuake and DarkPlaces

**ProQuake** (read: the 4.93 mirror). Common today: yes, in classic NetQuake multiplayer
communities. It is less common in single-player and speed-run archives, where measurement found
none: the six speed-run demos all printed the classic server line.
- **Protocol**: it stays protocol 15. The serverinfo protocol number is 15, and every message has
  protocol 15's layout [read].
- **The server banner** before server info is two print messages drawing a box with "ProQuake
  Server Version 4.93" (the version varies) [read]. That is a hint, not a rule: recordings made on
  other servers by a ProQuake client show the other server's line.
- **Its in-band extensions** ride inside the stuff-text message (9). When the string's first byte
  is 1, the client reads a ProQuake sub-message from it [read]:
  - the second byte is a command: 1 nop, 2 new team, 3 erase team, 4 team frags, 5 match time,
   6 match reset, 7 ping times,
  - small numbers are encoded so that no byte is 0: a team number is sent as team + 16; a "PQ
   byte" is two bytes, each one 16 + one nibble (value = 16 × first + second − 272); a "PQ
   short" is two PQ bytes, high first,
  - the ping list is PQ shorts of (slot × 4096 + ping), ended by a zero short.
  The text is still a valid string and is also fed to the console, so a plain protocol-15 reader
  parses it without error [read].
- **"Precise aim"** (16-bit view angles) is in the client-to-server move message only. It never
  appears in a demo [read].
- **Size of the difference for a reader**: none at the byte level. The rest is optional meaning
  inside stuff-text [derived].

**DarkPlaces** (read: master). It recognises these protocol numbers in server info [read]:
- 15 ("QUAKE"; also its own "QUAKEDP" variant),
- 96 and 97 (DP1, DP2),
- 3500..3505 (DP3..DP8),
- 250 (the Nehahra movie protocol),
- 10000..10002 (three "BJP" variants),
- 28 (QuakeWorld, which DarkPlaces also plays).

Its server defaults to DP7 (3504) [read].

How a reader tells them apart: by the protocol i32 in the first server info. 3500..3505 and 96,
97 are DarkPlaces; 250 and 10000..10002 are Nehahra-movie and "BJP" streams [read].

Encodings [read]:
- DP1 uses f32 coords and 8-bit angles. DP2, DP3 and DP4 use i16 whole-unit coords and 8-bit
  angles.
- DP5 and later use f32 coords and i16 angles (× 360 / 65536).
- From DP1 on, entities come as frame-based delta messages, the "entities" message (57), rather
  than classic updates. There are three frame layouts: one for DP1 to DP3, one for DP4 and one for
  DP5 and later. The DarkPlaces protocols use the 50..62 id range for their own messages: download data,
  update stat byte, effect, effect 2, precache, baseline 2, static 2, entities, CSQC entities,
  static sound 2, trail particles and point particles.
- vkQuake's notes say that DarkPlaces sends some of these extension ids even under protocol 15
  [read, from vkQuake].

The demo file wrapper is the same as protocol 15's: the CD-track line and then the length, three
angles and message blocks [read].

Size of the difference: large. Every DarkPlaces protocol needs a separate entity parser; the rest is new
messages and wider fields [derived]. DarkPlaces' entity-frame layout is not covered here (out of
the question's "short summary"). How common [derived, not measured]: DarkPlaces demos come mainly
from the separate games built on that engine. For Quake itself they are uncommon in the archives
sampled (none of six).

**Nehahra-style transparency in protocol 15** (see question 1) appeared in a measured 2024
speed-run demo [measured]. Bit 15 was set on 7,150 entity updates, each followed by two or three
f32s. A protocol-15 reader therefore needs that branch.

---

## D. Dzip

### 7. The archive layout, the demo transform and its inverse, versions, licence

#### 7.1 The archive

**Header** (12 bytes, at offset 0) [read] [measured]:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | 2 bytes | "DZ" |
| 2 | byte | major version (1, 2 or 3; measured 3) |
| 3 | byte | minor version (measured 0, 1 and 2) |
| 4 | u32 | offset of the directory |
| 8 | u32 | number of directory entries |

A reader refuses the file in these cases [read]:
- the file is shorter than 12 bytes,
- the magic is wrong,
- the major version is above its own (3),
- any of the top five bits of the entry count is set,
- the directory offset is not inside the file,
- the entry count times the entry size exceeds the file size.

**Directory entries** follow one after another at the directory offset. Each is 32 bytes (24 in
major version 1, which lacks the last two fields), followed by the name [read] [measured]:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | offset of this file's data in the archive |
| 4 | u32 | stored size (the compressed bytes) |
| 8 | u32 | original size |
| 12 | u16 | name length, counting the terminating NUL (refused above 257) |
| 14 | u16 | pak link: 0 for a plain file. For a pak, its member count. For a pak member, its 1-based index within the pak |
| 16 | u32 | checksum of the original bytes |
| 20 | u32 | type |
| 24 | u32 | file date and time |
| 28 | u32 | intermediate size: the length of the byte stream before zlib (for a demo, the transformed stream) |
| 32 | name | the name, with its NUL |

**Types** [read]:

| Value | Type |
| --- | --- |
| 0 | ordinary file |
| 1 | version-1 demo (refused in archives of major version 2 or later) |
| 2 | text (used only by version 1) |
| 3 | pak |
| 4 | dz (not written by the current encoder) |
| 5 | demo |
| 6 | Nehahra demo (decoded the same way as 5) |
| 7 | directory |
| 8 | stored |

When a file is marked Nehahra [read]:
- the demo contains message 35, 36 or 37 under protocol 15, temp entity type 17, or entity bit 15
  (transparency) under protocol 15,
- because 666 reuses 37 as skybox, a 15 demo with a skybox message is also marked 6,
- measured: a 2024 protocol-15 speed-run demo was type 6 because of its transparency bits.

The other fields [read] [measured]:
- **Checksum**: the reflected CRC-32 of the original bytes, with polynomial 0x04C11DB7 and initial
  value 0xFFFFFFFF, with no final inversion. That equals the bitwise complement of the usual CRC-32
  (the zlib value). Measured on two files of one archive: the stored values were exactly the
  complements of the zlib CRC-32 of the extracted files.
- **Date**: MS-DOS packing. The low 16 bits are the time: hour in bits 11..15, minute in bits
  5..10, seconds / 2 in bits 0..4. The high 16 bits are the date: years since 1980 in bits 9..15,
  month in bits 5..8 counted from 0 (January = 0, one less than true DOS), day in bits 0..4.
  Measured on two archives: a listed date of 20 Feb 2022 stores month 1, and a file made on
  13 Nov 2026 stores month 10.
- **Data**: each file's data is its own complete zlib stream (with the zlib header and Adler-32
  trailer; header bytes 78 9C measured), made at the default level. Inflating "stored size" bytes
  from the data offset gives "intermediate size" bytes. For an ordinary file, those bytes are the
  file. For a demo, they are the transformed stream below.
- **Stored files**: when compression would have made a file larger than its original, the encoder
  stores the original bytes as they are, with type 8 and stored size equal to original size.
- **Directories**: type 7 entries have sizes and checksum 0. Extraction creates the directory.
- **Version 1**: an archive of major version 1 has a different whole-archive layout (one zlib
  stream for everything before the directory) and a different demo transform. Not covered here
  (open); version 1 dates from June and July 2000 [read].
- **Names**: the encoder stores names with the separators of the system it ran on. Extraction
  turns "\\" into "/" (and the reverse on Windows), except inside paks.

**Paks** (type 3) [read]:
- A pak entry is followed directly by its member entries, which carry pak index 1..n. The pak
  entry's original size is the size of the rebuilt pak; its stored size spans all the members'
  data.
- Extraction rebuilds the pak in this layout:
  1. the 4 bytes "PACK",
  2. a u32 directory offset, equal to the pak's original size − 64 × n,
  3. a u32 directory size, 64 × n,
  4. each member's data in index order from offset 12,
  5. the 64-byte entries, each a 56-byte name, a u32 offset and a u32 length.
- Derived: the pak comes back with its members packed in directory order and the directory at the
  end. A pak that was laid out differently is not rebuilt byte for byte. Each member's checksum
  still holds.
- A demo inside a pak is transformed like a demo file.

**Which files are treated as demos** [read]: any file whose extension is ".dem", case-insensitive,
provided it passes the header test:
- the file must be longer than 13 bytes,
- within its first 12 bytes it must hold only digits and "-" before a "\n".

A file that fails the test becomes an ordinary file (type 0).

#### 7.2 The intermediate stream of a demo

[read; every statement below is the decoder's behaviour, checked against the encoder]

**Overall order:**
1. The CD-track line, copied byte for byte, up to and including its "\n". The decoder looks for
   the "\n" within the first 12 bytes and calls the stream corrupt without it.
2. Then a run of **encoded blocks**, one per .dem block, in the same order.
3. Then, optionally, a **raw tail**. It takes the place of an encoded block and runs to the end of
   the intermediate stream. It is a sequence of chunks, each the byte 0xFF, a u32 length L (at
   most 131,067) and L bytes copied to the output as they are. The encoder writes a raw tail when
   it meets anything it cannot reproduce (7.4). The tail then holds the rest of the .dem from the
   start of the failing block, including that block's length and angles. A raw tail ends the file:
   nothing encoded follows it.

**An encoded block** is:
1. A byte of angle flags. Bits 0, 1 and 2 say whether a delta follows for view angle 0, 1 or 2.
2. One i32 per flagged angle.
3. The block's messages, encoded as below.
4. A terminating byte 0.

The block's output is the .dem block: its i32 length, then the three angle words, then the
messages. The length is the byte count of the decoded messages.

**View angles**: each .dem angle is kept as its raw 32-bit pattern, as an integer, not as a
float. The block holds the difference from the previous block's pattern, modulo 2^32, only when it
is non-zero. The decoder adds the delta to a running pattern per angle. All three start at 0 at
the start of the file.

**Message dispatch inside a block**, by the first byte b:

| b | Meaning |
| --- | --- |
| 0x30..0x37 (top five bits 00110) | the frame's entity group (7.3) |
| 1..44 | a message with that id, handled per id (below). 35 has a Dzip meaning |
| 0x40..0x4F | client data (7.3) |
| 0x38..0x3F, or 0x60..0x7F | sound (7.3) |
| 0x80..0xFF | an entity update copied as it is from the .dem (7.3, "verbatim updates") |
| anything else | corrupt |

The checks run in that order. The encoder writes exactly 0x30 for an entity group.

**Messages kept byte for byte, by fixed total length including the id** [read; the lengths agree
with protocol 15 and 666 layouts with 16-bit coords and 8-bit angles]:

| Id | Message | Total bytes |
| --- | --- | --- |
| 1 | nop | 1 |
| 2 | disconnect | 1 |
| 3 | update stat | 6 |
| 4 | version | 5 |
| 5 | set view | 3 |
| 10 | set angle | 4 |
| 14 | update frags | 4 |
| 16 | stop sound | 3 |
| 17 | update colours | 3 |
| 18 | particle | 12 |
| 19 | damage | 9 |
| 20 | spawn static | 14 |
| 21 | spawn binary | 1 |
| 24 | set pause | 2 |
| 25 | signon number | 2 |
| 27 | killed monster | 1 |
| 28 | found secret | 1 |
| 29 | static sound | 10 |
| 30 | intermission | 1 |
| 32 | CD track | 3 |
| 33 | sell screen | 1 |
| 41 | fog | 6 (see the note below) |
| 44 | static sound 2 | 11 |

Messages kept byte for byte, with a variable length:
- **String messages**: the id and one string, for 8 print, 9 stuff text, 26 centre print, 31
  finale, 34 cutscene, 36 (Nehahra hide picture) and 37 (skybox).
- 12 light style and 13 update name: the id, a byte and a string.
- **11 server info**: copied as it is. The decoder notes the protocol number from it (15 and 666
  accepted; 999 or anything else is an error in the decoder). Its layout is assumed to be the
  protocol-15 one with no flags word. Server info also resets the "previous baseline index"
  (below) to 0.
- **43 spawn static 2**: 15 bytes, plus one for each of the flag bits large model, large frame
  and alpha.
- **23 temp entity**: the length is set by the type byte, at a 16-bit coord size:
  - 8 for types 0..4, 7, 8, 10, 11, 14 and 15,
  - 16 for types 5, 6, 9 and 13,
  - 10 for type 12,
  - 14 for type 16,
  - for type 17 (a Nehahra type), the string's length plus 17.
  The encoder gives up on types above 17.
- **38** decodes as Nehahra's "show picture" and is written out with id 35: the id, two strings,
  then 2 bytes. The encoder moves show picture from 35 to 38 because 35 is taken by the long time
  (below).
- Ids 39 and 40 have no handler in either direction (7.4).

**Time (7 and 35):**
- The game time is kept as a raw 32-bit pattern (the f32's bits as an integer), with a running
  value that starts at 0 for the file and is never reset.
- **Encoded id 7**: a u16 delta follows, zero-extended. The decoder adds it to the running value.
- **Encoded id 35** ("long time"): an i32 delta follows. The decoder adds it modulo 2^32.
- Both are written out as id 7 and the 4-byte pattern.
- The encoder uses the short form when the difference, modulo 2^32, is below 65,536; otherwise it
  uses the long form. A time that goes backwards always takes the long form.

**Spawn baseline (22 and 42)**: see 7.3.

#### 7.3 The transformed messages

Dzip keeps a model of every entity. All integer fields are compared exactly:
- model (16 bits), frame (16 bits), colormap, skin, effects,
- three angle bytes,
- three origins, as the raw i16 from the stream, in 1/8 units,
- three "velocities": the last change of each origin,
- a "no lerp" bit (entity bit 5),
- 666 alpha, scale and lerp-finish bytes,
- Nehahra alpha (f32) and fullbright (byte),
- a set of "force" bits,
- an "active" flag.

Three copies of this model are kept for each entity number below 32,768:
- **baseline (B)**: set by spawn baseline messages,
- **previous (O)**: the last frame written out,
- **current (N)**.

There is also an ordered set S of entity numbers that are "in the frame".

Start of the file:
- every B and O is all zero,
- S is empty,
- the previous client data has view height 22, items 0x4001 and every other field 0,
- the running time and angles are 0,
- the previous baseline index is 0,
- the protocol is taken as 15 until a server info is seen.

Derived: start each file at protocol 15 here; the decoder read keeps the last protocol from the
previous file.

**End of a block that contained any spawn baseline**: after the block is written, every entity's
O and N are set to its B. S is not changed.

**Spawn baseline, encoded id 22** (the protocol-15 form, entity below 1,024):
- A u16 X follows. Its low 10 bits are the entity number's step from the previous baseline index,
  modulo 1,024. Add it to the previous index modulo 1,024; the result is the entity number, and
  becomes the new previous index.
- Bits 10..15 of X are flags:

| Bit of X | Field present |
| --- | --- |
| 10 | frame |
| 11 | colormap |
| 12 | skin |
| 13 | origin: three i16 |
| 14 | angle 1 |
| 15 | angles 0 and 2 |

- Then come the model byte (always), then each present field in this order: frame, colormap,
  skin, origins 0, 1, 2, angle 1, angle 0, angle 2. A field that is absent is 0.
- Output: id 22, the i16 entity number, model, frame, colormap, skin, then origin 0, angle 0,
  origin 1, angle 1, origin 2, angle 2 (origins as i16). That entity's B becomes these values,
  with everything else zero.

**Spawn baseline, encoded id 42** (666, or a protocol-15 baseline for an entity of 1,024 or more):
- An i16 entity number (absolute).
- A flag byte. When its bit 0 is set, a second byte follows with bits 8..15. The flags:

| Bit | Meaning |
| --- | --- |
| 2 | frame present |
| 3 | colormap present |
| 4 | skin present |
| 5 | origin present |
| 6 | angle 1 present |
| 7 | angles 0 and 2 present |
| 8 | "large entity": the original was a plain 22 |
| 9 | large model |
| 10 | large frame |
| 11 | alpha present |

- Then the fields: the model (an i16 when "large model", otherwise a byte), the frame (likewise
  with "large frame", only when present), colormap, skin, origins, angle 1, angles 0 and 2, alpha.
- Output when "large entity" is set: a plain id 22, the i16 number and the protocol-15 body.
- Otherwise: id 42, the i16 number, then a flag byte made of large model (1), large frame (2) and
  alpha (4), then the 666 body. That is model (2 or 1 bytes), frame (2 or 1), colormap, skin,
  origin 0, angle 0, origin 1, angle 1, origin 2, angle 2, then the alpha byte when flagged.
- This form does not change the previous baseline index. The 666 scale flag on baselines (bit 3)
  is not handled; the 666 servers never set it (question 1).

**Client data (encoded 0x40..0x4F)**: a delta from the previous client data.

The encoded first byte holds bits 0..3 of a mask whose bits 4..7 are the identifier. Further
bytes follow: bit 3 brings the second byte (bits 8..15), bit 15 the third (bits 16..23), bit 23
the fourth (bits 24..31) and bit 31 the fifth (bits 32..39).

Then the fields follow in this order. "δ" means a char added to the previous value:

| Mask bit | Field |
| --- | --- |
| 0 | velocity 2, δ |
| 1 | velocity 0, δ |
| 2 | velocity 1, δ |
| 8 | weapon frame, δ (bit 29: an i16, the absolute value) |
| 9 | on ground: flips |
| 10 | punch 0, δ |
| 11 | ammo, δ (bit 30: an i16 absolute) |
| 12 | health: an i16 added |
| 13 | items: a u32 XORed in |
| 14 | armour, δ (bit 32: i16 absolute) |
| 16 | ideal pitch, δ |
| 17 | shells, δ (bit 33) |
| 18 | nails, δ (bit 34) |
| 19 | rockets, δ (bit 35) |
| 20 | weapon model, δ (bit 36) |
| 21 | active weapon, δ |
| 22 | in water: flips |
| 24 | view height, δ |
| 25 | cells, δ (bit 37) |
| 26 | punch 1, δ |
| 27 | punch 2, δ |
| 28 | "items bit absent": flips |
| 38 | weapon alpha, δ |

How the deltas work:
- Byte fields wrap modulo 256.
- For the 16-bit fields (weapon frame, ammo, armour, shells, nails, rockets, weapon model, cells),
  the absolute form and the δ form use the same place in the order; only one of the two bits is
  set. With δ, if the previous value's high byte is non-zero the result is the full 16-bit sum,
  otherwise the result is kept to 8 bits.

Then, optionally, a force record: a byte with top nibble 0x5 (0x50..0x5F). When its bit 3 is set,
a second byte follows (bits 8..15). The bits 0, 1, 2 and 8..15 of this value are XORed into the
client-data force set:

| Force bit | Field |
| --- | --- |
| 0, 1, 2 | velocity 0, 1, 2 |
| 8, 9, 10 | punch 0, 1, 2 |
| 11 | view height |
| 12 | ideal pitch |
| 13 | weapon frame |
| 14 | armour |
| 15 | weapon model |

The force set persists from message to message.

Output: a standard client data message rebuilt from the current values.
1. The items-present bit (9) is set unless "items bit absent" is set.
2. Each optional byte field is written, and its bit set, when its low byte differs from its
   default or its force bit is set. The defaults are view height 22 and 0 for everything else.
   The order is view height, ideal pitch, punch 0, velocity 0, punch 1, velocity 1, punch 2,
   velocity 2.
3. Then the i32 items.
4. The on-ground and in-water bits.
5. Weapon frame, armour and weapon model (low bytes, under the same rule).
6. The i16 health, then the bytes ammo, shells, nails, rockets, cells, active weapon (low bytes).
7. Then each 666 high byte, written when non-zero: weapon model, armour, ammo, shells, nails,
   rockets, cells, weapon frame. There is no force for these.
8. Then weapon alpha, written when it differs from the weapon alpha of the previous client data
   written.
9. The extend bits 15 and 23 are set when bits 16..23 or 24..31 are in use, and their bytes are
   placed after the u16.

**Sound (encoded 0x38..0x3F or 0x60..0x7F)**: the encoded byte b replaces both the id and the
mask.
- The real mask's bits 0..2 are b's bits 0..2.
- Large entity is set when b's bit 3 is clear, and large sound when b's bit 4 is clear. In the
  0x38 form both bits are set, so neither applies.
- Then the protocol layout follows, with two changes:
  - **Channel**: in the small-entity form the low 3 bits of the u16 store (2 − channel) modulo 8.
   The same operation restores the channel.
  - **Origin**: when the entity number is below 32,768, each origin i16 is stored as its
   difference, modulo 2^16, from that entity's current origin in the model. That is the origins
   written by the most recent entity group, or the baseline's after a baseline block. The decoder
   adds them back.
- Output: id 6, the restored mask, then the fields.
- An id 6 itself followed by a mask is also accepted and handled the same way.

**The entity group (encoded 0x30)**: all the classic entity updates of one .dem block, as one
frame.

Step 1, the activation list. Bytes follow until a 0:
- A byte 0xFF adds 254 to a running base, which starts at 0.
- Any other byte v names entity (base + v). It is marked active and inserted into S when it is
  not there already.

Step 2, the records. For each entity in S, in ascending order:
1. First add each velocity to its origin in N. This is done for every entity in S, whether or not
   a record follows.
2. An entity that is not active has no record. Skip it.
3. Otherwise read a record byte m:
   - m = 0x80: the entity left the frame. Set O and N to B, remove it from S, and write nothing
   for it.
   - m = 0x00: the entity becomes inactive. It stays in S with its predicted values.
   - Any other value: bit 7 brings a second byte (bits 8..15), and bit 15 a third (bits 16..23).
   Then the fields in this order, with δ a char and each delta relative to O:

| Record bit | Field |
| --- | --- |
| 0 | origin 2 small: the velocity becomes O's velocity + δ, and the origin becomes O's origin + that velocity |
| 11 | origin 2 absolute: an i16; the velocity becomes the new origin − O's origin |
| 1 | origin 1 small |
| 10 | origin 1 absolute |
| 2 | origin 0 small |
| 9 | origin 0 absolute |
| 3 | angle 0, δ |
| 4 | angle 1, δ |
| 5 | angle 2, δ |
| 6 | frame = O's frame + 1 |
| 8 | frame, δ (16-bit or 8-bit result, by the client data rule) |
| 22 | frame absolute, an i16 |
| 12 | effects, absolute byte |
| 13 | model low byte |
| 21 | model high byte |
| 14 | no-lerp bit flips |
| 16 | colormap, byte |
| 17 | skin, byte |
| 18 | protocol 15: Nehahra alpha, an f32. Otherwise: the 666 alpha byte |
| 19 | protocol 15: Nehahra fullbright, a byte. Otherwise: the 666 scale byte |
| 20 | lerp finish, a byte |

   The encoder sets bit 0 with a zero δ when the first byte would otherwise be empty. A zero δ
   leaves the prediction unchanged.

Step 3, the force records, each optional:
- **Byte 0x31**: records of a u16 each, ended by a zero u16 (whose 2 bytes are consumed).
  - The entity is the low 10 bits.
  - When bit 15 is set, a byte follows with bits 16..23.
  - When the protocol is not 15 and bit 23 is set, a byte follows with bits 24..31.
  - The value without its low 10 bits is XORed into that entity's force set.
- **Then byte 0x32**: records of a u16, then a byte (bits 16..23). When bit 23 is set, a byte with
  bits 24..31 follows. When the protocol is not 15 and bit 31 is set, a byte with bits 32..39
  follows. The records end with a zero u16. The entity is the low 15 bits, and the force bits are
  the rest of the value shifted down by 8.

The entity force bits:

| Bit | Field |
| --- | --- |
| 10 | origin 1 |
| 11 | angle 0 |
| 12 | angle 1 |
| 13 | angle 2 |
| 14 | frame |
| 16 | origin 0 |
| 17 | origin 2 |
| 18 | model |
| 19 | colormap |
| 20 | skin |
| 21 | effects |
| 22 | long entity |
| 23 | protocol 15: Nehahra transparency |
| 24 | alpha |
| 25 | scale |
| 26 | lerp finish |
| 27 | model high byte |
| 28 | frame high byte |

The continuation bits (15, and 23 in 666) are XORed in too, and have no effect.

Derived edge: the encoder writes a force section's terminator only when the first forced entity
of that section has a number above 0.

Step 4, the output. For each entity left in S, in ascending order, write one classic update
against its baseline B:
1. The entity number: an i16 with bit 14 when the number is above 255 or "long entity" is forced,
   otherwise a byte.
2. Model low byte, frame low byte, colormap, skin, effects: each when it differs from B or is
   forced.
3. Origin 0 (i16), angle 0, origin 1, angle 1, origin 2, angle 2, under the same rule.
4. Under protocol 15 with Nehahra transparency forced: the f32 mode (2 when fullbright is
   non-zero, else 1), the f32 alpha, and, in mode 2, the f32 (fullbright − 1). Bit 15 is set.
5. Then the 666 fields:
   - alpha when it differs from B or is forced (not in protocol 15),
   - scale under the same rule,
   - frame high byte, then model high byte: each when it differs from B's high byte and is
   non-zero, or is forced,
   - lerp finish when it differs from B or is forced.
6. Bit 5 when the no-lerp bit is set.
7. The bit bytes: bit 0 when any of bits 8..23 is used, and bit 15 when any of bits 16..23 is
   used. Then 1, 2 or 3 bit bytes, with bit 7 always set in the first.
8. Afterwards, O becomes N.

**Verbatim updates (0x80..0xFF)**: the update is copied byte for byte. Its length is worked out
from its own bits by the protocol rules:
- the bit bytes (in 666, bit 15 brings a third),
- the entity number (1 or 2 bytes),
- one byte each for model, frame, colormap, skin and effects,
- two per origin and one per angle,
- one each for alpha, scale, frame 2, model 2 and lerp finish.

The decoder refuses a verbatim update with Nehahra transparency under protocol 15. The encoder
writes the group (step 1 onwards) at the first non-entity message after the block's first entity
updates. It writes verbatim any later update in the same block, and any update whose entity number
is not above the previous update's. Because of that, a block holds at most one group.

#### 7.4 What the encoder cannot reproduce, and what happens then

When the encoder meets any of the following, it prints a warning and writes the rest of the file,
from the start of the current .dem block, as the raw tail [read; marked measured where tested].
The archive stays lossless; only the transform stops.
- A server info with protocol 999 [measured: a warning that protocol 999 is not supported yet, and the file round-tripped byte for byte] or any protocol other than 15 or 666.
- An unknown or zero message id. This includes the re-release's 52 [measured with a synthetic
  file: it round-tripped].
- A temp entity type above 17.
- A Nehahra transparency mode other than 1 or 2.
- An entity update using the fourth bit byte (bit 23).
- 666 client data whose weapon, armour or weapon-frame high byte is sent as 0.
- 666 client data carrying weapon alpha [derived: the encoder keeps weapon alpha in an 8-bit
  field after shifting it 8 bits up, so it always reads back 0 and always counts as "unchanged",
  which is one of its refusal conditions].
- A .dem block longer than 65,524 bytes, or an encoded block over 65,536 bytes.
- A truncated file.

Fog is mis-sized: Dzip takes fog (41) as 6 bytes in total, one less than the 7 that the 666
clients read (question 1). Measured with a synthetic 666 demo holding a 7-byte fog: the encoder
reported "bad message encountered" and stored the rest raw; the file round-tripped. Derived: a
real demo with fog loses the transform from that block on, but no data.

In the encoder, ids 38, 39 and 40 have no handler. Measured: a synthetic 666 demo holding a bonus flash (40) made
the Dzip 3.2 encoder crash (segmentation fault). It left an empty, invalid archive.

Not reproduced exactly [derived]: the decoder writes a 666 high byte of ammo, shells, nails,
rockets or cells only when it is non-zero, but the encoder does not refuse a zero one. A demo that
sent such a zero high byte would extract with a different byte count, and the stored checksum
would then fail on extraction. No such demo was seen.

#### 7.5 Versions and evidence

Version history as listed in its usage notes [read]:

| Version | Date | Change |
| --- | --- | --- |
| 1.0 | 20 Jun 2000 | first release |
| 2.0 | 28 Jul 2000 | the current archive layout: dates, paks rebuilt |
| 2.1 | Aug 2000 | Nehahra demos |
| 2.9 | 18 Jul 2002 | the long-standing release |
| 3.0 | 18 Apr 2021 | protocol 666, and the entity limit raised from 1,024 to 32,768 |
| 3.1 | 13 Jun 2023 | |
| 3.2 | 24 Apr 2025 | |

Major version 3's additions only appear in 666 streams or with entity numbers of 1,024 or more:
- the 0x60 sound form,
- the 0x32 force section,
- the baseline flags above bit 7,
- the record bits 20..22.

Derived: a major-3 decoder reads major-2 demo streams unchanged.

Measured on the public archive:
- The archives were made by 3.0 (four) and 3.1 (two).
- Re-compressing a protocol-15 demo with 3.2 gave the same intermediate size (324,717 bytes) and
  the same compressed size (117,046 bytes) as the published 3.1 archive. Derived: the transform
  did not change between 3.1 and 3.2.
- The 666 run (257,187,892 bytes) packed to 15,783,448, with a 39,452,632-byte intermediate
  stream.
- The protocol-15 runs packed to 6% to 12% of their size.

#### 7.6 Licence

The maintained repository carries a licence file with the Mozilla Public License 2.0 [read]. Its
original notes file, kept in the same repository, carries the authors' earlier permissive notice:
"(C) 2000-2002 Stefan Schwoon, Nolan Pflug". That notice grants use for any purpose, alteration
and redistribution, on three conditions: the origin must not be misrepresented, altered versions
must be marked, and the notice must not be removed. The text is close to the zlib licence [read].
It bundles zlib.

Dzip is not GPL. Which licence covers which part (the 2000–2002 code, or the later 666 work) is not
stated in the repository; that is open.
