# Quake and QuakeWorld: the console words, the server's lines and the protocol-15 client (facts)

WORKING AREA. Clean-room readers' answers (no code), kept for the ledger's rows they settle. Part 1 answered PR 3's questions, part 2 PR 4's.

## Part 1: PR 3 answers: Quake and QuakeWorld (id Software's GPL source release, 2012 import)

Read from the Quake engine (single and network play, "NetQuake", protocol 15) and the
QuakeWorld client and server of id Software's GPL source release. Each fact is marked
**[read]** (stated directly by the source), **[derived]** (worked out from what the source
does, not stated in one place) or **[measured]** (none here: nothing was run).

Notation: "a newline" means the line ends in one line-feed byte. Text given in quotation
marks is exactly what is printed, without the quotation marks.

QuakeWorld print levels, as the protocol defines them: low 0, medium 1, high 2, chat 3. A QW
client only receives a print whose level is at or above its own message level (the `msg`
userinfo key or the `msg` command; default 0) **[read]**. Chat level (3) makes the client play
`misc/talk.wav` and draw the line in the coloured (high-bit) font **[read]**.

---

## The idle sway (both engines)

### 1. `v_idlescale` and the six `v_i*` settings

- Defaults, identical in both engines **[read]**: `v_idlescale` 0, `v_iyaw_cycle` 2,
  `v_iroll_cycle` 0.5, `v_ipitch_cycle` 1, `v_iyaw_level` 0.3, `v_iroll_level` 0.1,
  `v_ipitch_level` 0.3. None is saved to the config (not archived) **[read]**.
- The formula, per axis **[read]**: offset = `v_idlescale` times the sine of (client time in
  seconds times that axis's cycle) times that axis's level. The sine's argument is in
  radians, so a cycle is an angular rate in radians per second **[derived]**: the periods at
  the defaults are 2π/2 ≈ 3.14 s for yaw, 2π/0.5 ≈ 12.57 s for roll, 2π/1 ≈ 6.28 s for
  pitch **[derived]**. The three axes are in phase at time zero (all sines start at 0)
  **[derived]**.
- The result is in degrees: it is added straight to view angles, which are degrees
  **[read]**. Peak offsets at scale 1 and the default levels: yaw ±0.3°, roll ±0.1°,
  pitch ±0.3° **[derived]**.
- Sign: the offset is ADDED to the yaw, roll and pitch of the camera **[read]**.
- Which clock: the client's time, not real time **[read]**.
  - Quake: the client clock advances by the host frame time every frame and is pulled
   toward the times stamped on the server's messages (interpolation between the last two
   server messages) **[read]**.
  - QW: the client clock is real time minus the measured latency minus `pushlatency`
   milliseconds (never ahead of real time), and it is not advanced while the client is
   paused **[read]**.
- Drawn angles only: the offsets go into the angles the frame is rendered with, never into
  the player's own view angles, so they never reach the move command or the server
  **[read]**.
- Order inside the normal view: the camera angles are copied from the player's view
  angles, then the damage kick and the strafe roll are added, then the idle sway, then
  (later) the punch angle **[read]**. Quake sets the dead roll of 80° before the sway, so a
  dead player's roll still sways; QW sets its dead roll of 80° after the sway, so the roll
  sway is lost while dead **[derived]**.
- The held weapon:
  - Quake: after the sway is added to the camera, the weapon's angles are set from the
   swayed camera angles and then the three sway offsets are SUBTRACTED from the weapon's
   angles **[read]**. Net effect **[derived]**: the weapon's yaw equals the unswayed view
   yaw; its roll is the player's view roll minus the roll offset; its pitch, because a
   model's pitch is stored with the opposite sign, ends up at the view pitch plus twice the
   pitch offset in world terms (so relative to the swaying camera it moves by one offset in
   pitch and by one offset the other way in yaw).
  - QW: the sway routine also subtracts the offsets from the weapon's angles, but the
   weapon's angles are then overwritten from the player's angles and the swayed camera
   angles **[read]**. Net effect **[derived]**: the weapon follows the swayed camera exactly
   in yaw and pitch; its roll is the player's unswayed roll.
- When it applies **[read]**:
  - Always, during normal play, scaled by `v_idlescale` (so off at the default 0).
  - At intermission (and the finale) the camera sits at the intermission spot and the sway
   is applied with the scale forced to 1, whatever `v_idlescale` is; the setting's value
   is restored right after, so the player never sees it change. In this mode there is no
   weapon drawn.
  - Quake skips recomputing the view entirely while the client is paused, so the picture
   (sway included) freezes **[read]**. QW recomputes the view while paused, but its client
   clock does not advance, so the sway holds still **[derived]**.

---

## Console words of the client

### 2. `play`, `playvol`, `stopsound` (both engines; the two are the same here) **[read]**

**`play <file> [<file> ...]`**
- Takes any number of files; each argument is one sound, all started in the same frame
  **[read]**.
- ".wav" is appended when the argument contains no "." anywhere in it; an argument with a
  dot anywhere (even in a directory name) is used as given **[read]**. The name is looked up
  under the `sound/` directory of the game's search path **[read]**.
- Each sound is placed at the listener's position at the moment it starts (a fixed point,
  not attached to the player), at full volume (1.0, which is 255 of 255) and attenuation
  1.0 (normal: silent at 1000 units) **[read]**. Because it starts at distance zero it is
  full and centred in both ears **[derived]**; as the channel keeps its fixed origin, a
  player who moves away hears it fade and pan **[derived]**.
- Each call uses a new private entity number (counting up from a fixed start, a different
  one for `play` and `playvol`) and the automatic channel, so successive plays never cut
  each other off **[read]**.
- A missing file prints "Couldn't load sound/<name>" followed by a newline on the local
  console, where <name> includes the appended ".wav" **[read]**. It is normally printed
  twice: once when the name is precached (the `precache` setting, default 1) and once when
  the start tries to load it again **[derived]**. Nothing plays and nothing else happens.
- With the sound system not started or `nosound` 1, nothing happens and nothing is printed
  **[read]**.
