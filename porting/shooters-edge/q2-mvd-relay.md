# Quake II's multi-view recorder, its TCP relay and the relay player (q2pro): clean-room facts

What this is: the lasting facts a clean-room reader wrote down about q2pro's server-side multi-view
recorder, the TCP stream it serves to relays, and the relay that plays such streams to ordinary
clients. It complements `demo-formats-q2.md` § 5 (the file and stream formats), which it does not
repeat, and answers the ledger rows cmd.mvd*, cmd.*gtv*, cvar.sv_mvd_*, cvar.mvd_*, mvdcmd.*,
msg.mvd.* and log.mvd* in `q2-ledger-server.md`.

Project and commit read: **q2pro** (GitHub q2pro/q2pro), commit 601a8df. No other fork was read.
Nothing was run, so nothing here is [measured].

Every fact is marked **[read]** (the source states it) or **[derived]** (worked out from read
facts). Where the source does not settle a question, the answer says so. The stream's framing, op
numbers, hello fields, deflate and the gamestate and frame blocks are in
`porting/shooters-edge/demo-formats-q2.md` § 5 and are not repeated. Op names below are that
note's: client ops hello, ping, stream start, stream stop, string command; server ops hello, pong,
stream start (ack), stream stop (ack), stream data, error, bad request, no access, disconnect,
reconnect.

Units: the server's frame is 100 ms (10 frames a second) **[read]**. "Clamped a to b" means a
value outside the range is replaced by the nearest end **[read]**.

In the relay's console lines, <warn> stands for a three-character marker: an equals sign, an
exclamation mark and an equals sign, with no spaces **[read]**.

Time settings in this area are stored in one unit and used in another: a setting "in seconds"
is turned into milliseconds when it is set, a setting "in minutes" likewise **[read]**.

---

## A. The game server's recorder and its TCP side

### 1. `sv_mvd_enable`: values, latch, the TCP port, telling a viewer from other TCP traffic

- **0** (default): no recorder at all. `mvdrecord` answers "MVD recording is disabled on this
  server." **[read]**
- **1**: the recorder exists (buffers, the stream flags, the dummy), so local recording with
  `mvdrecord` and auto-recording work; no TCP socket is opened **[read]**.
- **2** (or more): as 1, and the server also listens for TCP viewers **[read]**.
- **Latched** **[read]**. A change takes effect only when the game is next initialised (a server
  start, or a map change that restarts the server: one with latched settings pending, a
  `sv_recycle`, or the first map) **[read]**. At that restart the old recorder is shut down: every
  connected TCP viewer that had sent a hello gets the **reconnect** op, others are just closed
  **[read]**. If the new value is 0, the relay's reconnect attempts then fail to connect and back off
  (question 11) **[derived]**.
- **When enabled, it also forces** (at game init) `maxclients` to at most 255 (one client number is
  kept as the "none" value), `sv_reserved_slots` to at least 1 (a slot for the dummy) and
  `sv_mvd_maxclients` into 1 to 256 **[read]**.
- **The port**: the TCP listener binds the same address setting and the same port number as the
  UDP game socket (`net_ip` and `net_port`, default 27910) **[read]**. IPv6 TCP is opened only
  when `net_enable_ipv6` is 2 or more (with `net_ip6`) **[read]**. The listen backlog is 16
  **[read]**.
- **If the socket cannot be opened** the console gets "Error opening server TCP port." (an error)
  or, when it is already open, "Server TCP port already in use." and `sv_mvd_enable` is set back
  to 1 **[read]**.
- **Telling viewers apart**: nothing else in the server listens on that TCP port, and there is no
  HTTP or other protocol sniffing **[read]**. Every accepted connection must first send the 4
  magic bytes ("MVD2"); the server echoes them back. Anything else closes the connection with the
  reason "not a MVD/GTV stream" **[read]**. That drop is not logged, because drops are only logged
  once the magic has been accepted **[read]**. An HTTP request arriving there is therefore just
  closed **[derived]**.

### 2. The server's answer to a client hello, in order

**On accept** (before any byte is read) **[read]**:

1. **Per-address limit**: when `sv_iplimit` (default 3) is above 0, the server counts the TCP
   viewers already connected from the same IPv4 address, or the same IPv6 /48 network. At or
   above the limit: console "TCP client [<addr>] rejected: too many connections", and the socket
   is closed with nothing sent.
2. **A free TCP slot** out of `sv_mvd_maxclients`. None free: "TCP client [<addr>] rejected: no
   free slots", closed with nothing sent.
3. The connection is taken (developer-only line "TCP client [<addr>] accepted"), with a 4-byte send
   buffer, enough for the magic echo.

**The magic** must arrive first (question 1).

**The hello**, checked in this order **[read]**:

1. A hello on a connection that already had one: dropped, "duplicated hello message", nothing
   sent.
2. Bytes still waiting to be sent (the client did not read the magic echo yet): dropped, "send
   buffer not empty", nothing sent.
3. **Version word** not 0xED04: the **bad request** op is sent, then the drop "bad protocol
   version".
4. The flags, the reserved long, the name (up to 15 characters kept), the password and the version
   string are read.
5. **Access**, in this order:
   - the address matches the `addgtvhost` list: allowed, the password is not looked at.
   - else it matches the `addgtvban` list: refused.
   - else `sv_mvd_password` is empty: allowed.
   - else allowed only when the password equals `sv_mvd_password` exactly (case-sensitive).

   Refused: the **no access** op is sent, then the drop "not authorized". A list match also counts
   a hit on that list entry and stamps its time (question 8).
6. **Flags accepted**:
   - **string commands** (bit 1) is kept only when `sv_mvd_allow_stufftext` is negative, and
     removed at 0 or above (so it is removed by default).
   - **deflate** (bit 0) is kept when the server was built with zlib, removed otherwise.
   - other bits the client sent are echoed back untouched.
7. `sv_mvd_bufsize` is clamped to 1 to 4, and a send buffer of 32 768 bytes times it is set up.
8. The **hello** op is sent with the accepted flags (a long). If deflate was accepted, everything
   after this hello is deflated.
9. Console: "Accepted MVD client <name>[<addr>]".

`sv_mvd_maxclients` is not checked at hello time; it only sizes the slot pool checked on accept
**[read]**. The relay's TCP viewers are a pool separate from the UDP players' slots **[read]**.

**The other client ops** **[read]**:

- **Stream start**:
  - before the hello, or while already streaming: dropped, "unexpected stream start message".
  - the recorder is switched on (and the dummy created and spawned if it was not). If that fails:
     the **error** op, then the drop "couldn't create MVD dummy".
  - the buffer count is read (question 3), the client is marked as streaming, and the **stream
     start** ack is sent.
  - then one **stream data**: the gamestate if the recorder is active, or an empty one (suspended)
     if not. If the gamestate does not fit in 32 KiB: dropped, "gamestate overflowed".
- **Stream stop**: only while streaming, else dropped, "unexpected stream stop message". The client
  goes back to the "hello done" state and gets the **stream stop** ack.
- **Ping**: ignored before the hello; afterwards answered with **pong** (and a deflate sync flush).
- **String command**:
  - before the hello: dropped, "unexpected stringcmd message".
  - ignored when there is no dummy or the string-commands flag was not accepted (question 17).
- **Any other op**: dropped, "unknown command byte".
- **A message whose fields run past its length**: dropped, "read past end of message".
- **Framing faults**: a zero length gives "end of stream"; a length over 256 gives "oversize
  message".

### 3. Ping and pong, timeouts, `sv_mvd_bufsize`, the stream-start buffer count

- **The server never pings.** It only answers a ping with a pong, flushed at once **[read]**.
- **Timeouts** use the general server settings, checked every server loop **[read]**:
  - before the hello (connected, magic or not): dropped with "request timed out" after more than
     `sv_ghostime` (default 6 s) or more than `timeout` (default 90 s) of silence, whichever comes
     first. In practice that is 6 s at the defaults **[derived]**.
  - after the hello (with or without a stream): "connection timed out" after more than `timeout`
     (90 s) since the last whole message from the client.
  - streaming data to a client does not count as hearing from it; only messages it sends do. A
     silent but streaming viewer is therefore dropped after 90 s **[derived]**. The relay pings after
     60 s without sending (question 11), which keeps it alive **[derived]**.
