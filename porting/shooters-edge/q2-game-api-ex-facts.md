# Answers: Quake II server (Q2PRO upstream, commit 601a8df): the extended game interface, feature bits, protocol extensions

Reader's notes. These are a clean-room reader's facts about the Q2PRO server at upstream commit
601a8df, written in the reader's own words from the server sources (`src/server`, the headers
`inc/shared/game.h`, `inc/shared/gameext.h`, `inc/shared/files.h`, `inc/shared/shared.h`, and
`src/common` for shared pieces such as the filesystem, the zone allocator and cvars). They contain
no code. The only identifiers quoted are function, field, setting and command names and numeric
constants, and the only exact strings are texts a player, an rcon user or the console sees.
**[read]** = read directly in the source; **[derived]** = inferred from what was read.
"Console" means the server's own output (terminal and log file). A "developer line" is printed
only when the `developer` setting is 1 or more, and that setting exists only in debug builds.
So in a normal release build these lines are never printed **[read]**. A "warning line" is
printed in yellow on a colour console, with no text prefix added **[read]**. A "drop error"
ends the current map/server with the given message (the game is unloaded) **[read]**.

Protocol numbers: 34 = stock, 35 = the R1Q2 extension, 36 = Q2PRO's own **[read]**.
Stock game API versions the server accepts: 3 (the "old" game, old `gclient_t` and `pmove_t`
shapes) and 3302 (the "new" game, new shapes) **[read]**.

---

## 1. Finding and calling the extended entry point

1. The extended entry point is an exported symbol of the game library named `GetGameAPIEx`
   **[read]**.
2. It is looked up in `SV_InitGameProgs`, which runs when a new game starts (the first map, or
   a change of game directory / `maxclients` that forces a fresh game), not on every map change
   **[read]** (it is reached from the server's game init, which also chooses the default
   configstring layout before it; see § 7) **[derived]**.
3. The order inside `SV_InitGameProgs` **[read]**:
   1. Any loaded game is shut down first (`ge->Shutdown`, library freed, the extended export
      pointer cleared, `g_features` set back to `0`, then a leak test of game-tagged memory).
   2. The library is loaded and the stock entry `GetGameAPI` is called with the stock import
      table. A null result is a drop error: "Game library returned NULL exports".
   3. Developer line "Game API version: N".
   4. The stock version must be 3 or 3302, otherwise drop error
      "Game library is version N, expected 3 or 3302".
   5. Only then is `GetGameAPIEx` looked up in the same library. If the symbol is absent,
      nothing happens: the server runs with no extended exports (all extended behaviour falls
      back, see § 3) and prints nothing **[read]**.
   6. If present it is called once, with one argument: a pointer to the server's extended
      import table (read-only to the game). The table's `apiversion` field is 3 and its
      `structsize` field is the byte size of the server's table **[read]**.
   7. The game returns a pointer to its extended export table (or null). The server keeps that
      pointer (it does not copy the table) **[read]**. If the pointer is non-null and its
      `apiversion` is **1 or more**, it is accepted and a developer line
      "Game supports Q2PRO extended API version N." is printed; otherwise (null, or version 0)
      the server forgets it and behaves as if `GetGameAPIEx` were absent, silently **[read]**.
      There is no upper bound check: a game reporting a version above 3 is accepted **[read]**.
      The server never reads the game's `structsize` field **[read]**.
   8. Then the stock `Init` is called (`ge->Init`) **[read]**.
   9. After `Init`, `g_features` is checked for the protocol-extensions bit (§ 5, § 7), and
      `maxclients`, `edict_size` and `max_edicts` are sanity-checked **[read]**.
4. The header's contract, stated in the server's own header comments **[read]**: `GetGameAPIEx`
   is always called after `GetGameAPI` and before `Init`; the import table pointer stays valid
   as long as the library is loaded, so the game may use it directly without copying (if it
   copies, it must copy no more than `structsize` bytes); new fields are only ever appended at
   the end of either table and only together with a bump of the extended version; the game
   need not implement every export (an unimplemented export must be null), while the server
   must implement every import for the version it advertises; only the upstream engine defines
   new extended versions, and a mod's private additions go through `GetExtension`.
5. Version constants **[read]**: minimum accepted = 1; version 2 = "customize entity" added;
   version 3 = "entity visible" added (and the version-2 customize callback renamed, with the
   meaning of its return value changed); current = 3. Version history in the header:
   1 initial; 2 added `CustomizeEntity`; 3 added `EntityVisibleToClient`, renamed
   `CustomizeEntity` to `CustomizeEntityToClient` and changed its return value meaning.
6. The bundled sample game in the tree does not export `GetGameAPIEx` at all **[read]**.

## 2. Extended imports (server → game)

The import table, in order: `apiversion`, `structsize`, `local_sound`, `get_configstring`,
`clip`, `inVIS`, `GetExtension`, `TagRealloc` **[read]**. All six function slots are present
in version 1 as shipped; versions 2 and 3 added exports only, no imports **[read]** (the header
lists no import additions for 2 or 3) **[derived]**.

### 2.1 `local_sound`

Arguments in words: target entity (the client who hears it), an origin (may be null), a source
entity, channel, sound index, volume, attenuation, time offset **[read]**.

What it does exactly **[read]**:
- It sends a sound to ONE client only (the target), never multicast.
- It builds a sound message with only the "entity/channel present" flag set; it never writes
  volume, attenuation, offset or position. The origin, the source entity, the volume, the
  attenuation and the time offset arguments are all **ignored** (the source marks this as a
  to-do).
- The entity/channel word is built from the **target's** entity number shifted left by 3, OR
  the channel's low 3 bits; so the client hears the sound attached to its own entity.
