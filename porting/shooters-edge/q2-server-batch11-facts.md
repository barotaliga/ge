# Answers: Quake II server (Q2PRO upstream, commit 601a8df) — batch 11

Reader's notes. Facts in my own words; no code. [read] = seen directly in the source or the
shipped docs; [derived] = inferred from what was read. Print levels: "normal" = ordinary console
print, "warning" = the yellow warning print, "error" = the red error print, "developer" = shown
only with `developer` on. Every console print also goes to the main server log file when that is
enabled [read].

## A. The packet log (`net_log_enable`, `net_log_name`, `net_log_flush`)

**Build gate.** The three settings and the whole packet log exist only in builds with the debug
feature on; otherwise the settings are not registered and logging compiles to nothing [read]. The
default build type is "debug-optimized", which turns that feature on, so ordinary builds have it
[derived].

**Defaults** [read]: `net_log_enable` "0", `net_log_name` "network", `net_log_flush` "0". None
carries a flag (settable at run time, not archived).

### 1. What is written
Every packet that passes through the low-level network layer, both directions [read]:

| Direction tag (exact) | When |
| --- | --- |
| `UDP recv` | each datagram read from a UDP socket, IPv4 or IPv6, client or server side |
| `UDP send` | each datagram sent, after the send succeeded; the length logged is the count actually sent |
| `TCP recv` | each chunk read from a TCP stream (MVD/GTV connections, HTTP-less TCP streams) |
| `TCP send` | each chunk written to a TCP stream |
| `LP recv` | each in-process loopback message received (only in builds with a client) |
| `LP send` | each loopback message queued (only in builds with a client; logged after the drop simulation lets it through) |

So: in and out, UDP and TCP, and loopback when a client is built in [read]. Failed sends, would-block
results and receive errors are not logged [read]. TCP entries are per read/write chunk, not per
message [read].

**Entry layout** [read]:
- A header line: the local time in milliseconds since the program started (unsigned decimal), then
  `" : "`, the direction tag, `" : "`, the peer address, `" : "`, the length in decimal, then
  `" bytes"` and a newline. Example shape: `123456 : UDP recv : 192.168.1.5:27910 : 42 bytes`.
- The address is printed as `a.b.c.d:port` for IPv4, `[v6addr]:port` for IPv6 (brackets, scope
  suffix `%iface` inside the brackets when the address has a scope), and the bare word `loopback`
  for loopback (no port) [read].
- Then one row per 16 bytes (rounded up): a 4-digit lower-case hex offset (0000, 0010, …), `" : "`,
  sixteen 2-digit lower-case hex bytes each followed by one space (missing bytes in the last row
  padded with three spaces each), then `": "`, then the same 16 bytes as characters, printable ASCII
  (32 to 126) as itself and anything else as `.`, missing bytes padded with a space, then a
  newline.
- After the last row, one empty line [read].
- A zero-length entry would have the header and the blank line only [derived]; zero-length UDP
  sends are refused earlier, so they never appear [read].

