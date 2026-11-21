# The demo watcher's tables, the playlist and small demo settings (clean-room note)

WORKING AREA: deleted when porting ends. M18k4 round 28, client settings: the demo watcher's tables, playlist and small demo settings (ezQuake 3.6.9). Written by a clean-room reader in its own words, with no code; each fact is marked [read] or [derived].

Nothing was measured by running a binary. Quoted text is exactly what the player sees. Already covered elsewhere: `client-demo-settings.md` (automatic recording, naming, the demo clock, jumps) and `autotrack.md` (the autotrack ratings).
"Units" below means the 2D canvas the HUD is drawn on (the console's virtual resolution), where
one character cell is 8 by 8 units.

## `mvd_info`

### Who is listed, in what order

- One row per player. The list holds every client slot that has a name and is not a
  spectator, in slot order (slot 0 first), both teams mixed, never sorted by team, frags or
  name. Spectators are never listed. [read]
- The list is rebuilt when a match starts (the server's `status` leaves standby), whenever a
  player enters an empty slot or leaves one, and at the start marker some match servers write
  into the demo. Between those moments the rows keep their order. [read]
- The setting `mvd_info` (default 0) switches the table on; any non-zero value shows it. In
  the options menu it is not listed (only `mvd_moreinfo` "Moreinfo" and `mvd_status`
  "Status" are). [read]

### `mvd_info_setup` and its codes

- Default: `%p%n ` then glyph 16, `%l`, glyph 17, then ` %h/%a %w`. Glyphs 16 and 17 are two
  characters of the game's 256-glyph character set (in the stock set, the gold square
  brackets), so the default reads like "Nick [Location] 100/200 rl:25". [read]
  [derived: the look of glyphs 16 and 17]
- Codes, each one letter after `%`, case-sensitive: [read]
  - `n`: the player's name as the server sent it.
  - `h`: health, a whole number.
  - `a`: armour value, a whole number (not the armour's kind).
  - `f`: frags.
  - `P` (capital): ping in milliseconds.
  - `l`: the name of the nearest place from the map's location file; with no location file
   loaded it prints the setting `tp_name_someplace` (default "someplace").
  - `w`: the weapon in hand and the ammo it is showing, as `<weapon name>:<ammo>`. The weapon
   name is the player's `tp_name_*` setting for that weapon, printed as stored, so the
   defaults' braces show literally and their colour codes colour it (for example the rocket
   launcher prints as "{rl}" in the rocket launcher's colour). [read] [derived: braces visible]
  - `W` (capital): the best weapon the player owns and the ammo of that weapon's kind, as
   `<weapon name>:<count>`. Best is the first weapon the player owns in the order of the
   setting `tp_weapon_order`, then, if none matched, in the fixed order rocket launcher,
   lightning gun, grenade launcher, super nailgun, nailgun, super shotgun, shotgun, axe
   (impulses 7, 8, 6, 5, 4, 3, 2, 1). The axe and a player with no weapon print a count of
   0. [read]
  - `p`: meant for the powerups held, but prints nothing in this release: the text it would
   build is never filled. [read]
  - `v`: the player's autotrack rating as a decimal with six digits after the point. [read]
- Padding: a whole number between `%` and the letter sets a width. Without a sign the text is
  right-aligned in that width (spaces in front); with a minus sign (`%-12n`) it is
  left-aligned (spaces after). Text longer than the width is not cut. The width counts bytes,
  so colour codes in a name count as characters and colourful names come out narrower. A
  width of 0 or none means no padding. [read] [derived: colour codes count]
- An unknown letter: the `%`, any width and the letter are all dropped and nothing is
  printed in their place. A `%` at the very end of the text, or `%-` or `%12` with nothing
  after, ends the row there. A literal percent sign cannot be printed (`%%` is an unknown
  code). Each row is cut at 1023 bytes, each single replacement at 254. [read] [derived: no
  literal percent]

### `mvd_info_show_header`

- Default 0. When non-zero, one header row is drawn above the players: the same
  `mvd_info_setup` text with every code replaced by a word, with the same widths: `a`
  "Armor", `f` "Frags", `h` "Health", `l` "Location", `n` "Nick", `P` "Ping", `p` "Powerup",
  `v` "Value", `w` "Cur.Weap.", `W` "Best Weap.". The literal characters of the setup (the
  brackets, the slash, the spaces) stay. [read]
- The whole header row is drawn in the character set's red (high) half, every character,
  digits included. [read]

### `mvd_info_x`, `mvd_info_y`

- Both default 0, measured in character cells (8 units each), fractions allowed. [read]
  [derived: fractions]
- x of 0 or more: the left edge of the rows is 8 times x units from the left of the screen.
  x below 0: each row is right-aligned on its own so that it ends |x| cells from the right
  edge, its length counted in bytes (colour codes included). For the header row the length
  used is not the header's, so a negative x places the header unpredictably. [read]
  [derived: the header's unpredictable place]
- y of 0 or more: 8 times y units from the top. y below 0: |y| cells above the top of the
  status bar area (the bottom of the screen less the status bar's height at the current
  view size). [read]
- The rows start one cell BELOW that point: with no header the first player is at y plus 8
  units; with the header, the header is there and the first player one row lower. Each row
  is 8 units below the previous. [read]

### Look

- Fixed 8-unit characters at scale 1; there is no scale, font or proportional setting for
  it. No backing or box. [read]
- Player rows are drawn in the normal (white) half of the character set; colour codes in the
  text (`&c` with three hex digits, `&r` to reset) are honoured when coloured text is on
  (`scr_coloredText`). The header is red as in 3. [read]
- The tracked player's row is not marked in any way. [read]

### When it is drawn

- Only while playing back a multiview demo (MVD), which includes watching a QTV stream,
  since that is MVD playback too. Never in a single-view demo (.qwd), never while connected
  to a server as a spectator. [read]
- It is drawn while the game view is active, also while the scoreboard is held up, but not
  during the intermission or finale screens. [read]

## `mvd_status`

### Rows and contents, top to bottom

`mvd_status` (default 0, menu "Status") draws a block about one player. Every row is in the
character set's red half except the four section titles, which are orange when coloured text
is on and white when it is off. Counts are whole numbers; times are seconds of demo time with
three decimals. [read]

1. The player's name. [read]
2. "Took" (orange). [read]
3. "RL: n LG: n GL: n RA: n YA: n GA:n" (note: no space after the last colon): how many
   times the player came to hold the rocket launcher, lightning gun, grenade launcher (each
   time the weapon newly appears in the inventory, from a pack too), and the red, yellow and
   green armours (each time that armour newly appears or its value rises). [read]
4. "Ring: n Quad: n Pent: n MH: n": powerups taken and megahealths taken (a second mega
   while one is still counting down counts too, detected by health rising above its last
   value while above 100). [read]
5. "Deaths: n". [read]
6. "Average Run:" [read]
7. "Time      Frags TKS" (six spaces after Time). [read]
8. Averages over every life so far, the current life included: seconds alive (width 9,
   three decimals), frags per life and team kills per life (three decimals, no fixed width).
   A life's frags count the times the frag total rose, not by how much. [read]
9. "Last 3 Runs:" [read]
10. "No. Time      Frags TKS" [read]
11. One row per life from three lives back up to the current one, so up to four rows despite
   the title: number (1-based, width 3), seconds (width 9, three decimals), frags (width 5),
   team kills (width 3). The current life's time grows as you watch. [read] [derived: up to
   four rows]
12. "Last Fired Weapon: <name>": the weapon that was in hand the last time a shot animation
   played, by its `tp_name_*` name; after a spawn or death, before any shot, it prints
   `tp_name_none` (default empty). [read]
13. "Lost" (orange). [read]
14. "RL: n LG: n GL: n QUAD: n": deaths while holding each weapon as the weapon in hand,
   and deaths while holding quad. [read]
15. "Kills" (orange). [read]
16. "RL: n LG: n GL: n SNG: n NG: n SSG: n SG: n AXE: n": frags credited to the last fired
   weapon when the frag total rose, by the size of the rise. [read]
17. "SPAWN: n": frags gained before firing any shot in this life (telefrags in practice).
   [read] [derived: telefrags]
18. "Teamkills" (orange). [read]
19. The same eight weapons, counting falls in the frag total by the last fired weapon (so a
   suicide counts here too). [read] [derived: suicides]
20. "SPAWN: n": frag falls before any shot in the life. [read]
21. "Last 3 Quad Runs:" [read]
22. "No. Time      Frags TKS" [read]
23. One row per quad run, from three back up to the current, skipping a run with zero time:
   number, seconds held, frags and team kills during it, same widths as in 11. [read]

- No item, armour or powerup is counted in deathmatch mode 4 (megahealth still is). Nothing
  is gathered during the countdown or standby. [read]

### `mvd_status_x`, `mvd_status_y`

- Same units and the same positive and negative rules as `mvd_info_x` and `mvd_info_y` (both
  default 0, so both tables start at the top-left corner and overlap by default). [read]
  [derived: the overlap]
- Unlike `mvd_info`, the first row is at the point itself, not one row below. [read]
- A negative `mvd_status_x` is measured against a text length that is not yet set when the
  position is worked out, so a negative x puts the block at an unpredictable place. [derived]

### When drawn, and with no tracked player

- Same as `mvd_info`: MVD playback (QTV included), not intermission. [read]
- The player shown is the one the camera tracks. If the tracked slot is not in the player
  list (free flying, or tracking a slot that is not listed), the first listed player is
  shown instead. With no listed players at all, the name row is empty and every number is
  zero. [read]

## `mvd_moreinfo`

### The extra messages

- `mvd_moreinfo` (default 0, menu "Moreinfo") adds one-line pickup messages during MVD
  playback. They go into the frag tracker's lines, and the tracker shows them only when
  `r_tracker_pickups` is 1 (default 0); with it at 0 nothing appears even with
  `mvd_moreinfo` on. [read]
- No message at all is produced from a demo that carries the server's own item markers (the
  kind some match servers write); the watcher's guesses are used only for demos without
  them. [read]
- Wording, where ITEM is the item's name right-aligned to 9 visible characters (colour codes
  not counted), ARROW is glyph 13 of the character set, and [ ] are glyphs 16 and 17: [read]
  - Picked up: "ITEM ARROW player", then " [place]" unless the map holds exactly one item of
   that kind or the place's name equals the item's plain name.
  - Weapon taken from a backpack: "ITEM pack ARROW player [place]", where "pack" is the
   setting `tp_name_backpack` with its braces removed, or the word "pack" when that setting
   is empty. The place is always given.
  - Died holding quad or ring (deathmatch 1 only): "ITEM dead   (player)", with "dead" in red
   and three spaces before the parenthesis.
  - Died with the rocket launcher or lightning gun in hand (deathmatch 1 only): "ITEM drop
   ARROW player [place]", with "drop" in red.
  - A "ITEM over" line for a powerup running out exists in the wording but nothing in this
   release triggers it. [read]
