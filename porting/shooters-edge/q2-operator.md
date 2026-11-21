# Answers: Quake II server operator commands, player queries, userinfo checks

Project and release read: Q2PRO upstream, commit 601a8df (server and common code, plus the
client half of one feature where a question asks about it, marked so).

Tags: **[read]** stated directly by the release's behaviour as read; **[derived]** follows from
several read facts; **[open]** the release does not settle it.

Conventions used below:

- "console" means the server's own console (and its log). "Dedicated console" means a line that
  is printed only when the process is a dedicated server.
- Print levels a client receives: low (0), medium (1), high (2), chat (3). Every print to a
  single client or to all clients is filtered by that client's own `msg` userinfo value: a print
  whose level is below the client's `msg` is not sent. Default `msg` is 0, so everything passes.
  **[read]** The one exception is redirected command output (the player commands `info`, `sinfo`,
  and the error lines of `lag`), which is sent at high level without the `msg` filter, in chunks
  of at most 1384 bytes. **[read]**
- "All clients" for a broadcast print means every client fully in the game (spawned). Clients
  still connecting never get broadcast prints. **[read]**
- Exact text is quoted in backticks; a trailing newline is written as "(newline)". Placeholders are
  in angle brackets. A client address is shown as `<ip>:<port>` (IPv6 as `[<ip6>]:<port>`), a
  loopback client as `loopback`, an address that is not set as `<unspecified>`. **[read]**
- Operator console text is $-macro expanded by the server's command processor before any command
  sees it: `$name` outside double quotes is replaced by the server cvar's value, `$$` yields a
  literal `$`, text inside double quotes is not expanded, and a line with an unmatched quote is
  discarded with the console line `Line has unmatched quote, discarded.` **[read]**
- "Raw rest of line" (used by several commands below) means: the original command line from the
  first character of the named argument to the end of the last argument, internal spacing kept,
  trailing whitespace removed, and then, if the result both starts and ends with a double quote,
  that one pair of quotes removed. Nothing else is unquoted. **[read]**

---

## A. Operator console commands

### 1. `status` with no argument

Order of output (all console): **[read]**

1. If the server is not running: `No server running.` and nothing else.
2. If a map name is set: `Current map: <mapname>` followed by an empty line.
3. If there are no UDP client slots in use (connected, connecting or zombie):
   `No UDP clients.` Otherwise the client table below.
4. An empty line.
5. In builds with the MVD/GTV server (on by default), the TCP (GTV) client section follows:
   `No TCP clients.` or its own table, then another empty line. **[read]**

Header, exactly two lines:

`num score ping name            lastmsg address                rate pr fps`
`--- ----- ---- --------------- ------- --------------------- ----- -- ---`

Rows, one per slot in use, in slot-number order (zombies included), single spaces between
columns: **[read]**

| Column | Width, alignment | Content |
| --- | --- | --- |
| num | 3, right | slot number, 0-based |
| score | 5, right | the frags stat from the player's state (may be stale or 0 for a non-spawned client) |
| ping | 4, right | the client's ping in ms, capped at 9999; replaced by a 4-letter word, below |
| name | 15, left, truncated to 15 | the client's name |
| lastmsg | 7, right | milliseconds since the last packet was received from the client (for a zombie: since it was dropped) |
| address | at least 21, left, never truncated (a longer address pushes the rest right) | `<ip>:<port>` |
| rate | 5, right | the client's rate in bytes/s (see below) |
| pr | 2, right | the client's protocol number |
| fps | 3, right | movement commands per second |

The ping words: **[read]**

- `ZMBI`: the client was dropped and its slot is held as a zombie (default 2 s, `zombietime`).
- `ASGN`: the slot was given by `connect` but the client has not yet sent `new`.
- `DNLD`: connected or primed (not yet in game) and a UDP download is open.
- `HTTP`: connected or primed, no UDP download, and the client said `download http` (question 20).
- `CNCT`: connected (sent `new`), not primed, no download.
- `PRIM`: primed (gamestate sent, waiting for `begin`), no download.
- Any client in the game shows its number. In-game ping is recomputed every server frame; the
  default method (`sv_calcpings_method` 2) is the minimum latency over the last 16 frames,
  method 1 the average, 0 always 0. **[read]**

Rate: the userinfo `rate` clamped to `sv_min_rate` .. `sv_max_rate` (defaults 15000 and 60000,
both at least 1500); 5000 if the key is absent or empty (not clamped); 0 for a loopback client,
and 0 for a LAN client when `sv_lan_force_rate` is set. **[read]**

For a stock protocol 34 client: `pr` shows `34` **[read]**. `fps` is the count of movement
commands the server ran for that client, including backup commands re-run after packet loss,
divided by 10, refreshed every 10 s of server frames; it is 0 until the first refresh and for
clients not in game **[read]**. For a stock client that sends one new command per packet this
equals its packet rate **[derived]**.

### 2. `status d|l|p|s|t|v`

Only the first character of the argument is looked at (`status lag` is `status l`). If there are
no client slots in use, `No UDP clients.` is printed whatever the letter. An unknown first
character prints `Usage: status [d|l|p|s|t|v]` and then the default table of question 1. The
`Current map:` line and the trailing empty lines are as in question 1. In all tables, `num` is the
slot number right-aligned in 3 and `name` is left-aligned and truncated to 15. **[read]**

