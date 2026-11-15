# The sequel's game library: hosting it unmodified (answers)

Clean-room answers for M18v10: what a server must provide so an existing, unmodified game library
of the sequel (the stock rules or a mod's) loads and runs a deathmatch on it. Source read: the
Yamagi Quake II source, release 8.71pre, the modern port of id Software's GPL-2.0 release of
Quake II (its server, its shared code and its game library), as built for 64-bit x86-64 Linux.
Where the port behaves differently from what the original release is known to do, the answer says
"the port" and leaves the original open unless `sequel.md` already states it.

Every fact is marked: **[read]** stated by the source, **[measured]** printed by a small program of
the reader's own (structure sizes and offsets compiled against the published interface headers with
GCC on x86-64 Linux, and the port's own built stock game library loaded and its entry point called
with an empty table), **[derived]** worked out from read or measured facts. Nothing in this document
is copied from the source. The names in the layout tables are the interface's own field names, the
names a mod author compiles against; they are the binary interface, not internal names.

Related notes: the player move, the brush trace and point contents are `sequel-movement.md`; every
server message's bytes (the sound, the configstring, the print, the frame, the entity and player
deltas, the coordinate, angle and direction encodings) are `sequel-demo.md`; the client's side of
the connection and the signon commands are `sequel-net.md`.

Conventions: "integer" is a signed 32-bit two's complement value, "float" an IEEE 754 single,
"vector" three floats (12 bytes), "pointer" 8 bytes on x86-64 (4 on 32-bit x86), "yes/no" a 32-bit
integer (0 no, anything else yes) **[read]**. "Text" is a pointer to zero-terminated bytes.
"Printf-style" means a C variadic call with a format text and its arguments, formatted with the C
library's rules. All calls each way use the platform's C calling convention (System V AMD64 on
x86-64 Linux) **[derived]**. Records returned by value larger than 16 bytes (the trace result, 72
bytes) are returned through a hidden pointer the caller passes, as the C ABI does for any such
record **[derived]**: our host's functions must be declared with C linkage and the identical record.

---

## 1. The entry point, the version, the file and where it is looked for

- **The one exported symbol** is `GetGameAPI` **[read]** **[measured]**: the port's built stock
  library exports exactly one defined dynamic symbol, that one. It takes one pointer, to the
  engine's table of calls (question 2), and returns a pointer to the game's table (question 3)
  **[read]**. The stock library copies the engine's table into its own storage at that moment, so
  the engine's table need not outlive the call **[read]**; a host should still keep it alive for
  the library's lifetime, since a mod may keep the pointer instead **[derived]**.
- **The interface version is 3** **[read]**. The library writes it into the first field of the
  returned table (`apiversion`) **[read]** **[measured]**: the stock library returned 3. The engine
  does not pass a version to the library; the library has no say. After the call the engine
  compares the returned number with 3; any other number is an error that stops the server ("game
  is version N, not 3"), the map is not started **[read]**. The same error-stop happens when no file
  was found or the symbol is missing ("failed to load game DLL") **[read]**.
- **What `GetGameAPI` does in the stock library** besides filling the table: it records the size of
  one entity, seeds its own random generator, and returns. It does not register settings or
  allocate; that waits for `Init` **[read]**. Measured on the stock library before `Init`: the
  entity size field held 1120 (bytes, 64-bit), the entity count and the most entities held 0
  **[measured]**.
- **The file name.** The port's 64-bit Linux engine looks for `game.so` in each folder, and no other
  name **[read]**. (On macOS `game.dylib`; on Windows `game.dll` first, then `gamex86.dll` in the
  same folder before moving to the next folder **[read]**.) The original release's Linux library
  was `gamei386.so` (`sequel.md`); the port does not look for that name **[read]**. A host that
  wants both should try both **[derived]**.
- **The folders, in order.** The engine walks its search path's plain folders (it skips the packs):
  first the folder it writes to, then every folder on the search path in search order **[read]**.
  The search path puts a mod's folders before the base folder `baseq2` (a mod named by the `game`
  setting is added on top) **[read]**, and the port's search path is built from several roots (the
  user's home folder, the executable's folder, the data folder; the most recently added searched
  first), each contributing its `<root>/baseq2` and, with a mod, `<root>/<mod>` **[read]**. In
  practice: the mod's folder (in each root), then `baseq2` (in each root) **[derived]**.
- **A folder without the file** is passed over; the next one is tried **[read]**. So a mod folder
  with no library of its own runs the base folder's library with the mod's data (that is how
  map-only and model-only mods work) **[derived]**. **A file that exists but fails to load** (wrong
  architecture, missing dependency) stops the search at once: the loader's error text is printed and
  the load fails as above; the engine does not fall back to the base folder **[read]**.
- The library is opened with every symbol resolved at load time (no lazy binding) **[read]**.
- When the library is unloaded (the server shuts down, or a fresh game starts) the engine calls its
  `Shutdown` and then closes the file **[read]**.

## 2. The engine's table (engine to game), in order

44 pointers, 352 bytes on x86-64 (176 on 32-bit) **[measured]** **[derived]**. Every entry must be
filled: the library calls through them without checking **[derived]**. Offsets are on x86-64
**[measured]**.

| # | Offset | Name | Takes | Returns |
| --- | --- | --- | --- | --- |
| 1 | 0 | `bprintf` | integer print level, printf-style text | nothing |
| 2 | 8 | `dprintf` | printf-style text | nothing |
| 3 | 16 | `cprintf` | pointer to an entity (may be null), integer print level, printf-style text | nothing |
| 4 | 24 | `centerprintf` | pointer to an entity, printf-style text | nothing |
| 5 | 32 | `sound` | pointer to an entity, integer channel, integer sound index, float volume, float attenuation, float time offset | nothing |
| 6 | 40 | `positioned_sound` | pointer to a vector (may be null), pointer to an entity, integer channel, integer sound index, float volume, float attenuation, float time offset | nothing |
| 7 | 48 | `configstring` | integer index, text | nothing |
| 8 | 56 | `error` | printf-style text | never returns |
| 9 | 64 | `modelindex` | text | integer |
| 10 | 72 | `soundindex` | text | integer |
| 11 | 80 | `imageindex` | text | integer |
| 12 | 88 | `setmodel` | pointer to an entity, text | nothing |
| 13 | 96 | `trace` | pointers to four vectors (start, mins, maxs, end; mins and maxs may be null), pointer to the entity to pass through (may be null), integer contents mask | the trace result (72 bytes, by value) |
| 14 | 104 | `pointcontents` | pointer to a vector | integer contents |
| 15 | 112 | `inPVS` | two pointers to vectors | yes/no |
| 16 | 120 | `inPHS` | two pointers to vectors | yes/no |
| 17 | 128 | `SetAreaPortalState` | integer portal number, yes/no open | nothing |
| 18 | 136 | `AreasConnected` | two integer area numbers | yes/no |
| 19 | 144 | `linkentity` | pointer to an entity | nothing |
| 20 | 152 | `unlinkentity` | pointer to an entity | nothing |
| 21 | 160 | `BoxEdicts` | pointers to two vectors (mins, maxs), pointer to an array of entity pointers, integer most, integer area kind | integer count |
| 22 | 168 | `Pmove` | pointer to the move block (question 7) | nothing |
| 23 | 176 | `multicast` | pointer to a vector (may be null for "all"), integer destination | nothing |
| 24 | 184 | `unicast` | pointer to an entity, yes/no reliable | nothing |
| 25 | 192 | `WriteChar` | integer | nothing |
| 26 | 200 | `WriteByte` | integer | nothing |
| 27 | 208 | `WriteShort` | integer | nothing |
| 28 | 216 | `WriteLong` | integer | nothing |
| 29 | 224 | `WriteFloat` | float | nothing |
| 30 | 232 | `WriteString` | text | nothing |
| 31 | 240 | `WritePosition` | pointer to a vector | nothing |
| 32 | 248 | `WriteDir` | pointer to a vector (may be null) | nothing |
| 33 | 256 | `WriteAngle` | float | nothing |
| 34 | 264 | `TagMalloc` | integer size, integer tag | pointer |
| 35 | 272 | `TagFree` | pointer | nothing |
| 36 | 280 | `FreeTags` | integer tag | nothing |
| 37 | 288 | `cvar` | text name, text default value, integer flags | pointer to a setting record (question 8) |
| 38 | 296 | `cvar_set` | text name, text value | pointer to a setting record |
| 39 | 304 | `cvar_forceset` | text name, text value | pointer to a setting record |
| 40 | 312 | `argc` | nothing | integer |
| 41 | 320 | `argv` | integer | text |
| 42 | 328 | `args` | nothing | text |
| 43 | 336 | `AddCommandString` | text | nothing |
| 44 | 344 | `DebugGraph` | float value, integer colour | nothing |

