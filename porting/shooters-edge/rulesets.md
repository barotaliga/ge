A WORKING AREA note (`porting/`), deleted when porting ends.

# Competition rulesets and fairness checks: clean-room facts

Clean-room facts, in a reader session's own words with no code (`clean-room` skill), about the
QuakeWorld client's competition rulesets and fairness queries, what the QuakeWorld server and
its match mod do with them, and the Quake II equivalents, kept for Shooter's Edge's classic
settings and server rules. Projects read at their default branch heads on 2026-11-22: ezQuake
(QW-Group/ezquake-source, commit 5d92ab01491f, version text "3.7.0-dev"), MVDSV (QW-Group/mvdsv,
commit 70f1135699b7), KTX (QW-Group/ktx, commit b1b23a985dc2) and Q2PRO (q2pro/q2pro, upstream,
not a fork; commit 601a8df8433b of 2025-12-11) with its client and server manuals. Questions are
numbered as in the brief; `client-inventory.md` § 1.19 holds what was known before.

Marks: **(R)** read in the source or its manual, **(D)** derived from read facts (reasoning
given), **(M)** measured (none: nothing was run). Words in backticks are names a player types or
sees.

Two corrections to `client-inventory.md` § 1.19 came out of this reading; they are flagged where
they arise (Q7 roll angle, Q9 eyes and backpack).

"Strict sets" below means `smackdown`, `smackdrive`, `thunderdome` and `qcon`. "Playing" means
connected and fully in game, not a spectator, not watching a demo, and the server info key
`status` is not `Standby` (compared without case). Most strict-set restrictions apply only while
playing; the countdown counts as playing (R).

## Part A: ezQuake

### 1. Refuse, clamp or force back; the lines printed

- A locked setting is made read-only. Any attempt to set it (console, config, menu, script) is
  refused, the value is unchanged and the line is `"<name>" is write protected` (R).
- A capped setting (only `mtfl` has caps: `gl_picmip` at most 3, `gl_max_size` at least 512)
  refuses a value past the cap, keeps the old value, and prints `max "<name>" is limited to
  <cap>` or `min "<name>" is limited to <cap>`, the cap printed with two decimals (so `max
  "gl_picmip" is limited to 3.00`). It does not clamp to the cap (R).
- Setting `ruleset` while connected (any state other than fully disconnected): refused, line
  `ruleset can be changed only when disconnected` (R).
- An unknown name: refused, line `Unknown ruleset "<value>"` (R). Names are matched without case
  and must be the whole word (R).
- Success prints one of `Ruleset Smackdown initialized`, `Ruleset Smackdrive initialized`,
  `Ruleset Thunderdome initialized`, `Ruleset Qcon initialized`, `Ruleset MTFL initialized`,
  `Ruleset default initialized` (R).
- `cl_independentPhysics` changed while connected: `cl_independentPhysics can be changed only
  when disconnected` (R).
- Other strict-set refusals while playing, each the whole printed line (R):
  - `exec`: `The use of exec is not allowed during matches` (not on a local loopback game)
  - `packet`: `Packet command is disabled during match` (only when typed, not when stuffed by
     the server)
  - a font change: `Cannot change font during match`
  - `play`: `The use of play is not allowed during matches` (see Q10 for the exception)
  - the inter-process commands: `The use of IPC is not allowed during matches` (not on loopback)
  - `set_calc`: `The use of set_calc is not allowed during matches`
  - `set_eval`: `The use of set_eval is not allowed during matches`
  - `set_ex` and `set_ex2`: `The use of set_ex/set_ex2 is not allowed during matches`
  - `timerefresh` (strict sets; allowed in standby, as a spectator, in a demo): `Timerefresh is
     disabled during match`
  - changing a HUD group's picture: refused with no line (R).
  - setting a player-created variable by typing its name: `Current ruleset requires "set" with
     user created variables [<name>]` (any ruleset whose trigger restriction is on, connected or
     not) (R).