**d (downloads)** **[read]**

`num name            download                                 size    done`
`--- --------------- ---------------------------------------- ------- ----`

Only clients with a download are listed. For a UDP download: the file path left-aligned and
truncated to 40; size in bytes left-aligned in 7; done as a percentage right-aligned in 3 followed
by `%` (bytes sent times 100 divided by size, integer). For a client that said `download http`:
the path column shows `<HTTP download>`, size `0`, done `  0%`.

**l (lag)** **[read]**

`num name            PLs2c PLc2s Rmin Ravg Rmax dup scale`
`--- --------------- ----- ----- ---- ---- ---- --- -----`

- PLs2c: server-to-client loss in percent, 2 decimals, width 5: 100 times (1 minus frames
  acknowledged over frames sent); 0.00 when nothing was sent.
- PLc2s: client-to-server loss in percent, 2 decimals, width 5: 100 times packets lost over
  packets received, from the sequence gaps; 0.00 when nothing received.
- Rmin, Ravg, Rmax: width 4 each, ms. Rmin starts at 9999 and Rmax at 0 until a non-zero in-game
  ping is measured. Ravg is the mean of the ping sampled once every 10 s; before the first sample
  it shows the current ping.
- dup: width 3, the number of extra copies of each frame packet (question 17); 0 normally.
- scale: width 5, 3 decimals, the client's time scale (milliseconds of movement the client
  claimed divided by real milliseconds over the last window of `sv_timescale_time`, default 16 s),
  0.000 before the first window ends.

**p (protocols)** **[read]**

`num name            major minor msglen zlib chan`
`--- --------------- ----- ----- ------ ---- ----`

major (protocol) right-aligned in 5; minor version right-aligned in 5; msglen (the client's
maximum packet payload) right-aligned in 6; then two spaces, `yes` or `no ` (3 characters, with a
trailing space), two spaces, `new` or `old` (net channel kind). For a protocol 34 client:
`   34     0` then `  1390` with the default `net_maxmsglen` (1390), then `no ` and `old`.

**s (settings)** **[read]**

`num name            proto options upd fps`
`--- --------------- ----- ------- --- ---`

proto right-aligned in 5; options a 7-character field with one letter per client setting in this
order, a space where the setting is off: `G` no gun, `B` no blend, `R` recording, `I` no gibs,
`F` no footsteps, `P` no prediction, `L` no flares; upd (player-update setting) right-aligned in 3,
fps (update rate setting) right-aligned in 3. A protocol 34 client cannot send client settings
(the message kind is refused for protocol 34 with the drop reason `unknown command byte`), so for
it the options field is 7 spaces, upd 0, and fps 0 (in a build with variable server frame rate,
fps shows 10). **[read]**

**t (time)** **[read]**

`num name            idle time`
`--- --------------- ---- --------`

idle: whole seconds since the client's last activity, capped at 9999, right-aligned in 4.
Activity is a movement command whose buttons or move values differ from the previous one, or a
`say` or `say_team` command. time: time since the client first sent `new`, written as
`<mm>.<ss>` under an hour, `<h>:<mm>.<ss>` under a day, `<d>+<h>:<mm>.<ss>` beyond (minutes and
seconds two digits, a dot between them). A client that has not sent `new` yet has no start time,
so its value is measured from the clock's epoch **[derived]**.

**v (versions)** **[read]**

`num name            version`
`--- --------------- -----------------------------------------` (41 dashes)

version: the client's answer to the version probe (question 29), cut to its first 52 characters,
or `-` if it has not answered.

### 3. `kick <id>`, `kickban <id>`

Checks in order, all console lines: **[read]**

1. `No server running.`
2. Exactly one argument, else `Usage: kick <userid>` (or `Usage: kickban <userid>`).
3. The id is resolved:
   - All digits: a slot number. Out of range (not below `maxclients`):
     `Bad client slot number: <n>`. A free or zombie slot: `Client slot <n> is not active.`
     A name made only of digits can therefore never be matched by name.
   - Otherwise an exact, case-sensitive name match against every client not free or zombie; the
     first match wins.
   - With `sv_enhanced_setplayer` 0 (default) and no exact match:
     `Userid '<id>' is not on the server.`
   - With `sv_enhanced_setplayer` non-zero and no exact match: a case-insensitive exact match is
     accepted at once; otherwise case-insensitive substring matching. None:
     `No clients matching '<id>' found.` More than one: `'<id>' matches multiple clients.`
   - An empty id resolves to nobody and prints nothing.

The kick drops the client with the reason "was kicked" in its no-prefix form: **[read]**

- Everyone in the game (only if the kicked client was itself in the game), at high level:
  `<name> was kicked` (newline).
- The kicked client itself (whatever its state), at high level, before its disconnect:
  `<name> was kicked` (newline), followed by a reliable disconnect.
- Dedicated console: `<name>[<ip>:<port>] was kicked`
- The slot becomes a zombie and frees after `zombietime` (2 s). The game library is told the
  player left only if the player was in the game.

