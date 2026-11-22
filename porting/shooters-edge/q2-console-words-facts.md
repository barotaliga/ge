# Answers: Quake II server (Q2PRO upstream, commit 601a8df): general console words, macros and shutdown

Reader's notes. Facts only, no code. **[read]** = read directly; **[derived]** = inferred from what was read.
"Console" below means the server's own output: the terminal (stdout) and, when logging is on, the log
file. Every line printed to the console is also passed through a filter before it reaches the terminal
and the log: the high bit of each byte is stripped, bytes 16 and 17 become `[` and `]`, every other
non-printable byte except space, carriage return and newline becomes `.`; the `console_prefix` (terminal)
and `logfile_prefix` (log) are put at the start of each line (see 21) **[read]**.

"Dedicated server" covers both the dedicated-only build and a full build started dedicated; where they
differ it is said.

---

## A. Shutdown

### 1. `quit [reason]` and `recycle [reason]`

**Forming the reason**
- The reason is the command's arguments joined with single spaces (each argument as tokenized: quotes
  removed, runs of whitespace collapsed to one space, `$` macros already expanded outside quotes). It is not
  the raw rest of the line. **[read]**
- The final text is `Server quit: <reason>` + newline, or `Server quit` + newline when there is no reason
  (or it is empty). `recycle` uses `restarted` in place of `quit`: `Server restarted: <reason>` + newline /
  `Server restarted` + newline. The whole text is capped at 1023 characters (silently truncated). **[read]**
- `recycle` exists only in builds that include the server (always true for a dedicated server). `quit`
  exists everywhere. **[read]**

**What each connected client is sent** (in this order) **[read]**
1. Nothing happens on the client side if no clients are in the client list.
2. One message buffer is built: a print message at the **high** print level carrying the shutdown text
   above (with its newline), followed by a **disconnect** message for `quit` or a **reconnect** message for
   `recycle`.
3. That buffer is sent **twice** to every client that is not a zombie (a slot that is already
   disconnecting is skipped). Before each send, any outstanding fragments of a large reliable message
   on that client's channel are flushed first. The buffer goes out immediately as the unreliable payload of
   a channel packet, not queued for the next frame. **[read]** (that it rides as the unreliable part:
   **[derived]**)
4. Then every client slot is freed silently: no "dropped"/"disconnected" line is printed on the console
   and the game module is not told about each client leaving. **[read]**

**Order of the whole shutdown** **[read]**
1. If the server is receiving relayed matches (the multiview client side), those connections and channels
   are torn down (unless the server is itself running the built-in relay game and this is its own respawn).
2. The anticheat link (only in builds with it) is closed and its state cleared.
3. **Local multiview recording (`mvdrecord`)**: stopped. An end-of-demo marker (a zero length word) is
   written, then the file is closed. **No console message** is printed (unlike `mvdstop`). **[read]**
4. **Multiview relays over TCP** (spectator relays connected to this server): each one that is fully
   connected (primed or spawned) is sent a final "reconnect" operation for `recycle`, or a "disconnect"
   operation for `quit` (and for every other shutdown kind), then dropped; relays in earlier states are
   simply dropped. The streams are pumped twice so the last bytes go out, then every relay is removed. No
   console line per relay. The TCP listening socket is then closed. **[read]**
5. Clients get their final message (above).
6. **Masters**: on a dedicated server whose `public` setting is non-zero, every configured master address
   is sent a connectionless packet whose text is exactly `shutdown` (the usual four 0xFF bytes, then
   `shutdown`, no newline). A developer-level console line `Sending shutdown to <address>` is printed
   for each (only visible with `developer` set, which itself exists only in debug builds). No heartbeat is
   sent. When `public` is 0 nothing is sent. The masters' acknowledgement times are reset in every case.
   **[read]**
7. The game module is shut down (the stock game prints `==== ShutdownGame ====` on the console) and
   unloaded; the setting `g_features` is set to `0`. **[read]**
8. The map is freed, server state cleared, `sv_running` and `sv_paused` set to `0`, the terminal title
   updated. **[read]**
9. Then (both words, since both go through the same quit routine): the client side shuts down (nothing on a
   dedicated server), the network is shut down (sockets closed, network commands removed), the terminal's
   input history is saved (see 21, `sys_history`), the console log is closed — if a log was open this
   prints `Closing console log.` + newline on the console — the file system is shut down (a warning per file
   handle still open), background work is finished. **[read]**
10. The process exits with the **success** exit code (0), after restoring the terminal. **[read]**

