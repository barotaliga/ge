# Map visibility patch and entity override files: answers

Project read: Q2PRO, the actionquake/q2pro fork, default branch as cloned on 2026-11-19 (head commit dated 2026-11-16). Every fact below is marked **read** (stated by the source), **derived** (follows from what the source does, not written as such) or **measured** (none here: no map files were run).

## 1. How the three maps are recognised

- **read**: Only by the map file's checksum. Name and size play no part. The checksum is the one the server works out when it loads the map file: an MD4 digest of the whole file's bytes, as stored on disk, with its four 32-bit little-endian words XORed together into one 32-bit value. This is the same value the server puts in the map checksum configstring (index 31) when there is no override (see 5 and 6).
- **read**: The identifying values:
  - q2dm1: 0x04cfa792 (80717714 as signed decimal, how configstring 31 shows it)
  - q2dm3: 0x1e5b50c5 (509300933)
  - q2dm8: 0x2c3ab9b0 (742046128)
  - **read**: a fourth map is patched too, beyond the three in our notes: mgu6m2 (a map from the second mission pack's deathmatch set), checksum 0x1ebe8001 (515801089).
- **derived**: A map renamed to another name is still patched. A copy with any byte changed is not, even if its name is q2dm1. A name override (6) has no effect on the patch, and neither does a checksum override, because the test uses the checksum of the loaded file itself, not the value sent to clients.
- **read**: The setting is `map_visibility_patch`, default 1, with no flags (not latched, not archived). Any nonzero value turns the patch on.
- **derived**: Since it is not latched, a change applies the next time a visibility row is expanded, which happens each frame. So it applies at once, with no map restart.

## 2. What is changed, map by map

The patch works on the expanded (decompressed) row of one cluster, which is one bit per cluster: bit n is bit (n mod 8) of byte (n div 8), least significant bit first. It only SETS bits. It never clears a bit and never touches the compressed lump. (**read**)

- **q2dm1** (**read**): the row for cluster 395 gains clusters 176 and 183. The source names the spot as "above the lower rocket launcher" area.
- **q2dm3** (**read**): the rows for clusters 345 and 384 each gain clusters 466, 484 and 692. The spot is named as the bridge by the power-up (the pentagram of protection: "pent bridge").
- **q2dm8** (**read**): the rows for clusters 629, 631, 633 and 639 each gain clusters 908, 909, 910, 915, 923, 924, 927, 930, 938, 939 and 947. The spot is named as the chaingun / railgun area.
- **mgu6m2** (**read**): every cluster's row gains clusters 213, 214 and 217, whatever the viewer's cluster. The spot is named as the waterfall.
- **derived**: The patch runs one way only. For example, on q2dm1, cluster 395 now sees 176 and 183, but the rows of 176 and 183 are not given 395.
- **derived, why**: Because the patch only adds bits, the fault in the stock data must be that the compiled visibility leaves out clusters that a player at those spots can in fact see. Players and items in those clusters were then culled, so they vanished or popped in while in plain view. The source gives no more than the place names. It does not say whether the cause is a room seen through a gap, a missing door area or anything else, so the exact geometric cause is **open**.

## 3. Which set; order relative to other processing; checksum

- **read**: Both. The bits are set in whatever row is being expanded, whether the potentially visible set or the potentially hearable set is asked for. The same cluster numbers are added to both. The hearable set is not recomputed from the patched visible set: the same bits are just added to its stored row.
- **read**: The order is: the stored row is expanded from its run-length form, and then the patch bits are ORed in. It happens on every expansion. Nothing is cached or patched at load time.
- **derived**: Users of the row see the patched version: entity culling for each client, multicast and sound filtering by PVS or PHS, the "fat" PVS that ORs together the rows of every cluster within 8 units of the eye, the in-game visibility queries the game module asks, and the MVD demo server. The renderer of a Q2PRO client also expands rows the same way, so a Q2PRO client with its own `map_visibility_patch` 1 draws the patched clusters too.
- **read**: Area portals are a separate test, made after the row is built. The server first rejects an entity whose area is not connected to the viewer's area (closed doors), and then tests the entity's clusters against the patched row. So area connectivity still hides an entity in a patched cluster behind a closed area portal.
- **read**: The checksum is not changed. The patch alters no stored data, and configstring 31 still carries the map file's checksum (or an override's, see 6).

## 4. The entity file's CRC-16

- **read**: The variant is the CCITT CRC-16 the source calls the XMODEM one: polynomial 0x1021, not reflected (most significant bit first, input bytes not reversed), initial value 0xFFFF, final XOR 0x0000. In the standard catalogue this is CRC-16/CCITT-FALSE (also called CRC-16/IBM-3740). Check value for "123456789": 0x29B1 (**derived** from the parameters).
- **read**: Input is the map's own entity lump, byte for byte as stored in the file, with its length taken from the lump header, minus its last byte. A lump of length 0 gives CRC 0, so the name uses "0000".
- **derived**: In stock maps the last byte of the lump is the terminating NUL, so in practice the CRC covers the entity text without its terminator. The source itself asks why the byte is left out and gives no reason.
- **read**: The CRC is written as exactly 4 lowercase hexadecimal digits, zero-padded (e.g. `q2dm1@0a3f.ent`).
- **read**: The `<map>` part is the name of the BSP file actually loaded, with its leading "maps/" and trailing ".bsp" removed, both matched without regard to case. If that file name is not of the form maps/NAME.bsp, no entity file is looked for. The name keeps any subfolder below maps/.
- **derived**: After a name override (6), this is the redirected name, not the one the admin typed.