### 2. Forced at once? Kept or restored on return to `default`?

- On a successful change the previous set's locks are released first, then the new set's locks
  are applied at once: each locked setting is set to its locked value and then made read-only
  (R). With independent physics on at that moment, `cl_c2spps` is set to 0 and made read-only
  in the four strict sets (R).
- `mtfl`'s caps are only registered; the current value is not pulled inside the cap at the
  change (R). A `gl_picmip` of 5 stays 5 until it is next set (D: no clamp runs at the change; only later sets are checked). A setting first registered after the cap exists is clamped at
  registration (R).
- Nothing is re-applied on connect: the ruleset can only change while disconnected, and its
  locks persist across connections (R).
- Going back to `default` (or to any other set) only removes the read-only mark and the caps.
  The locked values stay: the player's earlier values are not remembered or restored (R). Because
  a config save writes every setting that differs from its default, the locked values (for
  example `allow_scripts 0` against the default 2) end up saved (D).
- `mtfl` also, while the player is flashed (team fortress flash), forces gamma to 0.55 and
  contrast to 1.0, read-only, and restores the previous values when the flash ends (10 s after
  one's own flash, 20 s after another's, or on death while flashed) (R).
- A change also flushes cached model skins so the eyes and backpack replacements are re-decided
  (Q9), and re-rates the team-play macros (Q10) (R).

### 3. Announced to others?

