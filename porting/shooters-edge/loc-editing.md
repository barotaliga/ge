# Location files: editing place names in game

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of how the
modern QuakeWorld client lets a player make and edit a map's place names (`locs/<map>.loc`) in
game, for Shooter's Edge's own location editing. No code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-12) read ezQuake 3.6.9 (the current
release, github.com/QW-Group) and its shipped help files. Each fact is marked **read** (seen in
the source or help), **derived** (follows from read facts put together) or **open** (not
settled). Nothing was run. What `configs.md` § Team messaging 4 already says (the file, x y z
in eighths, `//` comments, 63-character names, nearest point, `$loc_name_<x>`) is not repeated
except where it is sharpened. The Quake 2 client's equivalent (Q2PRO) could not be read in that
session: it is **open** (§ 7).

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

Open: its source was not reachable in that session. Its commands, file folder, units, layout
and display are not described here.