- **After a drop** the connection lingers as a "zombie" until its send buffer is empty, or for
  `zombietime` (default 2 s) at most, then it is closed **[read]**. If deflate was on, the deflate
  stream is finished first **[read]**.
- **Other drop reasons** **[read]**: "EOF from client" (the peer closed), "connection reset by
  peer" (a socket error), "deflate() failed", "deflateInit failed".
- **`sv_mvd_bufsize`** **[read]**:
  - default 2, latched, clamped to 1 to 4 at each hello.
  - the unit is one maximum message, 32 768 bytes, so the default send buffer is 64 KiB per viewer.
  - it holds deflated bytes when deflate is on.
- **A full send buffer**: when a frame (or part of it) does not fit, the viewer is dropped with
  "overflowed" ("TCP client <name>[<addr>] dropped: overflowed"). Nothing is skipped and nothing
  waits **[read]**.
- **The stream-start buffer count** **[read]**:
  - the server takes the larger of the client's short and 10.
  - with deflate on, it counts the frames fed into the deflater since the deflater last produced
     output. When the count goes above that number, the server forces a sync flush.
  - the gamestate, suspend markers, pongs and stream acks are always sync-flushed at once.

  The count therefore bounds how many frames can sit inside the compressor unsent **[derived]**.
  Without deflate it has no effect; every frame is written straight into the send buffer
  **[derived]**.

### 4. Stream stop and restart; map changes; shutdown

- **Map change without a restart** (the game moving to its next map): the TCP connections stay up.
  Each streaming viewer gets a new **stream data** carrying a whole new gamestate, which starts
  with the server-data message **[read]**. No reconnect op is sent **[read]**. The dummy is spawned
  again on the new map and its begin commands run again (question 6) **[read]**.
- **Restart** (a map change that reinitialises the game; `sv_recycle` 1 or above on the next map):
  - every viewer past the hello gets **reconnect** and is then closed **[read]**.
  - viewers still before the hello are just closed **[read]**.
  - `sv_recycle` 2 or above quits the process with the same "reconnect" kind, so viewers get
     **reconnect** there too **[read]**.
- **Shutdown** (`killserver`, quit, a fatal error): viewers past the hello get **disconnect**,
  then are closed **[read]**.