- The sound index is written as a 16-bit value (with the 16-bit-index flag set) only when
  protocol extensions are active AND the index is above 255; otherwise it is written as one
  byte (an index above 255 without extensions is therefore truncated on the wire) **[read]**
  / **[derived]**. There is no range check on the index **[read]**.
- Reliability: the message is sent reliably when the channel has the reliable channel bit
  (`CHAN_RELIABLE`) set, else unreliably **[read]**.
- Delivery goes through the stock `unicast` path, with all its checks: a null target just
  clears the buffer; a target that is not a client slot gives developer warning
  "PF_Unicast to a non-client N"; a free/zombie client gives "PF_Unicast to a free/zombie
  client N"; an overflowed write buffer is a drop error "PF_Unicast: message buffer
  overflowed" **[read]**. The unicast path also archives it into an MVD recording **[read]**.
- The message is appended to the shared "multicast" write buffer, so anything the game had
  already written into it and not sent would go out in front of it **[derived]**.

### 2.2 `get_configstring`

One argument, the configstring index. Returns the server's current string for that index (an
empty string for an unset slot) **[read]**. An index below 0 or at/above the active layout's
end (2080 without extensions, 13630 with; § 7) is a drop error
"PF_GetConfigstring: bad index: N" **[read]**. The returned pointer is the server's own storage
**[read]**; it changes when the configstring is set again **[derived]**.

### 2.3 `clip`

Arguments: start, mins, maxs, end, the one entity to clip against, content mask **[read]**.
- Like the stock `trace`, but tests only against the given entity, never the world plus other
  entities **[read]**.
- Null mins or maxs are treated as the zero vector **[read]**.
- If the entity is the world (entity 0), it traces the box against the world's BSP tree with
  the given content mask **[read]**.
- Otherwise it traces against that entity's hull placed at the entity's origin and angles
  (rotated for brush models): a `SOLID_BSP` entity **or a `SOLID_TRIGGER` entity** uses its
  inline BSP model's tree when its model index names one (this is the stated point of the
  call: clipping to a trigger by its brush shape); otherwise a temporary box hull from the
  entity's mins/maxs is used **[read]**. A `SOLID_BSP` entity whose inline model index is out
  of range is a drop error "SV_HullForEntity: inline model N out of range" **[read]**. A
  `SOLID_NOT` entity is still clipped (as a box) — there is no solidity skip **[read]**.
- The content mask is passed straight through to the BSP trace; no content-based entity
  filtering (dead monster, player, projectile flags) is applied, because those filters live in
  the multi-entity trace only **[read]**.
- The result is a normal trace result (fraction, end position, plane, surface, contents,
  allsolid, startsolid) with its entity field ALWAYS set to the entity passed in, whether or
  not anything was hit **[read]**. With protocol extensions active the trace uses the
  extended BSP content rules **[read]**.

### 2.4 `inVIS`

Arguments: two points and a visibility kind **[read]**. Kinds: `VIS_PVS` = 0, `VIS_PHS` = 1,
`VIS_NOAREAS` = 2, and `VIS_NOAREAS` may be OR-ed with either of the first two **[read]**.
- It finds the leaf of point 1 and decompresses that leaf's cluster row of the PVS (kind bit 1
  clear) or PHS (kind bit 1 set) **[read]**.
- It finds the leaf of point 2. If point 2's cluster is -1 (outside the map / solid) → false
  **[read]**. If point 2's cluster bit is not set in the row → false **[read]**.
- If `VIS_NOAREAS` is set → true without looking at areas **[read]**.
- Otherwise the two leaves' areas must be connected through the current area-portal state
  (open doors); if not → false ("a door blocks it"), else true **[read]**.
- If point 1's cluster is -1, the row is all zeros, so the answer is false **[read]**. If the
  map has no visibility data, the row is all ones, so only the area test (or nothing, with
  `VIS_NOAREAS`) decides **[read]**.
- The stock `inPVS` and `inPHS` imports are exactly this call with kind 0 and kind 1
  respectively **[read]**.

### 2.5 `GetExtension` (server side)

One argument, a name string. Returns a pointer to an extension's function table, or null
**[read]**:
- null name → null.
- `"FILESYSTEM_API_V1"` → the filesystem table (§ 4). Always available **[read]**.
- `"DEBUG_DRAW_API_V1"` → the debug-draw table, but only in builds that include a renderer and
  debug support, and only when the process is not running as a dedicated server
  (`dedicated` is 0); otherwise null **[read]**. Its calls (by name only): `ClearDebugLines`,
  `AddDebugLine`, `AddDebugPoint`, `AddDebugAxis`, `AddDebugBounds`, `AddDebugSphere`,
  `AddDebugCircle`, `AddDebugCylinder`, `AddDebugArrow`, `AddDebugCurveArrow`,
  `AddDebugText`; each takes positions/sizes, a 32-bit colour (arrows take a line colour and
  an arrow colour; the axis takes none), a lifetime value, and a depth-test switch **[read]**.
- any other name → null **[read]**. Names are compared exactly (case-sensitive) **[read]**.

### 2.6 `TagRealloc`

Arguments: an existing block pointer and a new size **[read]**.
- Null pointer with a non-zero size → drop error
  "PF_TagRealloc: untagged allocation not allowed" (a new block must come from the stock
  `TagMalloc`, which carries the tag) **[read]**.
- Null pointer with size 0 → returns null (the allocator returns null for a zero-size request)
  **[read]** / **[derived]**.
- Non-null pointer with size 0 → the block is freed and null is returned **[read]**.
- Non-null pointer with the same size → the same pointer back **[read]**.
- Otherwise the block is resized in place or moved; it keeps its original tag; the contents
  up to the smaller of the two sizes are kept; any grown part is NOT zeroed (unlike the stock
  `TagMalloc`, which zeroes) **[read]**. Failure to get memory is a fatal error
  "Z_Realloc: couldn't realloc N bytes" **[read]**. A corrupt or already-freed block, or a
  block from the engine's static pool, is a fatal assertion **[read]**.