- Limits **[read]**: a name of 64 characters or more is a fatal error, "Sound name too long:
  <name>"; more than 512 distinct sound names in the session is a fatal error, "S_FindName:
  out of sfx_t".

**`playvol <file> <vol> [<file> <vol> ...]`**
- Arguments are pairs: name then volume; the same ".wav" rule and placement as `play`, the
  given volume, attenuation 1.0 **[read]**.
- A trailing name with no volume gets volume 0 (the missing argument reads as 0), so it is
  loaded but not heard **[derived]**.
- Volume range: no clamp at the command. The volume is multiplied by 255 and truncated to an
  integer. 0 or below is silent (the channel is dropped as inaudible). For 8-bit samples the
  mixer clamps the channel to 255, so anything above 1.0 plays as 1.0; for 16-bit samples
  there is no clamp, so above 1.0 is louder, up to the output's clipping **[read]** for the
  clamps, **[derived]** for the audible result. The intended range is 0 to 1 **[derived]**.

**`stopsound`**
- Stops every playing channel at once and clears the output buffer (no tail) **[read]**:
  dynamic sounds (weapons, `play`), the level's ambient channels (water, sky, slime, lava
  by leaf) and the level's static sounds (torches, hums) **[read]**.
- Static (looping, map-placed) sounds are removed for the rest of the level: they return only
  when the next level's sign-on sends them again **[read]**.
- The leaf ambients come back by themselves on the next sound update and fade in at their
  normal fade rate **[derived]**.
- CD music is a separate system and is not touched **[read]**.

### 3. `version`

- Quake (client and the dedicated server console: it is one host command that runs locally
  and is never forwarded) prints two lines **[read]**:
  - "Version 1.09" followed by a newline (the number printed with two decimals, width 4).
  - "Exe: " then the build's compile time and compile date, then a newline. The time has the
   form HH:MM:SS and the date the form "Mmm dd yyyy" with the day padded by a space, for
   example "Exe: 13:05:22 Dec 21 1999" **[derived]** from the C compiler's build stamps.
- Quake reports 1.09 on every platform from this command **[read]**. (Separately, the Linux
  build prints "Linux Quake -- Version 1.300" at start-up, and the DOS build prints "Quake
  v1.09" **[read]**; not from `version`.)
- QW client: the same two lines, with "Version 2.40" **[read]**.
- QW server: has no `version` command **[read]**. It prints "Server Version 2.40 (Build
  NNNN)" between blank lines at start-up, sets the serverinfo key `*version` to "2.40", and
  refuses a too-old client's connect with "Server is version 2.40." **[read]**. The build
  number is a day count from the compile date, counted from 16 December 1998, printed as
  four digits with leading zeros **[read]**. The QW client prints "Client Version 2.40
  (Build NNNN)" the same way at start-up **[read]**.

### 4. Quake's `tell <name> <text>`

- The client always forwards it to the server as a string command (verb, a space, then the
  arguments exactly as typed) **[read]**.
- The receiver sees one line on the console: the sender's name, a colon and a space, then the
  WHOLE argument text, which still begins with the target's name, then a newline **[read]**.
  So "tell bob hi there" from alice reaches bob as "alice: bob hi there" **[derived]**. If
  the argument text starts with a double quote, the first and the last character of the
  argument text are removed **[read]**.
- The line carries no colour marker, so no talk sound plays and it is drawn in the plain
  font **[read]**.
- Length: the whole line before its newline is at most 62 bytes; the text is cut to fit
  **[read]**.
- The sender sees nothing **[read]**.
- Name matching: the first argument (a quoted name counts as one argument) is compared with
  each client's name, case-insensitively, over the whole string (no prefix match); only
  clients that are active and spawned are considered; the first match in slot order gets it,
  and only that one **[read]**. A player may tell themself **[derived]**.
- No match: nothing is sent and nothing is printed **[read]**.
- From the dedicated console: the command is forwarded like a client's, and since a
  dedicated server is not connected as a client it prints "Can't "tell", not connected"
  followed by a newline on the server console and does nothing else **[read]**.
- Too few arguments (fewer than a name and one word): nothing at all happens; there is no
  usage line **[read]**.

### 5. Quake's `ping`

- Always forwarded by the client, even on a listen server **[read]**. From the dedicated
  console it prints "Can't "ping", not connected" and a newline **[read]**.
- The server answers the asking client only, with the header "Client ping times:" and a
  newline, then one line per active client slot (spawned or not), in slot order **[read]**.
- Each line: the ping as an integer in milliseconds, right-aligned in a field of 4
  characters (wider if needed), one space, the player's name, a newline **[read]**. Example:
  "  48 player" **[derived]**.
- The value **[read]**: the mean of that client's last 16 samples; a sample is the server's
  time when a move arrives minus the server time the client stamped into that move (the
  time of the last server message the client had seen), in seconds; the mean is multiplied
  by 1000 and truncated toward zero. Slots start at zero, so a new client's figure ramps up
  over its first 16 moves **[derived]**.

---

## The server's lines

### 6. The operator's `say` on a dedicated server

**Quake** **[read]**
- Each client receives one print message whose text is: the byte 1, "<" + the `hostname`
  setting + "> ", the text, a newline. Example as seen: "<UNNAMED> hello" **[derived]**.
- The byte 1 makes the client draw the line in the coloured font AND play `misc/talk.wav`
  (the byte itself is not shown). Quake has no print levels.
- Sent to every client that is active and spawned. `say_team` from the dedicated console
  behaves as `say`.
- The server console echoes the line without the byte 1.
- Length: byte 1, prefix and text together are at most 62 bytes before the newline; the text
  is cut to fit. If the text starts with a double quote, its first and last characters are
  removed. With no text, nothing happens.
- `hostname` defaults to "UNNAMED"; the UDP driver replaces that default by the machine's
  host name (cut to 15 characters) at start-up, and an empty `hostname` is reset to
  "UNNAMED" when a map starts.

**QW** **[read]**
- Each spawned client (state "in the game"; connecting clients do not get it) receives a
  print at chat level (3) whose text is "console: " + the text + a newline.
- Chat level: the client plays `misc/talk.wav` and draws it coloured. A client whose message
  level is above 3 would not get it.