`kickban` additionally adds a ban for the client's address, only for IPv4 or IPv6 clients (not
loopback): the full address for IPv4 (mask /32), the /64 network for IPv6. It is appended to the
server's own address ban list, the same list `addban`, `delban` and `listbans` manage (it shows in
`listbans` as `<ip>/32` with hits 0, last hit `never`, empty comment). No duplicate check is made.
It is not the game library's `addip` list (that is the game's own command), and it is never
written to a file: it lasts until the process exits or it is deleted. A banned address trying to
connect gets the refusal `Your IP address is banned from this server.` (newline)
`Connection refused.` (newline) as a connectionless print. **[read]**

### 4. `stuff <id> <text>`, `stuffall <text>`, `printall <text>`

**stuff** **[read]**: `No server running.`; fewer than two arguments:
`Usage: stuff <userid> <raw text>`; then id resolution as in question 3 (so a free or zombie
slot gives `Client slot <n> is not active.`; any other state, including still connecting, is
accepted). The text is the raw rest of line from the second argument (see conventions). It is
sent to that client as one reliable stuffed command with **no newline added**. There is no
"not in game" refusal.

**stuffall** **[read]**: `No server running.`; no argument: `Usage: stuffall <raw text>`. The raw
rest of line from the first argument, no newline added, one reliable stuffed command to every
client not free or zombie (connecting clients included).

**printall** **[read]**: `No server running.`; no argument: `Usage: printall <raw text>`. The raw
rest of line plus a newline, sent at high level to every client not free or zombie (connecting
clients included, each subject to its `msg` filter). On a dedicated server the console also prints
the text followed by a newline.

Because the server expands `$` macros in its own console line first, a literal `$` to the client
needs `$$` or double quotes around the text; the outer quotes are then stripped. **[derived]**

### 5. `stuffcvar <id> <var> [<var> ...]`

**[read]** `No server running.`; fewer than two arguments:
`Usage: stuffcvar <userid> <variable> [...]`; then id resolution as in question 3.

For each variable name, in argument order, the client receives one reliable stuffed command whose
text is: `cmd`, a space, the DEL byte (0x7F) followed by `c`, a space, `console`, a space, the
variable name, a space, `$` immediately followed by the variable name, and a newline. The client
is expected to expand `$<var>` and forward the line, so it sends back the string command
"DEL c console <var> <value>". A per-client counter goes up by one per query.

An answer is recognised by its first word being DEL `c` and its second word `console`; the
variable name in the answer is not checked against what was asked. If the counter is above zero,
the console (dedicated or not) prints `<name>[<ip>:<port>]: "<var>" is "<value>"` where `<var>`
is the third word and `<value>` is the raw rest of the answer line after it; the counter goes down
by one. Answers that arrive when the counter is zero are ignored silently.

Limits: no limit of its own **[read]**. The operator line is limited to 1023 characters and 256
words by the command processor **[read]**; the server accepts at most 8 string commands from a
client per packet, and extras in the same packet are dropped silently, so a client that bundles
many answers into one packet may lose some **[derived]**.

### 6. `addstuffcmd`, `delstuffcmd`, `liststuffcmds`

There are two lists, named exactly `connect` and `begin` (case-sensitive). They are global, kept
across map changes, and lost when the process exits. **[read]**

When they run: **[read]**

- `connect` list: sent to a client at its first `new` of a connection (the `new` that moves it
  out of the "assigned" state), never on later `new`s after map changes. A client that reconnects
  is a new connection and gets it again. Order in that `new`: after the version probe, before the
  forced-reconnect query (question 30), before any cvar-ban queries, before the gamestate and the
  `precache` command. With `sv_force_reconnect` set, the first connection is dropped before this
  point, so the list goes only to the reconnected connection.
- `begin` list: sent at every successful `begin` (every map, including the first), after the
  client is marked as in game and before the game library's begin call.
- Each entry is sent to that one client as its own reliable stuffed command: the stored text
  followed by a newline.

Commands and messages (console): **[read]**

- `addstuffcmd <list> <command>`: fewer than two arguments: `Usage: addstuffcmd <list> <command>`.
  Unknown list name: `Unknown stuffcmd list: <name>`. The stored text is the raw rest of line from
  the second argument. An exact (case-sensitive) duplicate: `Stuffcmd already exists: <text>`.
  New entries are appended at the end. No count limit; the length is bounded only by the 1023
  character console line.
- `delstuffcmd <list> <id|cmd|all>`: fewer than two arguments:
  `Usage: delstuffcmd <list> <id|cmd|all>`. Unknown list: `Unknown stuffcmd list: <name>`.
  Empty list: `No stuffcmds registered.` (checked before the argument is read). The argument is
  the raw rest of line from the second argument. `all` empties the list silently. All digits: a
  1-based index; out of range (including 0): `No such stuffcmd index: <n>`. Otherwise an exact,
  case-sensitive text match: `No such stuffcmd string: <text>` if none. A successful delete prints
  nothing.
- `liststuffcmds <list>`: not exactly one argument: `Usage: liststuffcmds <list>`. Unknown list:
  `Unknown stuffcmd list: <name>`. Empty: `No stuffcmds registered.` Otherwise:

`id command`
`-- -------`

then one row per entry: the 1-based index left-aligned in 2, a space, the text.

### 7. `listmasters`

**[read]** No masters configured: `There are no masters.` Otherwise:

`num hostname              lastmsg address`
`--- --------------------- ------- ---------------------`