**Console output** for a plain `quit` with a game running is therefore just `==== ShutdownGame ====`
(the game's own line) and, if logging, `Closing console log.`. The shutdown text itself is **not**
printed on the server console. **[read]**

**Does `recycle` restart?** No. `recycle` and `quit` are the same routine except for the word in the text
and the reconnect (instead of disconnect) message to clients and relays. The process **exits with code 0**
in both cases; nothing restarts in place. Restarting the process is left to whatever supervises it (a
script or service manager); clients were told to reconnect so they come back when it is up again. **[read]**
(that the restart is external: **[derived]**)

**When issued over rcon**: the rcon reply buffer is never flushed, because the process exits during the
command; the rcon sender gets no reply text. **[derived]**

**Related shutdown texts an operator can hit** **[read]**
- `Server restarted` + newline, with a **reconnect** message: sent when a new game is started while one is
  already initialized — the `map` command when it decides on a full restart (game just starting, a pending
  `sv_recycle`, latched settings waiting, or `map <name> force`), the `gamemap` command with `sv_recycle`
  set to 1, and loading a saved game. The server carries on into the new map in place (same process,
  settings with latched values applied, `sv_recycle` reset to 0).
- With `sv_recycle` 2 or more, `gamemap` calls the quit routine with no reason and the reconnect kind:
  clients read `Server restarted` + newline, then the process exits with code 0.
- `Server was killed.` + newline (killserver, question 2).
- `Server was killed: <message>` + newline, `Server crashed: <message>` + newline, `Server fatal crashed:
  <message>` + newline (errors, question 3).

### 2. `killserver`

- When no game is initialized (no map ever loaded, or already killed): prints `No server running.` +
  newline on the console and does nothing else. **[read]**
- Otherwise the same shutdown sequence as in 1, steps 1–8, with the text `Server was killed.` + newline and
  the **disconnect** kind: every client gets the high-level print `Server was killed.` + newline plus a
  disconnect, twice; relays get the "disconnect" operation; local `mvdrecord` stopped silently; TCP
  listener closed; masters sent `shutdown` if dedicated and `public` is non-zero; game shut down
  (`==== ShutdownGame ====`). Nothing else is printed. **[read]**
- The process does **not** exit. Afterwards **[read]**:
  - The UDP socket stays open. Of connectionless packets, **only `rcon` is still answered**; `ping`,
    `ack`, `status`, `info`, `getchallenge` and `connect` are ignored silently (a developer-level note
    "ignored connectionless packet" only). Packets on the old client channels are ignored.
  - No game frames run, no heartbeats are sent, no relay (TCP) connections are accepted.
  - The console still reads and runs commands each frame; the terminal title shows `<hostname> (port
    <port>, down)`.
  - `sv_running` reads `0`.
  - It stays like this until a `map` (or `gamemap`, `demomap` etc.) starts a game.
- The game module also issues `killserver` itself when a level's "next server" chain ends with nothing
  left to load, and when a map command prefixed with `!` names a map that does not exist. **[read]**

### 3. Error paths

All three kinds below run from the command or frame that raised the error; the frame is then abandoned.
**[read]**

- **Disconnect-kind or reconnect-kind error** (raised by code, e.g. a client asking the server to
  disconnect itself): the console prints `<message>` + newline as a **warning** (yellow on a terminal); clients
  get `Server was killed: <message>` + newline with a disconnect (or reconnect for the reconnect kind),
  same sequence as 2. The process keeps running, idle as after `killserver`. **[read]**
- **Drop error** (a recoverable error, e.g. a bad map): the console prints, as an **error** (red),
  exactly:
  ```
  ********************
  ERROR: <message>
  ********************
  ```
  Clients get `Server crashed: <message>` + newline with a **disconnect**; relays get "disconnect". The
  server stays up, idle as after `killserver`. **[read]**
- **Fatal error**: if a console log is open, the line `FATAL: <message>` + newline is written to the log
  directly; clients get `Server fatal crashed: <message>` + newline with a **disconnect** (relays:
  "disconnect"); then the network is shut down, terminal history saved, the log closed (`Closing console
  log.`), file system shut down; then on **stderr** exactly:
  ```
  ********************
  FATAL: <message>
  ********************
  ```
  (each of those three lines starts with `<3>` when `console_prefix` begins with `<?>`), and the process
  exits with the **failure** exit code (1). **[read]**
- The setting `com_fatal_error` (default `0`): when non-zero, every drop error is treated as fatal.
  `com_debug_break` (debug builds only) breaks into a debugger before. **[read]**
- An error raised while handling an error goes straight to the fatal stderr banner with
  `recursive error after: <first message>`. An error during start-up gives `Error during initialization:
  <message>` in the fatal banner. **[read]**
- If the dedicated server cannot open its UDP port at start-up the fatal message is `Couldn't open
  dedicated server UDP port`. **[read]**

---

## B. Console language words

### 4. `_echo` and `echo`

**`echo`** (no options at all) **[read]**
- Prints the arguments joined by single spaces, then a newline, at the normal level. Quotes are removed by
  tokenizing, whitespace runs collapse to one space. `echo -n x` prints `-n x`. With no arguments it prints
  an empty line.

**`_echo [options] [text]`** **[read]**
- Options (parsed left to right; parsing stops at the first word that does not start with `-`, or after a
  `--` word, which is consumed):
  - `-h` / `--help`: prints the usage block below and does nothing else.
  - `-e` / `--escapes`: interpret backslash escapes in the text.
  - `-c <color>` / `--color <color>` / `--color=<color>`: print in this colour. A colour is a name —
    `black`, `red`, `green`, `yellow`, `blue`, `cyan`, `magenta`, `white`, `alt`, `none` (exact,
    case-sensitive) — or an index 0–9 in that order. Anything else silently means "no colour".
  - `-l <level>` / `--level <level>` / `--level=<level>`: the message level, chosen by the **first letter only**,
    case-insensitive: `a` all (normal), `t` talk, `d` developer, `w` warning, `e` error, `n` notice. Any
    other first letter prints `Bad print level: <value>` + newline and stops (nothing echoed).
  - `-n` / `--no-newline`: leave the trailing newline off.
  - Short options cannot be combined (`-en` is an unknown option). A lone `-` is an unknown option.
- Option errors (each stops the command, nothing echoed):
  - `Unknown option: <word>.` + newline, then `Try '_echo --help' for more information.` + newline
    (the command name is the word as typed).
  - `Missing argument to <word>.` + newline, then the same "Try" line (for `-c`/`-l` at the end of the line).
  - `<word> does not take an argument.` + newline, then the "Try" line (e.g. `--escapes=1`; `<word>` is the
    word with its `=` and value cut off, e.g. `--escapes does not take an argument.`). An unknown long
    option given with `=` is likewise named without its `=` part.
- **The help block** (`_echo -h`), exactly:
  ```
  Usage: _echo [-hec:l:n] [text]
  Print a line of text into the console.

  Available options:
  -h | --help          | display this message
  -e | --escapes       | enable interpretation of backslash escapes
  -c | --color=<color> | print text in this color
  -l | --level=<level> | print text at this message level
  -n | --no-newline    | do not output the trailing newline

  ```
  (the long names are left-aligned in a 13-column field; the block ends with an empty line).
- **The text**: the raw remainder of the command line starting at the first non-option word, as it stands
  after `$` macro expansion (inner spacing kept as typed, trailing whitespace dropped). If that remainder
  both begins and ends with a double quote, those two quote characters are removed (only those; inner
  quotes stay). With no text, only the newline is printed (or nothing with `-n`).
- **Escapes** with `-e` (each backslash pair is replaced by one byte):
  - `\a` bell (7), `\b` backspace (8), `\t` tab (9), `\n` newline (10), `\v` vertical tab (11), `\f` form
    feed (12), `\r` carriage return (13), `\\` one backslash.
  - `\x` followed by exactly two hex digits (either case): the byte with that value. If the two characters
    after `\x` are not both hex digits, the `\x` is dropped and the following characters are kept as text.
  - `\` followed by an octal digit 0–7: all consecutive octal digits that follow are read as one number;
    the byte is that number taken modulo 256. `\0` therefore ends the text at that point.
  - `\` followed by any other character: that character alone (the backslash is dropped).
  - A backslash as the very last character is kept.
- **The print**: the text plus the newline (unless `-n`) is printed at the chosen level in one print.
  Effects of the level on a dedicated server: the terminal colour (talk and developer green, warning
  yellow, error red, notice cyan; normal level uses the `-c` colour); the `<?>` / `@` substitutions of
  `console_prefix` and `logfile_prefix` (see 21); the error level also stores the text as the "last
  error" (see 9). The developer level is printed even when `developer` is 0. The `-c` colour only shows at
  the normal level (other levels override it with their own colour). Colours appear only on an
  interactive terminal console (ANSI escapes); the log file has none. Control bytes produced by escapes
  appear on the server's own console and log as `.` (except newline and carriage return), per the filter
  at the top. **[read]**

### 5. `text <command>`

- Takes all its arguments, joins them with single spaces (quotes removed as for any tokenized
  arguments), and **inserts that text at the front** of the command buffer it was run from, followed by a
  newline, so it runs next, before any remaining commands of the current buffer. **[read]**
- The `text` line itself was macro-expanded when it was run, but only outside double quotes. When the
  inserted text runs it is split on `;` and newlines and macro-expanded again like any command line. So the
  use is delayed expansion: `text "echo $com_time"` expands `$com_time` only when the inner command runs,
  and a quoted `;` becomes a command separator. **[derived]**
- No usage line: with no arguments nothing is inserted and nothing is printed. If the buffer has no room,
  a warning `<routine name>: overflow` is printed and nothing is inserted. **[read]**

### 6. `complete <command>`

- On a dedicated server (either build) it does **nothing** and prints nothing. **[read]**
- (Client only, for reference: it declares a name as a command to forward to the server so the console
  can complete it; `Usage: complete <command>` with **no** trailing newline when the argument is missing;
  `<name> is already defined as a cvar` + newline if a variable has that name; silent if the command
  exists.) **[read]**

### 7. `setu` and `sets`

- Syntax `setu <variable> <value…>` and `sets <variable> <value…>`. Fewer than two arguments prints
  `Usage: setu <variable> <value>` + newline (or `sets`; the word as typed). **[read]**
- The value is every argument after the name joined with single spaces. **[read]**
- `setu` gives the variable the **user-info** flag; `sets` gives it the **server-info** flag. `set <var>
  <value> u` and `set <var> <value> s` (exactly four words) do the same; with a fourth word other than
  `u`/`s`, `set` sets the value to everything after the name. **[read]**
- **Variable does not exist yet**: it is created with that value, the info flag, and marked as created by the
  user. Before creating, the name and the value are each checked as info strings; if either fails, one of
  these is printed and **nothing is created**: **[read]**
  - `Info cvars should not contain '\', ';' or '"' characters.` + newline (when the text contains a
    backslash, a semicolon or a double quote; the high bit of each byte is ignored for this test),
  - `Info cvars should be less than 64 characters long.` + newline (64 characters or more; 63 is
    accepted).