- No length cut beyond the 1024-byte print buffer. Same leading-quote stripping as Quake.
  With no text, nothing happens. The server console does not echo it.

### 7. `quit`

**QW server** **[read]**
1. One datagram is sent once to every client that is spawned (not to connecting ones),
   carrying: a print at high level (2) with "server shutdown" and a newline, immediately
   followed by a disconnect message. It goes out on the client's channel at once, not
   queued; it is not repeated.
2. The server console prints "Shutting down." and a newline.
3. Each master server gets one packet: the byte "C" then a newline (2 bytes). The console
   prints "Sending heartbeat to <address>" for each, although this is the shutdown packet.
   Nothing else goes to a master.
4. The log files are closed, the network is shut, the process exits.
- On the client **[read]**: the text "server shutdown" is printed, then the disconnect ends
  the game with the banner: a blank line, a line of 27 "=" signs, "Host_EndGame: Server
  disconnected", the line of "=" signs, a blank line. (A client still in the connected but
  not yet loading state would instead see "Server disconnected" followed by a line "Server
  version may not be compatible".) The client then disconnects itself.

**Quake server** **[read]**
- No text message is sent to players.
1. Pending reliable messages for every client (the final scores, for instance) are flushed,
   waiting up to 3 seconds in total.
2. A disconnect message is sent reliably to every active client, waiting up to 5 seconds for
   delivery; on failure the console prints "Host_ShutdownServer: NET_SendToAll failed for N
   clients".
3. Each active client is then dropped normally: if its channel can take a message, a second
   reliable disconnect is sent; the game program's client-disconnect function runs (its own
   prints are the deathmatch program's); the server console prints "Client <name> removed",
   the connection is closed; the remaining clients are told the slot's name is empty, its
   frags 0 and its colours 0.
4. The process exits.
- Quake has no master server: nothing is sent anywhere else **[read]**.
- On the client, the disconnect ends the game; its "Server disconnected" note is printed
  only with `developer` on; the client then disconnects (and returns to the demo loop if one
  is running) **[read]**.

### 8. A QW server error **[read]**

- The server console prints "SV_Error: <error>" and a newline.
- Every spawned client gets, in one datagram sent once and at once: a high-level (2) print
  "server crashed: <error>" followed by a newline, then a disconnect message.
- The client prints that line, then the disconnect ends its game with the same
  "Host_EndGame: Server disconnected" banner as in 7; the client then disconnects. Nothing
  more is sent, so a client that loses that datagram only finds out by its timeout
  **[derived]**.
- The masters get the shutdown packet ("C" and a newline) as in 7; the logs are closed and the
  network shut; the process exits with a fatal error "SV_Error: <error>". An error while
  handling an error is a fatal "SV_Error: recursively entered (<error>)" with no message to
  clients.

### 9. "<name> overflowed" (QW only) **[read]**

- When: at the server's send step for a client, if that client's reliable message has
  overflowed. That happens when a reliable write does not fit the reliable buffer (1450
  bytes) and the four back buffers of 1450 bytes each are already full, or a back buffer
  itself overflows.
- Line: "<name> overflowed" and a newline, at high level (2), broadcast to every client slot
  that is not free (connecting, spawned, or recently dropped) and whose message level allows
  it, and echoed on the server console. The server console also prints "WARNING: reliable
  overflow for <name>" (and, for the back-buffer cases, "WARNING: MAX_BACK_BUFFERS for
  <name>" or "WARNING: backbuf [N] reliable overflow for <name>" first).
