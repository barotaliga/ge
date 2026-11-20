# Recording matches automatically, demo naming and playback settings (clean-room note)

WORKING AREA: deleted when porting ends. M18k4 round 14, client settings: the behaviour behind
the demo and match-tool settings of `client-inventory.md` § 1.9 (ezQuake 3.6.9 for QuakeWorld:
automatic match recording and console logs, the name patterns, `easyrecord` and `record`, the
demo clock, jumps, the start-up demo, the third-person camera) and the Action Quake fork of
Q2PRO for Quake II (its automatic recording, demo snapshots, message length, waiting at the end,
suspend and resume, `seek`). Written by a clean-room reader in its own words, with no code; each
fact is marked [read] (the source says it) or [derived] (worked out from read facts). Nothing was
measured by running a binary. Quoted lines are exactly what the player sees.

## A. Recording a match on its own

### 1. What starts it, what it prints, modes 1 and 2, `match_save`

- The trigger is the server info key `status`. The client keeps a "standby" flag that is true
  only while `status` equals `standby` (case-insensitive); `countdown` sets a separate flag.
  [read]
- Once per client frame, while connected and active and not playing a demo, the client
  compares this frame's standby flag with last frame's. A match starts when last frame was
  standby, this frame is not, and the client is not in an intermission. [read]
- Consequence: the start fires when `status` leaves `standby`, which on a server that counts
  down is the moment the countdown begins, not the moment play begins. [derived]
- The remembered flag is cleared on every new map and on disconnect, so a client that joins
  while a match is already in play (never having seen `standby` on this map) does not start
  anything. [derived]
- There is no "matchdate" print or any chat print involved. [read]
- The command `match_forcestart` (no arguments) starts a match by hand; when not connected
  or while playing a demo it prints "You must be connected to a server before using
  "match_forcestart"", with any argument "match_forcestart : no arguments expected". [read]
- On a start, any match already running is cancelled first (see 2), the game clock is reset
  to zero, the match's name is worked out once from the patterns (section B; players, teams
  and the local date and time at that moment) and kept for the whole match, then recording
  and console logging start and the per-match statistics reset. [read]
- `match_auto_record` 0 (default): no recording. Non-zero: recording starts into a temporary
  file. Printed: "Auto demo recording commenced". [read]
- If a demo is already being recorded by hand: "Auto demo recording skipped (already
  recording)" and nothing else happens. If an automatic recording from an earlier start is
  still running, it is closed and a new one begins silently. [read]
- If the temporary file cannot be opened even after creating its folders: "Error: Couldn't
  open <path>" then "Auto demo recording failed to start!". [read]
- Mode 1 (keep until saved): when the match ends the recording stops and "Auto demo
  recording completed" is printed; the file stays in the temporary place until `match_save`.
  [read]
- Mode 2 (save at the end): when the match ends the file is moved to its final name at once
  ("Match demo saved to <name>", see 4), then "Auto record ok" is printed. [read]
- The temporary file has one fixed name, so in mode 1 an unsaved recording is overwritten
  when the next match starts. [derived]
- `match_save` (no arguments) checks three things in order: if a console log upload is still
  pending it prints "Log upload is still in progress" and stops. If a finished recording or a
  finished console log is waiting, it prints "Saving match..." (shown in the highlighted
  colour) and saves both (each prints its own "saved to" line). Otherwise, if either is still
  running it prints one of "Auto demo recording and console logging still in progress",
  "Auto demo recording still in progress" or "Auto console logging still in progress",
  and otherwise "Nothing to save!". [read]
- After a mode 2 save the recording no longer counts as waiting, so `match_save` then saves
  only a waiting console log, or prints "Nothing to save!". Because `match_auto_logconsole`
  defaults to 1, a waiting log is the usual case. [derived]

### 2. What stops it; `match_auto_minlength`

- Normal end: the client enters an intermission (the server's intermission or finale
  message). At that moment the auto screenshot is taken (if `match_auto_sshot` is on), and
  the recording and log are stopped 4 seconds of real time later, so the scoreboard is in the
  demo. [read]
- Cancel: any of these while a match is running: `status` returns to `standby` (a broken
  countdown or an aborted match), a new map begins loading, the client disconnects, or a new
  match start. If the 4-second end was still pending, it is completed first as a normal end,
  so a map change right after the intermission loses nothing. [read]