One row per configured master, in the order given to `setmaster` (at most 8): number 1-based
right-aligned in 3; the name as typed, left-aligned and truncated to 21; lastmsg right-aligned in
7, which is `down` if the server is not running, `never` if no acknowledgement has come since the
server started, else the milliseconds since the last acknowledgement; the resolved address
left-aligned in 21, or `<unspecified>` for a master that could not be resolved. Unresolved
masters are retried every 3 hours, resolved ones re-resolved every 24 hours. Heartbeats are sent
only by a dedicated server with `public` set, every 300 s, one master per send.

### 8. `dumpents <file>`

**[read]**

- No map loaded (or the map has no entity text): `No map loaded.` (checked first).
- Not exactly one argument: `Usage: dumpents <filename>`.
- The file is written under the current game directory (the write directory), in the folder
  `entdumps`, created if missing. The name is sanitised: backslashes become slashes, `.` and `..`
  parts and repeated, leading and trailing slashes are removed (so it cannot leave the folder),
  and every character outside printable ASCII (32 to 126) becomes `_` (on Windows also any of the
  characters less-than, greater-than, colon, double quote, bar, question mark, star). The
  extension `.ent` is appended unless the name already ends with `.ent` (case-insensitive).
- The content is the map's entity text exactly as loaded (raw bytes, no terminator added). An
  existing file is overwritten.
- Success: `Dumped entity string to entdumps/<name>.ent`
- A name that is empty after sanitising: `Couldn't open <name as typed>: Invalid quake path`.
  A too-long path: `Couldn't open <path>: <system message for a name too long>`. Other open or
  write failures: `Couldn't open <path>: <reason>` or `Couldn't write <path>: <reason>`. These
  failure lines are error-coloured console prints.

### 9. `killserver`, `quit [reason]`, `recycle [reason]`

All three end in the same shutdown of the server half. In the shutdown, in this order: **[read]**

1. Anticheat and the MVD/GTV server are shut down (GTV clients dropped).
2. Every UDP client that is not a zombie (any state: assigned, connecting, in game) is sent, at
   once and not through the normal queue, one packet holding a print at high level with the final
   message, followed by a disconnect (or, for `recycle`, a reconnect). Pending fragments are
   flushed first. The packet is sent twice, in two passes over all clients. This print bypasses
   the `msg` filter.
3. All client slots are freed; masters are told the server is shutting down (dedicated and public
   only); the game library is unloaded; the map is freed.
4. `sv_running` and `sv_paused` are set to 0.

The server core prints nothing to its own console for these commands (the game library may print
its own shutdown line). **[read]**

- `killserver`: `No server running.` if none. Final message `Server was killed.` (newline),
  then disconnect. A dedicated server process keeps running with no map. **[read]**
- `quit [reason]`: reason is the arguments joined by single spaces (quotes removed by
  tokenising). Final message `Server quit: <reason>` (newline), or `Server quit` (newline) with no
  reason, then disconnect. Afterwards the client half, networking, the log file and the file
  system are shut down and the process exits with success status, on a dedicated server and on a
  client build alike. **[read]**
- `recycle [reason]` (exists only in builds with a server): final message
  `Server restarted: <reason>` (newline) or `Server restarted` (newline), then a **reconnect**
  instead of a disconnect, then the same process exit with success status. The process does not
  restart itself; restarting is left to whatever supervises it. **[read]** **[derived]**
- Related: `gamemap` with `sv_recycle` 1 does a full server restart (game library reloaded) with
  the new map; with `sv_recycle` 2 or more it behaves like `recycle` with no reason. **[read]**

### 10. `dumpuser <id>`

**[read]** `No server running.`; not exactly one argument: `Usage: dumpuser <userid>`; id
resolution as in question 3. Then, console:

- an empty line, `userinfo`, `--------` (8 dashes),
- one line per userinfo pair in stored order: the key left-aligned in 20, a space, the value. An
  empty key prints as `<MISSING KEY>`, an empty value as `<MISSING VALUE>`. The stored userinfo
  includes the `ip` key the server added at connect (question 25).
- an empty line, `miscinfo`, `--------`,
- the miscinfo lines. Each label is padded with spaces to 21 columns and the value starts in
  column 22:

| Label | Value, for a protocol 34 client |
| --- | --- |
| `version` | the version-probe answer, or `-` |
| `protocol (maj/min)` | `34/0` |
| `maxmsglen` | maximum payload, `1390` by default |
| `zlib support` | `no` |
| `netchan type` | `old` |
| `ping` | current ping, integer ms |
| `movement fps` | as `fps` in question 1 |
| `update rate` | only in builds with variable server frame rate (off by default): the client's update rate |
| `RTT (min/avg/max)` | `<min>/<avg>/<max> ms`, as Rmin, Ravg, Rmax in question 2 |
| `PL server to client` | `<loss>% (approx)`, 2 decimals |
| `PL client to server` | `<loss>%`, 2 decimals |
| `packetdup` | only in builds with the packet-duplication option (off by default): extra copies |
| `timescale` | 3 decimals |
| `connection time` | time format of `status t` |

### 11. `pickclient <addr:port>` and `passive`

**[read]** Server side, `pickclient`, console messages in order: `No server running.`,
`Single player server running.` (when `maxclients` is 1); no argument:
`Usage: pickclient <address>`; an unresolvable address: `Bad client address: <text>`; an address
without a port: `Please specify client port explicitly.` Otherwise the server sends, from its game
socket, one connectionless packet whose text is `passive_connect` (newline) to that address. No
confirmation is printed.

