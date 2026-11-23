# Client settings: the gap list against the classic clients (working note)

WORKING AREA: deleted when porting ends. Which settings and commands of the first game's modern
client (`client-inventory.md`) and Quake II's stock and modern clients
(`q2-client-inventory.md`) the window has, and the ranked candidates for the next rounds,
gameplay and daily use first. The behaviour of each is in those two inventories (clean-room
notes); a building session reads only them, never the source.

**How the list was made (2026-11-18).** Every name in the two inventories' tables, matched without
case against the window's own (its `cvarlist` and `cmdlist` at start, plus every name the tree
registers by a literal: commands registered after the start configs run do not show in the first).
A name the window forwards to the server (`kill`, `use`, `drop`, `weapnext`, ...: Quake II's game
words go to its rules) counts as had. The element HUD's per-element settings (1405), the built-in
server's settings and commands, rendering internals (the GL driver's switches, `gl_ext_*`, the
software renderer, `sw_*`), developer tools and the joystick's old axis table are left out: the
window's HUD, renderer and input are its own. Roughly: of the modern client's 1155 settings, about
900 are not ours by name; of its 371 commands, about 230. Most are looks or tools no player misses.

**Homes and names.** Each round follows `docs/porting-games.md` § One name, one meaning: one name of
ours, each game's spelling a synonym in its game layer, the lowest layer that fits (engine, shooter
plugin, classic, shell, `game:<id>`), each game's default a `same` row. Names are checked with the
cvar-layers thread before a round lands.

## Done

| Classic name | Ours | Home | Round |
| --- | --- | --- | --- |
| team chat `%t %x %y %j`, `tp_msgpoint` and the pointing in `tp_msgenemypwr`, `safe`, `killme`, `utake`, `quaddead`, `getquad`, `getpent`; `tp_pointtimeout` | the same | game (`client/team_point.hpp`) | 1 |
| the server's stop-sound message | nothing to do: the reference server never sends it (`client-inventory.md` § 7); every client path already reads it | | 1 |
| `%i` | already built (pickups read from the state) | game | before |
| `fov` | a synonym of `cl_fov` in both games' layers; no `same` 90 row (`cl_fov 0`, the rig's own, is 90 and keeps the zoom's lens; a set value replaces both); Quake II's user info carries it (Quake II thread's `ClientConfig::fov`) | `game:classic`, `game:q2` | 2 |
| `cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed`, `cl_upspeed`, `cl_movespeedkey` | the same; the first game's 400 walking (the modern client's), Quake II's `same` 200 (`cl_backspeed` too: Quake II has none of its own, its back key asks the forward's speed); running asks `cl_movespeedkey` (2) times it | shooter plugin (`controls.hpp`) | 2 |
| `cl_pitchspeed`, `cl_anglespeedkey`, `+lookup`, `+lookdown`, `+klook`, `+mlook`, `freelook`, `m_forward`, `centerview`, `force_centerview` | the same; `freelook` 1 in both (the stock Quake II's 0 not kept: every config of today's players has it on) | shooter plugin | 2 |

