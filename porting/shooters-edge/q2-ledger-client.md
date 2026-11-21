# Quake II ledger: the client (every thing the client offers a player), against upstream Q2PRO

Clean-room reader's ledger, 2026-11-21. Reference: UPSTREAM Q2PRO, github.com/q2pro/q2pro, commit `601a8df` (committed 2025-12-11), its `src/client`, `src/refresh` and `src/common` parts, its menu script and its client manual (`doc/client.asciidoc`), read by a reader; described here in our words, no code. This replaces the earlier client ledger, which was read against the Action Quake community's fork of it (the wrong reference): every row of that ledger was checked against upstream, by name, default, menu row, printed words, flash light and sound and temporary entity, through a full comparison of the two programs' client, renderer and shared parts, menu scripts and manuals, and the fork-only and upstream-only names and words found that way were dropped or added. The Windows service commands (`installservice`, `deleteservice`) belong to a dedicated server and are left to the server ledger. Ours: the gameengine2 tree as of 2026-11-21. Status per row: `done` ours does it the same as a player sees, `partial` ours has it but the named part differs, `missing`, `not-taken` (a doc of ours decided; named). Cross-references: C1..C45 are the gaps of `porting/shooters-edge/q2-gaps2-client.md` (that file names the fork in places; where its words and this ledger's differ on a default or a behaviour, this ledger's are upstream's). A `done` row with only a file in the last column was matched by name in that file and by the notes (`client-gaps.md`, `q2-gaps2-client.md` § Checked and the same); a row marked partial with "verify" was found by name only.

Against the fork's ledger: **63 rows dropped as fork-only** (the Action Quake settings, weapon sounds, drawn crosshair and zoom sensitivity, the game-defined HUD and indicators, Discord and Steam, the setting sync and protocol 38's messages, `mul`/`div`, the lightstyle and muzzle-light overrides and the lag-graph scale with their menu rows, the map re-download messages); **14 rows added as upstream-only** (`cl_smooth_explosions`, `cl_lerp_lightstyles`, `cl_muzzlelight_time`, `ogg_menu_track`, `ogg_remap_tracks`, `al_merge_looping`, `gl_draworder`, `gl_clearcolor`, `$gl_viewleaf`, the player flashes' flash model, the map-version and music messages); 5 rows added for things both have that the earlier ledger missed (the scope picture in layouts and four printed lines); 70 rows rewritten where upstream differs (defaults of `cl_hit_markers` 2 and `ui_colorservers` 0, `ch_scale` not halved, `cl_footsteps` 2, `cl_async` 2, `cl_disable_particles` 32, the `cl_maxfps` cap of 125, `ogg pause`, item-name completion, the third-person camera, every player muzzle flash's light, sound and model, the hornet's remastered guns, the server browser's Mod column and colours, the sound page's "sound volume", the map checksum's plain drop, the single landing sound, and wording that named the fork). `utf8test` joined the test-command row.

Gaps this ledger finds that the C list of `q2-gaps2-client.md` does not name: a player's weapon muzzle flashes (light, sound and flash model) and every monster muzzle flash are skipped from a joined server and a recording (`client/demo_q2.cpp` reads only flashes 9 to 11); 16 temporary entities are read and dropped and the rest drawn as five generic kinds; the frame-cycling effect bits (`EF_ANIM*`), the CTF flag bits and the mission packs' effect bits are not read; light styles, the sky's configstrings and the general strings are not kept from a server; the layout words `rstring`, `rstring2`, `color`, `health_bars`, the scope picture and the five other `stat_` forms are missing; the console's own `help` hides Quake II's forwarded `help` (the help computer); ESCAPE does not put away the inventory or help computer; Alt+Enter is not fixed to full screen; the save pages have 12 slots, not 15 and an autosave; ours sends a `version` in the user info, which upstream does not; Tab does not complete item names after `use`, `drop`, `give`.

## Totals