- The client: its reliable and unreliable buffers are emptied, it is dropped (a disconnect
  message is queued, the game's disconnect function runs, the console prints "Client <name>
  removed" or "Spectator <name> removed"), and the disconnect is sent at once without
  waiting for the bandwidth choke. The client ends its game with the "Server disconnected"
  banner of 7 **[derived]**.
- Quake has no such line: an overflowed client is dropped silently, without even the final
  disconnect message **[read]**.

### 10. Pausing

**The announcements** **[read]**
- Quake: "<name> paused the game" and "<name> unpaused the game", each followed by a newline,
  to every active and spawned client (no print levels in Quake), then a pause on or off
  message to all. The name is the player's in-game name.
- QW: the same two texts at high level (2), broadcast to every non-free client slot whose
  message level allows it and echoed on the server console; then a pause message (on or off)
  is queued reliably to every non-free slot.
- QW gets the words backwards: it chooses the text from the state BEFORE the toggle, so a
  player who pauses a running game is announced as "<name> unpaused the game", and the one
  who resumes it as "<name> paused the game" **[read]** (the choice and the order are read, and
  the consequence is **[derived]**).

**Refusals** **[read]**
- Quake: only `pausable` 0 (default 1) refuses: "Pause not allowed." and a newline to the
  asking player only. A single player may pause. From the dedicated console the command is
  forwarded and prints "Can't "pause", not connected".
- QW: `pausable` 0 (default 1) gives "Pause not allowed." at high level to the asker; a
  spectator gets "Spectators can not pause." at high level; the `pausable` test comes first.
  The QW server console has no `pause` command.

**"Server is paused."** (QW only) **[read]**: sent to one client, at high level (2), when it
finishes entering the game (its "begin") while the server is paused, right after a pause-on
message to that client.

**"Pause released since no players are left."** (QW only) **[read]**: checked on the server
every frame with the timeouts. When the server is paused and no non-spectator client is
connecting or in the game, the pause is toggled off and the line (with a newline) is
broadcast at high level to every non-free slot (in practice the spectators still there, and
recently dropped slots) and printed on the server console, followed by a pause-off message to
those slots.

### 11. "Can't suicide -- allready dead!" **[read]**

- Exactly "Can't suicide -- allready dead!" followed by a newline (with the misspelling
  "allready").
- Both engines. Sent only to the player who typed `kill`, when that player's health is 0 or
  below; otherwise the game program's kill function runs.
- Quake: no print level. QW: high level (2).
- Spectators: QW has no spectator test for `kill`; a spectator's entity starts with all fields
  zero and the stock spectator program never sets its health, so a spectator gets this
  refusal **[derived]**.
- Quake's dedicated console: `kill` is forwarded and prints "Can't "kill", not connected".

---

## A NetQuake client that plays (Quake, protocol 15)

### 12. String commands during play **[read]**

- The client sends a string command (client message 4) by writing the command word, one
  space, then the rest of the line exactly as typed (quotes kept), with NO newline; a command
  typed with no arguments is sent as the word, a space and a newline. The text ends with a
  zero byte. Nothing else is prefixed. `cmd <text>` sends <text> alone, raw.
- Commands the client forwards: `say`, `say_team`, `tell`, `kill`, `pause`, `ping`, `god`,
  `notarget`, `noclip`, `fly`, `give`, `name` (only when connected and the name changed),
  `color` (only when connected), `status`, `kick` and `ban` (these three only when this
  machine is not itself running the server), and `cmd`. An unknown command is not forwarded:
  it prints "Unknown command "<word>"" locally.
- The server accepts from a normal client only text beginning (case-insensitively) with one
  of: status, god, notarget, fly, name, noclip, say, say_team, tell, color, kill, pause,
  spawn, begin, prespawn, kick, ping, give, ban. It is a prefix test, so text like "sayx"
  passes the filter and then fails as an unknown command on the server **[derived]**.
  Anything else is ignored, noted on the server console as "<name> tried to <text>" only in
  developer mode. The listen server's own player (privileged) may send any command.
- Several string commands may sit in one reliable message; the server runs them in order as
  it reads them **[read]**.

### 13. Name and colours **[read]**

- At sign-on stage 2 (the reply to the server's sign-on number 2), the client queues three
  string commands in one reliable message, in this order:
  1. `name "<name>"` and a newline: the word, a space, the name in double quotes.
  2. `color <top> <bottom>` and a newline, the top and bottom as decimal numbers 0 to 13 from
     the player's colour setting (top is the high four bits, bottom the low four bits).
  3. `spawn ` followed by the spawn parameters (empty for a remote connect, so the text is
     "spawn" and a space).
- Defaults: name "player", colours 0 0. They are the client's archived `_cl_name` and
  `_cl_color` settings, set through the `name` and `color` commands.
- On change during play: `name <args>` and `color <args>` are forwarded in the generic form of
  12 (the word, a space, the arguments as typed, no newline), not rewritten into the sign-on
  form. `name` with no arguments prints, locally, the word name in double quotes, " is ", and the
  current name in double quotes; it is not forwarded if the new name
  equals the current one. A name typed as several words is sent whole but cut to 15
  characters first; a name typed as one argument is sent as typed and cut to 15 characters by
  the server **[derived]**.
- `color` with one number sets top and bottom alike; each value keeps its low four bits and
  is capped at 13. With no arguments it prints the current pair and the line "color <0-13>
  [0-13]". The server sets the player's team to bottom plus 1 and tells every client.

### 14. The move's buttons and the impulse **[read]**

- Button byte: bit value 1 is attack, bit value 2 is jump. No other bit is sent; `+use`
  exists as a key command but is never sent.
- A button bit is set when the key is held now OR was pressed at any time since the last move,
  so a tap shorter than a frame still reaches one move.
- Impulse byte: sent in every move. It holds the number from the last `impulse N` command,
  and is set back to 0 right after each move is built, so a given impulse goes out in exactly
  one move. The server only overwrites the player's impulse when the byte is non-zero.
- Moves are sent only once sign-on is complete, unreliably, one per client frame. The first
  two moves after each new level are built but not sent, so an impulse or a tap that lands in
  one of them is lost **[derived]**. Nothing is sent during demo playback.

### 15. Leaving **[read]**

- On `disconnect` (also `quit`, a new `connect` or `map`, or the game ending), when
  connected and not playing a demo: a demo being recorded is stopped, the queued reliable
  data is discarded, and a message of exactly one byte, the disconnect byte (2), is sent
  ONCE as an UNRELIABLE datagram; the connection is then closed at once.
- The server drops the client when it reads that byte (a normal drop as in 7, step 3). If the
  datagram is lost, the server only notices by its message timeout (`net_messagetimeout`,
  default 300 seconds) **[derived]**.

## Part 2: PR 4 answers: Quake and QuakeWorld (id Software's GPL source release, 2012 import)

Read from id Software's 2012 GPL source release: the Quake engine ("WinQuake", network
protocol 15), the QuakeWorld client and server ("QW"), and the QuakeWorld deathmatch program
("qw-qc"). Each fact is marked **[read]** (stated directly by the source), **[derived]**
(worked out from what the source does, not stated in one place) or **[measured]** (none here:
nothing was run).

Notation: "a newline" means the line ends in one line-feed byte. Text in quotation marks is
exactly what is printed, without the quotation marks. `<n>` and `<name>` stand for a value
filled in. "Server console" means the console of the machine running the server (not sent to
any player).

QuakeWorld print levels: low 0, medium 1, high 2, chat 3. A QW server sends a print to a player
only when the print's level is at or above that player's message level **[read]**. The server
starts a slot's message level at 0 and then takes the `msg` user info key whenever the user info
is parsed (connect, and every changed `setinfo`), when that key is non-empty **[read]**. The QW
client's `msg` setting defaults to "1" and is a user info key, so a stock client asks for level 1
**[read]**.

---

## Server (QW unless said)

### 1. `msg` as a client's string command

- Exists only in the QW server, as a command a connected client sends **[read]**.
- With no argument (exactly one word), the player is sent, at print level high (2):
  "Current msg level is `<n>`" and a newline, where `<n>` is the current level as a signed
  decimal integer **[read]**. Any word count other than two (none, or two or more arguments)
  takes this branch **[read]**.
- With one argument: the level becomes the argument read as a decimal integer (leading digits
  only, anything unparsable is 0) **[read]**. There is no range check: negative numbers and
  numbers above 3 are stored as given **[read]**. The player is then sent, at level high (2):
  "Msg level set to `<n>`" and a newline **[read]**.
- Both replies are themselves filtered by the level: when the current level (for the first) or
  the new level (for the second) is 3 or more, the player receives nothing **[derived]**.
- The command does not change the player's user info. The next time any user info key actually
  changes (a `setinfo` with a new value), the server re-reads the `msg` key and, if it is
  non-empty, overwrites the level set by the command **[derived]**.

### 2. `user <userid>` on the server console

- Usage line, printed when the word count is not exactly two: "Usage: info <userid>" and a
  newline (it names `info`, not `user`) **[read]**.
- The argument is read as a decimal integer and matched against every slot that is not free
  (connecting, in game, or a dropped slot still held, see 4) **[read]**. No match: "Userid
  `<n>` is not on the server" and a newline, `<n>` being the parsed number (a non-numeric
  argument gives 0) **[read]**.
