# Answers: Quake II server (Q2PRO upstream, commit 601a8df): the multi-view recorder's dummy client, the relay's hacks, viewer free flight

Reader's notes. These are a clean-room reader's facts about the Q2PRO server at upstream commit
601a8df, written in the reader's own words from the server sources (`src/server/mvd.c` for the
game server's recorder, `src/server/mvd/game.c`, `client.c` and `parse.c` for the relay, with
`src/server/main.c`, `game.c`, `src/common/cmd.c` and `src/common/pmove` for shared pieces). They
contain no code. The only identifiers quoted are setting, command and flag names and numeric
constants, and the only exact strings are texts a player, a viewer, an rcon user or the console
sees. **[read]** = read directly in the source; **[derived]** = inferred from what was read.
"Console" means the server's own output (terminal and log file). A "developer line" is printed
only when the `developer` setting is 1 or more, and that setting exists only in debug builds.
So in a normal release build these lines are never printed **[read]**.

Protocol numbers: 34 = stock, 35 = the R1Q2 extension, 36 = Q2PRO's own **[read]**.
Multi-view stream protocol: 2010 = the default version, 2013 = the current version **[read]**.

---

Terms used below. "Recorder" = the game server's multi-view recorder (`mvd.c`). "Relay" = the
GTV/MVD client side that receives a stream and serves viewers (`mvd/*.c`). "Base frame" = one
10 Hz game frame of 100 ms (`BASE_FRAMETIME` 100, `BASE_FRAMERATE` 10). "realtime" is the
server's wall clock in milliseconds.

## A. The recorder's dummy client (`[MVDSPEC]`)

### A1. What it is

1. The dummy is a fake client kept entirely on the server side. It stands for all multi-view
   viewers: its player state is the base for free-flying viewers on the relay, and it is the
   client that receives the scoreboard (layout) and text messages that go into the stream
   **[read]**.
2. It has no network connection: its protocol number is set to -1, and every message the
   server "sends" to it is handed to a private hook instead of a net channel (see A6) **[read]**.

### A2. Reserving a slot, and which slot it takes

3. When `sv_mvd_enable` is nonzero (latched, default 0), server start clamps `sv_maxclients` to
   at most 255 (`MAX_CLIENTS` - 1, so one client number stays free as "no client") and clamps
   `sv_reserved_slots` to between 1 and `sv_maxclients` - 1, so at least one reserved slot
   always exists for the dummy **[read]**.
4. The soft limit for ordinary players is `sv_maxclients` - `sv_reserved_slots`; the reserved
   slots are the highest-numbered ones **[read]**.
5. The dummy takes the FIRST free slot among the reserved slots (lowest number first); only if
   every reserved slot is taken does it take the first free ordinary slot (lowest number first)
   **[read]**.
6. Its client number is that slot's index and its entity is that index + 1 (entity 0 is the
   world) **[read]**. The stream's header carries the dummy's client number, or -1 when there
   is no dummy **[read]**.

### A3. `sv_mvd_spawn_dummy` (default 1, not latched)

7. 0 or less: the dummy is never created. Developer line `Dummy MVD client disabled` **[read]**.
8. 1: the dummy is created only when the game library reports the "dummy MVD client aware"
   feature bit (`GMF_MVDSPEC`, bit 2 of `g_features`). If the game does not, no dummy is made,
   developer line `Dummy MVD client not supported by game` **[read]**.
9. 2 or more: the dummy is created whatever the game reports **[read]**.
10. In the "no dummy" cases above creation reports "nothing made, no error": recording and
    relaying proceed without a dummy (the stream says client -1) **[read]**.

### A4. When it is created

11. Created (if not already present) whenever the recorder is enabled: on the first GTV relay
    that sends "stream start", on the console command `mvdrecord` (local recording, writing
    `demos/<name>.mvd2`), and by the dummy's own `record` command (A8) **[read]**.
12. With `sv_mvd_autorecord` (latched, default 0) set, it is also created at every map change
    so that the game mod can drive recording through the dummy's `record` / `stop` commands;
    when it is newly made this way, developer line `Spawning MVD dummy for auto-recording`
    and `sv_mvd_suspend_time` is forced to 0 **[read]**.