What each must do (the port's behaviour) **[read]** unless marked:

- **Prints.** The engine formats into a buffer of 2048 bytes for `bprintf` and 1024 for the others,
  truncating longer text. `bprintf` sends a print message (level byte, then the text) on the
  reliable channel of every client that has finished joining (spawned), and on a dedicated server
  echoes the text to the console with the top bit of every byte cleared. `cprintf` with a null
  entity prints to the server console; with an entity that is not a client slot (number outside 1
  to `maxclients`) it is an error that stops the map ("cprintf to a non-client"); otherwise the print
  goes on that client's reliable channel. `dprintf` prints to the server console only.
  `centerprintf` sends the centre-print message on that client's reliable channel; to an entity
  that is not a client slot it does nothing (a null entity is not guarded against).
- **Print levels**: 0 low (pickups), 1 medium (deaths), 2 high (critical), 3 chat. The engine writes
  the level byte and does not filter on it; the client decides what to show. (The original release
  filtered on each client's `msg` user info; the port does not.) **[read]**
- **`error`** never returns: the engine prints "Game Error: " and the text, calls the library's
  `Shutdown`, unloads it, tells every client the server crashed, and unwinds straight out of the
  library's call to the engine's main loop (a non-local jump). The server is left with no map
  until a new `map` command **[read]**. A host must likewise never return into the library from this
  call **[derived]**.
- **Sound** (`sound`, `positioned_sound`). `sound` is `positioned_sound` with no position; a null
  entity is ignored. The channel's low 3 bits are the channel (0 auto, 1 weapon, 2 voice, 3 item, 4
  body); bit 8 (`CHAN_NO_PHS_ADD`) sends to everyone rather than the hearable set; bit 16
  (`CHAN_RELIABLE`) sends on the reliable channel. A volume outside 0 to 1 becomes 1, an
  attenuation outside 0 to 4 becomes 0, a time offset outside 0 to 0.255 s becomes 0. An entity
  numbered above 4095 plays nothing. The sound is **positioned** (carries a position on the wire)
  when a position is given, or the entity has `SVF_NOCLIENT`, or the entity is `SOLID_BSP`; the
  position used is the one given, else the entity's origin, except a `SOLID_BSP` entity's which is
  its origin plus the middle of its box. Routing: to the hearable set of that position, unless the
  no-hearable-set bit is set or the attenuation is exactly 0 (`ATTN_NONE`), which send to all; the
  reliable bit picks the reliable variant. The bytes are the sound message in `sequel-demo.md`.
  Attenuations the interface names: 0 none, 1 normal, 2 idle, 3 static.
- **`configstring`**: question 12.
- **The index calls** (`modelindex`, `soundindex`, `imageindex`): a null or empty name returns 0. A
  name already in the list returns its index (exact, case-sensitive match). A new name takes the
  first empty slot from 1 upwards and, unless the map is still loading, is sent at once to every
  connected client as a configstring update on the reliable channel. The lists hold indices 1 to
  255 (slot 0 is "none"). A full list returns 0 and prints "configstrings overflowed" once per
  name; a name of 64 bytes or more (it does not fit with its terminator) returns 0 too.
- **`setmodel`**: a null name is an error that stops the map. The entity's model index is set to the
  name's model index (the name is added if new). A name starting with `*` is an inline model of the
  map: the number after the `*` must be 1 up to the map's inline model count minus 1, else an error
  that stops the map; the entity's `mins` and `maxs` are set to that model's box and the entity is
  linked (as `linkentity`). Other names leave the box alone and do not link.
- **`trace`**: a null `mins` or `maxs` is a point (all zeros). The world is traced first with the
  mask; if the world stops the move at its start (fraction 0) that result is returned at once. Then
  every entity whose linked box touches the box enclosing the whole move (grown by 1 unit each way)
  in the solid list is clipped, except: the pass entity itself; entities whose `owner` is the pass
  entity; the pass entity's own `owner`; entities with `SVF_DEADMONSTER` when the mask lacks the
  dead-monster contents bit. A `SOLID_BSP` entity clips with its model's hull, turned by its angles.
  Any other entity clips as its axis-aligned box (boxes never rotate), with the contents bit of a
  monster. A result replaces the best so far when it is all-solid, start-solid or shorter; once the
  result is all-solid the search stops; a start-solid result stays start-solid. **The result's
  entity is the world (entity 0) whenever no entity was hit, also when nothing at all was hit** (the
  entity is never null), and the surface is never null (question 7). The shape of the brush trace
  itself is `sequel-movement.md`.
- **`pointcontents`**: the world's contents at the point, OR-ed with the contents of every solid
  entity whose box contains the point (a `SOLID_BSP` entity's hull tested exactly, turned by its
  angles; a box entity contributes the monster bit).
- **`inPVS`, `inPHS`**: yes when the second point's cluster is in the first point's potentially
  visible (or hearable) set, and the two points' areas are connected; a point outside every
  cluster (in the void, cluster -1) gives no.
- **`SetAreaPortalState`, `AreasConnected`**: question 17.
- **`linkentity`** (what it sets): it first unlinks the entity if linked. The world (entity 0) and
  an entity not in use are not linked (they stay unlinked). Then it sets `size` (maxs minus mins),
  the packed `s.solid` (question 5); the absolute box `absmin`, `absmax` (origin plus mins and maxs,
  grown by 1 unit each way; a `SOLID_BSP` entity with any non-zero angle instead gets a cube of plus
  and minus the largest absolute component of its mins and maxs around its origin, then grown by
  1); the area numbers `areanum` and `areanum2` (the first non-zero area among the leaves the
  absolute box touches, and a second, different one when the box straddles two; a third overwrites
  the second); the clusters `num_clusters` and `clusternums` (the distinct clusters, other than -1,
  of every leaf the absolute box touches); and when the box touches 128 leaves or more, or more than
  16 distinct clusters, `num_clusters` is -1 and `headnode` is the top node of the leaf search
  instead. On the entity's first link (`linkcount` 0) `s.old_origin` is set to `s.origin`.
  `linkcount` is then increased by 1. A `SOLID_NOT` entity stops there: it is visible to clients but
  in no collision list. Others are put in the area tree's solid list (or trigger list for
  `SOLID_TRIGGER`). Relinking is required whenever the origin, the box or the solid kind changes,
  and the engine never relinks on its own (except `setmodel` on an inline model).
