# Teamplay and console scripting in QuakeWorld and Quake II (clean-room answers)

Sources read:

- ezQuake: QW-Group/ezquake-source, default branch, commit 5d92ab01491f1bb5790b8f7c36881f5481be7ce1 (committed 2026-11-21).
- Q2PRO: upstream Q2PRO as published at github.com/q2pro/q2pro, default branch, commit 601a8df8433b0c50dbbe37c0716c3793fff140a7 (committed 2025-12-11). The github.com/skullernet/q2pro address refused an anonymous clone from this environment, so the q2pro/q2pro publication of the same upstream was read. It is not the Action Quake fork (its readme describes the plain Quake 2 client and server).

Tags: [read] = stated directly by the source; [derived] = follows from what the source does, worked out by the reader; [open] = the source does not settle it. Nothing here was measured by running a binary.

---

## A. ezQuake

### A1. The `$` macros

General rules that apply to every macro [read]:

- `macrolist` prints a header "List of macros:", one line per macro as `$name` in alphabetical order, then a rule line and "N/M macros". An optional argument is a regular expression that filters the names. The full list has 68 names.
- A `$word` is looked up both as a macro and as a cvar. A macro name matches when the text after `$` starts with the macro name, compared without regard to case. The longest matching macro name wins, so `$armortype` is not read as `$armor` followed by "type". If a cvar name matching the text is longer than the best macro name, the cvar wins. Text after the matched name is kept as typed, so `$health,` gives the value followed by a comma.
- No expansion happens inside double quotes (an odd count of `"` so far on the line). An unknown name is left in the line unchanged, with its `$`. The expanded line is capped at 1023 characters; expansion stops at the cap.
- Fixed width: if a cvar named `tp_length_<word>` exists (the user creates it; `<word>` is the whole text after `$` up to the next space or `$`), the value is padded or cut to that many printable characters, at most 40. A cvar `tp_align_<word>` whose value starts with r or c makes it right-aligned or centred; anything else is left-aligned. Colour codes and `{}` braces do not count toward the width.
- "Teamplay" macros (marked T below) restrict the line only under a ruleset that restricts triggers (smackdown, smackdrive, thunderdome, qcon). Then, if a T macro is expanded in a line run from the normal command stream (console, binds, configs, aliases run from them), that line and anything it inserts run in a restricted context. There, only these commands work: `if`, `wait`, `echo`, `say`, `say_team`, `set_tp`, `tp_point`, `tp_pickup`, `tp_took`, `tp_msgreport`, `tp_msgcoming`, `tp_msglost`, `tp_msgenemypwr`, `tp_msgquaddead`, `tp_msgsafe`, `tp_msgkillme`, `tp_msghelp`, `tp_msggetquad`, `tp_msggetpent`, `tp_msgpoint`, `tp_msgtook`, `tp_msgtrick`, `tp_msgreplace`, `tp_msgneed`, `tp_msgyesok`, `tp_msgnocancel`, `tp_msgutake`, `tp_msgitemsoon`, `tp_msgwaiting`, `tp_msgslipped`. Any other command, or setting any cvar, prints `"<name>" cannot be used in combination with teamplay $macros` and is skipped. Under the default ruleset and MTFL the T macros restrict nothing. A variable made with `set_tp` always restricts its line this way, under every ruleset. A line already inside message-trigger execution is not switched. [read; the scope covering inserted alias text is derived]

The 68 names, in the order the client defines them (T = teamplay macro):

