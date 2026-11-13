# Location files: editing place names in game

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of how the
modern QuakeWorld client lets a player make and edit a map's place names (`locs/<map>.loc`) in
game, for Shooter's Edge's own location editing. No code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-12) read ezQuake 3.6.9 (the current
release, github.com/QW-Group) and its shipped help files. Each fact is marked **read** (seen in
the source or help), **derived** (follows from read facts put together) or **open** (not
settled). Nothing was run. What `configs.md` § Team messaging 4 already says (the file, x y z
in eighths, `//` comments, 63-character names, nearest point, `$loc_name_<x>`) is not repeated
except where it is sharpened. The Quake 2 client's equivalent (Q2PRO) was read in a second
clean-room session (2026-11-12, commit 601a8df8433b0c50dbbe37c0716c3793fff140a7 of
github.com/q2pro/q2pro, 2025-12-11): § 7.

---
## 1. Commands and cvars

Every location command has a long name and an older short name that still works by re-running
the long one with the same argument text (read). The usage lines name the SHORT form (read).

| Command | Old name | Arguments | Does |
| --- | --- | --- | --- |
| `locations_loadfile` | `loadloc` | exactly one file name | reads `locs/<name>` (`.loc` added when the name has no dot), replacing every point in memory if found |
| `locations_savefile` | `saveloc` | exactly one file name | writes every point to `locs/<name>` in the current game folder (§ 4) |
| `locations_add` | `addloc` | exactly one name (quote it for spaces) | appends a point at the player (§ 2) |
| `locations_remove` | `removeloc` | none | removes the nearest point (§ 3) |
| `locations_clearall` | `clearlocs` | none | forgets every point in memory; prints "Cleared N locs." |

All of that is read. Wrong argument counts print a one-line usage and do nothing (read).
`locations_loadfile` prints "Loaded locfile "<file>.loc" (N loc points)", "... was empty", or
"Could not load locs/<file>.loc" (points already in memory are then kept) (read).

Cvars (read): `tp_loadlocs` (default 1) loads the map's file at map start; `tp_name_someplace`
(default `someplace`) is the location when there are no points; `loc_name_<key>` cvars are not
built in, a player `set`s them; the read-only `mapname` makes `locations_savefile $mapname`
work (derived). The team-info HUD element shows a teammate's location in a column of
`loc_width` characters (default 5), "unknown" when empty (read).

Not present (read): listing points, renaming, drawing points in the world, teleporting to one.
`mapgroup <group> <maps...>` (player groups; the built-in `exmy` group of e1m1..e4m8 cannot be
changed) decides the second file tried at map start (read).

## 2. Adding a point

- Refused unless in a game (demo playback counts, derived): "Need to be active to add a
  location." (read).
- The point is the player's predicted ORIGIN, the centre of the player box: 24 u above the
  feet, 22 u below the eye (read: origin; derived: the offsets). A spectator tracking a player
  stores that player's origin from the latest frame; a free spectator its own (read).
- Kept at full precision in memory; rounded to eighths only on save (read).
- Prints "Added location "<name>" at (x, y, z)", whole units, each at least 4 wide (read).
- No duplicate or nearness check, no limit on the count; always appended at the end (read).
- The name is not cut at add time (read). `$` text outside double quotes is expanded when the
  command runs; inside quotes it is kept, which is how a `$loc_name_ra` placeholder is stored
  (derived from the command line's expansion rule).

## 3. Removing a point

- Removes the single point nearest the player's origin (or the tracked player's), straight-line
  3D distance, no radius; on a tie the earlier point (read). The rest keep their order (read).
- Prints "Removed location "<name>" at (x, y, z)", or "There is no locations left for
  deletion!" (read). Does not require being in a game (read).

## 4. Saving

- No default file name (read). Refused with no points, with a name over 64 characters, or a
  path over the limit, each with a message (read). Success prints nothing (read).