- **The area tree** **[read]**: 32 nodes, 4 levels deep, built at each map load over the world
  model's box. A node splits its box in half along x when its x size is larger than its y size,
  else along y; z is never split. An entity is put in the deepest node whose split plane its
  absolute box does not cross, appended at the end of that node's list.
- **`unlinkentity`**: removes the entity from the area tree; nothing if not linked. It leaves the
  other link fields as they were.
- **`BoxEdicts`**: lists the entities whose absolute boxes touch the given box (touching faces
  count; the test is on `absmin`, `absmax`). Area kind 1 (`AREA_SOLID`) searches the solid lists,
  any other value, normally 2 (`AREA_TRIGGERS`), the trigger lists. `SOLID_NOT` entities are
  skipped. The order is the tree's: a node's own list in link order, then the upper half, then the
  lower half, depth first. At `most` entries it stops, prints "MAXCOUNT", and returns `most`.
- **`Pmove`**: runs the shared player move on the block (question 7, `sequel-movement.md`). It also
  reads one engine-side value not in the block: the air acceleration, set when each map starts to
  the `sv_airaccelerate` setting in deathmatch and 0 otherwise (question 15).
- **`multicast`, `unicast` and the write calls**: question 10.
- **Memory**: question 11. **Settings**: question 8. **Arguments**: question 9.
- **`AddCommandString`**: appends the text to the server's command buffer; it runs at the start
  of the next host frame, before the next server frame. The text must carry its own newline. The
  stock game uses it for the level change (`gamemap "<map>"`) and the ban list file.
- **`DebugGraph`**: a client-side debugging aid. **On the port's dedicated-only build this entry is
  never filled: it holds whatever was on the stack** **[read]**. The stock game never calls it
  **[read]**. A host should fill it with a function that does nothing **[derived]**.

## 3. The game's table (game to engine), in order

152 bytes on x86-64, 80 on 32-bit **[measured]** **[derived]**.

| Offset | Name | Type | Notes |
| --- | --- | --- | --- |
| 0 | `apiversion` | integer | 3 (4 bytes of padding follow on x86-64) |
| 8 | `Init` | function: nothing to nothing | once per game, after the load |
| 16 | `Shutdown` | function: nothing to nothing | before the unload |
| 24 | `SpawnEntities` | function: text map name, text entity text (writable), text spawn point; returns nothing | question 15 |
| 32 | `WriteGame` | function: text file name, yes/no autosave | question 16 |
| 40 | `ReadGame` | function: text file name | question 16 |
| 48 | `WriteLevel` | function: text file name | question 16 |
| 56 | `ReadLevel` | function: text file name | question 16 |
| 64 | `ClientConnect` | function: pointer to an entity, writable user info text; returns yes/no | question 14 |
| 72 | `ClientBegin` | function: pointer to an entity | question 14 |
| 80 | `ClientUserinfoChanged` | function: pointer to an entity, writable user info text | question 14 |
| 88 | `ClientDisconnect` | function: pointer to an entity | question 14 |
| 96 | `ClientCommand` | function: pointer to an entity | question 9 |
| 104 | `ClientThink` | function: pointer to an entity, pointer to a movement command | question 13 |
| 112 | `RunFrame` | function: nothing to nothing | one 100 ms frame |
| 120 | `ServerCommand` | function: nothing to nothing | question 9 |
| 128 | `edicts` | pointer to the first entity | the entity array |
| 136 | `edict_size` | integer | bytes per entity, the game's whole record (shared head plus its own fields) |
| 140 | `num_edicts` | integer | entities in use or ever used this level: the engine looks at indices 0 to this minus 1 |
| 144 | `max_edicts` | integer | the array's length |

Offsets **[measured]**; meanings **[read]**.

- **Entity n is at** `edicts` plus n times `edict_size` bytes; the engine never indexes by its own
  idea of the record's size **[read]**. Mods enlarge the record, so a host must do the same
  **[derived]**. The stock library's record is 1120 bytes on x86-64 **[measured]**.
