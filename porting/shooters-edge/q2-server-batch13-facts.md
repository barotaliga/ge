# Answers: Quake II server (commit 601a8df), batch 13: spectator flight, protocol extensions, the newer game interface

Facts in the reader's own words. [read] = seen in the source; [derived] = worked out from what was read.
Protocol numbers: 34 = the original, 35 = the R1Q2 family, 36 = the Q2PRO family. "Minor" = the minor
protocol version a 35/36 client sends on connect.

## A. Spectator flight for protocol 35 and 36 clients

### 1. What differs, and the whole flight step

Per-client move parameters [read]. Every client gets a copy of the server's default move parameters
when it connects. Defaults: speed multiplier 1, flight friction 9, the "fly hack" off, maximum speed 300
(320 in the server's QW mode), stop speed 100, acceleration 10 (both fixed constants of the move, not
parameters).
- A client on protocol 35 OR 36 gets the speed multiplier set to 2. [read]
- A client on protocol 36 ONLY additionally gets the fly hack switched on and the flight friction set to
  4. A 35 client keeps flight friction 9 and the fly hack off. [read]
- The server's move call (the game's Pmove import) uses the parameters of the client currently being
  thought for (the one whose command is being run); when no client is current it uses the server's
  defaults. [read]
- The client applies the same values to its own prediction when it reads the server data: on a 35
  server it sets the multiplier to 2; on a 36 server it sets the multiplier to 2, turns the fly hack on
  unconditionally and sets flight friction 4. So the change is in BOTH the server move and the client's
  prediction, and they agree. [read]

"Double time" [read]: what is doubled is the step's frame time. For a spectator the frame time is
(multiplier x command msec x 0.001) seconds; for every other move type it is (msec x 0.001) without the
multiplier. The msec value in the command is not changed; the speed clamp value is not changed. Because
the doubled frame time feeds the friction drop, the acceleration amount and the position update, the
effect is that a spectator covers twice the distance per command and both friction and acceleration act
twice as hard per command. [read for the formula; derived for the consequence]

Spectator path of the move, in order [read]:
1. Common start (all move types): clear outputs, convert the packed origin and velocity to floats (x
   0.125), remember the packed origin, clamp the view angles (command angles plus delta angles; pitch
   clamped to -89..89; while the teleport flag is set, yaw only and pitch/roll 0) and compute forward,
   right and up from the view angles (full angles including pitch).
2. Frame time = multiplier x msec x 0.001.
3. Flight step (below).
4. Snap the position to the 1/8 grid (the usual snap; for a spectator every candidate position counts as
   good, so the first candidate, the plain rounded position, is always accepted). No duck check, no
   ground/water categorisation, no timers for a spectator. [read]

Flight step, in order [read]:
1. View height set to 22.
2. Friction: speed = length of velocity. If speed < 1, velocity is zeroed. Otherwise control = the larger
   of speed and 100 (stop speed); drop = control x flight friction x frame time; new speed = speed - drop,
   floored at 0; velocity scaled by new speed / speed. Flight friction is 9 for 34 and 35 clients, 4 for 36
   clients.
3. Wish velocity: forward and right vectors are each normalised; wish velocity = forward x forwardmove +
   right x sidemove, then upmove added to its vertical component. Wish direction = wish velocity
   normalised; wish speed = its length.
4. Clamp: if wish speed > maximum speed (300, or 320 in QW mode), wish velocity is scaled down to it and
   wish speed set to the maximum.
5. Acceleration: current speed = velocity dot wish direction; add speed = wish speed - current speed.
   - If add speed <= 0: WITHOUT the fly hack (34 and 35), the step RETURNS here and the position is NOT
     updated this command (the original behaviour, called buggy in a comment). WITH the fly hack (36),
     the acceleration is merely skipped and the step carries on to the position update.
   - Otherwise accel amount = 10 x frame time x wish speed, capped at add speed; velocity += accel
     amount x wish direction.
6. Position update: origin += frame time x velocity.