- **Variable exists**: first the value is set exactly as a console `set` would set it, with its refusals
  (each one leaves the value unchanged) **[read]**:
  - same value as now: nothing changes (a pending latched value is dropped);
  - if the variable **already** carries an info flag, the value is checked as above and refused with the
    same messages; if it did not, the value is not checked;
  - read-only: `<name> is read-only.` + newline;
  - cheat-protected: `<name> is cheat protected.` + newline, unless the variable `cheats` is non-zero (on a
    dedicated server that is the test);
  - command-line-only: `<name> may be set from command line only.` + newline;
  - latched while a game is running: `<name> will be changed for next game.` + newline and the value is
    held until the next map (silent if the same latched value is already held).
  - Then, **whether or not the set was refused**, the variable's info flags are replaced: `setu` removes
    the server-info flag and adds user-info, `sets` removes user-info and adds server-info. Other flags
    stay. **[read]**
- On a dedicated server the user-info flag has no effect (there is no local client userinfo to send). A
  server-info variable appears in the server's info string, i.e. in `status` replies and `serverinfo`.
  **[derived]**

### 8. `macrolist`, the macros and `$` expansion

**`macrolist [filter]`** **[read]**
- One line per macro: the name left-aligned in a 16-column field, one space, the macro's current value.
  Then the footer `<shown> of <total> macros` + newline.