- Match: no header line; the user info is printed one key per line, in the order the keys sit
  in the string **[read]**. Each line is the key, padded with spaces on the right to exactly 20
  characters when shorter than 20, then the value, then a newline **[read]**. A key of 20
  characters or more is printed whole with no padding and no separator, so the value follows
  it directly **[read]**. If the string ends after a key with no value, the line reads the
  padded key then "MISSING VALUE" and a newline, and printing stops **[read]**.
- A dropped slot still held has an empty user info, so a match on it prints nothing at all
  **[derived]**.
- The same 20-column format is used by the server's `serverinfo` with no arguments (which first
  prints "Server info settings:" and a newline), by a client's `setinfo` with no arguments sent
  to the server (header "User info settings:" and a newline, printed on the server console, not
  sent to the player), and by the client commands in 9 **[read]**.

### 3. `sv_highchars`

- Default "1" (high characters allowed), not saved to a config **[read]**.
- At connect, with the setting 0: every byte of the whole user info string the client sent
  (keys, values and the backslash separators alike) is kept only when its value is from 32 to
  127 inclusive; bytes 0 to 31 and 128 to 255 are removed, not replaced **[read]**. With the
  setting non-zero the string is copied as sent (cut to the buffer) **[read]**.
- On every later write of a key into any info string on the server (a player's `setinfo`, the
  server's own renaming of a player, `serverinfo`, `localinfo`), with the setting 0: each byte
  of the new key and value has its high bit cleared (byte 128+x becomes x, so a coloured letter
  becomes the plain letter), and a byte that is then below 32 is dropped **[read]**. With the
  setting non-zero no byte is changed by this step **[read]**.
- Whatever the setting, that write step also drops every byte of value 13 or less (control
  characters up to carriage return) **[read]**. So with `sv_highchars` 1, a `setinfo` keeps
  bytes 14 to 255 **[derived]**.
- That write step also refuses, with a line on the server console and no change: a key or value
  containing a backslash (the words "Can't use keys or values with a " followed by one
  backslash and a newline), a key
  or value containing a double quote ("Can't use keys or values with a " followed by one double
  quote and a newline), a key or value longer than 63 characters ("Keys and values must be < 64
  characters." and a newline), and a result that would not fit ("Info string length exceeded"
  and a newline) **[read]**. The connect path applies none of these checks **[read]**.
- So: connect removes high and control bytes; setinfo converts high bytes to their low form
  **[derived]**. The two paths differ on purpose or not, the source does not say.

### 4. `zombietime`

- Default "2", in seconds, not saved **[read]**.
- When a player is dropped normally (disconnect, kick, overflow), the server writes the
  disconnect message onto the slot's reliable stream, calls the program's disconnect, prints
  "Client `<name>` removed" (or "Spectator `<name>` removed") and a newline on the server console,
  closes any download or upload, clears the slot's name, frags and user info, sends every
  remaining client the slot's now-empty player info, and holds the slot for `zombietime`
  seconds counted from the drop **[read]**.
- While held: packets from the dropped client's address are still accepted by the channel
  (their acknowledgements are processed) but no command in them runs **[read]**. Each time such
  a packet arrives the server answers with a packet carrying only the reliable stream, so the
  disconnect message is resent until the client acknowledges it **[read]** **[derived]**.
- The held slot is not free: it counts toward `maxclients` (or `maxspectators`) when a new
  connection is counted, and the free-slot search skips it **[read]**. A new player can take it
  only after the time has passed; the check runs once per server frame **[read]**.
- `status` shows a held slot with "ZOMBIE" in place of the rate, ping and loss columns
  **[read]**.
- A player who times out (no packet for `timeout` seconds, default 65) skips the hold: the slot
  is freed at once **[read]**.
- A new connect from the same address and port (or qport) as a held slot drops that slot again
  (server console: "`<address>`:reconnect" and a newline), which restarts its hold **[derived]**.

### 5. `sv_gamedir <dir>`

- QW server only **[read]**.
- No argument: "Current *gamedir: `<value>`" and a newline, the value being the server info key
  `*gamedir` (empty if unset) **[read]**.
- More than one argument: "Usage: sv_gamedir <newgamedir>" and a newline **[read]**.
- Refused, with "*Gamedir should be a single filename, not a path" and a newline: a name
  containing two dots in a row, a forward slash, a backslash or a colon anywhere **[read]**. A
  single dot, spaces, or an empty name are not refused **[derived]**.
- What it changes: only the server info key `*gamedir`. It does not change the server's own
  search path (that is the separate `gamedir` command, which changes both) **[read]**. An empty
  name removes the key **[derived]**.
- Clients are not told at once: no server info change message is sent **[read]**. A client
  learns it at its next connection handshake, where the server sends the `*gamedir` value as the
  game directory in its server data, or "qw" when the key is missing, and the whole server info
  string **[read]**.

### 6. `*progs` and `*cheats`

- `*progs`: set every time a map is spawned, when the program is loaded (the file
  `qwprogs.dat`, or `progs.dat` if that is missing) **[read]**. The value is the 16-bit CRC of the
  whole file's bytes (the same CRC the protocol uses elsewhere: CCITT polynomial, start value
  0xFFFF, no final XOR), written as an unsigned decimal integer from 0 to 65535 with no padding
  **[read]** **[derived]**. Clients are not sent a change message; they see it in the server
  info at their next handshake **[read]**.
- `*cheats`: set to "ON" (upper case) once, at server start, when the command line holds
  `-cheats` **[read]**. Nothing else sets or removes it: without `-cheats` the key is never
  present, and since cheats cannot be turned off while running, it is never removed **[read]**
  **[derived]**. The cheat commands (`god`, `give`, `noclip` on the server console) refuse
  without it, printing "You must run the server with -cheats to enable this command." and a
  newline **[read]**.