| kind | done | partial | missing | not-taken | rows |
| --- | --- | --- | --- | --- | --- |
| Client console commands | 103 | 11 | 32 | 10 | 156 |
| Client settings (cvars) | 95 | 30 | 185 | 40 | 350 |
| Macros ($name in typed and bound text) | 14 | 1 | 13 | 2 | 30 |
| Menu pages and rows | 77 | 72 | 12 | 1 | 162 |
| Server messages the client parses (svc) | 13 | 8 | 6 | 0 | 27 |
| Temporary entities (svc_temp_entity) | 12 | 50 | 3 | 0 | 65 |
| Muzzle flashes (svc_muzzleflash, a player's) | 18 | 0 | 2 | 0 | 20 |
| Muzzle flashes (svc_muzzleflash2, a monster's) | 32 | 1 | 0 | 0 | 33 |
| Entity effect bits (EF_) | 27 | 1 | 5 | 0 | 33 |
| Entity render bits (RF_) and view bits (RDF_) | 8 | 5 | 19 | 0 | 32 |
| Configstrings | 7 | 5 | 4 | 0 | 16 |
| The layout language (status bar and layouts) | 20 | 2 | 5 | 0 | 27 |
| HUD and screen elements | 11 | 10 | 7 | 1 | 29 |
| Messages printed to the player | 32 | 56 | 54 | 5 | 147 |
| Demo playback and recording features | 4 | 10 | 7 | 2 | 23 |
| Keys: the fixed keys and the default bindings | 4 | 3 | 4 | 0 | 11 |
| Sound and music | 5 | 6 | 2 | 0 | 13 |
| Picture and video | 5 | 9 | 6 | 0 | 20 |
| Connection, network, downloads and the rest | 2 | 6 | 9 | 0 | 17 |
| **all** | 489 | 286 | 375 | 61 | 1211 |

## Client console commands

done 103, partial 11, missing 32, not-taken 10

Every command the client, its renderer, sound, menus and the shared core register (developer-only test commands at the end). Forwarded words are those the client sends on to the game library unchanged.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cmd.+forward | `+forward` / `-forward` | move forward while held (with `+klook`, look down instead) | done | `plugins/shooter/controls.cpp` |
| cmd.+back | `+back` / `-back` | move back while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveleft | `+moveleft` / `-moveleft` | step left while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveright | `+moveright` / `-moveright` | step right while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveup | `+moveup` / `-moveup` | jump (swim up) while held: Quake II's only jump key | done | `plugins/shooter/controls.cpp` (ours also has `+jump`, which Quake II lacks; C7) |
| cmd.+movedown | `+movedown` / `-movedown` | crouch (swim down) while held | done | `plugins/shooter/controls.cpp` |
| cmd.+left | `+left` / `-left` | turn left at `cl_yawspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+right | `+right` / `-right` | turn right at `cl_yawspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+lookup | `+lookup` / `-lookup` | look up at `cl_pitchspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+lookdown | `+lookdown` / `-lookdown` | look down while held | done | `plugins/shooter/controls.cpp` |
| cmd.+strafe | `+strafe` / `-strafe` | turn keys and the mouse sidestep while held | done | `plugins/shooter/controls.cpp` |
| cmd.+speed | `+speed` / `-speed` | toggles run/walk against `cl_run` while held; turn rates times `cl_anglespeedkey` | done | `plugins/shooter/controls.cpp` |
| cmd.+attack | `+attack` / `-attack` | fire while held (button bit 1) | done | `plugins/shooter/controls.cpp` |
| cmd.+use | `+use` / `-use` | the use button (button bit 2), sent only on the extended protocols | missing | C35 |
| cmd.+klook | `+klook` / `-klook` | forward/back keys look up and down while held | done | `plugins/shooter/controls.cpp` |
| cmd.+mlook | `+mlook` / `-mlook` | mouse looks up and down while held; releasing re-centres with `lookspring` | done | `plugins/shooter/controls.cpp` |
| cmd.centerview | `centerview` | levels the view pitch | done | `plugins/shooter/controls.cpp` |
| cmd.impulse | `impulse <n>` | sends an impulse byte with the next move | done | `games/shooters_edge/client/controls.cpp` |
| cmd.in_restart | `in_restart` | restarts the input (mouse) layer | missing | C33 |
| cmd.bind | `bind <key> [command]` | binds a key, or prints its binding; "\"x\" isn't a valid key" | done | `engine/console/console_binds.cpp` |
| cmd.unbind | `unbind <key>` | removes a key's binding | done | `engine/console/console_binds.cpp` |
| cmd.unbindall | `unbindall` | removes every binding | done | `engine/console/console_binds.cpp` |
| cmd.bindlist | `bindlist` | lists every binding as `key "command"` | done | `engine/console/console_binds.cpp` |
| cmd.toggleconsole | `toggleconsole` | opens or closes the console | done | `engine/console/view.cpp` |
| cmd.messagemode | `messagemode` | opens the chat line to everyone | done | `engine/console/talk.cpp` |
| cmd.messagemode2 | `messagemode2` | opens the chat line to the team | done | `engine/console/talk.cpp` |
| cmd.remotemode | `remotemode <address> <password>` | turns the console's typed lines into rcon commands to that server (prompt changes) | done | `engine/console/view.cpp` (M18k4 round 33) |
| cmd.clear | `clear` | clears the console's text | done | `engine/console/view.cpp` |
| cmd.clearnotify | `clearnotify` | clears the notify lines at the top | done | `engine/console/view.cpp` (M18k4 round 33) |
| cmd.condump | `condump <file>` | writes the console's text to a file; "Dumped console text to x." | done | `engine/console/view.cpp` (M18k4 round 32) |
| cmd.cmd | `cmd <line>` | sends the line to the server as a client command | done | `games/shooters_edge/net/game_commands.cpp` |
| cmd.say | `say <text>` | chat to everyone (macros expanded) | done | `engine/console/talk.cpp` |
| cmd.say_team | `say_team <text>` | chat to the team | done | `engine/console/talk.cpp` |
| cmd.connect | `connect <address>` | connects to a server (a second argument is ignored; protocol from `cl_protocol`) | done | `games/shooters_edge/net/q2_remote.cpp` via `net/qw_remote.cpp` |
| cmd.disconnect | `disconnect` | leaves the server, or stops a demo | done | `games/shooters_edge/net/qw_remote.cpp` |
| cmd.reconnect | `reconnect` | reconnects to the last server; "No server to reconnect to." | done | `games/shooters_edge/net/qw_remote.cpp` |
| cmd.followip | `followip [n]` | connects to the n-th last address seen in chat | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.passive | `passive` | toggles waiting for a server to connect to us (through a NAT) | missing | C42 |
| cmd.rcon | `rcon <command>` | sends a remote-console command with `rcon_password` to the server or `rcon_address` | done | `games/shooters_edge/net/qw_admin.cpp` |
| cmd.pingservers | `pingservers` | pings the address book `adr0`..`adr15` and the LAN | missing | C41 |
| cmd.serverstatus | `serverstatus [address]` | asks a server for its info and players and prints them by frags | missing | C41 |
| cmd.pause | `pause` | pauses a single-player game or a demo | partial | a word of the window's own game (`client/listen.cpp`); not found to pause a demo or a joined single-player server (C24) |
| cmd.skins | `skins` | reloads every client's skin and prints "client n: name --> model/skin" | missing | not found as a console command in Quake II's client |
| cmd.userinfo | `userinfo` | prints the user info being sent | done | `engine/console/console_more.cpp`: "User info settings:" then each key padded to 20 |
| cmd.download | `download <file>` | asks the server for a file over the connection | missing | the client asks for files only on connecting (`net/q2_client.cpp`); no command (C36) |
| cmd.dumpclients | `dumpclients` | prints every client's index and configstring | done | `games/shooters_edge/client/q2_bar.cpp` |
| cmd.dumpstatusbar | `dumpstatusbar <file>` | writes the status-bar program to a file | done | `games/shooters_edge/client/q2_bar.cpp`: written under `layouts/` |
| cmd.dumplayout | `dumplayout <file>` | writes the current layout program to a file | done | `games/shooters_edge/client/q2_bar.cpp`: written under `layouts/` |
| cmd.writeconfig | `writeconfig <file> [-a -b -c -m]` | writes aliases, binds, saved and changed settings to a file | done | `engine/console/console_more.cpp`: -a -b -c -m, default bindings and archived settings, `.cfg` added |
| cmd.vid_restart | `vid_restart` | restarts video (manual use ignored unless `cl_allow_vid_restart`) | done | `games/shooters_edge/client/window_cvars.cpp`: typed, it is ignored with the line and why (the window applies video settings at once) |
| cmd.r_reload | `r_reload` | reloads every picture and model | missing | C33 |
| cmd.snd_restart | `snd_restart` | restarts the sound system | missing | C32, C33 |
| cmd.play | `play <sound> [...]` | plays local sounds by name | done | `games/shooters_edge/client/menu_sound.cpp` (`play`, `playvol`); the music player keeps its own words under `cd` |
| cmd.stopsound | `stopsound` | stops every sound playing | done | `games/shooters_edge/client/menu_sound.cpp`: every sampled voice stopped, the music bus kept (`engine/audio/synth.cpp` `sample_stop_all`) |
| cmd.soundlist | `soundlist` | lists loaded sounds | missing | ours `soundlist` is the server's |
| cmd.soundinfo | `soundinfo` | prints the sound device's state | missing | C32 |
| cmd.ogg | `ogg info/play/stop/next/pause` | the music player: `info` describes the playing stream's format (or "Playback stopped."), `play <track> [soft]` plays a track number or a file under `music/`, `stop`, `next` (the next shuffled track, or restarts the automatic track), `pause` or `toggle` pauses and resumes | missing | ours `cd` (`engine/audio/music_console.cpp`) (C29) |
| cmd.wave | `wave <n>` | forwarded: plays a gesture | done | forwarded to the rules (`games/shooters_edge/client/main.cpp` unknown-word hook; `server/q2_commands.cpp`) |
| cmd.inven | `inven` | forwarded: opens/closes the inventory | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.kill | `kill` | forwarded: suicide | done | forwarded (`server/q2_server.cpp`) |
| cmd.use | `use <item>` | forwarded: uses an item by name; Tab completes the item names the server listed | partial | forwarded (`client/main.cpp`); the item-name completion not found |
| cmd.drop | `drop <item>` | forwarded: drops an item by name; Tab completes item names | partial | forwarded (`client/main.cpp`); the item-name completion not found |
| cmd.info | `info` | forwarded: a mod's info word | done | forwarded when not ours |
| cmd.prog | `prog` | forwarded | done | forwarded when not ours |
| cmd.give | `give <item>` | forwarded cheat; Tab completes item names | partial | `games/shooters_edge/server/q2_commands.cpp`; the item-name completion not found |
| cmd.god | `god` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.notarget | `notarget` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.noclip | `noclip` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.invuse | `invuse` | forwarded: use the selected item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invprev | `invprev` | forwarded: select the previous item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invnext | `invnext` | forwarded: select the next item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invdrop | `invdrop` | forwarded: drop the selected item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.weapnext | `weapnext` | forwarded: next weapon | done | forwarded (`server/q2_fight.cpp`) |
| cmd.weapprev | `weapprev` | forwarded: previous weapon | done | forwarded (`server/q2_fight.cpp`) |
| cmd.help | `help` (unregistered) | an unknown word, so forwarded: the help computer (the stock menu binds F1 to it) | partial | ours has a console `help`, so `help` never reaches the rules; `cmd help` does (`net/game_commands.hpp`) |
| cmd.score | `score` / `putaway` (unregistered) | forwarded: scoreboard on/off, put away a screen | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.ignoretext | `ignoretext [match ...]` | adds chat ignore patterns, or lists them with hit counts | done | `engine/console/chat_ignore.cpp` |
| cmd.unignoretext | `unignoretext [all/match ...]` | removes chat ignore patterns | done | `engine/console/chat_ignore.cpp` |
| cmd.ignorenick | `ignorenick [nick]` | ignores chat from a nickname | done | `engine/console/chat_ignore.cpp` |
| cmd.unignorenick | `unignorenick [all/nick]` | stops ignoring a nickname | done | `engine/console/chat_ignore.cpp` |
| cmd.demo | `demo [/]<file[.ext]>` | plays `.dm2`, `.dm2.gz`, `.mvd2`, `.mvd2.gz` from `demos/`, extension optional, Tab completes | partial | ours `playdemo` and the demo page (`client/demo_playback.cpp`); the `demo` spelling and the `demos/`-relative completion not found |
| cmd.record | `record [-hzes] <file>` | records a client demo; while recording prints statistics; `-z` gzip, `-e`/`-s` packet size | partial | `client/listen_record.cpp`, `net/q2_client.cpp`; switches and statistics missing (C38) |
| cmd.stop | `stop` | stops recording, prints statistics | partial | `client/listen_record.cpp`; no statistics (C38) |
| cmd.suspend | `suspend` | pauses (or toggles, `cl_demosuspendtoggle`) recording | not-taken | `client-gaps.md` round 27: not taken (C38) |
| cmd.resume | `resume` | resumes a suspended recording | not-taken | `client-gaps.md` round 27: not taken |
| cmd.seek | `seek [+-]<time/percent>[%]` | jumps in a playing demo by frames, seconds, mm:ss.ff or percent of the file | partial | synonym of `demo_jump` (`client/cvar_layers.cpp`); percent and tenths forms not taken (`client-gaps.md` round 22) |
| cmd.timerefresh | `timerefresh` | draws 128 frames turning the view and prints "x seconds (y fps)" | missing | C33 |
| cmd.aashot | `aashot` | writes the scoreboard layout as a text file (ASCII screenshot) | done | `games/shooters_edge/client/q2_bar.cpp` (`hud/q2_layout.cpp` `layout_text`): `scoreshots/quakeNNN.txt` |
| cmd.aadump | `aadump` | prints the scoreboard layout as text in the console | done | `games/shooters_edge/client/q2_bar.cpp` |
| cmd.gtv | `gtv start/stop/status` | relays the player's own game to TV viewers over TCP; only in a build that asks for it (the usual build leaves it out) | missing | ours relays a hosted match through engine TV (`engine/tv`), not a client's own game |
| cmd.sizeup | `sizeup` | screen size up 10 | done | `games/shooters_edge/hud/layout.cpp`: viewsize up 10 |
| cmd.sizedown | `sizedown` | screen size down 10 | done | `games/shooters_edge/hud/layout.cpp`: viewsize down 10 |
| cmd.sky | `sky <name> [speed] [axis]` | changes the sky box and its rotation | missing | not found as a command (only the map's `sky` key is read); C33 |
| cmd.draw | `draw <name> <x> <y> [color]` | puts a setting's or macro's value on screen | done | `engine/hud/draw_strings.cpp` |
| cmd.undraw | `undraw <name/all>` | removes a drawn value | done | `engine/hud/draw_strings.cpp` |
| cmd.clearchathud | `clearchathud` | clears the chat lines on screen | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.viewpos | `viewpos` | prints the view's position and yaw | done | `games/shooters_edge/client/place_seen.cpp` |
| cmd.gun_next | `gun_next` | debug: show the next view model | missing | C33 |
| cmd.gun_prev | `gun_prev` | debug: show the previous view model | missing | C33 |
| cmd.gun_model | `gun_model <name>` | debug: show a named view model | missing | C33 |
| cmd.fog | `fog [r g b density] [sky_factor]` | prints or sets the global fog | done | `plugins/classic/app/look.cpp` |
| cmd.loc | `loc add/del/set/list/save` | edits the map's place names | done | `games/shooters_edge/client/place_names.cpp` |
| cmd.pushmenu | `pushmenu <menu> [args]` | opens a menu page by name | done | `games/shooters_edge/menu/game_menu.cpp`: the game's pages by name, Quake II's names mapped |
| cmd.popmenu | `popmenu` | closes the top menu page | done | `games/shooters_edge/menu/game_menu.cpp` |
| cmd.forcemenuoff | `forcemenuoff` | closes every menu page | done | `games/shooters_edge/menu/game_menu.cpp` |
| cmd.screenshot | `screenshot [format]` | writes a screenshot in `gl_screenshot_format` | partial | `engine/capture/screenshot.cpp`; `gl_screenshot_format` and `_quality` are synonyms of ours; the file name template missing |
| cmd.screenshotpng | `screenshotpng [file] [compression]` | PNG screenshot | done | `engine/capture/screenshot.cpp` |
| cmd.screenshotjpg | `screenshotjpg [file] [quality]` | JPEG screenshot | done | `engine/capture/screenshot.cpp` |
| cmd.screenshottga | `screenshottga [file]` | TGA screenshot | done | `engine/capture/screenshot.cpp` |
| cmd.imagelist | `imagelist` | lists loaded pictures | not-taken | `client-gaps.md`: rendering internals left out |
| cmd.modellist | `modellist` | lists loaded models | not-taken | `client-gaps.md`: rendering internals left out |
| cmd.strings | `strings` | prints the renderer's version strings | not-taken | `client-gaps.md`: rendering internals left out |
| cmd.cleardebuglines | `cleardebuglines` | clears debug lines | not-taken | `client-gaps.md`: developer tools left out |
| cmd.cmdlist | `cmdlist [wildcard]` | lists commands | done | `engine/console/console_builtins.cpp` |
| cmd.macrolist | `macrolist` | lists macros | done | `games/shooters_edge/client/team_chat.cpp` |
| cmd.exec | `exec <file>` | runs a config | done | `engine/console/console_builtins.cpp` |
| cmd.echo | `echo <text>` | prints text (macros expanded) | done | `engine/console/console_builtins.cpp` |
| cmd._echo | `_echo <text>` | prints text without expansion | done | `engine/console/console_more.cpp` |
| cmd.alias | `alias <name> <cmd>` | defines a command alias | done | `engine/console/console_builtins.cpp` |
| cmd.unalias | `unalias <name>` | removes an alias | done | `engine/console/console_builtins.cpp` |
| cmd.wait | `wait [n]` | delays the rest of the buffer by frames | done | `engine/console/console_builtins.cpp` |
| cmd.text | `text <str>` | adds text to the command buffer | done | `engine/console/console_more.cpp` |
| cmd.complete | `complete <partial>` | prints completions | done | `engine/console/console_more.cpp`: a name forwarded to the game, refused for a setting's name |
| cmd.trigger | `trigger <cmd> <match>` | runs a command when a server line matches | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.untrigger | `untrigger [all/cmd match]` | removes triggers | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.if | `if <a> <op> <b> then ... [else ...]` | conditional | done | `engine/console/script.cpp` |
| cmd.set | `set <cvar> <value> [u/s]` | sets or creates a setting | done | `engine/console/console_builtins.cpp` |
| cmd.setu | `setu <cvar> <value>` | set as user info | done | `engine/console/console_more.cpp` |
| cmd.sets | `sets <cvar> <value>` | set as server info | done | `engine/console/console_more.cpp` |
| cmd.seta | `seta <cvar> <value>` | set and save | done | `engine/console/console_builtins.cpp` |
| cmd.cvarlist | `cvarlist [-achlmnrstuv] [wildcard]` | lists settings by flags | partial | `engine/console/console_builtins.cpp`; flag switches not compared |
| cmd.toggle | `toggle <cvar> [v1 v2 ...]` | flips 0/1 or cycles listed values | done | `engine/console/script.cpp`: with values, the next after the one matched; "can't cycle" when none is |
| cmd.inc | `inc <cvar> [n]` | adds | done | `engine/console/script.cpp` |
| cmd.dec | `dec <cvar> [n]` | subtracts | done | `engine/console/script.cpp` |
| cmd.reset | `reset <cvar>` | resets to default | done | `engine/console/console_builtins.cpp` |
| cmd.resetall | `resetall` | resets every setting | done | `engine/console/console_more.cpp`: read-only settings and `game` kept |
| cmd.quit | `quit` | exits | done | `games/shooters_edge/client/main.cpp` |
| cmd.recycle | `recycle` | restarts the program in place | missing | not found |
| cmd.lasterror | `lasterror` | prints the last error | done | `engine/console/console_more.cpp` |
| cmd.path | `path` | prints the search path | missing | ours prints it through `ge`/`secli`, not the console |
| cmd.dir | `dir <path> [ext]` | lists files in the search path | missing | not found |
| cmd.fdir | `fdir <wildcard>` | lists files by wildcard | missing | not found |
| cmd.whereis | `whereis <path> [all]` | says which pack or folder a file comes from | missing | not found |
| cmd.fs_stats | `fs_stats` | file-system counters | not-taken | `client-gaps.md`: developer tools left out |
| cmd.link | `link <from> <to>` | links one path to another | missing | not found |
| cmd.unlink | `unlink <from>` | removes a link | missing | not found |
| cmd.softlink | `softlink <name> <target>` | a soft link in the search path | missing | not found |
| cmd.softunlink | `softunlink [-ah] <name>` | removes a soft link | missing | not found |
| cmd.fs_restart | `fs_restart` | restarts the file system | missing | C33 |
| cmd.bsplist | `bsplist` | lists loaded maps | not-taken | `client-gaps.md`: developer tools left out |
| cmd.z_stats | `z_stats` | memory statistics | not-taken | `client-gaps.md`: developer tools left out |
| cmd.net_restart | `net_restart` | restarts networking | missing | not found |
| cmd.net_stats | `net_stats` | prints network counters | missing | not found |
| cmd.showip | `showip` | prints the local addresses | missing | not found |
| cmd.dns | `dns <name>` | resolves a host name | missing | not found |
| cmd.tests | `error`, `errordrop`, `freeze`, `crash`, `doublefree`, `printjunk`, `bsptest`, `wildtest`, `normtest`, `infotest`, `snprintftest`, `modeltest`, `imagetest`, `soundtest`, `activate`, `mdfourtest`, `mdfoursum`, `extcmptest`, `nextpathtest`, `extract`, `utf8test` | the test build's self-tests and fault triggers | not-taken | `client-gaps.md`: developer tools left out |

## Client settings (cvars)

done 95, partial 30, missing 185, not-taken 40

Every setting the client, its renderer, sound, menus, the shared core and the platform layers register, with upstream's default. Names our Quake II layer spells another way count as done when the spelling is a synonym of ours.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cvar.con_notifytime | `con_notifytime` | seconds a printed line stays in the notify lines (default 3) | done | `engine/console/view.cpp` |
| cvar.con_notifylines | `con_notifylines` | how many notify lines are shown (default 4) | done | `engine/console/view.cpp` |
| cvar.con_clock | `con_clock` | a clock in the console's corner (default 0) | done | `engine/console/view.cpp` (M18k4 round 32) |
| cvar.con_height | `con_height` | the console's share of the screen height (default 0.5) | done | `engine/console/view.cpp` |
| cvar.scr_conspeed | `scr_conspeed` | how fast the console slides (default 3) | done | `games/shooters_edge/client/game_start.cpp` |
| cvar.con_alpha | `con_alpha` | the console's opacity (default 1) | done | a synonym of `con_bgalpha` (M18k4 round 33) |
| cvar.con_scale | `con_scale` | the console text's scale, 0 auto by window size (default 0) | done | `engine/console/view.cpp` (M18k4 round 33) |
| cvar.con_font | `con_font` | the console's font picture (default `conchars`) | partial | `engine/console/view.cpp` (ours names our charsets; verify a pack picture name is taken) |
| cvar.con_background | `con_background` | the console's backdrop picture (default `conback`) | missing | ours always draws the pack's `conback` (C23) |
| cvar.con_scroll | `con_scroll` | bit sum: a new line or a key press scrolls the console to the bottom (default 0) | missing | C23 |
| cvar.con_history | `con_history` | lines of typed history kept (default 128) | missing | C23 |
| cvar.con_timestamps | `con_timestamps` | stamps every console line with the time (default 0) | partial | `client/chat_lines.cpp` stamps chat lines (the first game's meaning); verify every line |
| cvar.con_timestampsformat | `con_timestampsformat` | the stamp's strftime format (default `%H:%M:%S `) | missing | C23 |
| cvar.con_timestampscolor | `con_timestampscolor` | the stamp's colour (default `#aaa`) | missing | C23 |
| cvar.con_auto_chat | `con_auto_chat` | a typed line without a slash: 0 a command, 1 say, 2 say_team (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` (M18k4 round 32) |
| cvar.cl_demosnaps | `cl_demosnaps` | seconds between seek snapshots while a demo plays (default 10) | partial | ours keeps its own keyframes (`engine/replay`); no setting |
| cvar.cl_demomsglen | `cl_demomsglen` | the largest message a recorded demo frame holds (default 1390) | not-taken | `client-gaps.md` (C38): not taken |
| cvar.cl_demowait | `cl_demowait` | 1 holds a finished demo on its last frame (default 0) | done | `games/shooters_edge/client/demo_playback.cpp` |
| cvar.cl_demosuspendtoggle | `cl_demosuspendtoggle` | `suspend` toggles rather than only pauses (default 1) | not-taken | `client-gaps.md` round 27: `suspend` not taken |
| cvar.cl_http_downloads | `cl_http_downloads` | fetch files over HTTP when the server names a URL (default 1) | missing | C37 |
| cvar.cl_http_filelists | `cl_http_filelists` | read a `.filelist` of extra files for a map or mod (default 1) | missing | C37 |
| cvar.cl_http_max_connections | `cl_http_max_connections` | parallel HTTP fetches (default 2) | missing | C37 |
| cvar.cl_http_proxy | `cl_http_proxy` | an HTTP proxy for downloads (default empty) | missing | C37 |
| cvar.cl_http_default_url | `cl_http_default_url` | a download URL used when the server names none (default empty) | missing | C37 |
| cvar.cl_http_insecure | `cl_http_insecure` | 1 skips TLS certificate checks (default 0) | missing | C37 |
| cvar.cl_http_debug | `cl_http_debug` | prints HTTP transfer details (default 0) | missing | C37 |
| cvar.cl_http_blocking_timeout | `cl_http_blocking_timeout` | seconds before a stalled HTTP fetch is dropped (default 15) | missing | C37 |
| cvar.in_enable | `in_enable` | use the input layer at all (default 1) | missing | C35 |
| cvar.in_grab | `in_grab` | 0 never grab the mouse in a window, 1 in a game, 2 also let go in demos and chase (default 1) | missing | C35 |
| cvar.cl_nodelta | `cl_nodelta` | 1 asks the server for full frames, never deltas (default 0) | missing | C43 |
| cvar.cl_maxpackets | `cl_maxpackets` | packets a second on the modern protocols, moves batched (default 30) | missing | waits for the batched protocol (`client-gaps.md` round 12, C43) |
| cvar.cl_fuzzhack | `cl_fuzzhack` | sends moves with a small jitter to beat a server's speed check (default 0) | missing | not found |
| cvar.cl_packetdup | `cl_packetdup` | each move sent again in the next packet (default 1) | missing | waits for the batched protocol (C43) |
| cvar.cl_showpackets | `cl_showpackets` | prints each move packet sent (default 0) | missing | ours `showpackets` (`plugins/classic/protocol/channel.hpp`) |
| cvar.cl_instantpacket | `cl_instantpacket` | a button press sent at once (default 1) | missing | C43 |
| cvar.cl_batchcmds | `cl_batchcmds` | batch several moves a packet (default 1) | missing | C43 |
| cvar.cl_upspeed | `cl_upspeed` | up and down move per key (default 200) | done | `games/shooters_edge/client/cvar_layers.cpp` (`same` 200) |
| cvar.cl_forwardspeed | `cl_forwardspeed` | forward and back move per key (default 200) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_sidespeed | `cl_sidespeed` | sideways move per key (default 200) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_yawspeed | `cl_yawspeed` | turn rate of the turn keys (default 140) | done | `plugins/shooter/controls.cpp` |
| cvar.cl_pitchspeed | `cl_pitchspeed` | look rate of the look keys; cheat-protected (default 150) | done | `plugins/shooter/controls.cpp` |
| cvar.cl_anglespeedkey | `cl_anglespeedkey` | turn rates times this while `+speed` held; cheat-protected (default 1.5) | done | `plugins/shooter/controls.cpp` |
| cvar.cl_run | `cl_run` | always run (default 1, saved) | done | `plugins/shooter/controls.cpp` |
| cvar.freelook | `freelook` | the mouse always looks up and down (default 1, saved) | done | `plugins/shooter/controls.cpp` |
| cvar.lookspring | `lookspring` | re-centre the view on releasing `+mlook` (default 0) | done | `plugins/shooter/controls.cpp` |
| cvar.lookstrafe | `lookstrafe` | the mouse strafes while `+mlook` is held (default 0) | done | `plugins/shooter/controls.cpp` |
| cvar.sensitivity | `sensitivity` | mouse speed (default 3, saved) | done | `plugins/shooter/controls.cpp` |
| cvar.m_pitch | `m_pitch` | degrees of pitch per mouse count, negative inverts (default 0.022) | done | `plugins/shooter/controls.cpp` |
| cvar.m_yaw | `m_yaw` | degrees of yaw per mouse count (default 0.022) | done | `plugins/shooter/controls.cpp` |
| cvar.m_forward | `m_forward` | forward move per vertical mouse count when not looking (default 1) | done | `plugins/shooter/controls.cpp` |
| cvar.m_side | `m_side` | sideways move per horizontal mouse count while strafing (default 1) | missing | C35 |
| cvar.m_filter | `m_filter` | average this frame's and last frame's mouse move (default 0) | done | `plugins/shooter/mouse_feel.cpp` |
| cvar.m_accel | `m_accel` | mouse acceleration (default 0) | done | `plugins/shooter/mouse_feel.cpp` (Quake II's meaning, `client-gaps.md` round 6) |
| cvar.m_autosens | `m_autosens` | scale the mouse by the field of view (default 0) | done | `plugins/shooter/controls.cpp` |
| cvar.loc_trace | `loc_trace` | 1 picks the nearest place that can be seen (default 0) | done | `games/shooters_edge/client/place_names.cpp` |
| cvar.loc_draw | `loc_draw` | draws each place point as a marker (default 0) | missing | C45 |
| cvar.loc_dist | `loc_dist` | the farthest a place counts, units (default 500) | done | `games/shooters_edge/client/place_names.cpp` |
| cvar.cl_gun | `cl_gun` | 0 hides the held gun, 2 right, 3 left (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gunalpha | `cl_gunalpha` | the held gun's opacity (default 1) | missing | C16 |
| cvar.cl_gunfov | `cl_gunfov` | the gun's own field of view (default 90) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_x | `cl_gun_x` | moves the gun right, units (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_y | `cl_gun_y` | moves the gun forward (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_z | `cl_gun_z` | moves the gun up (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_footsteps | `cl_footsteps` | 0 no footsteps (and tells the server, on the modern protocol), 1 each surface's own step set when one is found, 2 always the default step sound (default 1) | missing | C16, C34 |
| cvar.cl_noskins | `cl_noskins` | 1 every player the stock male, 2 female figures the stock female (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_predict | `cl_predict` | predict the player's own move (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_kickangles | `cl_kickangles` | 0 drops the view kick the server sends; cheat-protected (default 1) | done | `games/shooters_edge/client/q2_view.cpp` (M18k4 round 34) |
| cvar.cl_warn_on_fps_rounding | `cl_warn_on_fps_rounding` | warns when `cl_maxfps` will be rounded to whole milliseconds (default 1) | missing | not found |
| cvar.cl_maxfps | `cl_maxfps` | moves (packets) a second on the asynchronous client, held between 10 and 125 (default 62) | done | `games/shooters_edge/net/q2_remote.cpp` (`own` row, `client-gaps.md` round 12); verify the 125 cap |
| cvar.cl_async | `cl_async` | 0 one rate for moves and frames, 1 frames drawn apart from the move rate, 2 the display's buffer swaps pace the drawing (needs a newer OpenGL) (default 1) | partial | ours is always asynchronous; no setting (`client-gaps.md` round 12) |
| cvar.r_maxfps | `r_maxfps` | caps drawn frames a second, 0 none (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_autopause | `cl_autopause` | pause single player and demos while the console or a menu is up (default 1) | missing | C24 |
| cvar.cl_rollhack | `cl_rollhack` | other players' models lean the classic renderer's (wrong) way (default 1) | missing | C16 |
| cvar.cl_noglow | `cl_noglow` | 1 stops items' light pulsing (default 0) | partial | ours `r_itempulse` (Quake II's layer 1) does the same inverted; no `cl_noglow` spelling (C16) |
| cvar.cl_nobob | `cl_nobob` | 1 stops the view bobbing (default 0) | missing | ours has `cl_bob` (`engine/camera/cvars.cpp`); no `cl_nobob` spelling |
| cvar.cl_nolerp | `cl_nolerp` | 1 draws entities at the newest frame without interpolation (default 0) | partial | `games/shooters_edge/net/qw_remote.cpp` (the first game's: missiles and movers only); verify on a Quake II server |
| cvar.cl_shownet | `cl_shownet` | prints received messages: 1 sizes, 2 kinds, 3 entities (default 0) | missing | C43 |
| cvar.cl_showmiss | `cl_showmiss` | prints each prediction miss (default 0) | missing | C43 |
| cvar.showclamp | `showclamp` | prints when the client's time is clamped to the frames it has (default 0) | missing | not found |
| cvar.cl_timeout | `cl_timeout` | seconds of silence before the connection is dropped (default 120) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.rcon_address | `rcon_address` | where `rcon` goes when not connected (default empty) | done | `games/shooters_edge/net/qw_remote.cpp` |
| cvar.cl_thirdperson | `cl_thirdperson` | draws the view from behind the player; the camera stops at walls and at moving doors and platforms (traced where they are drawn), and the player's own model fades as the camera is pushed close; cheat-protected (default 0) | missing | ours has `cam_thirdperson` over demos only (`client-gaps.md` round 26) |
| cvar.cl_thirdperson_angle | `cl_thirdperson_angle` | the third-person camera's angle round the player (default 0) | missing | not found |
| cvar.cl_thirdperson_range | `cl_thirdperson_range` | the third-person camera's distance (default 60) | missing | ours `cam_dist` over demos only |
| cvar.cl_disable_particles | `cl_disable_particles` | bit sum: 1 grenade blast, 2 grenade trail, 4 rocket blast, 8 rocket trail, 16 blood, 32 blaster trail particles off (default 0) | missing | C16 |
| cvar.cl_disable_explosions | `cl_disable_explosions` | bit sum: 1 grenade, 2 rocket explosion models off (default 0) | missing | C16 |
| cvar.cl_dlight_hacks | `cl_dlight_hacks` | bit sum: 1 rocket light recoloured, 2 smaller blast lights, 4 no muzzle light (default 0) | missing | C16 |
| cvar.cl_smooth_explosions | `cl_smooth_explosions` | explosions fade smoothly over their whole life, which also fixes some explosions' length (default 1) | missing | ours draws its own explosions; not this switch (C16) |
| cvar.cl_lerp_lightstyles | `cl_lerp_lightstyles` | 1 blends light styles between their letters instead of stepping (default 0) | missing | not found (C21) |
| cvar.cl_gibs | `cl_gibs` | 0 draws no gibs and tells the server (default 1) | missing | ours `cl_gibfilter` is the first game's (C16) |
| cvar.cl_flares | `cl_flares` | 0 hides the flares the game sends (extended protocol) (default 1) | missing | C16 |
| cvar.cl_updaterate | `cl_updaterate` | asks the server for this many frames a second (extended protocol) (default 0) | partial | `engine/net/net_cvars.cpp` is our own protocol's; not sent to a Quake II server |
| cvar.cl_chat_notify | `cl_chat_notify` | 0 keeps chat out of the notify lines (default 1) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_chat_sound | `cl_chat_sound` | 0 none, 1 `misc/talk.wav`, 2 `misc/talk1.wav`, or a sound's name (default 1) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_chat_filter | `cl_chat_filter` | 1 strips unprintable characters from chat (default 0) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_disconnectcmd | `cl_disconnectcmd` | a command run at each disconnect (default empty) | missing | C33 |
| cvar.cl_changemapcmd | `cl_changemapcmd` | a command run at each map change (default empty) | missing | C33 |
| cvar.cl_beginmapcmd | `cl_beginmapcmd` | a command run on entering each map (default empty) | missing | C33 |
| cvar.cl_ignore_stufftext | `cl_ignore_stufftext` | 1 refuses server-pushed commands not on a whitelist file (default 0) | missing | C33 |
| cvar.cl_allow_vid_restart | `cl_allow_vid_restart` | 1 lets a typed `vid_restart` run (default 0) | missing | C33 |
| cvar.cl_protocol | `cl_protocol` | 0 the best the server offers; 34, 35, 36 forced (default 0) | missing | ours speaks 34 only (C42) |
| cvar.gender_auto | `gender_auto` | set `gender` from the skin's model (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_vwep | `cl_vwep` | draw other players' weapons (default 1) | done | synonym of `r_drawvweps` (`client/cvar_layers.cpp`) |
| cvar.password | `password` | user info: the join password | done | `games/shooters_edge/net/q2_client.cpp` |
| cvar.spectator | `spectator` | user info: join as spectator (or its password) | done | `games/shooters_edge/net/q2_client.cpp` |
| cvar.name | `name` | user info: the player's name (default `unnamed`) | partial | `net/q2_client.cpp` sends it at the connect only; a change while joined not sent (C30) |
| cvar.skin | `skin` | user info: model/skin (default `male/grunt`) | partial | sent at the connect only (C30) |
| cvar.rate | `rate` | user info: bytes a second the server may send (default 15000) | done | `plugins/classic` shared user info; a change while joined resends it (`q2_client_userinfo`) |
| cvar.msg | `msg` | user info: the lowest print level wanted (default 1) | partial | always sent as 1 (C30) |
| cvar.hand | `hand` | user info: 0 right, 1 left, 2 centre (default 0) | partial | `client/cvar_layers.cpp`; sent at the connect only (C30) |
| cvar.fov | `fov` | user info: the field of view (default 90) | partial | synonym of `cl_fov`; the field the server sends back is not drawn (C3), sent at the connect only (C30) |
| cvar.gender | `gender` | user info: male, female, none (default male) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.uf | `uf` | user info: flags (4 own FOV in demos, 32 recorded FOV, others for the server) (default empty) | done | flags 4 and 32 read for recordings, `client/q2_view.cpp` (M18k4 round 34); not sent in the user info |
| cvar.version | `version` | the program's name and version, read-only; part of a hosted server's info, not of the user info | partial | ours sends a version in the user info (`games/shooters_edge/net/q2_client.cpp`), which the original does not |
| cvar.vid_ref | `vid_ref` | read-only: the renderer (gl) | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.vid_driver | `vid_driver` | the video driver to use | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.vid_fullscreen | `vid_fullscreen` | 0 windowed, else the mode number from `vid_modelist` (default 0) | partial | `engine/platform/window_cvars.cpp` (on/off); no mode list |
| cvar._vid_fullscreen | `_vid_fullscreen` | the mode Alt+Enter goes back to (default 1) | missing | not found |
| cvar.vid_geometry | `vid_geometry` | the window's size and place, `WxH+X+Y` (default 640x480) | missing | not found |
| cvar.vid_modelist | `vid_modelist` | the full-screen modes offered (default `modelist`, the desktop's) | missing | not found |
| cvar.vid_hwgamma | `vid_hwgamma` | gamma by the display hardware (default 0) | missing | not found |
| cvar.vid_flip_on_switch | `vid_flip_on_switch` | Windows: switch to the desktop mode when the window loses focus (default 0) | missing | Windows only |
| cvar.viewsize | `viewsize` | the 3D view's size 40..100 percent, border tiled (default 100) | partial | ours is the first game's meaning (100..120) (C8) |
| cvar.scr_showpause | `scr_showpause` | 1 draws the pause picture, 2 "[PAUSED]" on the demo bar (default 1) | missing | C24 |
| cvar.scr_centertime | `scr_centertime` | seconds a centre print stays, faded 300 ms (default 2.5) | partial | `client/cvar_layers.cpp`; no fade, line cut differs (C9) |
| cvar.scr_printspeed | `scr_printspeed` | letters a second of a typewriter centre print (extended protocol) (default 16) | missing | not found |
| cvar.scr_demobar | `scr_demobar` | 1 a progress bar while a demo plays, 2 also the time (default 1) | partial | ours `demo_bar` (C39) |
| cvar.scr_font | `scr_font` | the HUD's font picture (default `conchars`) | missing | not found |
| cvar.scr_scale | `scr_scale` | the 2D scale, 0 auto by window height (default 0) | done | `games/shooters_edge/client/q2_bar.cpp` `q2_bar_scale` (M18k4 round 34) |
| cvar.crosshair | `crosshair` | 0 none, N the `chN` picture (default 0) | done | `engine/hud/crosshair.hpp` (Quake II's layer `same`) |
| cvar.netgraph | `netgraph` | 1 latency bars, 2 packet sizes (default 0) | missing | ours `r_netgraph` (C13) |
| cvar.timegraph | `timegraph` | a frame-time graph (default 0) | missing | C13 |
| cvar.debuggraph | `debuggraph` | a development graph (default 0) | missing | C13 |
| cvar.graphheight | `graphheight` | the graph's height (default 32) | missing | C13 |
| cvar.graphscale | `graphscale` | the graph's vertical scale (default 1) | missing | C13 |
| cvar.graphshift | `graphshift` | the graph's vertical offset (default 0) | missing | C13 |
| cvar.scr_chathud | `scr_chathud` | 0 off, 1 last chat lines on screen, 2 in the alternate colour (default 0) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.scr_chathud_lines | `scr_chathud_lines` | chat lines shown (default 4) | done | (M18k4 round 32) |
| cvar.scr_chathud_time | `scr_chathud_time` | seconds a chat line stays, 0 for ever (default 0) | done | (M18k4 round 32) |
| cvar.scr_chathud_x | `scr_chathud_x` | the chat lines' x (default 8) | done | (M18k4 round 32) |
| cvar.scr_chathud_y | `scr_chathud_y` | the chat lines' y, negative from the bottom (default -64) | done | (M18k4 round 32) |
| cvar.ch_health | `ch_health` | crosshair coloured by health: red full, green rising from 33 to 66, blue from 66 to 99, black when dead (default 0) | partial | `client/cvar_layers.cpp`; ours steps, the original ramps smoothly (C27) |
| cvar.ch_red | `ch_red` | crosshair red (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_green | `ch_green` | crosshair green (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_blue | `ch_blue` | crosshair blue (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_alpha | `ch_alpha` | crosshair opacity (default 1) | done | synonym of `crosshairalpha` (`client/cvar_layers.cpp`) |
| cvar.ch_scale | `ch_scale` | crosshair picture scale, 0.1 to 9 (default 1) | done | synonym of `crosshairsize` (`client/cvar_layers.cpp`); verify the 0.1 to 9 clamp |
| cvar.ch_x | `ch_x` | crosshair x offset (default 0) | done | synonym of `cl_crossx` (`client/cvar_layers.cpp`) |
| cvar.ch_y | `ch_y` | crosshair y offset (default 0) | done | synonym of `cl_crossy` (`client/cvar_layers.cpp`) |
| cvar.scr_draw2d | `scr_draw2d` | 0 no 2D, 1 no status bar, 2 all, 3 layouts only with F1 (default 2) | partial | 0, 1, 2 and 3 (M18k4 round 34); F1 held does not bring the layouts back at 3 |
| cvar.scr_showturtle | `scr_showturtle` | network trouble words, 2 more of them (default 1) | missing | C12 |
| cvar.scr_lag_x | `scr_lag_x` | the lag graph's x (default -1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_lag_y | `scr_lag_y` | the lag graph's y (default -1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_lag_draw | `scr_lag_draw` | 1 the lag graph, 2 on a dark box (default 0) | done | `engine/hud/net_graph.cpp` `draw_lag_graph` (M18k4 round 34) |
| cvar.scr_lag_min | `scr_lag_min` | the lag graph's bottom, ms (default 0) | done | (M18k4 round 34) |
| cvar.scr_lag_max | `scr_lag_max` | the lag graph's top, ms (default 200) | done | (M18k4 round 34) |
| cvar.scr_alpha | `scr_alpha` | the opacity of every 2D element but the crosshair (default 1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_showstats | `scr_showstats` | debug: prints the player's stats on screen (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.scr_showpmove | `scr_showpmove` | debug: the move state on screen (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.scr_hit_marker_time | `scr_hit_marker_time` | ms a hit marker shows (default 500) | missing | C26 |
| cvar.cl_muzzleflashes | `cl_muzzleflashes` | 0 hides the newer game's muzzle flash models (the flash lights stay) (default 1) | missing | ours `cl_muzzleflash` is the first game's (C16) |
| cvar.cl_muzzlelight_time | `cl_muzzlelight_time` | how long a muzzle flash light lasts, 0 to 1000 ms (default 16) | missing | the wire's flashes are skipped (tables "Muzzle flashes"); C16 |
| cvar.cl_hit_markers | `cl_hit_markers` | 0 off, 1 a hit marker, 2 a marker and a sound (default 2) | missing | C26 |
| cvar.cl_railtrail_type | `cl_railtrail_type` | 0 classic, 1 core beam, 2 core and spiral (default 0) | missing | C17 |
| cvar.cl_railtrail_time | `cl_railtrail_time` | seconds the rail trail lasts (default 1.0) | missing | C17 |
| cvar.cl_railcore_color | `cl_railcore_color` | the rail core's colour (default red) | missing | C17 |
| cvar.cl_railcore_width | `cl_railcore_width` | the rail core's width (default 2) | missing | C17 |
| cvar.cl_railspiral_color | `cl_railspiral_color` | the rail spiral's colour (default blue) | missing | C17 |
| cvar.cl_railspiral_radius | `cl_railspiral_radius` | the rail spiral's radius (default 3) | missing | C17 |
| cvar.cl_testblend | `cl_testblend` | debug: a fixed test tint (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testparticles | `cl_testparticles` | debug: a field of test particles (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testentities | `cl_testentities` | debug: a ring of test models (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testlights | `cl_testlights` | debug: test lights; cheat-protected (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_stats | `cl_stats` | debug: prints the view's entity, light and particle counts (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_lights | `cl_lights` | 0 adds no dynamic lights (default 1) | missing | C16 |
| cvar.cl_particles | `cl_particles` | 0 adds no particles (default 1) | missing | C16 |
| cvar.cl_entities | `cl_entities` | 0 adds no entities (default 1) | missing | C16 |
| cvar.cl_blend | `cl_blend` | 0 drops the full-screen tints (default 1) | missing | C16 |
| cvar.cl_adjustfov | `cl_adjustfov` | 1 hor+ on wide screens, 0 vert- (default 1) | done | `games/shooters_edge/client/q2_view.cpp` (M18k4 round 34) |
| cvar.ui_sortdemos | `ui_sortdemos` | the demo browser's sort column, negative descending (default 1) | missing | C40 |
| cvar.ui_listalldemos | `ui_listalldemos` | the demo browser lists every folder (default 0) | missing | C40 |
| cvar.ui_sortservers | `ui_sortservers` | the server browser's sort column (default 0) | missing | C41 |
| cvar.ui_colorservers | `ui_colorservers` | colour the server list: grey for a password, anticheat 2 or a no-fake server, green under 30 ms, white otherwise (default 0) | missing | C41 |
| cvar.ui_pingrate | `ui_pingrate` | server pings a second, 0 from `rate` (default 0) | missing | C41 |
| cvar.ui_scale | `ui_scale` | the menus' scale, 0 auto (default 0) | missing | C4 |
| cvar.ui_debug | `ui_debug` | debug: outlines menu items (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.ui_open | `ui_open` | 1 opens the main menu at start-up (default 0) | missing | ours opens its own title menu always |
| cvar.adr | `adr0` .. `adr15` | the address book: 16 saved server addresses (saved) | missing | C41 |
| cvar.s_khz | `s_khz` | the mixing rate in kHz (default 44, saved) | missing | ours mixes at 48 kHz always (`engine/audio/voice.hpp`) (C32) |
| cvar.s_mixahead | `s_mixahead` | seconds the mixer runs ahead (default 0.1, saved) | missing | C32 |
| cvar.s_testsound | `s_testsound` | debug: a test tone (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.s_swapstereo | `s_swapstereo` | swap left and right (default 0) | missing | C32 |
| cvar.s_driver | `s_driver` | the sound driver | missing | not found |
| cvar.s_enable | `s_enable` | 0 none, 1 software, 2 OpenAL (default 1) | missing | C32 |
| cvar.s_volume | `s_volume` | the effects volume (default 0.7, saved) | done | synonym of `volume` (`client/cvar_layers.cpp`) |
| cvar.s_ambient | `s_ambient` | 0 no loops, 1 all, 2 only loops with a model, 3 only the player's own (default 1) | missing | C32 |
| cvar.s_show | `s_show` | debug: lists the sounds playing (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.s_auto_focus | `s_auto_focus` | 0 always play, 1 not minimised, 2 only focused (default 0) | missing | C32 |
| cvar.s_underwater | `s_underwater` | muffle sound under water (default 1) | done | `games/shooters_edge/client/room_ear.cpp` over the synth's muffle (M18k4 round 34) |
| cvar.s_underwater_gain_hf | `s_underwater_gain_hf` | the muffle's treble gain (default 0.25) | done | (M18k4 round 34) |
| cvar.ogg_enable | `ogg_enable` | play background music in any of the supported formats (`.flac`, `.opus`, `.ogg`, `.mp3`, `.wav`) (default 1) | missing | C29 |
| cvar.ogg_volume | `ogg_volume` | the music volume (default 1) | missing | ours `bgmvolume` (C29) |
| cvar.ogg_shuffle | `ogg_shuffle` | shuffle every track in `music/` (default 0) | missing | C29 |
| cvar.ogg_menu_track | `ogg_menu_track` | the track played while disconnected, 0 none (default 0) | missing | C29 |
| cvar.ogg_remap_tracks | `ogg_remap_tracks` | remap the mission packs' track numbers to the digital release's file names (default 1) | missing | C29 |
| cvar.al_device | `al_device` | the OpenAL output device, empty the default; `?` lists the devices ("Available devices:") (default empty) | missing | C32 |
| cvar.al_merge_looping | `al_merge_looping` | OpenAL: merge copies of one looped sound so they play as loud as on the software mixer; 0 off, 1 auto (only in plain stereo), 2 on (default 1) | missing | ours one mixer, which merges loops itself (`client/demo_sound.cpp`); no OpenAL (C32) |
| cvar.al_hrtf | `al_hrtf` | OpenAL head-related sound (default 0) | missing | C32 |
| cvar.gl_screenshot_format | `gl_screenshot_format` | jpg, png or tga (default jpg) | done | a synonym of ours `scr_format` (`games/shooters_edge/client/cvar_layers.cpp`), the same default |
| cvar.gl_screenshot_async | `gl_screenshot_async` | write screenshots off the main thread (default 1) | missing | C22 |
| cvar.gl_screenshot_quality | `gl_screenshot_quality` | JPEG quality (default 90) | done | a synonym of ours `scr_quality`, the same default |
| cvar.gl_screenshot_compression | `gl_screenshot_compression` | PNG compression (default 6) | missing | C22 |
| cvar.gl_screenshot_template | `gl_screenshot_template` | the file name, X's numbered (default `quakeXXX`) | missing | C22 |
| cvar.r_override_textures | `r_override_textures` | load replacement textures (default 1) | partial | ours replaces by classic enhance (`plugins/classicenhance`), not this switch |
| cvar.r_texture_overrides | `r_texture_overrides` | bit sum of which kinds may be replaced (default -1) | missing | not found |
| cvar.r_glowmaps | `r_glowmaps` | load glow maps (default 1) | partial | ours has glow maps from classic enhance; no switch by this name |
| cvar.gl_partscale | `gl_partscale` | particle size (default 2) | missing | C18 |
| cvar.gl_partstyle | `gl_partstyle` | 0 blended, 1 additive particles (default 0) | missing | C18 |
| cvar.gl_partshape | `gl_partshape` | 0 soft dot, 1 square, 2 sharp dot (default 0) | missing | C18 |
| cvar.gl_beamstyle | `gl_beamstyle` | 0 flat beams, 1 another look (default 0) | missing | C18 |
| cvar.gl_celshading | `gl_celshading` | outlines on models, width (default 0) | missing | C19 |
| cvar.gl_dotshading | `gl_dotshading` | models shaded by precomputed directions (default 1) | missing | C19 |
| cvar.gl_shadows | `gl_shadows` | 1 blob shadows under models, 2 fading with height (default 0) | missing | C19 |
| cvar.gl_modulate | `gl_modulate` | brightens lightmaps and models (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_modulate_world | `gl_modulate_world` | the world's share of `gl_modulate` (default 1) | missing | C21 |
| cvar.gl_coloredlightmaps | `gl_coloredlightmaps` | lightmap saturation (default 1) | missing | C21 |
| cvar.gl_brightness | `gl_brightness` | adds to lightmaps (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_dynamic | `gl_dynamic` | 0 no dynamic lights, 1 all, 2 only switchable styles (default 1) | done | synonym of `r_dynamic` (`client/cvar_layers.cpp`) |
| cvar.gl_draworder | `gl_draworder` | the alpha below which translucent entities are drawn after the rest, sorted back to front (default 1) | missing | ours sorts its own translucent passes (`engine/render`); not this switch (C21) |
| cvar.gl_clearcolor | `gl_clearcolor` | the colour behind the world where nothing is drawn (with `gl_clear`) (default black) | missing | not found (C21) |
| cvar.gl_dlight_falloff | `gl_dlight_falloff` | softer dynamic-light edges (default 1) | missing | not found |
| cvar.gl_modulate_entities | `gl_modulate_entities` | the models' share of `gl_modulate` (default 1) | missing | not found |
| cvar.gl_doublelight_entities | `gl_doublelight_entities` | models lit twice as bright, as the classic renderer did (default 1) | missing | not found |
| cvar.gl_glowmap_intensity | `gl_glowmap_intensity` | glow map strength (default 0.75) | missing | C21 |
| cvar.gl_flarespeed | `gl_flarespeed` | how fast flares fade in and out (default 8) | missing | not found |
| cvar.gl_fontshadow | `gl_fontshadow` | shadows under 2D text (default 0) | missing | not found |
| cvar.gl_shaders | `gl_shaders` | 1 the shader backend, 0 legacy (default 1) | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.gl_md5_load | `gl_md5_load` | load MD5 replacement models (default 1) | missing | not found |
| cvar.gl_md5_use | `gl_md5_use` | draw MD5 replacements (default 1) | missing | not found |
| cvar.gl_md5_distance | `gl_md5_distance` | beyond this distance the MD2 is drawn (default 2048) | missing | not found |
| cvar.gl_damageblend_frac | `gl_damageblend_frac` | the damage tint drawn as a frame this wide (extended protocol) (default 0.2) | missing | C21 |
| cvar.gl_waterwarp | `gl_waterwarp` | warp the view under water (default 0) | missing | C21 |
| cvar.gl_fog | `gl_fog` | draw the fog the game sets (default 1) | missing | C21 (ours has `fog`, the command) |
| cvar.gl_bloom | `gl_bloom` | bloom (default 0) | partial | ours `ce_bloom`/`r_bloom`; Quake II's spelling missing (C21) |
| cvar.gl_bloom_sigma | `gl_bloom_sigma` | bloom blur width (default 4) | missing | not found |
| cvar.gl_swapinterval | `gl_swapinterval` | vertical sync (default 1, saved) | partial | ours `vid_vsync` (`engine/platform/pacing.cpp`); no synonym (C21) |
| cvar.gl_drawsky | `gl_drawsky` | 0 skips the sky (default 1) | missing | not found |
| cvar.gl_clear | `gl_clear` | clear the screen each frame (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_novis | `gl_novis` | 0 uses visibility, 1 draws every leaf (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lockpvs | `gl_lockpvs` | keeps the last visibility; cheat-protected (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lightmap | `gl_lightmap` | draw lightmaps only; cheat-protected (default 0) | done | synonym of `r_lightmap` (`client/cvar_layers.cpp`) |
| cvar.r_fullbright | `r_fullbright` | no lighting; cheat-protected (default 0) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_vertexlight | `gl_vertexlight` | light the world by vertex (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lightgrid | `gl_lightgrid` | light models from the map's light grid (default 1) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_polyblend | `gl_polyblend` | draw the screen tints (default 1) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_texturemode | `gl_texturemode` | nearest, linear, bilinear, trilinear filtering (default trilinear, saved) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_anisotropy | `gl_anisotropy` | anisotropic filtering (default 1) | missing | C21 |
| cvar.gl_picmip | `gl_picmip` | texture size halvings (default 0) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_saturation | `gl_saturation` | texture saturation (default 1) | missing | C21 |
| cvar.intensity | `intensity` | texture brightness factor (default 2) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_invert | `gl_invert` | inverts texture colours (default 0) | missing | not found |
| cvar.vid_gamma | `vid_gamma` | gamma (default 1, saved) | done | synonym of `gamma` (`client/cvar_layers.cpp`) |
| cvar.gl_gamma_scale_pics | `gl_gamma_scale_pics` | apply gamma to 2D pictures (default 0) | missing | not found |
| cvar.gl_bilerp_chars | `gl_bilerp_chars` | smooth the font (default 0) | missing | not found |
| cvar.gl_bilerp_pics | `gl_bilerp_pics` | smooth 2D pictures (default 1) | missing | not found |
| cvar.gl_bilerp_skies | `gl_bilerp_skies` | smooth the sky (default 1) | missing | not found |
| cvar.gl_upscale_pcx | `gl_upscale_pcx` | upscale 8-bit pictures by hq2x/hq4x (default 0) | partial | ours upscales by classic enhance's trained upscaler; no switch by this name |
| cvar.hqx | `hqx_y`, `hqx_cb`, `hqx_cr` | the hq2x upscaler's colour thresholds (48, 7, 6) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_downsample_skins | `gl_downsample_skins` | halve big skins (default 1) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_cubemaps | `gl_cubemaps` | load cube-map skies (default 0) | missing | not found |
| cvar.gl_znear | `gl_znear` | the near clip plane; cheat-protected (default 2) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_drawworld | `gl_drawworld` | 0 skips the world; cheat-protected (default 1) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_drawentities | `gl_drawentities` | 0 skips entities; cheat-protected (default 1) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showtris | `gl_showtris` | debug: wireframe; cheat-protected | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showorigins | `gl_showorigins` | debug: entity origins; cheat-protected | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showtearing | `gl_showtearing` | debug: tearing test; cheat-protected | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showbloom | `gl_showbloom` | debug: bloom buffer; cheat-protected | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showstats | `gl_showstats` | debug: renderer counters | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_showscrap | `gl_showscrap` | debug: the scrap texture | not-taken | `client-gaps.md`: developer tools left out |
| cvar.gl_internals | `gl_nobind`, `gl_novbo`, `gl_test`, `gl_cull_nodes`, `gl_cull_models`, `gl_finish`, `gl_showerrors`, `gl_profile`, `gl_gpulerp`, `gl_texturebits`, `gl_noscrap`, `gl_round_down`, `gl_lightmap_bits`, `r_texture_formats`, `gl_debug_linewidth`, `gl_debug_distfrac` | the GL driver's switches and debug knobs | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_context | `gl_colorbits`, `gl_depthbits`, `gl_stencilbits`, `gl_debug` | the GL context's bits and debug level (0, 0, 8, 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_multisamples | `gl_multisamples` | MSAA samples (default 0; menu 2, 4, 8) | missing | not found (the menu row C5) |
| cvar.basedir | `basedir` | the install folder; command line only | partial | ours finds the install itself; `basedir` is a word of the take-in (`client/take_in.cpp`) |
| cvar.homedir | `homedir` | the writable folder; command line only | missing | not found |
| cvar.libdir | `libdir` | the game-library folder; command line only | missing | not found |
| cvar.sys_console | `sys_console` | Unix: the terminal console's mode | missing | not found |
| cvar.win_noalttab | `win_noalttab` | Windows: disable Alt+Tab (default 0) | missing | Windows only |
| cvar.win_disablewinkey | `win_disablewinkey` | Windows: disable the Windows key (default 0) | missing | Windows only |
| cvar.win_noresize | `win_noresize` | Windows: a window that cannot be resized (default 0) | missing | Windows only |
| cvar.win_notitle | `win_notitle` | Windows: no title bar (default 0) | missing | Windows only |
| cvar.win_alwaysontop | `win_alwaysontop` | Windows: keep the window on top (default 0) | missing | Windows only |
| cvar.win_noborder | `win_noborder` | Windows: no border (default 0) | missing | Windows only |
| cvar.sys_exitonerror | `sys_exitonerror` | Windows: exit at once on a fatal error (default 0) | missing | Windows only |
| cvar.sys_viewlog | `sys_viewlog` | Windows: show the system console window | missing | Windows only |
| cvar.sys_disablecrashdump | `sys_disablecrashdump` | Windows: no crash dump | missing | Windows only |
| cvar.gl_allow_software | `gl_allow_software` | Windows: allow the software GL driver (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.map_visibility_patch | `map_visibility_patch` | patch known maps' broken visibility (default 1) | done | synonym of `sv_vispatch` (`net/q2_game.cpp`) |
| cvar.map_noareas | `map_noareas` | debug: every area counts as joined (default 0) | missing | not found |
| cvar.map_override_path | `map_override_path` | a folder of entity overrides for maps (default empty) | done | synonym of `sv_entpath` (`net/q2_game.cpp`) |
| cvar.z_perturb | `z_perturb` | debug: fills freed memory (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.host_speeds | `host_speeds` | debug: prints frame timings (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.developer | `developer` | prints developer messages (default 0) | partial | ours has `developer` for the game library (`server/qvm_server.cpp`); verify the client's prints |
| cvar.timescale | `timescale` | game speed; cheat-protected (default 1) | missing | not found |
| cvar.fixedtime | `fixedtime` | fixed frame time; cheat-protected (default 0) | missing | not found |
| cvar.logfile | `logfile` | 1 writes the console to a log file, 2 flushed each line (default 0) | missing | ours logs by a command-line flag (`engine/play/session_flags.cpp`), not this setting |
| cvar.logfile_flush | `logfile_flush` | flush the log each line (default 0) | missing | not found |
| cvar.logfile_name | `logfile_name` | the log's name (default `console`) | missing | not found |
| cvar.logfile_prefix | `logfile_prefix` | each log line's time prefix (default `[%Y-%m-%d %H:%M] `) | missing | not found |
| cvar.console_prefix | `console_prefix` | a prefix on the system console's lines (default empty) | missing | not found |
| cvar.dedicated | `dedicated` | read-only: a dedicated server | done | `engine/server/spawn.cpp` |
| cvar.cl_running | `cl_running` | read-only: the client is running | missing | not found |
| cvar.cl_paused | `cl_paused` | read-only: the client is paused | missing | not found |
| cvar.sv_running | `sv_running` | read-only: a server is running | missing | not found |
| cvar.sv_paused | `sv_paused` | read-only: the server is paused | missing | not found |
| cvar.timedemo | `timedemo` | play a demo as fast as possible and print frames per second; cheat-protected (default 0) | done | `games/shooters_edge/client/demo_playback.cpp` |
| cvar.com_date_format | `com_date_format` | the `$com_date` format (default `%Y-%m-%d`) | done | `engine/console/clock_macros.cpp` |
| cvar.com_time_format | `com_time_format` | the `$com_time` format (default `%H.%M`) | done | `engine/console/clock_macros.cpp`: `%H:%M`, as on every system but Windows |
| cvar.com_debug_break | `com_debug_break` | debug: break into the debugger on an error | not-taken | `client-gaps.md`: developer tools left out |
| cvar.com_fatal_error | `com_fatal_error` | debug: errors are fatal | not-taken | `client-gaps.md`: developer tools left out |
| cvar.allow_download | `allow_download` | client: 0 fetch nothing, -1 refuse even `download` (default 1, saved) | missing | ours has the server's (`net/q2_game.cpp`); the client's missing (C36) |
| cvar.allow_download_players | `allow_download_players` | client: fetch player figures (default 1) | missing | the server's only (C36) |
| cvar.allow_download_models | `allow_download_models` | client: fetch models (default 1) | missing | the server's only (C36) |
| cvar.allow_download_sounds | `allow_download_sounds` | client: fetch sounds (default 1) | missing | the server's only (C36) |
| cvar.allow_download_maps | `allow_download_maps` | client: fetch maps (default 1) | missing | the server's only (C36) |
| cvar.allow_download_textures | `allow_download_textures` | client: fetch wall textures and skies (default 1) | missing | C36 |
| cvar.allow_download_pics | `allow_download_pics` | client: fetch pictures (default 1) | missing | C36 |
| cvar.allow_download_others | `allow_download_others` | fetch any other file (default 0) | missing | C36 |
| cvar.rcon_password | `rcon_password` | the password `rcon` sends | done | `games/shooters_edge/net/qw_admin.cpp` |
| cvar.sys_forcegamelib | `sys_forcegamelib` | a game library path forced; command line only | missing | not found |
| cvar.sys_allow_unsafe_savegames | `sys_allow_unsafe_savegames` | load saves of another build; command line only (default 0) | missing | not found |
| cvar.sys_history | `sys_history` | lines of the terminal's history kept | missing | not found |
| cvar.fs_autoexec | `fs_autoexec` | run `autoexec.cfg` on a game change (default 1) | missing | not found |
| cvar.fs_debug | `fs_debug` | debug: prints file opens | not-taken | `client-gaps.md`: developer tools left out |
| cvar.fs_fuzz | `fs_fuzz_factor`, `fs_fuzz_filter` | debug: fail file reads at random | not-taken | `client-gaps.md`: developer tools left out |
| cvar.game | `game` | the mod folder; latched, server info | partial | ours switches games by its own words (`client/game_folder.cpp`); verify `game` as a setting |
| cvar.com_completion_mode | `com_completion_mode` | how Tab completion lists matches (default 1) | missing | not found |
| cvar.com_completion_treshold | `com_completion_treshold` | matches above which Tab asks first (default 50) | missing | not found |
| cvar.showpackets | `showpackets` | prints every packet (default 0) | done | `plugins/classic/protocol/channel.hpp` (`client-gaps.md` round 21) |
| cvar.showdrop | `showdrop` | prints dropped packets (default 0) | done | `plugins/classic/protocol/channel.hpp` |
| cvar.qport | `qport` | the client's port inside the connection (random) | missing | C43 |
| cvar.net_maxmsglen | `net_maxmsglen` | the largest packet asked of the server (default 1390) | missing | not found |
| cvar.net_chantype | `net_chantype` | 1 the newer net channel on modern protocols (default 1) | missing | ours speaks 34 only |
| cvar.net_ip | `net_ip` | the local address to bind | partial | ours has it for the server (`engine/server/main.cpp`), not the client |
| cvar.net_ip6 | `net_ip6` | the local IPv6 address | missing | not found |
| cvar.net_port | `net_port` | the server's port (default 27910) | partial | ours has it for the server (`engine/server/main.cpp`) |
| cvar.net_clientport | `net_clientport` | the client's own port (default any) | missing | not found |
| cvar.net_dropsim | `net_dropsim` | debug: drop this share of packets (default 0) | partial | ours simulates networks in `engine/net` (labs), not by this setting |
| cvar.net_log | `net_log_enable`, `net_log_name`, `net_log_flush` | write a network log | missing | not found |
| cvar.net_enable_ipv6 | `net_enable_ipv6` | IPv6 networking | missing | not found |
| cvar.net_ignore_icmp | `net_ignore_icmp` | ignore ICMP errors (default 0) | missing | not found |

## Macros ($name in typed and bound text)

done 14, partial 1, missing 13, not-taken 2

A `$name` in a typed line, a bind, `say` or a `draw` is replaced by the macro's value.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| macro.loc_here | `$loc_here` | the name of the place the player stands in | done | `games/shooters_edge/client/place_names.cpp` |
| macro.loc_there | `$loc_there` | the name of the place the player looks at | done | `games/shooters_edge/client/place_names.cpp` |
| macro.cl_mapname | `$cl_mapname` | the map's file name | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_server | `$cl_server` | the server's address | done | `games/shooters_edge/net/qw_remote.cpp`: the server joined, as asked |
| macro.cl_timer | `$cl_timer` | time on this level, m:ss or h:mm:ss | missing | C44 |
| macro.cl_demopos | `$cl_demopos` | the playing demo's position, m:ss.t | missing | C44 |
| macro.cl_ups | `$cl_ups` | the player's speed along the ground, units a second (predicted when predicting) | done | `games/shooters_edge/client/place_seen.cpp` |
| macro.cl_fps | `$cl_fps` | moves (client frames) a second | missing | C44 |
| macro.r_fps | `$r_fps` | drawn frames a second | done | `engine/platform/pacing.cpp`: frames drawn in the last second |
| macro.cl_mps | `$cl_mps` | moves a second sent | missing | C44 |
| macro.cl_pps | `$cl_pps` | packets a second sent | missing | C44 |
| macro.cl_ping | `$cl_ping` | the round trip, ms | missing | C44 |
| macro.cl_lag | `$cl_lag` | packets lost, percent with 2 decimals | missing | C44 |
| macro.cl_health | `$cl_health` | the health stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_ammo | `$cl_ammo` | the ammo stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_armor | `$cl_armor` | the armour stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_weaponmodel | `$cl_weaponmodel` | the held weapon's model name | missing | C44 |
| macro.cl_numentities | `$cl_numentities` | entities in the frame | missing | C44 |
| macro.cl_surface | `$cl_surface` | the texture name and flags in the line of sight | missing | C44 |
| macro.gl_viewcluster | `$gl_viewcluster` | the view's visibility cluster number | not-taken | `client-gaps.md`: developer tools left out |
| macro.gl_viewleaf | `$gl_viewleaf` | the view's leaf number (a debug build's) | not-taken | `client-gaps.md`: developer tools left out |
| macro.com_date | `$com_date` | today's date by `com_date_format` | done | `engine/console/clock_macros.cpp` |
| macro.com_time | `$com_time` | the time by `com_time_format` | done | `engine/console/clock_macros.cpp` |
| macro.com_uptime | `$com_uptime` | how long the program has run | done | `engine/console/clock_macros.cpp` |
| macro.com_uptime_long | `$com_uptime_long` | the same in words | done | `engine/console/clock_macros.cpp` |
| macro.random | `$random` | a digit 0..9 | done | `engine/console/clock_macros.cpp` |
| macro.com_maplist | `$com_maplist` | every map's name (the start-server page lists them) | missing | C44 |
| macro.net_uprate | `$net_uprate` | bytes a second sent | missing | C44 |
| macro.net_dnrate | `$net_dnrate` | bytes a second received | missing | C44 |
| macro.cvars | `$<any setting>` | any setting's value expands the same way (`$$` defers it in menu lines) | partial | ours expands `$name` of settings in the console language (`engine/console/script.cpp`); verify in binds and `say` |

## Menu pages and rows

done 77, partial 72, missing 12, not-taken 1

The menu as upstream ships it: its menu script's pages and rows (`q2pro.menu`), and the three built-in pages (player setup, the demo browser, the server browser). Ours: Quake II runs on the classic menu's pages with Quake II's plates (`games/shooters_edge/menu/main_menu.cpp`, `q2_menu`); C5 to C7 describe the difference.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| menu.look.background | menu background | black behind every page; the in-game page tinted blue at about 1/8 | partial | ours draws the classic menu's backdrop with Quake II's plates (C5) |
| menu.look.font | menu font and cursor | the pack's `conchars` letters, `ch1` as the cursor | partial | ours uses Quake II's plates and our carved letters (`menu/plate_letters.cpp`); verify `ch1` cursor |
| menu.look.colours | row colours | normal and selected rows light blue, disabled grey | partial | ours the classic menu's colours (C5) |
| menu.look.weapon | player-setup weapon | the figure on the player page holds the railgun's world model | missing | no figure drawn (C5) |
| menu.esc.disconnected | ESCAPE while not in a game | opens the main page | done | `games/shooters_edge/menu/main_menu.cpp` |
| menu.esc.ingame | ESCAPE in a game | opens the compact in-game page (below) over the world | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.esc.layout | ESCAPE with a layout up | sends `putaway` (closes the help computer or inventory); held, opens the in-game page | missing | not found |
| menu.main.plaque | main: plaque and logo | the `m_main_plaque` and `m_main_logo` pictures beside the list | done | `menu/classic_art.cpp` (Quake II's plates) |
| menu.main.game | main: game | picture row opening the single-player page | done | ours SINGLE PLAYER |
| menu.main.multiplayer | main: multiplayer | picture row opening the multiplayer page | done | ours MULTIPLAYER |
| menu.main.options | main: options | picture row opening the options page | done | ours OPTIONS |
| menu.main.video | main: video | picture row opening the video page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.main.quit | main: quit | quits at once | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.sp.easy | single player: easy | `skill 0`, deathmatch 0, coop 0, maxclients 1, new game | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.sp.medium | single player: medium | the same with `skill 1` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.sp.hard | single player: hard | the same with `skill 2` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.sp.nightmare | single player: nightmare | the same with `skill 3` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.sp.load | single player: load game... | opens the load page | done | ours LOAD |
| menu.sp.save | single player: save game... | opens the save page | done | ours SAVE |
| menu.load.slots | load game | 16 slots: the autosave, a gap, then 15, each showing the save's name | partial | ours 12 slots, no autosave slot (`menu/main_menu.hpp` `kSaveSlots`) |
| menu.save.slots | save game | 15 slots (no autosave) | partial | ours 12 slots |
| menu.mp.q2servers | multiplayer: browse q2servers.com | the server browser filled from an HTTP list | missing | C41 (ours JOIN A GAME lists LAN, masters and our hub) |
| menu.mp.addressbook | multiplayer: browse address book | the browser over the address book, `servers.lst` and the LAN | partial | ours JOIN A GAME has the LAN; no address book or `servers.lst` (C41) |
| menu.mp.demos | multiplayer: browse demos | the demo browser | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.mp.startserver | multiplayer: start server | the start-server page | done | ours NEW GAME (host page) |
| menu.ss.map | start server: initial map | every map found (`$com_maplist`) | done | ours MAP |
| menu.ss.rules | start server: rules | deathmatch or cooperative | partial | ours GAME TYPE and TEAMPLAY; verify the choices |
| menu.ss.timelimit | start server: time limit | a number field, "0 - no time limit" | done | ours TIME LIMIT (a choice) |
| menu.ss.fraglimit | start server: frag limit | a number field, "0 - no frag limit" | done | ours FRAG LIMIT |
| menu.ss.maxplayers | start server: max players | a number field | done | ours MAX PLAYERS |
| menu.ss.hostname | start server: host name | a text field | missing | ours SERVER is where to host, not a name |
| menu.ss.dmflags | start server: deathmatch flags... | opens the flags page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.ss.begin | start server: begin game! | sets deathmatch unless coop, then starts the map | done | ours START |
| menu.dmf.falling | flags: falling damage | toggles dmflag 8 (inverted) | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.weaponsstay | flags: weapons stay | dmflag 4 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.instantpowerups | flags: instant powerups | dmflag 16 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.allowpowerups | flags: allow powerups | dmflag 2 (inverted) | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.allowhealth | flags: allow health | dmflag 1 (inverted) | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.allowarmor | flags: allow armor | dmflag 2048 (inverted) | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.spawnfarthest | flags: spawn farthest | dmflag 512 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.samemap | flags: same map | dmflag 32 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.forcerespawn | flags: force respawn | dmflag 1024 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.infiniteammo | flags: infinite ammo | dmflag 8192 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.fixedfov | flags: fixed FOV | dmflag 32768 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.dmf.quaddrop | flags: quad drop | dmflag 16384 | done | `games/shooters_edge/menu/q2_pages.cpp`: one bit of `dmflags` (`Entry::bit`) |
| menu.opt.player | options: player setup | opens the player page | partial | `games/shooters_edge/menu/q2_pages.cpp`: OPTIONS opens player setup with name and handedness; model, skin and the figure missing |
| menu.opt.input | options: input setup | opens the input page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.keys | options: key bindings | opens the key page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.weapons | options: weapon bindings | opens the weapon-key page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.video | options: video setup | opens the video page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.sound | options: sound setup | opens the sound page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.effects | options: effects setup | opens the effects page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.screen | options: screen setup | opens the screen page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.downloads | options: download options | opens the download page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.addressbook | options: address book | opens the address book | partial | not on OPTIONS: our menu has no text field for `adr0`..`adr15` |
| menu.pl.name | player: name | a text field for `name` | done | ours SETUP NAME |
| menu.pl.model | player: model | every figure in `players/` | missing | C5 |
| menu.pl.skin | player: skin | the chosen figure's skins | missing | C5 |
| menu.pl.hand | player: handedness | right, left, center (`hand`) | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.pl.figure | player: the figure drawn | the chosen figure and skin turning, holding the weapon | missing | C5 |
| menu.pl.none | player: no figures | "No player models found." when none | missing | not found |
| menu.in.sens | input: mouse sens | a number field for `sensitivity` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.in.autosens | input: auto sens | `m_autosens` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.in.filter | input: mouse filter | `m_filter` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.in.freelook | input: free look | `freelook` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.in.run | input: always run | `cl_run` | done | ours ALWAYS RUN on OPTIONS |
| menu.key.attack | keys: attack | `+attack` | done | `menu/controls_page.cpp` |
| menu.key.weapnext | keys: next weapon | `weapnext` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.weapprev | keys: previous weapon | `weapprev` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.forward | keys: walk forward | `+forward` | done | `menu/controls_page.cpp` |
| menu.key.back | keys: backpedal | `+back` | done | `menu/controls_page.cpp` |
| menu.key.run | keys: run | `+speed` | done | ours WALK / RUN |
| menu.key.left | keys: step left | `+moveleft` | done | `menu/controls_page.cpp` |
| menu.key.right | keys: step right | `+moveright` | done | `menu/controls_page.cpp` |
| menu.key.up | keys: up / jump | `+moveup` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.down | keys: down / crouch | `+movedown` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.inven | keys: inventory | `inven` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.invuse | keys: use item | `invuse` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.invdrop | keys: drop item | `invdrop` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.invprev | keys: prev item | `invprev` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.invnext | keys: next item | `invnext` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.help | keys: help computer | `help` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.pause | keys: pause game | `pause` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.score | keys: scoreboard | `score` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.key.chat | keys: chat | `messagemode` | done | ours CHAT |
| menu.key.legacy | keys: legacy keys... | opens the legacy page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.lk.turnleft | legacy: turn left | `+left` | done | ours TURN LEFT |
| menu.lk.turnright | legacy: turn right | `+right` | done | ours TURN RIGHT |
| menu.lk.sidestep | legacy: sidestep | `+strafe` | done | ours SIDESTEP |
| menu.lk.lookup | legacy: look up | `+lookup` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.lk.lookdown | legacy: look down | `+lookdown` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.lk.center | legacy: center view | `centerview` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.lk.mlook | legacy: mouse look | `+mlook` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.lk.klook | legacy: keyboard look | `+klook` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.wk.weapons | weapon keys: blaster, shotgun, super shotgun, machinegun, chaingun, grenade launcher, rocket launcher, hyperblaster, railgun, bfg10k, grenades | 11 rows binding `use <item>` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.vid.mode | video: video mode | windowed or a listed full-screen mode | partial | ours FULLSCREEN on or off |
| menu.vid.fov | video: fov scaling | `cl_adjustfov` vert- or hor+ | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.vsync | video: vertical sync | `gl_swapinterval` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.msaa | video: anti-aliasing | no, 2x, 4x, 8x MSAA | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.gamma | video: texture gamma | `vid_gamma` 1.3 to 0.3 | partial | ours BRIGHTNESS on OPTIONS (`gamma`) |
| menu.vid.hwgamma | video: hardware gamma | `vid_hwgamma` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.picmip | video: texture quality | `gl_picmip` 3 to 0 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.filter | video: texture filter | nearest, linear, bilinear, trilinear | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.aniso | video: anisotropic filter | no to 16x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.saturation | video: texture saturation | `gl_saturation` 0 to 1 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.intensity | video: texture intensity | `intensity` 1x, 2x, 3x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.lmsat | video: lightmap saturation | `gl_coloredlightmaps` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.lmbright | video: lightmap brightness | `gl_brightness` 0 to 0.3 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.backend | video: rendering backend | legacy or shaders | not-taken | `client-gaps.md`: the window's renderer is its own |
| menu.snd.engine | sound: sound engine | no sound, software, OpenAL | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.latency | sound: sound latency | `s_mixahead` 0.05 to 0.2 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.volume | sound: sound volume | `s_volume` 0 to 1 | partial | ours SOUND VOLUME on OPTIONS |
| menu.snd.music | sound: music volume | `ogg_volume` | partial | ours MUSIC VOLUME (`bgmvolume`) on OPTIONS |
| menu.snd.enable | sound: enable music | `ogg_enable` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.shuffle | sound: shuffle tracks | `ogg_shuffle` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.underwater | sound: underwater effect | `s_underwater` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.ambient | sound: ambient sounds | no, yes, only player's own (writes 2) | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.snd.chatbeep | sound: chat beep | disabled, default, alternative | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.dlight | effects: dynamic lighting | no, yes, only switchable | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.cel | effects: entity cel-shading | no, 1x, 2x, 3x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.glow | effects: entity glowing | `cl_noglow` inverted | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.shadows | effects: ground shadows | no, yes, fade | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.blend | effects: screen blending | `gl_polyblend` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.warp | effects: screen warping | `gl_waterwarp` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.grenade | effects: grenade explosions | `cl_disable_explosions` bit 0 inverted | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.rocket | effects: rocket explosions | `cl_disable_explosions` bit 1 inverted | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.rail | effects: railgun trail setup... | opens the rail page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.rail.type | rail: rail trail type | default, core only, core and spiral | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.time | rail: rail trail duration | 0.1 to 3 s | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.width | rail: core width | 1 to 6 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.radius | rail: spiral radius | 1 to 6 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.corecolor | rail: core color | black, red, green, yellow, blue, cyan, magenta, white | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.spiralcolor | rail: spiral color | the same eight | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.size | screen: screen size | `viewsize` 40 to 100 by 10 | partial | ours SCREEN SIZE on OPTIONS (the first game's meaning, C8) |
| menu.scr.pinggraph | screen: ping graph | `scr_lag_draw` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.demobar | screen: demo bar | no, yes, verbose | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.hudalpha | screen: HUD opacity | `scr_alpha` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.conalpha | screen: console opacity | `con_alpha` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.hudscale | screen: HUD scale | auto, 1x, 2x, 4x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.conscale | screen: console scale | auto, 1x, 2x, 4x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.menuscale | screen: menu scale | auto, 1x, 2x, 4x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.crosshair | screen: crosshair setup... | opens the crosshair page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.ch.type | crosshair: crosshair type | none, cross, dot, angle | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ch.scale | crosshair: crosshair scale | 0.5x, 1x, 2x | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ch.health | crosshair: color by health | `ch_health` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ch.rgba | crosshair: red, green, blue, alpha channel | four sliders 0 to 1 | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.allow | downloads: allow downloading | `allow_download` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.maps | downloads: maps | `allow_download_maps` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.players | downloads: player models/skins | `allow_download_players` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.models | downloads: models | `allow_download_models` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.sounds | downloads: sounds | `allow_download_sounds` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.textures | downloads: textures | `allow_download_textures` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.pics | downloads: pics | `allow_download_pics` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.http | downloads: HTTP downloads | `cl_http_downloads` | partial | `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ab.fields | address book | 16 text fields `adr0` to `adr15` | missing | C5 |
| menu.game.page | the in-game page | compact list: browse q2servers.com, browse address book, browse demos, single player, options, disconnect, quit | partial | `games/shooters_edge/menu/q2_pages.cpp`: the page and ESCAPE in a level; one browse row (ours lists LAN, masters and hub) for the two |
| menu.demos.list | demo browser | lists `demos/` (all folders with `ui_listalldemos`): `.dm2`, `.dm2.gz`, `.mvd2`, `.mvd2.gz` | partial | ours `menu/demos_page.cpp` (folders anywhere, newest first) (C40) |
| menu.demos.columns | demo browser columns | name, Date, Size, Map, POV; sorted by `ui_sortdemos` | missing | C40 |
| menu.servers.list | server browser | servers from an HTTP list, favourites, `servers.lst`, broadcast | partial | ours `menu/servers_page.cpp` (LAN, masters, hub) (C41) |
| menu.servers.columns | server browser columns | Hostname, Mod (the game folder, `baseq2` when empty), Map, Players, RTT; the selection's info as Key and Value, its players as Frg, RTT, Name | partial | ours its own columns (C41) |
| menu.servers.colours | server browser colours | with `ui_colorservers` 1: grey for a password, anticheat or no-fake server, green under 30 ms, white otherwise | missing | C41 |
| menu.script | the menu script file | `q2pro.menu`: pages and rows defined as text a player or mod can change | missing | ours pages are built in (`menu/main_menu.cpp`) |

## Server messages the client parses (svc)

done 13, partial 8, missing 6, not-taken 0

Every message a server (or a recording) sends the client, and what the client does with it. Ours reads Quake II's protocol 34 in `games/shooters_edge/client/demo_q2.cpp` for both a joined server and a recording; the protocols above 34 (35, 36) are not spoken (C42).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| svc.bad | `svc_bad` (0) | never valid; the connection drops with an "illegible server message" error naming the byte | partial | ours refuses with its own words (`client/demo_q2.cpp`) |
| svc.muzzleflash | `svc_muzzleflash` (1) | a player's weapon fired (or entered, left, respawned): a coloured light at the player for a moment and the weapon's sound; the silenced bit plays it at 0.2 volume | done | `client/demo_q2.cpp` reads every value (`q2_flash_fx`) |
| svc.muzzleflash2 | `svc_muzzleflash2` (2) | a monster fired: a light at the monster's gun (offset from the monster's flash table) and the weapon's sound | done | `client/demo_q2.cpp` reads every value (`q2_flash_fx`) |
| svc.temp_entity | `svc_temp_entity` (3) | a one-off effect (table "Temporary entities") | partial | read; drawn as a few generic kinds, some dropped (`client/demo_q2.cpp`) |
| svc.layout | `svc_layout` (4) | the layout program (scoreboard, help computer) shown while the layout bit is up | done | `games/shooters_edge/hud/q2_layout.cpp` |
| svc.inventory | `svc_inventory` (5) | every item's count, drawn by the inventory screen | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.nop | `svc_nop` (6) | nothing | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.disconnect | `svc_disconnect` (7) | the server dropped us: "Server disconnected" | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.reconnect | `svc_reconnect` (8) | the server changes level: "Server disconnected, reconnecting", then connects again at once | partial | read (`client/demo_q2.cpp`); verify the quick reconnect on a joined server |
| svc.sound | `svc_sound` (9) | a positioned sound: index, volume, attenuation, time offset, entity and channel, place | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.print | `svc_print` (10) | a line at a level (pickup, medium, high, chat); chat filtered, kept out of notify by `cl_chat_notify`, played with the chat sound, scanned for addresses (`followip`) and triggers | partial | printed; the chat sound plays; filters, triggers and `followip` missing (C31) |
| svc.stufftext | `svc_stufftext` (11) | console text the server runs on the client | partial | ours runs the signon's and keeps place names (`client/demo_q2.cpp`); verify other stuffed commands run; `cl_ignore_stufftext` missing (C33) |
| svc.serverdata | `svc_serverdata` (12) | the protocol, the server's count, attract-loop flag, game folder, player number, map title; prints the map title between rule lines | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.configstring | `svc_configstring` (13) | one configstring changed (table "Configstrings") | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.spawnbaseline | `svc_spawnbaseline` (14) | an entity's baseline for later deltas | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.centerprint | `svc_centerprint` (15) | a centre print; also printed in the console and run past triggers | partial | C9 (no fade, line cut differs) |
| svc.download | `svc_download` (16) | a piece of a requested file with the percent done | partial | ours fetches the map, models, sounds, figures; other kinds missing (C36) |
| svc.playerinfo | `svc_playerinfo` (17) | the player state, inside a frame | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.packetentities | `svc_packetentities` (18) | the frame's entities, inside a frame | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.deltapacketentities | `svc_deltapacketentities` (19) | the stock protocol's delta entities | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.frame | `svc_frame` (20) | a server frame: number, delta source, area bits, then the player state and entities | partial | read; the area bits are not used by the draw (C1) |
| svc.zpacket | `svc_zpacket` (21) | a deflate-compressed run of messages (protocols 35 and up) | missing | protocol 34 only (C42) |
| svc.zdownload | `svc_zdownload` (22) | a compressed download piece (35 and up) | missing | C42 |
| svc.gamestate | `svc_gamestate` (23) | every configstring and baseline in one go (36 and up) | missing | C42 |
| svc.setting | `svc_setting` (24) | a server setting (protocols 35 and up): whether player updates are sent, and the server's frame rate | missing | C42 |
| svc.configstringstream | `svc_configstringstream` (25) | configstrings in a stream (protocol 36's newer versions, and recordings of it) | missing | C42 |
| svc.baselinestream | `svc_baselinestream` (26) | baselines in a stream (protocol 36's newer versions) | missing | C42 |

## Temporary entities (svc_temp_entity)

done 12, partial 50, missing 3, not-taken 0

Each one-off effect a server sends, what the client draws and plays for it. The window's own game draws its own effects from its rules' events (`app/weapon_fx.hpp`, `app/monster_fx.hpp`, `client/world_fx.cpp`); the status here is for a joined server and a recording, where the original's effect must come from the message.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| te.gunshot | `TE_GUNSHOT` (0) | a bullet hit: dark particles and a small smoke-and-flash sprite, sometimes a ricochet sound | partial | read; drawn as our generic impact (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.blood | `TE_BLOOD` (1) | red blood particles along the direction | partial | read; drawn as our generic blood (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.blaster | `TE_BLASTER` (2) | a blaster bolt's hit: orange particles, a flash model lit for a moment, `weapons/lashit.wav` | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.railtrail | `TE_RAILTRAIL` (3) | the rail's spiral and core of particles (or a beam by `cl_railtrail_type`), `weapons/railgf1a.wav` at the end | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.shotgun | `TE_SHOTGUN` (4) | a pellet hit: fewer particles and a smoke puff, no ricochet | partial | read; drawn as our generic impact (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion1 | `TE_EXPLOSION1` (5) | a large explosion: the explosion model, orange light, `weapons/rocklx1a.wav` | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion2 | `TE_EXPLOSION2` (6) | a grenade-style explosion with its light and `weapons/grenlx1a.wav` | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.rocket_explosion | `TE_ROCKET_EXPLOSION` (7) | a rocket's blast: model, particles (off by `cl_disable_particles` 4), light, sound | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grenade_explosion | `TE_GRENADE_EXPLOSION` (8) | a grenade's blast: model, particles, light, sound | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.sparks | `TE_SPARKS` (9) | a few bright sparks | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.splash | `TE_SPLASH` (10) | a splash of count particles coloured by kind (sparks, water, mud, slime, lava, blood); sparks play `world/spark5..7.wav` | partial | drawn from the wire as the listen server draws it (`client/demo_q2.cpp`: `sq_splash` in its kind's colour); the sparks' sound not played; no test reads it off the wire |
| te.bubbletrail | `TE_BUBBLETRAIL` (11) | a line of bubbles between two points | done | bubbles every 32 units rising, read in `client/demo_q2.cpp`, drawn by `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.screen_sparks | `TE_SCREEN_SPARKS` (12) | green sparks where a power screen took a hit, `weapons/lashit.wav` | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.shield_sparks | `TE_SHIELD_SPARKS` (13) | the same for a power shield | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bullet_sparks | `TE_BULLET_SPARKS` (14) | a bullet hit with sparks and a ricochet | partial | read; drawn as our generic impact (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.laser_sparks | `TE_LASER_SPARKS` (15) | a laser's sparks: count, colour | partial | drawn from the wire (`sq_laser_sparks` in its colour, `client/demo_q2.cpp`); no test reads it off the wire |
| te.parasite_attack | `TE_PARASITE_ATTACK` (16) | the parasite's drain: a beam model from the monster to the target | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.rocket_explosion_water | `TE_ROCKET_EXPLOSION_WATER` (17) | a rocket's blast under water: the model and `weapons/xpld_wat.wav` | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grenade_explosion_water | `TE_GRENADE_EXPLOSION_WATER` (18) | a grenade's blast under water | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.medic_cable_attack | `TE_MEDIC_CABLE_ATTACK` (19) | the medic's healing cable beam | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_explosion | `TE_BFG_EXPLOSION` (20) | the BFG ball's burst sprite and green light | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_bigexplosion | `TE_BFG_BIGEXPLOSION` (21) | the BFG's big green particle burst | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bosstport | `TE_BOSSTPORT` (22) | the boss's teleport: a particle swirl and `misc/bigtele.wav` | partial | read; drawn as our generic teleport (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_laser | `TE_BFG_LASER` (23) | the BFG's green laser line for a frame | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grapple_cable | `TE_GRAPPLE_CABLE` (24) | the CTF grapple's cable from the player's hand | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.welding_sparks | `TE_WELDING_SPARKS` (25) | sparks with a flickering light (mission pack) | done | its count of specks in its colour and a flickering yellow-white light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.greenblood | `TE_GREENBLOOD` (26) | green blood (mission pack monsters) | partial | read; drawn as our generic blood (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bluehyperblaster | `TE_BLUEHYPERBLASTER` (27) | a blue hyperblaster's hit (mission pack) | done | 40 blue specks thrown along its way; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.plasma_explosion | `TE_PLASMA_EXPLOSION` (28) | the phalanx's plasma blast (mission pack) | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.tunnel_sparks | `TE_TUNNEL_SPARKS` (29) | a ring of sparks (mission pack) | done | its specks rising; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.blaster2 | `TE_BLASTER2` (30) | a green blaster's hit (mission pack) | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.railtrail2 | `TE_RAILTRAIL2` (31) | the newer game's lighter rail line | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flame | `TE_FLAME` (32) | never sent; both refuse it | done | `client/demo_q2.cpp` refuses it too |
| te.lightning | `TE_LIGHTNING` (33) | a lightning beam between two entities (mission pack) | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.debugtrail | `TE_DEBUGTRAIL` (34) | a debug line | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.plain_explosion | `TE_PLAIN_EXPLOSION` (35) | an explosion model with no particles | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flashlight | `TE_FLASHLIGHT` (36) | a light that follows an entity for a frame (mission pack) | done | a white light of 400 units for its frame; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.forcewall | `TE_FORCEWALL` (37) | a force wall: a line of coloured particles | done | a falling line of specks in its colour, every 4 units; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.heatbeam | `TE_HEATBEAM` (38) | the player's heat beam: a spiral beam from the gun | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.monster_heatbeam | `TE_MONSTER_HEATBEAM` (39) | a monster's heat beam | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.steam | `TE_STEAM` (40) | a puff or a lasting jet of steam with an id | partial | a puff draws its count along its jet (`client/world_fx_q2.cpp`, `app/effects.hpp`); a lasting jet (an id with a time) draws its first puff only |
| te.bubbletrail2 | `TE_BUBBLETRAIL2` (41) | a lighter bubble trail | done | bubbles every 8 units with the lash hit sound; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.moreblood | `TE_MOREBLOOD` (42) | a bigger burst of blood | partial | read; drawn as our generic blood (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.heatbeam_sparks | `TE_HEATBEAM_SPARKS` (43) | the heat beam's sparks | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.heatbeam_steam | `TE_HEATBEAM_STEAM` (44) | the heat beam's steam | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.chainfist_smoke | `TE_CHAINFIST_SMOKE` (45) | the chainfist's smoke | done | a puff of rising smoke; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.electric_sparks | `TE_ELECTRIC_SPARKS` (46) | blue electric sparks | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.tracker_explosion | `TE_TRACKER_EXPLOSION` (47) | the disruptor's dark burst and a dark light | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.teleport_effect | `TE_TELEPORT_EFFECT` (48) | a teleport burst | done | named `sq_teleport` (`client/demo_q2.cpp`) |
| te.dball_goal | `TE_DBALL_GOAL` (49) | the dodgeball goal's burst | done | the teleport burst; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.widowbeamout | `TE_WIDOWBEAMOUT` (50) | the widow's beam-out effect with an id | missing | read and dropped from a joined server or a recording (`client/demo_q2.cpp`); not drawn |
| te.nukeblast | `TE_NUKEBLAST` (51) | the nuke's expanding blast | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.widowsplash | `TE_WIDOWSPLASH` (52) | the widow's splash | done | 256 specks in a sphere round it; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.explosion1_big | `TE_EXPLOSION1_BIG` (53) | a bigger explosion model | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion1_np | `TE_EXPLOSION1_NP` (54) | an explosion with no particles | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flechette | `TE_FLECHETTE` (55) | a flechette's hit | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bluehyperblaster_2 | `TE_BLUEHYPERBLASTER_2` (56) | the newer game's blue bolt hit | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_zap | `TE_BFG_ZAP` (57) | the newer game's BFG zap line | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.berserk_slam | `TE_BERSERK_SLAM` (58) | the berserker's ground slam | partial | read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grapple_cable_2 | `TE_GRAPPLE_CABLE_2` (59) | the newer game's grapple cable | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.power_splash | `TE_POWER_SPLASH` (60) | a power armour's splash round an entity | missing | read and dropped from a joined server or a recording (`client/demo_q2.cpp`); not drawn |
| te.lightning_beam | `TE_LIGHTNING_BEAM` (61) | the newer game's lightning beam | partial | read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion1_nl | `TE_EXPLOSION1_NL` (62) | an explosion with no light | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion2_nl | `TE_EXPLOSION2_NL` (63) | a grenade explosion with no light | partial | read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.damage_dealt | `TE_DAMAGE_DEALT` (128) | damage this player dealt: the hit marker and, at `cl_hit_markers` 2 (the default), `weapons/marker.wav` | missing | read and dropped (C26) |

## Muzzle flashes (svc_muzzleflash, a player's)

done 18, partial 0, missing 2, not-taken 0

A player's gun fired (or the player entered, left, respawned). Every flash puts a light 18 units ahead of the player and 16 to the right, radius 200 to 231 (100 to 131 when silenced), lasting `cl_muzzlelight_time` (16 ms) unless the row says otherwise, plays the weapon's sound at full volume (0.2 when silenced) and, when the flash is the player's own, the newer game's flash model on the view weapon (`cl_muzzleflashes`). The rows give each flash's light colour, sound and model.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| mz.blaster | `MZ_BLASTER` (0) | yellow light, `weapons/blastf1a.wav`, the blaster flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.machinegun | `MZ_MACHINEGUN` (1) | yellow light, one of `weapons/machgf1b..5b.wav` at random, the machine-gun flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.shotgun | `MZ_SHOTGUN` (2) | yellow light, `weapons/shotgf1b.wav` and the reload `weapons/shotgr1b.wav` 0.1 s later, the shotgun flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.chaingun1 | `MZ_CHAINGUN1` (3) | red-orange light of radius 200 to 231, one machine-gun sound, the machine-gun flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.chaingun2 | `MZ_CHAINGUN2` (4) | orange light of radius 225 to 256, two machine-gun sounds 0.05 s apart, a larger flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.chaingun3 | `MZ_CHAINGUN3` (5) | yellow light of radius 250 to 281, three machine-gun sounds 0.033 s apart, the largest flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.railgun | `MZ_RAILGUN` (6) | pale blue light, `weapons/railgf1a.wav` and `weapons/railgr1b.wav` 0.4 s later, the railgun flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.rocket | `MZ_ROCKET` (7) | orange light, `weapons/rocklf1a.wav` and `weapons/rocklr1b.wav` 0.1 s later, the rocket flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.grenade | `MZ_GRENADE` (8) | orange light, `weapons/grenlf1a.wav` and `weapons/grenlr1b.wav` 0.1 s later, the launcher flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.login | `MZ_LOGIN` (9) | a player entered: green light, a green particle burst, `weapons/grenlf1a.wav` at full volume | done | the burst, its light and sound (`sq_login`, `client/demo_q2.cpp`, `client/world_fx.cpp`) |
| mz.logout | `MZ_LOGOUT` (10) | a player left: red light, red burst, the same sound | done | `sq_logout` (`client/demo_q2.cpp`, `client/world_fx.cpp`) |
| mz.respawn | `MZ_RESPAWN` (11) | a player respawned: yellow light, yellow burst, the same sound | done | `sq_respawn` (`client/demo_q2.cpp`, `client/world_fx.cpp`) |
| mz.bfg | `MZ_BFG` (12) | green light and `weapons/bfg__f1y.wav` (the shot's start; the flash model comes with `MZ_BFG2`) | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.sshotgun | `MZ_SSHOTGUN` (13) | yellow light, `weapons/sshotf1b.wav`, the super shotgun flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.hyperblaster | `MZ_HYPERBLASTER` (14) | yellow light, `weapons/hyprbf1a.wav`, the blaster flash model | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.itemrespawn | `MZ_ITEMRESPAWN` (15) | no case of its own: a light with no colour, so nothing is seen or heard | done | ours skips it (`client/demo_q2.cpp`); item respawns show from the item's event |
| mz.missionpack | `MZ_IONRIPPER` (16), `MZ_BLUEHYPERBLASTER` (17), `MZ_PHALANX` (18), `MZ_BFG2` (19), `MZ_PHALANX2` (20), `MZ_ETF_RIFLE` (30), `MZ_PROX` (31), `MZ_SHOTGUN2` (32), `MZ_HEATBEAM` (33), `MZ_BLASTER2` (34), `MZ_TRACKER` (35), `MZ_NUKE1`..`MZ_NUKE8` (36 to 39) | the mission packs' and the newer game's weapons: ripper pink light and `weapons/rippfire.wav`; blue hyperblaster blue light; phalanx pink light and `weapons/plasshot.wav` (its second barrel the model only); BFG2 green light and the BFG flash model; flechette amber light and `weapons/nail1.wav`; prox orange light, the launch and `weapons/proxlr1a.wav`; flash 32 the second flechette barrel on the extended protocol, else a yellow light and `weapons/shotg2.wav`; heat beam yellow light held 100 ms, no sound; blaster2 green light and the blaster sound; disruptor a darkening light and `weapons/disint2.wav`; the nukes red, yellow, blue, cyan light held 100 ms | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.silenced | silenced bit (128) | the shot's sound at 0.2 volume, its light radius 100 to 131 instead of 200 to 231 | done | `server/q2_flash.cpp` (the table), `client/q2_flash_fx.cpp` (sounds with their tails, the flash at the gun), read in `client/demo_q2.cpp` and on the listen server; `tests/shooters_edge_q2_flash_test.cpp` |
| mz.dlight_hack | `cl_dlight_hacks` 4 | removes the machine gun's and chain guns' muzzle light | missing | C16 |
| mz.muzzlefx | the muzzle-flash model | the newer game's flash model drawn at the muzzle of the player's own view weapon for a moment, its size and offset by weapon (`cl_muzzleflashes`); other players' flashes get no model | missing | ours draws its own view weapon's flash (`client/main.cpp`); the wire's flashes skipped |

## Muzzle flashes (svc_muzzleflash2, a monster's)

done 32, partial 1, missing 0, not-taken 0

A monster fired: the client places a light at the gun (the monster's per-flash offset turned by its angles and scaled by its size), radius 200 to 231 for `cl_muzzlelight_time` (16 ms), plays the shot's sound and, for guns, a few particles and a smoke-and-flash sprite; the newer muzzle-flash model is drawn at the gun. One row per monster and weapon (the flash numbers are one per gun barrel or animation frame).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| mz2.infantry_mg | infantry machine gun (22 flashes) | yellow light, particles and smoke, `infantry/infatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.soldier_mg | soldier machine gun (9) | yellow light, particles, smoke, `soldier/solatck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.gunner_mg | gunner machine gun (8) | yellow light, particles, smoke, `gunner/gunatck2.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.actor_super_turret_mg | actor, supertank and turret machine guns (8) | yellow light, particles, smoke, `infantry/infatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.boss2_carrier_mg_l | hornet and carrier left guns (7) | yellow light, particles, smoke, `infantry/infatck1.wav`; on the extended protocol the hornet's second left gun plays `flyer/flyatck3.wav` with a small blaster flash model instead | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.boss2_carrier_mg_r | hornet and carrier right guns (7) | yellow light, particles, smoke, no sound (the left gun's plays); on the extended protocol the hornet's second right gun plays `flyer/flyatck3.wav` with a small blaster flash model | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.soldier_blaster | soldier and turret blasters (10) | yellow light, `soldier/solatck2.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.flyer_blaster | flyer blasters (2) | yellow light, `flyer/flyatck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.medic_blaster | medic blaster and hyperblaster (13 + 12) | yellow light, `medic/medatck1.wav` (the second hyperblaster set green) | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.hover_blaster | icarus blasters (2) | yellow light, `hover/hovatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.float_blaster | technician blaster (1) | yellow light, `floater/fltatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.soldier_shotgun | soldier shotgun (9) | yellow light, smoke, `soldier/solatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.tank_blaster | tank blasters (3) | yellow light, `tank/tnkatck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.tank_mg | tank machine gun (19) | yellow light, particles, smoke, one of the tank's gun sounds at random | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.chick_rocket | iron maiden and turret rockets (2) | orange light, `chick/chkatck2.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.tank_rocket | tank rockets (3) | orange light, `tank/tnkatck1.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.boss_rockets | hornet, carrier and supertank rockets (11) | orange light, `tank/rocket.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.grenades | gunner and supertank grenades (10) | orange light, `gunner/gunatck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.rails | gladiator, makron, arachnid, carrier and widow railguns (8) | blue light, no sound | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.makron_bfg | makron BFG (1) | green light (the sound commented out) | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.makron_blaster | makron blasters (17) | yellow light, `makron/blaster.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.jorg_mg_l | jorg left guns (6) | yellow light, particles, smoke, `boss3/xfire.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.jorg_mg_r | jorg right guns (6) | yellow light, particles, smoke, no sound | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.jorg_bfg | jorg BFG (1) | green light | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.green_blasters | daedalus, stalker, medic commander, widow blasters and sweeps (mission pack, 52) | green light, `tank/tnkatck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.widow_disruptor | widow disruptor (1) | a dark (negative) light, `weapons/disint2.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.widow_beams | widow plasma beam and beam sweeps (17) | yellow light, no sound | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.soldier_ripper | soldier ion ripper (9) | pink light, `weapons/rippfire.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.soldier_hypergun | soldier hyperblaster (9) | blue light, `weapons/hyprbf1a.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.guardian_blaster | guardian blaster (1) | yellow light, `weapons/hyprbf1a.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.guncmdr_chaingun | gunner commander chain gun (2) | blue light, `guncmdr/gcdratck2.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.guncmdr_grenades | gunner commander grenades (9) | orange light, `guncmdr/gcdratck3.wav` | done | `server/q2_flash.cpp` by value (sound, gun place rounded), `client/q2_flash_fx.cpp`; our monsters send it (`q2_monsters.cpp`); `tests/shooters_edge_q2_flash_test.cpp` |
| mz2.muzzlefx | the muzzle-flash model | the newer game's flash model drawn at the gun for a moment, scaled by the monster | partial | the window's own game draws its monsters' shots by kind (blaster, gun, rocket, rail, BFG: `app/monster_fx.hpp`, `server/q2_monsters.cpp`); from a joined server or a recording the message is skipped (`client/demo_q2.cpp`) |

## Entity effect bits (EF_)

done 27, partial 1, missing 5, not-taken 0

Bits of an entity's effects field a server sends; the client draws trails, lights, shells and frame cycles from them. Ours: `games/shooters_edge/client/q2_look.hpp` reads them for Quake II's entities.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ef.rotate | `EF_ROTATE` (1) | turns the model about its yaw on the client's clock (items) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.gib | `EF_GIB` (2) | a blood trail behind it (green with `EF_GREENGIB`) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.bob | `EF_BOB` (4) | the newer game's item bob | missing | not read |
| ef.blaster | `EF_BLASTER` (8) | a blaster bolt: yellow light and a particle trail (green with `EF_TRACKER`; the trail off by `cl_disable_particles` 32) | done | `games/shooters_edge/client/q2_look.hpp`; the 32 switch is a row of the settings table |
| ef.rocket | `EF_ROCKET` (16) | a rocket: orange light and the smoke-and-fire trail (`cl_disable_particles` 8) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.grenade | `EF_GRENADE` (32) | a grenade: smoke trail (`cl_disable_particles` 2) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.hyperblaster | `EF_HYPERBLASTER` (64) | a hyperblaster bolt: yellow light | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.bfg | `EF_BFG` (128) | the BFG ball: green light and particles, drawn at 0.3 | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.color_shell | `EF_COLOR_SHELL` (256) | a colour shell by the render bits | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.powerscreen | `EF_POWERSCREEN` (512) | the power screen's plate drawn as a green shell before the bearer | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.anim01 | `EF_ANIM01` (1024) | cycles frames 0 and 1 at 2 a second | done | `games/shooters_edge/client/q2_look.cpp` (`q2_texture_frame`), `client/world_models.cpp` (brush textures, models and sprites), `client/demo_video.cpp` |
| ef.anim23 | `EF_ANIM23` (2048) | cycles frames 2 and 3 at 2 a second | done | `games/shooters_edge/client/q2_look.cpp` (`q2_texture_frame`), `client/world_models.cpp` (brush textures, models and sprites), `client/demo_video.cpp` |
| ef.anim_all | `EF_ANIM_ALL` (4096) | cycles every frame at 2 a second | done | `games/shooters_edge/client/q2_look.cpp` (`q2_texture_frame`), `client/world_models.cpp` (brush textures, models and sprites), `client/demo_video.cpp` |
| ef.anim_allfast | `EF_ANIM_ALLFAST` (8192) | cycles every frame at 10 a second | done | `games/shooters_edge/client/q2_look.cpp` (`q2_texture_frame`), `client/world_models.cpp` (brush textures, models and sprites), `client/demo_video.cpp` |
| ef.flies | `EF_FLIES` (16384) | flies swarm round it (a corpse) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.quad | `EF_QUAD` (32768) | blue shell and blue light | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.pent | `EF_PENT` (65536) | red shell and red light | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.teleporter | `EF_TELEPORTER` (131072) | a teleporter pad's particle fountain | done | `games/shooters_edge/client/demo_q2.cpp` |
| ef.flag1 | `EF_FLAG1` (262144) | CTF red flag: red light and a red trail | done | a red column of specks and a red light (`client/q2_look.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp`) |
| ef.flag2 | `EF_FLAG2` (524288) | CTF blue flag: blue light and a blue trail | done | a blue column of specks and a blue light (`client/q2_look.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp`) |
| ef.ionripper | `EF_IONRIPPER` (1<<20) | ion ripper bolt: red light and trail (mission pack) | done | red specks shaken across and a pink light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.greengib | `EF_GREENGIB` (1<<21) | green blood trail | done | a green blood trail when no earlier branch takes it (`client/q2_look.cpp`) |
| ef.bluehyperblaster | `EF_BLUEHYPERBLASTER` (1<<22) | blue light (mission pack) | done | a blue light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.spinninglights | `EF_SPINNINGLIGHTS` (1<<23) | a spinning red light at the front (mission pack) | missing | not found |
| ef.plasma | `EF_PLASMA` (1<<24) | plasma bolt: orange light (mission pack) | done | a pink light, drawn at 0.6 (`client/q2_look.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp`) |
| ef.trap | `EF_TRAP` (1<<25) | the trap's light and particles (mission pack) | missing | not found |
| ef.tracker | `EF_TRACKER` (1<<26) | disruptor bolt: a dark light and the tracker trail (mission pack) | missing | not found |
| ef.double | `EF_DOUBLE` (1<<27) | double damage: a yellow shell | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.spheretrans | `EF_SPHERETRANS` (1<<28) | a see-through sphere (mission pack) | done | drawn at 0.3, 0.6 with the tracker trail (`client/q2_look.cpp`) |
| ef.tagtrail | `EF_TAGTRAIL` (1<<29) | the tag token's trail (mission pack) | done | a yellow column of specks and a yellow light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.half_damage | `EF_HALF_DAMAGE` (1<<30) | half damage: a shell | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.trackertrail | `EF_TRACKERTRAIL` (1<<31) | a tracker's dark trail and pulsing light (mission pack) | missing | not found |
| ef.lights | the lights per bit | each bit's light radius and colour, added to the view's dynamic lights (`cl_lights`) | partial | `q2_look.hpp` lights the missiles; verify radius and colour per bit against `q2-client-effects.md` § 6 |

## Entity render bits (RF_) and view bits (RDF_)

done 8, partial 5, missing 19, not-taken 0

Bits of an entity's render field, and of the player state's view flags.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| rf.minlight | `RF_MINLIGHT` (1) | always some light (the view weapon) | missing | not read from the wire |
| rf.viewermodel | `RF_VIEWERMODEL` (2) | not drawn from the player's own eyes | partial | ours hides the viewer's own body by its own rule; the bit not read |
| rf.weaponmodel | `RF_WEAPONMODEL` (4) | drawn only from the eyes | partial | ours draws its own view weapon; the bit not read |
| rf.fullbright | `RF_FULLBRIGHT` (8) | drawn at full brightness | missing | not read |
| rf.depthhack | `RF_DEPTHHACK` (16) | depth squeezed so it never enters walls (the view weapon) | partial | ours draws the view weapon on top by its own pass |
| rf.translucent | `RF_TRANSLUCENT` (32) | drawn at 0.7 | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.framelerp | `RF_FRAMELERP` (64) | blend between the old and new frame | partial | ours interpolates frames always; verify |
| rf.beam | `RF_BEAM` (128) | a laser beam of the frame's width in the skin's colour, at 0.3 | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.customskin | `RF_CUSTOMSKIN` (256) | the skin is a picture index | missing | not read |
| rf.glow | `RF_GLOW` (512) | the item's light pulses | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_red | `RF_SHELL_RED` (1024) | a red shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_green | `RF_SHELL_GREEN` (2048) | a green shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_blue | `RF_SHELL_BLUE` (4096) | a blue shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.noshadow | `RF_NOSHADOW` (8192) | casts no blob shadow | missing | goes with `gl_shadows` (C19) |
| rf.castshadow | `RF_CASTSHADOW` (16384) | the newer game's shadow caster | missing | not read |
| rf.ir_visible | `RF_IR_VISIBLE` (32768) | drawn flat red while infra-red goggles are on | missing | C20 |
| rf.shell_double | `RF_SHELL_DOUBLE` (65536) | double damage's shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_half | `RF_SHELL_HALF_DAM` (131072) | half damage's shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.use_disguise | `RF_USE_DISGUISE` (1<<18) | drawn with the disguise skin (mission pack) | missing | not read |
| rf.shell_lite_green | `RF_SHELL_LITE_GREEN` (1<<19) | a light green shell | missing | not read |
| rf.custom_light | `RF_CUSTOM_LIGHT` (1<<20) | a light of the entity's own colour | missing | not read |
| rf.flare | `RF_FLARE` (1<<21) | a flare sprite that fades by sight (`cl_flares`, `gl_flarespeed`) | missing | C16 |
| rf.old_frame_lerp | `RF_OLD_FRAME_LERP` (1<<22) | the old frame is sent, not guessed | missing | not read |
| rf.dot_shadow | `RF_DOT_SHADOW` (1<<23) | a dot shadow | missing | not read |
| rf.low_priority | `RF_LOW_PRIORITY` (1<<24) | dropped first when there are too many | missing | not read |
| rf.no_lod | `RF_NO_LOD` (1<<25) | no level-of-detail model | missing | not read |
| rf.stair_step | `RF_STAIR_STEP` (1<<26) | the stair step is smoothed for this entity | missing | not read |
| rdf.underwater | `RDF_UNDERWATER` (1) | the view is under water: the warp (`gl_waterwarp`) and the sound muffle (`s_underwater`) | partial | ours tints the view under water; warp and muffle missing (C21, C28) |
| rdf.noworldmodel | `RDF_NOWORLDMODEL` (2) | a view with no world (the player-setup figure) | missing | no player figure page (C5) |
| rdf.irgoggles | `RDF_IRGOGGLES` (4) | infra-red goggles on | missing | C20 |
| rdf.uvgoggles | `RDF_UVGOGGLES` (8) | the other goggles bit | missing | not read |
| rdf.teleport | `RDF_TELEPORT_BIT` (16) | the extended protocol's teleport mark: no view interpolation this frame | missing | protocol 34 only (C42) |

## Configstrings

done 7, partial 5, missing 4, not-taken 0

The indexed strings a server sets (stock protocol 34 indices; the extended protocols move every range further on). Ours keeps them in `games/shooters_edge/client/demo_q2.cpp` (`set_string`) for a joined server and a recording; both layouts are read.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cs.name | `CS_NAME` (0) | the level's title: printed with the server data, shown on the loading screen | partial | ours takes the title from the server data; index 0 not kept |
| cs.cdtrack | `CS_CDTRACK` (1) | the music: empty or 0 stops, N plays `music/trackNN` (or `NN`), the mission packs' numbers remapped by `ogg_remap_tracks`, text plays `music/<text>` | missing | not kept (C29) |
| cs.sky | `CS_SKY` (2) | the sky box's name; changing it changes the sky | partial | ours reads the sky from the map's world entity (`client/demo_content.cpp`); index 2 not kept |
| cs.skyaxis | `CS_SKYAXIS` (3) | the sky's turning axis | missing | not kept |
| cs.skyrotate | `CS_SKYROTATE` (4) | the sky's turning speed, degrees a second | missing | not kept |
| cs.statusbar | `CS_STATUSBAR` (5) | the status bar's layout program (strings 5 to 28 joined) | done | `games/shooters_edge/client/demo_q2.cpp`, `client/q2_bar.cpp` |
| cs.airaccel | `CS_AIRACCEL` (29) | the server's air acceleration, used by prediction | done | `games/shooters_edge/client/demo_q2.cpp` (`air_accelerate`) |
| cs.maxclients | `CS_MAXCLIENTS` (30) | the player slots; above 1 also turns the cheat settings off | partial | kept (`demo_q2.cpp`); the cheat lock not found |
| cs.mapchecksum | `CS_MAPCHECKSUM` (31) | the map's checksum: a mismatch drops the connection ("Local map version differs from server: %i != %s"); in a recording only a warning | missing | C36 |
| cs.models | `CS_MODELS` (32..) | the models by index; `maps/<map>.bsp` first, `*n` brush models | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.sounds | `CS_SOUNDS` (288..) | the sounds by index; `*name` a figure's own sound | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.images | `CS_IMAGES` (544..) | the pictures by index (`pics/<name>.pcx`) for the layouts | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.lights | `CS_LIGHTS` (800..) | the light styles as letter strings, animated at 10 a second; the game switches them | partial | not kept from a server or a recording (`demo_q2.cpp`); the window's own game animates its own styles |
| cs.items | `CS_ITEMS` (1056..) | the item names by index, for the inventory | done | `games/shooters_edge/client/demo_q2.cpp` (`item_names`) |
| cs.playerskins | `CS_PLAYERSKINS` (1312..) | each slot's `name\model/skin`, with the visible weapons | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.general | `CS_GENERAL` (1568..) | free strings a layout prints (`stat_string`) | partial | not kept (`demo_q2.cpp` drops the range); verify `stat_string` on a mod that uses it |

## The layout language (status bar and layouts)

done 20, partial 2, missing 5, not-taken 0

The words of the status-bar and layout programs the client runs every frame on a virtual 320-wide screen anchored to the window. Ours: `games/shooters_edge/hud/q2_layout.cpp`.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| lay.xl | `xl <n>` | x from the left edge | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.xr | `xr <n>` | x from the right edge | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.xv | `xv <n>` | x from the centred 320-wide box | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.yt | `yt <n>` | y from the top | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.yb | `yb <n>` | y from the bottom | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.yv | `yv <n>` | y from the centred 240-high box | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.pic | `pic <stat>` | the picture whose index the stat holds | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.picn | `picn <name>` | a picture by name | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.num | `num <width> <stat>` | a stat as big digits | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.hnum | `hnum` | health as big digits, red under 25, flashing with the stat-flashes bit | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.anum | `anum` | ammo as big digits, red under 5, flashing | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.rnum | `rnum` | armour as big digits, flashing | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.client | `client <x> <y> <slot> <score> <ping> <time>` | a deathmatch score block: the slot's icon, name, score, ping, time | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.ctf | `ctf <x> <y> <slot> <score> <ping>` | a CTF score line | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.string | `string <text>` | text in the plain colour | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.string2 | `string2 <text>` | text in the alternate colour | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.cstring | `cstring <text>` | text centred | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.cstring2 | `cstring2 <text>` | text centred, alternate colour | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.rstring | `rstring <text>` | text right-aligned | missing | not in `q2_layout.cpp` |
| lay.rstring2 | `rstring2 <text>` | text right-aligned, alternate colour | missing | not in `q2_layout.cpp` |
| lay.stat | `stat_string`, `stat_string2`, `stat_cstring`, `stat_cstring2`, `stat_rstring`, `stat_rstring2` `<stat>` | the configstring whose index a stat holds, in each of the six text forms | partial | ours `stat_string` only (`q2_layout.cpp`) |
| lay.if | `if <stat>` ... `endif` | runs the words between only when the stat is not 0 | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.color | `color <colour>` | sets the colour of the text that follows | missing | not in `q2_layout.cpp` |
| lay.health_bars | `health_bars <stat> <string>` | a boss's name centred and two health bars from the stat's two bytes (the newer game's) | missing | not in `q2_layout.cpp` |
| lay.flash | stat-flashes field | the field picture behind a number while its flash bit is set | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.scope | scope pictures | a `scope?x` picture placed at the centred 320 by 240 box is drawn stretched to 320 by 240 times `ch_scale`, centred and moved by `ch_x`/`ch_y` | missing | not in `q2_layout.cpp` |
| lay.errors | a bad index in a layout | drops the connection: "invalid stat index for pic", "invalid client index" and the like | partial | ours refuses or skips by its own words; verify |

## HUD and screen elements

done 11, partial 10, missing 7, not-taken 1

What the client draws in 2D over the view, in its drawing order, and the settings that govern each (the settings themselves are rows of the settings table).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| hud.scale | the 2D scale | every 2D element drawn on a virtual screen: the window divided by `scr_scale` (auto 1, 2 or 4 by height) | done | `q2_bar_scale` by the window, `scr_scale` (M18k4 round 34); the desktop's DPI floor not read |
| hud.draw2d | hiding the 2D | `scr_draw2d` 0 none, 1 no status bar, 3 layouts only while F1 is held | partial | `scr_draw2d` (M18k4 round 34); F1 held at 3 not read |
| hud.crosshair | the crosshair picture | `chN` at the centre plus `ch_x`/`ch_y`, scaled by `ch_scale`, its colour and alpha its own, hidden by the status flags' hide-crosshair bit (extended protocol) | partial | `engine/hud/crosshair.hpp`; the hide bit missing (C27) |
| hud.hitmarker | the hit marker | the `marker` picture, red, fading over `scr_hit_marker_time`; on by default | missing | C26 |
| hud.graphs | net, time and debug graphs | bars along the bottom (`netgraph`, `timegraph`, `debuggraph`) | missing | C13 |
| hud.statusbar | the status bar | the `CS_STATUSBAR` program run each frame, unless the hide-HUD layout bit | done | `games/shooters_edge/client/q2_bar.cpp`, `hud/q2_layout.cpp` |
| hud.layout | the layout | the server's layout (scoreboard, help computer) while the layout bit is up; in a demo, F1 shows it whenever held | partial | `hud/q2_layout.cpp`; the F1 hold in demos missing (C15) |
| hud.inventory | the inventory screen | the item list with counts, the selected one marked, while the inventory bit is up | done | `games/shooters_edge/server/q2_screens.cpp` (the rules' screen) and `client/q2_bar.cpp` |
| hud.centerprint | the centre print | centred on a quarter of the height, `scr_centertime`, faded over 300 ms, a queue of 4 on the extended protocol, typewriter prints at `scr_printspeed` | partial | ours the stock client's cut and place, no fade (C9) |
| hud.net | the lost-connection picture | the `net` picture blinking in the lag graph's box when 128 moves are unanswered | done | `client/q2_bar.cpp` (M18k4 round 34) |
| hud.laggraph | the lag graph | 48 columns of round trips, lost and held frames coloured (`scr_lag_*`) | partial | `engine/hud/net_graph.cpp` (M18k4 round 34); a frame held back for the rate not told apart |
| hud.draw | drawn values | `draw` objects: a setting or macro at a place, every frame | missing | C14 |
| hud.chathud | chat lines on screen | the last `scr_chathud_lines` chat lines (`scr_chathud*`) | done | `games/shooters_edge/hud/layout.cpp` chat feed (M18k4 round 32) |
| hud.turtle | network trouble words | CLIENTPRED, BADFRAME, OLDFRAME, OLDENT, NODELTA (and 3 more at 2) | missing | C12 |
| hud.pause | the pause picture | the pack's `pause` centred while paused (`scr_showpause`) | done | `games/shooters_edge/client/scores.cpp` (the plaque) |
| hud.loading | the loading plaque | the pack's `loading` centred while a level loads, unless the console or a menu is up; the sounds stop; gives up after 120 s ("Loading plaque timed out.") | partial | the plaque is drawn; the stop of the music and the timeout missing (C24) |
| hud.demobar | the demo bar | the share of the file read as a bar and a percentage, with `scr_demobar 2` the time; "[PAUSED]" | partial | ours `demo_bar` and the timeline (C39) |
| hud.notify | the notify lines | the last `con_notifylines` printed lines at the top for `con_notifytime` | done | `engine/console/view.cpp` |
| hud.chatinput | the chat input line | "say:" or "say_team:" and the typed text at the top | done | `engine/console/talk.cpp` |
| hud.blend | the screen tint | the player state's blend (damage red, pickup yellow, water), off by `cl_blend`/`gl_polyblend`; the extended protocol's damage tint as a frame `gl_damageblend_frac` wide | partial | the tint landed (`q2-gaps2-client.md` checked); the damage frame missing (C21) |
| hud.viewsize | the screen-size border | a view under 100 % centred with the `backtile` picture round it | missing | C8 |
| hud.locdraw | place markers | `loc_draw` draws each place point in the world | missing | C45 |
| hud.cinematic | cinematics | a `.cin` (or `.ogv`, `.mkv`, `.mp4`) full-screen with its sound, the known releases' letterbox cropped; any key skips | partial | `.cin` from the packs (`client/cinematic.cpp`); other formats and the crop missing (C25) |
| hud.endpic | the end pictures | a cinematic name ending `.pcx` shows a still picture until a key | done | `games/shooters_edge/client/cinematic.cpp` (`q2-gaps2-client.md` checked) |
| hud.console | the console | slides down to `con_height` at `scr_conspeed` over `conback`, `con_alpha`, `con_scale`; the program's version at the lower right; a yellow arrow prompt (`#` in `remotemode`) | partial | the look landed; `con_alpha`, `con_scale`, the remote prompt missing (C23) |
| hud.consoleload | the console's loading lines | while connecting: the file downloading with a bar and "nn% (size)", or "Loading <map>...", models, images, clients, sounds in turn | missing | C23 |
| hud.conclock | the console's clock | `con_clock` | done | `engine/ui/console.cpp` (M18k4 round 32) |
| hud.alpha | 2D opacity | `scr_alpha` for every 2D element but the crosshair | done | `client/q2_bar.cpp` (M18k4 round 34) |
| hud.debugstats | debug overlays | `scr_showstats` (the stats as numbers), `scr_showpmove` (the move state) | not-taken | `client-gaps.md`: developer tools left out |

## Messages printed to the player

done 32, partial 56, missing 54, not-taken 5

Lines the client prints in the console (and the notify lines), quoted as the reference prints them (`%s`, `%d` stand for the value; `\n` line ends dropped). A row is done when our tree prints the same words. The project's rule that a player's lines read like a game's words (CLAUDE.md, Players are people) may make some differences deliberate; none is recorded as a decision, so they are partial. Internal protocol faults are one row at the end.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| msg.connect.challenge | "Requesting challenge... %i" | each try to get a challenge while connecting | partial | ours does it but says it in its own words (the exact line not found) |
| msg.connect.request | "Requesting connection... %i" | each try to connect | partial | ours does it but says it in its own words (the exact line not found) |
| msg.connect.usage | "Usage: %s <server>" | `connect` with no address | partial | ours does it but says it in its own words (the exact line not found) |
| msg.connect.protoarg | "Second argument to `%s' is now ignored. Set protocol via `cl_protocol' variable." | a second `connect` argument | missing | the feature is missing |
| msg.connect.badaddr | "Bad server address" | an address that does not resolve | partial | ours does it but says it in its own words (the exact line not found) |
| msg.connect.connected | "Connected to %s (protocol %d)." | the server accepted us | partial | ours does it but says it in its own words (the exact line not found) |
| msg.connect.anticheat | "Loading anticheat, this may take a few moments..." | the server asked for anticheat | missing | the feature is missing |
| msg.connect.noanticheat | "Trying to connect without anticheat." | anticheat could not load | missing | the feature is missing |
| msg.connect.anticheatreq | "Anticheat required by server, but no anticheat support linked in." | the server insists on anticheat | missing | the feature is missing |
| msg.connect.passiverecv | "Received passive connect from %s." | a server connected to us in passive mode | missing | the feature is missing |
| msg.connect.notconnected | "Can't "%s", not connected" | a forwarded word typed while not connected | done | `engine/console/console_more.cpp` (`complete`): the same words |
| msg.follow | "Following %s..." | `followip` | missing | the feature is missing |
| msg.follow.none | "No IP address to follow." | `followip` with nothing seen | missing | the feature is missing |
| msg.passive.off | "No longer listening for passive connections." | `passive` off | missing | the feature is missing |
| msg.passive.on | "Listening for passive connections at %s." | `passive` on | missing | the feature is missing |
| msg.reconnect.none | "No server to reconnect to." | `reconnect` with no last server | partial | ours does it but says it in its own words (the exact line not found) |
| msg.reconnect.loop | "Can not reconnect to loopback." | `reconnect` to the local server | partial | ours does it but says it in its own words (the exact line not found) |
| msg.reconnect | "Reconnecting..." | `reconnect` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.changing | "Changing map..." | the server changes map | partial | ours does it but says it in its own words (the exact line not found) |
| msg.disconnected | "Disconnected from server" | the player disconnected (an error line) | partial | ours does it but says it in its own words (the exact line not found) |
| msg.serverdisc | "Server disconnected" | the server dropped us | partial | ours does it but says it in its own words (the exact line not found) |
| msg.serverdisc.re | "Server disconnected, reconnecting" | the server asked us to reconnect | partial | ours does it but says it in its own words (the exact line not found) |
| msg.reset | "Server connection was reset." | the connection was refused | partial | ours does it but says it in its own words (the exact line not found) |
| msg.timeout | "Server connection timed out." | `cl_timeout` passed | partial | ours does it but says it in its own words (the exact line not found) |
| msg.protover | "Requested protocol version %d, but server returned %d." | a forced `cl_protocol` the server does not speak | missing | the feature is missing |
| msg.demoproto | "Demo uses unsupported protocol version %d." | a demo of an unknown protocol | partial | ours does it but says it in its own words (the exact line not found) |
| msg.r1q2enh | "'Enhanced' R1Q2 servers are not supported" | an enhanced protocol-35 server | missing | the feature is missing |
| msg.maptitle | "(the map title between two rule lines)" | the server data: the level's title in a ruled block | partial | ours prints the level's name in its own form; verify the rule lines |
| msg.rcon.usage | "Usage: %s <command>" | `rcon` alone | partial | ours does it but says it in its own words (the exact line not found) |
| msg.rcon.nopass | "You must set 'rcon_password' before issuing an rcon command." | `rcon` without a password | partial | ours does it but says it in its own words (the exact line not found) |
| msg.rcon.noaddr | "You must either be connected, or set the 'rcon_address' cvar to issue rcon commands." | `rcon` with nowhere to send | partial | ours does it but says it in its own words (the exact line not found) |
| msg.rcon.badaddr | "Bad address: %s" | a bad `rcon_address`, `serverstatus` or `remotemode` address | partial | ours does it but says it in its own words (the exact line not found) |
| msg.status.head | "Status response from %s" | `serverstatus` answer | missing | the feature is missing |
| msg.status.cols | "Num Score Ping Name" | `serverstatus` player table | missing | the feature is missing |
| msg.status.usage | "Usage: %s [address]" | `serverstatus` misuse | missing | the feature is missing |
| msg.dl.progress | "[UDP] %s [%s] [%d remaining file%s]" | a download finished, with how many remain | partial | ours does it but says it in its own words (the exact line not found) |
| msg.dl.open | "[UDP] Couldn't open %s for writing: %s" | a download's file cannot be written | partial | ours does it but says it in its own words (the exact line not found) |
| msg.dl.rename | "[UDP] Couldn't rename %s to %s: %s" | the finished `.tmp` cannot be renamed | missing | the feature is missing |
| msg.dl.mustconnect | "Must be connected to a server." | `download` while not connected | missing | the feature is missing |
| msg.dl.disabled | "Downloads are permanently disabled." | `download` with `allow_download -1` | missing | the feature is missing |
| msg.dl.usage | "Usage: download <filename>" | `download` alone | missing | the feature is missing |
| msg.dl.fail | "Couldn't download %s: %s" | the server refused a file | partial | ours does it but says it in its own words (the exact line not found) |
| msg.dl.mapload | "Couldn't load %s: %s" | the map cannot be loaded (missing or damaged): the connection drops | partial | ours does it but says it in its own words (the exact line not found) |
| msg.dl.mapversion | "Local map version differs from server: %i != %s" | the map on disk is not the server's: the connection drops | missing | the feature is missing (C36) |
| msg.dl.demomapversion | "Local map version differs from demo: %i != %s" | the map on disk is not the recording's: a warning, playback goes on | missing | the feature is missing |
| msg.map.novis | "Map has no Visibility, expect bugs and bad performance!" | a map loaded with no visibility data (a warning) | missing | the feature is missing |
| msg.http.server | "[HTTP] Download server at %s" | the server named an HTTP download URL | missing | the feature is missing |
| msg.http.progress | "[HTTP] %s [%s, %s/sec] [%d remaining file%s]" | an HTTP download finished | missing | the feature is missing |
| msg.http.fail | "[HTTP] Failed to fetch '%s': %s" | an HTTP download failed | missing | the feature is missing |
| msg.http.filelist | "[HTTP] Filelist is requesting a .%s file '%s'" | a filelist names a file of a guarded kind | missing | the feature is missing |
| msg.http.illegal | "[HTTP] Illegal file type '%s' in filelist." | a filelist names a forbidden kind | missing | the feature is missing |
| msg.demo.notrec | "Not recording a demo." | `stop`, `suspend` or `resume` while not recording | done | the same words in `games/shooters_edge/client/listen_record.cpp` |
| msg.demo.stopped | "Stopped demo (%s)." | `stop`, with the statistics | partial | ours does it but says it in its own words (the exact line not found) |
| msg.demo.begin | "Begin client demo recording." | recording starts | partial | ours does it but says it in its own words (the exact line not found) |
| msg.demo.already | "Already recording (%s)." | `record` while recording | done | the same words in `games/shooters_edge/client/listen_record.cpp` |
| msg.demo.nofile | "Missing filename argument." | `record` or `writeconfig` without a name | done | `engine/console/console_more.cpp` (`writeconfig`); `record` keeps its own |
| msg.demo.mustlevel | "You must be in a level to record." | `record` while not in a level | partial | ours does it but says it in its own words (the exact line not found) |
| msg.demo.to | "Recording client demo to %s." | recording starts | partial | ours does it but says it in its own words (the exact line not found) |
| msg.demo.resumed | "Resumed demo recording." | `resume` | not-taken | `client-gaps.md` round 27: suspend and resume not taken |
| msg.demo.alreadyresumed | "Demo recording is already resumed." | `resume` while not suspended | not-taken | `client-gaps.md` round 27 |
| msg.demo.suspended | "Suspended demo recording." | `suspend` | not-taken | `client-gaps.md` round 27 |
| msg.demo.alreadysusp | "Demo recording is already suspended." | `suspend` twice | not-taken | `client-gaps.md` round 27 |
| msg.demo.toomany | "Too many demo frames don't fit into %u bytes!" | a frame too big for `cl_demomsglen` (with "Try to increase 'cl_demomsglen' value and restart recording.") | not-taken | `client-gaps.md`: `cl_demomsglen` not taken |
| msg.demo.finished | "Demo finished" | a demo played to its end | partial | ours does it but says it in its own words (the exact line not found) |
| msg.demo.cantread | "Couldn't read %s: %s" | `demo` of a file that cannot be read | partial | ours says it in its own words (verify `client/demo_playback.cpp`) |
| msg.demo.nomvd | "MVD support was not compiled in." | an MVD demo without MVD support | partial | ours does it but says it in its own words (the exact line not found) |
| msg.seek.usage | "Usage: %s [+-]<timespec\|percent>[%%]" | `seek` misuse | partial | ours does it but says it in its own words (the exact line not found) |
| msg.seek.notplaying | "Not playing a demo." | `seek` with no demo | partial | ours does it but says it in its own words (the exact line not found) |
| msg.seek.pct | "Invalid percentage." | `seek` with a bad percentage | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.size | "Unknown file size, can't seek." | `seek` by percent on a stream | missing | the feature is missing |
| msg.seek.rel | "Invalid relative timespec." | `seek` with a bad relative time | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.abs | "Invalid absolute timespec." | `seek` with a bad time | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.back | "Couldn't seek backwards without snapshots!" | `seek` back with `cl_demosnaps 0` | missing | the feature is missing |
| msg.timedemo | "%u frames, %3.1f seconds: %3.1f fps" | a `timedemo` ended | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.added | "Added location %s at %s" | `loc add` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.deleted | "Deleted location %s at %s" | `loc del` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.renamed | "Renamed location at %s to %s" | `loc set` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.none | "No closest location." | `loc del` or `set` with none near | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.nolist | "No locations to list." | `loc list` with none | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.listed | "%d location%s listed" | `loc list` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.nowrite | "No locations to write." | `loc save` with none | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.wrote | "Wrote %d location%s to %s" | `loc save` | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.usage | "Usage: %s <add\|del\|set\|list\|save>" | `loc` misuse | partial | ours does it but says it in its own words (the exact line not found) |
| msg.loc.line | "Line %d is incomplete in %s" | a bad line in a `.loc` file | partial | ours does it but says it in its own words (the exact line not found) |
| msg.mustlevel | "Must be in a level." | `loc`, `aashot` and others while not in a level | partial | ours does it but says it in its own words (the exact line not found) |
| msg.con.usage | "Usage: %s <filename>" | `condump`, `demo`, `dumpstatusbar` misuse | missing | the feature is missing |
| msg.con.dumped | "Dumped console text to %s." | `condump` | done | `engine/console/view.cpp` (#1248) |
| msg.con.chatlevel | "You must be in a level to chat." | `messagemode` while not in a level | partial | ours does it but says it in its own words (the exact line not found) |
| msg.con.remoteusage | "Usage: %s <address> <password>" | `remotemode` misuse | missing | the feature is missing |
| msg.con.badvalue | "Invalid value '%s' for '%s'" | a bad colour or value for a setting | partial | ours does it but says it in its own words (the exact line not found) |
| msg.bind.unbindusage | "unbind <key> : remove commands from a key" | `unbind` alone | partial | ours does it but says it in its own words (the exact line not found) |
| msg.bind.usage | "bind <key> [command] : attach a command to a key" | `bind` alone | partial | ours does it but says it in its own words (the exact line not found) |
| msg.bind.badkey | ""%s" isn't a valid key" | `bind` or `unbind` of an unknown key | partial | ours does it but says it in its own words (the exact line not found) |
| msg.bind.show | ""%s" = "%s"" | `bind <key>` of a bound key | partial | ours does it but says it in its own words (the exact line not found) |
| msg.bind.notbound | ""%s" is not bound" | `bind <key>` of a free key | done | the same words in `engine/console/console_binds.cpp` |
| msg.ignore.none | "No ignore filters." | `ignoretext` with none | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.list | "Current ignore filters:" | `ignoretext` lists them with "(n hits)" | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.short | "Match string "%s" is too short." | `ignoretext` of a too-short pattern | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.notfound | "Can't find ignore filter "%s"" | `unignoretext` of an unknown one | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.removed | "Removed %d ignore filter%s." | `unignoretext` | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.stuff.ignored | "Ignored stufftext: %s" | a pushed command refused by `cl_ignore_stufftext` | missing | the feature is missing |
| msg.writeconfig.help | "Save current configuration into file." | `writeconfig` help | done | `engine/console/console_more.cpp` |
| msg.writeconfig.wrote | "Wrote %s." | `writeconfig`, `aashot`, `dumpstatusbar` done | done | `engine/console/console_more.cpp` |
| msg.write.error | "Error writing %s" | a file could not be written | done | `engine/console/console_more.cpp` |
| msg.dump.mustlevel | "Must be in a level to dump." | `dumpclients`, `dumpstatusbar` outside a level | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.dump.none | "No %s to dump." | `dumplayout` with none | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.dump.done | "Dumped %s program to %s." | `dumpstatusbar`, `dumplayout` | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.skins.mustlevel | "Must be in a level to load skins." | `skins` outside a level | missing | the feature is missing |
| msg.skins.line | "client %d: %s --> %s/%s" | `skins` per player | missing | the feature is missing |
| msg.userinfo | "User info settings:" | `userinfo` | done | `engine/console/console_more.cpp` |
| msg.play.usage | "Usage: %s <sound> [...]" | `play` alone | done | `games/shooters_edge/client/menu_sound.cpp` |
| msg.vidrestart | "Manual `vid_restart' command ignored." | a typed `vid_restart` (with an explanation that video settings apply themselves) | done | `games/shooters_edge/client/window_cvars.cpp` |
| msg.fps.round | "%s value `%d' is inexact and will be rounded to `%d'." | a `cl_maxfps` that is not whole milliseconds (with "(Set `cl_warn_on_fps_rounding' to `0' to disable this warning.)") | missing | the feature is missing |
| msg.mouse.off | "Mouse input disabled." | the mouse could not start | missing | the feature is missing |
| msg.draw.none | "No draw strings registered." | `draw` or `undraw` with none | done | `engine/hud/draw_strings.cpp` |
| msg.draw.list | "Name X Y (table of draw strings)" | `draw` alone lists them | done | `engine/hud/draw_strings.cpp` |
| msg.draw.usage | "Usage: %s <name> <x> <y> [color]" | `draw` misuse | done | `engine/hud/draw_strings.cpp` |
| msg.draw.color | "Unknown color '%s'" | `draw` with a bad colour | done | `engine/hud/draw_strings.cpp` |
| msg.draw.deleted | "Deleted all draw strings." | `undraw all` | done | `engine/hud/draw_strings.cpp` |
| msg.draw.notfound | "Draw string '%s' not found." | `undraw` of an unknown name | done | `engine/hud/draw_strings.cpp` |
| msg.sky.usage | "Usage: sky <basename> [rotate] [axis x y z]" | `sky` alone | missing | the feature is missing |
| msg.sky.nomap | "No map loaded." | `sky` or `timerefresh` outside a level | missing | the feature is missing |
| msg.timerefresh | "%f seconds (%f fps)" | `timerefresh` result | missing | the feature is missing |
| msg.loadtimeout | "Loading plaque timed out." | a level that took over 120 s to load | missing | the feature is missing |
| msg.fog.none | "No fog." | `fog` with none set | partial | ours does it but says it in its own words (the exact line not found) |
| msg.fog.usage | "Usage: %s <r g b density> [sky_factor]" | `fog` misuse | partial | ours does it but says it in its own words (the exact line not found) |
| msg.menu.nomenu | "No such menu: %s" | `pushmenu` of an unknown page | done | `games/shooters_edge/menu/game_menu.cpp` |
| msg.menu.usage | "Usage: %s <menu>" | `pushmenu` alone | done | `games/shooters_edge/menu/game_menu.cpp` |
| msg.menu.noplayers | "No player models found." | the player page with no figures | missing | the feature is missing |
| msg.menu.script | "Unknown keyword '%s'" | a bad line in the menu script (and its other warnings) | missing | the feature is missing |
| msg.servers.http | "Can't fetch '%s', no HTTP support compiled in." | an HTTP server list without HTTP | missing | the feature is missing |
| msg.servers.badaddr | "Bad server address: %s" | a bad line in a server list | partial | ours does it but says it in its own words (the exact line not found) |
| msg.snd.notstarted | "Sound system not started." | `soundinfo` or `soundlist` with no sound | missing | the feature is missing |
| msg.ogg.stopped | "Playback stopped." | `ogg info` with no music, and `ogg stop` | missing | the feature is missing |
| msg.ogg.cin | "Can't play music in cinematic mode." | `ogg play` during a cinematic | missing | the feature is missing |
| msg.ogg.usage | "Usage: %s <info\|play\|stop\|next\|pause>" | `ogg` alone or with an unknown word | missing | the feature is missing (C29) |
| msg.ogg.notrack | "No such track: %s" | `ogg play` of a track that is not found | missing | the feature is missing (C29) |
| msg.ogg.playusage | "Usage: %s %s <track>" | `ogg play` with no track | missing | the feature is missing (C29) |
| msg.al.init | "OpenAL initialized." | the OpenAL backend started | missing | the feature is missing |
| msg.al.fail | "Failed to initialize OpenAL: %s" | the OpenAL backend failed | missing | the feature is missing |
| msg.al.devices | "Available devices:" / "No devices available" | `al_device ?` lists the OpenAL output devices | missing | the feature is missing (C32) |
| msg.gtv.listen | "Listening for GTV connections." | `gtv start` | missing | the feature is missing |
| msg.gtv.accepted | "Accepted MVD client [%s]" | a TV viewer joined the client's relay | missing | the feature is missing |
| msg.aashot.full | "All scoreshot slots are full." | `aashot` with 1000 files already | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.cin.open | "Couldn't open %s: %s" | a cinematic that cannot be opened (and its other errors) | partial | ours says it in its own words (verify `client/cinematic.cpp`) |
| msg.serverbug | "SERVER BUG: %s on entity %d last seen %d frames ago" | a server sent a delta for an entity the client never had (developer) | missing | the feature is missing |
| msg.faults | "%s: bad index / bad entity / invalid areabits length / illegible server message: %d ..." | the parse errors that drop the connection, each naming the function and the value | partial | ours refuses with its own words naming the byte (`client/demo_q2.cpp`) |

## Demo playback and recording features

done 4, partial 10, missing 7, not-taken 2

What a player can do with client recordings (`.dm2`) and multi-view recordings (`.mvd2`) in the client.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| demo.play.dm2 | play a `.dm2` | `demo <name>` plays a client recording at its own pace, the view the recorded player's | done | `games/shooters_edge/client/demo_q2.cpp`, `client/demo_playback.cpp` |
| demo.play.gz | play a gzipped recording | `.dm2.gz` and `.mvd2.gz` read directly | partial | ours reads packed recordings by its own archive list (`engine/vfs`); verify `.gz` |
| demo.play.mvd2 | play a multi-view recording | an `.mvd2` handed to the built-in server, which plays it as a broadcast | done | `games/shooters_edge/client/demo_mvd2.cpp` |
| demo.play.paths | where demos are read | `demos/` unless the name starts with `/` (then from the root of the search path); Tab completes | partial | ours from the demo page and any path (C40) |
| demo.play.fov | the field of view while playing | the player's own `fov` replaces a recorded field of 90 or more; `uf` 4 always own, 32 always recorded | missing | C3 |
| demo.play.f1 | the scoreboard while playing | holding F1 shows the recorded layout whether or not it was up | missing | C15 |
| demo.play.shift | free look while playing | holding SHIFT turns the view by the player's own mouse | partial | ours has its own free camera over demos (`client/demo_eyes.cpp`); not the SHIFT hold (C15) |
| demo.play.pause | pausing a demo | `pause` or the console/menu (`cl_autopause`) pauses playback; "[PAUSED]" on the bar | partial | ours pauses by the transport (`plugins/shooter/demo_transport.cpp`); the auto-pause missing (C24) |
| demo.play.seek | seeking | `seek` forward and back by frames, seconds, mm:ss.ff or percent, back from the snapshots every `cl_demosnaps` s | partial | ours `demo_jump` with `seek` as a synonym; percent of the bytes and tenths not taken (`client-gaps.md` round 22) |
| demo.play.bar | the progress bar | `scr_demobar` | partial | C39 |
| demo.play.end | the end of a demo | prints "Demo finished" and disconnects, or holds the last frame with `cl_demowait` | done | `games/shooters_edge/client/demo_playback.cpp` |
| demo.play.timedemo | timing a demo | `timedemo 1` plays as fast as frames draw and prints frames, seconds and frames a second | done | `games/shooters_edge/client/demo_playback.cpp` |
| demo.play.browser | the demo browser | the menu's demo page (table "Menu pages and rows") | partial | C40 |
| demo.play.compat | old recordings | a recording of an older kind (protocol 34 without the extended frames) plays; the bar is not drawn for it | partial | ours plays protocol 34 and the extended layouts (`demo_q2.cpp`); verify the bar rule |
| demo.rec.start | `record <name>` | records into `demos/<name>.dm2` from the current state (a late start plays); can record while playing another demo | partial | `client/listen_record.cpp`, `net/q2_client.cpp`; recording while playing a demo not found |
| demo.rec.stats | statistics | `record` while recording and `stop` print the frames, size and dropped messages | missing | C38 |
| demo.rec.gzip | `record -z` | gzip-compressed recording | missing | C38 |
| demo.rec.size | `record -e` / `-s` | the extended 4086-byte or standard 1390-byte frames | not-taken | `client-gaps.md` (C38): `cl_demomsglen` not taken |
| demo.rec.downgrade | protocol downgrade | a recording made on a modern protocol is written as protocol 34; too-large frames dropped, reliable messages kept first | partial | ours speaks 34 only, so nothing to downgrade |
| demo.rec.suspend | `suspend` / `resume` | pause and resume a recording | not-taken | `client-gaps.md` round 27 |
| demo.rec.auto | recording by a trigger | `cl_beginmapcmd "record ..."` starts a recording on each map | missing | C33 |
| demo.gtv | the client's own TV relay | `gtv start` serves the player's own game to TV viewers over TCP (only in a build that asks for it) | missing | not found |
| demo.aashot | text scoreboard shots | `aashot` writes the scoreboard as text; `aadump` prints it | missing | not found |

## Keys: the fixed keys and the default bindings

done 4, partial 3, missing 4, not-taken 0

Keys the client handles itself (a player cannot unbind them) and where the default bindings come from.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| key.console | the console key | ` and ~ (without SHIFT) always toggle the console | partial | ours: the key under ESCAPE toggles it (`engine/console/view.cpp`); verify SHIFT+` types the letter |
| key.altenter | Alt+Enter | toggles full screen, always | done | `engine/platform/window_cvars.cpp` `alt_enter` |
| key.escape.menu | ESCAPE | closes the console (or opens the main menu before a game), leaves chat, opens the in-game page in a game, the main page out of one | done | `games/shooters_edge/menu/game_menu.cpp`: in a level the in-game page (`MenuTable::in_level`) |
| key.escape.putaway | ESCAPE with a screen up | puts away the help computer or inventory (`putaway`); held, opens the menu | missing | not found |
| key.fkeys.menu | F1 to F12 in a menu | still run their bindings | missing | not found |
| key.cinematic | any button key in a cinematic | skips it | done | `games/shooters_edge/client/cinematic.cpp` |
| key.shift.demo | SHIFT in a demo | takes the mouse for free look | missing | C15 |
| key.f1.demo | F1 in a demo | shows the recorded layout | missing | C15 |
| key.buttons | button keys | a `+` binding gets the key and time; its release runs the `-` form even in the console | done | `engine/console/binds.cpp` |
| key.keypad | the keypad in the console | the keypad's keys type their characters in the console | partial | verify (`engine/console/view.cpp`) |
| key.defaults | the default bindings | none built in: the game's `default.cfg` (in the pack) binds the keys, then `config.cfg`; `bindlist` shows them | partial | ours runs the pack's `default.cfg` in Quake II's start order (`porting/shooters-edge/installs.md` § Q2PRO); verify that our own default binds (`client/controls.cpp`) do not replace the pack's |

## Sound and music

done 5, partial 6, missing 2, not-taken 0

How the client makes sound, beyond the settings themselves.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| snd.spatial | positioned sounds | each sound placed by its entity (moving with it) or a point, by volume and attenuation, on 8 channels a entity; the listener at the view | done | `games/shooters_edge/client/demo_sound.cpp`, `client/demo_heard.cpp` |
| snd.loops | entity loop sounds | an entity's sound field loops while it is in the frame, merged by sound | partial | landed (`q2-gaps2-client.md` checked); `s_ambient`'s choices missing (C32) |
| snd.underwater | under water | every sound high-shelf filtered at 5 kHz (`s_underwater`) | done | the eye in a liquid, `engine/audio/synth.cpp` `muffle` (M18k4 round 34) |
| snd.footsteps | footsteps | a step set by the surface under the entity (`.mat` files, up to 15 steps), never the same twice in a row | partial | ours the four classic steps (C34) |
| snd.landing | landings | `player/land1.wav` on a short fall | done | `games/shooters_edge/client/demo_q2.cpp` |
| snd.chat | the chat beep | `misc/talk.wav` (or `talk1.wav`) on a chat line | done | `cl_chat_sound` (M18k4 round 32) |
| snd.backends | the sound back-ends | the software mixer (any rate, `s_khz`) or OpenAL (HRTF) | partial | ours one mixer at 48 kHz (`engine/audio`) (C32) |
| snd.focus | sound and the window | `s_auto_focus` mutes when minimised or unfocused | missing | C32 |
| snd.figure | a figure's own sounds | a `*name` sound plays the figure's own `players/<model>/name`, else the male one | partial | verify (`client/demo_sound.cpp`) |
| snd.music.track | the level's music | configstring 1: empty or 0 stops, N `music/trackNN` or `music/NN` (remapped for the mission packs), text `music/<text>`, tried as `.flac`, `.opus`, `.ogg`, `.mp3`, `.wav`, loose files in the search path and the install and home folders; repeats | partial | ours plays the world's `sounds` track in its own game (`engine/audio/music_console.cpp`); a joined server's or a recording's track missing (C29) |
| snd.music.shuffle | shuffle | `ogg_shuffle` plays every file in `music/` shuffled | missing | C29 |
| snd.music.stop | when music stops | at a map change, the loading plaque and a cinematic; while disconnected the `ogg_menu_track` plays (none by default) | partial | verify (C29) |
| snd.cinematic | cinematic sound | a cinematic's own audio track | done | `games/shooters_edge/client/cinematic.cpp` |

## Picture and video

done 5, partial 9, missing 6, not-taken 0

How the client sets up and draws the picture, beyond the settings themselves. Ours draws with its own renderer (`engine/render`, `plugins/classic/app/look.cpp`); the GL driver's internals are not taken (`client-gaps.md`).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| vid.window | the window | windowed by `vid_geometry` or a listed full-screen mode | partial | ours full screen on or off (`engine/platform/window_cvars.cpp`) |
| vid.gamma | gamma | `vid_gamma` applied to textures, or by the display (`vid_hwgamma`) | partial | ours `gamma` (synonym `vid_gamma`); no hardware gamma |
| vid.hor | wide screens | the 4:3 field kept vertically (hor+) by `cl_adjustfov` | done | `q2-gaps2-client.md` checked (hor+) |
| vid.fov.server | the server's field of view | the player state's field drawn, interpolated (zoom, fixed FOV) | missing | C3 |
| vid.areas | areas behind shut doors | a leaf whose area is not joined is not drawn | missing | C1 |
| vid.pvs2 | the second PVS point | the PVS joined with that of a point 16 units below (above in water) | missing | C2 |
| vid.particles | particles | squares facing the view, grown with distance, `gl_partscale`, `gl_partstyle`, `gl_partshape` | partial | ours draws the classic particles (landed); the settings and size rule not compared (C18) |
| vid.shadows | blob shadows | `gl_shadows` | missing | C19 |
| vid.shells | colour shells | a second copy pushed out, flat colour, 0.3 | done | `games/shooters_edge/client/q2_look.hpp` |
| vid.ir | infra-red goggles | IR-visible models drawn flat red | missing | C20 |
| vid.lightstyles | light styles | each style's letters stepped at 10 a second, or blended between letters with `cl_lerp_lightstyles` | partial | the window's own game animates them; a server's styles not kept (table "Configstrings") |
| vid.dlights | dynamic lights | effect and flash lights added each frame (`gl_dynamic`, `cl_lights`, `cl_muzzlelight_time`) | partial | ours lights from its own events; the wire's flashes skipped (tables "Muzzle flashes") |
| vid.vweps | other players' weapons | the weapon model from the figure's folder (`cl_vwep`) | done | `plugins/classic/vweps.cpp` |
| vid.skins | player skins | `players/<model>/<skin>.pcx`, missing ones the stock figure (`cl_noskins`) | done | `games/shooters_edge/client/cvar_layers.cpp` (`cl_noskins`) |
| vid.gun | the view weapon | the gun model from the player state, its frame interpolated, by `hand`, `cl_gun`, `cl_gunfov`, `cl_gun_x/y/z`, `cl_gunalpha` | partial | landed but `cl_gunalpha` (C16) |
| vid.bob | the view | bob, roll, kicks, falls from the player state | done | `plugins/classic/q2_view.cpp` |
| vid.kick | the server's kick | the player state's kick angles added (`cl_kickangles`) | partial | added; the switch missing (C16) |
| vid.thirdperson | third person | `cl_thirdperson` with range and angle (cheat); the player fades as the camera comes close | missing | ours over demos only |
| vid.screenshot | screenshots | `screenshot` named `quakeXXX` in `gl_screenshot_format`, written off the main thread | partial | C22 |
| vid.replacements | replacement content | `.png`/`.jpg`/`.tga` textures over `.pcx`/`.wal`, MD5 models over MD2 | partial | ours replaces by classic enhance (`plugins/classicenhance`), not loose replacement files; verify |

## Connection, network, downloads and the rest

done 2, partial 6, missing 9, not-taken 0

Behaviour the client offers outside the screen.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| net.protocols | protocols | 34 (stock), 35 (R1Q2), 36 (Q2PRO) by `cl_protocol` | partial | 34 only (C42) |
| net.predict | prediction | the player's move predicted from the last frame by the shared move code; misses smoothed | done | `games/shooters_edge/net/q2_predict.cpp` |
| net.userinfo | user info while joined | a changed name, skin, hand, fov, rate sent at once | missing | C30 |
| net.cheatlock | the cheat lock | with more than one player slot, cheat-protected settings forced back each frame | missing | not found |
| net.stufftext | server-pushed commands | run in the client's console, refused unless whitelisted with `cl_ignore_stufftext` | partial | verify which run (table "Server messages"); C33 |
| net.triggers | message triggers | `trigger` runs a command when a server line matches; never in demos or on a relay | missing | C31 |
| net.autoreply | version reply | a chat line ending "!version" makes the client say its version after 1 to 2 s, at most once in 2 minutes | missing | not found |
| net.recentaddr | addresses in chat | addresses seen in chat remembered for `followip` | missing | C31 |
| net.download.order | what is fetched | the map, its models (and their skins and frames), sounds, pictures, figures (models, weapons, skins, icons, sounds), the sky, then the wall textures | partial | ours the map, models, sounds and figures' models and skins (`net/q2_client.cpp`) (C36) |
| net.download.resume | resuming | a `.tmp` resumed from its length, renamed when whole | missing | C36 |
| net.download.http | HTTP downloads | from the server's URL, with filelists, two at once, falling back to UDP | missing | C37 |
| net.passive | passive connections | wait for a server to connect to us | missing | C42 |
| net.browser | the server list | HTTP list, address book, `servers.lst`, LAN broadcast; pinged at `ui_pingrate` | partial | ours LAN, masters, hub (C41) |
| net.config.write | the saved config | on quit `config.cfg` (`q2config.cfg` on Windows) gets binds and saved settings | done | `engine/console/console_builtins.cpp` (`cfg_save`) |
| net.config.order | the start order | `default.cfg`, the config, `autoexec.cfg`, `postexec.cfg`, the command line, `postinit.cfg` | partial | ours runs Quake II's own order (`porting/shooters-edge/installs.md`); verify `postexec.cfg` and `postinit.cfg` |
| net.completion | Tab completion | commands, settings, aliases, macros and each command's own arguments (maps, demos, sounds, settings' values) | partial | ours completes commands and settings (`engine/console`); arguments not compared |
| net.history | the typed history | kept across runs (`con_history`) | missing | C23 |