- Written to `locs/<name>[.loc]` under the current game folder (user folder if set at start-up,
  else the home folder's game folder, else `<base>/<gamedir>`), folders created, an existing
  file overwritten without asking (read).
- Layout (read): one line per point in list order (the loaded file's order, then additions),
  no header, no comment; three integers = coordinate × 8 rounded to nearest, halves away from
  zero, each right-aligned in a field at least 4 wide (space padded), one space between, one
  space, the name exactly as stored (placeholders and `$` text unexpanded), LF line ends on
  every platform, the last line too.
- Quirk (derived): each line is built in 128 bytes, so an in-game name over about 110
  characters loses its line end and the next point joins its line.
- A point survives save and reload quantized to 1/8 u (derived).

## 5. Loading

- At map start, only when the map's NAME differs from the last one loaded for: forget all
  points; then, when `tp_loadlocs` is on and the server's `deathmatch` is not 0, or a demo is playing, read
  `locs/<map>.loc` quietly, else `locs/<group>.loc` for a player-made map group (read).
- So (derived): reconnecting to the same map keeps unsaved edits; changing map discards them, and
  where loading is skipped, every map start (even of the same map) wipes the points.
- Found through the search path, paks included; folders id1, ezquake, qw, home, extra data
  folders, mod folder, the later searched first (read: the order added; derived: the order
  searched). A saved file is on that path, so the next load finds it (derived).
- Reading (read): leading blanks skipped; empty lines and lines starting `//` skipped (a `//`
  after the numbers is part of the name); each coordinate an optionally negative integer
  followed by a space or tab (no `+`, no decimal point; a bad line prints "Locfile error (line
  N): couldn't parse coords" or "... unexpected '-'" and is skipped); the name is the rest of
  the line, inner and trailing blanks kept, CRs dropped, may be empty, cut to 63 with one
  warning per line.

## 6. Showing, and what may run the commands

- Nothing is drawn in the world (read). The location shown is the nearest point's name, its
  `$loc_name_<key>` placeholders replaced (keys ssg ng sng gl rl lg ga ya ra quad pent ring
  suit, `separator` = `-`, `mh` = `mega`, matched case-insensitively as prefixes; an unknown
  key leaves the text), then the whole text `$`-expanded; a location macro inside a name
  expands to nothing (read).
- Server-stuffed text may only run what `cl_remote_capabilities` lists; no location command is
  in its default, so a stuffed one prints "Blocked <command>: not in cl_remote_capabilities"
  (read). The old short names are re-run outside the server's context, so as read they escape
  that filter (derived, not run). Message triggers and lines that used teamplay macros cannot
  run them (read). Whether a demo's stuffed text is filtered the same way: open.
- The client hides server lines starting "Location :" or "Angles   :" from message triggers
  (read).

## 7. The Quake 2 client (Q2PRO)

Every fact here is read unless marked otherwise.

**The file.**
- The path is `locs/<map>.loc`, where `<map>` is the map's name without `maps/` and `.bsp`. It
  is found through the whole search path: packs included, the mod's folders first.
- Each line is `x y z name`, and lines are split at LF.
- **Coordinates are eighths of a unit**: the file's value times 0.125 on reading, times 8 on
  writing. Each is read as a single-precision decimal number, so a fraction or an exponent is
  accepted, and text that is not a number counts as 0 [derived].
- Fields are separated by any run of characters at or below the space, tabs included.
- The name is the raw rest of the line from the fourth field: inner blanks kept, trailing
  blanks and a CR dropped, double quotes kept as characters [derived].
- **No comment syntax.** A line with 1 to 3 fields warns "Line <n> is incomplete in
  locs/<map>.loc" and is skipped. So a `// note` line of four or more words becomes a point at
  0 0 0 [derived]. Blank lines are skipped.
- A line of 1,024 characters or more prints "Line exceeded 1024 chars, discarded.".
- At most 256 fields are read from a line, so a longer name is cut [derived].
- There is no other limit on the name or the number of points.

**Loading.**
- The file is read once per map, after the downloads and just before the client reports ready.
- Every new map's connection data clears all points, and so does a disconnect. So unsaved
  edits are lost on any map change or reconnect, even to the same map [derived].
- No command reloads the file.
- **Demos do not read the file.** Their precache path skips it, but the commands work during
  playback.
- Locations are client-side, so they work on any server.
- The only message on success is at developer level: "Loaded <n> location(s) from <path>".

**The command.** There are no `loc_add`, `loc_delete`, `loc_update` or `loc_write` commands.
There is one command, `loc`, with sub-commands. A bad or missing sub-command prints "Usage: loc
<add|del|set|list|save>". Every sub-command needs an active game, and prints "Must be in a
level." otherwise.

| Sub-command | Does | Prints |
| --- | --- | --- |
| `loc add <name>` | appends a point at the player; the name is the remaining words joined by single spaces, quotes removed | "Added location <name> at (x y z)"; "Usage: loc add <name>" |
| `loc del` | removes the nearest point | "Deleted location <name> at (x y z)"; "No closest location." |
| `loc set <name>` | renames the nearest point in place (same position, same list slot) | "Renamed location at (x y z) to <name>"; "No closest location."; "Usage: loc set <name>" |
| `loc list` | lists all points in order | "<name> at (x y z)" each, then "<n> location(s) listed"; "No locations to list." |
| `loc save [file]` | writes every point; the file name defaults to the map's | "Wrote <n> location(s) to locs/<file>.loc"; "No locations to write."; "Couldn't open <path>: <reason>"; "Error writing <path>" |

Positions in messages are whole units, not padded.

**The point added.**
- It is the player's entity origin as the view is computed that frame (interpolated and
  predicted), taken **before** the eye offset is added. That is the centre of the player box,
  not the eyes [derived from the game's player box].
- It is kept at full precision until saved.
- There is no check for duplicates or for nearness.

**Nearest** (used by del, set, the macros and the drawing):
- Straight-line 3D distance.
- Points beyond `loc_dist` (default 500 units) are ignored; exactly 500 still counts.
- With `loc_trace 1` (default 0), a point whose straight line from the position is blocked by
  solid world geometry is ignored.
- The smallest distance wins. On a tie, the earlier point in the list wins.

**Saving.**
- The name is cleaned (`..` and doubled slashes removed, invalid path characters turned into
  `_`). `.loc` is added unless the name already ends with it.
- It is written under `locs/` in the **write folder**: the user folder's game folder when one
  is set, else the base folder's, and the mod's folder when a mod is active.
- Folders are created and an existing file is overwritten.
- `locs/<file>.loc` must fit 63 characters, so the name can have at most 54 [derived].
- **Layout**: one line per point in list order: the three values times 8 as whole numbers, then
  the name, with single spaces between and no padding.
- Rounding sends halves to the even neighbour, and a small negative value is written as `-0`.
  Both were measured with glibc's printf. Windows was not measured.
- Line ends are LF on Linux and macOS and CRLF on Windows (text mode) [derived]. Every line
  ends, the last one too.

**Display.**
- `$loc_here` is the nearest point to the player's position.
- `$loc_there` traces 8,192 units from the player's position (not the eyes) along the view
  direction, against solid world geometry, and takes the nearest point to where the trace
  stops.
- Both give **`unknown`** when nothing qualifies or the client is not in a game.
- The forms `$loc_here`, `$loc_here$`, `${loc_here}` and `${$loc_here}` all expand.
- `loc_draw 1` (default 0) draws the item model `models/items/c_head/tris.md2` at every point
  within `loc_dist`. The nearest point's model bobs 10 units with a period of about 0.63 s
  [derived] and carries a white light of radius 200.