### 2. The file
- Path: `logs/<net_log_name>.log` inside the current writable game directory (the home or base
  directory's game folder for the active `game`; `baseq2` when no mod) [read]. The name is
  normalised (no escaping the folder, bad characters replaced by underscores) and `.log` is added
  unless the name already ends in it [read]. An empty name fails [read].
- `net_log_enable` 1 = truncate and write new; 2 or more = append [read]. It is a text-mode file [read].
- On a successful open, normal print: `Logging network packets to logs/network.log` (the path
  printed is the relative one built above, followed by a newline) [read].
- On close, normal print: `Closing network log.` [read].
- On failure, error print: `Couldn't open logs/network.log: <reason>` (reason is the system error
  text), and `net_log_enable` is set back to "0" [read].
- When changes take effect: immediately. Changing `net_log_enable` closes any open log and, if the
  new value is non-zero, reopens it. Changing `net_log_name` or `net_log_flush` while enabled closes
  and reopens (so with value 1 the file is truncated again) [read]. The log is opened at network
  start-up if already enabled (e.g. set on the command line or in a config) [read]. A later change
  of `game` does not move an open log; the next reopen lands in the new game folder [derived].

### 3. `net_log_flush`
| Value | Effect [read] |
| --- | --- |
| 0 or negative | default buffered output |
| 1 | line-buffered (flushed at each newline) |
| 2 or more | unbuffered (every write goes straight to the file) |

## B. IPv6 (`net_ip6`, `net_enable_ipv6`)

### 4. Defaults, values, effects
- `net_ip6`: default "" (bind all interfaces); no flags; a change restarts the network layer at
  once (sockets closed and reopened) [read]. Same restart for `net_enable_ipv6`, `net_ip`,
  `net_port` [read].
- `net_enable_ipv6` default: "1" if the system can create an IPv6 TCP socket at start-up, else "0"
  [read]. No flags [read].

| Value | Server UDP | Server TCP listener | Client side / name resolution |
| --- | --- | --- | --- |
| 0 | IPv4 only | IPv4 only | lookups restricted to IPv4 |
| 1 | IPv4 only (no IPv6 server socket) | IPv4 only | client opens an IPv6 UDP socket too; lookups may return IPv6, but an IPv4 answer is preferred when both exist |
| 2 (or more) | IPv4 socket plus a separate IPv6 socket on the same `net_port`, bound to `net_ip6` | IPv4 plus IPv6 listeners on the same `net_port` | lookups use the operating system's order |

[read] for all cells. The IPv6 sockets are set IPv6-only (no IPv4-mapped addresses), so both
families are served by two sockets side by side [read]. The TCP listeners only exist while the MVD
TCP server is enabled (`sv_mvd_enable` above 1) [read]. Outgoing TCP to an IPv6 peer binds to
`net_ip6` [read].

**When the IPv6 socket cannot be opened**: not fatal; the server simply runs without it [read].
Error prints, each prefixed by an internal routine name and `": <net_ip6>:<port>: "`:
`bad interface address: <reason>`, `can't create socket: <reason>`, `can't make socket
non-blocking: <reason>`, `can't bind socket: <reason>`; warning prints `can't enable ICMP6 error
queue: <reason>` and `can't make socket IPv6-only: <reason>` [read]. (By contrast, an IPv4 server
UDP failure on a dedicated server that never had a port is fatal: `Couldn't open dedicated server
UDP port` [read]; if a port had worked before, it prints `Reverting to the last valid port <n>...`
and sets `net_port` back [read].) For the TCP listener, the MVD code prints `Error opening server
TCP port.` or `Server TCP port already in use.` only when both families fail, and drops
`sv_mvd_enable` to 1 [read].

**Printing**: IPv6 as `[addr]:port`; with a scope id the numeric form includes `%scope` inside the
brackets; bare addresses (without port) have no brackets [read]. The `showip` command lists the
sockets as `<Client|Server> <UDP|TCP|UDP6|TCP6> socket bound to <addr>` [read]. Typed addresses
accept `[v6]:port` [read].

## C. Error paths and `com_fatal_error`

### 5. Drop (recoverable) errors an operator can hit
A drop prints, at error level, three lines: `********************`, `ERROR: <message>`,
`********************` [read]. It shuts the server down: every connected client is sent a
high-priority print `Server crashed: <message>` + newline followed by a disconnect, twice [read];
the game library is unloaded, `sv_running` set to 0, and the frame is abandoned; a dedicated
process stays up, idle, with its network still open [read]/[derived]. The message is kept for the
`lasterror` command [read].

**A bad map name is NOT a drop** [read]: the map command prints (normal) `Couldn't load
maps/<name>.bsp: <reason>` and returns, leaving the running map alone. If the name came from a
`nextserver` chain marked to kill the server, it queues `killserver` [read]. An over-long map
argument prints `Refusing to process oversize level string.` [read].

Drops (message as it appears after `ERROR: `) [read]:

| Source | Message |
| --- | --- |
| Game library's error call | `Game Error: <game's text>` |
| No game library found (mod, then base, home and lib dirs) | `Failed to load game library` |
| Entry returns nothing | `Game library returned NULL exports` |
| Wrong API version | `Game library is version <n>, expected <old> or <new>` |
| Game changed maxclients during init | `Game library corrupted maxclients value` |
| Bad entity size | `Game library returned bad size of edict_t` |
| Bad entity count | `Game library returned bad number of max_edicts` |
| Map with too many brush models | `Too many inline models` |
| Model/sound/image index table full (unless the game opts to allow overflow) | `<lookup routine>(<name>): overflow` |
| Config string out of range / too long | `<routine>: bad index: <n>`; `<routine>: index <n> overflowed: <len> > <max>` |
| Game sets model with no entity or name | `PF_setmodel: NULL` |
| Game writes a float to a message | `PF_WriteFloat not implemented` |
| Bad sound call from the game | `<routine>: edict = NULL`, `volume = <f>`, `attenuation = <f>`, `timeofs = <f>`, `soundindex = <n>` |
| Game memory calls | `<routine>: bad tag`; `<routine>: untagged allocation not allowed` |
| Message buffer overflow during unicast/multicast/broadcast | `<routine>: message buffer overflowed`; multicast `bad to: <n>` / `NULL origin` |
| Linking a null entity; bad inline model | `<routine>: NULL`; `<routine>: inline model <n> out of range` |
| Load game | `Savegame cvar name too long`, `Savegame cvar value too long`, `Game does not support enhanced savegames`, `Savegame path too long`, `Bad savegame configstring index`, `Savegame configstring too long` |
| Map and message internals | `FloodArea_r: reflooded`; `<routine>: bad cluster`; `<routine>: bad number: <n>`; `<routine>: read past end of message`; `MSG_ReadDir: out of range` |

(`<routine>` = an internal function name inserted into the text; I do not copy those.) A drop
raised while the program is still initialising (including `+map` on the command line, which runs
inside start-up) is turned into a fatal `Error during initialization: <message>` [read]/[derived].
A drop while a drop is being handled is fatal: `recursive error after: <first message>` [read].

### 6. `com_fatal_error`
- Default "0", no flags, settable any time [read]. Its only use is in the error handler [read].
- Non-zero: every drop is promoted to fatal [read]. Disconnect/reconnect-class errors (the quiet
  ones, e.g. `quit`-like shutdowns) are NOT affected: they are handled before the check [read].
- What a fatal does: no `ERROR:` banner; the main log file gets `FATAL: <message>`; clients get the
  high-priority print `Server fatal crashed: <message>` + newline and a disconnect (twice); the
  network, console history, log file and file system are shut down; then the process prints to
  stderr `********************`, `FATAL: <message>`, `********************` (on Unix each line may be
  prefixed `<3>` when the console prefix setting starts with `<?>`), and exits with status 1 (the C
  failure status) [read]. On Windows the same banner goes to the console and it exits 1 when
  exit-on-error is set or other processes share the console; otherwise it waits (`Press Ctrl+C to
  exit.`) or shows a message box [read].
- It touches nothing else [read].

## D. `fs_autoexec` and the game folder change

### 7.
- `fs_autoexec`: default "1", no flags [read].
- `game` is a latched server-info setting: while a server runs, setting it prints (normal)
  `game will be changed for next game.` and holds the value [read]. It is applied when a new game
  starts: `map` (which always fully restarts when a latched setting is pending), a load game, or
  `gamemap` with `sv_recycle` [read]/[derived]. Plain `gamemap` without a restart does not apply it
  [derived]. With no server running it applies immediately [read]. A value equal to `baseq2`
  (case-insensitive) becomes empty; an invalid name prints `'game' should contain characters
  [A-Za-z0-9_-] only.` and resets [read].
- On applying: the file system restarts (game paths only; prints `----- FS_Restart -----`, the path
  listing, a dashed line), the game library's own file-system hook is told if it has one, and then,
  only when `fs_autoexec` is non-zero, configs run [read]. Order [read]:
  1. `default.cfg` — game folder only (pack or loose).
  2. `config.cfg` (`q2config.cfg` on Windows) — real file, game folder only.
  3. `autoexec.cfg` from the base folder — only when a mod is active, real file.
  4. `autoexec.cfg` from the game folder — real file.
  5. `postexec.cfg` — real file, any folder.
  "Game folder only" widens to all folders when `game` is empty (back to base) [read]. Each found
  file prints `Execing <name>` [read]; a missing file is silent; any other failure prints the
  warning `Couldn't exec <name>: <reason>` [read]. Each file runs at once, before the new game
  library loads [derived].
- With `fs_autoexec` 0: the folder switch and file-system restart still happen; no config runs, no
  extra message [read].
- At first start-up, the same list runs regardless of `fs_autoexec`, from any folder, and
  `postinit.cfg` runs at the end of start-up [read].

## E. `sys_allow_unsafe_savegames`

### 8.
- Default "0", flag "command line only": after start-up, setting it prints `sys_allow_unsafe_savegames
  may be set from command line only.` and is ignored [read]. Exists only in builds with save games
  (on by default) [read].
- "Safe" games are those that set the enhanced-savegame feature bit (the bundled base game does);
  a second game family is recognised by its first two debug prints at init (`Game is starting
  up.` then `Game is <..> built on <..>.`) [read]. Every other library (the original id game and
  most mods) lacks it [derived].
- At each new non-deathmatch game, the server prints one of [read]: `Game supports Q2PRO enhanced
  savegames.` / `Game supports YQ2 enhanced savegames.` (normal), `Use of unsafe savegames forced
  from command line.` (warning, setting 1, unsafe game), `Game does not support enhanced savegames.
  Savegames will not work.` (warning, setting 0).
- Setting 0 with an unsafe game: `save` prints `Game does not support enhanced savegames.` and does
  nothing [read]; `load` copies the files, restarts the server, and then drops with `Game does not
  support enhanced savegames` [read]; automatic level-change saves and level restores are skipped
  silently [read].
- Setting 1: the unsafe game is treated as safe for save, load and autosave; the only warning is
  the start-of-game line above [read].

## F. `net_chantype`

### 9.
- Default "1", no flags; a CLIENT setting only — the server never reads it [read].
- The client sends it in its connect request only when talking protocol 36: 0 = the classic
  channel, 1 = the extended channel (which fragments large datagrams in the channel) [read]. Read at
  each connect attempt [read].
- The server picks per client at connect [read]:

| Client protocol | Channel |
| --- | --- |
| 34 (original) | classic (fixed) |
| 35 | classic (fixed) |
| 36 | the client's value; missing → extended; outside 0..1 → refused with print `Invalid netchan type.` |

- For protocol 36 the server confirms in its connect reply with ` nc=1` or ` nc=0` [read]. A client
  receiving another value disconnects with `Server returned invalid netchan type` [read].
- The extended channel also changes server behaviour per client (e.g. entity truncation off,
  fragments sent) [read].