## 5. What clients see; limits; messages (entity file)

- **read**: An entity file under `map_override_path` is looked for whenever that setting is not empty. It does not depend on `sv_load_ent`. It is looked for only if a binary override (6) has not already supplied entities. Both names are opened through the game's file search path, so packs count too. The setting `map_override_path` defaults to empty and has no flags.
- **read**: Clients see nothing different. Configstring 31 still carries the map file's checksum (or a binary override's), the map name is the same, and the inline model names are the same. Only the text handed to the game module for spawning changes.
- **derived**: As a result, the entity text is never sent to or checked by a client.
- **read**: The original lump's length is not enforced on the replacement, and the engine sets no entity-text limit of its own. The only cap is the general file-load cap: 0x4001000 bytes (64 MiB plus 4 KiB). Over that, the load fails with the system's "file too large" error text. Any limit the game module's parser has still applies (**derived**: not settled by the engine source).
- **read**: The loaded text gets a NUL appended. Its first NUL ends it as far as the game module is concerned (**derived**).
- **read**: Messages:
  - On success: `Loaded entity string from <path>`, where `<path>` is the name that was found: the hashed name if present, otherwise the plain one.
  - A missing file (neither name exists): nothing is printed, and the map's own entities are used.
  - A file of 0 or 1 bytes: `Couldn't load entity string from <path>: File too small`
  - Any other read error: the same `Couldn't load entity string from <path>: <reason>` line, where the reason is the system's error text (e.g. for a name longer than 63 characters, the system's "file name too long" text).
  - All failure lines go out on the error-print channel (shown as an error, usually in red). After any failure the map's own entities are used.
- **read**: The admin command `dumpents <name>` writes the map's ORIGINAL entity lump (never an override) to `entdumps/<name>.ent` and prints `Dumped entity string to <path>`. With no map it prints `No map loaded.`; with the wrong argument count, `Usage: dumpents <filename>`.
- **read**: The MVD demo client loads maps the same way, so entity files apply to it too.

## 6. The binary override file

- **read**: The file is used only when `sv_load_ent` is nonzero (default 1, latched, so it takes effect on the next map change) and `map_override_path` is not empty. The file is `<map_override_path>/<typed>.bsp.override`, where `<typed>` is the map name as given to the map command. The path must fit in 63 characters, or the load fails with the system's "file name too long" text. A missing file is silent.
- **read**: The `map` command's tab completion also offers every `*.bsp.override` under the path (searched recursively), with the suffix removed.
- **read**: All 32-bit values are little-endian. The layout, in order:
  1. A signed 32-bit word of flag bits: 1 for the name, 2 for the checksum, 4 for the entities.
  2. If bit 1 is set: 64 bytes holding a name. It must contain a NUL within the 64 bytes and must have the form `maps/NAME.bsp` (prefix and extension matched without regard to case).
  3. If bit 2 is set: a 32-bit checksum.
  4. If bit 4 is set: a 32-bit length, which must be greater than 0, then that many bytes of entity text. A NUL is appended, and NULs inside the text are not checked.
  5. Bytes after the last field are ignored.
- **read**: The name is a redirect, not a label. NAME (the "maps/" and ".bsp" stripped) replaces the map name the server works with from that point on. The BSP actually loaded is `maps/NAME.bsp`, and the same name is used for:
  - configstring 0 (the level name, used when the level sets no message of its own)
  - the server's map name given to the game module for spawning
  - the world model's configstring (`maps/NAME.bsp`), which is the file clients load
  - **derived**: the `changing map=NAME` line broadcast to clients
- **derived**: The typed name only picks the override file.
- **read**: The checksum replaces the value sent to clients in configstring 31, written as signed decimal. The checksum used for the visibility patch is still the loaded file's own (1).
- **derived**: A Q2PRO client compares its local file's checksum with configstring 31. If they differ, it drops with an error headed MAP VERSION MISMATCH (between rows of equals signs) (with local and server values and a hint to delete and re-download). A demo being played only warns that the local map version differs from the demo's, giving both values. So a checksum override is for making clients accept a file that differs from the one the server has.
- **read**: The entities replace the map's entity text, the same as an entity file does. When bit 4 is set, no `.ent` file is tried. The `.ent` lookup still happens if only bits 1 and/or 2 are set.
- **read**: On success the server prints `Loaded <path>`.
- **read**: On any failure it prints `Couldn't load <path>: <reason>` on the error channel. Then nothing from the file is applied: the name is not redirected, the checksum reverts to the file's, and no entities are taken. Reasons:
  - Any bit other than 1, 2 or 4 set: `Invalid file format`.
  - A file shorter than 4 bytes: `Invalid file format`. **derived**: a read past the end yields all bits set, which counts as unknown bits.
  - A name field cut short, missing its NUL in 64 bytes, or not of the form maps/NAME.bsp: `Invalid file format`.
  - An entity length of 0 or negative, or text shorter than the stated length (including a length field cut short, which reads as -1): `Invalid file format`.
  - A read error other than "missing": the system's error text (e.g. "file too large" past the 64 MiB + 4 KiB cap).
- **read, edge case**: A file with only bit 2 set whose checksum field is cut short does NOT fail. The short read yields -1, the load succeeds, and configstring 31 becomes `-1`.
- **read**: If the map then fails to load (for example the redirected BSP is missing), the server prints `Couldn't load maps/<name>.bsp: <reason>` and the map command is refused.