- The optional filter is a wildcard pattern (`*`, `?`, `\` escape), **case-sensitive**, matched against the
  whole name.
- Order: most recently registered first (not sorted). Each value is computed when listed and cut to at most
  63 characters.
- The list on a dedicated server, in output order: `net_dnrate`, `net_uprate`, `com_maplist`, `random`,
  `com_uptime_long`, `com_uptime`, `com_time`, `com_date` (8 macros). Builds with a client add others
  (client-only, not on a dedicated server). **[read]** (order from registration order: **[derived]** but
  certain)

**Each macro** **[read]**
- `$com_date`: the local date formatted with the C library's strftime by the setting `com_date_format`,
  default `%Y-%m-%d` (same on every platform). Empty if the formatted result is empty or does not fit.
- `$com_time`: the local time by `com_time_format`, default `%H:%M`; on Windows the default is `%H.%M`.
- `$com_uptime`: time since start-up finished, short form: `MM.SS` (both two digits, e.g. `03.07`) under an
  hour; `H:MM.SS` under a day (e.g. `2:03.04`); `D+H:MM.SS` from a day up (e.g. `1+2:03.04`). Note the
  `.` between minutes and seconds.
- `$com_uptime_long`: the long form, parts joined by `, `, each part only when non-zero: `<n> day`/`days`,
  `<n> hour`/`hours`, `<n> min`/`mins`, `<n> sec`/`secs` (singular when 1). Example `1 day, 2 hours, 3
  mins, 4 secs`; `2 hours, 5 secs`. Exactly zero gives `0 secs`.
- `$random`: one random decimal digit, 0–9, uniform, fresh each expansion.
- `$com_maplist`: the names of the `.bsp` files directly in `maps/` (not in subfolders) across all
  search paths (packs and folders), extension removed, sorted case-insensitively, case-insensitive
  duplicates removed, joined by single spaces. When it does not fit the space available, it holds only the
  leading whole names that fit.
- `$net_uprate` / `$net_dnrate`: the current upload / download rate as a plain integer number of **bytes per
  second**, no unit, no decimals. Measured over 3-second windows: bytes sent / received on UDP and TCP sockets
  are counted; once at least 3 seconds have passed since the last update (checked every frame), the rate
  becomes (bytes in the window) / 3, integer division, and the counters restart. **[read]**

**Expansion in a command line** **[read]**
- Every command line run from the console, a config file, the command buffer and full rcon is expanded
  before it is tokenized. (Limited-rcon lines are not expanded.)
- Text inside double quotes is never expanded.
- `$$` gives one literal `$`.
- A `$` at the very end of the line is left as is.
- The name after `$`: leading whitespace after the `$` is skipped; then either `{name}` (everything up to
  the closing `}`; `${$name}` also accepted) or a single word running to the next whitespace or to a
  next `$`, which ends the name and is consumed (`$name$text` form). An empty name leaves the `$` as is.
- Lookup order: (1) a macro of that name (case-sensitive); (2) a variable of that name (case-sensitive),
  unless it is a private one (e.g. `rcon_password`), which is skipped; (3) the keywords `qt` (gives `"`)
  and `sc` (gives `;`); (4) otherwise the `$name` is replaced by **nothing** (an empty string). So a macro
  wins over a variable of the same name (though a macro cannot be registered under an existing engine
  variable's name).
- A variable's value is scanned again for further `$` (nested expansion); a macro's value and the keyword
  results are not.
- `;` from `$sc` does not split the command (splitting already happened); it is a literal character in the
  arguments. **[derived]**
- Refusals, each prints its line and discards the whole command line:
  - `Line exceeded 1024 chars, discarded.` (line already 1024 or longer),
  - `Expanded line exceeded 1024 chars, discarded.`,
  - `Macro expansion loop, discarded.` (the 100th expansion in one line),
  - `Line has unmatched quote, discarded.` — printed for **any** expanded line with an odd number of
    double quotes, even one with no `$` at all.
- Inside aliases, `$1`, `$2`, `$@`, `$1-3`, `$2-` refer to the alias's arguments instead (a different
  expansion at alias time). **[read]**