- `stop` during an automatic recording ends it like a normal end (mode 2 saves and prints
  "Auto record ok"; mode 1 prints "Auto demo recording completed"). [read]
- `match_auto_minlength` (default 30, seconds of real time since the recording began) is
  consulted only on a cancel and only in mode 2: longer than the minimum, the demo is saved as
  at a normal end (but without "Auto record ok"); not longer, it prints "Auto demo recording
  cancelled" and does not save. [read]
- A cancelled short demo is not deleted: it stays in the temporary place, counts as waiting,
  so `match_save` can still keep it, and the next match overwrites it. [derived]
- A cancel in mode 1 prints "Auto demo recording completed" whatever the length. A normal end
  never checks the minimum length. [read]
- The same minimum, with the same rule, applies to the console log in its mode 2 ("Auto
  console logging cancelled"). [read]

### 3. `match_auto_spectating`

- At 0 (default), while the client is a spectator the match start is skipped entirely: no
  recording, no console log, no match name taken, no statistics reset. The auto screenshot is
  skipped too. Nothing at all is recorded automatically while spectating. [read]
- At non-zero, a spectator's matches are handled exactly like a player's, and the name's `%n`
  gets the spectator suffix (see 8). [read]
- Independently, `match_auto_unminimize` (default 1) restores a minimised window at a match
  start: 1 only when playing, 2 also when spectating. It acts even when spectating is not
  recorded. [read]

### 4. Folder, extension, format, taken names

- Folder: `demo_dir`. Empty (default): the current game folder (for example the mod's folder
  the client is running). Not empty: depends on `cl_mediaroot` (default 0): 0 places it under
  the install's base folder, 1 under the user's home folder for the client, 2 takes it as an
  absolute path. Leading slashes are dropped for 0 and 1; backslashes become slashes. [read]
- Setting `demo_dir` to a name with forbidden characters is refused with "<name> is not a
  valid filename (?*:<>" are illegal characters)". [read]
- The automatic recording is written first to `<demo folder>/temp/_!_temp_!_.qwd`. [read]
- Slashes in the match name (every default pattern starts with a folder such as `duel/` or
  `tdm/`) become subfolders of the demo folder; missing folders are created when the first
  move or open fails. [read]
- Every recording is written as `.qwd` whatever `demo_format` says; conversion happens after
  the file is saved. [read]
- `demo_format` takes exactly `qwd`, `qwz` (default), `mvd`, `mvd.gz`, `qwd.gz`. Anything else
  is refused: "Not valid demo format. Allowed values are: qwd, qwz, mvd, mvd.gz, qwd.gz." [read]
- On a save with `qwz` or `mvd`: "Converting QWD to <format> format." then a line naming the
  helper (in red) and its command line. `qwz` runs an external proxy program from the folder
  `qizmo_dir` (default `qizmo`, under the base folder); `mvd` runs an external converter from
  `qwdtools_dir` (default `qwdtools`). The client does neither conversion itself. [read]
- When the helper finishes and its output exists and is not empty: "Demo saved to <path from
  the base folder>" and the `.qwd` is deleted. If the output is missing or empty: "Compression
  failed, demo saved as QWD." If the helper dies unexpectedly, a warning line saying it
  terminated unexpectedly is printed and any partial output is deleted. If a
  helper is still running from before: "Cannot compress -- Qizmo still running!". [read]
- Helpers can be launched only on Windows and Linux builds. Elsewhere, or when the launch
  fails: "Failed to compress <path>", the `.qwd` stays, and "Match demo saved to <name>" is
  printed. [read]
- With `qwd`, `mvd.gz` or `qwd.gz`, an automatic save keeps the `.qwd` with no conversion and
  no conversion message; the two `.gz` values are accepted but nothing gzips. [derived]
- Taken names: the final name is always `<match name>_NNN.qwd`, NNN a three-digit number. The
  client tries 000, 001, ... up to 999 and takes the first number for which none of
  `<name>_NNN.qwd`, `.qwz` or `.mvd` exists. So the first save of a name is `_000`. Past 999:
  "Error: no available filenames" and the demo stays in the temporary place. [read]
- Success line for the move: "Match demo saved to <match name>_NNN.qwd" (name relative to the
  demo folder), suppressed when a conversion helper was launched (the helper's own line comes
  later). [read]

### 5. `match_auto_logconsole`

- Default 1. 0: no console log (a ladder-challenge feature of the client can still force one).
  1: logged and kept until `match_save`. 2: saved at the end. [read]
- What is written: every line printed to the console, from the match start to the stop (the
  normal end 4 seconds into the intermission, or a cancel). The start line "Auto console
  logging commenced" itself is printed before logging begins. [read] [derived]
- With `log_readable` 1 (default), coloured and special glyphs are folded to plain readable
  characters before writing; at 0 the raw bytes are written. [read]
- The text is held in memory (starting at 128 KiB and growing) and written to disk only when
  logging stops, to avoid frame hitches. [read]
- Temporary file: `_!_temp_!_.log` in a `temp` folder under the client's home folder. [read]
- Final place: `log_dir` (empty means the current game folder; `cl_mediaroot` applies as for
  demos), name `<match name>_NNN.log`, NNN the first free number checking `.log` only (so a
  demo and its log can get different numbers). [read]
- Messages: "Auto console logging commenced", "Auto console logging completed", "Auto console
  logging cancelled" (mode 2, shorter than the minimum), "Auto console logging skipped
  (already logging)" (a manual `log` is running), "Match console log saved to <name>_NNN.log",
  "Error: Couldn't open <path>", "Not enough memory to allocate log buffer", "Error: Can't save
  the log. Log upload is still pending.", "Error: no available filenames". [read]
- While an automatic log runs, the manual `log` command with no argument prints "Auto console
  logging is in progress", and starting another prints "Auto console logging must be stopped
  first!". [read]

## B. Name patterns

### 6. The %-codes

The name is built from the pattern by replacing each code; anything else is copied. [read]

- `%n`: your name, or `match_name_nick` if that is not empty; when spectating,
  `match_name_spec` is appended right after it with no separator. [read]
- `%p`: player 1: when playing, yourself; when spectating, the first non-spectator player in
  slot order. [read]
- `%e`: when exactly two players are in the game, player 2; otherwise team 2. Player 2 when
  playing is the first non-spectator player whose name differs from yours; when spectating,
  the second non-spectator player in slot order. [read]
- `%t`: team 1: when playing, your team; when spectating, the first team seen in slot order.
  Team 2 likewise: the first other team that differs from yours, or the second distinct team
  seen. [read]
- `%k`, `%l`: the names of team 1's and team 2's players, in slot order, joined by
  `match_name_and`; each name cleaned (see 9). [read]
- `%O`: team 1's player count when the server's `teamplay` is non-zero, else `1`. [read]
- `%E`: team 2's player count when `teamplay` is non-zero, else the player count minus one.
  [read]
- `%C`: the number of non-spectator players. [read]
- `%a`: each team's player count, in the order teams were first seen, joined by
  `match_name_on` (for example `4on3on4`). [read]
- `%b`: every team's name in that order joined by `match_name_versus`. [read]
- `%v`: the value of `match_name_versus`. [read]
- `%T`, `%F`, `%N`, `%D`: the server info's timelimit, fraglimit and teamplay as whole
  numbers, and the client's deathmatch mode. [read]
- `%M`: the current map's name. `%G`: the game folder's name (for example `qw`, `fortress`).
  [read]
- `%d`, `%m`, `%y`, `%Y`, `%H`, `%Q`, `%S`: local day, month, two-digit year, four-digit year,
  hour (24-hour), minute, second at the match start, each zero-padded to two digits (four
  for `%Y`). Note the minute is `%Q`, not `%M`. [read]
- `%%`: a single `%`. An unknown code such as `%x` is kept as the two characters `%x`; a lone
  `%` at the end is kept. [read]
- Missing values: a name or team that does not exist is written as nothing (an empty
  string); a missing server info number is written as `0`. [read] [derived]
- The listing command `match_format_macrolist` prints the codes with a short line each; it
  lists `%p` twice and calls the second one "teamplay setting", but the teamplay code that
  works is `%N`. [read]
- Before the client is fully in a game, the name is the text "No match in progress". [read]
- The name is also available as the macro `$matchname` and the type nickname as
  `$matchtype`. [read]

### 7. How the match type is decided

Decided from the non-spectator players, their team strings and server info, in this order, and
the first rule that fits wins. [read]

1. No players: empty (pattern "%n - [%M]").
2. Game folder `qw` (any case) and the server info key `race` equals the map name: race.
3. One player: solo.
4. A Team Fortress game (the client's own detection): two players: tf duel; fewer than two
   teams: unknown; otherwise tf clan war.
5. Game folder name containing `arena`: arena.
6. Deathmatch 0: coop.
7. Two players: duel when teamplay is 0 or there are exactly two teams; otherwise unknown.
8. Teamplay 0, or the biggest team has at most one player: ffa.
9. Not exactly two teams: more than two is multiteam, fewer is unknown.
10. Two teams: biggest team 2 is 2on2, 3 is 3on3, 4 is 4on4, anything else tdm.

- Teams are the distinct team strings of non-spectator players; an empty team string counts
  as a team but not when measuring the biggest team. [read]
- When unsure the result is unknown, named by the fixed pattern "%n - [Unknown Game, gamedir
  - %G, %C players] - [%M]". [read]
- `$matchtype` nicknames: duel, ffa, 2on2, 3on3, 4on4, tdm (also for multiteam), arena,
  tfduel, tfclanwar, solo, coop, race, empty, unknown. [read]
- Default patterns: solo `solo/%n - [%M]`; coop `coop/%n - [%C_player_coop] - [%M]`; race
  `race/%n - [race] - [%M]`; duel `duel/%n - %p%v%e - [dmm%D] - [%M]`; ffa
  `ffa/%n - [%C_player_ffa] - [%M]`; 2on2 `2on2/%n - [%k%v%l] - [%M]`; 3on3, 4on4 and tdm
  `tdm/%n - [%Oon%E_%t%v%e] - [%M]`; multiteam `tdm/%n - [%a_%b] - [%M]`; arena
  `arena/%n - %p%v%e - [%F_frags] - [%M]`; tf duel `tfduel/%n - %p%v%e [%M]`; tf clan war
  `tfwar/%n - [%Oon%E_%t%v%e] - [%M]` (the settings `match_format_<type>`). [read]
- The auto screenshot is taken only for duel, ffa, 2on2, 3on3, 4on4, tdm, multiteam, arena
  and the two tf types, and only when another player besides you is present ("Auto
  screenshot cancelled" otherwise). [read]

### 8. The name words

Defaults [read]: `match_name_and` `_&_`; `match_name_versus` `_vs_`; `match_name_on` `on`,
`match_name_nick` empty, `match_name_spec` `(SPEC)`.

- and: between the player names inside `%k` and `%l`. [read]
- versus: between team names inside `%b`, and wherever `%v` stands. [read]
- on: between the counts inside `%a`. (The `on` in the default `%Oon%E` is literal text in the
  pattern, not this setting.) [read]
- nick: replaces your own name in `%n` when not empty. [read]
- spec: appended to `%n` when you are a spectator, so the default spectating duel is
  `duel/Name(SPEC) - ...`. [read]
- The words are inserted as typed; the whole name is cleaned afterwards (9), so a space or `|`
  in a word becomes `_`. [derived]

### 9. Characters removed or replaced; length

- Each player name, team name and your own name is cleaned before it enters the name: every
  character first goes through the console's readable-character folding (coloured letters
  become plain letters; the gold digits and brackets become plain digits and brackets; the
  other special glyphs mostly become `_` or `.`); then any character below space, a space, a
  backslash, a slash, or one of `? * : < > " |` becomes `_`. [read]
- Runs collapse: after an `_` is written, any following characters that would also become `_`
  are skipped, so `a  b` and `a__b` both become `a_b`. [read]
- Then the whole name is cleaned the same way except that spaces and slashes are allowed
  (spaces stay; slashes make folders); a backslash becomes a slash; a leading slash is
  removed. [read]
- A slash inside a player's name therefore never makes a folder: it became `_` in the first
  pass. [derived]
- Limits: each field and the whole name are cut at 1023 characters, silently; the full path
  is then limited by a 1024-byte path buffer. No shorter limit applies. [read] [derived]

## C. `easyrecord` and `record`

### 10. `easyrecord`; what `record` prints

- `easyrecord [demoname]` exists. Not fully in a game: "You must be connected to easyrecord".
  More than one argument: "Usage: easyrecord [demoname]". [read]
- With no argument the name is the current match name from the same patterns (taken now, not
  at a match start); with an argument, that text. [read]
- It refuses during multiview demo playback ("Cannot record during mvd playback"), while an
  automatic recording runs ("Auto demo recording must be stopped first!"), and on a name with
  forbidden characters ("<name> is not a valid filename (?*:<>" are illegal characters)").
  A manual recording in progress is stopped first. [read]
- It writes to the demo folder as `<name>_NNN.qwd`, the same free-number rule as in 4, and
  prints "Recording to <name>_NNN.qwd". [read]
- When it is stopped, the conversion of 4 is attempted whatever `demo_format` is. With `qwz`
  or `mvd` the helper runs; with any other value it prints "<format> demo format not yet
  supported." and "Failed to compress <path>", and the `.qwd` stays. No "Completed demo" line
  is printed for an easyrecord stop. [read] [derived]
- With the client's own built-in server running, `easyrecord` instead records a server-side
  multiview demo: names `duel_<a>_vs_<b>_<map>`, `<n>on<m>_<team1>_vs_<team2>_<map>` or
  `ffa_<map>(<count>)`, a two-digit `_NN` from 01 when taken, extension `.mvd`. [read]
- `record` with no argument prints "Auto demo recording is in progress", "Recording to
  <name>" or "Not recording". [read]
- `record <name>`: while connecting, "Cannot record whilst connecting"; while an automatic
  recording runs, "Auto demo recording must be stopped first!"; a manual recording in progress
  is stopped first ("Completed demo"); bad characters give the invalid-filename line; the name
  is forced to end in `.qwd`; on failure "Error: Couldn't record to <path>. Make sure path
  exists."; on success "Recording to <name>.qwd". It also works while disconnected: the
  header is written when the game becomes active. [read]
- More than one argument: "Usage: record [demoname]". [read]
- With protocol extensions on (and `cl_pext_warndemos` on), `record` first warns that the demo
  may be unplayable in older clients and suggests `cl_pext 0`. [read]
- `stop` when nothing records: "Not recording a demo". [read]
- Every demo the client stops writes a sign-off print into the file before the end:
  a framed block reading "Recorded by ezQuake (http://www.ezquake.com/)", "Discord:
  http://discord.quake.world/", "Forums: http://quakeworld.nu/". [read]

## D. Playback conveniences

### 11. `cl_democlock`

- Draws a text clock only during demo playback (not QTV), not when the new element HUD is on
  (`scr_newhud` 1, which has its own `democlock` element), and not while the scores are
  shown. Default 0. [read]
- 1: the time since the demo became active (its first full frame), whole seconds truncated,
  as `hh:mm:ss` with two digits each (the hours wrap after 99). 2: the demo's own raw clock
  (time since the recording's clock began, not since playback became active), same format.
  [read]
- Neither counts from the match start; the match clock is a different element. [read]
- Position, in 8-pixel character cells: `cl_democlock_x` (default 0) from the left, or when
  negative from the right edge so the text ends that many cells in; `cl_democlock_y`
  (default -2) from the top, or when negative upward from the top of the status bar area, so
  the default sits two lines above the status bar. [read]
- The element HUD's `democlock` shows `mm:ss` of time since the demo became active (minutes
  wrap at 60), though its description says "hh:mm:ss". [read] [derived]

### 12. `demo_jump`, `demo_jump_rewind`, `demo_jump_skip_messages`

- Syntax: `demo_jump [+|-][m:]<s>`. No sign: absolute, counted from when the demo became
  active. `+`/`-`: relative to the current time. One colon allowed: digits before it are
  minutes, after it seconds; seconds may exceed 59 (`90` is 1:30). A second colon, or any
  non-digit, gives the usage line. Hours are not accepted. [read]
- Messages: "Error: not playing a demo", "Error: cannot jump during QTV playback.", "Error:
  demo must be active first", "Usage: demo_jump [+|-][m:]<s> (seconds)". A successful jump
  prints nothing; the notify lines on screen are cleared. [read]
- Forward: the demo is read fast up to the target. Backward: the demo file is restarted from
  its first byte and read fast to the target; the tracked player and the free camera's
  position and angles are saved first and put back when the target is reached. [read]
- While seeking, with `demo_jump_skip_messages` 1 (default) the prints the demo carries are
  not shown; at 0 they are shown. In multiview demos, prints not meant for the tracked player
  are hidden either way. [read]
- `demo_jump_rewind` (default -10) is not used by plain `demo_jump`. It applies when a search
  finds its point: `demo_jump_mark` (seek to the next demo mark the recorder inserted) and
  `demo_jump_status <conditions>` (seek until the tracked player's items, weapon, health or
  ammo match). When negative, playback then jumps back that many seconds before the found
  point; at 0 or positive it stays at the found point. [read]
- `demo_jump_end` seeks to 2 seconds before the end (the length rounded up, minus 2); it
  refuses with "Error: too close to end of demo" when the target is less than 2 seconds away
  (10 when already in an intermission). If an intermission is reached during that seek,
  playback stops there and is paused (`cl_demospeed` set to 0) instead of rewinding. [read]

### 13. `cl_startupdemo`

- Default empty. When set, at start-up (after the configs and the command line run) the
  client issues `playdemo <value>`, but only when no demo or broadcast file was given on the
  command line. [read]
- It plays once; nothing restarts it. When it ends, the demo playlist's next entry plays if a
  playlist is set; otherwise playback simply stops. [read] [derived]
- While it plays, the first key press does what `cl_onload` (default `menu`) names: `menu`
  opens the menu, `browser` the server browser, `console` or anything else drops to the
  console. Key bindings do not fire for that press. With a value other than these three, input
  returns to the game once the demo is active. The demo keeps playing behind the menu or
  console; leaving it is done by `disconnect` or starting something else. [read] [derived]

### 14. `demo_spawnwarn`

- Open: ezQuake 3.6.9 has no setting, command or text by this name, nor any spawn warning
  tied to demos (the whole source was searched). [read]

### 15. `cam_thirdperson`, `cam_dist`, `cl_chasecam`

- `cl_chasecam` (default 1): when a spectator is locked onto a tracked player, 1 shows the
  view through that player's eyes (their angles, their view weapon drawn); 0 keeps the classic
  free spectator camera that flies to a spot from which the player is visible and turns to
  face them. [read]
- `cam_thirdperson` (default 0) acts only when the view follows another player (a spectator
  tracking, or a multiview demo): the camera sits `cam_dist` units (default 100) behind the
  tracked player's position along the viewer's own facing direction, pitch included, at the
  player's eye height (the normal view height is added afterwards). The viewer's mouse turns
  the camera around the player; the player's own view angles are not copied. [read]
- No trace is made, so at short range from a wall the camera passes into or through it.
  [derived]
- Forward and back keys change `cam_dist` while it is on (never below 0), accelerating at
  `cam_zoomaccel` (default 2000 units per second squared) up to `cam_zoomspeed` (default 300
  units per second). `cam_lockpos` 1 freezes the camera's position and turns it to look at the
  player instead. [read]
- In a recording made as a player (not a spectator) the view is your own, so it has no
  effect there. [derived]
- The separate `cl_camera_tpp` 1 is the other third-person view: 56 units behind the eye and
  24 above (`cl_camera_tpp_distance` -56, `cl_camera_tpp_height` 24), with a wall trace that
  pulls the camera in to 8 units in front of the hit and 4 off the surface. [read]

## E. The Q2PRO fork's own demo settings

### 16. Automatic recording

- The client has no `cl_autorecord` or other automatic recording setting. [read]
- The bundled Action game library has a player command `autorecord`: each time it is typed
  the server makes the client run `record "<name>"`. Name: the server's local date and time as
  four-digit year, English three-letter month, day, hours and minutes joined as
  `YYYY_Mon_DD_HHMM`, then `-<map>`; in match mode `-<team1>_vs_<team2>-<map>` (three teams:
  `<t1>_vs_<t2>_vs_<t3>`) with spaces removed. Quotes, backslashes, `;`, `$` and control
  characters become `_`; the name is cut at 63 characters. The client then adds `.dm2` in its
  `demos/` folder. [read]
- The same game library can start a server-side multiview recording on its own when
  `use_mvd2` is set: `<YYYYMMDD-HHMMSS>-<mode>[-<flag>][-<teams>]-<port>-<map>.mvd2.gz`,
  announced as "Starting MVD recording to file <name>.mvd2.gz"; deathmatch with no time or
  frag limit is not recorded. [read]

### 17. `cl_demosnaps`, `cl_demomsglen`, `cl_demowait`, `cl_demosuspendtoggle`, `seek`, `suspend`/`resume`

- `cl_demosnaps` (default 10, seconds): during playback a snapshot of the state (frames,
  configstrings, the layout) is kept in memory every that many seconds of demo (10 server
  frames per second), made as playback passes. 0 or less: none. [read]
- Backward seeks start from the nearest earlier snapshot; with none: "Couldn't seek backwards
  without snapshots!". So a backward seek reaches only parts already played past a snapshot.
  [read] [derived]
- `cl_demomsglen` (default 1390 bytes: the standard 1400-byte packet less the 10-byte header):
  the size of each recorded frame's message buffer, clamped to 512 to 32768. [read]
- `record -e` uses the extended size (4086) and `-s` the standard (1390); on an extended
  protocol connection 32768 is used regardless. [read]
- A frame that does not fit is dropped. During the first 10 written frames, every 50th drop
  warns "Too many demo frames don't fit into <N> bytes!" and the 50th adds "Try to increase
  'cl_demomsglen' value and restart recording.". [read]
- `cl_demowait` (default 0): at the end of a demo, 0 ends playback ("Demo finished" as the
  disconnect reason, or the `nextserver` command runs if set); 1 holds on the last frame
  instead, and a forward seek there does nothing. [read]
- `cl_demosuspendtoggle` (default 1): `suspend` while already suspended resumes; at 0 it
  prints "Demo recording is already suspended.". [read]
- `suspend`: "Suspended demo recording." While suspended no frames or other messages are
  written, but configstrings that change are remembered. [read]
- `resume`: writes the remembered configstrings, then a complete frame, and continues; it
  prints "Resumed demo recording."; when not suspended "Demo recording is already resumed."
  [read]
- Both `suspend` and `resume` without a recording: "Not recording a demo.". The demo plays
  through the gap as a jump in time. [read] [derived]
- `seek [+-]<timespec|percent>[%]`. Without a sign: from the demo's first frame; with `+`/`-`:
  relative. [read]
- A timespec is `S`, `S.F`, `M:S` or `M:S.F`, where F counts tenth-second frames as a whole
  number (so `1.5` is 1.5 s but `1.25` is 3.5 s). [read] [derived]
- A value with `%` seeks to that share of the file's bytes, clamped to 0 to 100. [read]
- `seek` messages: "Usage: seek [+-]<timespec|percent>[%]", "Not playing a demo.", "Invalid
  percentage.", "Unknown file size, can't seek.", "Invalid relative timespec.", "Invalid
  absolute timespec.", "Couldn't seek demo: <reason>". Success prints nothing. [read]
- A seek of zero frames does nothing. While seeking, sounds are stopped and effects are not
  run; afterwards effects are cleared and the view does not blend from the old frame. If a
  demo is being recorded during playback, it continues after a seek with the changed
  configstrings written. [read]
- When the client hosts a multiview broadcast, `seek` is passed to `mvdseek`. [read]
- What the player sees: `scr_demobar` (default 1) draws a progress bar along the bottom by file
  position, with the time as `m:ss.f` (f in tenths), and "[PAUSED]" when paused. [read]
- `record` options: `-z` gzip (adds `.gz`), `-e`, `-s`, `-h`. Its messages are "Already
  recording (<size>, m:ss.f[, N frames dropped][, N messages dropped]).", "Missing filename
  argument.", "You must be in a level to record.", "Recording client demo to
  demos/<name>.dm2."; `stop` prints "Stopped demo (<size>)." or "Not recording a demo.".
  [read]
- Characters that are not printable (and on Windows `< > : " | ? *`) become `_` in the file
  name. No free-number step is taken, so an existing file of that name is written over.
  [read] [derived]
- Other `cl_demo*` settings: none in this fork. [read]