1. `$health` (T): the health stat as a whole number. [read]
2. `$armortype` (T): the armour-type name. It checks green, then yellow, then red: `tp_name_armortype_ga`, `tp_name_armortype_ya` or `tp_name_armortype_ra` (defaults are coloured g, y, r). With no armour it gives `tp_name_none` (default empty). [read]
3. `$armor` (T): the armour stat as a whole number. [read]
4. `$colored_armor` (T): with green armour, the percent code `%a` wrapped in a green colour group; yellow (ff0) for yellow armour; red (e00) for red armour; the literal `0` with no armour. The `%a` inside is filled in only when the line goes through `say`/`say_team` percent-code parsing. [read]
5. `$colored_powerups` (T): `tp_name_quad`, `tp_name_pent` and `tp_name_ring` for the powerups held, in that order, joined with no separator. Empty when none is held. [read]
6. `$colored_short_powerups` (T): coloured single letters, q blue (03F), r yellow (ff0), p red (e00). The order is q r p when all three are held, and q p, q r, p r for pairs. Empty when none is held. [read]
7. `$tp_powerups` (T): the same as `$colored_short_powerups` when `tp_poweruptextstyle` is non-zero, otherwise the same as `$colored_powerups`. [read]
8. `$shells`, 9. `$nails`, 10. `$rockets`, 11. `$cells` (all T): that ammo stat as a whole number. [read]
12. `$weapons` (T): the weapons held, in the fixed order lg, rl, gl, sng, ng, ssg, sg, axe, each as its `tp_name_*` value, joined with `tp_name_separator` (default `/`). With none held it gives `tp_name_none`. [read]
13. `$ammo` (T): the current-ammo stat as a whole number. [read]
14. `$bestweapon` (T): the first weapon held in `tp_weapon_order` (default 78654321, using weapon numbers 1 to 8). If none matches, the fixed order 78654321 is tried. The weapon's `tp_name_*` value is given, or `tp_name_none` if none is held. [read]
15. `$bestammo` (T): the ammo count for the best weapon's ammo type (shells for 2 and 3, nails for 4 and 5, rockets for 6 and 7, cells for 8). It gives `0` for the axe or no weapon. [read]
16. `$powerups` (T): quad, pent and ring as held, then the flag name `tp_name_flag`. The flag counts when the player carries a capture-the-flag flag (player effects) or, in Team Fortress, a key item. The parts are joined with `tp_name_separator`. With none it gives `tp_name_none`. [read]
17. `$location` (T): the name of the nearest location point to the player's predicted position. It gives `tp_name_someplace` (default "someplace") when no locations are loaded or the client is not fully in game. It also records the result as the "last reported location". [read]
18. `$deathloc` (T): the location recorded at the last death. It gives `tp_name_someplace` if there has been no death since the map started. [read]
19. `$tookatloc` (T): "<took name> <tp_name_at> <took location>" (`tp_name_at` defaults to "at"). It gives `tp_name_nothing` (default "nothing") if nothing was taken or the last take is older than `tp_tooktimeout` seconds (default 15). [read]
20. `$tookloc` (T): the location of the last take. It gives `tp_name_someplace` under the same emptiness rule. [read]
21. `$took` (T): the name of the last item taken. It gives `tp_name_nothing` under the same emptiness rule. [read]
22. `$pointatloc` (T): "<pointed name> <tp_name_at> <pointed location, or the current location if the pointed thing has none>". It gives `tp_name_nothing` while flash-blinded, or if the last point is older than `tp_pointtimeout` seconds (default 15). [read]
23. `$pointloc` (T): the pointed thing's location, or the player's own `$location` if empty. It gives `tp_name_nothing` while flash-blinded. [read]
24. `$point` (T): the pointed thing's name. It gives `tp_name_nothing` when nothing is pointed at or while flash-blinded. A pointed player's name is prefixed by `tp_name_quaded` and/or `tp_name_pented`. [read]
25. `$need` (T): what the player needs, joined with `tp_name_separator`, or `tp_name_nothing`. [read; the rules are the known `tp_need_*` rules]
26. `$droploc` (T): the location where the player last dropped a Team Fortress key or flag. It gives `tp_name_someplace` if none has been dropped since the map started. [read]
27. `$droptime` (T): the whole seconds since that drop, or `-1` if there has been none. [read]
28. `$ledpoint` (T): a status light for what is pointed at. Enemy gives `tp_name_status_red`, teammate gives `tp_name_status_green`, a powerup or flag gives `tp_name_status_yellow`. Anything else, including nothing pointed at, gives `tp_name_status_blue`. [read]
29. `$ledstatus` (T): counts the items `$need` would list, with the `tp_need_rl` rule switched off. Zero needs give `tp_name_status_green`, one gives yellow, two or more give red. [read]
30. `$lastloc` (T): the death location if the player died within the last 5 seconds, otherwise the current location. Also recorded as the "last reported location". [read]
31. `$lastpowerup` (T): the enemy powerups (quad, pent, ring, in that order, joined with the separator) seen within the last 5 seconds. If none were seen in that time it gives `tp_name_quad`. [read]
32. `$cam_pos_x`, 33. `$cam_pos_y`, 34. `$cam_pos_z`: one coordinate of the view position as a decimal with trailing zeros and any trailing point removed (for example 128 or 12.5). It gives `?` unless playing a demo or spectating. [read]
35. `$cam_pos`: the three coordinates, space-separated and wrapped in double quotes. [read]
36. `$cam_angles_pitch`, 37. `$cam_angles_yaw`, 38. `$cam_angles_roll`, 39. `$cam_angles`: the same for the view angles. [read]
40. `$demoname`: the playing demo's file name without folder or extension. Empty when no demo is playing. [read]
41. `$demolength`: the playing demo's length in seconds, rounded up. It gives `0` when no demo is playing. [read]
42. `$connectiontype`: `disconnected`, `spectator` or `player`. [read]
43. `$demoplayback`: `mvdplayback` for a multi-view demo, `qwdplayback` for another demo, otherwise `0`. [read]
44. `$demotime`: the demo clock in seconds, with six decimals. [read]
45. `$rand`: a pseudo-random number from 0 to 1 inclusive, with six decimals. [read]
46. `$matchstatus`: `disconnected`, `standby` (the server's "status" serverinfo key reads standby) or `normal`. [read]
47. `$serverip`: the server address as "a.b.c.d:port". When disconnected the source does not guard the value. [read; disconnected value open]
48. `$conwidth`, 49. `$conheight`: the console's virtual width and height in pixels. [read]
50. `$matchname`: the match description built from the `match_format_<type>` setting for the detected match type. It gives "No match in progress" when the client is not fully in game. [read]
51. `$matchtype`: one of `duel`, `ffa`, `2on2`, `3on3`, `4on4`, `tdm` (also used for more than two teams), `arena`, `tfduel`, `tfclanwar`, `solo`, `coop`, `race`, `empty` or `unknown`. It gives "No match in progress" when not fully in game. [read]
52. `$mp3info`, 53. `$mp3_volume`: listed by `macrolist`, but nothing gives them a value, so they never expand and stay in the line as typed. [read and derived]
54. `$lastip`: the last server address seen in a chat line. It is either a dotted quad with a 4- or 5-digit port, rewritten as "a.b.c.d:port", or a host name ending in a 2- to 5-letter domain with a 4- or 5-digit port, kept as written. Empty until one is seen. It is not cleared on map change. [read]
55. `$qt`: a double-quote character. [read]
56. `$latency`, 57. `$ping`: the same value, the client's latency estimate in whole milliseconds (rounded). The estimate drops at once to any lower observed packet round trip and rises by at most 1 ms per received frame. Round trips over 1 second are ignored. [read]
58. `$time`: local time as HH:MM (24-hour). [read]
59. `$timestamp`: local time as YYYYMMDD-HHMM. It gives `_baddate_` if the clock cannot be read. [read]
60. `$date`: local date as DD.MM.YY. It gives `#bad date#` if the clock cannot be read (the same holds for `$time` and `$dateiso`). [read]
61. `$dateiso`: local date and time as YYYY-MM-DD_HH-MM. [read]
62. `$weaponnum`: if weapon preselection (`cl_weaponpreselect`) is on and a best weapon is chosen, that weapon's number. Otherwise the active weapon's number, 1 to 8, or `0` for anything else. [read]
63. `$weapon`: the active weapon's `tp_name_*` value. The super lightning gun is the fixed word "slg", the keys are "key1" and "key2", and anything unknown gives `tp_name_none`. [read]
64. `$tf_skin`: in Team Fortress, as a player, the class name from the skin. The skin name without its "tf_" prefix is used, except tf_demo, tf_eng, tf_snipe and tf_sold give demoman, engineer, sniper and soldier. Otherwise it gives the raw skin name. [read]
65. `$gamedir`: the current game directory name. [read]
66. `$triggermatch`: see A2. [read]
67. `$team1`, 68. `$team2`: the first and second name in a sorted list of the distinct teams of all non-spectator players. Sorting ignores case and the high (coloured) bit. With teamplay off the list holds player names instead of teams. If there are fewer than one or two entries, the value is the literal `team1` or `team2`. [read]

Only the macros marked T restrict a line. `$weapon`, `$weaponnum`, `$qt`, `$lastip`, `$triggermatch`, `$team1`/`$team2`, the time macros and all the camera, demo, match and connection macros do not. [read]

### A2. `$triggermatch`

- It is set only when a `msg_trigger` fires and its alias exists. It is set just before that alias is queued. [read]
- The value is the whole scanned line, not only the matched part. For a chat line, that is the text after the speaker prefix ("name: " or "(name): "). For other print lines it is the whole line. It keeps its trailing newline and is cut to 255 characters. [read]
- It is not set by `re_trigger` matches or by f_ triggers. [read]
- It is cleared when a new map starts (all teamplay state is reset then). Otherwise it holds the last match. [read]

### A3. `msg_trigger`

Syntax: `msg_trigger <name> "<string>" [-l <level>]`. [read]

- With no arguments it lists every trigger as `name : "string"`, or prints "no triggers defined". [read]
- With one argument it shows `name: "string"`, or prints `trigger "<name>" not found`. [read]
- With more than four arguments it prints the usage line `msg_trigger <trigger name> "string" [-l <level>]`. [read]
- A name longer than 31 characters prints "trigger name too long". A string longer than 63 characters prints "trigger string too long". [read]
- Defining an existing name replaces its string. A new trigger gets level 2 (high). The level changes only when exactly `-l <level>` follows the string. The switch is case-insensitive; no other switch exists. Any other fourth or fifth word is silently ignored, and re-defining without `-l` keeps the old level. [read and derived]
- Level values: `0` low (pickup-style messages), `1` medium (obituaries), `2` high (the default), `3` chat. A `3` trigger fires on both normal and team chat. `t` fires on team chat only. A number above 3 falls back to 2. A non-number reads as 0. [read]
- Saving the configuration writes each trigger back as a `msg_trigger` line, adding `-l <n>` or `-l t` when the level is not 2. [read]
- No command removes a single trigger. All triggers are cleared by `cfg_reset` and by `cfg_load`. [read]

Matching [read]:

- The trigger string must appear anywhere in the scanned text, matching case exactly (a plain substring). An empty string never matches.
- Scanned lines: every completed line of server print messages at levels 0 to 3. Text is buffered until a newline. A chat line counts as team chat (level "t") when it is a team message from the player's own team. Centre prints and the client's own echoes are not scanned by `msg_trigger` (only `re_trigger` sees those).
- Chat lines that the client drops before printing (ignore lists, the team-message filter, flood ignore) are not scanned.
- A chat-level line containing any of `f_version`, `f_skins`, `f_fakeshaft`, `f_server`, `f_scripts`, `f_cmdline`, `f_system`, `f_speed` or `f_modified` never fires a trigger.
- In Team Fortress, high-level lines announcing a flag or key taken never fire a trigger. The phrases are " has your key!", " has taken your Key", " has your flag", " took your flag!", " took the blue flag", " took the red flag", " Has the Red Flag", " Has the Blue Flag", plus a few in the coloured character set.
- Every matching trigger fires, newest-defined first. Each queues its alias text into the message-trigger command stream. If the alias does not exist it prints `trigger "<name>" has no matching alias`.

Gates [read]:

- Nothing fires when `tp_msgtriggers` is 0 (default 1).
- Nothing fires while playing under a ruleset that restricts triggers.
- While spectating or playing a demo, the ruleset does not block triggers, but nothing fires when `cl_restrictions` is non-zero (default 0).

Commands allowed inside message-trigger execution (the "safe" stream): play, playvol, stopsound, stopsound_script, set, echo, say, say_team, alias, unalias, msg_trigger, inc, bind, unbind, record, easyrecord, stop, if, if_exists, wait, log, match_forcestart, dns, addserver, connect, join, observe, tcl_proc, tcl_exec, tcl_eval, exec, set_ex, set_alias_str, set_bind_str, unset, unset_re, toggle, toggle_re, set_calc, rcon, user, users, unalias_re, re_trigger, re_trigger_options, re_trigger_delete, re_trigger_enable, re_trigger_disable, re_trigger_match, hud262_add, hud262_remove, hud262_position, hud262_bg, hud262_move, hud262_width, hud262_alpha, hud262_blink, hud262_disable, hud262_enable, hud262_list, hud262_bringtofront, hud_262font, hud262_hover, hud262_button, alias_in, alias_out, cvar_in, cvar_out. [read]

- `impulse` is allowed only in Team Fortress, only for impulses 23, 99, 101 to 109, 135, 144, 145 and 159 to 167. After every 30 allowed impulses, a further one within 5 seconds of the previous 30-count reset is refused (outside demo playback). [read]
- Any other command prints `"<cmd>" cannot be used in message triggers`. Typing a cvar name to set it is not blocked. Aliases run and inherit the same restriction. [read and derived]
- The stream is 2 KB. The ruleset's cap on consecutive `wait`s applies: hitting it prints "Max number of wait commands detected." and empties the stream. [read]

### A4. f_ event triggers

Every f_ trigger the client defines, in its own list, is: f_newmap, f_spawn, f_mapend, f_reloadstart, f_reloadend, f_cfgload, f_exit, f_demostart, f_demoend, f_captureframe, f_sbrefreshdone, f_sbupdatesourcesdone, f_focusgained, f_freeflyspectate, f_trackspectate, f_weaponchange, f_took, f_respawn, f_death, f_flagdeath, f_conc, f_flash, f_bonusflash, f_demomatchstart, f_countdownstart, f_countdownbreak. That is 26 names. There is also one alias the client runs by name outside that list: f_qtvfinalscores (below). [read]

The common gate for the listed triggers [read]:

- Nothing runs if `tp_triggers` is 0 (default 1), or while spectating or playing a demo with `cl_restrictions` non-zero.
- Triggers marked "teamplay" (f_took, f_respawn, f_death, f_flagdeath, f_conc, f_flash, f_bonusflash) also need teamplay on and at least one other non-spectator player on the player's team, unless `tp_forceTriggers` is 1.
- Triggers marked "restricted" (the same seven) do not run under a ruleset that restricts triggers.
- If the alias exists, its text is appended to the command stream and runs later, not at once. Asking for an unknown f_ name prints `Unknown f_trigger "<name>"` with no newline.

f_freeflyspectate, f_trackspectate and f_captureframe skip this gate entirely: they run whenever the alias exists. [read]

The asked ones [read unless marked]:

- f_mapend: when the server sends the intermission message (the end-of-map scoreboard view). Common gate only. Before the alias runs, the client is in intermission, and the completion time and the intermission view position and angles are recorded (none are exposed as macros).
- f_flagdeath: instead of f_death, when the player's health goes from above 0 to 0 or below while carrying a capture-the-flag flag (player effects) or, in Team Fortress, a key item. Extra gate: never while spectating, and the teamplay gate. Before it runs, the death location (`$deathloc`, `$lastloc`) and the counts of nearby enemies and teammates at death are recorded.
- f_countdownstart: when the server's "status" serverinfo key is processed as "countdown" while the previous processed state was also countdown. As written, the update that first switches the status to countdown does not fire it; a later serverinfo update during the countdown does. [read; whether the server sends such a later update is open]
- f_countdownbreak: when a serverinfo update shows status "standby" after the previous state was countdown and not standby, that is, a countdown aborted back to standby. Common gate.
- f_demomatchstart: only in multi-view demo playback, when the demo carries the match server's hidden "match start" marker (a stuffed text line `//ktx matchstart`). It fires after per-player demo statistics and item timers are reset. Common gate.
- f_focusgained: when the operating system gives the game window keyboard focus. Common gate.
- f_freeflyspectate: whenever the spectator camera is unlocked to free flight (a `track` off or similar). No gate; runs in the normal command stream if the alias exists. It fires on every unlock request, even if the camera was not tracking.
- f_trackspectate: whenever the spectator camera locks on to a player. No gate; runs if the alias exists.
- f_captureframe: at the start of every frame while a movie or demo capture is recording. No gate; runs if the alias exists.
- f_sbrefreshdone: after a server-browser refresh (the ping and status sweep) finishes. Common gate.
- f_sbupdatesourcesdone: after the server browser finishes updating its master-server sources. Common gate.

The others [read]:

- f_reloadstart and f_reloadend: Team Fortress only, as a player or a spectator tracking someone. They fire when the view weapon model disappears (reload started) or reappears (reload ended).
- f_conc: Team Fortress only, when a concussion changes the field of view and the idle sway (`v_idlescale`) is exactly 100.
- f_flash: Team Fortress flash-grenade blinding, detected by a 240 or 255 screen-tint percent.
- f_bonusflash: when the server's item-pickup flash command arrives, at most once per frame.
- f_weaponchange: when the active-weapon stat changes.
- f_qtvfinalscores: run when the server stuffs a line beginning `//finalscores ` and an alias of that name exists. The rest of that line is passed as the alias's arguments. It runs in the server-command stream, with that stream's limits.

Is there an f_ trigger for [read]:

- a flag taken: no dedicated one. f_took fires with `$took` set to `tp_name_flag` when the pickup is a flag. In Team Fortress that is a key item appearing in the player's items. Flag models are recognised by pickup detection.
- a match start: only f_demomatchstart (multi-view demo playback). Live play has f_countdownstart and f_countdownbreak but no "match started" trigger.
- a frag, a team change, a map vote, a disconnect or a connect: none. The nearest are f_newmap (new map loaded) and f_spawn (player spawn).

### A5. f_ queries other clients answer

Common rules [read]:

- A query is answered only from a received chat line that is normal chat or spectator chat ("name: text" or a spectator's line). Team chat ("(name): text") never triggers an answer.
- The message part, with its high (coloured) bit stripped, must start exactly with the query word, matching case. After the word only spaces and digits may follow: optionally digits, then spaces, then nothing else. "f_version" and "f_version 2" answer; "f_version please" does not. [derived from the rule as read]
- A spectator never answers. Each query type answers at most once per 20 seconds. A query inside the 20 seconds is silently ignored.
- The answer goes out with `say` (to everyone). Several query types in the same line can each answer.
- The check happens before the ignore list is applied, so an ignored player's query is still answered. [derived]

The queries [read]:

- `f_version`: says "{ezQuake <version> <platform>:<renderer>}". Each part is in a colour group: ezQuake ice white, the version cyan, the platform coloured by operating system (Windows light blue, Linux orange, macOS silver, other grey), the renderer grey. The platform is one of Win64, Win32, Linux64, Linux32, MacOSX, FreeBSD, OpenBSD, NetBSD, DragonFly or SunOS.
- `f_server`: says "ezQuake f_server response: <a.b.c.d:port>", the address the client connected to (default port added). No answer if that address cannot be resolved. Also a command the player can type, which says the same.
- `f_skins`: says "all skins N% fullbright" (N = 100 × `r_fullbrightSkins`, limited by the server's "fbskins" serverinfo, which defaults to 1, or 0 in Team Fortress), or "not using fullbright skins". Either is followed by ", individual enemy skin forcing" when `enemyforceskins` is on.
- `f_scripts`: says "not using scripts", "using simple scripts" or "using advanced scripts" for `allow_scripts` 0, 1 or 2.
- `f_fakeshaft`: says "fakeshaft on" (value ≥ 0.999), "fakeshaft off" (< 0.001), "fakeshaft N.N%", or "fakeshaft 2 (emulation of fakeshaft 0 for servers with antilag feature)".
- `f_modified`: says, in green, "all models ok". Otherwise it says, in red, "modified:" followed by the base names of the checked models and sounds whose hashes differ (only those relevant to deathmatch or Team Fortress as appropriate), ending in " & more..." past about 240 characters. Also a command; when disconnected the command echoes locally instead of saying.
- `f_ruleset`: says one coloured line, padded so it lines up in a 320-pixel console:
  - spaces so that the speaker's name plus padding fills 15 characters,
  - the server address right-aligned in 21 characters ("server-na" if unknown),
  - a space, then "ezq<version number>" right-aligned in 16 characters,
  - the ruleset name, coloured by strictness: default grey, smackdown yellow, smackdrive lime, thunderdome orange, MTFL orange-red, qcon red,
  - feature flags: "+" then letters for features on, "-" then letters for features off. The letters are m (modified models or sounds), s (movement scripts allowed), f (individual enemy skin forcing) and i (the strafing aid).
  - The command `f_ruleset` alone prints help. `f_ruleset check` says "f_ruleset".
- `f_cmdline`: says the program's original command line, or "disabled" when `allow_f_cmdline` is 0 (default 1).
- `f_system`: says "{<memory>, <CPU name> (<MHz>MHz), <GPU name>}", with Intel blue, AMD red and NVIDIA green colouring. When `allow_f_system` is 0 (default 1) it says "{disabled}". Neither setting can be changed by a server.
- `f_speed`, `f_fps`, `f_sens`, `f_query`, `f_lagreport`: this client does not answer them. `f_speed` appears only in the list that stops message triggers firing on faked replies. `f_sens` and `f_fps` (with ezcomp, ezcomp2, ezcomp3, f_tj and f_ta) are alias names the client looks for, as server-made aliases, to detect that it is connected through a proxy.

### A6. cl_fakename, cl_fakename_suffix, cl_nofake

Sending [read]:

- `cl_fakename` (default empty) and `cl_fakename_suffix` (default ": ") take effect only on `say_team`, only when not a spectator, only when `cl_fakename` is not empty, and only when the message does not already contain a carriage-return character.
- The message sent becomes a carriage-return character (code 13), then `cl_fakename` followed by `cl_fakename_suffix` with fun-character codes applied, then the message text.
- The server prints "(realname): " before it. On every receiving console a carriage return sends the cursor back to column 0 and the next characters overwrite the line, so readers see "<fakename><suffix><text>". [derived]
- The built-in `tp_msg*` messages add their own short-nick prefix only when `cl_fakename` is empty. That prefix is a carriage return, then the `nick` cvar if set, otherwise the first 3 characters of the player's name, then `cl_fakename_suffix`. With `cl_fakename` set they add nothing and the general rule above applies.
- When a team message is printed locally only (the hidden-count case), it is shown as "<fakename><suffix><text>" if `cl_fakename` is set, otherwise "(name): <text>".

Receiving, `cl_nofake` (default 2) [read]:

- 0: received chat is printed as sent.
- 1: in every received chat line, each carriage-return character (code 13), and each line feed (code 10) that is not the line's last character, becomes a space. A faked name then shows after the real "(name): " prefix instead of covering it, and a fake extra line cannot be started.
- 2: the same, except team messages from the player's own team are left untouched, so teammates' fake names and line breaks still work.
- It applies to chat lines only (normal, team and spectator chat), not to other prints. The same rule is applied to the locally printed hidden team message.

### A7. tp_soundtrigger

- `tp_soundtrigger` (default "~") is a set of characters; any one of them starts a sound word. An empty value turns the feature off. [read]
- Only received team messages from the player's own team are checked, after the speaker prefix. Nothing is checked while a ruleset restricts triggers. [read]
- The check looks for a sound word that ends a line, immediately before a line break. Scanning back from the line end over letters and digits and allowed file-name characters, it stops at the first trigger character. The text between that character and the line end is the sound name. [read]
- Characters that end the scan without a sound: control characters, space, and `"&'*,:;<>?|` plus the delete character. A backslash is read as a forward slash. [read]
- Limits: the name must be 1 to 63 characters, and a name containing ".." is refused. [read]
- On a found word, the trigger character and the name are removed from the printed line (the line break stays). ".wav" is added if the name has no extension. The file must exist under the game's "sound/" folder; otherwise nothing plays, but the word is still removed. If it plays, as a local, non-positional sound, the normal chat beep is suppressed. If sound is not running, the word is removed and the normal beep rules apply. [read]
- Example: a teammate's line ending "quad up ~misc/gong" prints "quad up " and plays sound/misc/gong.wav. [derived]

### A8. Status names and separators

- `tp_name_status_green`, `_yellow`, `_red`, `_blue` default to the fun-character codes $G, $Y, $R, $B (the coloured light characters). They are read by `$ledpoint` (red enemy, green teammate, yellow powerup or flag, blue any other item or nothing), by `$ledstatus` (green, yellow or red by need count), and by the percent code `%s` (green with no enemies near, yellow when enemies ≤ teammates, red when enemies exceed teammates; hidden from the sender's own view like the other count codes). [read]
- `tp_name_status_white` (default $W) is registered but nothing reads it. [read]
- `tp_name_flag` (default "flag") is read by `$powerups` (when carrying a flag or key) and as the item name when pointing at or taking a flag, key or Team Fortress goal model. That makes it the `$took` and `$point` value and feeds `tp_msgtook` and `tp_msgpoint`. [read]
- `tp_name_filter` (default empty) is read only by `tp_msgtfconced`: the message "<enemy> conced at <place>" or "Enemy conced" is followed by a space and `tp_name_filter`. It is meant for a message-filter tag such as "#a". [read; the tag example is derived]
- `tp_name_none` (default empty) is the value for "nothing" in `$armortype`, `$powerups`, `$weapons`, `$weapon` and `$bestweapon`. It is also the empty test for the percent codes `%[a]` (which then show the letter "a") and `%[p]`/`%[P]` (which then show nothing). [read]
- `tp_name_separator` (default "/") joins list items in `$weapons`, `$powerups`, `$need`, `$lastpowerup` and `tp_msgsafe` (between armour and weapon). `$ledstatus` uses its own "/" for counting, whatever the setting. Example: with quad and pent, `$powerups` gives "quad/pent" (each name in its default colour group). [read]
- `tp_poweruptextstyle` (default 0) is read only by `$tp_powerups`, which several `tp_msg*` messages put in front of their text when the player holds a powerup. At 0 it gives the full names, for example "quadpent" coloured, with no separator. Non-zero gives the coloured letters, for example "qp". [read]

### A9. teamlock

Read unless marked:

- `teamlock` (default 0) has an effect only while the client is a spectator. As a player, "my team" is always the player's own team.
- While spectating:
  - `0`: my team is the tracked player's team, or no team (empty) while free-flying. The tracked player also always counts as a teammate for colour forcing.
  - `1` (exactly this text): my team is the first team found. In multi-view demo playback that is the scoreboard's first team if there is one; otherwise it is the team of the first non-spectator player, in player-slot order, whose team is not empty. The tracked player no longer counts as a teammate by being tracked.
  - Any other text: that text is the team name. A team really named "0" or "1" cannot be chosen.
- Which features read it: only skin and colour forcing.
  - `teamcolor`/`enemycolor`: which players get the team or enemy colours.
  - `teamskin`/`enemyskin` and their quad, pent and both-powerup variants.
  - The solid colour `r_teamskincolor`/`r_enemyskincolor` used when skins are drawn plain.
  - A player counts as a teammate there when teamplay is on and the player's team equals my team.
- Changing `teamlock` refreshes every player's skin and colours. Chat classification, `$team1`/`$team2`, nearby-player counts and the HUD do not read it.

### A10. Flood protection

- `cl_floodprot` (default 0) turns it on. `cl_fp_messages` (default 4) and `cl_fp_persecond` (default 4) set the limit. It is active only when all three are positive. [read]
- What counts: every `say` and `say_team` the client sends, including `tp_msg*` and messagemode, but not lines sent to a recognised proxy (lines beginning "proxy:", "," or "."). The last 10 send times are kept. [read]
- The rule: a new message is refused if the Nth most recent sent message, N = `cl_fp_messages` (at most 10), was sent less than 1.02 × `cl_fp_persecond` seconds ago. So at most N messages go out in any window of about `cl_fp_persecond` seconds. Despite its name, `cl_fp_persecond` is a window length in seconds. [read and derived]
- A refused line prints "Flood Protection" locally and its text is not sent. The server still receives an empty `say` or `say_team` command, because the command word is written before the check. [read; the server's reaction to the empty command is open]

### A11. Editing helpers

All read unless marked.

- `alias_in <alias> <cvar> [options]`: puts the cvar's current value into the alias text.
  - Options is a bit mask, the bits added together.
  - Option bit 1: append at the end (otherwise insert at the start).
  - Option bit 2: do nothing if the value already occurs anywhere in the alias.
  - Option bit 4: when bit 2 stops it, print "alias_in: already inserted".
  - Option bit 8: create the alias, empty, if it does not exist.
  - No separator is added; include spaces or semicolons in the cvar value.
  - The wrong argument count prints "alias_in <alias> <cvar> [<options>]".
  - An unknown alias (without option 8) prints `alias_in: unknown alias "<name>"`.
  - A missing or server-hidden cvar prints `alias_in: unknown cvar "<name>"`.
  - The result is limited to 1023 characters. If it then contains a `%`, the alias becomes one that takes `%` parameters (A12).
- `alias_out <alias> <cvar> [options]`: removes the first occurrence of the cvar's value from the alias text.
  - Option 1: test only, change nothing.
  - Option 2: stay silent when the value is not found; otherwise it prints "alias_out: not found".
  - Usage: "alias_out <alias> <cvar> [options]". Unknown names print `alias_out: unknown alias "<name>"` or `alias_out: unknown cvar "<name>"`.
- `cvar_in <cvar1> <cvar2> [options]` and `cvar_out <cvar1> <cvar2> [options]`: the same for a cvar's value. cvar2's value is inserted into or removed from cvar1.
  - Option 8 on `cvar_in` creates cvar1, empty.
  - Usages: "cvar_in <cvar1> <cvar2> [<options>]" and "cvar_out <cvar1> <cvar2> [<options>]". Messages are "cvar_in: unknown cvar", "cvar_in: already inserted" and "cvar_out: not found", each with the same prefix pattern.
  - When run from a server-sent command against a cvar the server may not change, it silently does nothing.
  - With option 2 and the value already present, nothing changes, wherever the value sits (position does not matter).
- `set_alias_str <cvar> <alias>`: sets the cvar to the alias's text.
  - A missing cvar is created first, unless the name is a command: `"<name>" is a command`.
  - An unknown alias prints `Unknown alias "<name>"`, and the cvar is left created but empty.
  - Usage: "usage: set_alias_str <cvar> <alias>".
- `set_bind_str <cvar> <key>`: sets the cvar to the key's binding, or empty if unbound. The cvar is created as above.
  - An unknown key prints `Unknown key "<key>"`.
  - Usage: "usage: set_bind_str <cvar> <key>".
- `aliasedit <name>`: replaces the console input line with `/alias "<name>" "<current text>"`, with the cursor just after the opening quote of the text. The text is empty for a new alias.
  - With no argument it prints "<cmd> <name> : modify an alias" and "aliaslist : list all aliases".
  - An empty name prints "Alias name must be specified". A name of 32 characters or more prints "Alias name is too long".
- `bindedit <key>`: the same with `/bind "<key>" "<binding>"`, cursor just after the opening quote of the binding.
  - With no argument it prints "<cmd> <key> : modify a bind" and "bindlist : list all binds". A bad key prints `"<key>" isn't a valid key`.
- `cvaredit <name>`: puts `/<name> "<value>"` on the input line, with the cursor at the start of the line.
  - With no argument it prints "<cmd> <name> : modify a cvar" and "cvarlist : list all cvars". An unknown name prints `"<name>" not a valid cvar`.
- `hash <string>`: prints one unsigned 32-bit decimal number.
  - It starts at 0 and, for each character c with bit 5 cleared (so case is ignored for letters), computes hash = c + hash × 64 + hash × 65536 − hash, modulo 2^32.
  - Anything but exactly one argument prints "Usage: hash <string to hash>".
- `clipboard <text>`: copies the rest of the command line, as typed after the command, to the system clipboard. It prints nothing.
- There is no paste command. Pasting is a console key action only. [read for commands; the key binding is not traced]

### A12. Alias arguments

Read unless marked:

- An alias takes parameters only if its text contained a `%` when it was defined (or after `alias_in` adds one).
- In such an alias, at run time:
  - `%1` to `%9` become that argument (empty if missing),
  - `%0` becomes all the arguments as one string, as typed,
  - `%%` becomes a single `%`,
  - any other `%x` is kept as is.
  - Only one digit is read, so `%10` is the first argument followed by "0".
- `$1`, `$*` and similar are not alias parameters. `$` is the macro and cvar sign.
- An alias without `%` ignores its arguments, with one exception: if its whole text is a single word naming a cvar or a command (not a + or - command), the call's arguments are added after that word.
- The expanded text is limited to 1023 characters.
- Alias names are matched without regard to case.

### A13. The expression language (`eval`, `set_eval`, the parenthesised `if`)

Read unless marked.

- Which `if`: when the first word after `if` starts with an opening parenthesis (and there are more than two words), the expression form is used. Its syntax is: if, a parenthesised condition, then, commands, optionally else and commands.
  - All words before `then` are joined with single spaces into the expression. An empty word becomes `''`.
  - Fewer than four words prints "Usage: if <expr> then <cmds> [else <cmds>]".
  - A missing `then` prints `if command: "then" not found`.
  - A bad expression prints `Error in condition: <description> ("<expression>")`.
  - The chosen branch's words are re-joined and run at once, ahead of the rest of the stream.
- Otherwise the old form is used: `if <a> <op> <b> [then] <cmds> [else <cmds>]`.
  - Its operators: `==` and `=` equal, `!=` and `<>` not equal (numeric when both sides look like numbers, otherwise exact text); `>` `<` `>=` `<=` numeric; `isin` and `!isin` (is a a substring of b, case exact); `=~` and `!~` (b is a regular expression matched against a; captures fill `re_trigger_match_0` to `_9`).
  - Fewer than five words prints "Usage: if <expr1> <op> <expr2> <command> [else <command>]".
  - An unknown operator prints `unknown operator: <op>`, then `valid operators are ==, =, !=, <>, >, <, >=, <=, isin, !isin, =~, !~`.
  - A bad regular expression prints "Error in regexp: <message>".
- `eval <expression>`: takes exactly one argument (quote the expression).
  - It prints "Result: <n> (integer)", "Result: <x> (double)", "Result: true|false (bool)" or "Result: (string)" with the text on the next line in quotes.
  - A bad expression prints "Error occured: <description>".
  - A wrong argument count prints "Usage: eval <expression>" and a line explaining it.
- `set_eval <var> <expression...>`: the rest of the line is the expression. Integer, double and bool results are stored as numbers (bool as 1 or 0); text is stored as text. A missing cvar is created.
  - It is refused while playing a match under a ruleset that forbids it: "The use of set_eval is not allowed during matches".
  - Fewer than two arguments prints a usage block. An error prints "Error in expression: <description>".
- Values: integers, doubles (digits with exactly one point; two points is "Malformed number found"), booleans (results of comparisons), and text.
  - Text is quoted with "…" or '…', or is a bare word running up to the next whitespace. A bare word swallows a closing parenthesis written against it, so a space is needed before `)`.
  - `%name` is a variable, but in all three commands variables are not wired up and always read as integer 0. `$name` is expanded by the console before the expression is seen.
- Operators, loosest binding first:
  1. `and`, `AND`, `&&`, `or`, `OR`, `||`. These are at one level, so and does not bind tighter than or.
  2. Comparisons: `<` `<=` `=` `==` `!=` `>=` `>` `isin` `!isin` `=~` `!~`. The form `<>` is not usable here; it lexes as < then >.
  3. `+` and binary `-`.
  4. `*`, `/`, `mod`, `xor`, `div`.
  5. Unary `-`, parentheses, and the functions.
  - Within one level, operators group from the right, so `a mod b * c` means a mod (b × c).
- Arithmetic: `+` adds numbers, joins two texts, and mixing text with a number is "Type mismatch". `/` always gives a double, and dividing by 0 gives "Division by zero". `mod`, `div` (integer division) and `xor` need two integers (`xor` also takes two booleans).
- Comparisons: two texts compare as byte strings, case-sensitive. Text against a number is "Type mismatch". Numbers within 0.00000001 count as equal.
- `isin` and `!isin` turn both sides to text, and an empty left side is never "in". `=~` and `!~` match the left text against the right as a regular expression. In `if`, captures go to `re_trigger_match_0` to `_9`.
- `and` and `or`: a text is true when not empty, a number when not 0.
- Functions:
  - `strlen x`: length of a text.
  - `int x`: truncates a number, reads a text's leading integer, and turns a boolean into 1 or 0.
  - `substr(text, pos, len)`: pos and len are integers from 0; past the end gives the shortened or empty text; negative values give "Invalid argument".
  - `pos(text, find)`: the 0-based index of find, or −1.
  - `tobrown x` and `towhite x`: turn the text into the coloured (high-bit) or plain character set.
  - Keywords are recognised as prefixes, so a bare word beginning with "int", "pos", "or", "and", "mod", "div", "xor", "strlen", "substr", "isin", "tobrown" or "towhite" is taken as that keyword. Such words must be quoted. [derived]
- Anything after a complete expression is silently ignored. [derived]
- The error descriptions are: "Invalid token found", "Unexpected character", "Type mismatch", "Division by zero", "Operation not implemented", "Malformed number found", "Out of memory", "Internal error", "Regexp error", "Invalid argument".

### A14. teamcolor and enemycolor

Read unless marked:

- `teamcolor` with no argument prints `"teamcolor" is "off"`, or `"teamcolor" is "<top> <bottom>"`. `enemycolor` behaves the same.
- `off` or an empty argument switches forcing off.
- One number sets both top and bottom. Two numbers set top then bottom.
- Each is clamped to 0 to 16. Non-numbers read as 0.
- They are stored in `teamtopcolor`/`teambottomcolor` and `enemytopcolor`/`enemybottomcolor`, where −1 means off.
- What they force: the shirt (top) and pants (bottom) colours used to draw each player. Teammates take the team colours; everyone else, except the player himself, takes the enemy colours.
  - Teammates are: the player himself; while spectating with `teamlock` 0, the tracked player; and, with teamplay on, anyone on "my team" (A9).
  - Off (−1) leaves a player's own colours.
- Forcing does nothing in Team Fortress or when the server's fpd setting forbids colour forcing.
- Changing either refreshes all players' skins. `cfg_reset` sets both to off.

---

## B. Upstream Q2PRO (commit 601a8df8433b0c50dbbe37c0716c3793fff140a7)

### B1. Macro expansion rules

All read unless marked.

- When: every command line is expanded just before it is split into words. This covers lines typed in the console, lines from configs, the text a key binding inserts, and each line an alias inserts. Expansion happens each time the line runs, not when it was stored, except as below.
- Quotes: nothing between double quotes is expanded. A `"` switches quoting on and off. A line ending inside an open quote is thrown away with "Line has unmatched quote, discarded.", whether or not it has a `$`. [derived from the rule as read]
- `$$` gives a literal `$`.
- `$name`: spaces after the `$` are skipped. The name is then the run of non-space characters up to the next space or the next `$`. So `$name$rest` ends the name at the second `$` (the second `$` is consumed), and punctuation becomes part of the name: `$cl_health,` looks up "cl_health,".
- `${name}`: everything up to `}`, spaces included. `${$name}` is the same. A missing `}` takes the rest of the line.
- Lookup order: a macro, then a cvar (cvars marked private are skipped), then the two keywords `$qt` (a double quote) and `$sc` (a semicolon). A name that is none of these expands to nothing, and the `$name` disappears. A `$` at the very end of the line, or with an empty name, is left alone.
- `$sc` gives a semicolon, but lines are split into commands before expansion, so it does not start a new command; it lands in an argument. [derived]
- Recursion: a cvar's value is scanned again, so a cvar holding `$other` expands that too. A macro's output, or an alias parameter, is not rescanned. The 100th expansion in one line aborts the line with "Macro expansion loop, discarded.".
- Length: a line, before or after expansion, may hold at most 1023 characters. "Line exceeded 1024 chars, discarded." or "Expanded line exceeded 1024 chars, discarded." is printed and the line is dropped.
- In aliases, at definition: `alias name text` is itself a command line, so unquoted `$` in it expand when the alias is defined. Quote the text (or use `$$`) to keep them for run time. At run: see B2. After the parameter pass the alias text is inserted as new lines, each expanded normally when it runs.
- In binds: `bind key text` expands unquoted `$` when the bind command runs. The stored text is expanded again each time the key is pressed. [derived]
- In `say`: in the console, `say ...` (or plain text with `con_auto_chat` 1 or 2, which sends "cmd say ..." or "cmd say_team ...") is expanded like any line. Text typed in the chat prompt (messagemode or messagemode2) is sent as typed with no expansion.

### B2. Alias arguments

Read unless marked:

- When an alias runs, a parameter pass over its text replaces only positional names:
  - `$0`: the alias's own name,
  - `$1`, `$2` and so on: that argument, empty if missing,
  - `$@`: all arguments joined by single spaces,
  - `${N-M}`: arguments N to M joined by spaces,
  - `${N-}`: N to the last.
  - The same forms work without braces where the word ends cleanly, for example `$2`.
- Any other `$name` is left for the normal pass when the inserted line runs. Inside quotes nothing is replaced. `$$` in the alias text is already turned into `$` by this pass, so the normal pass then sees a single `$`. [derived]
- There is no `$*` and no `%1` form.
- Arguments are the words after the alias name. Quoted arguments lose their quotes.
- A key bound to a name starting with + gets two extra arguments, the key number and the time, so a + alias sees them as `$1` and `$2`.
- Loop limit: 16 alias expansions per command stream per frame; beyond that it prints "Runaway alias loop". The same counter limits nested `exec`.
- Aliases cannot take the name of a command or a cvar: `"<name>" already defined as a command` or `... as a cvar`. Alias names are case-sensitive.

### B3. `if`

Read unless marked:

- Syntax: `if <a> <op> <b> [then] <commands> [else <commands>]`. Fewer than five words prints "Usage: if <expr> <op> <expr> [then] <command> [else <command>]".
- Operators:
  - `==`: numeric equality when both sides are numbers, otherwise exact text equality.
  - `!=` and `<>`: the reverse.
  - `<` `<=` `>` `>=`: numbers only. Otherwise it prints "Can't use '<op>' with non-numeric expression(s)" and runs neither branch.
  - `isin`: a occurs in b, case exact. `!isin`: not.
  - `isini` and `!isini`: the same ignoring case.
  - `eq` and `ne`: text equal or not, ignoring case.
  - The word operators may be typed in any case. The symbol operators must be exact.
  - An unknown operator prints `Unknown operator '<op>'` and then `Valid are: ==, != or <>, <, <=, >, >=, [!]isin[i], eq, ne`.
- "Number": an optional leading minus, then digits with at most one point. No plus sign, no exponent, no spaces. A lone point counts (as 0).
- `then` (any case) after `b` is optional.
- The first `else` word (any case) after it ends the true branch.
- The true branch is the raw text between them, trimmed of outer spaces, with one pair of enclosing quotes removed. The false branch is the raw text after `else`, with one pair of enclosing quotes removed.
- The chosen branch is inserted at the front of the current command stream and runs next.
- An empty branch does nothing.
- Nesting: the first `else` anywhere ends the outer true branch, so a nested `if ... else` must be quoted as one branch. [derived]
- A `;` outside quotes ends the whole `if` line before `if` sees it, so further commands must be quoted inside the branch. [derived]

### B4. `trigger` / `untrigger`

Read unless marked:

- `trigger <command> <match...>`: the command is the first word (quote it to include spaces or semicolons). The match is the remaining words joined by single spaces.
  - No arguments lists the triggers as `"<command>" = "<match>"` under "Current message triggers:", or prints "No current message triggers".
  - One argument prints "Usage: trigger <command> <match>".
  - A match under 3 characters prints "Match string is too short".
  - An identical command and match pair is silently not added twice. There is no count limit.
- Matching:
  - The match is a wildcard pattern over the whole line: `*` matches any run (preferring the longest), `?` matches one character, and `\` escapes the next character.
  - Case must match exactly. Without `*` at both ends it must equal the whole line.
  - `$` macros in the stored match are expanded each time a line is tested (quote them when defining).
- Lines scanned:
  - Every server print that is not chat (low, medium and high levels, and in the extended protocol the centre and typewriter prints carried as prints).
  - Every centre print.
  - Chat lines are never scanned.
  - Each line has its high (coloured) bit cleared, and non-printing characters, including the trailing newline, are removed first.
  - Nothing is scanned during demo playback or when connected to a broadcast or relay server.
- Pseudo-lines: when entering a level the client tests the line `#cl_enterlevel`, and when the map changes the line `#cl_changelevel`, so triggers can match those names. Neither is tested during demo playback.
- What runs: for every matching trigger, in the order defined, its command is added to the end of the main command stream as its own line. It runs in that same stream later. No message is printed.
- `untrigger`:
  - With no arguments it lists, like `trigger`.
  - With no triggers it prints "No current message triggers".
  - `untrigger all` (any case) removes all and prints "Removed N trigger" or "Removed N triggers".
  - Any other single argument prints the usage line.
  - `untrigger <command> <match>` removes that exact pair, or prints `Can't find trigger "<command>" = "<match>"`.

### B5. "Run a command when ..." hooks

Read unless marked:

- `cl_beginmapcmd` (default empty): run when the client becomes active in a level, on the first valid server frame after connecting or after a map change. Right after it, the pseudo-line `#cl_enterlevel` is offered to triggers. Not during demo playback.
- `cl_changemapcmd` (default empty): run when the server sends its "changing map" command (the start of a map change). That moment comes after a recording demo is stopped and before the loading screen. Right after it, `#cl_changelevel` is offered to triggers. Not during demo playback.
- `cl_disconnectcmd` (default empty): run when the client disconnects from a connected or connecting state, for any reason, including errors that drop the connection. Not when leaving demo playback.
- Each runs by appending the cvar's text to the end of the main command stream.
- No hook exists for connect, demo start or end, or death. A connect can be caught only through `cl_beginmapcmd` or `#cl_enterlevel`.
- Server side only:
  - `sv_changemapcmd` runs after a map is loaded on the server.
  - `sv_mvd_begincmd` (default "wait 50; putaway; wait 10; help;") is run by the server's demo-recording dummy client when recording begins.
  - `sv_mvd_scorecmd` (default "putaway; wait 10; help;") is run by the same dummy client at most every 9 seconds to refresh the scoreboard.

### B6. `$cl_surface`

Read unless marked:

- When not fully in a level it is empty.
- Otherwise a line is traced from the view origin straight ahead for 8192 units. It stops at solid world geometry, solid brush entities, and water, slime or lava surfaces (liquids stop it).
- The result is "<texture name> <surface flags>". The flags are hexadecimal with a 0x prefix, for example "e1u1/floor1_3 0x0", except a value of zero prints as plain "0".
- If nothing is hit within 8192 units, the result is the empty surface: an empty name, then a space and "0". [derived]

### B7. `loc_draw`

Read unless marked:

- When `loc_draw` is non-zero, every location point within `loc_dist` units (default 500, compared as a whole number) of the player's position is drawn. Each is drawn as a copy of the game's "ancient head" item model at its own size, with its own skin and no shadow, at the point's position.
- No name text is drawn.
- The nearest location also bobs up and down by 10 units, as a sine of the client clock with a period of about 0.63 seconds (2π × 100 ms). A white dynamic light of radius 200 sits at its true position.
- Nothing at all is drawn when there is no nearest location.
- `loc_trace` (default 0) affects only which point counts as nearest. With it on, a point counts only if a straight line from the player to it is not blocked by world geometry. So it changes which point bobs and glows, and whether anything is drawn when no point is visible. It does not hide the other, blocked points.

### B8. Other scripting commands and macro-like features

Read unless marked:

- `text <args>`: inserts its arguments as a new command line at the front of the current stream. The words have already been expanded once, so the new line is expanded again. This can build a command from macros.
- `wait [n]`: delays the rest of the stream by n frames (default 1). The total pending wait is capped at 1000; at the cap it prints "Runaway wait count".
- `toggle <cvar> [values...]`:
  - With no values it flips 0 and 1. Any other value prints `"<cvar>" is "<value>", can't toggle`.
  - With values it moves to the value after the current one (matched ignoring case), wrapping round. If the current value is not in the list it prints `... can't cycle`.
  - An unknown cvar prints "<name> is not a variable".
- `inc <cvar> [amount]` and `dec <cvar> [amount]`: add or subtract (default 1). A non-numeric value prints `"<cvar>" is "<value>", can't inc`.
- `reset <cvar>`: back to its default. `resetall`: every cvar that may be set goes back to its default (the game directory excepted).
- `set <cvar> <value> [u|s]`: with exactly `u` or `s` as a fourth word, marks the cvar as user info or server info. With more words, the rest of the line is the value. Usage: "Usage: set <variable> <value> [u / s]".
- `setu` and `sets`: set and mark as user or server info. `seta`: set and mark as saved to config.
- `echo <text>`: prints the words.
- `_echo [-e] [-c <color>] [-l <level>] [-n] <text>`:
  - `-e` interprets backslash escapes: \a \b \t \n \v \f \r \\, \xHH, and octal \NNN.
  - `-c` sets the colour.
  - `-l` sets the print level, by its first letter: all, talk, developer, warning, error, notice.
  - `-n` drops the newline.
  - "Bad print level: <x>" is printed for an unknown level.
- `complete <name>`: registers a name as a command with no action, for tab completion. Running it sends it to the server, or prints `Can't "<name>", not connected`.
- `macrolist [filter]`: each macro name, padded to 16 characters, followed by its current value, then "N of M macros". The filter is a wildcard.
- `cmdlist [filter]` and `cvarlist`: list commands and cvars.
- `draw <name> <x> <y> [color]`: shows a cvar's or macro's live value on screen at x, y. A negative x is measured from the right edge. The colour is `alt`, `none` or a colour name or code. With no arguments it lists the drawn items. `undraw <name>` or `undraw all` removes them.
- `followip [n]`: connects to the most recent server address seen in chat, or the n-th older one (0 to 3).
  - The client keeps the last 4 dotted-quad addresses found in chat lines. A port must be 1024 to 65535; without a port the default port is assumed.
  - It prints "Following <addr>..." or "No IP address to follow.".
- `ignoretext` / `unignoretext <pattern>` and `ignorenick` / `unignorenick <name>`: drop matching chat lines, using wildcard patterns over the cleaned line.
- Built-in macros beyond those in the reader notes:
  - `$random`: a whole number from 0 to 9.
  - `$com_maplist`: the map names found, space-separated.
  - `$net_uprate` and `$net_dnrate`.
  - `$gl_viewcluster` and `$gl_viewleaf`.
- Keywords `$qt` and `$sc` (B1).
- None of `pick`, `cvar_reset`, `opentmp` or `$cl_chatmode` exists.

### B9. Team chat

Read unless marked:

- The client fills no percent codes. It adds no teamplay-specific macros to `say_team`.
- Only the general `$` macros apply, and only when the line goes through the command stream (console, binds, aliases, `con_auto_chat`), never to text typed in the chat prompt.
- `say` and `say_team` are passed to the server as typed after expansion.
- No sound depends on a chat line's content or on its being a team line. Every received chat line plays misc/talk.wav (`cl_chat_sound` 1, the default), misc/talk1.wav (`cl_chat_sound` 2) or nothing (0). This includes team lines; chat relayed by a broadcast server is silent when it begins "[MVD] ".
- No chat filter is specific to teams. The ignore lists apply to all chat. `cl_chat_filter` 1 strips coloured and non-printing characters from every chat line.
- One content-based automatic reply exists when the client is built with its auto-reply option (on by default):
  - A received chat line whose message, after the first ": ", begins "!version" makes the client say its version string.
  - The reply comes after a random delay of 1024 to 2047 milliseconds.
  - It answers at most once per 120 seconds, and never during demo playback or on a broadcast server.
