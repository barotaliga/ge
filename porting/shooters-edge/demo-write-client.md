# Writing a protocol 28 client recording (.qwd) that the oldest client plays (clean-room answers)

Reader's note for the clean room: what a written client recording must hold for the 1998-2000
multiplayer client (and current ones) to play it. Facts only, in the reader's words, each marked
**[read]** (stated by the source), **[derived]** (worked out from read facts) or **[not read]**.
Nothing was built, run or measured. It adds to `protocol.md` (the message layouts) and
`demo-formats-classic.md` and does not repeat them.

## Sources read and their licences

| Source | Release read | Licence |
| --- | --- | --- |
| id Software's released source, its multiplayer client tree (and the single-player tree of the same release, for the single-player message numbers) | github.com/id-Software/Quake at bf4ac42 | GPL v2 |
| ezQuake client | ezquake-source at c633fae (Nov 2026) | GPL v2 |
| FTE (fteqw) | at f09ac7c (Nov 2026) | GPL v2 |
| QW-Group qwprot (the protocol header ezQuake shares) | at a5fce41 | no licence file; part of the GPL v2 projects above |

A research session read these sources on 2026-11-14; the building session takes everything
from this note and never opens them. "The original client" means the id release. "Ends the
game" means a boxed console error, playback stops, back to the console; "fatal" means the
program exits.

## 1. The rules a writer follows (summary)

1. **Every frame packet carries svc 47, never svc 48** (§ 3). [derived]
2. **Both net channel header words hold the same number**, rising by 1 per packet record. [derived]
3. **Every packet record is a whole frame**: playerinfo for the viewed player and every visible
   player, and a 47. Prints, stuffed text, sounds and temporary entities ride in a frame packet,
   never in a record of their own. [derived]
4. **One command record per frame**, written right after the frame's packet record with the
   same time stamp; the command for sequence k is read before packet k (§ 4). [derived]
5. **The view angles go in the command records' three trailing floats.** [read]
6. **A packet record is at most 1450 bytes counting its 8-byte header** (1442 bytes of
   messages). [read]
7. **At most 64 entities a frame**, numbers 1..511 rising through the list, every one with a
   baseline sent in this file. [read]
8. **Every model index written names a loaded model-list slot**; `progs/player.mdl` is in the
   list when players are sent without the model field. [derived]
9. **End with an in-band svc 2 (disconnect)** in a normal packet record. [derived]
10. **Single-player messages and temporary entities are rewritten or dropped** by § 8. [read]

## 2. Limits of the original client

| What | Limit | Past it |
| --- | --- | --- |
| Packet record length (header included) | 1450 bytes | fatal [read] |
| Packet record shorter than 8 bytes | | skipped, "Runt packet" printed [read] |
| A field read past the end of a packet | | ends the game, "Bad server message" [read] |
| Entities in one 47 or 48 (copied ones included) | 64 | ends the game [read] |
| Things drawn a frame (players, entities, nails, temporary models) | 256 | the rest not drawn, silently [read] |
| Nails in svc 43 | 32 a packet | the rest dropped [read] |
| Static entities | 128 | ends the game, "Too many static entities" [read] |
| Model list, sound list | slots 1..255 | the 256th name ends the game ("Server sent too many model_precache" or "…sound_precache") [read] |
| A model or sound name, a light style pattern | 63 characters | not checked: a longer one corrupts memory [read] |
| Light style index | 0..63 | fatal [read] |
| Player slot in frags, ping, loss, enter time, userinfo, setinfo | 0..31 | ends the game [read] |
| Player slot in playerinfo | 0..31 | above 32 fatal; 32 itself not caught and corrupts memory [read] |
| Stat index | 0..31 | fatal [read] |
| Entity number in a 47 | 0..511 (9 bits) | not expressible [read] |
| Baseline entity number | table of 768, number not checked | 768 or more, or negative, corrupts memory; 512..767 unreachable from a 47 [read] |
| Sound entity | 10 bits; 0..768 accepted | 769..1023 ends the game [read] |
| Static sounds | 116 | each extra dropped with a message [read, the count derived] |
| Beams | 8 at once, 0.2 s each; same entity replaces its beam | a 9th dropped, "beam list overflow!" [read] |
| Temporary entity kind | 0..13 | fatal [read] |
| Any string field | 2047 characters | cut, and the rest parsed as messages: the game ends [read, consequence derived] |
| Command buffer (stuffed text) | 8192 bytes in all; a token 1023 | text that does not fit is dropped with a message [read] |
| Centre print | 1023 characters kept | [read] |
| Server info | 512 bytes | `fullserverinfo` copies without a bound (corruption); svc 52 refuses with a message [read] |
| Userinfo (svc 40) | 195 characters kept | the rest dropped [read] |
| Info key or value (svc 51, 52) | under 64 characters, no `\` or `"`, no `*` keys | refused with a console message [read] |
| Level name | 39 characters shown | [read] |