- **When the engine reads the data fields** **[read]**: `edicts` and `edict_size` afresh on every
  entity access (there is no cached copy), `num_edicts` afresh at every loop over entities (the
  frame's entity list, baselines, the per-frame event clearing). `max_edicts` is never read by the
  port. **The game may raise `num_edicts` at any time** (the stock game raises it whenever it
  allocates a new entity past the end) and the engine sees it at its next loop **[read]**. The
  stock game allocates the array once in `Init` (its length from its `maxentities` setting, default
  1024, at least `maxclients` plus 1), sets `num_edicts` to `maxclients` plus 1, and never moves the
  array; at each `SpawnEntities` it clears the whole array in place **[read]**. The source does not
  promise the array never moves; a host that re-reads the pointer on every access (as the port
  does) is safe either way **[derived]**.
- **Entity 0 is the world**; entities 1 to `maxclients` are the client slots, slot i (from 0)
  using entity i plus 1 **[read]**.

## 4. The shared head of an entity

The engine reads and writes only this head; the game's own fields follow it. 280 bytes on x86-64
**[measured]**; 260 on 32-bit **[derived]** (pointers 4 bytes: `client` at 84, `inuse` 88,
`linkcount` 92, `area` 96 for 8, `num_clusters` 104, `clusternums` 108, `headnode` 172, `areanum`
176, `areanum2` 180, `svflags` 184, `mins` 188, `maxs` 200, `absmin` 212, `absmax` 224, `size`
236, `solid` 248, `clipmask` 252, `owner` 256).

| Field | Type | Size | Offset (x86-64) | Written by |
| --- | --- | --- | --- | --- |
| `s` | the network state (question 5) | 84 | 0 | the game; the engine fills `s.solid`, `s.number`, `s.old_origin` and clears `s.event` (question 5); `setmodel` sets `s.modelindex` |
| (padding) | | 4 | 84 | |
| `client` | pointer to the client's shared head (question 6), null for non-clients | 8 | 88 | the game |
| `inuse` | yes/no | 4 | 96 | the game (the engine's save of a level clears and restores the client slots' flags around the game's write; question 16) |
| `linkcount` | integer | 4 | 100 | the engine (`linkentity` adds 1) |
| `area` | two pointers (previous, next) linking the entity into an area tree list; both null when unlinked | 16 | 104 | the engine only |
| `num_clusters` | integer; -1 means "use `headnode`" | 4 | 120 | the engine |
| `clusternums` | 16 integers | 64 | 124 | the engine |
| `headnode` | integer | 4 | 188 | the engine (meaningful only when `num_clusters` is -1) |
| `areanum` | integer | 4 | 192 | the engine |
| `areanum2` | integer | 4 | 196 | the engine |
| `svflags` | integer bits | 4 | 200 | the game |
| `mins` | vector | 12 | 204 | the game; `setmodel` on an inline model |
| `maxs` | vector | 12 | 216 | the game; `setmodel` on an inline model |
| `absmin` | vector | 12 | 228 | the engine |
| `absmax` | vector | 12 | 240 | the engine |
| `size` | vector | 12 | 252 | the engine |
| `solid` | integer (the solid kind) | 4 | 264 | the game |
| `clipmask` | integer contents mask | 4 | 268 | the game; the engine never reads it |
| `owner` | pointer to an entity | 8 | 272 | the game; the engine reads it in the trace and the frame |

Sizes and offsets **[measured]**; writers **[read]**. The engine must keep the game's fields beyond
280 bytes untouched **[derived]**.

- **Server flags** **[read]**: 1 `SVF_NOCLIENT` (never sent to clients, even with effects; a sound
  from it is positioned), 2 `SVF_DEADMONSTER` (traces skip it unless the mask has the dead-monster
  bit; its `s.solid` is packed as 0), 4 `SVF_MONSTER` (treated as a monster for collision; the port
  clips it with the same box as anything else). Other bits are the game's own and the engine
  ignores them.
- **Solid kinds** **[read]**: 0 `SOLID_NOT` (no interaction), 1 `SOLID_TRIGGER` (in the trigger
  lists; touched only when inside, after moving), 2 `SOLID_BBOX` (its box), 3 `SOLID_BSP` (its
  inline model's hull).

## 5. The network state of an entity

84 bytes on both x86-64 and 32-bit (no pointers) **[measured]** **[derived]**.

| Field | Type | Size | Offset |
| --- | --- | --- | --- |
| `number` | integer, the entity's index | 4 | 0 |
| `origin` | vector | 12 | 4 |
| `angles` | vector | 12 | 16 |
| `old_origin` | vector | 12 | 28 |
| `modelindex` | integer | 4 | 40 |
| `modelindex2` | integer | 4 | 44 |
| `modelindex3` | integer | 4 | 48 |
| `modelindex4` | integer | 4 | 52 |
| `frame` | integer | 4 | 56 |
| `skinnum` | integer | 4 | 60 |
| `effects` | unsigned 32-bit bits | 4 | 64 |
| `renderfx` | integer bits | 4 | 68 |
| `solid` | integer, the packed box | 4 | 72 |
| `sound` | integer, a looping sound's index | 4 | 76 |
| `event` | integer, a one-frame event | 4 | 80 |

Layout **[measured]**. What the engine fills itself **[read]**:

- **`number`**: the engine sets the client slots' numbers (slot i gets i plus 1) when a game starts
  and again when each client asks for the level ("new"); it sets every other entity's number when
  it takes the level's baselines (question 15); and while building a client's frame, an entity
  whose number disagrees with its index is corrected (with a debug message). The stock game also
  sets it when it allocates an entity **[read]**.
- **`solid`** is packed by `linkentity`: for a `SOLID_BBOX` entity without `SVF_DEADMONSTER`, bits
  0 to 4 hold `maxs` x divided by 8, bits 5 to 9 hold minus `mins` z divided by 8 (each limited to 1
  to 31), and bits 10 to 15 hold `maxs` z plus 32, divided by 8 (limited to 1 to 63), each division
  truncated toward zero; a `SOLID_BSP` entity gets 31 (a value no box can produce, as the box's z
  fields are at least 1); everything else gets 0. Whatever the game wrote there is overwritten at
  each link. In a client's frame, an entity owned by that client's own entity is sent with
  `solid` 0, so the client does not predict against its own missiles.
- **`old_origin`**: copied from `origin` on the entity's first link and when the level's baselines
  are taken. Otherwise it is the game's: the stock game copies every entity's origin into it at the
  start of each of its frames **[read]**, and teleports and beams set it on purpose. When it goes on
  the wire is `sequel-demo.md`.
- **`event`**: cleared to 0 for every entity from 0 to `num_edicts` minus 1 at the very end of each
  server frame, after every client's frame has been sent **[read]**. An event set during a frame or
  during a client's move between frames therefore goes out in exactly one frame **[derived]**.

## 6. The shared head of a client

188 bytes on x86-64 and 32-bit: the player state (184 bytes) then `ping` (integer, at 184)
**[measured]**. The game's own client fields follow.

**The player state:**

| Field | Type | Size | Offset |
| --- | --- | --- | --- |
| `pmove` | the packed move state (below) | 28 | 0 |
| `viewangles` | vector | 12 | 28 |
| `viewoffset` | vector | 12 | 40 |
| `kick_angles` | vector | 12 | 52 |
| `gunangles` | vector | 12 | 64 |
| `gunoffset` | vector | 12 | 76 |
| `gunindex` | integer | 4 | 88 |
| `gunframe` | integer | 4 | 92 |
| `blend` | 4 floats (red, green, blue, alpha) | 16 | 96 |
| `fov` | float | 4 | 112 |
| `rdflags` | integer bits | 4 | 116 |
| `stats` | 32 signed 16-bit integers | 64 | 120 |

**The packed move state** (`pmove`, 28 bytes):

| Field | Type | Size | Offset |
| --- | --- | --- | --- |
| `pm_type` | integer: 0 normal, 1 spectator, 2 dead, 3 gib, 4 freeze | 4 | 0 |
| `origin` | 3 signed 16-bit, eighths of a unit | 6 | 4 |
| `velocity` | 3 signed 16-bit, eighths of a unit per second | 6 | 10 |
| `pm_flags` | byte bits: 1 ducked, 2 jump held, 4 on ground, 8 time is water jump, 16 time is land, 32 time is teleport, 64 no prediction | 1 | 16 |
| `pm_time` | byte, units of 8 ms | 1 | 17 |
| `gravity` | signed 16-bit | 2 | 18 |
| `delta_angles` | 3 signed 16-bit, 65536ths of a turn | 6 | 20 |

Layouts **[measured]**; values **[read]**. The number of stats is 32 **[read]**.

- **Who sets `ping`**: the engine, once per server frame just before `RunFrame`, for every client
  that has finished joining: the mean, in milliseconds, of the positive values among the last 16
  round trips (a frame's send time to the moment the client's move acknowledged it), or 0 when
  there are none **[read]**. The game only reads it (the scoreboard) **[read]**.
- The engine reads the player state; it never writes it **[read]** (question 13).

## 7. The command, the move block and the trace result

**The movement command** as `ClientThink` receives it (16 bytes, no padding) **[measured]**:

| Field | Type | Size | Offset |
| --- | --- | --- | --- |
| `msec` | byte, the command's length in ms | 1 | 0 |
| `buttons` | byte bits: 1 attack, 2 use, 128 any key | 1 | 1 |
| `angles` | 3 signed 16-bit, 65536ths of a turn | 6 | 2 |
| `forwardmove` | signed 16-bit | 2 | 8 |
| `sidemove` | signed 16-bit | 2 | 10 |
| `upmove` | signed 16-bit | 2 | 12 |
| `impulse` | byte | 1 | 14 |
| `lightlevel` | byte | 1 | 15 |

Button values **[read]**. The pointer handed to `ClientThink` points at the engine's own copy and is
valid only during the call **[derived]**.

**The move block** passed to `Pmove` (384 bytes on x86-64; 240 on 32-bit) **[measured]**
**[derived]**:

| Field | Type | Size | Offset (x86-64) | Offset (32-bit) | In or out |
| --- | --- | --- | --- | --- | --- |
| `s` | the packed move state | 28 | 0 | 0 | in and out |
| `cmd` | the command | 16 | 28 | 28 | in |
| `snapinitial` | yes/no: the state was changed outside the move | 4 | 44 | 44 | in |
| `numtouch` | integer | 4 | 48 | 48 | out |
| (padding) | | 4 | 52 | none | |
| `touchents` | 32 pointers to entities | 256 | 56 | 52 (128) | out |
| `viewangles` | vector | 12 | 312 | 180 | out |
| `viewheight` | float | 4 | 324 | 192 | out |
| `mins` | vector | 12 | 328 | 196 | out |
| `maxs` | vector | 12 | 340 | 208 | out |
| `groundentity` | pointer to an entity, or null | 8 | 352 | 220 | out |
| `watertype` | integer contents | 4 | 360 | 224 | out |
| `waterlevel` | integer, 0 to 3 | 4 | 364 | 228 | out |
| `trace` | pointer to a function: four pointers to vectors (start, mins, maxs, end), returns a trace result by value | 8 | 368 | 232 | in |
| `pointcontents` | pointer to a function: pointer to a vector, returns integer contents | 8 | 376 | 236 | in |

- The move zeroes the outputs first, then fills them **[read]**. The touch list holds at most 32
  (`MAXTOUCH`); an entity joins it each time a slide trace hits something, including the world, and
  the same entity can appear more than once **[read]**; the game removes duplicates before touching
  **[read]**. The two callbacks are the game's: the stock game's trace callback calls the engine's
  `trace` with the player's entity as the pass entity and the player or dead mask, and its
  contents callback is the engine's `pointcontents` itself **[read]**.

**The trace result** (72 bytes on x86-64; 56 on 32-bit) **[measured]** **[derived]**:

| Field | Type | Size | Offset (x86-64) | Offset (32-bit) |
| --- | --- | --- | --- | --- |
| `allsolid` | yes/no: the whole move was in a solid, the plane is not valid | 4 | 0 | 0 |
| `startsolid` | yes/no: the start was in a solid | 4 | 4 | 4 |
| `fraction` | float, 1 when nothing was hit | 4 | 8 | 8 |
| `endpos` | vector | 12 | 12 | 12 |
| `plane` | the plane (below) | 20 | 24 | 24 |
| (padding) | | 4 | 44 | none |
| `surface` | pointer to the surface (below) | 8 | 48 | 44 |
| `contents` | integer, the contents behind the hit surface | 4 | 56 | 48 |
| (padding) | | 4 | 60 | none |
| `ent` | pointer to the entity hit | 8 | 64 | 52 |

**The plane** (20 bytes, both): `normal` vector at 0, `dist` float at 12, `type` byte at 16 (0, 1,
2 for a plane facing along x, y, z; 3 to 5 otherwise, as the map format defines), `signbits` byte
at 17 (bit 0, 1, 2 set when the normal's x, y, z is negative), 2 bytes `pad` at 18 **[measured]**
**[read]**.

**The surface** (24 bytes, both): `name` 16 bytes of text at 0, `flags` integer at 16 (the surface
flags of `sequel-formats.md`), `value` integer at 20 **[measured]**. The name holds at most 15
characters; the engine keeps longer texture names elsewhere and the game sees the first 15
**[read]**.

- **When nothing is hit** the surface pointer points at a fixed empty surface (empty name, flags 0,
  value 0), never null; a box entity's sides report the same empty surface **[read]**. The entity is
  the world (question 2). The stock game nevertheless checks the surface for null before reading it
  in some places and not in others **[read]**; a host that never returns null is safe **[derived]**.

## 8. The setting record

56 bytes on x86-64 (32 on 32-bit) in the port **[measured]** **[derived]**:

| Field | Type | Size | Offset (x86-64) | Offset (32-bit) |
| --- | --- | --- | --- | --- |
| `name` | text | 8 | 0 | 0 |
| `string` | text, the current value | 8 | 8 | 4 |
| `latched_string` | text or null, a value waiting for the next game | 8 | 16 | 8 |
| `flags` | integer bits | 4 | 24 | 12 |
| `modified` | yes/no, set at each change | 4 | 28 | 16 |
| `value` | float, the text read as a number | 4 | 32 | 20 |
| (padding) | | 4 | 36 | none |
| `next` | pointer, the engine's list | 8 | 40 | 24 |
| `default_string` | text | 8 | 48 | 28 |

`default_string` is the port's addition, placed last so that libraries built against the original
48-byte (28 on 32-bit) record still work **[read]** **[derived]**.

- **What the game reads directly** **[read]**: `value` (most settings), `string` (texts such as
  passwords, the map list, the `game` folder) and `modified` (the stock game watches the two
  password settings' `modified` and clears it itself). It keeps the returned pointer for the whole
  game, so the record must not move or be freed while the library is loaded; the text pointers may
  change on each set **[read]** **[derived]**.
- **Flag values** **[read]**: 1 `CVAR_ARCHIVE` (saved to the config), 2 `CVAR_USERINFO` (part of a
  client's user info), 4 `CVAR_SERVERINFO` (part of the server info the status query returns), 8
  `CVAR_NOSET` (not settable from the console, only from the command line), 16 `CVAR_LATCH` (a
  change waits until the next game starts).
- **`cvar` (get)** **[read]**: a null default returns null. With the user-info or server-info flag,
  a name or default containing a backslash, a double quote or a semicolon is refused: it prints
  "invalid info cvar name" (or value) and returns **null**. If the setting exists, the flags are
  OR-ed into its flags, its value is left alone (the port also records the default) and the record
  is returned; else it is created with the default as its value, `modified` yes. (The port also
  renames a few deprecated setting names and maps `game` set to `baseq2` to empty.)
- **`cvar_set`** **[read]**: a missing setting is created with flags 0. An info setting refuses a
  value with those three characters (prints, returns the record unchanged). `CVAR_NOSET`: refused
  with "is write protected". `CVAR_LATCH`: if the value equals the current one (or the one already
  waiting), nothing; else, while a server is running, the value is kept in `latched_string` ("will
  be changed for next game") and `string`, `value` stay as they were; with no server running it
  applies at once (without setting `modified`). Otherwise, a value equal to the current one changes
  nothing; a different one replaces `string` and `value` and sets `modified`.
- **`cvar_forceset`** **[read]**: as `cvar_set` but ignores `CVAR_NOSET` and `CVAR_LATCH`: any
  waiting value is dropped and the new value applies at once (setting `modified` when it differs).
- **Latched values apply** when a game starts (a fresh `map`), before the library is loaded
  **[read]**.

## 9. The arguments

**[read]** throughout:

- **During `ClientCommand`**: the client's text command, split into words without expanding `$`
  settings: `argv(0)` is the command word (for example `say`), `argc()` counts the words, `args()` is
  the raw text after the first word (quotes kept, trailing white space removed). The engine handles
  its own commands first and never passes them on: `new`, `configstrings`, `baselines`, `begin`,
  `nextserver`, `disconnect`, `info`, `download`, `nextdl`. Every other word goes to `ClientCommand`,
  only while a game map (not a cinematic, picture or demo) is running. At most 7 text commands are
  run from one client packet; the rest are dropped.
- **During `ServerCommand`**: the console line that started with `sv`, split with `$` setting
  expansion: `argv(0)` is `sv`, `argv(1)` the game's command (for example `addip`), `args()` the
  text after `sv` (so it begins with the game's command). With no library loaded the engine prints
  "No game loaded".
- **Edge cases**: `argv` of a number outside 0 to `argc()` minus 1 returns an empty text, never null.
  Up to 80 words are kept (more are dropped from `argv` but stay in `args`); a word is at most 1023
  characters; `args` at most 2047. Double quotes group a word with spaces; `//` starts a comment.
  The texts are valid until the next command is split, so the game copies what it keeps.

## 10. The write calls, unicast and multicast

**[read]** throughout unless marked:

- **One shared buffer.** Every write call appends to one engine buffer of 1400 bytes (the most a
  message may be). `unicast` or `multicast` copies it to its destinations and empties it.
  Overflowing the 1400 bytes is a fatal error that ends the server process. The engine's own
  configstring updates, index additions, sounds, centre prints and broadcast commands use the same
  buffer: a `configstring` call empties it first (discarding a message the game had half written),
  and an index call that adds a new name appends to it and multicasts it (sending the half-written
  bytes reliably to everyone) **[read]** **[derived]**. The stock game always finishes a message
  before calling anything else **[derived]**.
- **Byte layouts** (all little-endian):

| Call | Bytes | Encoding |
| --- | --- | --- |
| `WriteChar` | 1 | the low 8 bits of the integer |
| `WriteByte` | 1 | the low 8 bits of the integer |
| `WriteShort` | 2 | the low 16 bits |
| `WriteLong` | 4 | the integer |
| `WriteFloat` | 4 | the float's IEEE bits |
| `WriteString` | length plus 1 | the bytes and a zero byte; a null text writes just the zero byte |
| `WritePosition` | 6 | three 16-bit values, each component times 8 truncated toward zero |
| `WriteDir` | 1 | the index of the direction table's entry with the largest positive dot product; null, or no positive dot product, gives 0 (`sequel-demo.md` § 9) |
| `WriteAngle` | 1 | degrees times 256 / 360, truncated toward zero, then the low 8 bits |

- **`unicast(entity, reliable)`**: a null entity does nothing. An entity that is not a client slot
  (number outside 1 to `maxclients`) does nothing **and leaves the buffer as it is**, so those bytes
  go out with the next message **[read]** **[derived]**. Otherwise the bytes are appended to that
  client's reliable channel (reliable yes) or to its datagram (reliable no), with no check of the
  client's state, and the buffer is emptied.
- **`multicast(origin, destination)`**: destinations 0 `MULTICAST_ALL`, 1 `MULTICAST_PHS`, 2
  `MULTICAST_PVS`, 3 `MULTICAST_ALL_R`, 4 `MULTICAST_PHS_R`, 5 `MULTICAST_PVS_R` (the `_R` ones
  reliable); any other value is a fatal error. For the hearable and visible kinds the set is that
  of the origin's cluster, and a client receives the message when its entity's `s.origin` (not its
  eye) is in a cluster in that set and in an area connected to the origin's area; the port also
  tries a point 32 units higher when the client's origin is in water. The "all" kinds ignore the
  origin (it may be null). Free slots and slots being dropped never receive; the unreliable kinds
  also skip clients that have not finished joining, while the reliable kinds reach them.
- **After**: reliable bytes join the client's reliable channel (1384 bytes; if it overflows, the
  client is dropped with "overflowed" at the next send). Unreliable bytes join the client's
  datagram, which is appended after the entities of the client's next frame packet; if the datagram
  overflowed its 1400 bytes, it is thrown away for that frame with a warning; if the whole frame
  packet overflows, it is emptied. Both go out when the server next sends to that client (after the
  next `RunFrame`).

## 11. Memory

**[read]** throughout:

- **`TagMalloc(size, tag)`** returns zeroed memory (the port allocates with the C library and
  zeroes it). A negative size is a fatal error; so is a size of 0 in the port. A tag outside 0 to
  65535 prints a message and becomes 0. Allocation failure is fatal: it never returns null.
- **`TagFree(pointer)`**: a null pointer is ignored; a pointer that is not one of these blocks is a
  fatal error.
- **`FreeTags(tag)`** frees every block with that tag. Tag 0 (the engine's own) or a tag outside 1
  to 65535 is refused with a message.
- **The tags the stock game uses** (defined by the game, not the engine): 765 for the whole game
  (the entity and client arrays, freed at `Shutdown`) and 766 for one level (entity texts, freed
  by the game itself when the next level spawns). The engine never frees the game's tags on its own
  **[read]**.

## 12. Configstrings

The table is 2080 entries of 64 bytes each (63 characters and a terminator) **[read]** **[derived]**:

| Index | Count | What the stock game puts there |
| --- | --- | --- |
| 0 | 1 | the level's name (the engine first puts the map's file name; the game overwrites it with the world's message) |
| 1 | 1 | the music track |
| 2 | 1 | the sky's name |
| 3 | 1 | the sky's rotation axis, as three numbers |
| 4 | 1 | the sky's rotation speed |
| 5 to 28 | 24 | the status bar layout: one text that may run on through the following entries (up to 1535 characters from index 5) |
| 29 | 1 | the air acceleration (set by the engine, question 15) |
| 30 | 1 | `maxclients`, as text |
| 31 | 1 | the map's checksum (set by the engine; the game must not change it) |
| 32 | 256 | models (32 plus 1 is the map itself, then the inline models; question 15) |
| 288 | 256 | sounds |
| 544 | 256 | images |
| 800 | 256 | light styles (pattern texts, a to z) |
| 1056 | 256 | items' pickup names |
| 1312 | 256 | player skins: index 1312 plus slot, "name\model/skin" |
| 1568 | 512 | general (free for mods) |

Indices **[read]** (they are the interface's own numbers); the stock game's uses **[read]**.

- **`configstring(index, text)`** **[read]**: an index outside 0 to 2079 prints "bad index" and is
  ignored. A null text is an empty text. A text that does not fit is refused with a message: 64
  bytes with its terminator for most entries; for an index i from 5 to 28 the room is (29 minus i)
  times 64 bytes.
- **When it is sent** **[read]**: while the map is loading (`SpawnEntities` and the two settling
  frames, question 15) nothing is sent; the table goes to each client in its signon. Any later set
  is sent at once to every connected client on the reliable channel. In the signon the engine sends
  every non-empty entry from its own start, so a long status bar is sent whole at index 5 and its
  tail again at each following index it reached **[read]** **[derived]**.

## 13. The server frame

**[read]** throughout unless marked. The host loop runs continuously; each pass runs the command
buffer (console text and `AddCommandString` text), then the server:

1. Each client that has been silent longer than `timeout` (125 s) is dropped ("timed out"); a
   dropped client's slot is freed after `zombietime` (2 s).
2. **Every waiting packet is read and handled as it comes.** A move message carries three commands
   (the two before and the newest); **`ClientThink` is called for each command at once, while the
   packet is read**, before and independent of the next `RunFrame`. With packets lost (fewer than
   20), the commands that can be recovered are run first, oldest first: the last command repeated
   for losses beyond two, then the two older commands in the message. Only one move message per
   packet counts. Moves from a client that has not finished joining, moves whose checksum fails, and
   all moves while paused are ignored. Each command's `msec` is taken from an allowance refilled to
   1800 ms every 16 frames; running out drops the command only when `sv_enforcetime` is set
   (default 0).
3. If less than 100 ms of server time has passed, the pass ends here (the server sleeps until
   then).
4. The pings are updated (question 6).
5. **`RunFrame` is called once**: the server's time advances by 100 ms each frame, and when the
   host has fallen behind, the server's clock is pulled forward rather than running two frames in
   a row. It is skipped only when paused with a single client slot.
6. **Each client's frame is built and sent** to every client that has finished joining (unless its
   rate is exceeded), with its reliable channel and datagram.
7. Clients still joining get their reliable channel sent too (in multiplayer, by default, this
   happens on every host pass instead).
8. **Every entity's event is cleared** (question 5).

**Which entities go into a client's frame** (entities 1 to `num_edicts` minus 1) **[read]**:

- The view point is the player state's origin (eighths, converted to units) plus `viewoffset`. The
  visible set is the union of the potentially visible sets of the clusters of every leaf within 8
  units of the view point each way; the hearable set is that of the view point's cluster.
- Skipped: entities with `SVF_NOCLIENT`; entities with `modelindex`, `effects`, `sound` and `event`
  all 0. `inuse` is **not** checked: a freed entity must have been cleared or flagged by the game
  (the stock game clears freed entities) **[read]**.
- The client's own entity skips the remaining tests. Every other entity is skipped when neither of
  its areas is connected to the view point's area; then an entity with `RF_BEAM` in `renderfx` is
  kept when its first cluster is in the hearable set; any other entity must have one of its
  clusters in the visible set (or, with `num_clusters` -1, its `headnode` must reach a visible
  cluster). Finally an entity with `modelindex` 0 (a sound or effect only) farther than 400 units
  from the view point is skipped.
- The entity's whole network state is copied as it is at that moment (with `solid` 0 when its owner
  is this client, question 5).
- **The player state** is copied whole from the client entity's `client` pointer at this moment,
  after `RunFrame`; a client entity whose `client` pointer is null gets no frame. The area bits sent
  with the frame are the areas connected to the view point's area.
- **Old origin** is not touched here; see question 5.

## 14. The connection flow

**[read]** throughout unless marked:

- **Connect.** When a valid connect request arrives (right protocol, valid challenge, a free slot or
  the same address reconnecting), the engine adds the key `ip` (the client's address and port) to
  the client's user info and calls **`ClientConnect`** with the slot's entity and a writable 512-byte
  user info buffer. **A no** refuses the client: if the game put a `rejmsg` key into the user info,
  the client is sent "print", that text and "Connection refused."; otherwise just "Connection
  refused.". **A yes**: the engine copies the (possibly changed) user info, and **calls
  `ClientUserinfoChanged` right away** (the stock game's connect also calls it itself, so it runs
  twice), then answers "client_connect" and starts the connection. Refusals the engine makes itself
  (full, bad challenge, wrong protocol) never reach the game.
- **Signon.** The client asks for the level ("new": the server data), the configstrings and the
  baselines in pieces (`sequel-net.md`), then sends "begin" with the level's spawn count.
  **`ClientBegin`** is called then, and the client counts as joined. A "begin" for an older level
  restarts the signon instead.
- **`ClientUserinfoChanged`** runs after a successful connect and whenever the client sends a new
  user info, joined or not. After the game's call the engine reads `name` (top bits cleared) and
  `rate` (100 to 15000, default 5000) for itself.
- **Level change.** The stock game ends a level by queueing `gamemap "<map>"` **[read]**. That
  command (as opposed to the console's `map`) keeps the library loaded: every joined client is put
  back to "connected", the clients are sent "changing", the new map is loaded and `SpawnEntities` is
  called **with the clients still connected**, and the clients are sent "reconnect"; each one then
  repeats the signon and `ClientBegin` is called again. **`ClientConnect` is not called again on a
  level change; `ClientBegin` is called on every level.** In a game map (not deathmatch only:
  always) the level being left is saved first (question 16).
- **The console's `map` command** starts a fresh game: clients are sent a final message and
  dropped, latched settings apply, the library is unloaded (its `Shutdown`) and loaded again
  (`GetGameAPI`, `Init`), then the map spawns. Clients that reconnect go through `ClientConnect`
  again **[read]**.
- **Drop** (the client's "disconnect", a timeout, a kick, an overflow): the client is sent the
  disconnect message; **`ClientDisconnect` is called only if the client had finished joining**; the
  slot then waits 2 s before reuse. A client dropped while still joining never gets
  `ClientDisconnect` although `ClientConnect` said yes. When the whole server shuts down, no
  `ClientDisconnect` is called; only `Shutdown`.

## 15. Spawning a level

**[read]** throughout:

- **`SpawnEntities(map name, entity text, spawn point)`**: the map name is the level's base name (no
  folder, no `.bsp`, no leading `*`); the entity text is the map's entity lump as zero-terminated
  text (the port first looks for an override file `maps/<name>@<checksum>.ent` or `maps/<name>.ent`
  when `sv_entfile` is set, default on); the spawn point is the text after a `$` in the map command,
  or empty. The game may write into the entity text buffer.
- **Before it, the engine has**: wiped its per-level state; set the server time to 1000 ms (frame
  0); put the map's name at configstring 0; set configstring 29 to the `sv_airaccelerate` value
  (printed as a short number) in deathmatch, or "0" otherwise, and set the move's air acceleration
  to match; loaded the map's collision; set configstring 33 (models plus 1) to `maps/<name>.bsp` and
  configstring 31 to the map's checksum (a signed decimal integer); built the area tree; set
  configstrings 34 onward to `*1`, `*2`, ... `*<inline models minus 1>`, the inline models (model
  index n plus 1 is inline model n); closed every area portal. The entities array is the game's:
  the engine does not clear it.
- **After it**: `RunFrame` is called twice to let things settle, still while "loading" (so neither
  configstrings nor new indices are sent); if the game changed configstring 31, that is an error
  that stops the map ("Game DLL corrupted server configstrings"); then the state becomes running,
  the baselines are taken (every in-use entity from 1 to `num_edicts` minus 1 with a model, a sound
  or effects; its number and old origin are set from its index and origin), and the setting
  `mapname` is set.

## 16. Save and load

**[read]**, briefly:

- **`WriteGame(file name, autosave)`**, **`ReadGame(file name)`**: the persistent state of the game
  and the clients. `WriteGame` is called by a save, and on a level change on a listen server (as
  the autosave); a dedicated server does not call it on a level change. `ReadGame` on a load.
- **`WriteLevel(file name)`**, **`ReadLevel(file name)`**: one level's entities. `ReadLevel` is
  called after `SpawnEntities` when the level is entered from a save or revisited.
- **`WriteLevel` is also called on every `gamemap` level change, in deathmatch too**: the engine
  saves the level being left (its configstrings and portal states to a file of its own, then the
  game's `WriteLevel`), with the client slots' `inuse` flags cleared during the call and restored
  after. In deathmatch the level is never read back (the engine skips the revisit check), so the
  file is only written **[read]** **[derived]**. A host may skip both in deathmatch **[derived]**.
- **File names**: the port changes its working folder to `<game folder>/save/current` for the call
  and passes a bare name (`game.ssv`, `<map>.sav`); the original release is known to pass a full
  path; the game opens the name with the C library either way **[read]** **[derived]**.
- Saving is refused in deathmatch ("Can't savegame in a deathmatch") **[read]**.

## 17. Area portals and area numbers

**[read]** throughout unless marked:

- The map's area lump lists areas (area 0 is unused) and each area's portals to other areas; each
  portal has a number. A portal is open or closed; at each map load every portal is closed and the
  areas are grouped into connected sets by flooding through open portals.
- **`SetAreaPortalState(n, open)`** sets portal n and regroups. A number above the map's portal
  count is an error that stops the map. The stock game's area portal entities close their portal
  when spawned and doors open it while open **[read]**.
- **`AreasConnected(a, b)`** is yes when both areas are in the same group. An area number above the
  map's area count is an error that stops the map. Area 0 is never grouped with a real area
  **[derived]**. Entities link to the areas of the leaves they touch (question 2).
- **What it is used for**: hiding entities behind closed doors in each client's frame (question
  13), the area bits each frame carries (the client's renderer culls with them), `inPVS`, `inPHS`
  and multicast.
- **With portals not tracked**: the port's own debugging setting `map_noareas` 1 makes every
  `AreasConnected` answer yes and every area bit set **[read]**. Treating every portal as open is
  that same behaviour: the game still calls `SetAreaPortalState` (harmless to ignore), entities
  behind closed doors are sent anyway (more bandwidth, no wrong picture: the visible sets still
  apply), and sounds and multicasts pass closed doors **[derived]**. Nothing in the stock game's
  rules depends on the answer except its monsters' sight and hearing **[derived]**, so a deathmatch
  plays the same.

## 18. Everything else a deathmatch needs

- **`maxclients`** **[read]**: read by the engine when a game starts (latched, so it changes only at
  a fresh `map`). In deathmatch a value of 1 or less becomes 8 and above 256 becomes 256; in coop
  outside 2 to 4 it becomes 4; single player forces 1. The engine allocates that many slots, and
  the game reads the same setting in `Init` to size its arrays; the two must agree for the
  library's lifetime **[derived]**. The engine registers it with default 1.
- **`deathmatch`** **[read]**: read when a game starts (the mode) and at each map spawn (the air
  acceleration configstring, the save rules). A dedicated server with none of `deathmatch`, `coop`
  or `singleplayer` set forces `deathmatch` 1. When both `deathmatch` and `coop` are set, `coop` is
  cleared with a message.
- **`game`** **[read]**: the mod folder name, latched and server-info; a change applies at the next
  fresh game, rebuilding the search path before the library is looked for. An empty value (or
  `baseq2`) means the base folder. A value with a path separator or `..` is refused. The engine also
  publishes `gamedir` (no-set, server-info). The stock game reads `game` itself (the ban list's
  file folder).
- **Settings the engine registers before the library loads** (the game's `cvar` call then finds
  them with the server's values) **[read]**: `dedicated` (1 on the dedicated build, no-set), `cheats`
  (0, server-info, latched: **the game's `sv_cheats` is this setting**; with it 0 the game refuses
  its cheat commands in deathmatch), `deathmatch`, `coop`, `singleplayer` (0, server-info, latched),
  `skill` (1), `dmflags` (16, instant items, server-info), `fraglimit` and `timelimit` (0,
  server-info), `maxclients`, `hostname` ("noname"), `protocol` (34, no-set), `timeout` (125),
  `zombietime` (2), `sv_airaccelerate` (0, latched), `sv_enforcetime` (0), `paused` (0); and
  `mapname` after each spawn. Every other setting the game reads it creates itself with its own
  default through `cvar` **[read]** (the stock game's list includes `maxentities` 1024 latched,
  `maxspectators`, `password`, `spectator_password`, `needpass`, `filterban`, `sv_gravity` 800,
  `sv_maxvelocity`, `sv_rollspeed`, `sv_rollangle`, the bob and run settings, the flood settings and
  `sv_maplist`) **[read]**; it also reads the client-side `cl_gun`, which on a server simply holds its
  default **[read]**.
- **What commonly breaks a library** **[read]** **[derived]**: a null from `cvar` (the stock game
  never checks, so any refused name or value crashes it, and a host should never return null for the
  game's own registrations); an unfilled table entry (the port's own dedicated build leaves
  `DebugGraph` as stack garbage); indexing entities by any size other than `edict_size`; a trace
  result with a null `ent` or `surface` when nothing is hit (the stock game dereferences `ent` after
  some traces); returning into the library from `error`; reading the player state from a client
  entity whose `client` pointer is null (the engine's status reply reads `client` of every connected
  slot's entity without checking, the frame builder checks); the client slots' entity numbers not
  set before the first frame (the engine sets them, question 5); a configstring 31 changed by the
  game (the engine refuses the map); the shared write buffer interleaved with engine messages
  (question 10); the setting records moving or being freed while the library is loaded.
- **Not settled by the source**: whether an unmodified original-release library behaves differently
  under any of the port's changes listed above (the size-0 allocation, the print levels, the
  sound's clamping); measuring that needs the original release's library on a 32-bit host and is
  left open.