- The item's name is the player's `tp_name_*` setting with braces removed (its colour codes
  kept), or, when that setting is empty, a built-in coloured short name: ssg, ng, sng, gl, rl,
  lg in magenta, ring and ya in yellow, quad and mh in blue, pent and ra in red, ga in
  green. [read]
- The player's name: in a team game it is coloured by the player's bottom colour (bottom
  colour 3 forced to a light green, 12 to yellow) and any red-half characters are turned
  white; outside team games it is the plain name. The place is the location file's name with
  its special characters expanded. [read]
- Which pickups are announced: rocket launcher and lightning gun (deathmatch 1 only; the
  shotguns, nailguns, grenade launcher and green armour never), yellow and red armour, ring,
  quad, pent (not in deathmatch 4), and megahealth (any mode, a second mega included). A
  player's first gathered frame after the list is built announces nothing. [read]
- Lines are also kept in a 16-line store with a 10 second life (fading over the last 2
  seconds), but nothing in this release draws that store. [read]

## The demo playlist

### Commands and menu

- Console commands, none taking arguments: `demo_playlist_next`, `demo_playlist_prev`,
  `demo_playlist_stop`, `demo_playlist_clear`. There is no console command to add, remove or
  start; that is done only in the demo menu. [read]
- `demo_playlist_next` / `demo_playlist_prev`: do nothing, silently, unless a playlist is
  running. Otherwise they stop the current demo and play the next / previous entry, wrapping
  from the last to the first and from the first to the last whatever `demo_playlist_loop`
  says. `demo_playlist_prev` prints "Prev <n>" with the zero-based index first. [read]