On the client, svc 51 and 52 strip the high bit and control characters from every value except
`name`, and lower-case `team`. So svc 51 cannot change a star key in the original client.
[read]

Modern clients: ezQuake ends playback on a record larger than 8192 bytes and takes up to 300
entities a frame; FTE takes records up to 65536 bytes and has no fixed entity cap. [read]

## 3. Frames and the delta byte

1. **The frame ring** is 64 frames, indexed by a sequence modulo 64. [read]
2. **The first header word** (low 31 bits) becomes the incoming sequence: the 47's entities are
   stored in that frame, and the renderer, the view and the prediction start from it. [read]
3. **The second header word** (low 31 bits) indexes the frame that playerinfo, the latency
   estimate, the solid entities prediction clips against and the muzzle-flash positions use.
   [read] Hence rule 2: the two words must be equal. [derived]
4. **The from byte of svc 48 is not used by the original client** (only a developer warning on
   a mismatch). It deltas from a frame number it remembers only when it sends a move itself,
   which it never does in playback (0 after each serverdata): a 48 deltas from slot 0's stale
   entities, or, once the outgoing sequence reaches 63, the frame is dropped. [read, consequence
   derived]
5. **The original client never asks for deltas while recording**, so its own recordings hold only
   svc 47. [read]
6. A 47 with an entity carrying the remove bit, or one needing a copy from an old frame, prints a
   warning and drops the whole frame. Falling entity numbers in a 47 are not detected. [read]
7. **Modern clients**: ezQuake requires a 48's from byte to name the sequence of the last packet
   entities parsed before the preceding command record (modulo 64), or it drops the frame; FTE
   uses the from byte directly and drops the frame when that frame is invalid or 63 or more
   behind the outgoing sequence. Both play an all-47 file. [read]
8. **A packet without a 47** leaves its frame slot holding the entities stored there 64
   sequences earlier, and they are drawn. **A packet without a player's playerinfo** hides that
   player for the frame; for the viewed player the view height, weapon frame and dead flags come
   from a stale slot and the view snaps. [read, consequence derived]

## 4. Command records and the view

1. **A command record**: the 24-byte command (byte msec, 3 padding bytes, three float angles, three
   shorts forward, side and up move, byte buttons, byte impulse; little-endian) is stored in the
   frame of the outgoing sequence with the record's time as that frame's sent time; the outgoing
   sequence goes up by 1; the three trailing floats become the view angles at once. [read]
2. Its angles, moves, msec and buttons are used **only by prediction**; the impulse not at all.
   [read]
3. **The view position**: the viewed slot's playerinfo origin and velocity in the latest frame, run
   forward by the player move on the commands of the frames between it and the outgoing sequence,
   then interpolated between the packet's state and the predicted one by the client's time (its
   clock less its latency estimate) between the two frames' sent times, held to 0..1. Any axis
   moving more than 128 units between them is a teleport: no interpolation. `cl_nopred 1` places
   the view at the packet's origin. Moves over 50 ms are split in halves. [read]
4. **The view freezes** (its position is not updated that client frame) when no command with a
   sequence above the latest packet's has been read, and prediction is skipped when the outgoing
   sequence runs 63 or more ahead. Several packets per command, or a missing command, cause it.
   [read, consequence derived]
5. **The pattern**: before frame packet k is parsed the command for sequence k has been read,
   and right after it one command with the same time stamp follows. With signon packets numbered
   1..n: a sequences record (outgoing n + 1, incoming n), one command record, then for k = n + 1,
   n + 2, … a packet record with both words k and one command record at its time. Without a
   sequences record, n + 2 command records before the first frame packet do the same. [derived]
6. **Stutter**: commands whose moves do not reproduce the motion make the view correct every
   packet; msec 0 makes it step at the packet rate; a teleport-sized jump or times that go back
   break the interpolation. [derived]
7. **Prediction inputs**: the movement floats of serverdata and svc 49/50, the health stat (health
   ≤ 0 predicts as dead), the world, the solid entities and the other players' boxes. The latency
   estimate is never reset: it drops at once to any smaller observed delay and rises 1 ms a packet
   otherwise, ignoring delays outside 0..1 s. [read]
