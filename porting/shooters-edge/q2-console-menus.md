# Answers: Quake II client console commands, macros, keys, printed lines and menus

Project read: Q2PRO (upstream, github.com/q2pro/q2pro), at commit 601a8df. Licence GPL-2.0.
Every fact below is marked **[read]** (read from the source at that commit), **[derived]**
(worked out from what was read, not observed) or **[measured]** (none here: nothing was run).
Printed lines are quoted word for word; `%s`, `%i`, `%d` stand for the value printed in that
place, and a trailing "(newline)" is written only where its absence matters.

## A. Console commands

### 1. `toggle`

- **[read]** With no argument it prints `Usage: toggle <variable> [values]` (the
  command's own name is printed in place of `toggle`, so an alias or another spelling shows that
  word).
- **[read]** If the named setting does not exist it prints `<name> is not a variable` (the name
  as typed, then " is not a variable"). It does not create the setting.
- **[read]** With no values: the value is compared as text. Exactly "0" becomes "1", exactly "1"
  becomes "0". Any other text (including "0.0", "2", "" or " 1") is left unchanged and the line
  `"<name>" is "<value>", can't toggle` is printed (both in double quotes).
- **[read]** With values: the current value is compared against each listed value in order,
  ignoring letter case, as whole strings (no numeric comparison: "1.0" does not match "1"). On
  the first match it sets the NEXT listed value, wrapping from the last back to the first. The
  value written is the list's spelling, not the old value's case.
- **[read]** If the current value matches none of the listed values nothing changes and it
  prints `"<name>" is "<value>", can't cycle`. It does not jump to the first value.
- **[derived]** A list of one value that matches sets the same value again (a no-op change).
- **[read]** The set goes through the normal setting path, so a read-only, cheat-protected or
  command-line-only setting prints that refusal instead (see the setting-change lines: `<name> is
  read-only.`, `<name> is cheat protected.`, `<name> may be set from command line only.`, and for
  a latched setting `<name> will be changed for next game.`).
- **[read]** Tab completion offers setting names for the first argument and, for later arguments,
  the setting's own value list where the setting has one.

### 2. `setu` and `sets`

- **[read]** Arguments: `<variable> <value...>`. Every word after the name is joined with single
  spaces into the value (so `setu name Big Bob` sets "Big Bob").
- **[read]** With fewer than two arguments: `Usage: setu <variable> <value>` (or `sets`), i.e.
  "Usage: " then the command word then " <variable> <value>".