So "keeps moving when no more acceleration is wanted" means: once the spectator is already at or above
the wished speed along the wished direction (including when no key is held, wish speed 0, while still
gliding), the old step skipped the position update entirely, freezing the origin for that command even
though velocity was non-zero; the 36 step still moves the origin by velocity x frame time. [read; the
consequence is derived]

### 2. Settings, minor version, other move types

- No setting controls the multiplier, the fly hack or the flight friction; they are chosen by the
  client's major protocol only (35/36 for the multiplier; 36 for the fly hack and friction 4). The minor
  version plays no part in them. [read]
- The server's QW-mode setting (applied to 36 clients only) changes the maximum speed used by the clamp
  (to 320) but nothing else in the flight step. [read]
- Dead, gibbed and frozen move types never run the flight step and use the plain frame time without the
  multiplier; none of this applies to them. [read]

## B. Protocol extensions (game feature bit 15)

### 3. Limits and the configstring layout

The server switches to the extended layout when, after the game's Init returns, the game's exported
feature value (the g_features setting) has bit 15 set; it prints "Game supports Q2PRO protocol
extensions." to the console. [read] Limits, old -> new [read]:
- Entities (edicts): 1024 -> 8192 (entity numbers carried in 13 bits).
- Models: 256 -> 8192 (comment: half reserved for inline brush models). Index 255 is still always
  skipped when allocating model indexes (kept for the player model).
- Sounds: 256 -> 2048.
- Images: 256 -> 2048.
- Light styles 256, items 256, player skins (clients) 256, general 512 (= 2 x clients): unchanged in
  count, only moved.
- Entities per client frame: 128 -> 512 (unless a server setting caps it).
- Configstring count: 2080 -> 13630.

Configstring indexes [read the definitions; derived the arithmetic]:
| block | old index | new index |
| --- | --- | --- |
| name, cd track, sky, sky axis, sky rotate, status bar | 0..5 | 0..5 (unchanged) |
| air acceleration | 29 | 59 |
| max clients | 30 | 60 |
| map checksum | 31 | 61 |
| models | 32 (256 slots) | 62 (8192 slots) |
| sounds | 288 (256) | 8254 (2048) |
| images | 544 (256) | 10302 (2048) |
| lights | 800 (256) | 12350 (256) |
| items | 1056 (256) | 12606 (256) |
| player skins | 1312 (256) | 12862 (256) |
| general | 1568 (512) | 13118 (512) |
| end (count) | 2080 | 13630 |