8. **Viewed player's playerinfo fields used**: origin, velocity (a missing axis is 0), dead (0x200)
   and gib (0x400) flags, weapon frame (0x100; absent is frame 0), effects (its light). Its model,
   frame, skin and command are not used for the view. [read]
9. **Other players**: drawn at their origin predicted forward by half their state's age (playerinfo
   msec), at most 255 ms, on their command's moves; yaw from the command, pitch a third of the
   command's. The command is not cleared when the flag is absent, so **send the command (0x2) for
   every other player every frame**. [read, rule derived]
10. **View height** is fixed: +22 alive, +8 gibbed, −16 dead (dead also rolls the view 80°). No view-height stat in the original client. Stair smoothing lifts the view at most 80 units a second, lagging at most 12. [read]
11. **The gun**: the model-list slot in stat 2 (0 for none), frame from the playerinfo weapon frame, hidden while dead, gibbed or in intermission. [read]
12. **Spectator bit** (bit 7 of the player number): leave it clear for a player's view; set, the client predicts with the spectator move and its camera logic. The viewed slot is never drawn, and it may have no userinfo (the scoreboard then skips it). [read, derived]

## 5. Net channel header and time in playback

1. No address check. A first word of 0xFFFFFFFF is out of band (§ 7). [read]
2. **The only drop rule**: a sequence not greater than the last accepted is dropped silently
   (`showdrop 1` shows it). Gaps only count as drops. The reliable and reliable-ack bits are never
   checked. [read]
3. The channel starts at 0/0 when the file opens; a serverdata does not reset it, so the first
   packet is at least 1 and a second map continues the numbering. [read]
4. **The sequences record (type 2)**: the client's recorder writes one, once, after its signon
   block, with the live outgoing then incoming sequence. Playback sets the outgoing and incoming
   sequences from it. It is **not required**; if written, later packets must exceed its second
   number. [read, derived]
5. **Time**: records are read while their time is not later than the client's clock. An earlier
   time is read at once. A gap over 1 s is cut to 1 s. Before the serverdata and while paused the
   clock follows the records. `timedemo` reads one time stamp's records per client frame. [read]
6. `cl_timeout` (60 s) applies in playback but the 1 s gap rule keeps it out of reach. The client
   runs 30 to 72 frames a second; several frame packets in one client frame are all parsed. [read,
   derived]

## 6. The signon

**What the client's own recorder writes** (header words both a counter from 1, every record the
same time): serverdata + cdtrack 0 + stufftext `fullserverinfo "<info>"`, the sound list in chunks,
the model list in chunks (each chunk closed past 725 bytes; the continuation byte nonzero for "more",
0 on the last; the next chunk starts at the count already sent); static entities, then baselines
for every non-zero baseline, then stufftext `cmd spawn <count> 0`; for all 32 slots frags, ping,
loss, enter time and userinfo; all 64 light styles; all 32 stats as svc 38; stufftext `skins`; the
sequences record. It never writes static sounds. [read]

**What playback requires** [derived from the read handling of each message]:

1. **serverdata first**: it wipes the map state (players, stats, model and sound tables, light
   styles, server info, statics) and accepts protocol 26, 27 or 28 in playback, refusing others
   ("Server returned version N, not 28").
2. **The sound list completes before the model list**: completing the sound list loads the sounds
   and then clears the model table.
3. **The model list completes** (continuation 0): the models load, the world comes from slot 1
   (`maps/<name>.bsp`), submodels `*1`, `*2`, … from the world. A missing model stops playback with
   "The required model file '<name>' could not be found or downloaded." and a hint to get a client
   pack. Missing sounds are only warned about when played. Nothing downloads in playback.
4. **Static entities after the model list completes** (an earlier one has no model and is never
   drawn); **static sounds after the sound list completes**.
5. **Baselines** any time after serverdata and before the 47 that uses them. **They survive
   serverdata and a new demo**, so send one for every entity number used.
6. Light styles not sent are empty; send every style the map uses (the recorder sends 64).
7. updateuserinfo for every slot with a player; frags, ping, loss and enter time feed only the
   scoreboard.
8. Stats start at 0: send health and what the status bar shows.
9. `fullserverinfo` after serverdata (which wipes the server info): teamplay for the scoreboard,
   `watervis`, and `map`. The GL build compares the world's path with `maps/<map key>.bsp` while
   loading it and, when they differ, does not warp its liquids; stuffed text runs at the start of
   the next client frame, so put the record that completes the model list at a later time stamp
   than the `fullserverinfo` record (1/30 s is enough).