13. A dummy that cannot be made fails the enable: a relay that asked for the stream is sent an
    error and dropped with console line
    `TCP client <name>[<address>] dropped: couldn't create MVD dummy` **[read]**; a local
    `mvdrecord` closes its file and records nothing **[read]**.

### A5. How it connects to the game library

14. Creation sets the slot to "connected", installs the private message hook, then calls the
    game's `ClientConnect` with the dummy's entity and its userinfo, with the server's "current
    client" pointing at the dummy **[read]**.
15. The userinfo is exactly these key/value pairs (backslash-separated info string):
    `name` = `[MVDSPEC]`, `skin` = `male/grunt`, `spectator` = `1`, then
    `mvdspec` = `2013` (the current stream protocol version) and `ip` = `loopback` **[read]**.
16. If the game reports `GMF_EXTRA_USERINFO` (bit 12), the first three pairs are the normal
    userinfo string and the `mvdspec` / `ip` pairs are passed as a SECOND string right after the
    first one's terminator ("extra userinfo"); otherwise all five pairs are one string **[read]**.
    (For real players the server strips any `mvdspec` key from userinfo when the game does not
    take extra userinfo, so only the dummy carries it there **[read]**.)
17. If `ClientConnect` refuses, the console error line is
    `Dummy MVD client rejected by game: <rejmsg>` where `<rejmsg>` is the `rejmsg` key the game
    set, or `Connection refused` when it set none; creation fails (A4 item 13) **[read]**.
18. On acceptance the (possibly game-modified) userinfo is stored and parsed like any client's
    (name, etc.) **[read]**.
19. "Spawn" = the game's `ClientBegin` for the dummy's entity, called at enable time right
    after creation, and again on EVERY map change while a dummy exists; after it, the slot is
    marked spawned **[read]**. `ClientConnect` is not repeated across map changes **[read]**.
20. Each spawn appends `sv_mvd_begincmd` (if non-empty) plus a line end to the dummy's command
    buffer, and resets the "last scoreboard seen" clock to now **[read]**.
21. No failure-to-find-slot is silent: console error `No slot for dummy MVD client` **[read]**.

### A6. Every frame: the dummy's commands and its think

22. On each base frame where the recorder is enabled (only on frames aligned to the 10 Hz base
    when the server runs a faster tick), the recorder runs the dummy **before** it builds the
    stream's frame, in this order **[read]**:
    a. execute the dummy's command buffer until it is empty or a wait is pending;
    b. count the pending wait down by one, and reset the alias-loop counter;
    c. call the game's `ClientThink` for the dummy with an all-zero command whose duration is
       100 ms (so the game does not time it out);
    d. the scoreboard check (A7).
23. The dummy's `wait N` adds N base frames to the buffer's wait count (N below 1, or missing,
    counts as 1). There is no cap of 1000 for the dummy's own wait (the general console's
    wait has one) **[read]**.
24. Timing of the default `sv_mvd_begincmd` `wait 50; putaway; wait 10; help;` **[derived]**:
    at the first run after spawn, `wait 50` sets 50 and the same frame's countdown makes it
    49; it reaches 0 at the 49th frame after; on the 50th base frame after the first run
    `putaway` executes and `wait 10` starts; 10 base frames later `help` executes. So about
    5 s after spawn the scoreboard is closed, and about 1 s after that it is opened again.
25. How each dummy command line is handled **[read]**:
    - `cmd <rest>`: `<rest>` is sent to the game's `ClientCommand` as the dummy's command
      (developer line `dummy cmd: <rest>`);
    - `set` and `alias`: run as the server console's own `set` / `alias` (they change the
      SERVER's cvars and aliases);
    - `play`, `stopsound`, `exec`, `screenshot`: silently ignored;
    - `wait`: as above;
    - `record <name>` / `stop`: see A8;
    - a name that is a server alias: its text is inserted into the dummy's buffer (a runaway
      loop is stopped at the alias loop count with a warning naming the function and
      `runaway alias loop`);
    - a name that is a server cvar: handled as a cvar query / set on the server;
    - anything else (so `putaway`, `help`, `score`, ...): sent to the game's `ClientCommand` as
      the dummy's own client command (developer line `dummy forward: <line>`).