### 7. Settings copied into the server info

- These QW server settings write themselves into the server info whenever set, including once at
  registration with their default, so all are present from start **[read]**: `fraglimit` (0),
  `timelimit` (0), `teamplay` (0), `samelevel` (0), `maxclients` (8), `maxspectators` (8),
  `deathmatch` (1), `spawn` (0), `watervis` (0), `hostname` ("unnamed").
- Each set also sends all clients a server info change (key and value) when a map is running
  **[read]**.
- A value of "0" is a non-empty string and is kept **[read]**. An empty value removes the key,
  for these settings and for any info key **[read]**.
- Other keys the server writes itself **[read]**: `*version` (the version number with two
  decimals, at start), `*progs` and `*cheats` (6), `*gamedir` (5), `map` (the map's name, at
  each spawn), `needpass` (1 for a player password, 2 for a spectator password, 3 for both,
  removed when neither is set or both are "none"; rechecked each frame when either password
  setting has changed, printing "Updated needpass." and a newline on the server console).
- The `serverinfo <key> <value>` console command sets any key (star keys refused: "Star
  variables cannot be changed." and a newline) and, when a setting of that name exists, sets its
  value too, without going through the setting's own copy step **[read]**.

### 8. `pmodel` and `emodel`

- Server side **[read]**: at each map spawn the server takes the 16-bit CRC of its own
  `progs/player.mdl` and `progs/eyes.mdl`. When a player (or spectator) begins on the map, the
  server reads the `pmodel` and `emodel` user info keys as decimal integers (missing or
  non-numeric is 0) and compares them with those two CRCs.
- When either differs, every client is sent, at print level high (2): "`<name>` WARNING: non
  standard player/eyes model detected" and a newline **[read]**. The player is kept; nothing
  else happens **[read]**.
- The check runs only at that begin, once per map per player; a later `setinfo` of either key is
  not checked **[read]** **[derived]**.
- Client side (both the software and GL QW clients) **[read]**: when the client loads
  `progs/player.mdl` or `progs/eyes.mdl`, it computes the same CRC of the file and stores it in
  its own user info as `pmodel` or `emodel`, as a decimal number, and, when connected, sends
  "setinfo pmodel `<n>`" (or emodel) to the server. The client refuses to let the player set
  these keys by `setinfo` or `fullinfo` (silently).

---

## Client (QW unless said)

### 9. `user <name or userid>`

- Wrong word count: "Usage: user <username / userid>" and a newline **[read]**.
- The client walks its player slots in order, skipping slots with an empty name; the first slot
  whose user id equals the argument read as an integer, or whose name equals the argument
  exactly (case-sensitive), is printed **[read]**. The output is that player's user info in the
  20-column format of 2 (key padded to 20, value, newline; no header) **[read]**.
- No match: "User not in server." and a newline **[read]**.

### 10. `fullinfo <string>`

- Wrong word count: "fullinfo <complete info string>" and a newline **[read]**.
- The argument is an info string (a leading backslash is skipped). Each key and value pair is
  written into the client's own user info, in order **[read]**. It is not forwarded to the
  server and sends nothing on the network **[read]**.
- Skipped silently: the keys `pmodel` and `emodel` (compared without regard to case) **[read]**.
- A key starting with an asterisk is refused with "Can't set * keys" and a newline, and the next
  pair is tried **[read]**.
- A trailing key with no value prints "MISSING VALUE" and a newline and stops; the pairs before
  it are already applied **[read]**.
- Each write goes through the client's info rules **[read]**: an empty value removes the key,
  the refusals of 3 apply with the same lines (backslash, double quote, 64 characters, length
  exceeded); every key except `name` has its bytes' high bits cleared and control bytes dropped,
  and the `team` value is lowercased.

### 11. `togglechat`

- QW only; Quake has no such command **[read]**.
- It does exactly what `toggleconsole` does **[read]**: clears the console's input line, then:
  if the console has the keyboard and the client is fully in game, the keyboard goes back to the
  game (the console closes); if the console has the keyboard and the client is not fully in
  game, the console stays; otherwise (game, chat line or menu has the keyboard) the console takes
  the keyboard. In every case the notify lines (the recent lines shown at the top of the view)
  are cleared.
- It prints nothing and does not hide or show chat **[read]**.

### 12. `rerecord <demoname>`

- QW only **[read]**.
- Wrong word count: "rerecord <demoname>" and a newline **[read]**.
- No server address known yet (no `connect` this session): "No server to reconnect to..." and a
  newline **[read]**. Being disconnected is not itself refused, as long as an address is known
  **[derived]**.
- If a demo is being recorded, it is stopped first: an end-of-demo disconnect is written, the
  file is closed and "Completed demo" and a newline is printed **[read]**.
- The file is `<game directory>/<demoname>`, with ".qwd" appended when the name has no extension
  **[read]**. If it cannot be created: "ERROR: couldn't open." and a newline **[read]**.
  Otherwise: "recording to `<full path>`." and a newline (lower-case "recording", a full stop
  before the newline) **[read]**.
- Then it disconnects and starts connecting again to the last server address; the reconnect
  prints "Connecting to `<address>`..." and a newline at once **[read]**.
- Edge: when the client was connected, the disconnect step itself stops any recording, so the
  new demo is closed at once (a second "Completed demo" line) holding only the end-of-demo
  message, and nothing of the reconnect is recorded **[derived]**. Only when the client was
  already disconnected does the recording carry through the new connection **[derived]**.

### 13. `path` (both games)

- First line in both: "Current search path:" and a newline **[read]**.
- Then one line per search path entry, most recently added first **[read]**: a pack prints its
  file path as opened, a space, then "(`<n>` files)" and a newline, `<n>` the pack's file count,
  a directory prints its path and a newline **[read]**.
- Quake, order **[derived]**: each game directory adds its directory, then `pak0.pak`, `pak1.pak`
  and so on while they exist, each on top. So the listing runs: the `-game` directory's packs
  from highest number down, then that directory; then `hipnotic`, then `rogue` (when given on
  the command line) the same way; then `id1`'s packs highest first, then `id1`. Paths are the
  base directory (a trailing slash removed) joined by "/" to the directory name. With `-path`,
  the listed entries replace all of this and print in reverse of their command-line order.
- QW, order **[read]** **[derived]**: the base entries are `id1` then `qw` (so `qw`'s packs and
  directory print above `id1`'s). A game directory set by `gamedir` sits above them. A line of
  exactly ten hyphens ("----------" and a newline) is printed just before the first base entry,
  so it appears directly under the header when no game directory is set.

### 14. `port` (Quake)

- No argument (or more than one): `"port" is "<n>"` and a newline, with the double quotes
  printed, `<n>` the current host port **[read]**. Default 26000; `-port`, `-udpport` or
  `-ipxport` on the command line sets the start value **[read]**.
- With an argument: read as a decimal integer; when below 1 or above 65534, "Bad value, must be
  between 1 and 65534" and a newline, and nothing changes **[read]**.
- Otherwise the port becomes the new value (both the current and the default port) with no
  confirmation line **[read]**. If the game is listening for network clients, it queues
  `listen 0` then `listen 1` so the sockets reopen on the new port **[read]**.

### 15. `noaim` (QW)

- Client setting, default "0", saved to the config, and a user info key **[read]**.
- Server meaning: the program's aim helper (used by the QW deathmatch program for the rocket
  launcher's rocket and for a grenade thrown with the view level, pitch exactly zero) returns the
  plain view direction, with no search for a target, when the player's `noaim` value read as an
  integer is greater than 0 **[read]**.
- Otherwise the helper first traces straight ahead 2048 units from 20 units above the origin and
  keeps the view direction if it hits something that can be aimed at (not a teammate in
  teamplay); else it looks for the target whose direction is closest to the view and at least
  `sv_aim` (dot product) close **[read]**. The QW server's `sv_aim` defaults to "2", which no dot
  product can reach, so at the default no target is ever chosen and `noaim` changes nothing
  **[derived]**. It matters only on a server that lowers `sv_aim` below 1 **[derived]**.
- Edge: the check is made only for entity numbers 1 to 31, so the player in the 32nd slot cannot
  turn aim help off **[derived]**.

### 16. `localid` (QW)

- Client setting, default empty, not saved **[read]**.
- It guards a remote command packet: an out-of-band packet of type 'B' that carries a command
  string and an id string, accepted only from the client's own address or loopback (from
  elsewhere: "Command packet from remote host.  Ignored." with two spaces, and a newline)
  **[read]**.
- Until the first connection the client runs such a command without checking the id **[read]**.
  When the server accepts a connection the client turns that allowance off, so every later
  command packet needs an id (leading and trailing white space trimmed) equal to `localid`
  **[read]**. After a command runs, the allowance stays off **[read]**.
- `localid` empty: three lines, a line of 27 equals signs, then "Command
  packet received from local host, but no localid has been set.  You may need to upgrade your
  server browser." (two spaces after the full stop), then the 27 equals signs again, each with a
  newline; the command is not run **[read]**.
- Wrong id: the 27 equals signs; "Invalid localid on command packet received from local host. "
  (with a trailing space) and a newline; a line made of a vertical bar, the id sent, a vertical bar, a space, an exclamation mark, an equals sign, a space, a vertical bar, the `localid` value and a vertical bar, then a newline; "You may
  need to reload your server browser and QuakeWorld." and a newline; the 27 equals signs and a
  newline. Then `localid` is cleared to empty and the command is not run **[read]**.
- Every such packet first prints "`<sender address>`: client command" and a newline **[read]**.

### 17. `_cl_name` and `_cl_color` (Quake)

- The player's own name and colours, saved to the config. Defaults: `_cl_name` "player",
  `_cl_color` "0" **[read]**.
- Written by the `name` and `color` console commands typed by the player (the setup menu uses
  those commands) **[read]**. `name` takes the single argument, or the whole argument string when
  there are several, cut to 15 characters; if it equals the current value nothing happens
  **[read]**. `color` takes top and bottom (one argument sets both), keeps the low 4 bits of
  each, caps each at 13, and stores top times 16 plus bottom as a number with six decimals (for
  example "77.000000") **[read]**. When connected the command is also forwarded to the server
  **[read]**.
- Read: at the second signon step the client sends the server `name "<_cl_name>"` and `color
  <top> <bottom>` (top is the stored value divided by 16, bottom its low 4 bits) **[read]**; by
  `name` with no argument, printing `"name" is "<value>"` and a newline **[read]**; by `color`
  with no argument, printing `"color" is "<top> <bottom>"` and a newline then "color <0-13>
  [0-13]" and a newline **[read]**; by the setup menu for its starting values; and by `kick`
  typed on a listen server's console, as the kicker's name in "Kicked by `<name>`" **[read]**.

### 18. The chase camera (Quake)

- Defaults: `chase_back` 100, `chase_up` 16, `chase_right` 0, `chase_active` 0; none saved
  **[read]**.
- Placement, each frame while `chase_active` is non-zero **[read]**: start from the first-person
  eye point; move `chase_back` units backward along the full view direction (pitch included)
  and `chase_right` units along the view's right direction but subtracted, so a positive value
  moves the camera to the player's left; then replace the height with eye height plus
  `chase_up`. So the horizontal offset shrinks as the player looks up or down (by the cosine of
  the pitch) and the height never depends on pitch **[derived]**.
- Aim **[read]**: a line is traced 4096 units along the view from the eye against the world; the
  camera's pitch is set so it looks at the same height as that hit point, at the hit's forward
  distance along the view (at least 1 unit). Yaw and roll stay the player's.
- The camera position itself is not traced: it can sit inside walls **[read]**.
- The player's own model is drawn while it is active **[read]**.

### 19. `m_side` (both)

- Default 0.8 in both; saved in Quake, not saved in QW **[read]**.
- It scales the mouse's horizontal axis (after `sensitivity` and the optional `m_filter`
  averaging) into sideways movement **[read]**. This happens when `+strafe` is held, or when
  `lookstrafe` is non-zero and `+mlook` is held; otherwise the horizontal axis turns the view by
  `m_yaw` **[read]**. Same rule in both games and in every platform's mouse code **[read]**.