### 9. `lasterror`

- Prints the last error text followed by a newline. When there has been none, it prints an **empty line**
  (just the newline). **[read]**
- What counts: **[read]**
  - the message of every raised error of any kind (disconnect, reconnect, drop or fatal), as raised (no
    banner, no "Server crashed" wording);
  - every line printed at the **error** level (red lines, including `_echo -l e`), with one trailing newline
    removed; error-level lines printed while an error is being handled (e.g. the drop banner) do not
    replace it.
  - Warnings and normal lines do not count. It is never cleared on a dedicated server; it holds the most
    recent one, cut to 1023 characters.

### 10. `z_stats`

- Exists in **every** build (not debug-only). **[read]**
- Output, exactly this layout:
  ```
      bytes blocks name
  --------- ------ -------
  <bytes> <blocks> <name>
  ...
  --------- ------ -------
  <bytes> <blocks> total
  ```
  Bytes right-aligned in 9 columns, block count right-aligned in 6 columns, one space between. One row per
  memory category that currently holds at least one block, in this fixed order: `game` (the game module's
  allocations), `static`, `generic`, `cmd`, `cvar`, `fs`, `refresh`, `ui`, `server`, `mvd`, `sound`,
  `cmodel`. The byte counts include each block's own bookkeeping header. **[read]**

---

## C. Files

### 11. `path`

- First line `Current search path:` + newline. **[read]**
- Then one line per search path entry, in search order (highest priority first): a pack prints `<pack's
  file system path> (<n> files)`; a folder prints `<folder's file system path>`. Paths use `/`. **[read]**
- Order: the game folder's entries (home folder before the install folder) before the base folder's; within
  one folder its packs come before the folder itself, the highest-numbered `pakN` first (packs named `pak`
  + number are loaded in numeric order, then other packs alphabetically, and the last loaded is searched
  first). An optional `Q2Game.kpf` zip in the install (or home) root sits below that root's base folder.
  **[derived]** from the load order read.
- Footer lines, each only when non-zero: `<n> files in PAK files` and (builds with zip support) `<n> files in
  PKZ files` (`.pkz` packs and the `.kpf` count there). No other footer. **[read]**
- The same listing is printed at file system start-up and at every file system restart. **[read]**

### 12. `dir` and `fdir`

**`dir <directory> [extension]`** **[read]**
- Fewer than one argument: `Usage: dir <directory> [.extension]` + newline.
- The directory is a game path (relative to the game root), normalized: backslashes to `/`, duplicate,
  leading and trailing slashes removed, `.` and `..` components resolved (cannot go above the root).
- Lists the **files directly in** that directory, from **every** search path (packs and folders merged, base
  and game). Not subfolders' contents, and folder names are not listed. In folders, names starting with
  `.` are skipped. Names are shown relative to the directory (the directory part removed).
- The optional extension is matched as a **case-insensitive suffix** of the name; several may be given
  separated by `;`; the dot is not required (`bsp` matches `q2dm1.bsp`, and would also match a name merely
  ending in `bsp`).
- Folders are skipped if the directory path contains a non-printable character (on Windows also any of
  `<>:"|?*`).
- The result is sorted case-insensitively and case-insensitive duplicates are removed (a file in a pack
  and in a folder appears once).
- Output: one name per line, then `<n> files listed` + newline (always "files"). Nothing found: just `0
  files listed`.