10. `cmd spawn …` and `skins` do nothing needed in playback (commands for the server are dropped, and skins load on first use); copying them is harmless.
11. **Do not stuff `setinfo`** or other settings: stuffed text runs on the viewer's client and persists into their configuration.
12. The client starts drawing at the first client frame after a 47, provided the outgoing sequence is under 63 ahead.

**The game folder** in serverdata [read]: a different folder first saves the viewer's
configuration, then runs `config.cfg` and `frontend.cfg` from the new folder when its `config.cfg`
exists. A name with `..`, `/`, `\` or `:` is refused ("Gamedir should be a single filename, not a
path"). `qw` and `id1` both mean the base path (`id1` then `qw`); any other name adds that folder and
its `pak0.pak`, `pak1.pak`, … ahead of it; a folder that does not exist adds nothing and is not an
error [derived]. The folder stays selected after the demo. `id1` is therefore acceptable and loads
the same as `qw`.

## 7. The end of a recording

The original client's `stop` writes a 15-byte out-of-band record (0xFFFFFFFF, byte 2, "EndOfDemo").
The original client treats it as an unknown out-of-band packet (prints "unknown:") and stops only
at the end of the file. An in-band svc 2 in a normal packet ends playback cleanly ("Server
disconnected"). Write no other out-of-band records: an out-of-band `c` makes the client try to
connect. An unknown record type stops playback with "Corrupted demo." [read, derived]

## 8. Single-player messages and effects

**Refused by the original client** (ends the game, "Illegible server message"): svc 0, 4, 5, 7, 13,
15, 17, 18, 21, 25 and every id from 54. In the single-player protocol these are version, set view,
time, update name, client data, update colours, particle and signon number (21 unused). [read]

**Shared ids with a different body** (rewrite) [read]:

| Id | Single player | Original multiplayer client |
| --- | --- | --- |
| 3 | stat: byte, long | byte, byte (38 for values over 255) |
| 6 | sound with a field-mask byte first | the base form of `protocol.md` § 4.3 |
| 8 | print: string | byte level, string |
| 11 | the single-player signon | serverdata |
| 30 | intermission, no body | coord × 3, angle × 3 |
| 32 | CD track, two bytes | one byte |
| 34 | cutscene, a string | small kick, no body |

Others accepted: 24 pause, 26 centre print, 27 (stat 14 + 1), 28 (stat 13 + 1), 29, 31 finale, 33
(runs the `help` command), 34/35 kicks. [read]

**Temporary entities**: the original client knows 0..13 (layouts in `protocol.md` § 4.4); others
are fatal. The explosion (3) draws the sprite `progs/s_explod.spr` and lightning (5, 6, 9) the
models `progs/bolt.mdl`, `bolt2.mdl`, `bolt3.mdl`, each loaded on first use and **fatal when
missing**; a beam from entity viewed slot + 1 starts at the view. The single-player 12 (explosion 2)
and 13 (beam) collide with blood and lightning blood and are rewritten (as 3 and a lightning); the
single-player gunshot (2) needs a count byte added; 14 and up are dropped or rewritten. [read,
mapping derived]

**Entity numbers**: players are drawn only from playerinfo; packet entities 1..32 are not filtered
and are drawn as well, so the viewed player's own single-player entity must not also be sent. A
packet entity with colormap 1..31 and `progs/player.mdl` takes slot colormap − 1's colours and skin.
muzzleflash names entity 1..32 (slot + 1); others are ignored. [read, derived]

**Sounds**: entity bits 3..12 (bit 13 ignored), above 768 ends the game; an empty sound slot plays
nothing; stop sound uses the word's top 13 bits. Sounds on the viewed entity play at full volume
unpanned. 8 dynamic channels: the same entity and channel is replaced first (channel 0 never
replaces), else the one nearest its end, never one playing a viewed-entity sound for another
entity. [read]

**Status**: stats keep their value until changed (send on change); damage (svc 19) tints by
3 × max(10, (armour + blood) / 2) percent up to 150 and kicks the view toward the source by
`v_kickroll`, `v_kickpitch` over `v_kicktime`; svc 34/35 punch the pitch 2 or 4 degrees, decaying
10 degrees a second. svc 10 sets the view angles but the next command record overwrites them, so a
converter puts new angles in the command records. Intermission (svc 30) holds the view at its
origin and angles, stops prediction and hides the gun until the next serverdata. [read, derived]