### 20. `ambient_level` and `ambient_fade` (both)

- Defaults: `ambient_level` 0.3, `ambient_fade` 100; not saved; identical in both games
  **[read]**.
- The map gives each leaf four ambient levels, one byte each (0 to 255): water, sky, slime, lava
  **[read]**. Only water (`ambience/water1.wav`) and sky (`ambience/wind2.wav`) have a sound,
  slime and lava are silent in both games **[read]**.
- Each frame, for the leaf the listener is in, each ambient's target volume is `ambient_level`
  times the leaf's byte, on the 0 to 255 volume scale where 255 is a sound at full volume
  **[read]**. A target below 8 becomes 0 **[read]**. The sound plays equally in both ears, not
  placed **[read]**.
- `ambient_fade` is the rate at which the playing volume moves toward the target, in volume
  units (of 255) per second, up or down, never overshooting **[read]**. At the default, silence
  to full takes 2.55 s **[derived]**.
- `ambient_level` 0 (or no map loaded, or the listener in no leaf) cuts all ambients at once, with
  no fade **[read]**.

### 21. `scr_printspeed` (Quake)

- Default 8, not saved **[read]**. Unit: characters per second **[read]** **[derived]**.
- It applies only while the centre text is drawn during an end-of-game finale or a cutscene (the
  two server messages that carry a text); in normal play the whole centre text shows at once,
  and in the ordinary level-end intermission the centre text is not drawn **[read]**.