Client side (client code of the same release, read for this question): **[read]**

- The client command is `passive` (not `passive_connect`). Used while not listening, it kills any
  local server (its clients read `Server was killed.`), disconnects, and prints
  `Listening for passive connections at <ip>:<port>.` (its own client socket address). Used again
  while listening, it prints `No longer listening for passive connections.` and stops.
- When a listening client receives the `passive_connect` packet, it prints
  `Received passive connect from <ip>:<port>.`, stops listening, takes the sender as its server
  with its configured protocol, and starts an ordinary connection (challenge, then connect) to it
  at once. If not listening, the packet is ignored (developer-only line).
- So it is a client-side waiting mode that lets the server start the exchange, for a client the
  server could not otherwise reach first **[derived]**.

### 12. `autosave`

**[read]** It is the same command as `save` with a fixed slot. Refusals, console, in order:

1. Server not running a game map: `You must be in a game to save.`
2. Game library without saving support: `Game does not support enhanced savegames.`
3. `deathmatch` set: `Can't savegame in a deathmatch.`
4. A game library that offers its own can-save check may refuse silently; otherwise, in single
   player with the player's health at 0 or below: `Can't savegame while dead!`

Then it always writes the slot `save1`; any arguments are ignored and there is no usage line or
`Bad savedir.` check (those are `save`'s). The save is marked as an "autosave" kind. Failures:
`Couldn't write level file.`, `Couldn't write server file.`, `Couldn't wipe 'save1' directory.`,
`Couldn't write 'save1' directory.` Success: `Game saved.` (The automatic level-start save, made
on map changes, is separate and goes to `save0`.)

### 13. `serverinfo`

**[read]** No running-server check. Prints `Server info settings:` then one line per server-info
setting: the key left-aligned in 20, a space, the value (empty key and value placeholders as in
question 10). The settings are every cvar flagged as server info, not private, with a non-empty
value, in alphabetical order of the cvar name (byte order, case-sensitive), characters outside
printable ASCII dropped, stopping before the total would reach 512 characters. `maxclients` here
is the full number, not reduced by reserved slots.

---

## B. Player commands

All of these are handled by the server core before the game library sees them, in any client
state (also while connecting and during a cinematic), and are not subject to the operator's
filtered-command list. Each counts as one of the 8 string commands allowed per packet. **[read]**

### 14. `info`

**[read]** The client receives the server-info lines exactly as in question 13 but without the
`Server info settings:` header: key left-aligned in 20, a space, value, one line each. Sent at
high level as redirected output (not `msg` filtered), in chunks of at most 1384 bytes. No refusal,
no flood control besides the per-packet limit; spectators and connecting clients get it too.

### 15. `sinfo`

**[read]** Exactly the miscinfo block of `dumpuser` (question 10, same labels, padding and value
formats) about the asking client, without the `miscinfo` title and dashes, sent at high level as
redirected output.

### 16. `lag [player]`