- `demo_playlist_stop`: marks the playlist stopped and runs `disconnect`, which ends the
  demo. [read]
- `demo_playlist_clear`: empties the list and marks it stopped, but the demo playing goes on.
  Silent; does nothing on an empty list. [read]
- The menu's demo pages (tabs "Browser", "Playlist", an entry page, and options):
  - Browser: Insert, or Ctrl with Enter or a click, adds the selected demo. On a folder it
   adds every file the browser lists in it (not into subfolders, skipping archives); on a
   zip it adds each file inside whose extension is qwd, mvd, dem or qwz (any case). Enter
   plays a demo without adding it; Shift and Enter runs it as a timedemo. [read]
  - Full list: "Playlist is full, cannot add "<name>" to it. Max allowed demos in playlist
   is 256". [read]
  - Playlist page: "Playlist is empty" when empty. Enter plays the entry under the cursor
   and starts the playlist from it; Delete removes it; Ctrl with Up or Down moves it. [read]
  - Entry page rows: "Currently playing:" and the name, or "Not playing anything"; "Next
   demo", "Previous demo", "Stop  playlist", "Clear playlist" (Enter on each acts like the
   commands); "Currently selected:" or "No demo in playlist"; and "Track" with a 15-character
   box for the entry's own player name, or "Tracking only available with mvds" when the
   entry's name does not end in ".mvd" (any case). Only printable characters (codes 32 to
   127) can be typed into it. [read]
  - Options page "Playlist options": "looping" (`demo_playlist_loop`) and "default track"
   (`demo_playlist_track_name`). [read]