- Game tags, for reference: the stock `TagMalloc`, `FreeTags` and the filesystem `LoadFile`
  shift the game's tag into a namespace above the engine's own tags; a game tag larger than
  65535 minus the number of engine tags is a drop error "<function>: bad tag", e.g.
  "PF_TagMalloc: bad tag", "PF_FreeTags: bad tag", "PF_LoadFile: bad tag" **[read]**.

## 3. Extended exports (game → server)

The export table, in order: `apiversion`, `structsize`, `GetExtension`, `CanSave`, `PrepFrame`,
`RestartFilesystem`, `CustomizeEntityToClient`, `EntityVisibleToClient` **[read]**.

1. **`GetExtension`** (game side): declared for a mod's private extensions; this server never
   calls it **[read]**.
2. **`CanSave`** (version 1+; used whenever non-null) **[read]**: called by the `save` and
   `autosave` console commands, after these server checks have passed, in order: the server is
   in a running game (else "You must be in a game to save."); the game supports enhanced
   savegames (else "Game does not support enhanced savegames."); `deathmatch` is 0 (else
   "Can't savegame in a deathmatch."). If `CanSave` returns false, the save is abandoned
   silently by the server (the game is expected to print its own reason) **[read]** /
   **[derived]**. Fallback when absent: on a single-player server (`maxclients` 1), if the
   first client's health stat is 0 or less, it prints "Can't savegame while dead!" and stops
   **[read]**. When `CanSave` exists, that dead-player check is NOT done by the server
   **[read]**.
3. **`PrepFrame`** (version 1+; used whenever non-null) **[read]**: called once per server
   game frame, at the end of the frame, after the game's frame has run, after client messages
   have been sent and after the master heartbeat, before the frame counter advances. When
   present it REPLACES the server's default end-of-frame work; the default (when absent) is:
   on frames that are whole base-rate frames, clear every entity's `event` field (events last
   one frame) **[read]**. So a game providing `PrepFrame` must clear events itself
   **[derived]**. During MVD playback (broadcast state) neither runs; the MVD layer's own step
   runs instead **[read]**.
4. **`RestartFilesystem`** (version 1+; used whenever non-null) **[read]**: called from the
   filesystem restart, after the search paths have been rebuilt and before the path listing is
   printed. A filesystem restart happens on the `fs_restart` command (full restart) and when
   `game` (the `fs_game` setting) changes while running (game-path-only restart) **[read]**.
   Absent → nothing. The point is for the game to drop cached file handles or lists
   **[derived]**.