- The count shown is the setting times the client time in seconds since the text arrived,
  truncated to an integer, plus one: the first character shows at once **[derived]**. Newlines
  are not counted **[read]**. Characters beyond 40 on a line are neither drawn nor counted
  **[derived]**.
- Layout **[read]**: each line centred, 8 pixels per character and per line; a text of 4 lines
  or fewer starts at 35 % of the screen height, a longer one at 48 pixels from the top. During
  the finale and cutscene the text never times out. It is drawn only while the game has the
  keyboard.

### 22. `showturtle` and the net icon

- `showturtle`: default 0, not saved, both games **[read]**. When non-zero, the "turtle" picture
  from the graphics archive is drawn at the top-left corner of the 3D view once three or more
  consecutive frames have each taken 0.1 s or more (10 frames per second or fewer); any faster
  frame resets the count **[read]**.
- Net icon, no setting **[read]**: the "net" picture, drawn 64 pixels right of the view's top-left
  corner (beside the turtle), never during demo playback.
- Net icon in Quake: shown when 0.3 s or more of real time has passed since the last message
  from the server **[read]**.
- Net icon in QW: shown when 63 or more of the client's sent packets are unacknowledged (the
  packet backup of 64 minus one) **[read]**.
- Neither is drawn during an intermission, the loading plaque or a dialog **[read]**.

### 23. `cl_hudswap` (QW)

- Default 0, saved; the options menu shows it as "HUD on left side" **[read]**.
- It acts only in the overlay status bar mode (`cl_sbar` 0 and `viewsize` 100 or more), where the
  owned weapons stand in a column and the four ammo counts in a column above the status bar
  **[read]**. At 1 both columns move from the right edge of the screen to the left edge: the
  weapon icons (24 pixels wide) at x 0, the ammo boxes (42 pixels wide) at x 0 with their digits
  at x 3, 11 and 19 **[read]**. Nothing else moves **[read]**.

### 24. `+use`

- Both games register `+use` and `-use`, which only record the key's state on the client
  **[read]**. That state is never sent: the button byte of each move carries only bit 1 (attack)
  and bit 2 (jump) in both Quake and QW **[read]**.
- Both servers set the program's `button0` and `button2` from those bits and never write
  `button1` (the field the program calls "use") **[read]**. So in both games `+use` does nothing
  **[derived]**.
- The QW deathmatch program reads `button1` (an intermission ends, and a dead player respawns, on
  any of attack, use or jump), but since it is always 0 only attack and jump do it **[read]**
  **[derived]**. The Quake game program is not part of this release, so what it does with the
  field is not answered here.

---

## Game (the QW deathmatch program)

### 25. `item_weapon`

- An old ammo box kept for compatibility; its editor note says not to use it and that it will be
  removed **[read]**.
- Spawnflags **[read]**: 1 shells, 2 rockets, 4 nails, 8 big.
- What each gives and its model **[read]**: shells 20 (big 40), `maps/b_shell0.bsp` (big
  `maps/b_shell1.bsp`); nails 20 (big 40), `maps/b_nail0.bsp` (`maps/b_nail1.bsp`); rockets 5
  (big 10), `maps/b_rock0.bsp` (`maps/b_rock1.bsp`).
- Several type flags **[derived]**: the checks run shells, then nails, then rockets, each
  overwriting the last, so rockets win over nails and nails over shells (every matching model is
  still precached). With no type flag the box has no model and gives nothing, but touching it
  still prints "You got the " followed directly by a newline, plays the sound and hides it.
- Box **[read]**: mins 0 0 0, maxs 32 32 56. It is placed 0.2 s after the map starts: raised 6
  units and dropped to the floor; if it falls out of the level it is removed (developer print
  "Bonus item fell out of level at `<origin>`" and a newline).
- Unlike the other ammo boxes it does not skip itself in `deathmatch` 4 **[read]**.
- Touch, by the shared ammo rules **[read]**: only a living player; refused while that ammo is
  at its cap (shells 100, nails 200, rockets 100); else adds, then caps at those limits. The
  player is sent at print level low (0) "You got the " then the ammo's name ("shells", "spikes"
  or "rockets") then a newline. Sound `weapons/lock4.wav` on the item channel at normal
  attenuation; the client runs `bf` (the pickup flash). If the player held his best weapon, he
  switches to the new best. Then the item's targets fire.
- Respawn **[read]**: the box hides; it returns 30 s later, 15 s in `deathmatch` 3 or 5, and never
  in `deathmatch` 2; on return it plays `items/itembk2.wav`.