26. Rcon / console command `mvdstuff <text>`: appends `<text>` (outer quotes stripped) plus a
    line end to the dummy's buffer; without a dummy it prints
    `Can't 'mvdstuff', dummy MVD client is not active` (the command name as typed) **[read]**.

### A7. "The scoreboard has been gone a while": `sv_mvd_scorecmd`

27. The recorder keeps a "last layout time". It is set to now at every spawn of the dummy, and
    again every time a layout message (`svc_layout`) the game sends to the DUMMY is accepted
    into the stream **[read]**. Layout messages are only examined while the stream is active
    and the dummy passes the "player is active" test (A10; the dummy always passes when its
    field of view is nonzero) **[read]**.
28. On each dummy run, while the stream is active (not suspended) and `sv_mvd_scorecmd` is not
    empty, if more than 9000 ms have passed since the last layout time, `sv_mvd_scorecmd` plus
    a line end is appended to the dummy's buffer and the last layout time is reset to now
    **[read]**. So with the default `putaway; wait 10; help;` the dummy closes its scoreboard,
    waits 10 base frames (1 s) and asks for it again, at most once per 9 s while no layout has
    arrived **[derived]**.
29. No stat is checked for this (not `STAT_LAYOUTS`); the sole test is the time since the last
    layout message to the dummy **[read]**.

### A8. `record` / `stop` from the dummy (auto-recording)

30. Only when `sv_mvd_autorecord` is set; otherwise both are silently ignored **[read]**.
31. `record` without a name prints `Usage: record <filename>` **[read]**. With a name it opens
    `demos/<name>.mvd2` and prints `Auto-recording local MVD to <path>` **[read]**. Refusals:
    `MVD recording is disabled on this server.` and `Already recording a local MVD.` **[read]**.
32. `stop`: `Not recording a local MVD.` or `Stopped local MVD auto-recording.` **[read]**.

### A9. Stufftext and string commands: `sv_mvd_allow_stufftext` (latched, default 0)

33. The private message hook looks only at RELIABLE messages whose first byte is a stufftext;
    everything else the server sends the dummy is thrown away **[read]**.
34. When `sv_mvd_allow_stufftext` is 1 or more, the stufftext's text (cut to 1024 characters)
    is appended to the dummy's command buffer and runs by A6's rules (developer line
    `dummy stufftext: <text>`). At 0 or less it is discarded **[read]**.
35. The same setting gates relays' string commands: when it is 0 or more, the recorder clears
    the "string commands" permission a GTV relay asked for at handshake, so string commands a
    relay sends for the dummy are ignored (developer line
    `ignored stringcmd from <name>[<address>]`). Only a NEGATIVE value lets a relay's string
    command run, which is passed straight to the game's `ClientCommand` as the dummy
    (developer line `dummy stringcmd from <name>[<address>]: <text>`) **[read]**. A string
    command is also ignored when there is no dummy **[read]**.
36. So: -1 = relays may send the dummy commands, stufftext ignored; 0 = neither; 1 = stufftext
    executed, relay commands refused **[derived]**.

### A10. Which players are recorded: `sv_mvd_capture_flags` (default 5)

37. A player slot's state goes into the stream only when the "player is active" test passes.
    The test, in order **[read]**:
    a. if the game reports proper in-use flags (`GMF_PROPERINUSE`) and the entity is not in
       use: no;
    b. the entity has no client structure, or its number is out of client range: no;
    c. bit 1 (value 1) set: the server slot must be fully spawned (in game), else no. This
       excludes entities the game drives as clients with no server connection (bots);
    d. the player state's field of view is 0 (state not valid): no;
    e. the entity is the dummy: YES, always (after d);
    f. move type spectator: no, never;
    g. the entity is "invisible" (flagged not to be sent to clients, or with no model, no
       effects, no sound and no event): with bit 2 (value 2) set, no. Otherwise (visible):
       with bit 4 (value 4) set, YES immediately (steps h and i skipped);
    h. move type freeze (likely chasing someone): no;
    i. the "no prediction" move flag is set (likely chasing someone): no;
    j. otherwise yes.