5. **`EntityVisibleToClient`** (only used when the game's extended version is **3 or more**)
   **[read]**: two arguments, the viewing client's own entity and a candidate entity. It is
   called while building each client's frame, for each candidate that already passed every
   server cull (in use when the game declares proper in-use, not `SVF_NOCLIENT`, has a model
   or effect or sound or event, the client's no-gibs / no-flares settings, area connection,
   PVS/PHS and distance culls — or `SVF_NOCULL` with extensions), and before the cap on the
   number of entities per packet is applied. It is also called for the client's own entity.
   Returning false drops the entity from this client's frame for this frame only **[read]**.
   It is not consulted for MVD recordings **[read]** (the MVD path never reads the extended
   exports) **[derived]**.
6. **`CustomizeEntityToClient`** (only used when the game's extended version is **3 or
   more**) **[read]**: three arguments: the viewing client's own entity, the entity, and a
   scratch record the server provides that holds an entity state plus (with extensions) the
   extension fields (`morefx`, `alpha`, `scale`, `loop_volume`, `loop_attenuation`). It is
   called for each entity that will actually be sent to this client (after visibility and
   after prioritisation trimmed the list), in entity-number order. If it returns true, the
   server sends the scratch record instead of the entity's real state; the game must have
   filled the whole record, and the record's entity number must equal the entity's number, or
   the server stops with a fatal error "SV_BuildClientFrame: assertion ... failed"
   **[read]**. If it returns false (or is null) the real state is sent **[read]**. The scratch
   record is reused across entities and is not cleared by the server **[read]**. The real
   entity is never modified **[derived]**.
7. **Version 2 games**: a game reporting extended version 2 gets its version-1 exports used
   (`CanSave`, `PrepFrame`, `RestartFilesystem`) but its version-2 customize callback is never
   called; this server only reads the two per-client callbacks when the version is 3 or more
   **[read]**. (The version-2 constant is defined but unused by the server.) **[read]**
8. **Version 1 games**: only `CanSave`, `PrepFrame`, `RestartFilesystem` are used **[read]**.
9. Absent `GetGameAPIEx` / rejected table: every fallback above applies; there is no other
   effect **[read]**.

## 4. The filesystem extension, `FILESYSTEM_API_V1`

The table, in order: `OpenFile`, `CloseFile`, `LoadFile`, `ReadFile`, `WriteFile`, `FlushFile`,
`TellFile`, `SeekFile`, `ReadLine`, `ListFiles`, `FreeFileList`, `ErrorString` **[read]**. Each
is the engine's own filesystem call, used directly, except `LoadFile` (game tag shifted, § 2.6)
**[read]**.

### 4.1 Return codes and `ErrorString`

- Calls return 0 or a positive count/length on success and a negative error code on failure
  **[read]**.
- Error codes are either a negated system error number (when it lies between 1 and 0x5000)
  or one of the engine's own codes, which sit below -0x5000 **[read]**. The engine's own codes
  and their `ErrorString` texts, in order from -0x5000 downward **[read]**: "Unspecified
  error", "Unknown file format", "Invalid file format", "Invalid quake path", "Unexpected end
  of file", "File too small", "Not a regular file", "Decompression overrun", "String
  truncation avoided", "Infinite loop avoided", "Library error", "Out of slots", "Inflate
  failed", "Deflate failed", "Coherency check failed", "Bad compression method".
- `ErrorString(code)`: 0 or any non-negative value → "Success"; a system error → the C
  library's text for it; an engine code → the table above; an engine code beyond the table →
  "Unspecified error" **[read]**.
- System errors used by the calls below: bad handle (EBADF), invalid argument (EINVAL), not
  found (ENOENT), name too long (ENAMETOOLONG), too many open files (EMFILE), not ready
  (EAGAIN: the filesystem is not initialised yet), file too big (EFBIG), not supported
  (ENOSYS), overflow (EOVERFLOW), is a directory (EISDIR) **[read]**.

### 4.2 Mode flags (one unsigned word; values from the header) **[read]**

- Open mode (mask 0x3): read 0x0; write 0x1 (create/truncate); append 0x2 (create if missing,
  position at end); read-write 0x3 (open existing for update, positioned at end; does not
  create).
- Output buffering (mask 0xC): default 0x0; full 0x4; line 0x8; none 0xC.
- Source type (mask 0x30): any 0x0; real files only 0x10; packs only 0x20.
- Path set (mask 0xC0): any 0x0; base search paths only 0x40; game search paths only 0x80.
- Open flags (mask 0xFF00): gzip 0x100 (transparent compress/decompress); exclusive 0x200
  (write: fail if the file exists); text 0x400 (open a disk file in text mode; binary is the
  default); deflate 0x800 (read raw deflate data of a compressed pack member, fail otherwise);
  load-file 0x1000 (internal: a short-lived non-unique handle).
- Directory root (mask 0x30000): any 0x0; base directory 0x10000; home directory 0x20000.
- List flags (mask 0xFF00, for `ListFiles` only): by-filter 0x100 (wildcard patterns instead
  of extensions; implies recursion); save-path 0x200 (keep the searched path in each name);
  extra-info 0x400 (each entry is a record with size, creation time, modification time and
  name, instead of a plain string); strip-extension 0x800; directories-only 0x1000 (cannot be
  combined with any other list flag); recursive 0x2000.
- Defaults: when no path-set bits are given, or no `game` directory is active, both base and
  game sets are searched; when no directory-root bits are given, or no home directory is
  configured, both roots are searched **[read]**.

### 4.3 The calls

- **`OpenFile`(path, out handle, mode)** **[read]**: sets the handle to 0 first. Returns the
  file length for read mode, or the starting position for write modes (0 for write, the
  current end for append and read-write), or a negative error. Not initialised → EAGAIN; no
  free handle (the table holds 1024) → EMFILE. Handles are small positive integers; 0 is never
  valid.
  - Reading: the path is normalised (backslashes to slashes, `.` and `..` resolved without
    climbing above the root, duplicate and leading/trailing slashes removed); hard links
    (`link` command) are expanded first and soft links (`softlink`) are tried if the file is
    not found. The search runs through the search path list from highest priority: within each
    directory, its packs (`.pak`, and `.pkz` zips when built with zlib) are searched before the
    loose directory; the game directory's entries come before the base directory's (`baseq2`);
    the home directory's come before the install directory's. Pack lookup is skipped for names
    of 64 characters or more. Loose files: a name with a non-printable character (or on
    Windows one of `<>:"|?*`) is skipped for disk but still searched in packs; on case-sensitive
    systems a mixed-case name is retried in lower case. Not found → ENOENT, or
    "Invalid quake path" when the name was invalid for disk. An empty name → "Invalid quake
    path".
  - Writing (write, append, read-write): the path is normalised and must be valid (else
    "Invalid quake path"); writes never go into packs. If the path-set bits are exactly "base
    only", the file goes under `<home>/baseq2/` (or `<basedir>/baseq2/` with no home);
    otherwise it goes under the current game directory (`fs_gamedir`: the home or install
    copy of the active `game`, or `baseq2` when none). Missing directories are created. The
    `..` resolution means a write cannot escape that directory **[derived]**. With the gzip
    flag the file is written compressed.
- **`CloseFile`(handle)**: closes it and frees the handle; returns the first sticky error the
  handle had seen, or an error from closing; a bad handle → EBADF **[read]**.
- **`LoadFile`(path, out buffer, flags, game tag)**: reads a whole file into a new block
  tagged with the game's tag (+ a terminating zero byte not counted in the length); returns
  the length or an error. A null buffer pointer only checks existence and returns the length.
  Open-mode bits in the flags → EINVAL. Over 0x8001000 bytes (128 MiB plus slop) → EFBIG. A
  short read → the read error or "Unexpected end of file". Out-of-range tag → drop error
  "PF_LoadFile: bad tag" **[read]**. The block is freed by the stock `TagFree` or `FreeTags`
  **[derived]**.
- **`ReadFile`(buffer, length, handle)**: returns bytes read (0 at end), or an error. Only
  for read-mode handles (else EBADF); length over 2^31-1 → EINVAL; length 0 → 0; after any
  earlier error the handle keeps returning that error **[read]**.
- **`WriteFile`(buffer, length, handle)**: returns the length or an error; read-mode handle →
  EBADF; a short write sets a sticky "Unspecified error" (or "Library error" for gzip)
  **[read]**.
- **`FlushFile`(handle)**: flushes a write handle; a read handle → EBADF **[read]**.
- **`TellFile`(handle)**: current position, or an error **[read]**.
- **`SeekFile`(handle, offset, whence)**: whence is the C library's set/current/end. Disk files
  and gzip seek freely. Pack and zip members: "set" with a negative offset or "current"
  before the start → EINVAL; positions are clamped to the member's length; "end" always goes to
  the end, ignoring the offset. Returns 0 on success **[read]**.
- **`ReadLine`(handle, buffer, size)**: reads one line (including its newline) into the buffer,
  zero-terminated; returns its length, 0 at end of file, or an error. Read-mode handles only;
  size 0 or over 2^31-1 → EINVAL; works only on disk files and gzip files: a pack or zip
  member → ENOSYS **[read]**.
- **`ListFiles`(path, filter, flags, out count)**: returns a null-terminated array of entries
  (strings, or info records with extra-info) and stores the count; returns null with count 0
  when nothing matched, the filesystem is not initialised, the path is too long, or
  directories-only is mixed with other list flags **[read]**.
  - The path is normalised; null means the root. Without save-path, names are relative to the
    searched path; with it they keep the path **[read]**.
  - Filter: without by-filter it is a list of extensions separated by `;` matched at the end
    of the name, case-insensitively (e.g. `.bsp;.ent`); with by-filter it is a list of
    wildcard patterns separated by `;` matched against the relative path **[read]**.
  - Without by-filter, recursive or directories-only, only the immediate directory is listed;
    recursion on disk goes at most 8 levels deep; dot-files are skipped on disk **[read]**.
  - Packs and directories are both searched (subject to the type, path-set and root bits);
    a pack's member size is reported, with zero times **[read]**.
  - Plain-string results are sorted and de-duplicated (the same name from several search
    paths appears once); info-record results are sorted but not de-duplicated **[read]**.
- **`FreeFileList`(list)**: frees every entry and the array; null is allowed **[read]**.

## 5. Feature bits

### 5.1 The two settings

- `sv_features` (server → game): created by the server at startup, read-only (`CVAR_ROM`), its
  value the set of features this server build supports **[read]**. The game reads it (typically
  in `Init`) to decide what to enable **[read]** (the bundled game does so) **[read]**.
- `g_features` (game → server): created by the server at startup with value `0`, read-only
  (`CVAR_ROM`) **[read]**. The game sets it with the forcing setter (`cvar_forceset`, which
  is allowed); the ordinary `cvar_set` import and a console user are refused with
  "g_features is read-only." **[read]**.
- `g_features` is reset to `0` whenever the game library is shut down (before a new game is
  loaded, or the server stops) **[read]**. The MVD playback mode sets it to its own value
  (client number, proper in-use, all disconnects) while playing and to `0` when leaving
  **[read]**.
- When it is read: the protocol-extensions bit is read ONCE, right after `Init` returns;
  variable FPS is read at each map spawn; every other bit is read live at each point of use
  (so a game could change those later) **[read]** / **[derived]**.

### 5.2 The bits (name, value, meaning, what the server does) **[read]**

- `GMF_CLIENTNUM` = bit 0 (1): the game keeps the `clientNum` field of each client's game
  record (the point-of-view entity number minus 1). The server then uses it as the frame's
  client number (what the client sees as "me"); if it is out of range (below -1, or not below
  max entities minus 1) a developer warning "SV_BuildClientFrame: bad clientNum N for client M"
  is printed and the slot number is used instead. Without the bit the slot number is always
  used.
- `GMF_PROPERINUSE` = bit 1 (2): the game keeps every entity's `inuse` field correct. The
  server then skips entities not in use when building frames, baselines and MVD state.
  Without the bit, `inuse` is ignored for those decisions.
- `GMF_MVDSPEC` = bit 2 (4): the game understands the dummy MVD spectator client. With
  `sv_mvd_spawn_dummy` 1 the dummy is created only when this bit is set (developer line
  "Dummy MVD client not supported by game" otherwise); with `sv_mvd_spawn_dummy` 2 or more it
  is created regardless.
- `GMF_WANT_ALL_DISCONNECTS` = bit 3 (8): the game wants `ClientDisconnect` for every
  dropped client, including ones that connected but never entered the game. Without it,
  `ClientDisconnect` is only called for clients that had spawned.
- `GMF_ENHANCED_SAVEGAMES` = bit 10 (1024): the game's savegames are safe/portable. Without
  it (and without the other allowances) save/load refuse ("Game does not support enhanced
  savegames."). On a non-deathmatch game start the console prints "Game supports Q2PRO
  enhanced savegames." when set, otherwise warning "Game does not support enhanced
  savegames. Savegames will not work." (or other lines for a detected other port's game or a
  command-line override: "Game supports YQ2 enhanced savegames.", "Use of unsafe savegames
  forced from command line.").
- `GMF_VARIABLE_FPS` = bit 11 (2048): the game runs at a variable server frame rate. Only in
  builds with variable FPS (a build option, off by default). When set, each map spawn takes
  the frame time from `sv_fps`; otherwise the base 10 Hz; `sv_fps` is then rewritten to the
  rate actually used.
- `GMF_EXTRA_USERINFO` = bit 12 (4096): see § 5.3.
- `GMF_IPV6_ADDRESS_AWARE` = bit 13 (8192): the game copes with IPv6 addresses in `ip`.
  Without it, an IPv6 client's address is shown to the game as a fake IPv4 address
  "198.51.100.X:PORT", where X is the XOR of the first 6 bytes of the IPv6 address (its /48
  prefix) and PORT the client's port number.
- `GMF_ALLOW_INDEX_OVERFLOW` = bit 14 (16384): when the model, sound or image index lists are
  full, the index imports return 0 with a developer line "PF_ModelIndex(name): overflow"
  (likewise `PF_SoundIndex`, `PF_ImageIndex`) instead of the default drop error with the same
  text.
- `GMF_PROTOCOL_EXTENSIONS` = bit 15 (32768): the game uses the extended limits and the
  extended entity/player state (§ 7). Console line "Game supports Q2PRO protocol
  extensions." and the extended configstring layout is chosen.
- Bits 4–9 are not assigned **[read]**. Bits 0–3 are labelled as shared with the R1Q2 server;
  bits 10–15 as this server's own **[read]**.
- `sv_features`' value = the OR of all of the above, except bit 11 in builds without variable
  FPS: 62479 by default, 64527 with variable FPS **[read]** (sum) **[derived]**.

### 5.3 Extra userinfo (`GMF_EXTRA_USERINFO`)

The userinfo given to `ClientConnect` is a buffer of two zero-terminated info strings back to
back: the client's normal userinfo, then the extra one **[read]**.
- **With the bit** **[read]**: the extra string is, in this order, `challenge`, `ip`, `major`,
  `minor`, `netchan`, `packetlen`, `qport`, `zlib`, as `\key\value` pairs:
  - `challenge`: the connection's challenge number, decimal.
  - `ip`: the client's address as "a.b.c.d:port" (or the IPv6 form / the fake IPv4 of § 5.2).
  - `major`: the protocol number (34, 35 or 36).
  - `minor`: the protocol's minor version (0 for 34).
  - `netchan`: 0 = the old net channel, 1 = the new one.
  - `packetlen`: the agreed maximum packet length.
  - `qport`: the client's qport.
  - `zlib`: 1 if the client accepts compressed messages, else 0.
  The normal userinfo is passed as the client sent it: no `ip` key is added, and `password`
  and `mvdspec` are left in.
- **Without the bit** **[read]**: the extra string is empty; any `mvdspec` key is removed;
  the `password` key is removed when the server has `sv_password` or
  `sv_reserved_password` set; and `ip` is set in the normal userinfo (refusal "Oversize
  userinfo string." if it does not fit).
- The dummy MVD client's userinfo **[read]**: normal part `\name\[MVDSPEC]\skin\male/grunt\spectator\1`;
  extra part `\mvdspec\2013\ip\loopback`. Without the bit both are joined into the normal
  part.
- If `ClientConnect` refuses, the server reads `rejmsg` from the (possibly modified)
  userinfo and answers the client "<rejmsg>\nConnection refused.\n", or just
  "Connection refused.\n" when empty **[read]**.

## 6. Log lines of the extended interface

- Developer line, after `GetGameAPIEx` accepted: "Game supports Q2PRO extended API version
  N." **[read]**.
- Console line (always), after `Init`, when the game set bit 15:
  "Game supports Q2PRO protocol extensions." **[read]**.
- Warning line from the stock `cvar` import whenever the game passes flag bits above the
  five it may use (archive 1, userinfo 2, serverinfo 4, no-set 8, latch 16; i.e. any bit from
  32 up): "Game attempted to set extended flags on 'NAME', masked out." The offending bits are
  cleared and the setting is created anyway; the server always adds its own "created by the
  game" mark **[read]**.
- Developer line "Game API version: N" (stock API), and drop errors "Game library returned
  NULL exports", "Game library is version N, expected 3 or 3302", "Failed to load game
  library", "Game library corrupted maxclients value", "Game library returned bad size of
  edict_t", "Game library returned bad number of max_edicts" **[read]**.
- Drop errors from the imports: "PF_GetConfigstring: bad index: N",
  "PF_TagRealloc: untagged allocation not allowed", "PF_LoadFile: bad tag" **[read]**.
- `local_sound` prints nothing of its own; its unicast path's developer warnings apply (§ 2.1)
  **[read]**.
- Nothing is printed when `GetGameAPIEx` is absent or its table is refused **[read]**.

## 7. With protocol extensions on

Chosen once per game load, after `Init`, when `g_features` has bit 15; the server starts every
game with the stock layout first **[read]**. The server must be built with protocol extensions
(the default build has them) **[read]**.

| Limit | Off | On |
| --- | --- | --- |
| Entities (`max_edicts`) | 1024 | 8192 (13-bit entity numbers) |
| Models | 256 | 8192 (half meant for inline BSP models) |
| Sounds | 256 | 2048 |
| Images | 256 | 2048 |
| Light styles / items / player skins / general | 256 / 256 / 256 / 512 | same |
| Entities per packet (default) | 128 | 512 |

Configstring index layout **[read]** (values computed from the header) **[derived]**:

| Slot | Off | On |
| --- | --- | --- |
| name, cdtrack, sky, skyaxis, skyrotate, statusbar | 0–5 | 0–5 (unchanged) |
| air acceleration | 29 | 59 |
| maxclients | 30 | 60 |
| map checksum | 31 | 61 |
| models | 32 | 62 |
| sounds | 288 | 8254 |
| images | 544 | 10302 |
| lights | 800 | 12350 |
| items | 1056 | 12606 |
| player skins | 1312 | 12862 |
| general | 1568 | 13118 |
| end (total count) | 2080 | 13630 |

Other effects when on **[read]**:
- The status bar string may run on through the slots up to air acceleration (24 slots × 64
  bytes off, 54 slots on), and the general strings may run to the end; any other slot holds at
  most 63 characters (longer is truncated with a developer warning; running past the end of
  the whole table is a drop error).
- The game's entity record must be the full extended size (with the extension fields);
  without extensions the server accepts the record without them.
- The serverinfo `protocol` key reads "36" instead of "34".
- Only Q2PRO-protocol clients of minor version 1024 or later (1025 or later for a game on the
  new 3302 API) may connect; others are refused with "This is a protocol limit removing
  enhanced server.\nYour client version is not compatible. Make sure you are running the
  latest Q2PRO client version.\n".
- Sound indices above 255 go on the wire as 16-bit; positions written by the game
  (`WritePosition`) use the wider extended coordinate encoding when the game is on the 3302
  API; entity sizes are packed in the wider form; entity state carries the extension fields;
  traces and point contents use the extended content rules (player and projectile entities,
  via `SVF_PLAYER` and `SVF_PROJECTILE`, are skipped unless the mask asks for them);
  `SVF_NOCULL` entities skip visibility culling.
- Extra `svflags` only meaningful with extensions **[read]**: `SVF_PLAYER` 8, `SVF_BOT` 16,
  `SVF_NOBOTS` 32, `SVF_RESPAWNING` 64, `SVF_PROJECTILE` 128, `SVF_INSTANCED` 256, `SVF_DOOR`
  512, `SVF_NOCULL` 1024, `SVF_HULL` 2048 (stock: `SVF_NOCLIENT` 1, `SVF_DEADMONSTER` 2,
  `SVF_MONSTER` 4).
- Model index 255 (`MODELINDEX_PLAYER`) stays reserved in both layouts **[read]**.

## 8. Binary layout

Conventions **[read]**: a "vector" is a pointer to three 32-bit floats (x, y, z) passed as a
pointer to its first element; a "qboolean" is the stock boolean, a C enumeration with false =
0 and true = 1, so a 32-bit int on every supported ABI **[derived]**; a "file handle" is a
32-bit signed int; an "entity pointer" points at the game's entity record. All tables are
plain C structures of the C calling convention of the platform. Offsets below assume a 64-bit
target (LP64 or LLP64: 8-byte pointers, 4-byte ints and floats); the 32-bit sizes are given
alongside **[derived]**, computed from the field order.

### 8.1 The entry point

- Exported symbol name: exactly `GetGameAPIEx` (unmangled C name) **[read]**. The server looks
  it up by that name in the already-loaded game library **[read]**.
- One parameter: a pointer to the server's extended import table (read-only, § 8.2) **[read]**.
- Returns: a pointer to the game's extended export table (read-only from the server's side,
  § 8.3), or null **[read]**.
- It takes no version argument: the version the server offers is the import table's first
  field (3), and the version the game speaks is its export table's first field **[read]**.

### 8.2 The extended import table (server → game) — 56 bytes on 64-bit, 32 on 32-bit

| # | Offset (64-bit) | Field | Kind | Since |
| --- | --- | --- | --- | --- |
| 1 | 0 | `apiversion` | a 32-bit unsigned int: the API version the server implements (3) | 1 |
| 2 | 4 | `structsize` | a 32-bit unsigned int: the byte size of the server's table (56 on 64-bit) | 1 |
| 3 | 8 | `local_sound` | a pointer to a function taking (an entity pointer: the target client, a vector: origin (may be null), an entity pointer: the source, an int channel, an int sound index, a float volume, a float attenuation, a float time offset) returning nothing | 1 |
| 4 | 16 | `get_configstring` | a pointer to a function taking (an int index) returning a pointer to a constant zero-terminated string | 1 |
| 5 | 24 | `clip` | a pointer to a function taking (a vector start, a vector mins (may be null), a vector maxs (may be null), a vector end, an entity pointer: the entity to clip against, an int content mask) returning a trace result BY VALUE (§ 8.6) | 1 |
| 6 | 32 | `inVIS` | a pointer to a function taking (a vector p1, a vector p2, an int visibility kind: 0 PVS, 1 PHS, OR 2 for no areas) returning a qboolean | 1 |
| 7 | 40 | `GetExtension` | a pointer to a function taking (a pointer to a constant zero-terminated name) returning an untyped pointer (the extension's table, or null) | 1 |
| 8 | 48 | `TagRealloc` | a pointer to a function taking (an untyped pointer: the block, a size_t new size: 64-bit on 64-bit targets) returning an untyped pointer | 1 |

The header's version history lists no import added after version 1, so every slot is
version 1 **[read]** / **[derived]**. The visibility kind is declared as a C enumeration
(int-sized) **[read]** / **[derived]**.

### 8.3 The extended export table (game → server) — 56 bytes on 64-bit, 32 on 32-bit, for version 3

| # | Offset (64-bit) | Field | Kind | Since |
| --- | --- | --- | --- | --- |
| 1 | 0 | `apiversion` | a 32-bit unsigned int: the extended API version the game speaks (1–3) | 1 |
| 2 | 4 | `structsize` | a 32-bit unsigned int: the byte size of the game's table (not read by this server) | 1 |
| 3 | 8 | `GetExtension` | a pointer to a function taking (a pointer to a constant name string) returning an untyped pointer; may be null; never called by this server | 1 |
| 4 | 16 | `CanSave` | a pointer to a function taking nothing returning a qboolean; may be null | 1 |
| 5 | 24 | `PrepFrame` | a pointer to a function taking nothing returning nothing; may be null | 1 |
| 6 | 32 | `RestartFilesystem` | a pointer to a function taking nothing returning nothing; may be null | 1 |
| 7 | 40 | `CustomizeEntityToClient` | a pointer to a function taking (an entity pointer: the viewing client's entity, an entity pointer: the entity, a pointer to a customize record (§ 8.6) to fill) returning a qboolean (true = the record was filled, send it) | 2 (as `CustomizeEntity`, other return meaning); renamed with today's meaning in 3 |
| 8 | 48 | `EntityVisibleToClient` | a pointer to a function taking (an entity pointer: the viewing client's entity, an entity pointer: the entity) returning a qboolean (false = hide) | 3 |

A version-1 game's table may end after field 6 (40 bytes on 64-bit, 24 on 32-bit) and a
version-2 game's after field 7 (48 / 28): the server reads fields 7 and 8 only when
`apiversion` is 3 or more **[read]** / **[derived]**.

### 8.4 The filesystem table, `FILESYSTEM_API_V1` — 96 bytes on 64-bit, 48 on 32-bit

Every field is a function pointer; offsets 0, 8, 16 … 88 on 64-bit **[derived]**.

| # | Offset | Field | Takes | Returns |
| --- | --- | --- | --- | --- |
| 1 | 0 | `OpenFile` | a pointer to a constant path string, a pointer to a file handle (out), a 32-bit unsigned mode word | a signed 64-bit length/position or negative error |
| 2 | 8 | `CloseFile` | a file handle | an int: 0 or negative error |
| 3 | 16 | `LoadFile` | a pointer to a constant path string, a pointer to an untyped pointer (out buffer; may be null), a 32-bit unsigned flags word, a 32-bit unsigned game tag | an int: length or negative error |
| 4 | 24 | `ReadFile` | an untyped pointer to the buffer, a size_t length, a file handle | an int: bytes read or negative error |
| 5 | 32 | `WriteFile` | a pointer to a constant buffer, a size_t length, a file handle | an int: bytes written or negative error |
| 6 | 40 | `FlushFile` | a file handle | an int: 0 or negative error |
| 7 | 48 | `TellFile` | a file handle | a signed 64-bit position or negative error |
| 8 | 56 | `SeekFile` | a file handle, a signed 64-bit offset, an int whence (the C library's set 0 / current 1 / end 2) | an int: 0 or negative error |
| 9 | 64 | `ReadLine` | a file handle, a pointer to a char buffer, a size_t buffer size | an int: line length, 0 at end, or negative error |
| 10 | 72 | `ListFiles` | a pointer to a constant path string (may be null), a pointer to a constant filter string (may be null), a 32-bit unsigned flags word, a pointer to an int count (out; may be null) | a pointer to a null-terminated array of untyped pointers (strings, or info records with extra-info), or null |
| 11 | 80 | `FreeFileList` | a pointer to such an array (may be null) | nothing |
| 12 | 88 | `ErrorString` | an int error code | a pointer to a constant string |

All fields are version 1 of this extension (the only version) **[read]**. The info record
returned with the extra-info list flag is: a signed 64-bit size, a signed 64-bit creation time,
a signed 64-bit modification time (seconds since the epoch), then the zero-terminated name
inline (offsets 0, 8, 16, 24) **[read]** / **[derived]**. The whence values are the host C
library's (0, 1, 2 on all common platforms) **[derived]**.

### 8.5 The debug-draw table, `DEBUG_DRAW_API_V1` — 88 bytes on 64-bit, 44 on 32-bit

Every field is a function pointer returning nothing; offsets 0, 8 … 80 **[derived]**. "Colour"
is a 32-bit unsigned int; "time" is a 32-bit unsigned int lifetime; "depth test" is a qboolean
**[read]**.

| # | Offset | Field | Takes |
| --- | --- | --- | --- |
| 1 | 0 | `ClearDebugLines` | nothing |
| 2 | 8 | `AddDebugLine` | a vector start, a vector end, a colour, a time, a depth test |
| 3 | 16 | `AddDebugPoint` | a vector point, a float size, a colour, a time, a depth test |
| 4 | 24 | `AddDebugAxis` | a vector origin, a vector angles, a float size, a time, a depth test |
| 5 | 32 | `AddDebugBounds` | a vector mins, a vector maxs, a colour, a time, a depth test |
| 6 | 40 | `AddDebugSphere` | a vector origin, a float radius, a colour, a time, a depth test |
| 7 | 48 | `AddDebugCircle` | a vector origin, a float radius, a colour, a time, a depth test |
| 8 | 56 | `AddDebugCylinder` | a vector origin, a float half height, a float radius, a colour, a time, a depth test |
| 9 | 64 | `AddDebugArrow` | a vector start, a vector end, a float size, a line colour, an arrow colour, a time, a depth test |
| 10 | 72 | `AddDebugCurveArrow` | a vector start, a vector control point, a vector end, a float size, a line colour, an arrow colour, a time, a depth test |
| 11 | 80 | `AddDebugText` | a vector origin, a vector angles, a pointer to a constant text string, a float size, a colour, a time, a depth test |

All version 1 of this extension **[read]**.

### 8.6 Records passed through the tables

- **Trace result** (returned by value from `clip`, same record as the stock `trace`)
  **[read]**, 72 bytes on 64-bit **[derived]**: allsolid (qboolean, 0), startsolid (qboolean,
  4), fraction (float, 8), end position (three floats, 12), the hit plane (20 bytes at 24:
  normal as three floats, distance float, a type byte, a sign-bits byte, two pad bytes), a
  pointer to the hit surface record (48; that record is a 16-character name, an int flags, an
  int value), contents (int, 56), 4 pad bytes, the entity pointer (64). Because it is larger
  than two registers it is returned through a hidden result pointer on every common 64-bit ABI
  **[derived]**. On 32-bit x86 GCC builds the server may be built with a "game ABI hack" option
  that makes such by-value returns follow the convention where the caller, not the callee,
  pops the hidden pointer, for compatibility with games built by other compilers; the
  `clip` slot carries that attribute like the stock `trace` **[read]**.
- **Customize record** (passed to `CustomizeEntityToClient`) **[read]**, 104 bytes
  **[derived]**: a full entity state (84 bytes: number int; origin, angles, old origin as
  three floats each; model index and model indices 2, 3, 4 as ints; frame int; skin int;
  effects 32-bit unsigned; render flags int; solid int; sound int; event int), followed at
  offset 84 by the extension fields (20 bytes: `morefx` int, `alpha` float, `scale` float,
  `loop_volume` float, `loop_attenuation` float). The extension part exists only in builds
  with protocol extensions (the default) **[read]**.