**`fdir <filter>`** **[read]**
- Fewer than one argument: `Usage: fdir <filter>` + newline.
- Searches **all** files under the game root, recursively (folders up to 8 levels deep; packs at any
  depth). The filter is matched against the **whole path relative to the game root** (e.g. `maps/*.bsp`,
  `*.wav`): `*` matches any run of characters including `/` and nothing, `?` exactly one character, `\`
  makes the next character literal; matching is case-insensitive; several patterns may be given separated
  by `;` (a name matching any one is listed). On Windows a `\` in a name matches `/`.
- Output as `dir`: full relative paths, sorted case-insensitively, duplicates removed, then `<n> files
  listed`; `0 files listed` when nothing matches.

### 13. `fs_stats`

- **Debug builds only.** **[read]** Output:
  ```
  File slots allocated: <n>
  Total calls to open_file_read: <n>
  Total path comparisons: <n>
  Total calls to open_from_disk: <n>
  Total mixed-case reopens: <n>
  ```
  then, if no pack has any file, `No stats to display`; otherwise `Maximum hash bucket length is <n>,
  average is <x>` (x with two decimals) and, `Dumping longest bucket (<pack path>):` followed by that bucket's
  file names, one per line. (These counter labels are printed text, not interface.) **[read]**

### 14. `whereis <path> [all]`

- No argument: `Usage: whereis <path> [all]` + newline. Any second word (not only `all`) means "report
  all". **[read]**
- The path is normalized as for `dir`. Refusals: `Refusing to lookup oversize path.` (255+ characters);
  `Refusing to lookup empty path.`. **[read]**
- If a hard link (see 15) applies: `Symbolic link ('<name>' --> '<target>') in effect.` + newline, then the
  search uses the rewritten path; if the rewrite is too long: `Oversize symbolic link ('<name> --> '<target>').`
  (note the missing quote after the name, verbatim) and stop. **[read]**
- A path of 64 characters or more: `Not searching for '<path>' in pack files since path length exceedes 63
  characters.` (sic, "exceedes"), and packs are skipped. **[read]**
- Then each search path in order:
  - pack holding it: `<pack file system path>/<path> (<size> bytes)`;
  - folder holding it: `<folder path>/<path> (<size> bytes)`; on non-Windows, if the path has upper-case
    letters and only the lower-cased name exists: `Physical path found after converting to lower case.`
    first, and the line shows the lower-cased path;
  - a path with non-printable characters: `Not searching for '<path>' in physical file system since path
    contains invalid characters.` once, folders skipped;
  - a too-long full path: warning `Full path length '<folder>/<path>' exceeded 255 characters.`;
  - any other file system error: error line `Couldn't get info on '<full path>': <error text>`.
  Without "all", the first found (or the first error) ends the command — no count line then. **[read]**
- If nothing was found (or in "all" mode), and a soft link applies, `Symbolic link ('<name>' --> '<target>')
  in effect.` is printed and the search runs once more with the rewritten path. **[read]**
- Endings: "all" mode with finds: `<n> instances of <path>`; not found (either mode): `<path> was not found`.
  **[read]**

### 15. `link`, `unlink`, `softlink`, `softunlink`, `fs_restart`

Links are read-only path rewrites kept in memory (two lists: hard and soft). A link `name → target`
applies to any path that **begins** with `name` (case-insensitive, plain text prefix, not only at a `/`
boundary): that prefix is replaced by `target`. The first link (in creation order) that matches wins.
Hard links are applied to every file opened for reading before searching; soft links are tried only when
the file was not found under its own name. They do not affect `dir`/`fdir` listings or writing. **[read]**

**`link` / `softlink`** **[read]**
- With no arguments: lists that kind's links, one per line `<name> --> <target>`, then
  ```
  ------------------
  <n> symbolic link listed.
  ```
  with `link` for 1 and `links` otherwise (e.g. `0 symbolic links listed.`).
- With exactly two arguments: both are normalized as game paths; `Invalid symbolic link name.` or `Invalid
  symbolic link target.` if either becomes empty or too long. An existing link of the same name (case-
  insensitive) gets the new target; otherwise a new link is added at the end. Silent on success.
- Any other count:
  ```
  Usage: link <name> <target>
  Creates symbolic link to target with the specified name.
  Virtual quake paths are accepted.
  Links are effective only for reading.
  ```
  (the first line names the word as typed).

**`unlink` / `softunlink`** **[read]**
- Options: `-a`/`--all` deletes every link of that kind and prints `Deleted all symbolic links.`;
  `-h`/`--help` prints `Usage: unlink [-ah] <name>`, then `Deletes a symbolic link with the specified
  name.` (no newline of its own), then the options block:
  ```

  Available options:
  -a | --all  | delete all links
  -h | --help | display this message

  ```
  Option errors as for `_echo` (`Unknown option: …` + `Try 'unlink --help' for more information.`).
- No name: `Missing name argument.` then `Try 'unlink --help' for more information.`.
- Name found (case-insensitive, normalized only by exact comparison — the name is not normalized here):
  removed silently. Not found: `Symbolic link '<name>' does not exist.`.

**`fs_restart`** (no arguments) **[read]**
- Fully rebuilds the search paths from scratch (base, then game), re-reading pack lists, and prints:
  ```
  ----- FS_Restart -----
  <the path listing of question 11>
  ----------------------
  ```
- Also updates the read-only settings `gamedir` (server-info) and `fs_gamedir`, and tells a game module
  that offers the extended "restart file system" call. It does not re-run config files. Links are kept.
  (A client build does more: reloads media.) **[read]**

---

## D. Network

### 16. `net_restart`

- Closes all UDP sockets and the TCP listening sockets, then reopens what was open before: the server UDP
  socket (IPv4 on `net_ip`:`net_port`; IPv6 on `net_ip6` when `net_enable_ipv6` is 2 or more), and the TCP
  listener(s) if one was listening (TCP over IPv6 only when `net_enable_ipv6` is 2 or more). Then updates the
  terminal title. **[read]**
- It prints nothing on success (only a developer-level line naming itself). **[read]**
- Lines that can appear: socket errors (`<where>: <ip>:<port>: can't bind socket: <error>` and similar
  error lines for create, non-blocking, reuse-address (warning) and IPv6-only (warning)); if the server UDP
  port cannot be opened and a previous good port is remembered and differs: `Reverting to the last valid
  port <n>...` and `set net_port <n>` is queued; if it cannot be opened otherwise: warning `Couldn't open
  server UDP port.`; on a dedicated server that never had a good port, a fatal error `Couldn't open
  dedicated server UDP port`. **[read]**
- The same restart runs automatically when `net_ip`, `net_ip6`, `net_port` or `net_enable_ipv6` change.
  **[read]**

### 17. `net_stats`

Exactly these lines **[read]**:
```
Network uptime: <time>
Bytes sent: <n> (<n> bytes/sec)
Bytes rcvd: <n> (<n> bytes/sec)
Packets sent: <n> (<n> packets/sec)
Packets rcvd: <n> (<n> packets/sec)
Total errors: <send>/<recv>/<icmp> (send/recv/icmp)
Current upload rate: <n> bytes/sec
Current download rate: <n> bytes/sec
```
- `<time>` is the time since start-up finished, in the short form of `$com_uptime` (`MM.SS`, `H:MM.SS`,
  `D+H:MM.SS`); taken as at least 1 second.
- Bytes are lifetime totals over UDP and TCP sockets; packets count UDP datagrams only (TCP is not counted in
  packets). The per-second figures are lifetime total divided by the uptime in whole seconds (integer).
- The errors line: builds with ICMP error reporting (Linux with the error-queue header, and Windows) show
  three counts as above; other builds show `Total errors: <send>/<recv> (send/recv)`.
- The two "current" rates are the 3-second-window values described under `$net_uprate`.
- All numbers are plain unsigned decimals.

### 18. `showip`

**[read]**
- Takes the machine's host name. If that fails: error line `Couldn't get system host name`.
- Resolves it (IPv4 only when `net_enable_ipv6` is below 1; otherwise IPv4 and IPv6). Failure: `Couldn't
  resolve <hostname>: <resolver error text>`.
- If the resolver gives a canonical name: `Hostname: <canonical name>`.
- Then one line per address returned: `IP      : <numeric address>` for IPv4 (`IP`, then 6 spaces, `: `), or
  `IP6     : <numeric address>` for IPv6 (`IP6`, 5 spaces); followed by ` (<reverse DNS name>)` when a reverse
  lookup succeeds.
- Then one line per open socket: `Server UDP socket bound to <ip>:<port>`, `Server TCP socket bound to
  <ip>:<port>` (only while the TCP listener for relays is open), `Server UDP6 socket bound to [<ip6>]:<port>`,
  `Server TCP6 socket bound to [<ip6>]:<port>`. (Client sockets are listed in client builds only.) If a
  socket's name cannot be read: error `Couldn't get <Server> <UDP> socket name: <error>`. No header beyond
  `Hostname:` and no footer.

### 19. `dns <name>`

**[read]**
- Not exactly one argument: `Usage: dns <address>` + newline.
- The argument may be `[ipv6]` or `[ipv6]:port` (brackets removed; no closing bracket gives `Bad IPv6
  address`). Otherwise everything from the **first** `:` on is cut off (so an unbracketed IPv6 address is
  cut at its first colon).
- Resolves it (IPv4 only when `net_enable_ipv6` is below 1). Failure: `Couldn't resolve <host>: <resolver
  error text>`.
- Success: `Hostname: <canonical name>` when given, then one `IP      : <address>` / `IP6     :
  <address>` line per address, with ` (<reverse name>)` when the reverse lookup succeeds — the same format
  as `showip`. No count line.

---

## E. Other

### 20. `bsplist`

**[read]**
- Lists the maps currently loaded in the shared map cache (on a dedicated server that is the running map,
  at most one; after `killserver` none). The name is the game path, e.g. `maps/q2dm1.bsp` **[derived]**.
- Empty cache: `BSP cache is empty`.
- Otherwise:
  ```
  ------------------
  <bytes> : <name> (<n> refs)
  Total resident: <bytes>
  ```
  one middle line per map in load order, bytes right-aligned in 8 columns (the memory held for that map),
  the reference count in parentheses.
- With any argument (verbose), after each map's line: one line per count, `<n> : <label>` with n right-aligned
  in 8 columns, labels in order `brushsides`, `texinfo`, `planes`, `nodes`, `leafs`, `leafbrushes`,
  `models`, `brushes`, `visibility`, `entitychars`, `areas`, `areaportals` (builds with a renderer add
  `faces`, `leaffaces`, `lightmapbytes`, `vertices`, `edges`, `surfedges` and light-grid lines); then
  `<n> : clusters` when the map has visibility; then, when the map uses the extended format or extra
  lumps, `Features :` followed by ` QBSP` and/or ` BSPX`; then `Checksum : <0x-prefixed lower-case hex>`
  (`0` when zero); then `------------------`.

### 21. Settings

- **`com_completion_mode`** (default `1`) and **`com_completion_treshold`** (default `50`; sic, spelled
  "treshold"): used only by Tab completion at an **interactive terminal console** (on Unix that is
  `sys_console` 2, the default when both stdin and stdout are terminals and `TERM` is set and not `dumb`;
  on Windows the console window). Otherwise unused. **[read]**
  - When more matches than the threshold are found the first Tab prints `Press TAB again to display all <n>
    possibilities.` and the second Tab shows them. The threshold is clamped to 1..250000000 when used (an
    out-of-range value is rewritten into the setting).
  - When several matches are shown, first the line `]\` followed by the current line's words joined with
    spaces is echoed, then: mode `0` one name per line, sorted; mode `1` sorted names in up to 6 columns
    (each column as wide as its longest name + 2, columns filled top to bottom, as many as fit the terminal
    width); mode `2` or any other value: grouped, each group sorted and in columns, under the headers
    `<n> possible command(s):`, `<n> possible variable(s):`, `<n> possible alias(es):` (each preceded by an
    empty line; singular forms `command`, `variable`, `alias` when n is 1). Arguments (not the first word)
    always use the columns form. Range: none enforced. **[read]**
- **`console_prefix`** (default empty; exists where the system console exists, i.e. every dedicated server):
  text put at the start of every line written to the terminal. Before use, on non-Windows, a leading `<?>`
  has its `?` replaced by a syslog-style digit for the line's level: normal `6`, talk `5`, developer `7`,
  warning `4`, error `3`, notice `5`; the first `@` is replaced by a letter for the level: `A`, `T`, `D`,
  `W`, `E`, `N` respectively; if the text contains `%` it is passed through strftime with the local time
  (so `%H:%M` etc. work). Read at every line, so a change applies at once. If it begins with `<?>`, the
  fatal banner lines on stderr get `<3>`. It prints nothing itself. `logfile_prefix` (default
  `[%Y-%m-%d %H:%M] `) works the same way for the log file. **[read]**
- **`logfile_flush`** (default `0`): buffering of the console log: `0` normal buffering, `1` line-buffered
  (flushed at each newline), `2` or more unbuffered. Changing it while logging is on closes and reopens the
  log, printing `Closing console log.` and `Logging console to <path>` (the log is `logs/<logfile_name>.log`
  under the writable game folder; `logfile` 1 overwrites, 2 or more appends). No effect when `logfile` is 0.
  **[read]** (path wording **[derived]**)
- **`host_speeds`** (default `0`): exists **only in builds that include the client**; absent from a
  dedicated-only build. In a client build started dedicated, non-zero prints each frame
  `all:<n> ev:<n> sv:<n> gm:<n> cl:<n> rf:<n>` with each number right-aligned in 3 columns (milliseconds:
  whole frame, events, server minus game, game, client minus refresh, refresh). **[read]**
- **`sys_history`** (default `128`; exists where the system console exists): how many lines of the terminal's
  typed-command history are saved at shutdown (quit and fatal error) to the file `.syshistory` in the base
  game folder, and whether it is loaded at start-up. Values above 128 save 128; `0` or less disables both
  saving and loading. Only with an interactive terminal console. Empty lines are not saved. Prints nothing.
  **[read]**

### 22. The `game` validation message

- The `game` setting (default empty in a stock build; flags: latched, server-info, never archived) is
  checked every time its value actually takes effect: at file system start-up (including a value given on
  the command line), when it is set while no game is running (e.g. after `killserver`; the change applies
  at once), and when a latched value is applied at the next map start. **[read]**
- Check, when the value is non-empty: **[read]**
  1. If it equals `baseq2` (case-insensitive), it is silently set to empty.
  2. Otherwise, if any character is not an ASCII letter, digit, `_` or `-`, the console prints
     `'game' should contain characters [A-Za-z0-9_-] only.` + newline and the value is reset to its default
     (empty) — the reset itself is silent and does not trigger the check again.
  3. The file system is then set up/restarted with the resulting value anyway: at start-up the path listing
     of question 11 is printed; later a game-folder change prints the `----- FS_Restart -----` block and then
     the config files `default.cfg`, `config.cfg`, `autoexec.cfg` and `postexec.cfg` are run again (when
     `fs_autoexec` is 1, the default).
- Because `game` is server-info, a value with a backslash, semicolon or double quote, or 64+ characters, is
  refused earlier with the info-string messages of question 7 and never reaches this check. While a game is
  running, setting it prints `game will be changed for next game.` and the check happens at the next map.
  **[read]**

---

## Open points

- The numeric values of the protocol's print level "high" and of the disconnect/reconnect message ids were
  not re-read here (the ledger already has the reconnect as message 8). **[derived]** from protocol knowledge:
  high = 2.
- Whether a client actually receives both copies of the final shutdown message depends on the channel's
  sequence handling on the client side; not examined.
- The exact console text when a relay (TCP) connection is shut down: none is printed by the server side as far
  as read; what the relay itself prints was not examined.
- `fs_stats` counter labels name internal routines; they are debug-build output only and may be worth
  dropping rather than copying.