- Entries added from a zip keep only the file's path inside the zip, not the zip's, so they
  will usually fail to open when played. [derived]

### `demo_playlist_loop`

- When a playlist demo's playback ends (by itself, or any other stop that is not
  `disconnect`), the next entry plays. [read]
- At the end of the last entry: with 0 (default) the client prints "End of demo playlist.",
  the playlist stops and its position goes back to the first entry. With any non-zero value
  the first entry plays again, forever. [read]
- On such a loop back, the first entry's own track name is not applied (the name taken is
  the empty one past the end), so only `demo_playlist_track_name` applies in the second and
  later rounds. [derived]
- The `disconnect` command (typed, or from `demo_playlist_stop`) always stops the playlist.
  [read]

### `demo_playlist_track_name` (default empty)

- Which name: the entry's own track name (from the menu's "Track" box) when it is not empty,
  else `demo_playlist_track_name`. [read]
- Matching: each player's name has its colour and red-half characters made plain, a few
  characters (space, slash, backslash, quote, semicolon, line breaks) turned into
  underscores, leading and trailing underscores dropped, and is lower-cased. The wanted name
  is NOT lower-cased, so it must be typed in lower case to match. It matches anywhere inside
  the name; the player whose match starts earliest wins, and on a tie the lower slot.
  Spectators and QTV watchers are never chosen. The client then runs `track` with the
  player's full name. If nothing matches, nothing happens. [read] [derived: lower case needed]