- No chat line announces a ruleset change or the ruleset on connect (R: no such message exists).
- The ruleset is visible to others only through the user info key `*rsn` (Q6, Q11), which any
  client can read with `user <name>` (the client prints that player's full user info) (R), and
  through `f_ruleset` replies (Q4) (R).
- Several individual settings announce themselves in chat when changed while connected and not
  a spectator (already listed in § 1.19's always-on rules); the exact said texts are: `all skins
  <N>% fullbright` or `not using fullbright skins`; `not using scripts`, `using simple scripts`,
  `using advanced scripts`; `delay packet: target ping <N> ms (<D>ms dev)`, `delay packet: adding
  <N> ms (<D>ms dev)`, `delay packet: off`; `side step aid (strafescript): on` or `off`; `fakeshaft on`, `fakeshaft off`, `fakeshaft <N.N>%`, `fakeshaft 2 (emulation of fakeshaft 0 for
  servers with antilag feature)` (R).

### 4. `f_ruleset`

- Trigger: a chat line seen by the client, said in ordinary (all) chat by a player or by a
  spectator, whose text after the speaker's `name: ` begins with `f_ruleset` and continues with
  nothing, or only digits, or digits then spaces (so `f_ruleset` and `f_ruleset 4812` match; `f_rulesets` or `f_ruleset please` do not). Team chat and spectator team chat never trigger.
  High-bit (red) characters are folded to plain before matching (R).
- Who answers: every ezQuake client that sees the line and is a player, including the asker
  (who also sees their own line) (D: the asker's client receives its own chat). Spectators never
  answer (R).
- Rate limit: each client answers each query kind at most once per 20 s of real time; a query
  inside that window is ignored silently (R).
- The reply is said in all chat (R). Its text, in order (R):
  1. Spaces so that the speaker's `name: ` plus the padding fill 17 characters (names over 15
     characters get none).
  2. The server's address as `ip:port`, right-aligned in 21 characters (port 27500 filled in
     when none was given), or `server-na` when the server name does not resolve.
  3. A space and the client version `ezq3.7.0-dev`, right-aligned in 16 characters.
  4. A space and the ruleset's name: `default`, `smackdown`, `smackdrive`, `thunderdome`, `qcon`,
     or `MTFL` (upper case for that one only), coloured by strictness: grey for `default`,
     yellow for `smackdown`, lime for `smackdrive`, orange for `thunderdome`, orange-red for
     `MTFL`, red for `qcon`.
  5. A feature string: a `+` followed by the letters of features that are on, then a `-`
     followed by the letters of features that are off, each group left out when empty, letters
     in the order m, s, f, i. `m`: modified models or sounds (the `f_modified` check is not "all
     models ok"); `s`: movement scripts (`allow_scripts` not 0); `f`: individual enemy skin
     forcing (`enemyforceskins` not 0); `i`: the side-step aid (`cl_iDrive` 1). Example ends:
     `smackdown-msfi`, `default+s-mfi`.
  The whole text is wrapped in ezQuake's brace colour markup and said inside quotes (R); servers
  strip the outer quotes (R for KTX's say handling); other clients may show the markup literally
  (D).
- The design intent, per the reply's own layout, is that a reply fits two console lines at the
  320-pixel width (38 characters a line) (R).
- `f_ruleset` typed with no argument (or any argument other than `check`) prints a help text:
  its purpose (all clients should answer an `f_ruleset` message with their settings), the usage
  (`f_ruleset` prints help, `f_ruleset check` sends the check to all players) and the four flag
  letters with their meanings, `+` meaning enabled and `-` disabled (R).
- `f_ruleset check` says `"f_ruleset"` in all chat, which makes every ezQuake player answer,
  oneself included (R; D for oneself).
- No verdict: the reply never says fair or unfair; the reader of the replies judges (R).

### 5. The other fairness queries

All share Q4's trigger rule (all chat or spectator chat, the word first, optional digits then
spaces), the 20 s per-kind limit, and never answer as a spectator (R). Each reply is said in all
chat. Replies that carry colour use the brace markup (R).

- `f_version`: `ezQuake <3.7.0-dev> <build revision> <platform>:GL`, coloured (product white,
  version cyan, platform blue on Windows, orange on Linux, silver on macOS, grey otherwise,
  renderer grey). Platform names: `Win64`, `Win32`, `Linux64`, `Linux32`, `MacOSX`, `FreeBSD`,
  `OpenBSD`, `NetBSD`, `DragonFly`, `SunOS`, `Unknown` (R). The revision text is supplied by the
  build (D: not fixed in the source).
- `f_modified`: `all models ok` in green, or `modified:` followed by the bare file names of
  stock files found modified, in red; past about 240 characters it stops with ` & more...` (R).
  Only files the client has actually loaded and checked are listed; the stock list (models,
  sprites, sounds, the palette, the colour map, a map) differs for deathmatch and team fortress
  (R). Typed as a command it says the same, or prints it locally when disconnected (R). When a
  file already checked as clean later loads modified, the client says `warning: models changed
  !!  Use f_modified again` (at most once per 3 s) (R).
- `f_server`: `ezQuake f_server response: <ip:port>` (R). Not answered when the server name does
  not resolve (R). Also a console command that says the same (R).
- `f_skins`: `all skins <N>% fullbright` or `not using fullbright skins`, followed by `,
  individual enemy skin forcing` when enemy skin forcing is on. N is `r_fullbrightSkins` as
  allowed by the server (`fbskins`, or 0 on team fortress), times 100 (R).
- `f_scripts`: `not using scripts`, `using simple scripts` or `using advanced scripts` by
  `allow_scripts` 0, 1, 2 or more (R).
- `f_fakeshaft`: the four `fakeshaft` texts of Q3 (R).
- `f_system`: memory, processor (with its MHz when known) and graphics adapter, comma-separated,
  vendor-coloured (Intel blue, AMD red, NVIDIA green). With `allow_f_system 0` (default 1) the
  reply is `disabled` (R).
- `f_cmdline`: the client's original command line, verbatim. With `allow_f_cmdline 0` (default
  1) the reply is `disabled` (R). Both `allow_f_` settings cannot be changed by the server (R).
- `f_speed`: no responder exists in this client (R); the word is only on the anti-forgery list
  below.
- Anti-forgery: a player's own message triggers never fire on a chat line containing any of
  `f_version`, `f_skins`, `f_fakeshaft`, `f_server`, `f_scripts`, `f_cmdline`, `f_system`,
  `f_speed`, `f_modified`, so a script cannot fake a reply (R).

### 6. `*rsn`

- The value is the text given to `ruleset` as typed, case kept (D: the change is accepted
  without case and the same text is stored). Set from the command line (`-ruleset <name>`), it
  is the canonical lower-case name; an unknown name there falls back to `default` without a
  message and without setting the key (R).
- It is written into the client's user info each time `ruleset` is changed successfully, and so
  is sent with the next connect (R). The key is absent when `ruleset` was never changed in the
  session, for example a player who never touched it (D: the default value does not run the
  change handler; a config line `ruleset smackdown` does, since configs run while disconnected).
- It cannot change during a connection, since `ruleset` cannot (R).

### 7. Particle settings the strict sets restrict

Applies in `smackdown`, `smackdrive` and `qcon` (not `thunderdome`, not `mtfl`), while playing,
and only when the server does not allow cheats (R). While it applies:

- `r_drawparticles 0` is ignored: particles are always drawn (R).
- `gl_particle_sparks` (sparks on spike hits), `gl_particle_spikes` (nail hits) and
  `gl_particle_gunshots` are ignored: the classic particle effect is used (R).
- `gl_lightning` is ignored: the lightning gun beam is drawn as the classic beam model (R).
- `gl_lightning_sparks` is ignored: no sparks where the beam hits (R).
- `r_shaftalpha` is ignored: the beam is fully opaque (except, in racing mode, a beam not owned
  by a player) (R).
- Nothing is printed; the settings keep their values (R).

Correction to § 1.19: the roll-angle restriction is read in this commit. In the strict sets,
while not spectating or watching a demo, `cl_rollangle` is used clamped to 0 to 5 (R).

### 8. The physics cap: 77, 72 and the server's `maxfps`

- The cap is applied only with `cl_independentPhysics 0` (default 1). Then the client's one
  frame rate, `cl_maxfps` (default 0), is held between 30 and the cap. The cap is the server
  info key `maxfps` when the server sets one (raised to at least 30); otherwise 77 in the strict
  sets and 72 in `default` and `mtfl`. With `cl_maxfps 0` the rate is the cap on a local game,
  otherwise `rate` divided by 80, held between 30 and the cap (R).
- With independent physics on (the default), the ruleset plays no part: the physics rate is the
  server's `maxfps`, or 72 when the server sets none, lowered by `cl_physfps` when that is set
  (default 0; `cl_physfps_spectator` default 77 is used instead while spectating), and never
  below 10; the drawing rate is `cl_maxfps` (0 unlimited, otherwise at least 30) (R).
- The cap never changes the setting's stored value and nothing is printed (R).

### 9. Eyes and backpack models

Correction to § 1.19, which said all of this applies outside `default` (R throughout):

- Replacement (24-bit external) skins: never for the eyes model, in every ruleset; for the
  backpack, refused in `smackdown`, `smackdrive` and `qcon` only.
- Simple-item replacements: never for the eyes, the player model, and the team fortress sentry
  gun and detpack; for the backpack, refused in `smackdown`, `smackdrive` and `qcon` only.
- Glow (luma) maps: never on the eyes, the backpack, the player model, the sentry gun or the
  detpack, in every ruleset.
- `gl_fb_models` (full-bright models): ignored for the eyes and the backpack in every ruleset
  other than `default`. (It is also never applied to gibs, the view model or a local single
  player game, in any ruleset.)
- Powerup shells: never drawn on the eyes model while playing, in every ruleset; drawn for
  spectators and demos.
- Outlines: the eyes and the lightning gun bolt are never outlined; the backpack is not outlined
  in `smackdown`, `smackdrive` and `qcon` outside demos; in `qcon` no model is outlined outside
  demos; world edges are outlined only in `default` on a server that allows cheats.
- In `qcon` the alternative legal player model is not accepted as unmodified by `f_modified` (R).

### 10. `qcon`'s `wait` limit and what triggers may run

- `wait`: each command buffer may defer at most 10 times in a row under `qcon` (32768 in every
  other set). On reaching it the client prints `Max number of wait commands detected.` and empties
  that buffer (R).
- Under the strict sets, while playing, message triggers (`msg_trigger`) and pattern triggers
  (`re_trigger`) do not run at all, and sound triggers (`tp_soundtrigger`) never run, playing or
  not (R). The game-event triggers `f_took`, `f_respawn`, `f_death`, `f_flagdeath`, `f_conc`,
  `f_flash` and `f_bonusflash` are skipped; the others (`f_newmap`, `f_spawn`, `f_mapend`,
  `f_weaponchange`, `f_demostart` and the rest) still run (R).
- The "safe buffer" is not a strict-set feature: in every ruleset, message and pattern triggers
  (when they may run at all) execute in it, and it accepts only these commands: `play`,
  `playvol`, `stopsound`, `stopsound_script`, `set`, `echo`, `say`, `say_team`, `alias`,
  `unalias`, `unalias_re`, `msg_trigger`, `inc`, `bind`, `unbind`, `record`, `easyrecord`,
  `stop`, `if`, `if_exists`, `wait`, `log`, `match_forcestart`, `dns`, `addserver`, `connect`,
  `join`, `observe`, `tcl_proc`, `tcl_exec`, `tcl_eval`, `exec`, `set_ex`, `set_alias_str`,
  `set_bind_str`, `unset`, `unset_re`, `toggle`, `toggle_re`, `set_calc`, `rcon`, `user`,
  `users`, the `re_trigger` family (`re_trigger`, `_options`, `_delete`, `_enable`, `_disable`,
  `_match`), the `hud262` family, `alias_in`, `alias_out`, `cvar_in`, `cvar_out`, and `impulse`
  only for the team fortress class impulses (at most 30 in 5 s). Any other command prints
  `"<name>" cannot be used in message triggers` (R). Settings may be set by name there (R).
- Team-play macros under the strict sets: a line that expands a team-play macro (health, armour,
  ammo, weapons, powerups, location and the like) is run in a restricted mode that accepts only
  `if`, `wait`, `echo`, `say`, `say_team`, `set_tp`, `tp_point`, `tp_pickup`, `tp_took` and the
  `tp_msg` report commands; any other command prints `"<name>" cannot be used in combination
  with teamplay $macros`, and naming a setting there is refused with the same line (R).
- `play` under the strict sets while playing is refused when typed or scripted; a sound the
  server stuffs is allowed unless its name contains `ktsound` (R).
- The console log to file pauses while playing under the strict sets (it continues in standby
  and in the countdown) (R).
- `mtfl` replaces the nearby enemy and team-mate count codes in team messages (`%e`, `%E`, `%o`,
  `%O`) with the text `banned by MTFL ruleset` (R).
- A declared TCL restriction for the strict sets has no reader (R: no use found).

## Part B: MVDSV and KTX

### 11. Does the server keep `*rsn` and show it?

- The connect packet's user info is taken whole, star keys included, so `*rsn` is kept (R). The
  server then overwrites or adds its own (`*VIP`, `*spectator`) and removes `*z_ext` (R).
- Every star key is copied into the short user info that the server sends to all clients and
  writes into its multi-view demos, so every client and every demo sees each player's `*rsn`
  (R).
- After connecting, a client cannot set or change a star key: `setinfo` on a key starting with
  `*` is ignored silently (R).
- The server's `status` and `users` do not show it; the server console's `user <id>` prints the
  whole user info, `*rsn` among it (R). The client's `user <name>` shows it to any player (R).
  Neither the scoreboard nor the match start shows it (R).

### 12. Does the match mod know rulesets?

- KTX never reads `*rsn` and has no setting that requires a ruleset or refuses a player by
  ruleset (R: no occurrence).
- KTX does ask for the replies. When a match is about to start (the last player readies, all
  are ready, at least two players) and the server info key `matchtag` is set, the player who
  readied last is made to say `f_modified`, `f_ruleset` and `f_version`, each under its own
  setting `k_on_start_f_modified`, `k_on_start_f_ruleset`, `k_on_start_f_version` (all default
  1). When a started match ends, one player is made to say each, under `k_on_end_f_modified`,
  `k_on_end_f_ruleset`, `k_on_end_f_version` (default 1). Every ezQuake player then answers in
  chat by Q4 and Q5 (R; D for the answering).
- The ready line names the tag: `<name> is ready - <matchtag>` or `<name> is ready - no matchtag
  set` (plus the team in brackets in team modes) (R). Setting the tag prints `matchtag is <tag>`,
  `matchtag disabled` or `matchtag not set` (R).
- KTX's `cmd check <query>` (only while no match is in progress): prints `<name> is checking
  [<query>]` to all, then broadcasts at chat level `<name>: <query>`, with random digits appended
  for `f_version` and `f_modified` so the line triggers fresh answers. For 3 s it captures every
  player's said lines instead of broadcasting them, then prints `player's [<query>] replies:`,
  each player's lines as `<name>: <line>` or `<name> did not reply!`, and `end of player's
  [<query>] replies` (R). The brackets are the game's bracket glyphs (R). Ordinary players may
  check only `f_version`, `f_modified`, `f_server` and `f_movement`; anything else prints `You
  are not allowed to check [<query>]` and `available checks are: f_version, f_modified,
  f_server and f_movement`; so `f_ruleset` is admin-only here (R). A second check inside the 3 s
  prints `Waiting from previous reply`; a wrong argument count prints `usage: cmd check
  <f_query>` and `for example: cmd check f_version` (R). `f_movement` is answered by the server
  itself from its strafe statistics, not by clients (R).
- KTX reads `*client` (set by ezQuake to `ezQuake <revision>`) to recognise ezQuake and its
  build number, used for features, not for fairness (R).

## Part C: Q2PRO

### 13. Rulesets, tournament mode, fairness queries, frame-rate settings

- None: no ruleset, tournament mode or chat-triggered fairness query exists in the client (R:
  no such names). Nothing is locked while connected except the cheat-protected settings (Q14)
  (R).
- `cl_async` (default 1): 0 runs everything in step at `cl_maxfps`; 1 runs physics at
  `cl_maxfps` and drawing at `r_maxfps`, the main loop unlimited; 2 is as 1 but drawing follows
  the display's sync when the renderer supports it (R).
- `cl_maxfps` (default 62; the manual says 60): held between 10 and 125, the clamped value
  written back without a message; 0 means 125 (R).
- `r_maxfps` (default 0): used only with `cl_async 1`; held between 10 and 1000, written back; 0 means 1000 (R; the manual calls 0 "no particular limit").
- Rates become whole milliseconds (1000 divided by the rate, rounded down), so most values are
  inexact. With `cl_warn_on_fps_rounding 1` (default) the client warns `cl_maxfps value '60' is
  inexact and will be rounded to '62'.` (the quote marks are backquote then apostrophe), and the
  first time also `(Set 'cl_warn_on_fps_rounding' to '0' to disable this warning.)` (R). This is
  why the default is 62 (D: 62 gives 16 ms, which reads back as 62).
- Outside the game or with the window not focused the loop runs at 60 per second, minimized at
  10 (R).
- `timescale` is cheat-protected (default 1) (R).
- `fov` is a user info setting (default 90). The bundled game holds the received value between 1
  and 160 (below 1 becomes 90), and its fixed-fov deathmatch flag forces 90 (R). `cl_gunfov`
  (when above 0) is held between 30 and 160 (R).

### 14. Cheat-protected client settings

- The list (R): `timescale` (1), `fixedtime` (0), `timedemo` (0), `cl_pitchspeed` (150),
  `cl_anglespeedkey` (1.5), `cl_kickangles` (1), `cl_thirdperson` (0), `cl_testlights` (0),
  `gl_znear` (2), `gl_drawworld` (1), `gl_drawentities` (1), `gl_showtris` (0),
  `gl_showorigins` (0), `gl_showtearing` (0), `gl_showbloom` (0), `gl_nobind` (0), `gl_lockpvs`
  (0), `gl_lightmap` (0), `r_fullbright` (0). On a server whose game directory is `gloom`, also
  `gl_modulate_world` (1), `gl_modulate_entities` (1) and `gl_brightness` (0) while connected
  (R). `cvarlist -c` lists them, flagged `C` (R).
- Cheats are allowed when disconnected, while playing a demo or a multi-view demo, and on a
  local server that has `cheats` set or a single-player game (one client slot). On a remote
  server they are never allowed, whatever its `cheats` setting (R).
- When not allowed: setting one is refused with `<name> is cheat protected.` (R); every one is
  forced back to its default each time a level begins (after downloads, before the level is
  loaded) (R); a value the player set before the setting was registered is reset to the default
  at registration (R). Values are not restored after disconnecting (R).

### 15. What a Q2PRO server can ask or enforce

All of these exist upstream (R):

- Setting checks: `addcvarban <cvar> <match> [log|print|stuff|kick] [comment]` (default
  action `log`), `delcvarban`, `listcvarbans`, `checkcvarbans`. Match strings: a leading `!`
  negates; `*` matches a non-empty value; `=`, `<`, `>` compare numbers; `~` matches a
  substring; `#` escapes a literal; anything else is a literal match; comparisons ignore case.
  Several entries for one name are tried in order, stopping at the first matching kick (R,
  manual and code agree).
- When checked: each time a client is sent a new level's data, and on `checkcvarbans` for
  clients in game (R).
- The reply mechanism: the server puts a command into the client's command buffer that the
  client forwards back to the server as a client command whose name is the byte 127 followed by
  `c`, with the variable's name and its value as the client's console expands `$<name>`. Any
  client whose console expands `$` variables answers; the player sees nothing (R; D for "any
  client"). A reply that matches a cvarban does not count toward the limit of client commands
  a single packet may carry (R).
- Results: log prints on the server console `<name>[<address>]: matched cvarban: "<cvar>" is
  "<value>"` (also for kick); print sends the comment to the client as a message; stuff sends
  the comment as commands; kick sends the comment (if any) then drops the client, announced as
  `<name> was kicked` (R).
- Version: on first connection (not on later levels) the server asks for `version`; the reply is
  stored as the client's version string and printed on a dedicated server's console as
  `<name>[<address>]: <version>`. A cvarban on `version` matches that stored string (R). Q2PRO's
  own version text is `q2pro <version> <build date> <build string> <cpu>` (R).
- `stuffcvar <userid> <variable> [...]` asks any variable ad hoc; the server console prints
  `<name>[<address>]: "<variable>" is "<value>"` (R).
- User info bans: `adduserinfoban <key> <match> [log|print|stuff|kick] [comment]`, same syntax; checked when the client enters the level and on user info changes; a kick-type ban also
  refuses connection with `<comment or Userinfo banned.>` then `Connection refused.` (R).
- `addfiltercmd <command> [ignore|log|print|stuff|kick] [comment]` blocks client commands the
  server does not know from reaching the game (R). `addstuffcmd <connect|begin> <command>` stuffs
  commands on connect or each level begin; `stuff`, `stuffall` stuff ad hoc (R).
- Speed cheat: every 1.6 s of server time each client gets 1800 ms of movement; moves past it
  are dropped while `sv_enforcetime` is 1 (default) (R). Every `sv_timescale_time` seconds
  (default 16) the server computes each client's time skew (movement time used over real time); above `sv_timescale_warn` (default 0, off unless above 1) it prints `<name>[<address>]:
  detected time skew: <x.xxx>`; above `sv_timescale_kick` (default 0, same rule) it drops the
  client, announced `<name> was dropped: time skew too high` (R).
- User info keys the engine reads: `name`, `rate` (held between `sv_min_rate` and
  `sv_max_rate`, 5000 when absent, unlimited on loopback and, with `sv_lan_force_rate`, on the
  LAN), `msg` (lowest print level the client gets, held between 0 and 256), `password` (on
  connect, against `sv_password` and `sv_reserved_password`, then removed), and `rejmsg` (a
  refusal reason the game may set). The bundled game reads `fov`, `gender`, `hand`, `ip`,
  `name`, `password`, `skin`, `spectator`. No `fps` key is read (R).
- Client settings sent by the protocol (no gun, no blend, recording, no gibs, no footsteps, no
  prediction, no flares, player updates, update rate) are kept for the server's status (R).

### 16. Competitive server settings

The manuals have no section for tournaments or duels (R). Defaults read in the code; the manual
agrees unless said (R):

- `sv_fps` (10, latched): the native frame rate, rounded down to a multiple of 10 and held
  between 10 and 60, written back; used only when the game module announces variable frame
  rate. Clients on older protocols still get 10 updates a second.
- `sv_allow_nodelta` (1): 0 drops a client that has asked for more than 64 uncompressed frames
  in a row, announced `<name> was dropped: too many nodelta frames`.
- `sv_waterjump_hack` (1, latched): equal underwater speed in all directions, surfacing
  included; 1 for Q2PRO-protocol clients, 2 or more for every client.
- `sv_strafejump_hack` (1, latched): frame-rate-independent strafe jumping; 1 for R1Q2 and
  Q2PRO protocol clients, 2 or more for all.
- `sv_packetdup_hack` (0; not in the manual): the most duplicate packets a client may ask for
  with the client command `packetdup <n>`; past it the client is told `Packetdup of <n> is not
  allowed on this server.`, otherwise `Server is sending <n> duplicate packets to you.` (`packet` when n is 1).
- `sv_qwmod` (0, latched) and `sv_airaccelerate` (0, latched): movement options passed to
  clients (not in the manual's list).
- `sv_min_rate` (15000) and `sv_max_rate` (60000) bytes a second, latched; `sv_calcpings_method`
  (2, minimum round trip); `sv_idlekick` (0); `sv_enforcetime` and the time-skew settings of
  Q15.
- `sv_mvd_enable` (0; 1 local recording, 2 also remote viewers), `sv_mvd_maxclients` (8),
  `sv_mvd_password` (empty), `sv_mvd_nogun` (0), `sv_mvd_noblend` (0), `sv_mvd_nomsgs` (1),
  `sv_mvd_maxtime` (0 minutes, unlimited), `sv_mvd_maxsize` (0 kB, unlimited), `sv_mvd_maxmaps`
  (1), `sv_mvd_begincmd` (`wait 50; putaway; wait 10; help;`), `sv_mvd_scorecmd` (`putaway; wait
  10; help;`), `sv_mvd_suspend_time` (5 minutes), `sv_mvd_disconnect_time` (15 minutes),
  `sv_mvd_spawn_dummy` (1).
- Game-side `g_` settings: `g_features` (read-only, announced by the game module), and in the
  bundled game `g_select_empty` (0) and `g_protocol_extensions` (0, latched) (R).

### 17. Does the client show other players' versions or settings?

- No client listing of other players' versions or settings exists (R). `serverstatus` shows the
  server info and the players sorted by frags (score, ping, name) (R).
- Server commands a player can type: `info` (server info), `sinfo` (one's own version string,
  protocol, packet size, compression, ping, movement rate, update rate, loss, packet
  duplication, time skew and connection time), and `lag [<player>]` which shows another
  player's round-trip times, losses and time skew (`Lag stats for:`, `RTT (min/avg/max):`,
  `Server to client PL:`, `Client to server PL:`, `Timescale          :`) (R).
- Only the server operator sees versions and settings of all: `status v` (versions), `status s`
  (protocol, the setting letters G, B, R, I, F, P, L, update rate and frame rate), `status l`
  (lag), `status p`, `status t`, `status d` (R).