| `weapon <n> [n ...]` (10 and 12 step), `impulse` with a list, `cl_weaponpreselect`, `cl_weaponhide`, `cl_weaponhide_axe`, `cl_weaponforgetorder` | the same (`weapon-choice.md`); the list is not yet reset on a new map (a later round) | classic (`client/weapon_choice.hpp`) | 3 |
| `w_switch`, `b_switch` | the same, user info sent on joining a first-game server; changing it while joined still needs `setinfo`; our own server does not read them yet | `game:classic` | 3 |
| `+showscores`, `+showteamscores` | the same, TAB bound to `+showscores` by default | shell (`client/controls.hpp`) | 3 |
| `ignore`, `ignore_id`, `unignore`, `unignore_id`, `ignorelist`, `ignore_team`, `unignore_team`, `unignoreall`, `unignoreall_team`, `ignore_mode`, `ignore_opponents`, `ignore_spec`, `ignore_flood`, `ignore_flood_duration` | the same (`chat-ignore.md`); a number is the player's place, not the server's user id (the window does not keep user ids yet) | engine (`console/chat_ignore.hpp`), read on a classic server joined (`client/chat_ignore.hpp`) | 4 |
| `ignore_qtv` | a synonym of our `ignore_tv` (engine) | `game:classic` | 4 |
| `ignore_no_weapon`, `ignore_not_enough_ammo` | the same | shooter plugin (`weapon_lines.hpp`) | 4 |
| `cvarlist_re`, `cmdlist_re`, `cvar_reset_re`, `toggle_re`, `unset_re`, `unalias_re`; `unset <name>` for a setting `set` made | the same; a pattern is matched anywhere in the name without case; `unset_re` never touches a game's own value | engine (`console/console_patterns.cpp`) | 5 |
| `toggle a b c` | open: one name, two meanings. The first game's modern client flips each name; Quake II's modern client cycles `a` through the values `b c` (our `cycle`) (`client-mouse-and-q2-player.md`). Ours still flips one name; the meaning per game waits for a command layer | | 5 |
| `m_filter`, `m_accel`, `m_accel_power`, `m_accel_offset`, `m_accel_senscap`, `in_raw` | the same (`client-mouse-and-q2-player.md`); Quake II's `m_accel` keeps its own meaning (counts a frame, held within 0..1); `in_raw` takes effect the next time the window takes the mouse | shooter plugin (`mouse_feel.hpp`), Quake II's meaning set by `game:q2` | 6 |
| `hand`, `gender`, `gender_auto` | the same, Quake II's user info; `gender_auto` picks from the skin's figure when joining; `hand 2` draws no weapon (round 10); `hand 1` draws it mirrored on the left, and `cl_gun` is its own (1 by the hand, 2 right, 3 left, 0 and -1 none; round 16). Beams still start from the middle | `game:q2` | 6 |
| `cl_predict` | Quake II's own setting, bound to the remote's prediction beside our `cl_nopred` (0 draws the server's place; the first game always predicts unless `cl_nopred 1`) | `game:q2` | 7 |
| `ignoretext`, `unignoretext`, `ignorenick`, `unignorenick` | the same (`client-mouse-and-q2-player.md` § 10), for every game; the ignore lists now apply on a Quake II server joined too | engine (`console/chat_ignore.hpp`) | 8 |
| `mul`, `div` | the same (the chosen fork's, `q2-client-fork-delta.md`); `div` by 0 keeps the value | engine (`console/script.cpp`) | 8 |
| `cl_predict_crouch` | the same (the chosen fork's): the predicted move's eye height replaces the frame's; 1 (the default) on our servers, which tell the move they run (svc 122), 2 on every server | `game:q2` | 9 |
| `version` (user info) | the client's name and version sent to a Quake II server on joining (`kProgramVersion`, the first game's server info too) | Quake II client | 9 |
| `cl_noskins` | Quake II's own beside the shared `noskins`: 0 and 1 mean what `noskins` 0 and 1 do in Quake II, but 2 does not (`noskins 2` keeps skins and fetches none), so they are two settings: 1 every player the stock male figure, 2 a female figure's player (female, crackhor) the stock female, every other the stock male (`client-mouse-and-q2-player.md`) | `game:q2` (picture's `stock_figures`) | 10 |
| `+zoom`, `-zoom` | the same (`client-view-and-zoom.md` § 1): while held, `cl_fov` 35 at once and `sensitivity` times 35 over the lens it had (90 for `cl_fov 0`); release puts both back as they were. No default key (ALT is `+strafe` here). In both games: Quake II's modern client has none of its own | shooter plugin (`controls.hpp`) | 11 |
| `v_viewheight` | the same (`client-view-and-zoom.md` § 2): raises or lowers the eye by -7..4 units over both heights, and any value but 0 stops the vertical bob. The engine camera's `FirstPersonRow::eye_lift_m` (`CameraCvars::viewlift`), bound in the shell for both games; Quake II's modern client has none of its own | engine camera, shell (`present.cpp`) | 11 |
| `m_autosens` | the same (`client-mouse-and-q2-player.md` § `m_autosens`): 0 off; 91..179 the field it is tuned at, any other value 90; the mouse's counts scaled across by the lens over the tuned one in degrees and up and down by their 4:3 heights. With it on, `+zoom` leaves `sensitivity` alone. In both games (the first game's client has none) | shooter plugin (`controls.hpp`) | 11 |
| `r_viewmodelsize` | the same (`client-view-and-zoom.md` § 4): 0..1, the gun's depth axis scaled by 0.5 + value / 2 about its origin (engine `PlaceRow::depth`) | `game:classic` (`cvar_layers.cpp`) | 11 |
| `r_viewmodeloffset` | the same (§ 5): up to three numbers in units, right, DOWN, forward, missing ones 0; one thing with our `cl_gunx/y/z` in two units, so it adds to them (`view::game_offset_m`) | `game:classic` | 11 |
| `cl_gun_x`, `cl_gun_y`, `cl_gun_z` | the same (§ 9): units, right, FORWARD, up; added to our `cl_gunx/y/z` | `game:q2` | 11 |
| `cl_gunfov` | the same (§ 9): the gun's lens, degrees across at 4:3, 90 by default, 0 the view's, else 30..160; our `r_viewmodel_fov` (vertical) wins when set | `game:q2` | 11 |
| `cl_physfps`, `cl_physfps_spectator` | the same (`client-net-pacing.md` § 2, 3): the moves a second a QuakeWorld server is sent, its `maxfps` (72 when it names none) lowered by the value, never under 10; 77 while watching; each move's msec whole milliseconds with the fraction carried (77 a second: one 12 in 77). Always independent physics (our drawn frames never pace the moves), so `cl_independentphysics` has nothing to switch | `classic` (`net/qw_remote.cpp`) | 12 |
| `cl_c2spps`, `cl_c2sImpulseBackup` | the same (`client-net-pacing.md`, `cl_c2spps`): packets a second, 0 one a move, else 10..72; a move over the rate held back with its sequence spent and carried in the next packet's backups, at most 2 in a row, never when one of the last 1..3 moves (by `cl_c2sImpulseBackup`, 3) has an impulse; the balance gains each move's time and is held to ±0.1 s. One difference, on purpose: the balance gains the move's time, not the drawn frame's, so the rate is the setting at any frame rate | `classic` (`net/qw_client.cpp`) | 13 |
| `v_gunkick` | the same (`client-view-and-zoom.md` § 3, `client-net-pacing.md`'s kicks): on or off, 0 by default; the server's small and big kick messages (a mod's own, sent to one player) set a target of -2 or -4 reached at 20 or 40 a second, back to 0 at 20; tracked always, drawn when on: the eye back along the view by the kick, the pitch up by half. Read from a game joined and from client and multiview recordings (a multiview recording's to the player watched) | `classic` (`net/qw_remote.cpp`, `client/demo_eyes.cpp`) | 14 |
| `cl_bobhead` | the same in our terms (`client-view-and-zoom.md` § 6, `first-person-view.md`): 0 by default; 1 the weapon's bob carried by the eye too, so the eye bobs twice over (once over `v_viewheight`, which gives up the eye's own); the weapon's forward push stays 0.4 of one bob. One difference, on purpose: the stride's bob is worked out once a frame, so the eye's and the weapon's are in phase and the cycle runs at `cl_bobcycle` | `classic` (`client/present.cpp`) | 15 |
| `cl_bonusflash` | the same in meaning (`client-view-and-zoom.md` § View tints): the server's `bf` flashes the view (215,186,69), 50 of 255, falling by 100 a second, restarted by another; our default 1, as the games always flashed (the modern client's is 0). Read from a game joined, a recording (a multiview one's to the player watched) and the window's own game; `f_bonusflash` runs on each, flashed or not | `classic` look (`app/effects.cpp`) | 17 |
| `r_viewmodellastfired` | the same (§ 7): recordings played back only; the weapon that last fired (a weapon frame not 0) stays drawn while the player looks through others, forgotten when the watched player changes or holds none | `game:classic` (`client/held.cpp`) | 17 |
| `cl_crossx`, `cl_crossy`; Quake II's `ch_x`, `ch_y`, `ch_scale`, `ch_alpha` | the same (`client-view-and-zoom.md` § Crosshair placement): right and down from the middle in the HUD's pixels; Quake II's four are spellings of ours (`ch_scale` is `crosshairsize`, `ch_alpha` `crosshairalpha`) | engine (`hud/crosshair.hpp`); `game:q2` synonyms | 18 |
| `ch_red`, `ch_green`, `ch_blue`, `ch_health` | the same: shares 0..1 over the picture; `ch_health 1` white at 100, yellow at 66, red at 33 and below, black dead | `game:q2` | 18 |
| `crosshairimage` | the same (`client-view-and-zoom.md` § Crosshair placement and pictures): `crosshairs/<name>` as TGA, PNG, JPEG or PCX, at its own pixels times `crosshairsize`, over any mark, even `crosshair 0` | engine (`hud/crosshair.hpp`), the game's reader (`client/picture_files.hpp`) | 19 |
| `tp_took`, `tp_pickup`, `tp_point` | the same (`client-teamplay-sets.md` § A): names, groups, defaults, `+`/`-`; an unknown word is said (the reference says nothing); an empty set lists "none" | game (`client/team_items.hpp`) | 19 |
| `filter` | the same (§ B): team lines by a 1..4 letter tag; `#inlay#` lines hidden | engine (`console/chat_ignore.hpp`) | 19 |
| `re_trigger`, `re_trigger_options`, `re_trigger_delete`, `re_trigger_enable`, `re_trigger_disable`, `re_trigger_match`, `re_trigger_match_0..9`, `tp_msgtriggers` | the same (§ C), the patterns ECMAScript; masks 1..16 fed (the echo's 32 and the client's own 64 not yet); the aliases not limited to a safe list | engine (`console/re_triggers.hpp`), fed by the game (`client/chat_ignore.hpp`) | 20 |
| `mapgroup`, `skygroup` | the same (§ D); a sky group takes effect on the next map | classic (`content/map_groups.hpp`), commands in the game (`client/place_names.cpp`) | 20 |
| `alias_in`, `alias_out`, `cvar_in`, `cvar_out`, `set_alias_str`, `set_bind_str`, `hash` | the same (`teamplay-scripting.md` § A): the option bits, the 1023-character cap, the hash case-blind | engine (`console/script_strings.cpp`) | teamplay |
| `msg_trigger -l`, `$triggermatch`, `if ... isin/eq/ne`, alias `$0 $@ ${N-M} %N`, `$qt`, `$sc` | the same (§ A, B): levels low, medium, high, chat, team; the game's lines fed with their level (the window's own game's too); Quake II's alias forms and `if` words in every game | engine (`console/script.cpp`, `console/lex.cpp`), fed by the game (`client/chat_ignore.cpp`) | teamplay |
| `cl_fakename`, `cl_floodprot`, `cl_fp_messages`, `cl_fp_persecond`, `cl_nofake`, `tp_soundtrigger`, `cl_queries` | the same (§ A); the menu rows not yet | `classic` layer (`client/team_chat.cpp`, `client/chat_lines.cpp`) | teamplay |
| `f_mapend`, `f_countdownstart`, `f_countdownbreak`, `f_version`, `f_server` answers | the same (§ A), from a game joined or a recording; `f_flagdeath`, `f_focusgained`, `f_trackspectate`, `f_freeflyspectate`, `f_demomatchstart`, `f_sbrefreshdone` too (`f_captureframe` waits for a capture in the window, `f_sbupdatesourcesdone` for the browser's sources) | game (`client/team_chat.cpp`, `client/chat_lines.cpp`) | teamplay |
| `$health` .. `$ledstatus` (the team's 34 `$` macros), `$time`, `$date`, `$timestamp`, `$dateiso`, `$rand`, `$serverip`, `$connectiontype`, `$latency`, `$ping`, `$lastip` | the same (§ A1), in every game by each game's `tp_name_*` words; `$loc_here`, `$loc_there` and the map commands (`cl_beginmapcmd`, `cl_changemapcmd`, `cl_disconnectcmd`) now in Quake too; fixed widths (`tp_length_*`, `tp_align_*`) too; the ruleset's restricted lines not yet | game (`client/team_macros.cpp`), engine (`console/clock_macros.cpp`) | teamplay |
| `cl_timeout`, `rcon_password` | the same (§ F); Quake II's `cl_timeout` a `same` row of 120 | `classic` layer; `game:q2` | 20 |
| `showpackets`, `showdrop` | the same lines as the Quake client's (`client-teamplay-sets.md` § F) in both games, without its ` [c]` mark; Quake II's own debug-build forms not taken | classic (`protocol/channel.hpp`'s `ChannelTrace`), cvars in `net/qw_remote.cpp` | 21 |
| `rcon_address` | the same (§ F): joined, rcon goes to that server; else to this address (27500, 27910 in Quake II), the answers said until 5 s quiet; an empty `rcon_password` still takes the password first on the line | `classic` layer (`net/qw_remote.cpp`, `net/qw_admin.cpp`) | 21 |
| `cl_solid_players` | the same (§ F) for the player's own prediction; the others' run-ahead passes through players as before | `classic` layer (`net/qw_remote.cpp`) | 21 |
| `cl_earlypackets`, `cl_nolerp_on_entity` | not taken: our client reads packets every frame (the early way, always); the mover underfoot is not known to the view yet | | 21 |
| `record`, `stop`, `easyrecord` while joined | the same (`client-demo-settings.md` § C) on a QuakeWorld server: the level from its server data, so a late start plays; `.qwd` only (no helper programs); our `record` keeps its stricter names | game (`net/qw_match_record.hpp`, `client/listen_record.cpp`) | 22 |
| `match_format_*`, `match_name_*` | the same (§ B): kinds, codes, cleaning, `_NNN` | game (`client/match_name.hpp`), `classic` layer | 22 |
| `match_auto_record`, `match_auto_minlength`, `match_auto_spectating`, `match_save`, `match_forcestart` | the same (§ A), the temporary file `temp/_match_.qwd`; the console log, screenshots, upload and window restore not taken | game (`net/qw_match_record.hpp`) | 22 |
| `record`, `stop`, `easyrecord` on a Quake II server joined | the same as QuakeWorld's (round 22), as `.dm2` blocks ended by -1 | game (`net/qw_match_record.hpp`, `net/q2_client.cpp`) | 23 |
| `match_auto_logconsole` | the same (`client-demo-settings.md` § 5): held in memory, written at the end; `log_readable` and the manual `log` not taken (our lines are plain already) | game (`net/qw_match_record.hpp`), engine's `Console::set_tap` | 23 |
| `cl_democlock`, `cl_democlock_x`, `cl_democlock_y` | the same (§ 11), a cell one line of the HUD's caption text | `classic` layer (`client/demo_playback.cpp`) | 23 |
| `cl_demowait` | the same (§ 17): held paused on the last frame | shooter layer (`client/demo_playback.cpp`) | 23 |
| `cl_chatmode` | the engine's `con_chat` with its new 2 (the first word decides); our default stays 0 so a mod's own words go to the server; `\` and `//` lines in every mode; `con_hide_chat_input` not needed (our chat is not echoed as typed) | engine (`console/view.hpp`), `game:classic` synonym | 24 |
| `s_chat_custom` (`cl_chatsound`), `s_mm1_file`, `s_mm2_file`, `s_spec_file`, `s_otherchat_file` and their volumes (`con_sound_*`) | the same (`client-chat-settings.md` § 2); a team line from another team is "other" (our reading calls it no team line) | `classic` layer (`client/chat_lines.hpp`) | 24 |
| `msg_filter`, `con_timestamps`, `con_highlight`, `con_highlight_mark` | the same (§ 2, 4, 5); `msg_filter` whatever the sound; `con_highlight` 1 changes nothing we draw | `classic` layer (`client/chat_lines.hpp`) | 24 |
| `con_notify`, `con_clearnotify` | the same (`client-chat-settings.md` § 3), every game; `con_mm2_only` not taken (our lines keep no chat kind) | engine (`console/view.hpp`) | 25 |
| `cam_thirdperson`, `cam_dist` | the same (`client-demo-settings.md` § 15), both games, over a demo or a tracked player; the distance keys and `cl_camera_tpp` not taken | game (`client/demo_eyes.hpp`, `demo_playback.hpp`) | 26 |
| `mvd_autotrack`, `autotrack`, `cl_hightrack` | autotrack as our one hottest-player plan (`autotrack.md`), not the five formulas; the top fragger the same; `demo_track` turns both off | game (`client/demo_playback.hpp`, `demo_autotrack.hpp`) | 27 |
| `demo_jump_end`, `demo_jump_skip_messages`, Quake II's `seek` forms | the same (`client-demo-settings.md` § 12, § 17); `seek N%` a share of the length, not of the bytes; `demo_jump_mark`, `demo_jump_status`, `demo_jump_rewind`, `suspend`, `resume` not taken | shooter (`demo_transport.hpp`), game (`client/demo_playback.hpp`) | 27 |
| `mvd_info`, `mvd_info_setup`, `mvd_info_show_header`, `mvd_info_x`, `mvd_info_y` | the same (`client-demo-tables.md`); drawn in the demo clock's cells and face; `%P` and `%p` empty as in the reference | game (`client/mvd_info.hpp`) | 28 |
| `demo_playlist_track_name`, `demo_benchmarkdumps` | the same; the record in the game folder's `timedemo.log`; the playlist itself not taken | game (`client/mvd_info.hpp`, `timedemo.hpp`) | 28 |
| `cam_lockpos`, `cam_zoomaccel`, `cam_zoomspeed` | the same (`client-demo-settings.md` § 15), over demos | game (`client/demo_eyes.hpp`, `demo_playback.hpp`) | 28 |
| `cl_startupdemo` | the same (`client-demo-settings.md` § 13), in place of the title's recordings; `cl_onload` not taken | `classic` layer (`client/demo_playback.cpp`) | 25 |
| `demo_jump` (Quake II's `seek`) | the same syntax (§ D); a backward jump is our timeline's seek; Quake II's percent and tenths forms not taken | shooter (`demo_transport.hpp`), `game:q2` synonym | 22 |
| `cl_maxfps` (Quake II's), `r_maxfps` | the same (`client-net-pacing.md` § 8): its `cl_maxfps` is the moves a second, 62 by default, 0 the top (125), at least 10, whole milliseconds apart (62 is 16 ms); declared `own` over our frame cap, which comes back with its value when another game loads. Its `r_maxfps` is the frame cap: one thing with our `cl_maxfps` under another name while it is loaded, started at the cap in force and saved with its own settings. Our client is always the asynchronous kind (`cl_async` 1); `cl_maxpackets` and `cl_packetdup` wait for its batched protocol | `game:q2` (`cvar_layers.cpp`, `net/q2_remote.cpp`) | 12 |
| `cvar_reset`, `viewalias` | the first game's spellings of our `reset` and `alias <name>` (command synonyms) | `game:classic` | 5 |
| `set_ex`, `set_ex2`, `set_tp`, `macrolist` | the same: `set_ex` fills the percent codes and fun characters as `echo` does, `set_ex2` leaves the fun characters, `set_tp` sets a `tp_` setting only (the strict rulesets' safe buffer, which allows only it, is not built) | game (`client/team_chat.hpp`) | 5 |

## Ranked candidates

Gameplay and the configs players carry first; each row is a round or part of one.

1. **`fov` and `default_fov`** (both games: `fov` 90, Quake II's also user info): ours is
   `cl_fov` (0 = the rig's own), so every config's `fov 110` is passed over today. A synonym in each
   game's layer and the game's default 90; Quake II sends it in its user info.
2. **The move keys' speeds**: `cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed`, `cl_upspeed`,
   `cl_movespeedkey`, `cl_anglespeedkey`, `cl_pitchspeed` (modern client 400 each, 2.0, and
   ezQuake's turn rates; Quake II 200 forward, side and up, no back speed, 1.5 and 150): the speed the client asks for, the server capping it. Shooter
   plugin settings with each game's default.
3. **Looking**: `freelook` (modern client 1; Quake II 0), `+mlook`, `+klook`, `+lookup`,
   `+lookdown`, `centerview`, `force_centerview`, `m_forward`, `m_side`, `v_centermove`,
   `v_centerspeed`. Shooter plugin.
4. **Weapon choice**: the `weapon <n> [n ...]` command (the best owned of a list; every modern
   config's weapon keys), `cl_weaponpreselect`, `cl_weaponhide`, `cl_weaponhide_axe`,
   `cl_weaponforgetorder`; `w_switch` and `b_switch` (user info the server reads on a pickup).
5. **The scoreboard keys**: `+showscores`, `+showteamscores` (TAB is bound to them in configs).
6. **Chat**: `ignore`, `unignore`, `ignorelist`, `ignore_team`, `unignoreall` (first game);
   `ignoretext`, `unignoretext`, `ignorenick`, `unignorenick` (Quake II's modern client);
   `cl_chat_sound`, `cl_chat_notify`, `cl_chat_filter`, `scr_chathud*`.
7. **Scripting a config needs**: `unset`, `cvar_reset`, `set_ex`, `set_tp`, `viewalias`, `toggle_re`,
   `cvarlist_re`, `cmdlist_re`, `unalias_re`, `macrolist`.
8. **Mouse** (done; `m_autosens` with the view round; the chosen fork's `cl_zoom_autosens` left
   for its scopes, with Action Quake).
9. **Quake II's player**: `hand`, `gender`, `gender_auto` and `cl_predict` done; `cl_vwep`
   (other players' weapons drawn), `cl_noskins` (its 2 keeps the female figure, unlike our
   `noskins 2`) and the gun's side on screen (`hand` 1 mirrored, 2 hidden; our view model is
   centred by default) next, with the view round. The chosen fork's own client additions are in
   `q2-client-fork-delta.md` (its crosshair `xhair_*`, hit markers off by default, the
   game's HUD elements, Action Quake's commands): rank them into the rounds below.
10. **The view**: `v_viewheight`, `+zoom`, `v_gunkick`, `r_viewmodelsize`, `r_viewmodeloffset`,
    `cl_bobhead`, the gun on the left, `r_viewmodellastfired`, `cl_bonusflash` (and
    `f_bonusflash`) done; `v_dlightcshift` waits for glow spheres (`gl_flashblend`) and
    `scr_fovmode` for a view the status bar shortens: neither is drawn by our client.
11. **Teamplay** (done, rounds 19 and 20): the item sets, `filter`, the regex triggers,
    `mapgroup`, `skygroup`; `tp_pointpriorities` left out (broken in its own client).
12. **Network and prediction**: `cl_physfps`, `cl_c2spps`, `cl_timeout`, `rcon_password`,
    `rcon_address`, `showdrop`, `showpackets` and `cl_solid_players` done (round 21);
    `cl_earlypackets` is always our way; `cl_nolerp_on_entity` and `qport` left.
13. **Crosshair**: `cl_crossx`, `cl_crossy` and Quake II's `ch_*` done (round 18);
    `crosshairimage` done (round 19); `crosshairscale` (the painted marks' grid) has no meaning for marks
    painted at any size.
14. **Demos and the rest**: `record` while joined, `easyrecord`, `match_format_*`,
    `match_name_*`, `match_auto_record`, `match_auto_minlength`, `match_auto_spectating`,
    `match_save`, `match_forcestart` and `demo_jump` (Quake II's `seek`) done (round 22); Quake II
    recorded while joined, `match_auto_logconsole`, `cl_democlock`, `cl_demowait` (round 23); the
    demo watcher's settings (rounds 26 to 29); the server browser's and the screenshots' (round
    30); proxy routes, the browser's caches and the remote screenshot (round 31); the rest of the broadcast and
    match-tool settings, the console's own looks.