- **[read]** `setu` marks the setting as userinfo (sent to the server in the player's info),
  `sets` marks it as serverinfo. The two marks replace each other: `setu` on a serverinfo setting
  removes the serverinfo mark, and vice versa (the new info mark replaces whatever info marks the
  setting had). No other marks (archive, latch and so on) are added or removed.
- **[read]** An unknown name is created as a user-made setting carrying that mark and the value.
- **[read]** For an existing setting, a userinfo change forces the userinfo to be sent again even
  if the value did not change (for server admin tools that watch it).
- **[read]** `set <variable> <value> u` and `set <variable> <value> s` do the same when given
  exactly three arguments after `set`; `set` with fewer than two prints
  `Usage: set <variable> <value> [u / s]`. With four or more words and a last word other than
  `u`/`s`, all words after the name form the value.
- **[read]** `seta` is the third member of the family: it sets (creating if unknown) and marks the
  setting archived unless it is one that may never be archived (read-only, cheat, command-line,
  private or never-archive settings).

### 3. `_echo`, `text`, `complete`

- **[read]** `echo` prints its arguments joined by single spaces, then a newline. Quotes are
  removed and runs of spaces collapse, because the line is split into words first.
- **[read]** `_echo` prints the raw rest of the line exactly as typed (original spacing kept; one
  pair of surrounding double quotes removed), and takes options before the text:
  - `-e` / `--escapes`: interpret backslash escapes (`\a \b \t \n \v \f \r \\`, `\xHH` two hex
      digits, `\` followed by octal digits, and `\` before any other character gives that
      character).
  - `-c <color>` / `--color=<color>`: print in this colour.
  - `-l <level>` / `--level=<level>`: print at a message level, chosen by the first letter:
      a(ll), t(alk), d(eveloper), w(arning), e(rror), n(otice). Any other first letter prints
      `Bad print level: <arg>` and nothing else.
  - `-n` / `--no-newline`: no trailing newline.
  - `-h` / `--help`: prints `Usage: _echo [-hec:l:n] [text]`, then `Print a line of text into the
      console.`, then a blank line, `Available options:`, one line per option in the form
      `-<letter> | --<long name padded> | <help>` (help texts: "display this message", "enable
      interpretation of backslash escapes", "print text in this color", "print text at this
      message level", "do not output the trailing newline"; options taking a value show as
      `color=<color>` and `level=<level>`), and a blank line.
  - Unknown option: `Unknown option: <arg>.` then `Try '_echo --help' for more information.`,
      an option missing its value: `Missing argument to <arg>.` then the same hint.
- **[read]** Both `echo` and `_echo` see the line AFTER macro expansion (expansion happens to the
  whole command line before it is split, outside double quotes). So `_echo` does not avoid macro
  expansion; text inside double quotes is not expanded for either.
- **[read]** `text <words>` inserts its arguments (joined with single spaces) plus a newline at the
  FRONT of the command buffer that is running it, so it executes next, before the rest of the
  buffer. If the buffer would overflow, a warning `Cbuf_InsertText: overflow` style line is
  printed (the warning names an internal routine; not a player-facing string) and nothing is
  inserted.
- **[read]** `complete <name>` does not print completions. It registers `<name>` as a command
  name with no local action, so that the name tab-completes and, when typed, is forwarded to the
  server like the other forwarded commands. With no argument it prints `Usage: complete
  <command>` with NO trailing newline. If the name is already a setting it prints `<name> is
  already defined as a cvar`; if it is already a command it does nothing silently. On a
  dedicated server it does nothing.
- **[read]** Running such a placeholder command when not in a level prints
  `Can't "<name>", not connected` (see 47).

### 4. `resetall`

- **[read]** Sets every setting back to its default, through the normal setting path (so latched
  settings print `<name> will be changed for next game.` and cheat-protected ones may refuse
  while connected, each with its usual line). It skips: read-only settings; command-line-only
  settings once the engine has finished starting; and the game-directory setting (`game`).
  User-created settings are NOT skipped (they are set to the value they were created with, their
  default).
- **[read]** It prints no summary line of its own.

### 5. `writeconfig`

- **[read]** Form: `writeconfig [-a] [-b] [-c] [-m] [-h] <filename>`.
  - `-a` / `--aliases`: write aliases, one line each, `alias "<name>" "<value>"`.
  - `-b` / `--bindings`: write key bindings, one line each, `bind <key> "<command>"`, for every
      key with a non-empty binding, in key-code order.
  - `-c` / `--cvars`: write archived settings, one line each, `seta <name> "<value>"`.
  - `-m` / `--modified`: write every setting whose value differs from its default, of any kind
      (not only archived ones), one line each, `set <name> "<value>"` (plain `set`, no archive
      mark). It overrides `-c`.
  - Settings are written in alphabetical order of name (byte order).
  - In both settings modes, settings that may never be archived (read-only, cheat-protected,
      command-line-only, private, never-archive) are skipped, and a latched setting writes its
      pending value.
  - `-h` / `--help`: `Usage: writeconfig [-abchm] <filename>`, then `Save current configuration
      into file.`, then the options block as in 3 (help texts: "write aliases", "write bindings",
      "write archived cvars", "display this help message", "write modified cvars").
- **[read]** Default with no switches: bindings and archived settings (as `-b -c`). Aliases are
  not written by default.
- **[read]** The file starts with the line `// generated by q2pro`. Each part follows a blank line
  and a header: `// key bindings`, `// command aliases`, then `//archived cvars` or `//modified
  cvars` (no space after the slashes on this last header). Order: bindings, aliases, settings.
- **[read]** Folder: `configs/` under the game's write directory. Extension `.cfg` is appended
  unless the name already ends in `.cfg`. The name is normalised so it cannot climb out of the
  folder, and characters unsafe in file names are replaced by underscores; an empty name is
  refused.
- **[read]** Lines: success `Wrote configs/<name>.cfg.` (the path relative to the game folder,
  then a full stop). No filename: `Missing filename argument.` then `Try 'writeconfig --help'
  for more information.`. Open failure (error level): `Couldn't open <path>: <reason>`. Write
  failure on close (error level): `Error writing <path>`.

### 6. `macrolist`

- **[read]** Optional argument: a wildcard filter on the macro name. One line per macro (the most
  recently registered macro first), the name left-aligned and padded to 16 characters,
  a space, then the macro's current value. Then the count line `<shown> of <total> macros`.
  `cmdlist` is the same for commands with `<shown> of <total> commands`; `cvarlist` ends with
  `<shown> of <total> cvars`.

### 7. `lasterror`

- **[read]** Prints the last error message (the last line printed at error level, or the last
  fatal/drop error), with its trailing newline removed, followed by a newline. When there has been
  none it prints an empty line (the "No error" text exists only for other readers of the last
  error, not for this command).

### 8. `sizeup` / `sizedown`

- **[read]** Add or subtract 10 to `viewsize` (default 100, archived). The set is unclamped
  itself; the value is clamped to 40..100 when the screen layout is next worked out, which writes
  the clamped value back into the setting. So `sizeup` at 100 briefly sets 110 then reads back
  100; `sizedown` at 40 goes to 30, then back to 40.
- **[read]** Nothing is printed beyond any refusal from the setting path.

### 9. `viewpos`

- **[read]** One line: the view origin as `(<x> <y> <z>)`, each rounded to a whole number with no
  decimals, then ` : `, then the view yaw in degrees with no decimals. Example shape:
  `(1024 -512 88) : 90`. Order x, y, z, yaw. Pitch and roll are not printed.

### 10. `userinfo`

- **[read]** Prints `User info settings:` then one line per userinfo key: the key left-aligned and
  padded to 20 characters, one space, then the value. A key with an empty name shows as
  `<MISSING KEY>` and an empty value as `<MISSING VALUE>` (in practice settings with empty values
  are left out of the userinfo, so they do not appear). Private settings are left out. Order:
  alphabetical by setting name (byte order, so capitals before lower case). Only printable ASCII is kept
  in values.

### 11. `skins`

- **[read]** Not in a level (before the level's resources are loaded): `Must be in a level to
  load skins.`.
- **[read]** In a level: reloads the view-weapon models, then for each client slot that has a
  skin string, reloads that client's model, skin, icon and weapon models, and prints
  `client <n>: <skin string> --> <model>/<skin>`, where `<skin string>` is the server's
  "name\model/skin" text and the right side is what was actually loaded (the base model and skin
  are shown when that client's own could not be loaded). The screen is redrawn after each line.
  Nothing is downloaded by this command itself.

### 12. `serverstatus`

- **[read]** With no address: uses the address of the server the client is connected (or
  connecting) to. If there is none: `Usage: serverstatus [address]`.
- **[read]** A bad address: `Bad address: <text>`. Default port 27910 when none is given.
- **[read]** It sends an out-of-band "status" request. The answer block printed:
  `Status response from <ip:port>`, a blank line, then the server's info as key/value lines (key
  padded to 20 characters, a space, the value; as in 10), then a blank line, then the header
  `Num Score Ping Name`, then one line per player: number from 1 right-aligned in 3 characters,
  a space, score right-aligned in 5, a space, ping right-aligned in 4, a space, the name.
- **[read]** Players are sorted by score, highest first (ties keep no defined order). At most a
  fixed number of players is read (the status player limit).
- **[read]** An info string that fails validation is replaced by a single key `hostname` with value
  `badinfo`. Over-long info strings are cut to 511 characters.
- **[read]** There is no line for no answer or timeout: a reply that arrives more than 6 seconds
  after the request is ignored silently.

### 13. `pingservers`

- **[read]** Sends the out-of-band request "info 34" (protocol 34) to the LAN broadcast address on
  port 27910, then to each address-book entry `adr0`, `adr1`, ... in order, stopping at the first
  number that does not exist as a setting (up to 64), skipping empty ones. Each reply is printed as
  the raw text the server sends back (Quake II servers answer one line: host name, map and player
  count). LAN broadcast replies are accepted for 3 seconds and only from LAN addresses; direct
  replies for 6 seconds.
- **[read]** A bad address-book entry prints `Bad address: <text>` and is skipped. The "Pinging
  ..." lines are developer-level only.

### 14. `dumpclients`, `dumpstatusbar`, `dumplayout`

- **[read]** All three, when not fully in a level: `Must be in a level to dump.`.
- **[read]** `dumpclients`: one line per client slot that has a name: slot number right-aligned in
  3 characters, `: `, the name. No header or count.
- **[read]** `dumpstatusbar <file>` / `dumplayout <file>`: write the raw text of the status bar
  program (the server's status bar layout string) or of the current layout (scoreboard etc.) to
  `layouts/<file>.txt` (folder `layouts/`, `.txt` appended unless present, same name cleaning as
  in 5). Wrong argument count: `Usage: dumpstatusbar <filename>` / `Usage: dumplayout
  <filename>`. Nothing to write: `No status bar to dump.` / `No layout to dump.`. Success:
  `Dumped status bar program to layouts/<file>.txt.` / `Dumped layout program to
  layouts/<file>.txt.`. Open failure as in 5.

### 15. `aashot` and `aadump`

- **[read]** Both need a level: `Must be in a level.`.
- **[read]** The text picture is 80 columns by 40 rows of spaces. The status bar program is drawn
  first, then the current layout over it, each as text at one character per 8 virtual pixels:
  - Positions: left/top offsets divide by 8; right/bottom offsets count from column 80 / row 40,
      the "virtual centre" offsets start at column 20 (80/2 minus 20) and row 5 (40/2 minus 15).
  - Pictures (by stat or by name) draw nothing.
  - A number field is right-aligned in its width (width capped at 5; digits beyond the width are
      cut from the right).
  - Strings and stat strings are written at the position; centred strings are centred on the
      position plus 20 columns; text past column 80 is cut.
  - A deathmatch client block writes four lines, indented 4 columns: the name, `Score: <n>`,
      `Ping:  <n>`, `Time:  <n>` (two spaces after "Ping:" and "Time:").
  - A CTF client block writes one line: score right-aligned in 3, a space, ping right-aligned in 3
      (capped at 999), a space, the name cut/padded to 12.
  - A conditional on a stat skips to its end marker when the stat is zero.
  - Character mapping: the high (coloured) bit is dropped; the special bar characters become
      `>` (13), `[` (16), `]` (17), `<` (29), `=` (30), `>` (31); other control characters become
      spaces.
- **[read]** `aashot [name]`: with a name, writes `scoreshots/<name>.txt` (extension appended
  unless present). Without, the first free of `scoreshots/quake000.txt` .. `quake999.txt` (three
  digits, from 000), opened exclusively. All taken (error level): `All scoreshot slots are
  full.`. Other open failure (error level): `Couldn't exclusively open <path> for writing:
  <reason>`. Success: `Wrote <path>.`; write failure (error level): `Error writing <path>`. Every
  one of the 40 rows ends in a newline.
- **[read]** `aadump` prints the same picture to the console instead (39 rows end in a newline,
  the last row is followed by one newline from the print).

### 16. `timerefresh`

- **[read]** Needs a level: `No map loaded.`.
- **[read]** It renders 128 frames from the current view position, turning the yaw by 360/128
  degrees (2.8125 degrees) per frame, from 0 up to 357.1875 degrees. Each frame is a full begin,
  render, end (page flip) cycle. With one extra argument of any kind (exactly one),
  all 128 views are rendered inside a single frame with one page flip at the end, with no page flip
  between views. Two or more extra words give the normal mode.
- **[read]** Then prints `<seconds> seconds (<fps> fps)` with both numbers in the C default of six
  decimals, for example `1.234000 seconds (103.727715 fps)`. Time is wall-clock milliseconds,
  fps is 128 divided by the seconds.

### 17. `draw` and `undraw`

- **[read]** `draw <name> <x> <y> [color]`. Fewer than three arguments (but more than none):
  `Usage: draw <name> <x> <y> [color]`.
- **[read]** `<name>` is looked up first as a macro, then as a setting. A name that is neither
  creates an empty placeholder setting of that name (so it draws an empty string until set).
- **[read]** Only the VALUE is drawn (the macro's current text or the setting's string), never the
  name.
- **[read]** Colours: `alt` (the alternate text colour), `none` (draw in the text's own colour; the
  same as giving no colour), one of the names `black`, `red`, `green`, `yellow`, `blue`,
  `cyan`, `magenta`, `white`, an index `0`..`7` for those eight, or a hex colour `#RGB`,
  `#RRGGBB` or `#RRGGBBAA`. Anything else: `Unknown color '<arg>'` and nothing is added.
- **[read]** Position: in HUD units (the screen divided by the HUD scale, `scr_scale`, so it scales
  with the HUD). A negative x counts from the right edge (x plus HUD width plus 1) and makes the
  text right-aligned on that point; a negative y counts from the bottom (y plus HUD height minus
  one character height plus 1). Font: the HUD's text font (`scr_font`, default the console
  charset), 8 by 8 units per character. The text is drawn after the centre print and the net
  graph, every frame the HUD is drawn.
- **[read]** Drawing the same name again replaces its position and colour (no duplicate). There
  is no limit on how many names are drawn.
- **[read]** `draw` alone: with none registered `No draw strings registered.`; otherwise a header
  of two lines, `Name               X    Y` and `--------------- ---- ----`, then one line per
  entry, the name left-aligned in 15, a space, x right-aligned in 4, a space, y right-aligned in
  4, in the order they were added.
- **[read]** `undraw <name>` removes one; `undraw all` removes all and prints `Deleted all draw
  strings.`. Wrong argument count: `Usage: undraw <name>`. None registered: `No draw strings
  registered.`. Name not drawn: `Draw string '<name>' not found.`. Tab completion after
  `undraw` offers `all` and the drawn names.

### 18. `play`, `stopsound`, `soundlist`, `soundinfo`

- **[read]** `play <sound> [...]` plays each named sound in turn, all at once. `.wav` is added when
  the name has no extension. The name is looked up under `sound/` (a leading `#` means a path
  from the game root instead; a leading `*` is a player-model sound placeholder). Each plays at
  full volume (1.0) with no attenuation (heard everywhere, not placed in the world), on the
  player's own entity, on the automatic channel. No arguments: `Usage: play <sound> [...]`.
- **[read]** A missing file prints nothing (the sound is silently not played). A file that exists
  but cannot be read or decoded prints, at error level, `Couldn't load <path>: <reason>`. Tab
  completion lists `.wav` files under `sound/`, without the extension.
- **[read]** With the sound system not started, `play` does nothing and prints nothing.
- **[read]** `stopsound` stops every playing and pending sound; prints nothing.
- **[read]** `soundlist`: one line per known sound. A loaded sound: first character `L` if it loops
  or a space if not, then `(<bits>b) (<channels>ch) <bytes> : <name>` with bits right-aligned in
  2 and the size right-aligned in 6, for example `L(16b) (1ch)  23456 : sound/world/amb10.wav`.
  A placeholder (player sound) shows `  placeholder : <name>`; a sound that failed shows
  `  not loaded  : <name> (<reason>)`. Then `Total sounds: <n> (out of <slots> slots)` and
  `Total resident: <bytes>`.
- **[read]** `soundinfo`: if sound is not running, `Sound system not started.`. Otherwise it
  depends on the back end. Software mixing prints six numbers right-aligned in 5 followed by a
  word: `channels`, `samples`, `samplepos`, `samplebits`, `submission_chunk`, `speed`, then
  the buffer's memory address followed by ` dma buffer`. The OpenAL back end prints
  `AL_VENDOR: <text>`, `AL_RENDERER: <text>`, `AL_VERSION: <text>`, `AL_EXTENSIONS: <text>`
  and `Number of sources: <n>`.

### 19. `ogg`

- **[read]** Tracks: the client builds a list at start-up of every music file in a `music/` folder
  of each game search path (recursively), plus (not recursively) `music/` beside the home and the
  base folders. Formats are those the media library supports out of `.flac`, `.opus`, `.ogg`,
  `.mp3`, `.wav`. The track's key is its path inside `music/` without extension; when two files
  share a key the one found first in the scan wins (search paths in priority order). Music is read from real files only, never from packs.
- **[read]** `ogg info`: when something is playing, prints the media library's own description of
  the open file (container, duration, bit rate, streams) through the console; otherwise
  `Playback stopped.`. The description's exact wording is the media library's, not the game's.
- **[read]** `ogg play <track> [soft]`: fewer than two words after `ogg`: `Usage: ogg play
  <track>`. Sound off: `Sound system not started.`. During a cinematic: `Can't play music in
  cinematic mode.`. A number is a CD track: it is looked up as `track<NN>` (two digits) and then
  `<NN>`; for the mission packs (game folder `rogue`: tracks 2..11 become 12..21; `xatrix`:
  2..11 become 9, 13, 14, 7, 16, 2, 15, 3, 4, 18) when `ogg_remap_tracks` is 1 (default). A
  name is looked up by path with any known extension removed. Unknown: `No such track: <arg>`.
  The current track is stopped first (with `soft`, closed without dropping the sound already
  buffered, so it ends smoothly). A track started by hand is not looped and not replaced by the
  server's track; when it ends, automatic music resumes. Open failures print at error level,
  for example `Couldn't open <path>: <reason>`, `Couldn't find audio stream`.
- **[read]** `ogg stop`: stops music, no line.
- **[read]** `ogg next`: same refusals as play (`Sound system not started.`, `Can't play music in
  cinematic mode.`). It cancels a manual track and restarts automatic music: with
  `ogg_shuffle` 1 the next track of the shuffled list of all tracks; otherwise the level's track
  again from the start (or the menu track `ogg_menu_track` when not connected). If music is
  disabled (`ogg_enable` 0) or the level's track is empty or "0", nothing plays.
- **[read]** `ogg pause [0|1]` and `ogg toggle`: nothing playing: `Playback stopped.`. With a
  number, sets paused to that; without, flips it. No line.
- **[read]** Anything else (or nothing): `Usage: ogg <info|play|stop|next|pause>`.
- **[read]** Automatic music loops the level's track (rewinding at its end) unless shuffle is on,
  in which case the next shuffled track plays. Settings: `ogg_enable` 1, `ogg_volume` 1 (clamped
  0..1), `ogg_shuffle` 0, `ogg_menu_track` 0, `ogg_remap_tracks` 1.

### 20. `pushmenu`, `popmenu`, `forcemenuoff`

- **[read]** `pushmenu <name>`: names are the menu script's page names plus the built-in pages
  `players`, `servers` and `demos` (tab completion lists them all). No argument: `Usage:
  pushmenu <menu>`. Unknown: `No such menu: <name>`.
- **[read]** If the page is already somewhere in the open stack, the pages above it are closed and
  it becomes the top again (no second copy). Otherwise it is pushed on top (up to a fixed depth,
  past it an error line saying the depth was exceeded is printed and nothing opens). Opening any
  page closes the console. When no menu was open before, the mouse is moved to the page's
  top-left and the open sound plays once the page has drawn.
- **[read]** A page may refuse to open (a built-in page whose data is missing, for example the
  player setup page with no player models); then the stack is left as it was.
- **[read]** `popmenu`: closes the top page; closing the last page closes the menu altogether.
  Does nothing when no menu is open. `forcemenuoff`: closes every page at once. Neither prints.

### 21. Screenshots

- **[read]** Settings and defaults: `gl_screenshot_format` "jpg" (when JPEG support is built in,
  otherwise "png"), `gl_screenshot_quality` 90 (JPEG quality, clamped 0..100 when used),
  `gl_screenshot_compression` 6 (PNG compression level, clamped 0..9 when used),
  `gl_screenshot_async` 1, `gl_screenshot_template` "quakeXXX".
- **[read]** `gl_screenshot_async`: 0 writes every format on the main thread; 1 writes PNG in the
  background; 2 or more writes PNG and JPEG in the background. TGA is always written at once.
- **[read]** `screenshot [format]`: no name can be given. The format word (or the setting when no
  word is given) is chosen by its first letter only: `j...` JPEG, `p...` PNG, anything else TGA.
  More than one argument: `Usage: screenshot [format]`.
- **[read]** `screenshotjpg [name] [quality]`, `screenshotpng [name] [compression]`,
  `screenshottga [name]`: with a name, the file is `screenshots/<name>.<ext>` (the extension is
  always appended, even if the name already has one; the name is normalised, cannot leave the
  folder, and unsafe characters become underscores; sub-folders are created). Too many
  arguments: `Usage: screenshotjpg [name] [quality]`, `Usage: screenshotpng [name]
  [compression]`, `Usage: screenshottga [name]`.
- **[read]** Without a name the template is used: its trailing run of `X` characters is the
  number field; the run must be 3 to 9 long. The prefix (everything before the X's, which may
  include folders) is followed by the lowest unused number, zero-padded to the width, from 0.
  Default: `screenshots/quake000.jpg`, `quake001.jpg`, ... up to `quake999`. Files are created
  exclusively, so an existing file is never overwritten. A bad template prints the warning
  `Bad value '<value>' for 'gl_screenshot_template'. Falling back to 'quakeXXX'.`, resets the
  setting and uses "quake" with 3 digits.
- **[read]** Folder: `screenshots/` under the current game's write directory.
- **[read]** Lines: success `Wrote <full path>` (the full path on disk, no full stop). All numbers
  used: `Couldn't create screenshot: Out of slots` (error level). Other creation errors:
  `Couldn't create screenshot: <reason>`. A failure while reading the pixels or writing:
  `Couldn't write <full path>: <reason>` (error level), and the partial file is deleted.
- **[read]** When TGA support is not built in and neither JPEG nor PNG was chosen: `Can't take
  screenshot, TGA format not available.`.

### 22. Restart commands

- **[read]** `vid_restart`: unless `cl_allow_vid_restart` is 1 (default 0) or the argument is
  `force`, it is refused. Sent by a server (stuffed text) it is ignored silently. Typed, it prints
  `Manual `vid_restart' command ignored.` (a backquote before vid_restart and a straight quote
  after) and, the first time in a session only, the line `Video settings are automatically
  applied by Q2PRO, thus manual `vid_restart' is never needed. To force restart of video
  subsystem, use `vid_restart force', or set `cl_allow_vid_restart' variable to 1 to restore old
  behavior of this command.` (one line, same quote style). When it runs it restarts input and the
  whole renderer, reloads the level's media, and prints whatever the renderer start-up prints:
  a banner `------- R_Init -------`, the video details, and a closing rule of 22 dashes.
- **[read]** `r_reload`: always runs; restarts the renderer without recreating the window, and the
  menus; same banners.
- **[read]** `snd_restart`: stops sound and starts it again, then reloads the level's sounds.
  Prints `------- S_Init -------`, the back end's lines, then a rule of 22 dashes. With
  `s_enable` 0: `Sound initialization disabled.`. On failure: `Sound failed to initialize.`
  (error level).
- **[read]** `in_restart`: restarts input devices; prints only what the input start-up prints.
- **[read]** `fs_restart`: full file-system restart: `----- FS_Restart -----`, the search path
  listing, then a rule of 22 dashes; then the client reloads its media.

### 23. `download <file>`

- **[read]** Checks in order: not connected: `Must be connected to a server.`; downloads
  permanently off (`allow_download` -1): `Downloads are permanently disabled.`; not exactly one
  argument: `Usage: download <filename>`.
- **[read]** Then the path: longer than 63 characters fails ("File name too long" from the system's
  error text); after normalising it must be a valid path, start and end with a path character,
  contain no `..`, contain at least one `/` (so a file in the game root is refused), and have one
  of the extensions bsp, dm2, ent, jpg, loc, md2, md3, ogg, pcx, png, sp2, tga, txt, wal, wav
  (any case). Otherwise: `Couldn't download <file>: Invalid quake path`.
- **[read]** If the file already exists (in any pack or folder): `Couldn't download <file>: File
  exists` (the system's wording for "exists"). A mixed-case path is lowered. Paths under
  `save/`, `music/` or `video/`, or matching the player's download ignore list, are refused:
  `Couldn't download <file>: Operation not permitted` (system wording). Already queued: refused
  with the "exists" reason. Otherwise it is queued over HTTP when the server offers it, else by
  the classic in-game transfer, and started. The `<reason>` texts for system errors are the
  operating system's own wording.

### 24. `pause`

- **[read]** `pause` toggles a manual pause: it sets `cl_paused` to 2, or back to 0 if it was 2.
  There is no printed line.
- **[read]** Single player (a local server with only this client, not a timedemo): the server
  stops running frames while `cl_paused` is non-zero (`sv_paused` becomes 1). The picture `pause`
  is drawn in the centre of the screen when `scr_showpause` is 1 (default).
- **[read]** Demo playback: the demo stops advancing while paused (not during a timedemo). With the
  demo bar on (`scr_demobar` 1) and `scr_showpause` 2, the bar shows `[PAUSED]` instead of the
  centre picture.
- **[read]** On a remote server the setting changes but nothing pauses (no line). Opening the
  console or a menu in single player pauses automatically (`cl_autopause`, default 1) as
  `cl_paused` 1, and closing it resumes only that automatic pause, never a manual one. Leaving the
  level clears both pauses.

### 25. Completion after `use`, `drop`, `give`

- **[read]** Only while connected and not playing a demo, and only for the first argument. The
  names are the item names the server sent for the level (in item order), exactly as the
  server spells them (for example "Rocket Launcher").
- **[read]** Matching ignores case. Duplicate names are not removed.
- **[read]** A single completed name is inserted WITHOUT quotes even when it contains spaces
  (`use Rocket Launcher` works because the server joins the arguments). When several names match,
  the common prefix is inserted, with an opening quote added if any of the candidates contains a
  space, `$` or `;`.
- **[read]** Elsewhere (for names from other lists) a single completion containing a space, `$`,
  `;` or another non-printing character is wrapped in double quotes.

### 26. `sky`

- **[read]** `sky <basename> [rotate] [axis x y z]`. No argument: `Usage: sky <basename> [rotate]
  [axis x y z]`. Not in a level: `No map loaded.`. An empty name (`sky ""`) restores the sky the
  server set.
- **[read]** `rotate` is degrees per second, default 0 (no rotation). The axis is used only when
  all three numbers are given (exactly five arguments after `sky`); otherwise it is 0 0 1 (the
  vertical). The six faces loaded are `env/<name><face>.tga` for the faces `rt`, `lf`, `bk`,
  `ft`, `up`, `dn` (where cube-map skies are in use, `sky/<name>.tga` is tried first); if any
  face is missing the sky is unset (drawn as the default). An axis of near-zero length, or a
  rotate of 0, means no rotation; otherwise the rotation runs continuously with time. With
  `gl_drawsky` 0 no sky is drawn at all. No line is printed.

### 27. `gun_next`, `gun_prev`, `gun_model`

- **[read]** `gun_next` adds one to the debug gun frame and prints `frame <n>`. `gun_prev`
  subtracts one, not below 0, and prints `frame <n>`. The frame starts at 0 (0 means "use the
  real frame").
- **[read]** `gun_model <name>` sets the debug view weapon to `models/<name>/tris.md2`; with no
  argument (or more than one) it clears the override. Nothing is printed.

### 28. Macro and time formats

- **[read]** `com_date_format` default `%Y-%m-%d`; `com_time_format` default `%H:%M` (on Windows
  `%H.%M`). They are the C library's date-format letters, applied to local time. `$com_date` and
  `$com_time` show them; an invalid or empty result shows as empty.
- **[read]** `$com_uptime` (time since the program started): under an hour `MM.SS` (minutes
  zero-padded to 2, a full stop, seconds zero-padded to 2, e.g. `07.05`); under a day
  `H:MM.SS` (e.g. `2:07.05`); otherwise `D+H:MM.SS` (e.g. `1+2:07.05`).
- **[read]** `$com_uptime_long`: comma-separated parts, each only when non-zero: `<n> day` /
  `days`, `<n> hour` / `hours`, `<n> min` / `mins`, `<n> sec` / `secs`, joined by `, `. For
  example `1 day, 2 hours, 5 secs`. Exactly zero is `0 secs`.
- **[read]** `$cl_timer` (the level's time): `M:SS` under an hour (`4:05`, minutes not padded),
  else `H:M:SS` (minutes NOT padded: `1:4:05`).
- **[read]** `$cl_demopos`: `M:SS.F` where F is the tenth of a second (the frame within the
  second at 10 frames per second), for example `3:07.4`. Not playing anything: `0:00.0`.
- **[read]** `$cl_lag`: dropped packets over received packets as a percentage with two decimals
  and a percent sign, for example `1.25%`; `0.00%` when nothing was received.
- **[read]** `$cl_surface`: traces 8192 units straight ahead from the eye against solids and water,
  shows the hit surface's texture name, a space, and its surface flags in hexadecimal with a
  `0x` prefix (for example `e1u1/floor1_3 0x1`; zero flags show as `0`). Not in a level: empty.
- **[read]** Other macros: `$cl_ups` speed rounded to a whole number; `$cl_fps`, `$r_fps`,
  `$cl_mps`, `$cl_pps`, `$cl_ping`, `$cl_health`, `$cl_ammo`, `$cl_armor`,
  `$cl_numentities` whole numbers; `$cl_mapname`, `$cl_server`, `$cl_weaponmodel` text,
  `$random` one digit 0..9; `$com_maplist` every map name under `maps/` without extension,
  space-separated.
- **[derived]** Not in a level the level state is cleared, so: `$cl_mapname` empty, `$cl_timer`
  `0:00`, `$cl_health`, `$cl_ammo`, `$cl_armor`, `$cl_numentities` `0`, `$cl_ups` `0`,
  `$cl_weaponmodel` empty, `$cl_surface` empty. Whether `$cl_lag` keeps the last connection's
  figure after a disconnect is not settled by what was read. `$cl_server` keeps the last server's name until a new connect.

### 29. Macro expansion rules

- **[read]** Every command line run through the command buffer (typed in the console, in a cfg
  file, from a bind, from an alias, stuffed by the server) is expanded just before it runs:
  `$name` is replaced by a macro's value, else a setting's value (private settings are not
  expanded), else the words `qt` (a double quote) and `sc` (a semicolon), else nothing (an
  unknown name becomes empty).
- **[read]** Name forms: `$name` (ends at whitespace or at a following `$`, so `$a$b` works),
  `${name}` (braces; `${$name}` also accepted). Spaces between the `$` and the name are skipped.
  A lone `$` at the end of a line stays.
- **[read]** Escape: `$$` gives a literal `$`.
- **[read]** Nothing between double quotes is expanded. So `say "$cl_health"` sends the text
  literally, while `say $cl_health` sends the number.
- **[read]** Typed console chat (a line without a leading slash in a level, when `con_auto_chat`
  sends it as chat) goes through the buffer and IS expanded. The chat line opened by
  `messagemode` / `messagemode2` is sent straight to the server quoted and is NOT expanded.
- **[read]** Binds: the command is stored as written; it is expanded each time the key runs it.
  Writing `bind x "say $loc_here"` stores the macro (quotes stop expansion at bind time), so it
  expands at press time. Values that expand to a value containing `$` are expanded again
  (settings), up to 100 expansions per line; then `Macro expansion loop, discarded.` and the
  line is dropped. Other limits: `Line exceeded <n> chars, discarded.`, `Expanded line exceeded
  <n> chars, discarded.` (n is 1024), and an odd number of quotes: `Line has unmatched quote,
  discarded.`.
- **[read]** Inside an alias's text, `$1`, `$2`... are the alias's arguments, `$@` all of them,
  `$2-` from the second on, `$2-4` a range.

## B. Keys

### 30. The console key

- **[read]** Two key codes are fixed console keys that cannot be unbound: the grave/backquote key
  (`` ` ``) and the tilde key code (`~`, which some keyboard layouts report directly). Either one
  opens or closes the console, but only when SHIFT is NOT held, and only on the first press (key
  repeat does not flick it).
- **[read]** With SHIFT held the key is not intercepted: it goes on as an ordinary key, so in the
  console SHIFT+`` ` `` types `~` (SHIFT maps `` ` `` to `~`), and in the game it runs whatever
  the key is bound to.
- **[read]** The release of the console key is swallowed too.

### 31. Alt+Enter

- **[read]** ALT held plus ENTER (the main Enter key, not the keypad Enter) toggles full screen, on
  the first press only, in every key mode (game, console, menu, chat). Nothing else runs for
  that press. No message is printed by the toggle itself.
- **[read]** Going to full screen sets `vid_fullscreen` to the remembered full-screen mode
  `_vid_fullscreen` (archived, default 1; if it was 0 it is first set to 1). Going back sets
  `vid_fullscreen` 0. The change is applied by the video setting's own handling (no
  `vid_restart` needed).

### 32. ESCAPE with the inventory, help computer or a layout up

- **[read]** ESCAPE is fixed (cannot be unbound) and only its presses count (releases are
  ignored).
- **[read]** When in the game (no console, menu or chat open), not playing a demo, and the server
  shows a layout, the inventory or the help computer: the first press sends `putaway` to the
  server; the SECOND press event while the key is still held, which is the first key-repeat event
  from the operating system's auto-repeat, opens the in-game menu (the `game` page, or `main`
  when there is no `game` page). Further repeats do nothing. So "held" means held long enough
  for the system's key repeat to fire once.
- **[read]** Otherwise repeats are ignored, and one press: in the console, opens the main menu if
  not in a level (and no menu is open) or else closes the console; in a menu, goes back one page
  (handled by the menu); in the chat line, cancels chat; in a level, opens the in-game page; not
  in a level, opens the main page.

### 33. Function keys inside a menu

- **[read]** While a menu is open, F1 through F12 (all twelve, no exceptions) run their key
  bindings exactly as in the game (a `+` binding gets its release too), and are not passed to the
  menu. No other key runs bindings in a menu (ESCAPE and the console key are handled before this
  rule, see 30 and 32).
- **[read]** In the console the rule is the reverse: every key that the console does not use runs
  its binding. The console uses: all printable ASCII keys, BACKSPACE, TAB, ENTER, the four
  arrows, ALT, CTRL, SHIFT (either side), INS, DEL, PGUP, PGDN, HOME, END, every keypad key,
  the middle mouse button and the mouse wheel. Everything else (F-keys, mouse buttons 1, 2, 4,
  5, joystick buttons, PAUSE and so on) runs its binding while the console is open.
- **[read]** Releases of keys whose `+` binding ran are always delivered, in any mode, so an
  action never sticks.

### 34. SHIFT and F1 during demo playback

- **[read]** While a demo plays and the game view has the keys (no console or menu): holding SHIFT
  makes the view follow the player's own mouse look instead of the recorded view angles
  (free look; the demo's position is still used). The mouse is grabbed for this when SHIFT is
  first pressed, even in a window, when `in_grab` is 2 or more (with `in_grab` 1, the default,
  the mouse stays grabbed in the game anyway).
- **[read]** Holding F1 while a demo plays draws the recorded layout (for example the scoreboard
  or the help computer) even when the recorded frame says no layout is shown. With
  `scr_draw2d` 3 (the TV setting that hides layouts), holding F1 shows the layout in any game.
  F1 still runs its binding too.

### 35. The keypad in the console

- **[read]** With neither CTRL nor ALT held, the keypad types characters whatever the Num Lock
  state the system reports as the keypad key: `/` slash, `*` star, `-` minus, `+` plus, HOME
  `7`, UP `8`, PGUP `9`, LEFT `4`, 5 `5`, RIGHT `6`, END `1`, DOWN `2`, PGDN `3`, INS `0`,
  DEL `.`. SHIFT applies on top of that as on the main keys ([derived]: SHIFT with keypad 7 types
  `&`). The keypad ENTER runs the line like ENTER. These keypad keys do not act as cursor
  keys in the console (the keypad arrows do not move through history).
- **[read]** The same character mapping is used for menu text fields and the chat line. In menu
  lists the keypad arrows, HOME, END, PGUP and PGDN do move (they act as navigation there).

## C. Menus

### How every scripted page behaves (applies to all of 36)

- **[read]** The scripted pages come from a text file `q2pro.menu` in the game folder, read when
  the menus start (at client start, and again on `r_reload` / `vid_restart`). Without the file
  there are no scripted pages at all; the three built-in pages (`players`, `servers`, `demos`)
  always exist and cannot be replaced. The file shipped with the release defines the pages below.
- **[read]** Global look in the shipped file: page background opaque black; menu font the console
  charset (`conchars`); mouse cursor picture `ch1` (drawn only in full screen); the player
  figure's weapon `w_railgun.md2`; row highlight colours normal `#0f90eb64`, active
  `#0f90ff64`, selection `#0f90eb64` (light blue at about 39% opacity), disabled grey
  `#7f7f7f`.
- **[read]** Layout: rows are 10 units apart (8-unit characters plus a quarter), stacked and
  centred vertically on the screen; a banner picture, when present, sits centred above the first
  row and adds 1.25 times its height. Labels are right-aligned ending 16 units left of the page's
  centre line, in the alternate text colour; values start 16 units right of it. A plain action
  row is centred on the line; an "aligned" action row is drawn left-aligned starting 16 units
  right of the centre line, in the alternate colour. A title (words) is drawn centred in the
  alternate colour on the top line of the page. Pages are drawn at `ui_scale` (0 = automatic).
- **[read]** The selected row is marked by a blinking glyph 13 (the charset's small arrow) that
  blinks on and off every 256 ms, placed 8 units left of the centre line (for centred actions,
  one character left of the label, and the centred label turns to the alternate colour).
- **[read]** Keys: up / down (also keypad arrows, TAB for down, `k` / `j`) move between selectable
  rows, wrapping round, skipping blanks and greyed rows. Left / right (also keypad arrows, the
  mouse wheel down / up, `h` / `l`) change a value. ENTER (or keypad ENTER, or a click on the
  selected row with mouse 1 or 3) activates. ESCAPE or mouse 2 goes back one page.
- **[read]** Kinds of row:
  - **Action**: runs its console command when activated.
  - **Toggle**: shows `no` / `yes`. On a plain setting, `yes` writes 1 and `no` writes 0; a
      setting holding anything other than 0 or 1 shows `???` and is left alone. A toggle can be
      inverted (`yes` writes 0) and can drive one bit of a number (bit n, value 2 to the power n),
      possibly inverted; a bit toggle reads and writes only that bit.
  - **Spin with listed choices ("values")**: the setting holds the index of the choice, from 0.
      An index outside the list shows `???`.
  - **Spin of strings**: the setting holds the chosen word itself (matched ignoring case when
      the page opens; no match shows `???`).
  - **Spin of pairs**: each choice has a shown label and a written value; the setting's text is
      matched (ignoring case) against the written values when the page opens; no match shows
      `???`.
  - **Slider ("range")**: drawn as a bar of 10 segments with end caps (glyphs 128 to 131) and a
      thumb; left / right move by the step, HOME / END jump to the first / last end, a click left
      or right of the thumb moves one step, dragging the thumb snaps to whole steps. With no step
      given the step is one tenth of the range. The range may run backwards (first end larger than
      the last); values are kept inside the two ends.
  - **Text field / number field**: an edit box `width` characters wide that also holds at most
      `width` characters; a number field accepts only digits, `+`, `-` and `.` (anything else
      beeps). A field with no label is centred.
  - **Blank**: an empty, unselectable line.
- **[read]** WHEN VALUES ARE WRITTEN: every toggle, spin and field on a page writes its value to
  its setting when the page is closed (going back, or `forcemenuoff`, or leaving the menu), not
  as it is changed. A slider writes only if it was moved. A spin or toggle showing `???` writes
  nothing. Writes are made as "from the menu": refused settings print their usual line, and the
  game folder (`game`) cannot be changed from a menu (`Changing game from menu is not
  allowed.`).
- **[read]** Status (help) lines: when the selected row has a help text it is shown at the bottom
  of the page, centred, on a grey band, word-wrapped to the screen width, at most 8 lines. In the
  shipped file only the key-binding rows and two start-server fields have one (quoted below).

### 36. The pages, row by row

Page names below are the names `pushmenu` takes. "Banner" means a picture drawn above the rows,
"Title" means words on the top line.

**`main`** — no title or banner; a plaque and a logo (see 37). Five picture rows:
1. picture `m_main_game` — opens `singleplayer`.
2. picture `m_main_multiplayer` — opens `multiplayer`.
3. picture `m_main_options` — opens `options`.
4. picture `m_main_video` — opens `video`.
5. picture `m_main_quit` — runs `quit`.

**`singleplayer`** (the "game" page of the main menu) — banner `m_banner_game`.
1. action `easy` — sets `skill` 0, `deathmatch` 0, `coop` 0, `maxclients` 1, then runs
   `newgame`.
2. action `medium` — the same with `skill` 1.
3. action `hard` — the same with `skill` 2.
4. action `nightmare` — the same with `skill` 3.
5. blank.
6. action `load game...` — opens `loadgame`.
7. action `save game...` — opens `savegame`.

**`loadgame`** — banner `m_banner_load_game`. Rows: load slot `save0`, a blank, then load slots
`save1` through `save15` (17 rows in all, 16 slots). See 38.

**`savegame`** — banner `m_banner_save_game`. Save slots `save1` through `save15` (15 rows; the
level-start slot `save0` cannot be saved to). See 38.

**`multiplayer`** — banner `m_banner_multiplayer`.
1. action `browse q2servers.com` — opens the server browser on the web list
   `http://q2servers.com/?raw=2` (the source is marked as a binary list: addresses as 6-byte
   records rather than lines of text). Without HTTP support built in it prints `Can't fetch
   '<url>', no HTTP support compiled in.`.
2. action `browse address book` — opens the server browser on three sources: the address book
   (`adr0`..`adr15`), a `servers.lst` file found in the game's search path (one address per line), and the LAN
   broadcast.
3. action `browse demos` — opens `demos`.
4. action `start server` — opens `startserver`.

**`startserver`** — banner `m_banner_start_server` (see 39).
1. spin of strings `initial map`, setting `_ui_nextserver`, choices: every map name.
2. spin `rules`, setting `coop`: `deathmatch` writes 0, `cooperative` writes 1.
3. number field `time limit`, setting `timelimit`, 3 characters, help line `0 - no time limit`.
4. number field `frag limit`, setting `fraglimit`, 3 characters, help line `0 - no frag limit`.
5. number field `max players`, setting `maxclients`, 3 characters.
6. text field `host name`, setting `hostname`, 16 characters.
7. blank.
8. aligned action `deathmatch flags...` — opens `gameflags`.
9. aligned action `begin game!` — see 39.

**`gameflags`** (deathmatch flags; there is no page named `dmflags`) — title `Deathmatch Flags`.
Every row is a yes/no toggle of one bit of `dmflags`:
1. `falling damage` — bit 3 (value 8), inverted (yes = bit clear).
2. `weapons stay` — bit 2 (value 4).
3. `instant powerups` — bit 4 (value 16).
4. `allow powerups` — bit 1 (value 2), inverted.
5. `allow health` — bit 0 (value 1), inverted.
6. `allow armor` — bit 11 (value 2048), inverted.
7. `spawn farthest` — bit 9 (value 512).
8. `same map` — bit 5 (value 32).
9. `force respawn` — bit 10 (value 1024).
10. `infinite ammo` — bit 13 (value 8192).
11. `fixed FOV` — bit 15 (value 32768).
12. `quad drop` — bit 14 (value 16384).

**`options`** — banner `m_banner_options`. Ten actions, each opening a page:
`player setup` (`players`), `input setup` (`input`), `key bindings` (`keys`),
`weapon bindings` (`weapons`), `video setup` (`video`), `sound setup` (`sound`),
`effects setup` (`effects`), `screen setup` (`screen`), `download options` (`downloads`),
`address book` (`addressbook`).

**`players`** (built in) — see 40.

**`input`** — title `Input Setup`.
1. number field `mouse sens`, setting `sensitivity`, 8 characters.
2. toggle `auto sens`, `m_autosens`.
3. toggle `mouse filter`, `m_filter`.
4. toggle `free look`, `freelook`.
5. toggle `always run`, `cl_run`.

**`keys`** — title `Key Bindings`. Key-binding rows (see 41), label and the command bound:
1. `attack` binds `+attack`.
2. `next weapon` binds `weapnext`.
3. `previous weapon` binds `weapprev`.
4. `walk forward` binds `+forward`.
5. `backpedal` binds `+back`.
6. `run` binds `+speed`.
7. `step left` binds `+moveleft`.
8. `step right` binds `+moveright`.
9. `up / jump` binds `+moveup`.
10. `down / crouch` binds `+movedown`.
11. `inventory` binds `inven`.
12. `use item` binds `invuse`.
13. `drop item` binds `invdrop`.
14. `prev item` binds `invprev`.
15. `next item` binds `invnext`.
16. `help computer` binds `help`.
17. `pause game` binds `pause`.
18. `scoreboard` binds `score`.
19. `chat` binds `messagemode`.
20. blank.
21. aligned action `legacy keys...` opens `legacykeys`.

**`legacykeys`** (the "legacy" page) — title `Legacy Key Bindings`. Key-binding rows:
1. `turn left` binds `+left`.
2. `turn right` binds `+right`.
3. `sidestep` binds `+strafe`.
4. `look up` binds `+lookup`.
5. `look down` binds `+lookdown`.
6. `center view` binds `centerview`.
7. `mouse look` binds `+mlook`.
8. `keyboard look` binds `+klook`.

**`weapons`** — title `Weapon Bindings`. Key-binding rows, each binding `use` followed by
the item's name as the game spells it:
1. `blaster` binds "use Blaster".
2. `shotgun` binds "use Shotgun".
3. `super shotgun` binds "use Super Shotgun".
4. `machinegun` binds "use Machinegun".
5. `chaingun` binds "use Chaingun".
6. `grenade launcher` binds "use Grenade Launcher".
7. `rocket launcher` binds "use Rocket Launcher".
8. `hyperblaster` binds "use HyperBlaster".
9. `railgun` binds "use Railgun".
10. `bfg10k` binds "use BFG10K".
11. `grenades` binds "use Grenades".

**`video`** — banner `m_banner_video` (no words title).
1. spin `video mode`, `vid_fullscreen`: `windowed` writes 0; then one choice per entry of the
   video driver's mode list (`vid_modelist`), writing 1, 2, 3... in list order.
2. spin `fov scaling`, `cl_adjustfov`: `vert-` 0, `hor+` 1.
3. toggle `vertical sync`, `gl_swapinterval`.
4. pairs `anti-aliasing`, `gl_multisamples`: `no` 0, `2x MSAA` 2, `4x MSAA` 4, `8x MSAA` 8.
5. slider `texture gamma`, `vid_gamma`, from 1.3 down to 0.3, step 0.1 (moving right lowers the
   number).
6. toggle `hardware gamma`, `vid_hwgamma`.
7. slider `texture quality`, `gl_picmip`, from 3 down to 0, step 1 (moving right lowers the
   number, i.e. better quality).
8. pairs `texture filter`, `gl_texturemode`: `nearest` GL_NEAREST, `linear` GL_LINEAR,
   `bilinear` GL_LINEAR_MIPMAP_NEAREST, `trilinear` GL_LINEAR_MIPMAP_LINEAR.
9. pairs `anisotropic filter`, `gl_anisotropy`: `no` 1, `2x` 2, `4x` 4, `8x` 8, `16x` 16.
10. slider `texture saturation`, `gl_saturation`, 0 to 1, step 0.1.
11. pairs `texture intensity`, `intensity`: `1x` 1, `2x` 2, `3x` 3.
12. slider `lightmap saturation`, `gl_coloredlightmaps`, 0 to 1, step 0.1.
13. slider `lightmap brightness`, `gl_brightness`, 0 to 0.3, step 0.03.
14. spin `rendering backend`, `gl_shaders`: `legacy` 0, `shaders` 1.

**`sound`** — title `Sound Setup`.
1. spin `sound engine`, `s_enable`: `no sound` 0, `software` 1, `OpenAL` 2.
2. slider `sound latency`, `s_mixahead`, 0.05 to 0.2, step 0.015.
3. slider `sound volume`, `s_volume`, 0 to 1, step 0.1.
4. slider `music volume`, `ogg_volume`, 0 to 1, step 0.1.
5. toggle `enable music`, `ogg_enable`.
6. toggle `shuffle tracks`, `ogg_shuffle`.
7. toggle `underwater effect`, `s_underwater`.
8. spin `ambient sounds`, `s_ambient`: `no` 0, `yes` 1, `only player's own` 2.
9. spin `chat beep`, `cl_chat_sound`: `disabled` 0, `default` 1, `alternative` 2.

**`effects`** — title `Effects Setup`.
1. spin `dynamic lighting`, `gl_dynamic`: `no` 0, `yes` 1, `only switchable` 2.
2. spin `entity cel-shading`, `gl_celshading`: `no` 0, `1x` 1, `2x` 2, `3x` 3.
3. toggle `entity glowing`, `cl_noglow`, inverted (yes writes 0).
4. spin `ground shadows`, `gl_shadows`: `no` 0, `yes` 1, `fade` 2.
5. toggle `screen blending`, `gl_polyblend`.
6. toggle `screen warping`, `gl_waterwarp`.
7. toggle `grenade explosions`, bit 0 (value 1) of `cl_disable_explosions`, inverted.
8. toggle `rocket explosions`, bit 1 (value 2) of `cl_disable_explosions`, inverted.
9. blank.
10. aligned action `railgun trail setup...` — opens `railtrail`.

**`railtrail`** — title `Railgun Trail Setup`.
1. spin `rail trail type`, `cl_railtrail_type`: `default` 0, `core only` 1, `core and spiral` 2.
2. slider `rail trail duration`, `cl_railtrail_time`, 0.1 to 3.0, step 0.1.
3. slider `core width`, `cl_railcore_width`, 1 to 6, step 1.
4. slider `spiral radius`, `cl_railspiral_radius`, 1 to 6, step 1.
5. spin of strings `core color`, `cl_railcore_color`: black, red, green, yellow, blue, cyan,
   magenta, white (the word is written).
6. spin of strings `spiral color`, `cl_railspiral_color`: the same eight words.

**`screen`** — title `Screen Setup`.
1. slider `screen size`, `viewsize`, 40 to 100, step 10.
2. toggle `ping graph`, `scr_lag_draw`.
3. spin `demo bar`, `scr_demobar`: `no` 0, `yes` 1, `verbose` 2.
4. slider `HUD opacity`, `scr_alpha`, 0 to 1, step 0.1.
5. slider `console opacity`, `con_alpha`, 0 to 1, step 0.1.
6. pairs `HUD scale`, `scr_scale`: `auto` 0, `1x` 1, `2x` 2, `4x` 4.
7. pairs `console scale`, `con_scale`: the same four.
8. pairs `menu scale`, `ui_scale`: the same four.
9. blank.
10. aligned action `crosshair setup...` — opens `crosshair`.

**`crosshair`** — title `Crosshair Setup`.
1. spin `crosshair type`, `crosshair`: `none` 0, `cross` 1, `dot` 2, `angle` 3.
2. pairs `crosshair scale`, `ch_scale`: `0.5x` 0.5, `1x` 1, `2x` 2.
3. toggle `color by health`, `ch_health`.
4. slider `red channel`, `ch_red`, 0 to 1, step 0.1.
5. slider `green channel`, `ch_green`, 0 to 1, step 0.1.
6. slider `blue channel`, `ch_blue`, 0 to 1, step 0.1.
7. slider `alpha channel`, `ch_alpha`, 0 to 1, step 0.1.

**`downloads`** — title `Download Options`.
1. toggle `allow downloading`, `allow_download` (shows `???` when it is -1, permanently off).
2. toggle `maps`, `allow_download_maps`.
3. toggle `player models/skins`, `allow_download_players`.
4. toggle `models`, `allow_download_models`.
5. toggle `sounds`, `allow_download_sounds`.
6. toggle `textures`, `allow_download_textures`.
7. toggle `pics`, `allow_download_pics`.
8. blank.
9. toggle `HTTP downloads`, `cl_http_downloads`.

**`addressbook`** — banner `m_banner_addressbook`; 16 unlabelled centred text fields, 32
characters each, editing `adr0` through `adr15` in order. See 43.

**`game`** (the in-game page) — see 42.

**`servers`** and **`demos`** (built in) — see 45.

- **[read]** The question's page names map as follows: "game" (single player) is
  `singleplayer`; "dmflags" is `gameflags`; "legacy" is `legacykeys`; "players / player setup"
  is the built-in `players`; the in-game page is `game`. There is no `crosshair` banner; pages
  with a title show words only.

### 37. The main page's pictures and cursor

- **[read]** Pictures: plaque `m_main_plaque`, logo `m_main_logo`; rows `m_main_game`,
  `m_main_multiplayer`, `m_main_options`, `m_main_video`, `m_main_quit`, each replaced by the
  same name with `_sel` appended while selected (`m_main_game_sel` and so on).
- **[read]** The animated cursor: pictures `m_cursor0` through `m_cursor14` (15 frames), one frame
  per 100 ms (10 frames a second, a 1.5 second loop, timed from the real clock), drawn 25 units
  left of the selected picture's left edge, level with its top.
- **[read]** Placement: the picture rows are stacked from the screen's vertical centre (the stack
  is centred), each taking 1.25 times its picture's height. Horizontally the block made of the
  widest row picture, a 32-unit cursor gap and the wider of plaque and logo is centred on the
  screen; the plaque and logo sit at the left of that block, right-aligned to 32 units left of
  the row pictures. Plaque and logo together (with 5 units between) are centred vertically: the
  plaque's top at the top of that pair, the logo's bottom at its bottom.

### 38. Load and save pages

- **[read]** A slot shows, for a used slot: the save's date then a space then the level's name
  (its long title as the level names itself, not the map file). The date is local time as
  `<Mon> <DD> <HH>:<MM>` (e.g. `Mar 05 14:22`) when saved this year, or `<Mon> <DD>  <YYYY>`
  (two spaces before the year) for another year; `???` if the date cannot be formatted.
- **[read]** Special slots: a level-start save (made automatically on entering a level, kept in
  `save0`) shows `ENTERING <level name>`; a save made by the `autosave` command (it writes to
  `save1`) shows `AUTOSAVE <level name>`. An empty or unreadable slot shows `<EMPTY>`.
- **[read]** On the load page an empty slot is greyed and cannot be selected. Selecting a used slot
  runs `load "<slot>"`.
- **[read]** On the save page every slot can be selected; selecting runs `save "<slot>"` and then
  closes the menu. There is no confirmation before saving over a used slot. The save command
  itself prints `Game saved.` on success, or refusals such as `You must be in a game to save.`,
  `Can't savegame in a deathmatch.`, `Can't savegame while dead!`.
- **[read]** Slot texts are read again each time the page is opened.

### 39. The start-server page

- **[read]** The map list is the names of every `.bsp` file under `maps/` in all search paths
  (packs included), without the extension, in alphabetical order with duplicates removed (the
  `$com_maplist` macro). **[derived]** The list is taken when the menu file is read (client
  start, `r_reload`, `vid_restart`), not when the page opens.
- **[read]** Initial values: each row shows the setting's current value. The map row shows the
  current `_ui_nextserver` if it is in the list, else `???` (on first use it is empty, so `???`).
  `rules` shows `deathmatch` or `cooperative` from `coop` (default 0). The fields show
  `timelimit` (default 0), `fraglimit` (default 0), `maxclients` (default 8) and `hostname`
  (default `noname`).
- **[read]** `begin game!` runs, in order: close all menus (which writes every row of the page,
  including the chosen map into `_ui_nextserver` and the rules into `coop`); then if `coop` is 1
  set `deathmatch` 0, otherwise set `deathmatch` 1; then `map <the chosen map> force` (the word
  `force` makes the server do a full restart even if a game is running, so latched settings
  such as `coop`, `deathmatch` and `maxclients` take effect).
- **[read]** Co-op therefore differs only by `coop` 1 and `deathmatch` 0; the deathmatch flags
  page still applies to `dmflags` either way.
- **[derived]** If no map was ever picked (the row shows `???`), `_ui_nextserver` stays empty
  and the command becomes `map force`, which asks for a map called "force".

### 40. Player setup (`players`)

- **[read]** Title: the picture `m_banner_plauer_setup` (the classic picture's own spelling) if
  it exists, else the words `Player Setup`.
- **[read]** Rows in order: text field `name` (15 characters, the longest player name), then a gap
  of one row, then spin `model`, spin `skin`, spin `handedness` (labelled `hand` when the menu
  is narrower than 640 units). Handedness choices `right`, `left`, `center` (0, 1, 2). The name
  row has focus first.
- **[read]** Model list: every folder under `players/` that holds `tris.md2` and at least one
  usable skin. A usable skin is a `.pcx` file in that folder, not itself an icon, for which an
  icon `<skin>_i.pcx` also exists (compared ignoring case). Models are ordered `male` first,
  `female` second, then the rest alphabetically ignoring case; at most 1024 models. Skins are in
  alphabetical file order, shown without extension.
- **[read]** If no model qualifies, the page does not open and prints `No player models found.`.
- **[read]** Starting choices: the model and skin from the `skin` setting (split at the first `/`
  or `\`; when it has neither, `male` and `grunt`); a model or skin not found falls back to the
  first in its list. Handedness from `hand`; outside 0..2 it shows `right`. Changing the model
  resets the skin to the model's first skin.
- **[read]** The figure: the chosen model with the chosen skin, plus the weapon
  `players/<model>/w_railgun.md2` when the model has it, drawn full-bright in a view on the
  right half of the screen (from the horizontal centre to the right edge, from one tenth of the
  height, 0.8 of the height tall, 90-degree horizontal field of view), 40 units in front of the
  eye, facing yaw 260 degrees. It does not turn. It plays frames 0 to 39 in a loop, one frame
  every 120 ms (about 8.3 frames a second, a 4.8 second loop), interpolated between frames.
- **[read]** On closing the page it writes, as if typed: `name` to the field's text, `skin` to
  `<model>/<skin>`, and `hand` to 0, 1 or 2.

### 41. Key-binding rows

- **[read]** Help line while a binding row is selected: `Press Enter to change, Backspace to
  clear`. After ENTER, while waiting for a key: `Press the desired key, Escape to cancel`
  (the other rows are drawn in the disabled colour meanwhile).
- **[read]** The right column shows the first key bound to the command (lowest key code), or
  `<key1> or <key2>` when two or more keys are bound (only the first two are shown), or `???`
  when none is. Key names are the console's key names. A command matches when a key's binding
  text equals it ignoring case.
- **[read]** While waiting: ESCAPE cancels (nothing changes). The console key (`` ` ``) cannot be
  bound: it beeps and the row keeps waiting. Any other key is bound to the command; if the
  command already had two or more keys, all of them are unbound first (so it ends with only the
  new key); with one key already bound, the new key is added beside it.
- **[read]** BACKSPACE or DEL on a selected row unbinds every key bound to that command.
- **[read]** A `use <item>` row binds the key to the text `use <Item Name>` exactly as written in
  the row (for example `use Rocket Launcher`), and a saved config writes it as
  `bind <key> "use Rocket Launcher"`.

### 42. The in-game page (`game`)

- **[read]** No title or banner. Background `#0000ff20`: pure blue at alpha 32 of 255 (about 12.5%
  opacity), so the game shows through. Compact style: the page's band is only as tall as its rows
  plus one row-gap above and below, centred vertically.
- **[read]** Rows (all centred actions): `browse q2servers.com` (as on `multiplayer`), `browse
  address book` (as on `multiplayer`), `browse demos` (opens `demos`), `single player` (opens
  `singleplayer`), `options` (opens `options`), `disconnect` (runs the console command
  `disconnect`), `quit` (runs `quit`).
- **[read]** `disconnect` leaves the server: it prints `Disconnected from server` at warning level
  (yellow), stops a local server, and closes the menus; the main page then opens only if
  `ui_open` is 1 (default 0). It is the same as typing `disconnect`.
- **[read]** ESCAPE in a level opens this page (or `main` if the file has no `game` page).

### 43. The address book (`addressbook`)

- **[read]** Sixteen text fields with no labels, centred, each 32 characters wide and holding at
  most 32 characters, editing the settings `adr0` to `adr15` in order. Each holds one server
  address (host name or IP, optional `:port`). Values are written when the page closes. The
  server browser's "address book" source and `pingservers` read these settings.

### 44. Menu sounds

- **[read]** `misc/menu1.wav`: a page opened from no menu (played once the page has drawn), and
  "enter" / activate (a choice taken, a key-binding wait started, a binding cleared).
- **[read]** `misc/menu2.wav`: moving between rows and changing a spin or toggle value.
- **[read]** `misc/menu3.wav`: going back a page (ESCAPE / mouse 2) and finishing a key-binding
  wait.
- **[read]** `misc/talk1.wav`: the error beep (a refused character in a field, a spin with no
  choices, the console key in a binding wait, an empty demo list).
- **[read]** Slider moves and typing in fields are silent.

### 45. Server browser and demo browser

- **[read]** Server browser (`servers`): title `Server Browser`. Main list columns `Hostname`,
  `Mod`, `Map`, `Players`, `RTT`. Players shows `<players>/<maxclients>`; Mod shows the server's
  game folder or `baseq2`; unknown values show `???` (and `?/?`); a server that did not answer
  shows `down` in place of the player count. When a server is selected and the screen is at least
  640 wide, a list of its settings with columns `Key` and `Value` appears, and, when it has
  players, a player list with columns `Frg`, `RTT`, `Name`.
- **[read]** Its bottom line, centred, is one of: `Press Enter to connect; Space to refresh` (a
  live server selected), `Press Space to refresh; Alt+Space to refresh all` (a dead or pending
  server selected), `Pinging servers; Press Backspace to abort`, `Select a server; Press
  Alt+Space to refresh`, `No servers found; Press Space to refresh`, and while lists load
  `Resolving servers, please wait...`. When the menu is at least 800 wide it also shows, at the
  right, `<n> player(s) on <m> server(s)` (each word singular for exactly 1, e.g.
  `1 player on 3 servers`), and at the left the selected server's address. While pinging, the
  grey band fills from the left in proportion to the servers pinged.
- **[read]** Demo browser (`demos`): title `Demo Browser`. Columns: the first column's header is
  the folder being browsed (starting at `/demos`), then `Date`, `Size`, `Map`, `POV`. Folders
  show `[DIR]` in the Size column and `-` in Date, Map and POV; multi-view demos show `[MVD]` as
  their point of view (both bracketed words in the alternate-colour glyphs); `..`
  goes up; unknown map or point of view shows `???`. Dates use the same format as save slots.
- **[read]** Its bottom line: `Building list...` while scanning, then `Press Enter to play demo`
  (a demo selected), `Press Enter to change directory` (a folder selected) or `No demos found`.
  When at least 640 wide, the right of the bottom line shows `<n> demo(s), <total size>`, for
  example `12 demos, 34 MB` (sizes: `<n> byte(s)`, `<n> kB`, `<n.n> MB` under 10 MB,
  `<n> MB`, `<n.n> GB`).

## D. Printed lines

### 46. The map title block on server data

- **[read]** When the server's data arrives for a level (not for a cinematic), the console gets: two
  empty lines; then one line of 36 bar glyphs: glyph 29 (the bar's left end), 34 of glyph 30 (the
  bar's middle) and glyph 31 (the right end), so the rule is 36 characters wide (288 units at 8
  units a character); then one empty line; then the level's name (the server's long level
  title) on its own line in the alternate text colour (the charset's high-bit glyph set, the
  "green" text). The rule itself is printed in the normal colour (its look comes from the
  charset's bar glyphs). The rule does not adapt to the console width.
- **[read]** For a cinematic level the block is not printed; the cinematic plays instead.

### 47. "Can't ..., not connected"

- **[read]** The exact line is `Can't "<command>", not connected` with the command word in
  straight double quotes, a comma, and no full stop.
- **[read]** It is printed in two cases:
  - A command registered only so that it completes, with no local action, typed while not fully
      in a level: `say`, `say_team`, `wave`, `inven`, `kill`, `use`, `drop`, `info`, `prog`,
      `give`, `god`, `notarget`, `noclip`, `invuse`, `invprev`, `invnext`, `invdrop`,
      `weapnext`, `weapprev`, and any name added with `complete`. (Names starting with `+` or `-`
      are never forwarded.) It is not printed on a dedicated server.
  - `cmd <text>` typed while not connected at all (connecting counts as not connected; once
      connected, even during loading, `cmd` sends). During demo playback `cmd` silently does
      nothing; `cmd` alone sends nothing.
- **[read]** An unknown word (not a command, alias or setting) typed while not in a level prints
  `Unknown command "<word>"` instead; in a level it is sent to the server.

### 48. Connecting lines

- **[read]** `Requesting challenge... <n>` and `Requesting connection... <n>`: `<n>` is the attempt
  count, starting at 1 and going up by 1 for each packet sent. A request is re-sent every 3
  seconds until answered; there is no limit on attempts (the client keeps trying until the player
  disconnects). The count is NOT reset when the challenge arrives: the connection requests carry
  on numbering from the challenge requests (e.g. `Requesting challenge... 1`, then
  `Requesting connection... 2`). Connecting to the player's own local server skips the
  challenge, so it starts at `Requesting connection... 1`. A `reconnect` or a server-ordered
  reconnect restarts the count at 1.
- **[read]** The first request goes out at most 1 second after `connect` (and at once if the
  previous attempt was long ago); the connection request after a challenge goes out at once.
- **[read]** `Connected to <address> (protocol <n>).` where `<address>` is the server's address as
  `ip:port` (the word `loopback` for the local server) and `<n>` the protocol number agreed (34 for
  the original protocol, 35 for the R1Q2 extension, 36 for the Q2PRO extension; 26 is the
  oldest accepted), with
  a full stop after the closing parenthesis.
- **[read]** Between them, a text answer from the server while connecting (for example a refusal)
  is printed as the server sent it, and the client goes back to requesting a challenge.

### 49. Disconnection lines

- **[read]** `Disconnected from server` (no full stop; from `disconnect`, or the in-game page),
  `Server disconnected` (no full stop; the server telling the client it is dropped, or a
  reconnect order met during demo playback) and
  `Server connection timed out.` (with a full stop) are all printed at WARNING level, which
  colours them yellow. Each is followed by the normal disconnect: the local server, if any, is
  shut down, sound and level state are cleared, the menu track starts, and the menus close (the
  main page opens only if `ui_open` is 1). Nothing else is printed by the client after the line.
- **[read]** A server's reconnect order during normal play prints `Server disconnected,
  reconnecting` (normal colour) and the client starts requesting a challenge again.
- **[read]** The time-out: `cl_timeout` seconds (default 120) without a packet, and then more than
  5 further client frames, before the line (never for the local server). With network error
  reporting, a connection refused for more than 5 seconds prints `Server connection was reset.`
  in the same way.
- **[read]** By contrast a drop error prints three lines in red: a row of 20 asterisks,
  `ERROR: <message>`, and another row of 20 asterisks.

### 50. `rcon`

- **[read]** No arguments: `Usage: rcon <command>`.
- **[read]** `rcon_password` empty: `You must set 'rcon_password' before issuing an rcon
  command.` (straight single quotes, one line).
- **[read]** Not connected and `rcon_address` empty: `You must either be connected, or set the
  'rcon_address' cvar to issue rcon commands.` (one line). With `rcon_address` set but not a
  valid address: `Bad address: <rcon_address>`.
- **[read]** The command text sent is the rest of the line as typed, with one pair of surrounding
  quotes removed. Replies are printed as they arrive (several packets possible) for 6 seconds.

### 51. `bind`, `unbind`

- **[read]** `bind` with no arguments: `bind <key> [command] : attach a command to a key`.
- **[read]** `unbind` with other than exactly one argument: `unbind <key> : remove commands from
  a key`.
- **[read]** An unknown key name (for either): `"<name>" isn't a valid key` (name in straight
  double quotes). A single character is always a valid key (taken as its lower-case letter),
  longer names are matched ignoring case against the key-name list.
- **[read]** `bind <key>` alone: `"<key>" = "<command>"` when bound, `"<key>" is not bound` when
  not (the key is shown as the player typed it).
- **[read]** `bind <key> <words...>`: the binding is all remaining words joined by single spaces.
  `unbindall` clears every key silently. `bindlist` prints `<key> "<command>"` per bound key in
  key-code order, with no count line.