**[read]** With no argument the subject is the asking client. With an argument, the player is
found by the resolution of question 3 **with partial, case-insensitive matching always on**
(regardless of `sv_enhanced_setplayer`); the error lines (`Bad client slot number: <n>`,
`Client slot <n> is not active.`, `No clients matching '<id>' found.`,
`'<id>' matches multiple clients.`) go to the asker at high level as redirected output, and
nothing else is sent. On success, one high-level print (subject to the asker's `msg`) of five
lines, each label padded to 21 columns:

- `Lag stats for:       <name>`
- `RTT (min/avg/max):   <min>/<avg>/<max> ms`
- `Server to client PL: <loss>% (approx)` (2 decimals)
- `Client to server PL: <loss>%` (2 decimals)
- `Timescale          : <scale>` (3 decimals; note the colon sits after the padding)

Values as Rmin, Ravg, Rmax, PLs2c, PLc2s and scale in question 2.

### 17. `packetdup <n>`

**[read]** The command exists only in builds made with the packet-duplication option, which is
off by default; without it `packetdup` is an unknown command and goes on to the game library like
any other (when the client is in the game). The setting `sv_packetdup_hack` likewise exists only
in such builds, default 0.

- No argument: `Server is sending <n> duplicate packet<s> to you.` at high level, where `<s>` is
  empty when n is 1 and `s` otherwise.
- With an argument (read as an integer, non-numbers as 0): if below 0 or above
  `sv_packetdup_hack`: `Packetdup of <n> is not allowed on this server.` and nothing changes.
  Otherwise the setting is stored and the same `Server is sending ...` line is printed.
- Meaning: every frame datagram to that client is transmitted n+1 times, identical (same
  sequence numbers), and the rate accounting counts all copies. Reliable-only sends to a client
  not yet in the game are sent once. The value is shown as `dup` in `status l` and `packetdup` in
  `dumpuser`/`sinfo`.

### 18. `stopdl`

**[read]** With no UDP download open: nothing happens and nothing is sent. With one open: the
client is sent a reliable download message with size -1 and the percentage done so far (bytes
sent times 100 divided by size); the download is closed; the client's frame numbering is
realigned. The server prints only a developer-level console line. No text print goes to the
client; what the client shows is up to the client.

### 19. `nogamedata`

**[read]** Each use toggles a per-client "no game data" flag (off, on, off ...). No answer is
printed. Any client may use it. While on, the client counts as not active: no frames (entity and
player state) are sent, no positional sounds, no unreliable broadcasts; only reliable messages
(prints, stuffed commands, configstring changes) are sent, with a resend or keep-alive at most
once per second. Turning it off resumes frames (frame numbering realigned).

### 20. `download http`

**[read]** When the argument is exactly `http` (case-sensitive), the server only records that
this client is fetching over HTTP; nothing is sent back and no file is looked up. It shows as the
ping word `HTTP` in `status` (while the client is connected or primed and has no UDP download) and
as a `<HTTP download>` row with size 0 and `  0%` in `status d`. The mark is cleared when the
client enters the game. The client is expected to send it while it fetches files from the HTTP
download server that the server announces in its connect reply when `sv_downloadserver` is set
**[derived]**.

---

## C. Userinfo and connect checks

### 21. Userinfo checks at the connectionless `connect`

A connectionless packet whose first line is 1024 characters or longer is ignored silently
(developer line only) **[read]**. The userinfo is the fifth word of that line (so it cannot
contain a double quote). Every refusal is a connectionless `print` packet with the text shown.

Order of all connect checks: **[read]**

1. Protocol outside 34..36: `Unsupported protocol version <n>.` Below 34:
   `You need Quake 2 version 3.19 or higher.`
2. Loopback clients skip 2 to 5. Challenge: a stored challenge for this address that differs:
   `Bad challenge.`; none stored: `No challenge for address.`
3. Address banned (question 3): `<ban comment or "Your IP address is banned from this server.">`
   (newline) `Connection refused.`
4. `sv_locked` set: `Server is locked.`
5. Per-address limit `sv_iplimit` (default 3; zombies count half; IPv6 per /48, link-local IPv6
   exempt): `Too many connections from your IP address.` or
   `Too many connections from your IPv6 network.`
6. Maximum packet length (protocols 35 and 36 only): `Invalid maximum message length.`
7. Protocol 36 options: `Invalid netchan type.`, and the extended-limits server refusal (only for
   a server running with extended limits, never for protocol 34 on a normal server).
8. Userinfo, in this order:
   1. Empty: `Empty userinfo string.`
   2. Malformed or oversize: `Malformed userinfo string.` (one message covers both, see below).
   3. The `name` value, cut to its first 15 characters, contains no character from 33 to 126
      (empty or only spaces): `Please set your name before connecting.`
   4. Password: with `sv_password` set, an empty `password` key:
      `Please set your password before connecting.`; too many attempts (rate `sv_auth_limit`,
      default 1 per second with a burst of 5; only failed attempts use it up) or a wrong password: `Invalid password.`
   5. `ip` key could not be added (question 25): `Oversize userinfo string.`
   6. A userinfo ban that kicks: `<ban comment or "Userinfo banned.">` (newline)
      `Connection refused.`
9. Slot: `Server is full.`, `Server and reserved slots are full.`, or a redirect when
   `sv_redirect_address` is set.
10. The game library may refuse: `<its rejmsg>` (newline) `Connection refused.`, or just
   `Connection refused.`

Every refusal text ends with a newline. **[read]**

What counts as malformed (the same rule is used for later userinfo changes): **[read]**

- An optional single backslash at the very start; then alternating key and value separated by
  backslashes.
- A key may be empty (two backslashes in a row where a key is expected are accepted); a value may
  not be empty. A string that ends right after a key, or right after a trailing backslash (odd
  number of fields, or a missing value), is malformed.
- Every character of keys and values must be printable ASCII 32 to 126 and must not be a double
  quote or a semicolon. So a character with the high bit set anywhere in the userinfo makes the
  whole userinfo malformed (this includes high-bit "coloured" names) **[derived]**.
- A key of 64 characters or more, a value of 64 characters or more, or a whole string of 512
  characters or more (backslashes counted) is refused as malformed. So the limits are 63, 63 and
  511 characters.

### 22. Userinfo change from a connected client

For protocol 34 the only userinfo message is the full userinfo string. Per packet at most 8
userinfo updates are taken; extras are skipped silently (developer line). For each accepted
update, in order: **[read]**

1. The string read is 512 bytes or longer: drop `oversize userinfo`.
2. Empty: drop `empty userinfo`.
3. Fails the rule of question 21: drop `malformed userinfo`.
4. The name, cut to 15 characters, is empty or blank, or the name is changing faster than
   `sv_namechange_limit` allows (default `5/min`): if the server has no name stored for the client
   yet, drop `malformed name`; otherwise the stored name is put back into the userinfo (if that
   does not fit: drop `oversize userinfo`), the client is told at high level
   `You can't have an empty name.` or `You can't change your name too often.` (newline), and is
   sent the stuffed command `set name "<stored name>"` (newline).
5. Userinfo bans are checked (they may kick).
6. The game library sees the new userinfo, then the server takes the name, rate and `msg`
   (question 24).

`oversize userinfo key` and `oversize userinfo value` come only from the delta-userinfo message
that protocol 36 clients send (key or value of 64 characters or more; a key or value with a
backslash, double quote or semicolon, or a result of 512 characters or more, gives
`malformed userinfo` there). A protocol 34 client that sent that message kind would be dropped
with `unknown command byte`. **[read]**

How any drop reason without a marker is shown (all the reasons above): **[read]**

- Everyone in the game, high level, only if the dropped client was in the game:
  `<name> was dropped: <reason>` (newline).
- The dropped client itself, high level, whatever its state: the same line, then a reliable
  disconnect.
- Dedicated console: `<name>[<ip>:<port>] was dropped: <reason>`
- `<name>` is the name the server had stored before the change.

Two internal markers change this for some reasons used elsewhere: a "silent" marker sends the line
to nobody but the dedicated console; a "no prefix" marker replaces ` was dropped: ` by a single
space (`<name> was kicked`, `<name> timed out`). **[read]**

### 23. `You can't have an empty name.`

**[read]** Sent at high level when a connected client's new userinfo has a name whose first 15
characters are empty or only characters 32 and below (spaces). The client keeps the name the server
already had: it is written back into the userinfo and the client is sent
`set name "<that name>"` (newline). At connect time there is no stored name, so a blank name is
refused there instead (question 21). If the server somehow has no stored name, the client is
dropped with `malformed name`.

### 24. The name cut

**[read]** After the game library has seen (and possibly changed) the userinfo, the server takes
the `name` value, keeps at most its first 15 characters, and clears the high bit of each. Nothing
else: leading and trailing spaces are kept, there is no duplicate-name check between clients, and
control characters are not filtered at this point (the userinfo rule of question 21 has already
refused them, unless the game library put them in) **[derived]**.

On a change of name: the dedicated console prints `<old>[<ip>:<port>] changed name to <new>`; with
`sv_show_name_changes` 1 (and the client in game) or 2 and above (any state), everyone in the game
gets `<old> changed name to <new>` (newline) at high level; default 0 sends nothing. **[read]**

### 25. `mvdspec`, `password`, `ip`

**[read]** Only at the connect, after the name and password checks and before userinfo bans and
the game library's connect call, and only when the game library does not declare that it takes
connection details as an extra userinfo block:

- `mvdspec` is always removed.
- `password` is removed only when the server has `sv_password` or `sv_reserved_password` set.
- `ip` is forced to the client's `<ip>:<port>` (an IPv6 client is shown to a game that is not
  IPv6-aware as `198.51.100.<n>:<port>`, n derived from its /48 network). If that does not fit:
  `Oversize userinfo string.`