- When: once, in the first client frame of MVD playback (QTV included). It is not retried:
  if the player is not there yet it is missed for that demo. It is armed again when the
  playlist moves to its next demo and by `disconnect`; a demo started by hand with
  `playdemo` after an earlier one that ended without `disconnect` does not run it again.
  [read] [derived: the case of a later hand-started demo]
- It applies to any MVD, not only playlist ones. [read]

## Small settings

### `cl_demoteamplay`

- Default 0. It applies only to the original single-player engine's network demos (.dem),
  which do not record whether the game was a team game. [read]
- At such a demo's start the client's teamplay flag is set from it (any non-zero integer
  means team game). Changing it during such a demo applies at once (any value whose first
  character is not "0" means team game), re-applies team and enemy colour forcing
  (`teamcolor`, `enemycolor` and the like) to the players, and re-marks spectators. [read]
- With it on, a player is treated as a spectator when he has a name, both colours 0 and
  frags of -99 or lower. With it off nobody is. [read]
- Everything else that reads the teamplay flag (team-based scoreboard and displays) follows
  it. [derived]

### `demo_dir` and `cl_mediaroot`

- The note is right: empty `demo_dir` (default) means the current game folder; otherwise
  `cl_mediaroot` 0 (default) puts it under the install's base folder, 1 under the user's home
  folder for the client, 2 takes it as an absolute path; leading slashes are dropped for 0
  and 1. [read]
- `playdemo` (and `timedemo`) look, in order: for a name starting "../", in the base folder
  with that prefix removed, and only there; else in the game's search path (packs
  included); then in the demo folder from `demo_dir`; then the name as a plain path on the
  disk. Without a known extension it tries .qwd, .mvd and .dem in that order. [read]
- The demo browser starts in the folder "qw" under the client's working folder. Whenever
  `demo_dir` is set to a non-empty value (also from a config at start-up) the browser moves
  to it, but under the base folder for both `cl_mediaroot` 0 and 1 (not the home folder for
  1), and as given for 2. [read] [derived: config at start-up]

### `demo_benchmarkdumps`

- Default 1. When on, every timedemo (both `timedemo` and `timedemo2`), when its playback
  stops for any reason, appends one record to the file "timedemo.log" in the log folder
  (`log_dir`, empty meaning the current game folder, with the same `cl_mediaroot` rules as
  `demo_dir`). [read]
- If the file cannot be opened: "Can't open <path> to dump timedemo result". [read]
- The record is a small XML block of several lines, not one line: an outer "timedemo"
  element whose "date" attribute is the local time as year-month-day, the letter T, and
  hours:minutes:seconds; a "system" element holding the platform's name ("os") and a
  hardware description; a "client" element holding the name "ezQuake", the version, the
  build configuration and the renderer; a "demo" element holding the demo's name as it was
  opened; and a "result" element with attributes "frames" (whole number), "time" (an ISO 8601
  duration, "PT" then seconds with six decimals then "S") and "fps" (six decimals). A
  "screen" element exists in the format but is never written. [read]

### `cl_demoplay_flash`

- Default 0.33, clamped to 0 to 1. It scales the strength of the screen tint a server sets
  with its colour-shift command (the one a class-based mod's flash grenade uses), only
  during demo playback, only when the game folder is "fortress" (case-insensitive), and only
  when that tint's red and green parts are equal (a white or grey flash). [read]
- 0 removes such a flash from the demo entirely, 1 leaves it at full strength, 0.33 shows a
  third. It has no effect in any other mod, live play or a tint of another colour. [read]
  [derived: what 0 and 1 look like]
