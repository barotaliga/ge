# Writing a protocol 15 single-player demo (.dem) that the oldest single-player client plays (clean-room answers)

Reader's note for the clean room: what a written `.dem` (protocol 15) must hold for the 1996-97
single-player/netgame client (the original id release) and for a current client to play it, when the
file is converted from a multiplayer match of up to 32 players seen from one player's view. Facts
only, in the reader's words, each marked **[read]** (stated by the source), **[derived]** (worked out
from read facts) or **[not read]** (the source does not settle it). Nothing was built, run or
measured. It adds to `protocol.md` and `demo-formats-single.md` (message layouts, the music line, the
block framing) and does not repeat them.

## Sources read and their licences

| Source | Release read | Licence |
| --- | --- | --- |
| id Software's released source, its single-player/netgame client tree (the original client, software and GL builds) | github.com/id-Software/Quake at bf4ac42 (2012-01-31) | GPL v2 |
| QuakeSpasm (the current client checked) | quakespasm at e8795a7 (2026-10-29) | GPL v2 |

A research session read these sources on 2026-11-14; the building session takes everything from this
note and never opens them. "The original client" means the id release; "QuakeSpasm" the current one.
Three outcomes recur:

- **ends playback**: a console error is printed, the client disconnects, playback stops and the
  attract loop of demos is stopped too (no next demo).
- **fatal**: the program exits with an error box.
- **ends the demo normally**: the "server disconnected" path: playback stops and, in the attract
  loop, the next demo starts.

Message ids below are the protocol 15 numbers (`time` 7, `signonnum` 25 and so on, as in
`protocol.md`).

## 1. The rules a writer follows (summary)

1. **A block is at most 8000 bytes of messages** (the 12-byte header not counted), never 0 bytes.
   Keep a frame block near 1024 bytes, the size the original server's frames never exceeded. [read]
2. **The signon, in order:** serverinfo, then setview, then `signonnum` 1; baselines, statics and
   static sounds, then `signonnum` 2; `time`, the players' names, frags and colours, the light styles,
   clientdata, then `signonnum` 3; then the first frame. Anything the client must know about the level
   comes after serverinfo, because serverinfo wipes the client's state. [read]
3. **The first frame block starts with `time` and contains at least one fast update**: the first
   fast update after `signonnum` 3 is what starts the game (§ 2). [read]
4. **One block per frame, and every frame block starts with `time`**, then clientdata, then a fast
   update for **every** entity that is visible this frame. An entity not updated after the frame's
   `time` is hidden for that frame (§ 5). [derived]
5. **A fast update is a delta from the entity's baseline, not from the last frame**: every field
   that differs from the baseline is written in every frame. [read]
6. **Frames at most 0.1 s apart; time strictly rising.** A gap over 0.1 s is played in 0.1 s of real
   time (the demo runs fast there); time going backwards makes the client race through blocks
   (§ 11). [derived]
7. **The view angles go in the block header's three floats** (pitch, yaw, roll in degrees),
   `setangle` has no lasting effect during playback (§ 6). [read]
8. **At most 16 players** (`maxclients` 1..16). Player slot k is entity k+1, with colormap k+1.
   The viewed player is one of those entities and `setview` names it. Every name, frags or colours
   message names a slot below `maxclients` (§ 3). [read]
9. **Entity numbers 1..599**, numbers 256 and up with the long-entity bit; at most 256 entities
   drawn a frame, the view entity not counted (§ 4). [read]
10. **Model and sound lists of at most 255 names each, model 1 the map**, every name under 64
   characters, every file present. [read]
11. **Never set the unused high bits**: bit 15 of a fast update's bits, bits 8 and 15 of
   clientdata's bits, bits 2 and up of a sound's field mask. QuakeSpasm reads extra bytes for some of
   them; the original ignores them. [read]
12. **No byte 255 inside a string** (a name, a print, a model name): the original ends the string
   there. [read]
13. **Drop or rewrite**: `svc` ids 0, 21 and 35 up, temporary entity types 14 up, `setpause`,
   `sellscreen`, and stuffed text other than client-side effects (§ 9). [read]