When the game library does declare it, none of these edits are made; the details (challenge, ip,
protocol major and minor, net channel, packet length, qport, zlib) are passed in a second info
block after the userinfo. Later userinfo changes from the client are not edited at all, so a
client can later send its own `ip`, `password` or `mvdspec` keys **[derived]**.

### 26. `oversize stringcmd`

**[read]** A string command of 1024 bytes or more (so the limit is 1023 characters) drops the
client with `oversize stringcmd`. A ninth and later string command in one packet is ignored
silently (developer line) rather than dropping.

### 27. `<name> timed out` and `<name> was dropped: reconnected`

**[read]**

- Timed out: checked every frame. Not for loopback clients. A client in any state but zombie with
  no packet for more than `timeout` (default 90 s), or a client still in the "assigned" state
  (has not sent `new`) with no packet for more than `sv_ghostime` (default 6 s), is dropped with
  the no-prefix reason `timed out` and its slot freed at once (no zombie period). Shown as
  `<name> timed out` (newline) to everyone in game only if the client was in the game, to the
  client itself, and on the dedicated console as `<name>[<ip>:<port>] timed out`. The difference
  while connecting is the shorter 6 s limit in the assigned state and that others are not told.
- Reconnected: when a `connect` arrives from exactly the same address and port as a slot that is
  in use and not a zombie, the old client is dropped with reason `reconnected`
  (`<name> was dropped: reconnected` to everyone in game if it was in game, to the old client,
  and on the dedicated console with address) and the new connection takes the same slot. A zombie
  slot from the same address and port is reused without any message.

### 28. `outgoing sequence too big`

**[read]** Checked every frame for every client before the timeouts: when the server's outgoing
packet sequence for that client reaches 2^31 minus 256 (2,147,483,392) on the old net channel that
protocol 34 uses, or 2^30 minus 256 on the new one. The client is dropped with
`<name> was dropped: outgoing sequence too big`. At 10 packets per second this takes about 6.8
years **[derived]**.

---

## D. Version probe and forced reconnect

### 29. The version probe

**[read]**

- Sent at the client's first `new` of a connection only (not on map changes), to every protocol,
  34 included, and including loopback: one reliable stuffed command: `cmd`, a space, the DEL byte
  (0x7F) and `c`, a space, `version`, a space, `$version`, newline. (Builds with the anticheat
  server interface, off by default, append a second similar line asking for `$actoken`.) It is
  sent before the `connect` stuffcmd list, the gamestate and `precache`.
- The client must send back the string command: DEL `c`, space, `version`, space, its version
  text. The server records the raw rest of the line after `version` as the version string, only
  the first answer, even an empty one. On a dedicated server the console prints
  `<name>[<ip>:<port>]: <version>`.
- At `begin`, if no answer has been recorded, the client is dropped silently with the reason
  `failed version probe`: no print to the client or to others, the client gets a plain
  disconnect, and only the dedicated console shows
  `<name>[<ip>:<port>] was dropped: failed version probe`.