Other effects inside the server when extended [read]: the edict minimum size grows to include a per-entity
extension block (see 7); the world trace and point contents use the "extended" contents of the map (a
second contents value per leaf that also folds in brush contents) and the fixed trace behaviour (a box
that starts all-solid gets fraction 0 and the brush's contents); the clip skips entities flagged as player
or projectile unless the content mask asks for player or projectile contents; the bounding-box packing
for prediction changes to a 32-bit form (one byte each: x half-width, y half-width, down distance, up
distance + 32); the serverinfo key "protocol" is set to "36" instead of "34".

### 4. Wire changes for a client on it

Two levels exist on the wire: "extensions" (granted when the server is extended) and "extensions 2"
(granted only when the server is extended AND the game is the newer interface 3302). A third,
"more bits", is granted on top of extensions 2 to clients of minor 1026 or later. [read]

Server data [read]: the major protocol sent is the client's own (36), then the usual fields, then for 36:
minor as a 16-bit word, the server state byte, and (for minor >= 1024) a 16-bit flags word with: bit 0
strafe-jump hack, bit 1 QW mode, bit 2 water-jump hack, bit 3 protocol extensions (set when the server is
extended), bit 4 extensions 2 (set when the client was granted extensions 2). A client receiving bit 4
without bit 3 drops with "Q2PRO_PF_EXTENSIONS_2 without Q2PRO_PF_EXTENSIONS". [read]

Entity numbers [read]: still a byte, or a 16-bit word when the number has any bit in 0xff00 (the
"number 16" flag); valid numbers go up to 8191.

Entity delta [read]:
- Model indexes: when extended and any model field is being sent and any of the four model indexes has a
  bit in 0xff00, a new flag (bit 28, "model 16") makes ALL sent model indexes 16-bit words; otherwise bytes.
- Looping sound field (bit 26): when extended it is a 16-bit word: low 14 bits the sound index, bit 14 =
  a loop-volume byte follows, bit 15 = a loop-attenuation byte follows. The sound flag is also raised when
  only loop volume or loop attenuation changed. Without extensions it is a byte.
- New fields, extended only: "more effects" (bit 29 for 8 bits, bit 33 for 16, both for 32; a byte, a
  word or a long), alpha (bit 30, a byte, value x 255 clamped 1..255, 0 = default), scale (bit 32, a byte,
  value x 16 clamped 1..255, 0 = default; 16 is sent as 0). Bit 31 says a fifth bits byte follows. The
  writer sends 1 to 5 bits bytes.
- Loop attenuation byte: value x 64 clamped 1..255; "static" (192) is coded as 0 and "no attenuation"
  (-1) is coded as 192. Loop volume: x 255, 255 sent as 0.
- Coordinates, extensions 2 only: each origin component is a delta-coded 23-bit value: if (new - old) in
  1/8 units lies in -16384..16383 it is a 16-bit word holding the delta shifted left by one (low bit 0);
  otherwise a 24-bit little-endian value holding the absolute value shifted left by one with the low bit
  1 (sign-extended from 23 bits on read). Old origin uses the same 23-bit form but always absolute (delta
  from 0). Without extensions 2, origins are 16-bit words as before.
- Angles: unchanged by the extensions (bytes, or words for brush models on minor >= 1018).
- Solid: a long for clients with the long-solid flag (all 36 clients); when extended the value is the
  32-bit packing described in 3.
- When extended the client keeps all renderfx bits; without it the client masks off bits from the
  lite-green shell upward.

Player state (36 enhanced form) [read]:
- Gun index (weapon index field): a 16-bit word when extended (low 13 bits the model index, top 3 bits a
  skin number for the gun); a byte otherwise.
- Extensions 2: pmove origin and velocity components are delta-coded 23-bit values (as above, delta from
  the previous frame's value); pm_time and pm_flags are 16-bit words (otherwise bytes).
- Extensions 2: up to 64 stats; the stat mask is a variable-length integer (7 bits per byte, high bit =
  more follows, least significant group first) instead of a 32-bit long; each sent stat a 16-bit word.
- Extensions 2: the blend flag is also raised when only the damage blend changed; the blend is then
  written as a flags byte (bits 0-3 = blend r,g,b,a present; bits 4-7 = damage blend r,g,b,a present)
  followed by the present bytes. Without it the blend is 4 bytes.
- More bits (minor >= 1026, with extensions 2): the player-state flags word gets a third byte (bit 15 of
  the word says it follows); bit 16 = fog. Fog: a bits byte, then by bit: 0 colour (3 bytes), 1 density
  (a long: low 16 bits density x 65535, high 16 bits sky factor x 65535), 2 height-fog density (word,
  x 65535), 3 height falloff (word, x 65535), 4 height start colour (3 bytes), 5 height end colour
  (3 bytes), 6 height start distance (word, coordinate x 8), 7 height end distance (word, coordinate x 8).
- The client number in the frame (36) is a 16-bit word for minor >= 1022 (a byte before), valid range -1
  up to max entities - 2.

Sound message [read]: flags byte; bit 5 "index 16": when the server is extended and the sound index is
above 255 the index is a 16-bit word, else a byte. The entity/channel field is unchanged: a 16-bit word,
entity number shifted left 3 with the channel in the low 3 bits (13 entity bits fit exactly). The
position (sent when the entity is not in the client's frame, or forced) is three 16-bit words, or three
23-bit coded values for an extensions-2 client.

Temporary entities and positions written by the game [read]: the game's write-position call writes 23-bit
coded values (absolute) when the server is extended AND the game is 3302; otherwise three 16-bit words.
The client reads temp-entity positions in the 23-bit form when it holds extensions 2. Entity numbers
inside temp entities stay 16-bit words.

Muzzle flashes [read]: the player muzzle flash is unchanged (entity word, weapon byte with the silenced
bit). For the monster muzzle flash, when extended, the top 3 bits of the 16-bit entity word carry bits
8-10 of the flash number (entity = low 13 bits), so flash numbers up to 2047 fit; the byte carries the
low 8 bits.

Configstring message [read]: the index is a 16-bit word (always, extended or not); the gamestate and
configstring stream end with the configstring count (13630 when extended) as the terminator word.

Print message [read]: when extended, two extra print levels exist after chat: "typewriter" and "center",
which a client shows as a centre print (typewriter one letter at a time) instead of console text.

Frame [read]: the extended client gets entity frames of up to 512 entities; its parse buffers are sized
for that.

Anything else [read]: with extensions the server also stops sending entities flagged as no-cull through
the PVS check (always sent), culls looping sounds by distance, honours a low-priority renderfx bit when
the frame overflows, lets a client's "no flares" setting drop flare entities, and keeps the newer
footstep/ladder-step events. These are server behaviours, not encoding.

### 5. Who may connect, and the refusal

[read] When the server is extended, only a protocol 36 client may connect, and its minor must be at least
1024 (old interface game) or at least 1025 (game is 3302). Protocol 34 and 35 clients, and older 36
clients, are refused. Note the client's minor is first clipped into 1015..1026 (1016 is never used: it
becomes 1015). The refusal is an out-of-band print packet (the connectionless "print" command, then the
text) to the address that sent the connect, with exactly:

"This is a protocol limit removing enhanced server.\nYour client version is not compatible. Make sure you are running the latest Q2PRO client version.\n"

i.e. two lines. Nothing goes to the server console from this check. [read]

Other connect refusals nearby, same channel: "Unsupported protocol version %d.\n" (major outside 26..36)
and "You need Quake 2 version 3.19 or higher.\n" (major below 34). [read] The challenge reply always
advertises "p=34,35,36" regardless of extension. [read]

### 6. What a game library must do

[read] Only the feature bit is needed to turn on the extended limits; the server checks the game's
g_features value for bit 15 right after calling the game's Init. The server publishes its own supported
features in a read-only setting sv_features (bit 15 included in this build); the sample game sets bit 15
in g_features only when sv_features has bit 15 AND its own latched setting g_protocol_extensions is
non-zero (default "0"), and switches its own configstring layout to the new one at the same moment.
[read] The interface version 3302 is NOT required for the extended limits, but it is required for the
"extensions 2" level (23-bit coordinates, 64 stats, damage blend, fog, wider pm_time/pm_flags) and for
the move's 23-bit coordinates and millisecond timers. [read]

If the game sets the bit on a server that cannot do it [derived]: the server reads the bit itself; this
server build always supports it (it is in its sv_features). A server built without protocol extensions
does not exist in this tree's server build (the server build always compiles them in). A well-behaved
game checks sv_features first; the source does not describe what an older third-party server does with
the bit. [the source does not say beyond this]

Checks after the bit is seen [read]: the game's edict size must be at least the full edict including the
extension block (otherwise the size up to that block), at most INT_MAX / max edicts, and a multiple of the
edict's alignment, else "Game library returned bad size of edict_t"; max edicts must be above max clients
and at most 8192 (1024 without the bit), else "Game library returned bad number of max_edicts". Both are
fatal drop errors.

## C. The newer game interface (version 3302)

### 7. Telling them apart, layouts, behaviour

How [read]: the game's export table's first field, the interface version, is read after the entry point
returns. 3 = the original interface, 3302 = the newer; anything else is a fatal error "Game library is
version %d, expected %d or %d". The import and export TABLES are the same shape for both (same calls in
the same order). A separate, optional extended table is fetched through a second entry point
(GetGameAPIEx) for either version and is independent of 3302.

Client record (the part the server reads at the start of the game's client struct) [read]:
- Version 3: the original player state; then ping (int); then client number (int, entity number - 1 of
  the point-of-view entity; read only when feature bit 0 is set).
- 3302: the newer player state (below); then ping (int); then client number (int). Same order.

Move state (pmove state) [read]:
- Version 3: move type (enum, int-sized); origin 3 x 16-bit signed (12.3 fixed); velocity 3 x 16-bit
  signed; flags 8-bit; timer 8-bit (units of 8 ms); gravity 16-bit signed; delta angles 3 x 16-bit signed.
- 3302: move type (enum); origin 3 x 32-bit signed (19.3 fixed; comment); velocity 3 x 32-bit signed;
  flags 16-bit unsigned; timer 16-bit unsigned in milliseconds; gravity 16-bit signed; delta angles 3 x
  16-bit signed.
- 3302 flags add bit 7 "ignore player collision" (the same bit the original server family used for a
  teleport marker) and bit 8 "on ladder".

Player state [read]:
- Version 3: move state; view angles (3 floats); view offset (3 floats); kick angles (3 floats); gun
  angles (3 floats); gun offset (3 floats); gun index (int); gun frame (int); blend (4 floats); field of
  view (float); refdef flags (int); stats 32 x 16-bit.
- 3302: newer move state; view angles; view offset; kick angles; gun angles; gun offset (each 3 floats);
  gun index (int); gun frame (int); two reserved ints; blend (4 floats); damage blend (4 floats); fog
  (colour 3 floats, density float, sky factor float); height fog (start: colour 3 floats + distance float;
  end: colour 3 floats + distance float; then density float, falloff float); field of view (float); refdef
  flags (int); two reserved ints; stats 64 x 16-bit.

Move record passed to the move call [read]:
- Version 3: move state; user command; snap-initial flag (int-sized boolean); touch count (int); 32
  touched entity pointers; view angles (3 floats); view height (float); mins, maxs (3 floats each);
  ground entity pointer; water type (int); water level (int); trace callback (start, mins, maxs, end);
  point-contents callback.
- 3302: the newer move state; then the same fields, except a ground plane record (normal 3 floats,
  distance float, type byte, sign bits byte, 2 pad bytes) is inserted after the ground entity pointer, and
  the trace callback takes a fifth argument, a content mask.
- The 3302 move fills the ground plane from the ground trace, uses the content mask "ladder" for the
  ladder probe (version 3 ignores the mask and traces with the default), and sets or clears the on-ladder
  flag at the end of each non-spectator move. [read]

Entity state and edict [read]: the entity state is the same struct for both versions. The edict is the
same for both versions; with the extensions bit, the server also reads an extension block placed right
after the owner pointer (before the game's own fields): more effects (int), alpha (float), scale (float),
loop volume (float), loop attenuation (float). New server flags exist for extended servers: player (bit
3), bot (4), no bots (5), respawning (6), projectile (7), instanced (8), door (9), no cull (10), hull (11).

User command [read]: the same for both: msec byte, buttons byte, 3 x 16-bit angles, forward/side/up 16-bit
each, impulse byte, light level byte.

What the server does differently for 3302 [read]:
- Runs the newer move (32-bit state, the ground plane, the ladder mask) instead of the original.
- Reads the client's view origin, stats, client number and writes ping through the newer client record.
- Packs the newer player state for the wire (64 stats, damage blend, fog).
- If ALSO extended: the move's parameters switch to timer shift 0 (pm_time and the timers in
  milliseconds; the original uses 8 ms units, so values like 2040 ms become 255 units there) and
  coordinate bits 23 (packed origin and velocity sign-extended from 23 bits, so a range of about
  +/- 524288 units); the game's write-position call uses the 23-bit coding; clients are granted
  "extensions 2" (and "more bits" at minor 1026); the minimum client minor becomes 1025.
- If 3302 but NOT extended: the move keeps timer shift 3 and 16-bit coordinates (values wrap as in the
  original), the wire is the ordinary protocol, and 32-bit origins are sent truncated to 16 bits. [read
  the parameters; derived the truncation from the 16-bit writes]
- The call arguments of the import and export tables do not differ between the versions; only the
  pointer passed to the move call points at a different record. [read]