14. **End with a block holding `disconnect` (2)**: QuakeSpasm exits with an error at end of file
   without one (§ 10). [read]

## 2. Limits of the original client (and QuakeSpasm where it differs)

| What | Original | Past it (original) | QuakeSpasm |
| --- | --- | --- | --- |
| Block length (messages only) | 8000 bytes | fatal, with an error that the demo message is too long [read] | 64000 bytes, then fatal [read] |
| Block length 0 | | playback stops quietly, as at end of file [derived] | same [derived] |
| A field read past the end of a block | | ends playback, "Bad server message" [read] | same [read] |
| `maxclients` | 1..16 | the serverinfo is abandoned half read (§ 3) [read] | 1..16, ends playback, "Bad maxclients" [read] |
| Entity number (update, baseline) | 0..599 | ends playback (an "invalid number" error) [read]; a negative long number is not checked and corrupts memory [derived] | 0 up to the `max_edicts` setting less one (default 8192); negative ends playback [read] |
| Sound entity number | 0..600 | above 600 ends playback [read] | up to `max_edicts` [read] |
| Entities drawn a frame (entities and temporary models together) | 256 | the rest not drawn, silently, highest numbers first lost [read] | 4096 [read] |
| Temporary models (beam segments, etc.) a frame | 64 | the rest not drawn [read] | 256 [read] |
| Beams at once | 24 | "beam list overflow!" printed, the beam dropped [read] | 32 [read] |
| Static entities | 128 | ends playback, "Too many static entities" [read] | 4096 [read] |
| Static entity fragments (world leaves touched by statics) | 640 | "Too many efrags!" printed, the static entity partly unlinked [read] | not read |
| Static sounds | 116 (128 channels less 4 ambient and 8 dynamic) | a message that the channels are full printed, the sound dropped [read] | 1024 channels less 4 ambient and 128 dynamic [derived] |
| Sounds playing at once | 8 | the oldest replaced (§ 12) [read] | 128 [read] |
| Model list, sound list | slots 1..255 | the 256th name abandons the serverinfo, "Server sent too many model precaches" (or "sound") [read] | 4095 models, 2047 sounds, then ends playback [read]; a protocol 15 index is a byte, so 255 is still the useful limit [derived] |
| Different models the client holds at once (list, the map's inline models, anything else loaded) | 256 | fatal [read] | not read |
| Different sounds the client has ever named | 512 | fatal [read] | not read |
| A model or sound name | 63 characters | not checked: a longer one corrupts memory [read] | not read |
| A player name | 31 characters | not checked: a longer one corrupts memory [read] | cut to 31 [read] |
| The level name (serverinfo's text) | 39 characters kept | cut [read] | not read |
| Light style index | 0..63 | fatal [read] | same [read] |
| Light style pattern | 63 characters | not checked: a longer one corrupts memory [read] | not read |
| Stat index (`updatestat`) | 0..31 | fatal [read] | same [read] |
| A string | 2047 bytes | the rest read as the next message [read] | same [read] |
| Colormap of an entity | 0..`maxclients` | fatal [read] | same [read] |
| Temporary entity type | 0..13 | fatal, "bad type" [read] | same [read] |

## 3. Questions and answers

### 1. Block limits

- The original accepts a block whose length word is at most **8000** bytes (the messages after the
  12-byte header). A larger length is **fatal**: the program exits with an error that the demo message is
  too long. QuakeSpasm's limit is **64000** bytes, with the same fatal error past it. [read]
- The buffer the block is read into is 8192 bytes in the original, so 8000 is the binding number.
  [read]
- A length of **0** stops playback: the client asks the file for zero bytes, takes the answer as a
  failed read and stops quietly (no error, no next demo). Never write an empty block. [derived]
- A negative length passes the size check and then fails the read: playback stops quietly. [derived]
- There is **no separate per-frame limit on the client**. Inside a block the only limits are the
  table limits in § 2 (entity numbers, things drawn). The original server never sent a frame
  larger than **1024** bytes (its datagram size) and stopped adding entities when fewer than 16
  bytes were left; reliable messages (the signon) went up to 8000. A writer that keeps frame blocks
  at or under 1024 bytes makes files any client of the time would also have received over the
  network; the client itself does not need it. [read] [derived]

### 2. The signon

What the client does with each stage during playback [read]:

- **serverinfo (11)** wipes the whole client state (entities and their baselines, the score slots,
  light styles, stats, the view entity, the clock) and sets the signon stage back to **0**. It then
  loads every listed model and sound (§ 8). So setview, names, colours and light styles must come
  after it.
- **`signonnum` n** is accepted only when n is greater than the current stage; otherwise playback
  ends with "Received signon n when at m". The stage becomes n. Stages may be skipped (0 straight to
  3 is accepted). [read] [derived]
- On stages 1, 2 and 3 the client prepares its usual replies (1: "prespawn"; 2: its name, colour and
  "spawn"; 3: "begin"), but during playback the outgoing buffer is **emptied every frame without
  being sent**, and no movement command is sent either. **Nothing is sent.** [read]
- **Stage 4 is not normally sent as a `signonnum`**: the **first fast update parsed while the stage
  is 3** moves the stage to 4. At 4 the loading plaque is taken down and normal screen updates
  start; the console is held full screen while the stage is not 4 or no map is loaded. A
  `signonnum` 4 sent while at 3 also works (4 is greater than 3). A `signonnum` above 4 leaves the
  client never playing. [read] [derived]
- **Until the stage is 4 the client reads blocks back to back in the same frame**, without
  looking at time. From stage 4 on it reads a block only when its clock has passed the last `time`
  received (§ 11). [read]

So the number of blocks the signon spans does not matter to the client: all of it may sit in one
block if it fits in 8000 bytes, or in many. [derived]

Consequences [derived]:

- Without `signonnum` 3 (stage left at 0, 1 or 2) no fast update ever moves the stage to 4: the
  console stays down over the screen and every block is read in the first frame, so the whole demo
  rushes past unseen.
- Without any fast update after `signonnum` 3 the same happens.

What the original server sends, which a writer copies [read]:

1. A print with the server version line, then serverinfo, then `cdtrack` (the level's track twice),
   then `setview` (the client's own entity number), then `signonnum` 1.
2. The level's signon buffer: every `spawnbaseline`, `spawnstatic` and `spawnstaticsound` (and
   anything the game's code wrote to the signon while the level spawned), then `signonnum` 2.
3. `time` (the server's clock), then for every slot below `maxclients`: `updatename`, `updatefrags`
   (with the frags at the time), `updatecolors`; then `lightstyle` for all 64 styles; then
   `updatestat` for total secrets (index 11), total monsters (12), found secrets (13) and killed
   monsters (14); then `setangle` (pitch and yaw of the player, roll 0); then clientdata; then
   `signonnum` 3.
4. Frames: each one `time`, then (when the player's angle was forced) `setangle`, then clientdata,
   then the fast updates, then that frame's sounds, temporary entities, particles and prints.

Between serverinfo and the first frame a demo must hold at least: `setview`, `signonnum` 3 (1 and 2
before it are expected and harmless), the baselines the writer relies on, the names and colours of
the slots it draws, and the light styles the map uses (a style never sent has no pattern and lights at normal brightness,
[read]). Statics,
static sounds, the stats and clientdata before the first frame are optional to the client. [read]
[derived]

### 3. Players

- **`maxclients`**: 1..**16** in the original. A value of 0 or above 16 prints "Bad maxclients (n)
  from server" and **abandons the serverinfo half read**: the level name, model list and sound list
  are left in the block and are then parsed as messages (a garbled error at best); the score slots
  are never allocated while the count is kept, so the first name, frags or colours message writes
  through a null table. Treat it as a crash. QuakeSpasm ends playback with the same words. A 32-player
  match must be cut to **16 slots**. [read] [derived]
- **Gametype** (the byte after `maxclients`): 1 is deathmatch, 0 is co-op. Deathmatch makes the
  status bar show frags and the intermission show the frag table; co-op shows the level-complete
  screen with time, secrets and monsters. With `maxclients` above 1 the status bar shows the frags
  of the top players. [read]
- **Slots and entities**: slot k (0-based) belongs to entity k+1. The entity's model is any model in
  the list (normally `progs/player.mdl`), its frame byte is the model's animation frame (shown
  without blending between frames in the original), its skin byte the skin. [read]
- **Colours**: `updatecolors` sets slot k's colour byte: the high four bits pick the shirt range
  (times 16 into the palette), the low four bits the trousers. An entity's **colormap** value 0
  draws it in the model's own colours; a value c in 1..`maxclients` draws it in **slot c-1's**
  colours (software renderer). The GL renderer instead colours an entity with entity number
  1..`maxclients` and a non-zero colormap in **its own slot's** colours (entity number less one),
  whatever the value. So write slot k's player as entity k+1 with colormap k+1, and both agree. A
  colormap above `maxclients` is **fatal** in both clients. [read] [derived]
- Players beyond the 16 slots can only be drawn as ordinary entities numbered above `maxclients`:
  in software they may borrow a slot's colours through the colormap; in GL they are drawn in the
  model's own colours. They have no name or frags on the scoreboard. [derived]
- **Names, frags, colours** for a slot at or above `maxclients` **end playback** (an error naming the message and the scoreboard limit). The scoreboard lists the slots below `maxclients` with a non-empty
  name, sorted by frags. [read]
- **The viewer** is the entity `setview` names (a 16-bit number). It starts at 0 after serverinfo
  (the world), so `setview` must follow serverinfo. The view entity is **never drawn** (unless the
  player turns on `chase_active`), its sounds play at full volume from no direction, and the
  scoreboard marks slot (view entity less one) as "you". Keep the view entity within 1..`maxclients`:
  a mission pack's team colour box reads that slot without a range check. [read] [derived]
- **A player's entity not updated in a frame is hidden** for that frame (as every entity, § 5),
  and when it comes back it is placed at once, not slid from where it was. [read]

### 4. Entity numbering and counts

- The client's entity table holds **600** entries: numbers **0..599**. A fast update or a baseline
  for 600 or more **ends playback** with an error calling the number invalid. A negative number
  (a long number with the top bit set) is not checked in the original and corrupts memory, and
  QuakeSpasm ends playback on it. QuakeSpasm's table is the `max_edicts` setting (256..32000,
  default 8192). [read]
- Entity 0 is the world; fast updates to it change nothing drawn but overwrite its model with the
  baseline's (none). Do not write entity 0. [derived]
- Numbers 256 and up need the long-entity bit (a 16-bit number); below 256 a byte. [read]
- There is **no limit on how many entities one block updates** other than the block's 8000 bytes.
  [read]
- **At most 256 things are drawn in one frame** (software and GL alike): entities in rising number
  order first, then temporary models (beam segments). Past 256 the rest are silently not drawn, so
  high-numbered entities and beams vanish first. The view entity is not counted. QuakeSpasm draws
  up to 4096 and warns past 256 only in developer mode. [read] [derived]
- Empty numbers between used ones cost nothing but a table entry. A writer that numbers players
  1..16 and other entities from 17 rising stays well inside both limits. [derived]

### 5. Frames

- **What a frame is**: the client's frame clock is the last `time` (7) value parsed. A block with
  no `time` does not start a frame: the client reads the next block in the same screen frame, so its
  contents merge into whichever block next carries a `time`. [read] [derived]
- **The hide rule**: each entity remembers the frame clock at its last update. When the client
  places entities for drawing, **any entity whose remembered clock is not the current frame
  clock is removed** (its model cleared) for that frame. Therefore [read] [derived]:
  - every visible entity must be updated in every frame,
  - **`time` must come before the fast updates in the block**: an entity updated before the
   block's `time` keeps the old clock and is hidden,
  - when the client reads several frames in one screen frame (catching up), only the entities of
   the last one read are drawn.
- **Delta base**: every field a fast update leaves out is taken **from the entity's baseline**
  (model, frame, colormap, skin, effects, each origin and angle component). It is never a delta
  from the previous frame. So each field that differs from the baseline is written in every frame,
  and fields equal to the baseline may be left out. An entity that never had a baseline has an all-zero
  one (model 0, origin 0 0 0). [read]
- **Interpolation**: the client keeps the last two origins and angle triples of each entity and
  the last two frame times. Each screen frame it computes the fraction of the way from the older
  frame time to the newer one (clamped to 0..1) and places each entity at that fraction between its
  two origins; angles go the short way round (a difference over 180 degrees is taken as the other
  way). [read]
- An entity is **not interpolated** (placed at its new spot at once) when: it was not updated in
  the previous frame; its model is none; the update has the no-lerp bit; or any axis moved more than
  **100** units since the last frame (taken as a teleport). Animation frames are not interpolated in
  the original. The `cl_nolerp` setting turns all of it off; timedemo also does. [read]
- QuakeSpasm keeps the same hide rule and origin interpolation, and blends animation frames as
  well. [read]

### 6. The view

- **Angles**: during playback the client stores the three header floats of every block it reads
  (pitch, yaw, roll in degrees; pitch positive looks down [derived]) and each screen frame sets the view
  angles to the interpolation between the last two blocks' headers, at the same fraction as the
  entities, the short way round. This happens after the block's messages are parsed, so a
  **`setangle` in the block is overwritten in the same screen frame**: the header wins. [read]
  [derived]
- The header history moves one step per block read, not per `time`: a block without `time`
  shifts it too. One block per frame keeps the angle interpolation right. [derived]
- **Origin**: the view sits at the view entity's (interpolated) origin, raised by the view height
  from clientdata (22 when absent) plus a bob, plus 1/32 unit on each axis. The bob is computed from
  the horizontal speed in clientdata's velocity (interpolated between the last two clientdatas),
  the `cl_bob`, `cl_bobcycle` and `cl_bobup` settings, and the client clock; it is held to -7..4
  units. [read]
- Step smoothing: when clientdata says on ground and the view entity rises, the view rises at
  most 80 units a second and never lags more than 12 units. [read]
- Roll: the view rolls with sideways speed (`cl_rollangle`, `cl_rollspeed`), plus a damage kick
  after a `damage` message; with health 0 or below the roll is **80 degrees** (the dead view) and
  the scoreboard is shown. The punch angles in clientdata are added to the view. [read]
- No prediction: the client does not move the view itself; the gun model is placed from the same
  origin. The pitch drift toward the ideal pitch is off during playback, so clientdata's ideal
  pitch has no effect. [read]
- The view entity's own model is turned to face the view yaw and (negated) pitch, which only shows
  in the chase view. [read]
- **During intermission** the view comes from the view entity's own origin and angles (the fast
  updates), not from the header, with idle sway; the gun is hidden. [read]
- If the view entity is not updated in a frame the view stays at its last origin. [derived]

### 7. clientdata

Every clientdata sets every field [read]:

- Optional fields, by bit; when the bit is clear the field is set to its default, not kept: view
  height (default 22), ideal pitch (0), punch angles (0 each), velocity components (0 each; a signed
  byte times 16 units a second), weapon frame (0), armour (0), weapon model (0: no gun drawn).
- Always present whatever the bits: items (32-bit, bit 9 is set by the server but not read), health
  (16-bit signed), current ammo, shells, nails, rockets, cells (one byte each), active weapon (one
  byte).
- On ground and in water are bits only. In the original, on ground is cleared at the start of
  every block and set only by a clientdata in it. [read]
- The weapon byte is a **model list index** of the gun model shown (e.g. the index of
  `progs/v_shot.mdl`), not a weapon number; the weapon frame is that model's frame. [read]
- The active weapon byte is the weapon's **item bit value** in the standard game (so only weapons
  in the low byte can be shown); in a mission pack it is the bit's number. [read]
- Items newly set flash on the status bar. [read]
- A frame block without clientdata keeps the old stats but loses the on ground flag and keeps
  the old velocity, so the bob and step smoothing go wrong; write clientdata in every frame. [derived]
- Unused bits: 8 (no field) and 15. QuakeSpasm reads one extra byte when bit 15 is set (and more for
  its own higher bits); never set them in protocol 15. [read]
- Stats not in clientdata (secrets, monsters and their totals) come by `updatestat`, `killedmonster`
  and `foundsecret`. [read]

### 8. Model and sound lists

- Names fill slots 1 up; slot 0 is "none". The original takes **255 models and 255 sounds**: the
  256th name prints "Server sent too many model precaches" (or "sound") and **abandons the
  serverinfo**: nothing is loaded and no map is set, so the console stays over the screen. The
  remaining names of that serverinfo are left unread and are parsed as messages. QuakeSpasm takes
  4095 and 2047 and ends playback past them. [read] [derived]
- **Model 1 must be the map** (`maps/<name>.bsp`): it becomes the world. The map's inline models
  (`*1`, `*2`, …) follow as the server listed them. [read]
- A model that cannot be loaded prints "Model <name> not found" and abandons the rest of the
  loading the same way (no world). QuakeSpasm ends playback. A missing sound only prints "Couldn't
  load <name>" and plays nothing. [read]
- Names longer than 63 characters corrupt memory in the original. [read]
- The model cache holds 256 models at once (list plus anything else loaded) and the sound table
  512 names over the life of the program; past either, fatal. [read]
- An update naming a model slot with no name draws nothing; a sound naming an empty slot plays
  nothing. [read]

### 9. Messages refused or fatal

- **Ends playback**, "Illegible server message": ids **0** (bad), **21** (an obsolete id), and
  **35..127**. QuakeSpasm additionally knows 37 and 40..44 and some others (its own protocol's), but
  a protocol 15 writer must not use them. [read]
- **Ends playback**: `version` (4) with any number but 15; `signonnum` not rising; names, frags,
  colours for a slot at or above `maxclients`; an entity number of 600 or more; a sound entity
  above 600; more than 128 static entities; a field read past the block's end. [read]
- **Fatal**: a light style index of 64 or more; a stat index of 32 or more; a colormap above
  `maxclients`; a temporary entity type of 14 or more; a block over 8000 bytes. [read]
- **Executed, so filtered**: `stufftext` (9) is run as console commands. Commands that need a server
  are dropped quietly during playback, but anything else runs (settings, binds, `bf` the bonus
  flash). Keep only effect commands such as "bf"; drop the rest. [read] [derived]
- **Drop**: `setpause` (24) pauses the view and the CD; `sellscreen` (33) opens the help screen,
  `disconnect` (2) anywhere but the end ends the demo (§ 10). [read]
- **Multiplayer-only trouble**: names, frags and colours for slots 16..31 end playback; a
  32-player recording's slot numbers must be remapped into 0..15 (and the players beyond dropped
  from the scoreboard). A frags value is a signed 16-bit number. [read] [derived]
- `version` is optional; ids 27 and 28 (killed monster, found secret) only count. [read]

### 10. The end

- **Write a last block holding `disconnect` (2)**. The client takes it as the server
  disconnecting: playback stops and, in the attract loop, the next demo starts; this is also what
  the original's own recorder writes when recording stops. [read]
- At end of file without it:
  - the **original** stops playback quietly at the next read (the clock past the last `time`),
   the attract loop does **not** go on to the next demo [read] [derived],
  - **QuakeSpasm** treats a missing length word at end of file as **fatal** ("Demo read error")
   [read]; a file cut in the middle of a block just stops [read],
  - a demo inside a pack has no end of file of its own: the client reads on into whatever
   follows it in the pack. [derived]
- Nothing after the `disconnect` in that block is read. [read]

### 11. Time

- `time` is a 32-bit float of seconds. The client's own clock advances each screen frame by the
  frame's length (held to 0.001..0.1 s, at most 72 screen frames a second, or `host_framerate` when
  set). From stage 4 on, a new block is read whenever the client clock has passed the last `time`,
  several blocks may be read in one screen frame. [read]
- **Start**: any value. serverinfo resets the client clock to 0, and the first frames snap it: when
  the client clock lies more than 0.01 s outside the window between the last two frame times, it is
  moved to the window's edge. A match may keep its own clock (say 412.5 s). [read] [derived]
- **The 0.1 s window**: when two frame times are more than 0.1 s apart the older one is taken as
  newer time less 0.1. As a result a gap larger than 0.1 s is **skipped**: the client clock jumps to
  0.1 s before the new frame and plays only that last 0.1 s. Frames 0.5 s apart play five times
  too fast. Keep frames at most 0.1 s apart (the original server sent 20 to 72 a second); write
  repeated frames through a pause rather than a gap. [read] [derived]
- **Equal times**: two frames with the same time snap the clock to it with no interpolation; the
  next block is read on the next screen frame. Harmless but jerky. [read] [derived]
- **Backwards**: a frame time lower than the last does not move the client clock back (more than
  0.01 s); the client then finds its clock past the frame time and reads block after block in the
  same screen frame until a time ahead of its clock appears. Those frames are never shown. [derived]
- Float precision: past about 65536 s a float's step is about 0.008 s; start well below that.
  [derived]
- `timedemo` ignores time and reads one block per screen frame. [read]

### 12. Anything else a converter must know

- **Intermission** (30): from then on the view is the view entity's own origin and angles,
  the gun is hidden, and the deathmatch frag table (gametype 1) or the level-complete screen
  (gametype 0) is shown. **Nothing turns it off** but a new serverinfo, so put it only at the end.
  `finale` (31) and `cutscene` (34) are the same with a centred text. [read]
- **Sounds** (6): the entity number is the top 13 bits of the 16-bit channel word, read signed in the
  original (so -4096..4095, above 600 ends playback). The position in the message places the sound,
  the entity only decides replacement: a sound on a non-zero channel replaces the sound playing on
  the same entity and channel; otherwise the oldest of the **8** dynamic channels is replaced,
  except that a sound from another entity never replaces one from the view entity. Sounds from the
  view entity play at full volume from no direction. Use the converted entity numbers so a player's
  sounds follow their player and the viewed player's are heard as their own. [read] [derived]
- Sound field mask: only bits 0 (volume byte) and 1 (attenuation byte) exist in protocol 15,
  QuakeSpasm reads a wider entity or sound number when bit 3 or 4 is set. [read]
- **Particles** (18): a count byte of 255 means a 1024-particle explosion; otherwise that many
  particles. The pool is 2048 particles by default; extra particles are silently not made. [read]
- **Temporary entities** (23): types 0..13 are known (13 is the grappling beam); 14 and up are
  fatal. Lightning and beam start points follow the view entity's origin when the beam's entity is
  the view entity. [read]
- **Damage** (19): the kick direction is worked out from the view entity's origin and angles. [read]
- **Static entities** (20): at most 128 (ends playback past it) and 640 leaf fragments (printed,
  partly unlinked past it). **Static sounds** (29): at most 116, and each must be a looping sound
  file, else "Sound <name> not looped" is printed and it is dropped. [read]
- **Light styles** (12): indices 0..63 (fatal past), patterns of letters `a`..`z` up to 63
  characters. All 64 are sent at spawn; changes during play are allowed. [read]
- **A second map in one demo**: a new serverinfo resets the stage to 0 and wipes everything, so the
  whole signon is repeated after it; no `reconnect` is needed (QuakeSpasm ignores `reconnect` during
  playback, the original resets the stage on it). [read] [derived]
- **CD track**: `cdtrack` (32) plays the level's track unless the file's music line forces one
  (any value but -1). [read]
- **The music line**: QuakeSpasm reads it as a C-style integer (a leading 0 means octal, 0x hex) and
  requires a line feed right after it, refusing the demo ("is invalid") on a carriage return. The
  original builds the number digit by digit until a line feed and never stops if there is none.
  Write decimal digits (or "-1") and a bare line feed. [read] [derived]
- **Strings**: the original reads string bytes as signed, so byte 255 ends the string early and the
  rest is read as messages. QuakeSpasm reads them unsigned. Other high bytes (128..254) are fine
  (coloured text). [read]
- The original sets the player's position offsets (`scr_ofsx`, `scr_ofsy`, `scr_ofsz`) to 0 when
  `maxclients` is above 1. [read]