- There is no exemption for protocol 34 or for clients that cannot answer **[read]**. A stuffed
  line reaches the client before `precache`, so a client that runs stuffed commands in order
  answers before it can send `begin` **[derived]**. How a stock client answers (whether it expands
  `$version` from its own version setting and forwards the `cmd` line) is client behaviour outside
  this release: **[open]**, but the server's unconditional drop means the release assumes stock
  clients do answer **[derived]**.

### 30. `sv_force_reconnect`

**[read]** Default empty; latched (takes effect at the next map start). Its value is the address
clients are told to connect to. Loopback clients are exempt (counted as already reconnected).

First connection (no remembered reconnect variable):

1. The connect reply leaves out the anticheat part (anticheat builds only).
2. At the first `new`, before anything else of `new`, the server makes 8 random strings of 15
   characters each, every character drawn uniformly from an 80-entry alphabet (punctuation
   characters, digits, lower-case letters, with the backtick and the tilde each appearing 10
   times). Call them A to H. It remembers C as the variable name and D as its expected value.
3. It sends these stuffed lines, each its own reliable command, each ending in a newline:
   `set A set`, then `$A B connect`, then, on a fair coin, either the three lines `$A C D`,
   `$A E <sv_force_reconnect value>`, `$A F G`, or the three lines `$A E <value>`, `$A F G`,
   `$A C D`; then `$A A ""` and finally `$B $E`. Expanded on the client, these set B to
   `connect`, set C to D and E to the address, set a decoy F to G, clear A, and run
   `connect <address>`. H is unused.
4. The client is then dropped with no reason: nobody is told, nothing is printed on the console,
   the client just gets a disconnect. The slot stays a zombie for `zombietime` (2 s).

Second connection:

5. If it comes from the same address and port while the old slot is still a zombie, C and D are
   carried over to the new slot. Then at its first `new` no junk is sent; instead, after the
   version probe and the `connect` list, it gets `cmd` DEL `c` `connect $C` (newline).
6. The answer DEL `c` `connect <value>` marks the client as reconnected when the value equals D
   exactly.
7. At `begin`, a client not marked is dropped silently with `failed to reconnect` (dedicated
   console only: `<name>[<ip>:<port>] was dropped: failed to reconnect`).
8. A second connection from another port, or after the zombie slot was freed, has no remembered
   variable and goes through steps 1 to 4 again **[derived]**.

The mark lasts for the connection, so map changes do not repeat the check. What a stock client
does with the stuffed lines depends on whether it expands `$` macros at the start of a line and in
arguments: **[open]** in this release; without that expansion it would never send the answer and
would be dropped at `begin` **[derived]**.

---

## E. Server state settings

### 31. `sv_running`, `sv_paused`, `sv_enhanced_setplayer`, `sv_packetdup_hack`

**[read]**

- `sv_running`: read-only, default 0. Set to the server's state number whenever the state changes:
  0 no map, 1 loading (spawning the level), 2 game running, 3 showing a picture, 4 relaying an MVD
  stream, 5 playing a cinematic. Set to 0 on every shutdown (`killserver`, `quit`, `recycle`,
  errors).
- `sv_paused`: read-only, default 0, values 0 and 1. Set to 1 only by a listen server (never a
  dedicated one) when the local player has paused, no timedemo is running, the local player is
  the only client and no GTV clients are watching; set back to 0 when any of that stops, at every
  map start, and on shutdown.
- `sv_enhanced_setplayer`: default 0. When non-zero, the operator commands `kick`, `kickban`,
  `dumpuser`, `stuff` and `stuffcvar` also accept a case-insensitive full name or a unique
  case-insensitive part of a name (question 3). The player command `lag` always does.
- `sv_packetdup_hack`: exists only in builds with the packet-duplication option (off by default),
  default 0; the largest number of duplicate copies a client may ask for with `packetdup`
  (question 17). 0 means clients can only keep 0.

### 32. `maxclients` minus reserved slots

**[read]** Reserved slots are `sv_reserved_slots` (default 0, latched, clamped to 0 ..
`maxclients` minus 1); when the MVD server is enabled (`sv_mvd_enable` non-zero) at least 1 slot is
reserved for its own recording client. The public count is `maxclients` minus the reserved slots.

Where the reduced number shows:

- The connectionless `status` reply and the master heartbeat: the `maxclients` key in their
  server-info line carries the reduced number.
- The connectionless `info` reply: `<hostname> <map> <players>/<public max>`, the hostname
  right-aligned in 16, the map right-aligned in 8, both numbers right-aligned in 2. The player
  count counts every slot not free or zombie, reserved ones included.

Where it does not: the console `serverinfo` command, the player `info` command and the
configstring a client receives all show the full `maxclients`.

Who may use reserved slots: a client whose `password` equals `sv_reserved_password`; and, when
`sv_password` is set, every client that passes it (the full slot count then applies to all of
them). With no reserved password set, nobody can use them. A forced redirect (an
`sv_redirect_address` starting with `!`) applies only to clients limited to the public slots.

---

Protocols 35 and 36: they differ in the packet-length, net-channel, zlib, minor-version and
client-setting fields noted above, the delta-userinfo message (question 22) and the new net
channel's sequence limit (question 28). Everything else above applies to all three. **[read]**