- **A fatal recorder error** (question 10's "Fatal MVD server error" lines) **[read]**:
  - stops any local recording.
  - sends **error** to every viewer past the hello and closes them all.
  - switches the recorder off and removes the dummy.
- **Stream stop and start from the client** do not touch the recorder; they only move the client in
  and out of the set that is sent frames **[read]**. A later stream start gets a fresh gamestate
  **[read]**.

### 5. Suspension; `sv_mvd_suspend_time`; `sv_mvd_disconnect_time`

- **`sv_mvd_suspend_time`** **[read]**:
  - in minutes, default 5, clamped to 0 to 34 560 (24 days).
  - 0 means never suspend.
- **"Active players"** **[read]**:
  - any player slot, other than the dummy, that passes the capture test of question 6
     (`sv_mvd_capture_flags`).
  - spectators, players frozen or chasing someone, and (at the default flags) bots without a fully
     connected slot do not count.
- **The rule**, checked at the start of every server frame while the recorder is on **[read]**:
  - when there is an active player, or the setting is 0, the activity clock is reset, and a
     suspended recorder resumes.
  - when there has been none for longer than the setting, an active recorder suspends.
- **Suspending** **[read]**:
  - every streaming viewer gets one empty **stream data** (sync-flushed).
  - after that nothing is sent: no frames, no prints, no sounds.
  - the local recording, if any, also gets no frames.
- **Resuming** **[read]**:
  - the gamestate is rebuilt and sent as one **stream data** to every streaming viewer.
  - the gamestate is also written into the local recording.
  - frames follow from the next frame.
- **At startup** the recorder starts inactive. It resumes on the first frame with an active player
  (or at once when the setting is 0) **[read]**.
- **Auto-recording** sets `sv_mvd_suspend_time` to 0 when it creates the dummy at a map change, so
  an auto-recorded match is never suspended **[read]**.
- **`sv_mvd_disconnect_time`** **[read]**:
  - in minutes, default 15, clamped to 0 to 34 560.
  - while the recorder is on, a local recording or any streaming TCP viewer keeps it alive.
  - when there has been neither for longer than the setting, the recorder is switched off and the
     dummy dropped and removed. Viewers in the "hello done" but not streaming state stay connected.
  - 0 means never switch off.
  - a later stream start or `mvdrecord` switches it on again (creating a new dummy).

### 6. The dummy client and the stream filters

- **What the dummy is**: a fake client held entirely by the server. It stands for every relay
  spectator: it receives the scoreboard layout, text messages and a free spectator's base player
  state **[read]**. Its client number is written in the gamestate, or -1 when there is none
  **[read]**.
- **`sv_mvd_spawn_dummy`** **[read]**:
  - default 1, not latched.
  - 0 or below: never created ("Dummy MVD client disabled", developer only).
  - 1: created only when the game module declares that it knows the dummy (the game feature bit
     for recorder awareness, value 4). Otherwise not created ("Dummy MVD client not supported by
     game", developer only).
  - 2 or more: always created.

  Without a dummy the stream still runs, with client number -1 **[read]**.
- **Its slot**: the first free reserved slot (the slots above `maxclients` minus
  `sv_reserved_slots`). Failing that, the first free ordinary slot. None free: error "No slot for
  dummy MVD client", and the recorder is not enabled **[read]**.
- **Its user info** **[read]**:
  - name `[MVDSPEC]`, skin `male/grunt`, spectator `1`.
  - `mvdspec` set to the current MVD minor version, and `ip` set to `loopback`.
  - when the game takes "extra user info", the last two go there instead of the main string.
- **Connecting it** **[read]**:
  - the game's connect call may refuse it. Then the error "Dummy MVD client rejected by game:
     <reason>" is printed, with the game's `rejmsg` value, or "Connection refused" when the game set
     none.
  - accepted, it is begun at once (and again on every map change), and is fully in game.
  - every server frame (at 10 Hz) the server runs its command buffer, then calls the game's think
     with an empty command of 100 ms, so the game does not time it out.
- **Dropped by the game**: if anything drops the dummy, that is a fatal recorder error ("dummy
  client was dropped") **[read]**.
- **Its command buffer** **[read]**:
  - up to 1 024 characters.
  - `wait <n>` pauses it for n server frames (at least 1).
  - `cmd <text>` hands the text to the game as the dummy's own client command.
  - `set` and `alias` work as on the console.
  - `play`, `stopsound`, `exec` and `screenshot` are ignored.
  - `record <name>` and `stop` work only when `sv_mvd_autorecord` is on (question 7).
  - an alias is expanded; 16 expansions in one run give a warning naming "runaway alias loop" and
     stop.
  - a word naming a cvar is handled as a console cvar command, so it prints or sets a server cvar.
  - anything else is handed to the game as the dummy's client command.
- **`sv_mvd_begincmd`** **[read]**:
  - default `wait 50; putaway; wait 10; help;` (a pause of about 5 s, close any layout, 1 s, open
     the scoreboard).
  - added to the dummy's buffer each time it is begun: when first created, and at each map change.
  - empty means nothing.
- **`sv_mvd_scorecmd`** **[read]**:
  - default `putaway; wait 10; help;`.
  - while streams are active, if no layout has been sent to the dummy for more than 9 s, it is
     added to the dummy's buffer and the 9 s clock restarts.
  - so at most once every 9 s, and only when the game has stopped sending layouts.
  - empty means never.
- **`sv_mvd_capture_flags`** **[read]**:
  - default 5, not latched.
  - it decides which player slots are captured into the stream (player states, and whether
     unicasts to them are kept).
  - The base rule, in order:
     - the slot's entity must be in use (when the game reports that properly).
     - it must have a client, and a field of view that is not 0.
     - the dummy is always captured.
     - a player whose movement type is spectator is never captured.
     - then the flags apply (below).
     - after them, a frozen movement type, or a set "no prediction" flag, means not captured (taken
      as chasing someone).
  - Bit value 1: the slot must also be a fully connected client in the game. Slots without one,
     such as some bots, are not captured.
  - Bit value 2: a player that is invisible (flagged "no client", or with no model, effects or
     sound) is never captured.
  - Bit value 4: a visible player is always captured, skipping the frozen and no-prediction tests.
     An invisible one goes on to those tests unless bit 2 refused it.
- **`sv_mvd_allow_stufftext`** **[read]**:
  - default 0, latched.
  - above 0: a reliable text command the game sends to the dummy is added to the dummy's command
     buffer, up to 1 024 characters. This is how a game can drive `record` and `stop`.
  - 0 or above: the string-commands flag is refused in every hello.
  - below 0: stufftext is ignored, but relay viewers' forwarded commands are accepted (question 17).
- **`sv_mvd_noblend`** (default 0, latched): player states in the stream leave out the screen blend
  **[read]**.
- **`sv_mvd_nogun`** (default 0, latched): player states leave out the held weapon's model index
  and its frames **[read]**.
- **`sv_mvd_nomsgs`** (default 1, latched) **[read]**:
  - only while a dummy exists: print messages unicast to anyone other than the dummy are left out
     of the stream, and the stream's "messages left out" flag (bit 0) is set in the gamestate.
  - broadcast prints still go in.
- **Filters that always apply**, whatever the settings **[read]**:
  - unicasts to players that are not captured are dropped.
  - unicast text commands are dropped unless they start with `play `.
  - while a dummy exists, layouts unicast to anyone but the dummy are dropped.
  - a unicast or multicast payload of 2 048 bytes or more is dropped with a warning.
  - nothing at all is added while the recorder is inactive or suspended.

### 7. Recording on the game server

- **`mvdrecord [-h] [-z] <filename>`** **[read]**. Long forms: `--help`, `--compress`.
  - `-h` prints "Usage: mvdrecord [-hz] <filename>", then "Begin local MVD recording.", then a
     blank line, "Available options:" and one line per option ("-h | --help | display this
     message", "-z | --compress | compress file with gzip"), then a blank line.
  - Not in a game (no map loaded): "No server running."
  - No name: "Missing filename argument." then "Try 'mvdrecord --help' for more information."
  - Recorder disabled (`sv_mvd_enable` 0): "MVD recording is disabled on this server."
  - Already recording: "Already recording a local MVD."
  - The file:
     - it is `demos/<filename>.mvd2`, with `.mvd2` added unless the name already ends in it.
     - `-z` adds `.gz` and gzips it.
     - the name may not climb out of `demos/`.
     - an existing file is overwritten.
     - failing to open prints the error "Couldn't open <name>: <error>".
  - Then the recorder is switched on (dummy created if needed); failure closes the file silently
     after the dummy's own error line.
  - Success: "Recording local MVD to <path>".
  - The file starts with the magic. The gamestate follows at once if the recorder is active, or at
     the next resume if not.
- **With no name** it refuses, as above **[read]**.
- **On a relay** (the server running as a relay), the same command records a channel; see
  question 12 **[read]**.
- **`mvdstop`** **[read]**:
  - "Not recording a local MVD." or "Stopped local MVD recording.".
  - stopping writes the end marker (a zero length word) and closes the file.
- **`mvdstuff <text>`** **[read]**:
  - the raw argument text, outer quotes stripped, is added to the dummy's command buffer, plus a
     line end.
  - with no dummy: "Can't 'mvdstuff', dummy MVD client is not active".
- **`sv_mvd_maxsize`** **[read]**:
  - in kilobytes of 1 000 bytes, default 0 (no limit), clamped to 0 to 2 000 000 (2 GB).
  - checked after each frame is written: when the file position passes the limit, "Stopping MVD
     recording, maximum size reached." and the recording stops.
- **`sv_mvd_maxtime`** **[read]**:
  - in minutes, default 0 (no limit), turned into frames at 600 a minute.
  - its clamp's upper end is 3 456 000 minutes, which looks like a slip for 24 days but is what
     the code does.
  - counted in frames written; past the limit, "Stopping MVD recording, maximum duration reached."
  - frames are not written while suspended, so suspended time does not count **[derived]**.
- **`sv_mvd_maxmaps`** **[read]**:
  - default 1, not clamped.
  - at each map change during a recording a counter goes up. When it reaches the setting (if the
     setting is above 0): "Stopping MVD recording, maximum number of level changes reached.".
  - so the default stops a recording at the first map change. 0 never stops.
  - Otherwise the new map's gamestate is written into the same file, if the recorder is active.
- **A write failure** gives the error "Couldn't write local MVD: <error>" and stops the recording
  **[read]**.
- **`sv_mvd_autorecord`** **[read]**:
  - default 0, latched.
  - when on, at each map change the server creates the dummy if it does not exist yet. When it does
     create one, `sv_mvd_suspend_time` is set to 0 (developer line "Spawning MVD dummy for
     auto-recording").
  - the dummy's `record <name>` and `stop` commands then work:
     - `record` with no name prints "Usage: record <filename>".
     - otherwise the same refusals as `mvdrecord`.
     - success prints "Auto-recording local MVD to <path>".
     - `stop` prints "Not recording a local MVD." or "Stopped local MVD auto-recording.".
     - the file is always `demos/<name>.mvd2`, never gzipped.
  - **The engine picks no file name itself.** The name comes from whatever sends `record` to the
     dummy: the game through a text command (which needs `sv_mvd_allow_stufftext` above 0),
     `mvdstuff`, or `sv_mvd_begincmd`.

### 8. The access lists

- **The lists**: `addgtvhost`, `delgtvhost` and `listgtvhosts` keep the allow list.
  `addgtvban`, `delgtvban` and `listgtvbans` keep the ban list. They work like the server's UDP
  `addban` family, on separate lists **[read]**.
- **`add… <address[/mask]> [comment]`** **[read]**:
  - the address is IPv4 or IPv6, without a port.
  - the mask is a bit count, defaulting to the whole address (32 or 128).
  - the comment is the rest of the line.
  - messages: "Usage: <cmd> <address[/mask]> [comment]", "Please specify a mask after '/'.", "Bad
     address: <text>", "Bad mask: <n> bits", "Entry <address>/<bits> already exists.".
  - success prints nothing.
- **`del… <address[/mask]|id|all>`** **[read]**:
  - `all` empties the list.
  - a plain number is the 1-based row number.
  - messages: "Usage: <cmd> <address[/mask]|id|all>", "Address list is empty.", "No such index:
     <n>", "No such entry: <text>".
  - success prints nothing.
- **`list…`** **[read]**:
  - empty: "Address list is empty.".
  - otherwise a header "id address/mask       hits last hit     comment", a rule line, then one
     row per entry: the 1-based id, address/bits, the hit count, the last hit's local time (day,
     month name, hours:minutes) or "never", and the comment.
- **Hits**: every successful match while checking a hello counts a hit and stamps the time
  **[read]**.
- **Persistence**: the lists live in memory only. They survive map changes and restarts within
  one process, but not a quit **[read for in-memory; derived for the restart part, since nothing
  clears them at shutdown]**. To keep them across runs an admin puts the commands in a config
  **[derived]**.

### 9. Every `sv_mvd_*` setting

| Setting | Default | Flags | Range and unit |
| --- | --- | --- | --- |
| `sv_mvd_enable` | 0 | latched | 0, 1, 2 (question 1) |
| `sv_mvd_maxclients` | 8 | latched | clamped 1 to 256 when the recorder starts; TCP viewer slots |
| `sv_mvd_bufsize` | 2 | latched | clamped 1 to 4 at each hello; units of 32 768 bytes |
| `sv_mvd_password` | empty | private | compared exactly |
| `sv_mvd_maxsize` | 0 | none | kilobytes (1 000 bytes), clamped 0 to 2 000 000; 0 means no limit |
| `sv_mvd_maxtime` | 0 | none | minutes, clamped 0 to 3 456 000; 0 means no limit |
| `sv_mvd_maxmaps` | 1 | none | map changes; 0 or below means no limit; no clamp |
| `sv_mvd_noblend` | 0 | latched | boolean |
| `sv_mvd_nogun` | 0 | latched | boolean |
| `sv_mvd_nomsgs` | 1 | latched | boolean |
| `sv_mvd_begincmd` | `wait 50; putaway; wait 10; help;` | none | dummy command text |
| `sv_mvd_scorecmd` | `putaway; wait 10; help;` | none | dummy command text |
| `sv_mvd_autorecord` | 0 | latched | boolean |
| `sv_mvd_capture_flags` | 5 | none | bit mask, values 1, 2, 4 |
| `sv_mvd_disconnect_time` | 15 | none | minutes, clamped 0 to 34 560; 0 means never |
| `sv_mvd_suspend_time` | 5 | none | minutes, clamped 0 to 34 560; 0 means never |
| `sv_mvd_allow_stufftext` | 0 | latched | below 0, 0, above 0 (question 6) |
| `sv_mvd_spawn_dummy` | 1 | none | 0 or below, 1, 2 or more (question 6) |

All [read]. "Private" means it is not shown in server info and not given to the game module's
reads **[read]**.

Related general settings the TCP side uses **[read]**:

- `timeout` (90 s).
- `sv_ghostime` (6 s).
- `zombietime` (2 s).
- `sv_iplimit` (3).
- `sv_reserved_slots`.
- `net_port` (27910), `net_ip`, `net_ip6` and `net_enable_ipv6`.

### 10. The admin log lines

Normal console prints, recorder **[read]**:

- "Recording local MVD to <path>": `mvdrecord` started.
- "Stopped local MVD recording.": `mvdstop`.
- "Not recording a local MVD.": `mvdstop`, or the dummy's `stop`, with nothing recording.
- "Auto-recording local MVD to <path>": the dummy's `record`.
- "Stopped local MVD auto-recording.": the dummy's `stop`.
- "MVD recording is disabled on this server.": a record attempt with `sv_mvd_enable` 0.
- "Already recording a local MVD."
- "No server running.": `mvdrecord` outside a game.
- "Missing filename argument." (followed by the hint line).
- "Begin local MVD recording.": the `-h` text.
- "Stopping MVD recording, maximum size reached."
- "Stopping MVD recording, maximum duration reached."
- "Stopping MVD recording, maximum number of level changes reached."
- "Can't 'mvdstuff', dummy MVD client is not active" (no full stop).
- "Usage: record <filename>": the dummy's `record` with no name.

Errors, shown in red **[read]**:

- "No slot for dummy MVD client"
- "Dummy MVD client rejected by game: <reason>"
- "Couldn't write local MVD: <error>"
- "Fatal MVD server error: <reason>", where the reason is one of "reliable message overflowed",
  "frame overflowed" (the reliable part plus the frame reached 32 KiB), "gamestate overflowed" or
  "dummy client was dropped".
- "Error opening server TCP port."
- "Server TCP port already in use."

Warnings, shown in yellow **[read]**:

- "Unreliable MVD datagram overflowed.": the unreliable part was discarded.
- "Dumping unreliable MVD datagram.": the reliable part plus the frame plus the unreliable part
  would reach 32 KiB, so the unreliable part is discarded for this frame.
- An "overflow" warning, prefixed with an internal name, when a unicast or multicast payload is
  2 048 bytes or more. Also "leafnum out of range" with the same prefix.

TCP side **[read]**:

- "Accepted MVD client <name>[<addr>]"
- "TCP client <name>[<addr>] dropped: <reason>". Logged only for connections that got past the
  magic, and not when every client is dropped at once at shutdown or on a fatal error. The reasons
  are "duplicated hello message", "send buffer not empty", "bad protocol version", "not
  authorized", "deflateInit failed", "unexpected stream start message", "couldn't create MVD
  dummy", "gamestate overflowed", "unexpected stream stop message", "unexpected stringcmd message",
  "end of stream", "oversize message", "unknown command byte", "read past end of message",
  "overflowed", "deflate() failed", "request timed out", "connection timed out", "EOF from client"
  and "connection reset by peer". The name is empty before the hello.
- "TCP client [<addr>] rejected: too many connections"
- "TCP client [<addr>] rejected: no free slots"

**The `status` command** prints the TCP viewers after the UDP clients **[read]**:

- "No TCP clients." when there are none.
- otherwise the header "num name             buf lastmsg address               state", a rule
  line, and one row per connection:
  - the index from 0.
  - the name (16 columns).
  - the bytes waiting in the send buffer.
  - the milliseconds since the last message from it.
  - the address.
  - a state word: `ZMBI` (dropped, draining), `ASGN` (accepted, no magic yet), `CNCT` (magic seen,
     no hello), `PRIM` (hello done, not streaming) or `SEND` (streaming).
- When `status` is given any argument, the TCP part becomes the versions table instead: header
  "num name             version", then index, name and the version string from the hello (40
  columns).

Developer-only lines (shown only with developer output on) **[read]**:

- "Enabling server MVD recorder." and "Disabling server MVD recorder."
- "Suspending MVD streams." and "Resuming MVD streams."
- "Dummy MVD client disabled" and "Dummy MVD client not supported by game"
- "Spawning MVD dummy for auto-recording"
- "TCP client [<addr>] accepted"
- "ignored stringcmd from <name>[<addr>]" and "dummy stringcmd from <name>[<addr>]: <text>"
- "dummy cmd: <text>", "dummy forward: <text>" and "dummy stufftext: <text>"

---

## B. The relay (the MVD client and player)

The relay is the same q2pro server binary, switched into a mode (server state "broadcast") where
its built-in relay game replaces the game module **[read]**. Ordinary clients connect to it over
UDP **[read]**.

### 11. `mvdconnect` and the relay's connection settings

- **Form**: `mvdconnect [-h] [-n <string>] [-u <string>] [-p <string>] <address[:port]>` **[read]**.
  - Long forms: `--help`, `--name`, `--user`, `--pass`.
  - `-h` prints "Usage: mvdconnect [-hn:u:p:] <address[:port]>", then "Connect to the specified
     MVD/GTV server.", then the options block.
  - The port defaults to 27910.
  - `-n` names the channel, `-u` and `-p` override `mvd_username` and `mvd_password` for this
     connection.
- **Its messages** **[read]**:
  - "Missing address argument." (plus the hint line).
  - "Bad server address: <text>".
  - "[<name>] <warn> Connection to <addr> already exists." (same address and port as an existing
     connection).
  - the error "Unable to connect to <addr>" when the socket cannot even start.
  - success: "[<name>] -=- Connecting to <addr>...".
- **The connection's id and name**: the id comes from a counter starting at 0, shared with
  `mvdplay`. The default name is `net<id>` **[read]**. The name is cut to 15 characters **[read]**.
- **The handshake** (relay console lines in quotes) **[read]**:
  1. The TCP connect completes: "[<name>] -=- Connected to the game server!". The relay sends the
     magic.
  2. The magic comes back: "[<name>] -=- Sending client hello...". The hello carries:
     - version 0xED04.
     - flags: string commands always, plus deflate when built with zlib.
     - reserved 0.
     - the user name (the `-u` value, else `mvd_username`), the password (the `-p` value, else
       `mvd_password`), and the relay's version string.
  3. The server's hello: "[<name>] -=- Server hello done.". The relay keeps the accepted flags. If
     deflate was accepted, the relay inflates everything that follows. If the relay was not yet
     running as a relay, it starts relay mode now (a game init plus the Waiting Room). If it was,
     and this connection already had a channel, the channel's viewers get "[MVD] Restored
     connection to the game server!".
  4. When the relay is active (below) and the connection is idle: "[<name>] -=- Sending stream
     start request...". The buffer count it asks for is half the channel's current wait target in
     frames, at least 10. With no channel yet it is half of `mvd_wait_delay` in frames: 100 at the
     default.
  5. The ack: "[<name>] -=- Stream start ack received."
  6. Stream data:
     - an empty one while reading: "[<name>] -=- Stream suspended by server.".
     - a non-empty one while suspended: "[<name>] -=- Stream resumed by server.".
     - the first data creates the channel (question 12).
     - until the first gamestate, data is parsed at once; after it, every message goes into the
       delay buffer.
  7. When the relay goes inactive while reading or suspended: "[<name>] -=- Sending stream stop
     request...", then "[<name>] -=- Stream stop ack received.". Data arriving between the request
     and the ack is discarded.
- **On each server op** **[read]**:
  - **hello**: as step 3. A second one destroys the connection, "Duplicated server hello".
  - **pong**: nothing.
  - **stream start and stream stop acks**: as above. An ack in the wrong state destroys the
     connection, "Unexpected stream start ack in state <n>" or "Unexpected stream stop ack in state
     <n>", with the state's number.
  - **stream data** before the stream was started: destroy, "Unexpected stream data packet".
  - **error**: destroy, "Server side error occurred."
  - **bad request**: destroy, "Server refused to process our request."
  - **no access**: destroy, "You don't have permission to access MVD/GTV stream on this server."
  - **disconnect**: destroy, "Server has been shut down."
  - **reconnect**: drop and retry, "Server has been restarted."
  - an unknown op: destroy, "Unknown command byte".
- **"Drop" and "destroy"** **[read]**:
  - a **drop** prints "[<name>] <warn> <reason>" and keeps the connection for a retry.
  - a **destroy** prints "[<name>] =X= <reason>" and removes the connection for good.
- **Other faults** **[read]**:
  - Drops: a zero length, "End of MVD/GTV stream"; a socket error, "<error text> to <addr>"; the
     peer closing, "Server has closed connection."; no data, "Server connection timed out."
  - Destroys: the first 4 bytes not the magic, "Not a MVD/GTV stream"; a length over 32 768,
     "Oversize message"; fields past the end, "Read past end of message"; the relay's 256-byte send
     buffer full, "Send buffer overflowed"; inflate failing, "inflate() failed with error <n>" or
     "inflateInit() failed: <text>".
- **Retries and backoff** **[read]**:
  - the backoff starts at 0.
  - a drop before the TCP connect completed adds 15 s.
  - a drop after connecting:
     - the channel's viewers get "[MVD] Lost connection to the game server!".
     - if the server's hello had been received, the backoff is set to 5 s.
     - otherwise it grows by 30 s.
  - the backoff is capped at 5 hours.
  - the console prints "[<name>] -=- Reconnecting in <time>.", with the time written in words:
     "0 secs", or for example "1 min, 5 secs", "2 hours" (days, hours, mins and secs, each made
     singular for 1).
  - once the backoff has passed: "[<name>] -=- Attempting to reconnect to <address as typed>...".
  - a name lookup failing drops again, "Unable to lookup <address>".
  - a connect failing drops again, "Unable to connect to <addr>".
  - So a server that stays down is retried after 15 s, 30 s, 45 s and so on, up to 5 hours
     **[derived]**.
- **Destroying a connection** **[read]**:
  - the channel's viewers get "[MVD] Disconnected from the game server!" (only if the channel had
     reached its gamestate).
  - the channel itself keeps playing what is left in its delay buffer, then is destroyed with "End
     of MVD stream reached".
- **A dropped connection's channel** **[read]**:
  - it plays on from its buffer.
  - when the buffer runs dry it is destroyed with "Ran out of buffers", but the connection is kept.
  - the next stream after a reconnect creates a new channel with the same id and name.
- **Destroying a channel** **[read]**:
  - the relay console prints "[<channel>] =X= <reason>".
  - on a dedicated relay the channel's viewers get "[MVD] <reason>".
  - all its viewers are moved to the Waiting Room.
- **`mvd_timeout`** **[read]**:
  - seconds, default 90, clamped to 0 to 2 073 600 (24 days).
  - the connection is dropped when nothing has been received for longer than this.
  - the clock also restarts at each connect attempt.
- **Pings**: when the relay has sent nothing for 60 s it sends a ping. It also pings at once
  whenever a channel's buffer runs dry, to make the server flush **[read]**.
- **`mvd_suspend_time`** **[read]**:
  - minutes, default 5, clamped to 0 to 34 560.
  - the relay is "active" while there has been viewer activity within that time; 0 means always
     active.
  - activity is any of these: a client connecting, any client command, a change in buttons or move
     keys, and every frame in which a viewer chases a real player.
  - the relay starts inactive. It goes active at the first activity, or at once when the setting
     is 0.
  - while active, every connection in the idle state asks for a stream. While inactive, every
     streaming connection stops its stream.
  - the Waiting Room's chooser then shows the traffic-saving text (question 13).
- **`mvd_wait_delay`** **[read]**:
  - seconds, default 20, clamped to 0 to 3 600, used as frames (10 a second: 200 at the default).
  - a new channel waits until that many frames are buffered before playing. This is the relay's
     delay behind the live game **[derived]**.
- **`mvd_wait_percent`** **[read]**:
  - default 50, a percentage of the delay buffer, not clamped.
  - waiting also ends when the buffer is at least this full.
  - so the delay is the smaller of `mvd_wait_delay` and what fits in that share of the buffer
     **[derived]**.
- **When waiting ends** **[read]**:
  - by count: "[<channel>] -=- Waiting finished, reading...".
  - by fill: "[<channel>] -=- Buffering finished, reading...".
  - either way the viewers get "[MVD] Streaming resumed.".
  - if the connection is not streaming, waiting ends as soon as one message is buffered, and a
     channel with nothing buffered is destroyed with "Ran out of buffers".
- **Running dry while playing** **[read]**:
  - "[<channel>] -=- Buffering data...", and the viewers get "[MVD] Buffering data, please
     wait...".
  - the new wait target is 50 frames plus 5 for each earlier underflow, but never above
     `mvd_wait_delay` in frames.
  - a ping is sent.
  - if the connection is no longer streaming, the channel is destroyed with "Ran out of buffers".
  - if there is no connection at all, it is destroyed with "End of MVD stream reached".
- **`mvd_buffer_size`** **[read]**:
  - default 8, clamped to 2 to 256 when a channel is created.
  - units of 32 768 bytes, so 256 KiB at the default.
  - it is the delay buffer each network channel keeps.
- **A full delay buffer** **[read]**:
  - "[<channel>] <warn> Delay buffer overflowed!", and the viewers get "[MVD] Delay buffer
     overflowed!".
  - the buffer is emptied and the channel goes back to waiting, with a target of 50 frames.
  - a stream stop is sent. The stream is started again when the ack comes and the relay is active
     **[derived]**.
  - an overflow while already waiting destroys the connection, "Delay buffer overflowed in waiting
     state".
- **`mvd_username`** (default `unnamed`) and **`mvd_password`** (default empty, private): the
  name and password sent in every hello, unless `-u` or `-p` was given **[read]**.
- **`mvd_snaps`** **[read]**:
  - default 10, in seconds of play.
  - only for played files (`mvdplay`): every that many seconds the player keeps a snapshot (the
     entity and player states, changed configstrings, per-player configstrings and the scoreboard)
     with its file position, so `mvdseek` can jump back.
  - 0 or below keeps none, so seeking backwards is refused.

### 12. Channels and the operator commands

- **A channel** is one stream being played to viewers. It is fed by one `mvdconnect` connection or
  by one `mvdplay` playlist **[read]**.
  - It has the id of its connection or playlist (one shared counter from 0) and a name (`net<id>`,
     `dem<id>` or the `-n` value) **[read]**.
  - It joins the channel list only once its first gamestate arrives, kept in id order **[read]**.
  - Its state is DEAD (no gamestate yet), WAIT (buffering or paused) or READ **[read]**.
  - The Waiting Room is a separate place, not in the list and with no id **[read]**.
- **Naming a channel** (`[chanid]` below) **[read]**:
  - a number matches the id, any other text the exact name.
  - with no argument and exactly one channel, that channel is used.
  - on a listen relay, `@@` means the channel the local player is on.
  - messages: "No active channels.", "Please specify an exact channel ID.", "No such channel ID:
     <text>".
- **Naming a connection** (`[conn_id]`) works the same, with the messages "No GTV connections.",
  "Please specify an exact connection ID." and "No such connection ID: <text>" **[read]**.
- **`mvdchannels [r]`** **[read]**:
  - "No MVD channels." when there are none.
  - otherwise the header "id name         map      spc plr stat buf pckt address", a rule line,
     and one row per channel:
     - the id, the name (12 columns) and the map (8).
     - spectators on it and players in the stream.
     - DEAD, WAIT or READ.
     - the buffer: the delay buffer's fill in percent, or for a played file its progress in percent.
     - the packets buffered.
     - the connection's address as typed (for a file, the file's name), or "<disconnected>".
  - An argument starting with `r` lists recordings instead: header "id name         map      size
     name", with the file's size in short form, or "-", and the file's name, or "-".
- **`mvdservers`** **[read]**:
  - "No GTV connections." when there are none.
  - otherwise the header "id name         state        ratio lastmsg address", a rule line, and
     one row per connection:
     - the id and the name.
     - the state: disconnected, connecting, preparing, connected, resuming, waiting, reading or
      suspending.
     - the ratio: compressed bytes in per 100 bytes out, shown with a percent sign, 100% when not
      deflating.
     - milliseconds since the last message.
     - the address.
- **`mvdcontrol [-h] [-l <number>] [-n <string>] [chanid]`** **[read]**:
  - "Change attributes of existing MVD channel." in its help.
  - with no option: "At least one option needed." plus the hint line.
  - a negative `-l`: "Invalid value for l option." The text names the option as given, so it
     may be a long name.
  - `-n` renames the channel: "[<old>] Channel renamed to <new>.".
  - `-l` is accepted and checked but changes nothing in this release.
- **`mvdisconnect [-a] [-h] [conn_id]`** **[read]**:
  - "Destroy specified MVD/GTV server connection." in its help.
  - `-a`: "No GTV connections." or "Destroyed all GTV connections.".
  - otherwise "[<name>] =X= Connection destroyed.".
  - the channel plays out its buffer (question 11).
- **`mvdkill [chanid]`** **[read]**:
  - "[<name>] =X= Channel was killed.".
  - the viewers go to the Waiting Room and its connection or playlist is destroyed too.
- **`mvdspawn`** **[read]**:
  - takes no arguments.
  - starts relay mode by hand: the server restarts as a relay with the Waiting Room, before any
     stream exists.
  - (`mvdconnect`'s hello and `mvdplay`'s first gamestate also start relay mode on their own when
     it is not running.)
- **`mvdpause [chanid]`** **[read]**:
  - played files only, else "[<name>] Only demo channels can be paused.".
  - toggles between playing and paused, with no message.
  - viewers stay connected, frozen.
- **`mvdskip [chanid] [count]`** **[read]**:
  - "Usage: mvdskip [chan_id] [count]" when no channel resolves.
  - the count defaults to 1 (anything below 1 counts as 1).
  - played files only, else "[<name>] Maps can be skipped only on demo channels.".
  - at the next frame: "[<name>] -=- Skipping map..." (or "maps...") and the player reads on to the
     count-th next gamestate.
  - running off the end goes to the next file.
- **`mvdseek [+-]<timespec|percent>[%] [chanid]`** **[read]**:
  - "Usage: mvdseek [+-]<timespec|percent>[%] [chanid]" with no argument.
  - played files only, else "[<name>] Seeking is only supported on demo channels.".
  - refused while that channel is being recorded: "[<name>] Seeking is not yet supported during
     demo recording, sorry.".
  - a percentage (ending in `%`) seeks by file position. Faults: "[<name>] Invalid percentage." and
     "[<name>] Unknown file size, can't seek.".
  - the time is written `S`, `S.F`, `M:S` or `M:S.F`, where F is a count of tenths added as frames
     (so `5.3` is 53 frames, but `5.12` is 62).
  - a leading `+` or `-` is relative to the current frame; without one, the time counts from the
     first frame of the current map.
  - faults: "Invalid relative timespec." and "Invalid absolute timespec.".
  - seeking to where it already is does nothing.
  - going back uses the nearest earlier snapshot. With none: "[<name>] Couldn't seek backwards
     without snapshots!".
  - meeting a new map's gamestate while seeking stops the seek there.
  - a file error: the error "[<name>] Couldn't seek demo: <error>".
- **`mvdplay [-h] [-l <number>] [-n <string>] [-r <chanid>] [/]<filename> [...]`** **[read]**:
  - its help adds "Create new MVD channel and begin demo playback.", "Final path is formatted as
     demos/<filename>.mvd2." and "Prepend slash to specify raw path.".
  - each file is looked up as `demos/<name>`, then `demos/<name>.mvd2`, and may be gzipped. Files
     that fail print "Couldn't open <name>: <error>" and are left out. If none is left, nothing
     happens.
  - `-l` is the play count: the list plays that many times and then the channel ends, "End of play
     list reached". 0 loops forever, the default is 1, a negative value gives "Invalid value for
     <option> option.".
  - `-n` names the channel (default `dem<id>`).
  - `-r <chanid>` replaces that played channel's playlist instead of making a new channel.
  - per file the console prints "[<name>] -=- Reading from <path>".
  - faults destroy the channel: "Couldn't open <path>: <error>", "Couldn't read <path>: <error>",
     "First message of <path> does not contain gamestate".
- **Recording on the relay**: `mvdrecord [-h] [-z] <filename> [chanid]` and `mvdstop [chanid]`
  **[read]**.
  - "Begin MVD recording on the specified channel." in its help.
  - Messages:
     - "[<name>] Already recording into <path>."
     - "[<name>] Recording into <path>"
     - "[<name>] Not recording a demo."
     - "[<name>] Stopped recording."
     - "Usage: mvdstop [chanid]" when no channel resolves.
     - The error "[<name>] Couldn't write demo: <error>".
  - The file is the channel's current state as a gamestate, then every message as it is played.
     Unlike the server's recorder, the scoreboard and per-player configstrings are not in the
     starting gamestate.
- **Every channel line on the relay console** is prefixed "[<channel name>]". The marker "-=-"
  means progress, <warn> (question 11) a recoverable fault and "=X=" the end of a connection or channel
  **[read]**.
- **Server info on a relay** **[read]**:
  - `gamename` is `gtv` and `protocol` is 34.
  - `mvd_channels` and `mvd_players` hold the channel count and the total players across channels.
  - `mapname` is the Waiting Room's map.

### 13. How an ordinary client joins the relay; the Waiting Room

- **Joining**: a viewer connects to the relay's UDP port like any server (protocols 34 to 36)
  **[read]**.
  - If there is exactly one channel and the viewer's client can show it, the viewer is put
     straight on it. Otherwise the viewer goes to the Waiting Room **[read]**.
  - A channel from an extended-limits stream needs a q2pro client of a version new enough for
     those limits, otherwise the client is "not compatible" with it **[read]**.
- **The Waiting Room** **[read]**:
  - it is not a channel. Its map is `mvd_default_map` (default `q2dm1`, latched), loaded from
     `maps/<name>.bsp`.
  - the viewer stands frozen at the map's spectator spawn point.
  - if the map cannot be loaded, the error "Couldn't load maps/<name>.bsp for the Waiting Room:
     <error>" is printed, the setting is reset to `q2dm1`, and a fixed point in q2dm1 is used.
  - its level name is "Waiting Room", its sky `unit1_`, maxclients 8.
- **The layout shown there is the Channel Chooser** **[read]**:
  - the inventory backdrop.
  - the relay's version string near the bottom right.
  - a centred title "Channel Chooser" in brackets.
  - the column header "Name         Map     S/P" over a dashed rule.
  - one row per channel: a cursor mark on the selected row, the name (12), the map (7), and
     spectators/players. The viewer's own channel is in the highlighted font. At most about 13 rows
     fit.
  - With no channels, and the relay active: the lines " No active channels.", " Please wait until
     players" and " connect.".
  - With no channels and the relay inactive: " Traffic saving mode.", " Press any key to wake" and
     " this server up.".
  - It is redrawn when the channel list changes.
- **Keys in the chooser**: the inventory next and previous keys move the cursor, wrapping. The
  inventory use key joins the selected channel **[read]**.
- **Entering a channel**:
  - The relay sends the viewer the text command "changing map=<map>; reconnect". The viewer's
     client reconnects, which loads the channel's map, and is then begun there **[read]**.
  - The same happens to everyone on a channel at each of its map changes, where viewers keep
     their chase target **[read]**.
- **On begin in a channel** **[read]**:
  - the others there get "[MVD] <name> entered the channel" (subject to `mvd_part_filter`, and not
     to viewers ignoring connect messages).
  - the viewer starts chasing the most followed player, or flies free from the map's spectator
     spawn point if no one is playing.
  - during an intermission the viewer chases the dummy and sees the scoreboard.
  - after 2 s, a viewer not local and not in the Waiting Room is told "[MVD] GTV server doesn't
     have this map!" when the relay lacks the map's file (no visibility data), and "[MVD] Buffering
     data, please wait..." when the channel is still waiting.
- **Commands** (sent as ordinary client commands) **[read]**:
  - **`channels`** prints to the viewer the header "id name         map      spc plr who is
     playing", a rule line, then per channel: the id, the name (12), the map (8), the spectator
     count, the player count, and the players' names joined by ", ".
  - **`clients`** (also `players`) opens a layout listing the viewers on the same channel. Its
     header is "    Name            RTT Status". Each row has the number, the name (15), the round
     trip in ms, and "observing" or an arrow (a hyphen and a greater-than sign) followed by the
     chased player's name. 16 rows a page, refreshed every 3 s.
  - **`commands`** prints these nine lines:
     - "chase [player_id]      toggle chasecam mode"
     - "autochase <...>        control automatic chasecam"
     - "observe                toggle observer mode"
     - "menu                   show main menu"
     - "score                  show scoreboard"
     - "oldscore               show previous scoreboard"
     - "channels               list active channels"
     - "join [channel_id]      join specified channel"
     - "leave                  go to the Waiting Room"
  - **`join [id or name]`** resolves like `[chanid]`, printing the resolution messages ("No active
     channels." and so on) to the viewer. Then:
     - a channel without its gamestate: "[MVD] This channel is not ready yet.".
     - otherwise the switch rules below.
  - **`leave`** goes to the Waiting Room by the same rules.
  - **`help`** (also `score`) toggles the scoreboard layout.
- **The switch rules**, in order **[read]**:
  1. Already there: "[MVD] You are already in the Waiting Room." or "[MVD] You are already on this
     channel."
  2. Client cannot show it: "[MVD] This channel is not compatible with your client version."
  3. Less than 2 s since the viewer last entered a channel: "[MVD] You may not switch channels too
     soon."
  4. Otherwise the old channel hears "[MVD] <name> left the channel" (part filter and connect-mute
     apply), and the viewer is moved with the reconnect text command.

### 14. Viewer commands

Every reply starts "[MVD] " unless shown otherwise. "High" and "medium" are the print levels
**[read]**.

- **`follow`** (also `chase`) **[read]**:
  - in the Waiting Room: "[MVD] Please enter a channel first.".
  - ignored during an intermission.
  - with no argument, the same as `observe`.
  - **a number** is a player slot from 0. Too large: "[MVD] Player slot number <n> is invalid.".
     empty, or the dummy: "[MVD] Player slot number <n> is not active.".
  - **a name**:
     - a case-insensitive exact match wins.
     - otherwise exactly one player whose name contains the text, case-insensitively.
     - none: "[MVD] No players matching '<text>' found.".
     - several: "[MVD] '<text>' matches multiple players.".
  - **`!q`, `!i`, `!r`, `!b`** start powerup chasing: from then on the camera jumps to the first
     player carrying quad damage, invulnerability, the red flag or the blue flag. Only the first
     letter after the `!` counts. The reply is "[MVD] Chasing players with '<text>' powerup.", with
     the text after the `!` (medium level). Auto chasing is switched off.
  - **`!p`** goes back to the previous target. Faults: "[MVD] Previous chase target is not active."
     and "[MVD] You have no previous chase target.".
  - **`!!<name>`** treats the rest, with one `!` dropped, as a player name, so a name starting
     with `!` can be followed.
  - any other `!` text: "[MVD] Unknown chase target '<text>'. Valid targets are:
     q[uad]/i[nvulner]/r[ed_flag]/b[lue_flag]/p[revious_target].".
  - starting to chase anyone prints "[MVD] Chasing <name>." at the low level. The chase layout
     shows "[<channel>] Chasing <name>" (or "<no target>"), placed by `mvd_chase_prefix`.
- **`autofollow`** (also `autochase`) **[read]**:
  - in the Waiting Room "[MVD] Please enter a channel first.", and ignored during an intermission.
  - with no argument: "[MVD] Auto chasing is ON (<n> targets)." or "... is OFF (...)". It says
     "target" for 1.
  - `add <wildcard> [...]` marks every player whose name matches a wildcard (case-insensitive).
     with no wildcard: "Usage: autofollow add <wildcard> [...]" (the command word as typed). Adding
     also moves to a marked player at once when auto chasing is on.
  - `rm` or `del <wildcard> [...]` unmarks; removing the last one switches auto chasing off.
  - `set <slot> [...]` replaces the marks with those slot numbers.
  - `list`: "[MVD] Auto chasing <names joined by ', '>" or "[MVD] Auto chase list is empty.".
  - `on`: "[MVD] Please add auto chase targets first." when none is marked. Otherwise it is on: the
     camera only cycles through marked players and moves to the next marked one when its target
     leaves.
  - `off` switches it off.
  - anything else: "[MVD] Usage: autofollow <add|rm|set|list|on|off> [...]".
  - the marks are by slot, and are cleared when the viewer changes channel or the stream's
     maxclients changes.
- **`observe`** (also `spectate`, `observer`, `spectator`, `obs`, `spec`) **[read]**:
  - in the Waiting Room "[MVD] Please enter a channel first.".
  - clears powerup and auto chasing.
  - ignored during an intermission.
  - when chasing: stops and flies free from the chased player's eye, with the dummy's
     configstrings.
  - otherwise it resumes the last target if still playing, else chases the next player. With none:
     "[MVD] No players to chase." (medium).
- **The attack button** acts like `observe`. When free, a player under the crosshair (a straight
  line out to 8 192 units, stopped by walls) is picked first **[read]**.
- **Jump and crouch** while chasing move to the next or previous player, once per press **[read]**.
- **`inven`** (also `menu`) toggles the Main Menu layout **[read]**. Its rows:
  - "Enter in-eyes mode" (or "Leave in-eyes mode" while chasing).
  - "Show scoreboard".
  - "Show spectators (<n>)".
  - "Show channels (<n>)".
  - "Leave this channel".
  - "Ignore spectator chat: <Yes|No>".
  - "Ignore connect msgs:   <Yes|No>".
  - "Ignore player chat:    <Yes|No>".
  - "Ignore player FOV: <Default|No|Yes>".
  - the hint " (use 'set uf <n> u' in cfg)".
  - "Exit menu".
  - "[BUFFERING]" centred near the bottom while the channel waits, and the version string.

  "Yes", "No" and "Default" are in the highlighted font, and a cursor mark shows the selected row.
- **`invnext` and `invprev`** **[read]**:
  - with the menu, the client list or the chooser open, they move the cursor, wrapping (the client
     list pages).
  - otherwise, outside an intermission, they chase the next or previous player and clear powerup
     chasing.
- **`invuse`** on the menu **[read]**:
  - row 1 closes the menu and toggles chasing like `observe`.
  - rows 2 to 4 open the scoreboard, the client list or the chooser.
  - row 5 goes to the Waiting Room.
  - rows 6 to 8 toggle the three ignore settings.
  - row 9 cycles the field of view choice: Default, then Yes (always the viewer's own), then No
     (always the player's), then back. Default uses the viewer's own when the chased player's is 90
     or more, and the player's when it is narrower.
  - row 10 closes the menu.

  When an ignore or field of view setting changes, the relay sends the client "set uf <n> u", so
  its user info keeps the choice. On the chooser, `invuse` joins the selected channel. Elsewhere it
  does nothing.
- **The `uf` user-info bits** **[read]**:
  - 4: own field of view.
  - 8: ignore player chat.
  - 16: ignore spectator chat.
  - 32: ignore connect and part messages.
  - 64: player's field of view.

  The `fov` user-info value is kept between 1 and 160; anything below 1 means 90.
- **`oldscore`** (also `oldscores`, `lastscore`, `lastscores`) toggles the saved scoreboard
  **[read]**:
  - it is saved when a layout for the dummy arrives in the same message as a high-level print
     containing "Match ended.", or exactly "Fraglimit hit." or "Timelimit hit.".
  - an empty scoreboard shows "<no scoreboard>".
- **`help`** (also `score`) toggles the dummy's live scoreboard **[read]**:
  - with no dummy it is resent every 3 s.
  - at intermission it is delivered reliably.
- **`putaway`** goes back to the default layout **[read]**:
  - the chooser in the Waiting Room.
  - the scoreboard during an intermission.
  - the chase line while chasing (unless the stream is single point of view).
  - none otherwise.
- **`say`** and **`say_team`** **[read]**. Any command the relay does not know is also taken as
  chat, with the whole line as the text. The order of checks:
  1. `flood_mute` on and the sender not an admin: "[MVD] Spectators may not talk on this server."
  2. The sender ignores spectator chat: "[MVD] Please turn off ignore mode first."
  3. The flood rule (question 16).

  Then the line "[MVD] <name>: <text>" (cut to 149 characters) goes to every viewer on the same
  channel who is fully in, except those ignoring spectator chat; an admin's line reaches them too.
  q2pro clients new enough get it at chat level. Others get it at high level, in the highlighted
  (high-bit) characters. Nothing goes upstream, and players in the game never see it. Viewers in
  the Waiting Room talk among themselves.
- **Chat from the game**: chat-level prints in the stream reach every viewer on the channel except
  those ignoring player chat. Prints unicast to the dummy (or to the chased player, when the
  stream carries them) go to that viewer, subject to the same ignore flag **[read]**.
- **`!mvdadmin [password]`** (the command word includes the `!`) **[read]**:
  - an admin who sends it loses the status: "[MVD] Lost admin status.".
  - a local client (on a listen relay) is granted without a password.
  - otherwise, with no argument: "Usage: !mvdadmin <password>" (no prefix).
  - an empty `mvd_admin_password`, or a wrong password: "[MVD] Invalid password.".
  - right: "[MVD] Granted admin status.".
- **`fwd <command>`** **[read]**:
  - only for admins: "[MVD] You don't have admin status.".
  - on a played-file channel: "[MVD] This channel does not support command forwarding.".
  - then, from the connection: "[MVD] Not connected to the game server." (no connection, or no
     hello yet), "[MVD] Game server does not allow command forwarding." (the flag was refused) or
     "[MVD] Send buffer not empty, please wait.".
  - otherwise the rest of the line goes upstream (question 17), with no confirmation.
- **Name changes and leaving**, shown to the channel by the part filter and connect-mute rules
  **[read]**:
  - "[MVD] <old> changed name to <new>".
  - "[MVD] <name> disconnected".
  - for drops, the server's usual drop text with the "[MVD] " prefix: "[MVD] <name> was dropped:
     <reason>", or the reason's own wording.
- **Stream state lines**, as broadcast to a channel's viewers **[read]**:
  - "[MVD] Streaming resumed."
  - "[MVD] Buffering data, please wait..."
  - "[MVD] Restored connection to the game server!"
  - "[MVD] Delay buffer overflowed!"
  - "[MVD] Disconnected from the game server!"
  - "[MVD] Lost connection to the game server!"
  - "[MVD] <reason>" when a channel ends, for example "[MVD] End of MVD stream reached", "[MVD] Ran
     out of buffers" or "[MVD] End of play list reached".

  All of them only on a dedicated relay. The ledger's msg.mvd rows are confirmed, with these
  details:
  - the "already" text is either "in the Waiting Room" or "on this channel".
  - autochase's count reads "target" or "targets".
  - the usage line is "[MVD] Usage: <cmd> <add|rm|set|list|on|off> [...]".

### 15. The relay's other settings

- **`mvd_admin_password`**: default empty, private. While empty, no remote viewer can become admin
  **[read]**.
- **`mvd_part_filter`** **[read]**:
  - default 0: every "entered", "left", "changed name", "disconnected" and drop line is shown.
  - below 0: none.
  - above 0, a number of seconds: shown only for admins and for viewers who have chatted within
     that many seconds. A viewer who never talked is never announced. Values above 2 073 600 mean
     "has ever talked".
- **`mvd_filter_version`** **[read]**:
  - default 0.
  - when on, every "!version" inside a chat-level line broadcast to a channel (chat coming from
     the game) is rewritten to "!versi0n", so viewers' clients do not answer it.
  - viewers' own `say` lines are not filtered.
- **`mvd_default_map`**: default `q2dm1`, latched; the Waiting Room's map (question 13) **[read]**.
- **`mvd_stats_score`** **[read]**:
  - default 0: the frags stat sent to every viewer is forced to 0.
  - 1: it shows the channel's id.
  - any other value: the chased player's own frags (or the dummy's when free).
- **`mvd_stats_hack`** **[read]**:
  - default 0.
  - a bit mask over the first 32 stats: while chasing a real player, each stat whose bit is set is
     taken from the dummy instead of the chased player.
- **`mvd_freeze_hack`** **[read]**:
  - default 1: when the dummy's movement type is frozen, the channel is taken to be in
     intermission. Everyone is moved onto the dummy's view with the scoreboard, and moved back when
     it unfreezes.
  - 0: no intermission handling.
- **`mvd_chase_prefix`** **[read]**:
  - default `xv 0 yb -64`.
  - the layout position text put before the chase line (question 14).
- **`mvd_shownet`** **[read]**:
  - default 0, present only in debug builds.
  - 1: each message's size.
  - 2: each op in the stream.
  - 3: also the entity and player field bits.
  - -1: per read, "[<name>] <n> bytes, <n> msgs" for each connection.
  - all but -1 print at developer level.
- **No ranges are enforced** on these, beyond the cases named **[read]**.

### 16. Chat flood on the relay

- **The settings**, registered by the relay game **[read]**:
  - `flood_msgs`: default 4, clamped to 0 to 15 each time it is used.
  - `flood_persecond`: seconds, default 4.
  - `flood_waitdelay`: seconds, default 10.
  - `flood_mute`: default 0.

  These are the same names the game modules usually use, here read by the relay.
- **The rule** **[read]**:
  - each line spoken is stamped in a 16-entry ring.
  - a new line is refused when the stamp `flood_msgs` + 1 lines back is less than
     `flood_persecond` seconds old.
  - so at most `flood_msgs` + 1 lines (5 at the defaults) in any `flood_persecond` seconds.
- **A refusal** **[read]**:
  - prints "[MVD] You can't talk for <n> seconds.", where n is `flood_waitdelay` in whole seconds.
  - starts the wait.
  - during the wait every attempt gets "[MVD] You can't talk for <n> more seconds.", with the time
     left rounded down, so the last second shows 0.
  - refused lines are not stamped.
- **The ring and the wait are reset** when the viewer is begun (each channel switch or map change)
  **[read]**.
- **`flood_mute`** at any non-zero value silences all non-admin viewers, with "[MVD] Spectators
  may not talk on this server." **[read]**
- **Admins** are subject to the flood rule too. Only the mute and the ignore-mode check treat them
  differently **[read]**.

### 17. Forwarding a viewer's command upstream, and what the game server does with it

- **The relay** **[read]**:
  - sends the **string command** op.
  - its payload is the text after `fwd` (the raw rest of the line, cut to 150 bytes), plus a
     terminating zero.
  - it needs the server's hello to have accepted the **string commands** flag (bit 1).
  - it sends only when its send buffer is empty, and expects no reply.
- **The game server** **[read]**:
  - accepts the op only after the hello.
  - when the flag was not accepted, or there is no dummy, the command is ignored, with only a
     developer line.
  - the flag is accepted only when `sv_mvd_allow_stufftext` is negative. At the default 0 forwarding
     never works, and the relay knows it from the hello and tells the viewer so.
- **When accepted** **[read]**:
  - the text is split into words. Quotes group words, but the text is not run as several
     commands, so a semicolon is just a character.
  - it is handed straight to the game module as a client command from the dummy, as if the
     `[MVDSPEC]` client had typed it in its console and sent it with `cmd`.
  - it does not go through the dummy's command buffer, so `wait`, `set`, `record` and the
     others are not special, and it never reaches the server console. Who can act on it is the game
     module's choice, with the dummy's rights. A typical use is the game's own spectator or admin
     commands **[derived]**.
- **Logging**: there is no log line other than the developer line "dummy stringcmd from
  <name>[<addr>]: <text>" **[read]**.

## Open points

- Whether a game module behaves differently for the dummy beyond the recorder-aware feature bit is
  the module's business. This release's server code does not settle it.
- The relay's handling of protocol 34 versus 35 viewers beyond the compatibility test is outside
  this source area (it is the ordinary server's client code). Not answered here.