38. Default 5 = bits 1 and 4: in-game slots only, and every visible player recorded; invisible
    ones fall through to the freeze / no-prediction tests **[derived]**.
39. The same test also decides whether data unicast to a player is kept (A11) and whether any
    player is "active" for the suspend timer (the dummy itself never counts) **[read]**.

### A11. What the stream leaves out: `sv_mvd_nomsgs` (latched, default 1)

40. Unicast data to a player that fails the active test is never recorded **[read]**.
41. Stufftext unicast to anyone is always dropped, except stufftexts beginning with `play `
    (sound hacks) **[read]**.
42. When there is NO dummy, nothing else is filtered **[read]**.
43. With a dummy: layout messages to anyone but the dummy are always dropped; text prints
    (`svc_print` via the game's per-client print or a unicast) to anyone but the dummy are
    dropped when `sv_mvd_nomsgs` is nonzero **[read]**. Centerprints and other unicast
    messages are kept **[read]**.
44. Broadcast prints (the game's "print to everyone") are always recorded, as their own stream
    op with their print level **[read]**.
45. The stream header carries a "no messages" flag (`MVF_NOMSGS`) when `sv_mvd_nomsgs` is set
    AND a dummy exists **[read]**.
46. A unicast over 2047 bytes is not recorded; warning naming the function and `overflow`
    **[read]**.

### A12. When it is removed

47. Disable: when no relay is streaming and nothing is recording for longer than
    `sv_mvd_disconnect_time` minutes (default 15; 0 = never), the recorder disables itself: the
    dummy is dropped with no reason text (so no drop message), which calls the game's
    `ClientDisconnect` because it was spawned, and its slot is freed **[read]**. Developer line
    `Disabling server MVD recorder.` **[read]**.
48. Server shutdown: the slot is freed WITHOUT a drop (no `ClientDisconnect`) **[read]**.
49. If the dummy is dropped by anything else (a kick, an error), the recorder treats it as fatal:
    console error `Fatal MVD server error: dummy client was dropped`, local recording stopped,
    every relay sent an error and dropped, recorder disabled **[read]**. The same fatal path is
    used for `Fatal MVD server error: gamestate overflowed` **[read]**.

### A13. Related timers and limits

50. `sv_mvd_suspend_time` (default 5, minutes; 0 = never): when no non-dummy player passes the
    active test for longer than this, the stream is suspended (relays get a suspend marker,
    developer line `Suspending MVD streams.`); it resumes as soon as one does
    (`Resuming MVD streams.`) **[read]**. The scoreboard check (A7) does not run while
    suspended **[read]**.
51. Stop lines for local recording: `Stopping MVD recording, maximum size reached.`,
    `Stopping MVD recording, maximum duration reached.`,
    `Stopping MVD recording, maximum number of level changes reached.`, and on a write error
    `Couldn't write local MVD: <error>` **[read]**.

## B. The relay side

### B1. `mvd_filter_version` (default 0)

52. Applies to text the relay broadcasts to a channel's viewers at print level CHAT, which in
    practice is the recorded server's broadcast chat (the stream's print op at chat level)
    **[read]**. Viewers' own `say` on the relay goes another way and is not filtered
    **[read]**.
53. When nonzero, every occurrence of `!version` in that text is rewritten to `!versi0n` (the
    letter o after the i becomes the digit zero), so bots and scripts listening for the
    `!version` trigger do not answer **[read]** (the replacement **[read]**, the purpose
    **[derived]**).

### B2. `mvd_stats_hack` (default 0)

54. It is a BIT MASK over stat indices 0 to 31 (`MAX_STATS_OLD`), not an on/off switch
    **[read]**.
55. While a viewer chases a player who is not the dummy, after the chased player's state is
    copied into the viewer's, each stat whose index bit is set in `mvd_stats_hack` is overwritten
    with the dummy's value for that stat; this needs a dummy in the stream **[read]**.
56. It runs every time the relay updates viewers, which is right after each parsed stream frame
    **[read]**.
57. A viewer NOT chasing (free flight) always gets ALL the dummy's stats copied, whatever this
    setting says (C) **[read]**.
58. After either copy, `mvd_stats_score` (default 0) overrides the frags stat: 0 sets it to 0,
    1 sets it to the channel's id, other values leave it **[read]**.

### B3. `mvd_freeze_hack` (default 1): intermission

59. Detection: after each parsed frame, the channel counts as "in intermission" when
    `mvd_freeze_hack` is nonzero, the stream has a dummy, and the dummy's move type is FREEZE.
    No frame-to-frame comparison is made; only the dummy's move type is read **[read]**.
60. Entering intermission: every spawned viewer's current chase target is saved, every viewer is
    forced to chase the DUMMY (so sees the dummy's view, which is the game's intermission
    view), and viewers who had no special layout up get the default layout, which in
    intermission is the scoreboard **[read]**.
61. During intermission: attack, jump / crouch, `invnext` / `invprev` and the observe commands
    cannot change target; the "player under the crosshair" pick is off; automatic chase by
    effects is off; the scoreboard is sent reliably **[read]**. A viewer who joins then is put
    on the dummy with the follow layout rules **[read]**.
62. Leaving intermission (the dummy's move type is no longer FREEZE): a viewer whose layout is
    the scoreboard has it closed; each viewer returns to their saved target if that player is
    still in use, otherwise to free flight **[read]**.
63. A map change also clears the intermission flag; viewers who were not on the dummy keep their
    target as the one to return to **[read]**. On a map change viewers are sent the stufftext
    `changing map=<map>; reconnect` **[read]**.
64. Separately, a `match ended` hack: a broadcast HIGH print containing `Match ended.` or equal
    to `Fraglimit hit.` + newline or `Timelimit hit.` + newline makes the next dummy layout of
    that frame be saved as the "old scores" (`oldscores` command) **[read]**.

### B4. `[MVD] <old> changed name to <new>` and the leave messages

65. All of these go through the relay's channel broadcast, at print level MEDIUM, to every
    viewer of THAT channel who is spawned, whose `messagelevel` allows MEDIUM, and who has not
    set the "mute misc" bit (bit 5, value 32) of their `uf` userinfo key **[read]**. Each is one
    reliable `svc_print` **[read]**.
66. All of them are sent only when the viewer has begun in a channel (has a begin time) and
    `mvd_part_filter` lets them through **[read]**. `mvd_part_filter` (default 0): 0 = always;
    negative = never; positive N = only for relay admins or for viewers whose most recent chat
    line was less than N seconds ago (a viewer who never spoke is not announced) **[read]**.
67. Name change: when a viewer's userinfo `name` changes and the old name was not empty, text
    `[MVD] <old> changed name to <new>` followed by a newline **[read]**. It replaces the
    normal server's name-change broadcast (`sv_show_name_changes` is not consulted on a relay)
    **[read]**. A dedicated relay's console also gets `<old>[<address>] changed name to <new>`
    **[read]**.
68. Voluntary leave (the viewer's `disconnect`): the drop is made silent, then the relay's
    disconnect hook sends `[MVD] <name> disconnected` followed by a newline **[read]**.
69. Forced leave of a viewer who was spawned (kick, time-out, errors): instead the text is
    `[MVD] <name>` + the drop prefix + the reason + newline, for example
    `[MVD] <name> was kicked`, `[MVD] <name> timed out`,
    `[MVD] <name> was dropped: <reason>` **[read]**. After it the begin time is cleared, so the
    `disconnected` line is NOT sent as well (one message per leave) **[read]**.
70. Channel moves: `[MVD] <name> left the channel` to the old channel (when switching away
    after at least 2 s) and `[MVD] <name> entered the channel` to the new one on first begin
    there, same level, mask and filter **[read]**.
71. Lines a viewer alone gets: `[MVD] Chasing <name>.` (LOW) on starting to chase,
    `[MVD] No players to chase.` (MEDIUM), `[MVD] Please enter a channel first.` (HIGH),
    `[MVD] GTV server doesn't have this map!` and `[MVD] Buffering data, please wait...` (HIGH,
    once, 2 s after joining) **[read]**.
72. Routing of the recorded server's unicast prints: each viewer receives the prints unicast to
    the player they chase, or the dummy's when they are not chasing; when the stream has the
    "no messages" flag every viewer receives the dummy's prints instead **[read]**. Chat-level
    prints are skipped for viewers with the "mute players" `uf` bit (bit 3) **[read]**.

## C. Free flight on the relay

73. A viewer who is not chasing anyone moves with the game's player move at move type
    SPECTATOR on a real channel; in the waiting room the move type is FREEZE (no movement)
    **[read]**.
74. The move runs on every user command the viewer sends, with a trace that never hits anything
    and point contents that are always empty; the spectator fly move does not trace at all.
    So free flight passes through walls (noclip-like) **[read]**.
75. The spectator fly move **[read]**: view height 22; friction `flyfriction` times the larger
    of speed and 100, times the frame time; wish velocity = forward and side input along the
    view's forward and right vectors, plus the up input added straight to vertical; wish speed
    clamped to the move's maximum speed (300, or 320 when the server's `sv_qwmod` is on for a
    Q2PRO-protocol viewer); acceleration 10 times frame time times wish speed, not past the wish
    speed; origin += velocity times frame time; then the position is snapped to the 1/8 unit
    grid **[read]**.
76. Per-viewer parameters by protocol **[read]**: stock protocol: time multiplier 1, fly
    friction 9, the original behaviour (no acceleration at all when already at or above the wish
    speed along the wish direction, so friction alone acts that frame). R1Q2 and Q2PRO
    protocols: the frame time is doubled (`speedmult` 2), so the viewer flies twice as fast.
    Q2PRO protocol additionally: fly friction 4 and the "fly hack" (velocity still integrated
    when no acceleration is added).
77. View angles follow the command's angles (the move's view angles are copied when the move
    type is not FREEZE) **[read]**.
78. Switching **[read]**:
    - pressing attack (the button newly down) toggles: if chasing, stop and fly free from where
      the view is; if free, first pick the player under the crosshair (a line of 8192 units
      from the view point, stopped by solid world; the nearest in-use, box-solid, non-dummy
      player whose bounding box it crosses) as the one to return to, then chase
      that one or the last target, or the next player; the observe commands do the same
      toggle: `observe`, `spectate`, `observer`, `spectator`, `obs`, `spec`;
    - `follow` / `chase` start chasing a named or numbered player; `autofollow` / `autochase`
      turn on automatic chasing;
    - while chasing, jump (up input 10 or more) and crouch (-10 or less) move to the next or
      previous player, as do `invnext` / `invprev` with no menu open.
    No setting gates free flight; it is always allowed outside intermission **[read]**.
79. Stopping a chase: roll zeroed, the delta angles set so the view stays where it was, kick and
    screen blend cleared, move flags cleared, move type SPECTATOR, gun model removed, field of
    view = the viewer's own `fov` userinfo (1 to 160, default 90), the dummy's configstrings
    re-sent, the viewer's client number shown as the dummy's, and a follow layout replaced by
    the default **[read]**.
80. What the free viewer sees **[read]**: their own origin and angles; all stats copied from the
    dummy each frame (so the dummy's HUD); underwater view when the view point (origin plus view
    offset) is in water, slime or lava; a full-screen tint: solid or lava red-orange
    (1, 0.3, 0, alpha 0.6), slime dark green (0, 0.1, 0.05, alpha 0.6), water brownish
    (0.5, 0.3, 0.2, alpha 0.4), none otherwise; damage blend cleared. The stats-layouts flag
    shows whether a layout is up **[read]**.
81. A viewer joining a channel where no target is available starts free at the map's
    `info_player_intermission` origin and angles, else the last `info_player_start` or
    `info_player_deathmatch` in the entity text, and waits to return to their previous target
    once it is back in use **[read]**. No message is printed on entering free flight **[read]**.
82. A change in buttons or movement input counts as viewer activity for the relay's idle
    timer; chasing a real player does too **[read]**.
