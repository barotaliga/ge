# Demos, the client's view and skins, specified

The two recorded-game formats (the client's QWD and the server's MVD), how a client plays them
back and seeks, how a client smooths what it sees between packets, and how a player's skin is
found and coloured, in our words (README.md, the rules), for M18h. The bytes inside a recording
are the protocol's (`protocol.md`); the client's own prediction is `gameloop.md` § The client.

**Where each fact came from.** A research session read the modern client, the modern server's
recorder and the shared protocol header (2026-10-30), with the **modern client's defaults as
the baseline** for everything a player sees. Record types and hidden-message ids are wire
constants. Nothing here is yet checked against a recorded file: every layout below is to be
confirmed by `ge demo` reading a file the modern programs wrote, and the items that most need
it are marked **verify**.

## 1. Two formats

| | QWD | MVD |
| --- | --- | --- |
| Recorded by | a client (`record`) | the server (`sv_demo…`, or a client's `mvdrecord` relay) |
| Point of view | the recording player's, exactly the packets he received | every player's at once, a spectator chooses whose |
| Contains | the server's packets as received, the client's moves | the server's messages to all, one-player and multi-player streams, stats per player |
| Seeking | play from the start, fast | play from the start, fast |
| Extension | `.qwd` (`.qwz` is a compressed QWD made by an external tool: not supported) | `.mvd`; `.mvd.gz` is the same file run through plain gzip (measured: § 3.9) |

Neither has a file header: a file is a run of records from its first byte. A file ends at its
last complete record (a truncated record is ignored), or at a disconnect message.

## 2. QWD

1. **A record**: float time (the client's clock when the record was written, seconds), byte
   type, then the type's body:

   | Type | Id | Body |
   | --- | --- | --- |
   | cmd | 0 | the client's usercmd as a raw struct, then 3 floats: the view angles |
   | read | 1 | long length, then a whole packet as received, net channel header included |
   | set | 2 | long outgoing sequence, long incoming sequence |

2. **The usercmd struct** in a cmd record, 24 bytes: byte msec, 3 bytes padding (written as
   whatever was in memory: ignore on read, write zeros), 3 floats angles, 3 shorts forward / side
   / up, byte buttons, byte impulse. **verify**: the modern client's default build adds one byte
   after the impulse, which pads the struct to 28 bytes, so its QWDs carry 28-byte commands and
   other readers misread them. `ge demo` must detect the size (try 24, check that the next byte is
   a valid record type at a sane time, else 28) and our writer writes 24.
3. **Writing**: recording starts mid-game, so the recorder first writes a synthetic signon as
   read records (server data, the lists, the baselines and statics, the light styles, the
   players' info, frags, pings, stats, and a stuffed `skins`), each with an increasing fake
   sequence (its index × 2), then a set record, then the live packets as they arrive and a cmd
   record for each move sent.
4. **Delta entities**: a live packet whose entities are a delta against a frame the recording
   never saw is rewritten as a delta against the baselines before it is stored (the
   `cl_demo_qwd_delta` cvar, default 1, governs this), so a QWD always replays from its start.
5. **Playback**: a read record is fed to the parser as if it came from the network (the header
   gives the sequences); a cmd record becomes the frame's command (so prediction and the ping
   display work); time comes from the record times, scaled by the playback speed.

## 3. MVD

1. **A record**: byte milliseconds since the previous record, byte type and target, then the
   body below. The milliseconds (research session that read source; the recording in § 3.9
   agrees): the writer takes the whole milliseconds since its own last mark, moves its mark on by
   all of them, then writes 0 for a gap under 2 ms and 255 for a gap over 255. A gap is never
   split across records, so a reader that sums the bytes loses each cut gap's excess and up to
   1 ms whenever a 1 ms gap is written as 0; a frame's later records carry 0.

   | Type | Id (low 3 bits) | Target (high 5 bits) | Body |
   | --- | --- | --- | --- |
   | cmd | 0 | — | as QWD: not written by the server |
   | read | 1 | — | long length, message bytes (no net channel header) |
   | set | 2 | — | long, long (as QWD) |
   | multiple | 3 | — | u32 bit mask of players, long length, message: for those players only |
   | single | 4 | player number | long length, message: for that player only |
   | stats | 5 | player number | long length, message: that player's stats |
   | all | 6 | — | long length, message: for everyone |

2. **Frames**: the server writes at `sv_demofps` (default 77) a second, dropping to
   `sv_demoIdlefps` (default 10) when nothing happens; pings every `sv_demopings` (default 3)
   seconds. One frame is, in order (research session that read source): the stats records of
   the players whose stats changed, an `all` record holding the players' playerinfo and the
   packetentities, then an `all` record holding what was multicast to everyone that frame
   (sounds, temporary entities, prints). The writer keeps up to about five frames in hand before
   writing, so it can interpolate each player to the frame's time (item 4).
3. **Entities** (research session that read source): the demo is written as if to one more
   client that sees everything: no visibility culling, and the larger per-packet entity limit.
   Each frame's packetentities is a delta (`svc_deltapacketentities`, the from byte the previous
   frame's sequence & 255) against the frame written just before it; the first frame after the
   signon is a full `svc_packetentities`. A reader therefore needs only the last frame's entity
   list, keyed by that byte.
4. **The start**: the same signon as a client gets, except the server data's player number is
   replaced by a float (the server time) and the MVD hidden-messages bit is always set in the
   extension pairs. Lists are chunked as in a live signon (`protocol.md` § 5). The extension pairs
   (`<long tag><long bits>`) come before the protocol version; a reader must honour every bit
   that changes a message's layout, since a recording made on a modern server carries several
   (§ 3.9: alpha, the model and entity number extensions, the second static spawn, colour tint
   and 256 packet entities in the first family; a bit 0x1 in the second family that the
   protocol's public header leaves undefined, ignorable; hidden messages in the MVD family).
5. **Players** are written each frame as playerinfo (42) with the MVD flag set (instead of the
   live flags):

   | Bit | Meaning |
   | --- | --- |
   | 1, 2, 4 | origin x, y, z follow (coords) |
   | 8, 16, 32 | angle x, y, z follow (angle16) |
   | 64 | effects (byte) |
   | 128 | skin number (byte) |
   | 256 | dead |
   | 512 | gib |
   | 1024 | weapon frame (byte) |
   | 2048 | model (byte) |

   Body: byte player, short flags, byte frame, then the fields present in the order: the three
   origins, the three angles, model, skin, effects, weapon frame. Missing fields keep the previous
   frame's value (a delta against the last record). Positions are the server's interpolation to
   the frame time, so a player between commands is still smooth; it does not interpolate across
   a teleport or a respawn (that frame's value is used as it is).

   Research session that read source, replacing the earlier "verify":
   - **Pitch**: the recorder multiplies the player entity's pitch by −3. The game keeps a player
     entity's pitch at −⅓ of the view pitch, so **the pitch read is the player's view pitch**: a
     reader uses it as it is, it does not divide. Roll is always 0. A dead player is written
     with pitch 0 (the yaw of his body, roll 0), so a corpse does not look around.
   - **Dead and gib** (256, 512) are set every frame from the player's state, not as changes:
     dead while health ≤ 0, gib while the entity's lowest bound is not −24 (the head left by a
     gib). Spectators are never written as players.
   - **What "missing" is measured against**: the writer leaves a field out when the value it is
     about to write equals the previous frame's value *before* interpolation, while what it wrote
     was interpolated. A reader keeping its last decoded value can so be off by up to one
     frame's interpolation step while a player slows to a stop; small, and nothing to correct.
6. **Stats** go in stats records (5) to the player they belong to.
7. **Hidden messages**: a multiple record with mask 0 that no client sees, holding blocks of
   `<long length><u16 type>` (type 0xFFFF means a further u16 type follows). Types: 0 antilag
   positions, 1 a player's usercmd, 2 weapon choice, 3 demo info, 4..6 commentary, 7 damage done,
   8 server-side weapon, 9 weapon instruction, 0xA paused duration, 0xB start time (u64 Unix ms).
   A player of ours may skip them all; `ge demo` should list them. A paused frame's own
   milliseconds byte is 0; the real time paused is the paused-duration block (0xA, one byte of
   milliseconds), and the server sends that block to live relays, not to a file on disk
   (research session that read source).
8. **The end**: a disconnect (2) carrying the string "EndOfDemo".
9. **A measured recording** (this round's facts above, checked against a real file): a 4-player
   game on a modern server, 8,216,131 bytes unpacked (3,613,938 as `.mvd.gz`; SHA-256
   of the unpacked file `e325d2f318678d911a5d0e1083794dcf2fef7a782e41eb2f2a97d9e1710c1d09`, the
   name it is published under, so the hash names the MVD, not the gzip). Read with the record
   layout of item 1 it is 141,590 records ending exactly at the last byte: 86,298 `all`, 45,879
   `multiple` (every one of them hidden, mask 0), 4,712 `single`, 4,701 `stats`, none of `cmd`,
   `read` or `set`. Milliseconds bytes: 99,770 zeros, never 1, at most 26, mostly 13 to 16 (77
   frames a second); they sum to 621.2 s. Single and stats targets: players 0, 1, 2 and 4.
   Hidden blocks, all ending flush with their records: 136,727 of type 0x1 (a player's usercmd),
   777 of 0x7 (damage done), 2 of 0x3 (demo info), 1 of 0xB (the start time). The first `all`
   record opens with server data (11), then the pairs `FTEX` 0x01487008, `FTE2` 0x1, `MVD1`
   0x20, then protocol 28. It ends with a disconnect carrying "EndOfDemo". The recording is not
   committed; its URL is in the PR that added this item.

## 4. Playback and seeking

1. **Clock**: QWD time is the record's float; MVD time is the sum of the ms bytes. Playback speed
   is `cl_demospeed` (default 1, clamped to 0..20; 0 pauses).
2. **Seeking** (`demo_jump [+|-][[h:]m:]s`): forwards is reading ahead without drawing until the
   demo clock reaches the target; backwards is restarting the file and reading ahead to the
   target. `demo_jump_rewind` (default −10) is the default step; `demo_jump_skip_messages`
   (default 1) hides prints while seeking. Our design target (M18h) is the same rule, plus keyframes
   (`docs/replays.md`) so a long backward seek does not replay the whole file.
3. **MVD view**: the viewer is a spectator; the player tracked is whose single and stats records
   apply, and whose view is drawn. Other players are interpolated between the two frames around
   the demo time by the fraction of that interval: origins and velocities linearly, angles the
   short way round.
4. **QWD view**: exactly as the recorder saw it (his prediction re-run from his cmd records).

## 5. The client's view between packets

`gameloop.md` § The client covers command building and the client's own prediction. What a
client does for everything else:

1. **Entities** are drawn between their last two updates: position and angles interpolated by
   how far the client clock is between the two packets' times. `cl_newlerp` (default 0) uses an
   average-velocity form instead. An entity that moved more than 100 units on any axis between
   two updates is snapped (a teleport); one that moved more than 200 units, or changed model, is
   reset (no interpolation from the old state).
2. **Model animation frames** blend from the previous frame over 0.1 s: weight `(now − frame
   change time) × 10`, clamped to 0..1.
3. **Other players** are predicted forward from their last state by their msec (the time since
   their last command) plus a small fixed lead (`gameloop.md` § The client), and are solid for
   the client's own prediction when `cl_solid_players` is on.
4. **Independent physics** (`cl_independentPhysics`, default 1): the client sends commands at a
   physics rate separate from its frame rate and draws the view interpolated over the last
   physics frames, with an adapting latency that drifts down slowly (about a thousandth of a
   frame time a frame, verify).
5. **The cvars and their defaults** (the modern client):

   | Cvar | Default | Effect |
   | --- | --- | --- |
   | `cl_nolerp` | 0 | 1 draws entities at their last update |
   | `cl_nolerp_on_entity` | 0 | no smoothing while standing on a moving entity |
   | `cl_newlerp` | 0 | average-velocity interpolation |
   | `cl_lerp_monsters` | 1 | interpolate monsters |
   | `cl_nopred` | 0 | 1 turns off the client's own prediction |
   | `cl_predict_players` | 1 | predict other players |
   | `cl_solid_players` | 1 | other players block the client's prediction |
   | `cl_predict_half` | 0 | predict half a frame (for high frame rates) |
   | `cl_predict_smoothview` | 1 | smooth small prediction errors |
   | `cl_predict_jump` | 1 | predict jumps |
   | `cl_predict_weaponsound` | 1 | play weapon sounds on the predicted shot |
   | `cl_predict_buffer` | 1 | (weapon prediction buffer, verify the meaning) |
   | `cl_independentPhysics` | 1 | § 5.4 |
   | `cl_physfps` | 0 | physics rate; 0 means the server's `maxfps` (77) |
   | `cl_maxfps` | 0 | draw rate cap; 0 means none |
   | `cl_c2spps` | 0 | packets a second to the server; 0 means one a physics frame |
   | `cl_c2sImpulseBackup` | 3 | an impulse is repeated in this many packets so a loss does not drop it |
   | `cl_nodelta` | 0 | 1 asks for full entity updates |
   | `cl_demospeed` | 1 | § 4.1 |

## 6. Skins

1. **Which name**: the first that applies of: `allskins` (if set, every player wears it); the
   team and enemy skin forcing (`teamskin` / `enemyskin`, and the power-up skins when a player
   carries quad or pentagram, verify the precedence between those two); the player's own `skin`
   info key; `baseskin` ("base"). A name containing `..` or starting with `.` is replaced by the
   base skin.
2. **Which file** (the modern client): `skins/<name>.link` (a text file naming another skin),
   then the 32-bit images `skins/<name>.tga`, `.png`, `.jpg`, preferring a file outside the
   protected game packs; then the 8-bit `skins/<name>.pcx` (at most 320 × 200; the model uses its
   top-left 296 × 194, verify). If none loads, the base skin. M18h states the order as png then
   pcx; tga and jpg are optional for us.
3. **Colours** (the 8-bit path only). Top = `clamp(topcolor, 0, 13) × 16`, bottom =
   `clamp(bottomcolor, 0, 13) × 16`. Palette rows 1 (indices 16..31, the shirt) and 6 (96..111,
   the pants) are remapped: index `16 + i` becomes `top + i` when `top < 128`, else `top + 15 − i`
   (the upper palette rows run dark to light the other way); the same for `96 + i` from bottom.
   Every other index is unchanged. Example: topcolor 4, i = 3 → index 67.
4. **32-bit skins are not recoloured** by the modern client: they are drawn as they are, only
   tinted when `r_teamskincolor` / `r_enemyskincolor` are set. This differs from M18h's wording
   ("with the player's colours"); the decision for us: recolour 8-bit skins by the remap, and for
   png skins either draw them as they are (match the modern client) or apply the colours to a
   mask, which would be ours. **verify** with the lead.
5. **Downloads**: during signon the client checks each player's skin and downloads
   `skins/<name>.pcx` when missing (`protocol.md` § 12), then sends `begin`. `noskins 1` draws
   every player with the base skin and downloads nothing.

## 7. What is ours, not theirs

1. `ge demo` reads both formats and writes QWD with 24-byte commands and MVD as the modern server
   does.
2. Seeking backwards uses our keyframes rather than a restart (`docs/replays.md`); the result must
   be the same frame a restart would reach.
