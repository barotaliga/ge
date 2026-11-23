# Quake II ledger: the client (every thing the client offers a player), against upstream Q2PRO

Clean-room reader's ledger, 2026-11-21. Reference: UPSTREAM Q2PRO, github.com/q2pro/q2pro, commit `601a8df` (committed 2025-12-11), its `src/client`, `src/refresh` and `src/common` parts, its menu script and its client manual (`doc/client.asciidoc`), read by a reader; described here in our words, no code. This replaces the earlier client ledger, which was read against the Action Quake community's fork of it (the wrong reference): every row of that ledger was checked against upstream, by name, default, menu row, printed words, flash light and sound and temporary entity, through a full comparison of the two programs' client, renderer and shared parts, menu scripts and manuals, and the fork-only and upstream-only names and words found that way were dropped or added. The Windows service commands (`installservice`, `deleteservice`) belong to a dedicated server and are left to the server ledger. Ours: the gameengine2 tree as of 2026-11-21. Status per row: `done` ours does it as a player sees it (our own words for the same meaning count), `missing` (ranked in its note), `not-taken` (a doc of ours or the owner decided; named). Cross-references: C1..C45 are the gaps of `porting/shooters-edge/q2-gaps2-client.md` (that file names the fork in places; where its words and this ledger's differ on a default or a behaviour, this ledger's are upstream's). A `done` row with only a file in the last column was matched by name in that file and by the notes (`client-gaps.md`, `q2-gaps2-client.md` § Checked and the same); a note saying "verify" was found by name only.

**Regraded 2026-11-23** (the owner's decision): the `partial` grade is gone, as in the Quake ledger. Each partial row became `done` (it works: its setting landed since, it was checked by name, or it says the same thing in our own words, which the player-text rule makes the intended ones), `not-taken` (our own design on purpose, noted "Our design:"; or the tail the owner chose not to build: cosmetic details, mission-pack and later effects, debug, developer and admin extras, hosted game libraries' edge cases, noted "Tail (decided 2026-11-23)"), or `missing` with a rank (rank 1 gameplay and daily use; rank 2 settings' Quake II names and the base game's effects seen on a joined server or in a demo). A missing row whose note says LIFT has a counterpart in the Quake code already: build it there once for both games (the settings lift). The missing count rose once by the regrade, because hidden gaps became rows; from here it only goes down.

Against the fork's ledger: **63 rows dropped as fork-only** (the Action Quake settings, weapon sounds, drawn crosshair and zoom sensitivity, the game-defined HUD and indicators, Discord and Steam, the setting sync and protocol 38's messages, `mul`/`div`, the lightstyle and muzzle-light overrides and the lag-graph scale with their menu rows, the map re-download messages); **14 rows added as upstream-only** (`cl_smooth_explosions`, `cl_lerp_lightstyles`, `cl_muzzlelight_time`, `ogg_menu_track`, `ogg_remap_tracks`, `al_merge_looping`, `gl_draworder`, `gl_clearcolor`, `$gl_viewleaf`, the player flashes' flash model, the map-version and music messages); 5 rows added for things both have that the earlier ledger missed (the scope picture in layouts and four printed lines); 70 rows rewritten where upstream differs (defaults of `cl_hit_markers` 2 and `ui_colorservers` 0, `ch_scale` not halved, `cl_footsteps` 2, `cl_async` 2, `cl_disable_particles` 32, the `cl_maxfps` cap of 125, `ogg pause`, item-name completion, the third-person camera, every player muzzle flash's light, sound and model, the hornet's remastered guns, the server browser's Mod column and colours, the sound page's "sound volume", the map checksum's plain drop, the single landing sound, and wording that named the fork). `utf8test` joined the test-command row.

Gaps this ledger finds that the C list of `q2-gaps2-client.md` does not name: a player's weapon muzzle flashes (light, sound and flash model) and every monster muzzle flash are skipped from a joined server and a recording (`client/demo_q2.cpp` reads only flashes 9 to 11); 16 temporary entities are read and dropped and the rest drawn as five generic kinds; the frame-cycling effect bits (`EF_ANIM*`), the CTF flag bits and the mission packs' effect bits are not read; light styles, the sky's configstrings and the general strings are not kept from a server; the layout words `rstring`, `rstring2`, `color`, `health_bars`, the scope picture and the five other `stat_` forms are missing; the console's own `help` hides Quake II's forwarded `help` (the help computer); ESCAPE does not put away the inventory or help computer; Alt+Enter is not fixed to full screen; the save pages have 12 slots, not 15 and an autosave; ours sends a `version` in the user info, which upstream does not; Tab does not complete item names after `use`, `drop`, `give`.

## Totals

| kind | done | missing | not-taken | rows |
| --- | --- | --- | --- | --- |
| Client console commands | 138 | 1 | 17 | 156 |
| Client settings (cvars) | 231 | 9 | 110 | 350 |
| Macros ($name in typed and bound text) | 26 | 0 | 4 | 30 |
| Menu pages and rows | 146 | 4 | 12 | 162 |
| Server messages the client parses (svc) | 26 | 0 | 1 | 27 |
| Temporary entities (svc_temp_entity) | 21 | 18 | 26 | 65 |
| Muzzle flashes (svc_muzzleflash, a player's) | 19 | 0 | 1 | 20 |
| Muzzle flashes (svc_muzzleflash2, a monster's) | 32 | 0 | 1 | 33 |
| Entity effect bits (EF_) | 32 | 0 | 1 | 33 |
| Entity render bits (RF_) and view bits (RDF_) | 22 | 3 | 7 | 32 |
| Configstrings | 10 | 0 | 6 | 16 |
| The layout language (status bar and layouts) | 27 | 0 | 0 | 27 |
| HUD and screen elements | 18 | 0 | 11 | 29 |
| Messages printed to the player | 130 | 0 | 17 | 147 |
| Demo playback and recording features | 13 | 0 | 10 | 23 |
| Keys: the fixed keys and the default bindings | 10 | 0 | 1 | 11 |
| Sound and music | 13 | 0 | 0 | 13 |
| Picture and video | 16 | 0 | 4 | 20 |
| Connection, network, downloads and the rest | 15 | 1 | 2 | 18 |
| **all** | 945 | 36 | 231 | 1212 |

## Client console commands

done 138, missing 1, not-taken 17

Every command the client, its renderer, sound, menus and the shared core register (developer-only test commands at the end). Forwarded words are those the client sends on to the game library unchanged.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cmd.+forward | `+forward` / `-forward` | move forward while held (with `+klook`, look down instead) | done | `plugins/shooter/controls.cpp` |
| cmd.+back | `+back` / `-back` | move back while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveleft | `+moveleft` / `-moveleft` | step left while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveright | `+moveright` / `-moveright` | step right while held | done | `plugins/shooter/controls.cpp` |
| cmd.+moveup | `+moveup` / `-moveup` | jump (swim up) while held: Quake II's only jump key | done | `plugins/shooter/controls.cpp` (ours also has `+jump`, which Quake II lacks; C7); held by `tests/shooters_edge_listen_test.cpp` (an up of 10 or more jumps) |
| cmd.+movedown | `+movedown` / `-movedown` | crouch (swim down) while held | done | `plugins/shooter/controls.cpp` |
| cmd.+left | `+left` / `-left` | turn left at `cl_yawspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+right | `+right` / `-right` | turn right at `cl_yawspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+lookup | `+lookup` / `-lookup` | look up at `cl_pitchspeed` while held | done | `plugins/shooter/controls.cpp` |
| cmd.+lookdown | `+lookdown` / `-lookdown` | look down while held | done | `plugins/shooter/controls.cpp` |
| cmd.+strafe | `+strafe` / `-strafe` | turn keys and the mouse sidestep while held | done | `plugins/shooter/controls.cpp` |
| cmd.+speed | `+speed` / `-speed` | toggles run/walk against `cl_run` while held; turn rates times `cl_anglespeedkey` | done | `plugins/shooter/controls.cpp` |
| cmd.+attack | `+attack` / `-attack` | fire while held (button bit 1) | done | `plugins/shooter/controls.cpp` |
| cmd.+use | `+use` / `-use` | the use button (button bit 2), sent only on the extended protocols | done | `plugins/shooter/controls.cpp` (`+use`, `kButtonUse`; a tap between two moves kept), sent as Quake II's use bit by `games/shooters_edge/net/q2_remote.cpp` |
| cmd.+klook | `+klook` / `-klook` | forward/back keys look up and down while held | done | `plugins/shooter/controls.cpp` |
| cmd.+mlook | `+mlook` / `-mlook` | mouse looks up and down while held; releasing re-centres with `lookspring` | done | `plugins/shooter/controls.cpp` |
| cmd.centerview | `centerview` | levels the view pitch | done | `plugins/shooter/controls.cpp` |
| cmd.impulse | `impulse <n>` | sends an impulse byte with the next move | done | `games/shooters_edge/client/controls.cpp` |
| cmd.in_restart | `in_restart` | restarts the input (mouse) layer | done | `games/shooters_edge/client/window_cvars.cpp`: typed, it says why none is needed (input devices are picked up as they are plugged in) |
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
| cmd.connect | `connect <address>` | connects to a server (a second argument is ignored; protocol from `cl_protocol`) | done | `games/shooters_edge/net/qw_remote.cpp`: a second argument is ignored with a line that says to use `cl_protocol`; the protocol is chosen from the challenge (net/q2_client.cpp) |
| cmd.disconnect | `disconnect` | leaves the server, or stops a demo | done | `games/shooters_edge/net/qw_remote.cpp` |
| cmd.reconnect | `reconnect` | reconnects to the last server; "No server to reconnect to." | done | `games/shooters_edge/net/qw_remote.cpp` |
| cmd.followip | `followip [n]` | connects to the n-th last address seen in chat | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.passive | `passive` | toggles waiting for a server to connect to us (through a NAT) | done | `games/shooters_edge/net/q2_pingservers.cpp` (`add_passive_command`): leaves any game, listens on a socket of its own, joins the server whose `passive_connect` comes |
| cmd.rcon | `rcon <command>` | sends a remote-console command with `rcon_password` to the server or `rcon_address` | done | `games/shooters_edge/net/qw_admin.cpp` |
| cmd.pingservers | `pingservers` | pings the address book `adr0`..`adr15` and the LAN | done | `games/shooters_edge/net/q2_pingservers.cpp`: `info 34` to the LAN broadcast and each address book entry, each answer printed (3 s broadcast, 6 s direct) |
| cmd.serverstatus | `serverstatus [address]` | asks a server for its info and players and prints them by frags | done | `games/shooters_edge/net/qw_serverstatus.cpp`: its own socket, joined or not; both games' answers |
| cmd.pause | `pause` | pauses a single-player game or a demo | done | a game played alone on the window's own server (`client/listen.cpp`); over a played demo it stops and starts the demo where it is (`client/main.cpp` through `net/window_words.hpp`'s `first`, any game's demo; a timed demo runs on); on a joined server nothing stops, as on the original |
| cmd.skins | `skins` | reloads every client's skin and prints "client n: name --> model/skin" | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/net/q2_remote.cpp`: on a Quake II server joined, the level read again and a line a player; the right side is the look asked, not the stand-in drawn when it is missing |
| cmd.userinfo | `userinfo` | prints the user info being sent | done | `engine/console/console_more.cpp`: "User info settings:" then each key padded to 20 |
| cmd.download | `download <file>` | asks the server for a file over the connection | done | `games/shooters_edge/net/q2_remote.cpp` (`net/q2_client.cpp` `q2_download`) |
| cmd.dumpclients | `dumpclients` | prints every client's index and configstring | done | `games/shooters_edge/client/q2_bar.cpp` |
| cmd.dumpstatusbar | `dumpstatusbar <file>` | writes the status-bar program to a file | done | `games/shooters_edge/client/q2_bar.cpp`: written under `layouts/` |
| cmd.dumplayout | `dumplayout <file>` | writes the current layout program to a file | done | `games/shooters_edge/client/q2_bar.cpp`: written under `layouts/` |
| cmd.writeconfig | `writeconfig <file> [-a -b -c -m]` | writes aliases, binds, saved and changed settings to a file | done | `engine/console/console_more.cpp`: -a -b -c -m, default bindings and archived settings, `.cfg` added |
| cmd.vid_restart | `vid_restart` | restarts video (manual use ignored unless `cl_allow_vid_restart`) | done | `games/shooters_edge/client/window_cvars.cpp`: typed, it is ignored with the line and why (the window applies video settings at once) |
| cmd.r_reload | `r_reload` | reloads every picture and model | done | the level's world made again, its textures and models read from the files (`games/shooters_edge/client/render_words.cpp`); "No map loaded." outside one |
| cmd.snd_restart | `snd_restart` | restarts the sound system | done | `games/shooters_edge/client/menu_sound.cpp`: typed, it says why none is needed (sound settings apply as they change), as `vid_restart` does |
| cmd.play | `play <sound> [...]` | plays local sounds by name | done | `games/shooters_edge/client/menu_sound.cpp` (`play`, `playvol`); the music player keeps its own words under `cd` |
| cmd.stopsound | `stopsound` | stops every sound playing | done | `games/shooters_edge/client/menu_sound.cpp`: every sampled voice stopped, the music bus kept (`engine/audio/synth.cpp` `sample_stop_all`) |
| cmd.soundlist | `soundlist` | lists loaded sounds | done | each sound read with its length, rate and loop, the ones not found, then the count and memory (`DemoSounds::list_lines`, `games/shooters_edge/client/demo_sound.cpp`) |
| cmd.soundinfo | `soundinfo` | prints the sound device's state | done | `games/shooters_edge/client/menu_sound.cpp` (`add_sound_status_commands`): the device's driver, the rate and format, the sounds playing and cut short |
| cmd.ogg | `ogg info/play/stop/next/pause` | the music player: `info` describes the playing stream's format (or "Playback stopped."), `play <track> [soft]` plays a track number or a file under `music/`, `stop`, `next` (the next shuffled track, or restarts the automatic track), `pause` or `toggle` pauses and resumes | done | `engine/audio/music_console.cpp` (`music`, `ogg` its Quake II spelling in `client/cvar_layers.cpp`): info names the track ("Playback stopped." when none; ours, not the decoder's dump), play takes a track number or a name under `music/` (its extension and case ignored, subfolders too: `classic/content/soundtrack.hpp`'s `music_file`), soft lets the old fade, stop, next, pause and toggle (M18v, sound slice) |
| cmd.wave | `wave <n>` | forwarded: plays a gesture | done | forwarded to the rules (`games/shooters_edge/client/main.cpp` unknown-word hook; `server/q2_commands.cpp`) |
| cmd.inven | `inven` | forwarded: opens/closes the inventory | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.kill | `kill` | forwarded: suicide | done | forwarded (`server/q2_server.cpp`) |
| cmd.use | `use <item>` | forwarded: uses an item by name; Tab completes the item names the server listed | done | `client/q2_bar.cpp` (`add_q2_bar_commands`): forwarded; Tab after `use` completes the item names the last frame's world listed, sorted, a name with a space quoted (`engine/console/tab_list.cpp`'s argument completion, `Console::arg_values`), and the suggestions offer them |
| cmd.drop | `drop <item>` | forwarded: drops an item by name; Tab completes item names | done | `client/q2_bar.cpp`: forwarded; Tab after `drop` completes the item names as `use` does |
| cmd.info | `info` | forwarded: a mod's info word | done | forwarded when not ours |
| cmd.prog | `prog` | forwarded | done | forwarded when not ours |
| cmd.give | `give <item>` | forwarded cheat; Tab completes item names | done | `games/shooters_edge/server/q2_commands.cpp`; Tab after `give` completes the item names as `use` does (`client/q2_bar.cpp`) |
| cmd.god | `god` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.notarget | `notarget` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.noclip | `noclip` | forwarded cheat | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.invuse | `invuse` | forwarded: use the selected item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invprev | `invprev` | forwarded: select the previous item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invnext | `invnext` | forwarded: select the next item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.invdrop | `invdrop` | forwarded: drop the selected item | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.weapnext | `weapnext` | forwarded: next weapon | done | forwarded (`server/q2_fight.cpp`) |
| cmd.weapprev | `weapprev` | forwarded: previous weapon | done | forwarded (`server/q2_fight.cpp`) |
| cmd.help | `help` (unregistered) | an unknown word, so forwarded: the help computer (the stock menu binds F1 to it) | done | `net/game_commands.cpp`: `help` alone in a Quake II game (the window's own or a server joined) goes to the rules, the help computer; `help <name>` stays the console's explanation |
| cmd.score | `score` / `putaway` (unregistered) | forwarded: scoreboard on/off, put away a screen | done | `games/shooters_edge/net/game_commands.hpp` |
| cmd.ignoretext | `ignoretext [match ...]` | adds chat ignore patterns, or lists them with hit counts | done | `engine/console/chat_ignore.cpp` |
| cmd.unignoretext | `unignoretext [all/match ...]` | removes chat ignore patterns | done | `engine/console/chat_ignore.cpp` |
| cmd.ignorenick | `ignorenick [nick]` | ignores chat from a nickname | done | `engine/console/chat_ignore.cpp` |
| cmd.unignorenick | `unignorenick [all/nick]` | stops ignoring a nickname | done | `engine/console/chat_ignore.cpp` |
| cmd.demo | `demo [/]<file[.ext]>` | plays `.dm2`, `.dm2.gz`, `.mvd2`, `.mvd2.gz` from `demos/`, extension optional, Tab completes | missing | Missing, rank 2 (a Quake II name for a setting ours has; the settings lift): ours `playdemo` and the demo page (`client/demo_playback.cpp`); the `demo` spelling and the `demos/`-relative completion not found |
| cmd.record | `record [-hzes] <file>` | records a client demo; while recording prints statistics; `-z` gzip, `-e`/`-s` packet size | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `client/listen_record.cpp`, `net/q2_client.cpp`; switches and statistics missing (C38) |
| cmd.stop | `stop` | stops recording, prints statistics | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `client/listen_record.cpp`; no statistics (C38) |
| cmd.suspend | `suspend` | pauses (or toggles, `cl_demosuspendtoggle`) recording | not-taken | `client-gaps.md` round 27: not taken (C38) |
| cmd.resume | `resume` | resumes a suspended recording | not-taken | `client-gaps.md` round 27: not taken |
| cmd.seek | `seek [+-]<time/percent>[%]` | jumps in a playing demo by frames, seconds, mm:ss.ff or percent of the file | done | synonym of `demo_jump` taking Quake II's forms (`plugins/shooter/demo_transport.cpp`): S, S.F in tenths, M:S, M:S.F, + and -, and a percent of the demo's length |
| cmd.timerefresh | `timerefresh` | draws 128 frames turning the view and prints "x seconds (y fps)" | done | `games/shooters_edge/client/render_words.cpp` and `world_draw.cpp` (`time_refresh`): 128 frames turning a circle, each shown, or with a word all in one |
| cmd.aashot | `aashot` | writes the scoreboard layout as a text file (ASCII screenshot) | done | `games/shooters_edge/client/q2_bar.cpp` (`hud/q2_layout.cpp` `layout_text`): `scoreshots/quakeNNN.txt` |
| cmd.aadump | `aadump` | prints the scoreboard layout as text in the console | done | `games/shooters_edge/client/q2_bar.cpp` |
| cmd.gtv | `gtv start/stop/status` | relays the player's own game to TV viewers over TCP; only in a build that asks for it (the usual build leaves it out) | not-taken | experimental and off in released builds (a build option, left out of the release packages); watching a match is ours through TV (`engine/tv`, `setv`) and the server's multiview stream (`net/q2_mvd_server.hpp`, `sv_mvd_enable 2`), so a playing client never serves its own game (decided by the owner, 2026-11-23) |
| cmd.sizeup | `sizeup` | screen size up 10 | done | `games/shooters_edge/hud/layout.cpp`: viewsize up 10 |
| cmd.sizedown | `sizedown` | screen size down 10 | done | `games/shooters_edge/hud/layout.cpp`: viewsize down 10 |
| cmd.sky | `sky <name> [speed] [axis]` | changes the sky box and its rotation | done | `games/shooters_edge/client/render_words.cpp` into `classic/sky_typed.hpp`: the box and its turn for the map typed on, its world made again behind the window |
| cmd.draw | `draw <name> <x> <y> [color]` | puts a setting's or macro's value on screen | done | `engine/hud/draw_strings.cpp` |
| cmd.undraw | `undraw <name/all>` | removes a drawn value | done | `engine/hud/draw_strings.cpp` |
| cmd.clearchathud | `clearchathud` | clears the chat lines on screen | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cmd.viewpos | `viewpos` | prints the view's position and yaw | done | `games/shooters_edge/client/place_seen.cpp` |
| cmd.gun_next | `gun_next` | debug: show the next view model | done | `games/shooters_edge/view/hand.cpp` (`add_gun_commands`), drawn by `client/held.cpp`: says "frame n" |
| cmd.gun_prev | `gun_prev` | debug: show the previous view model | done | `games/shooters_edge/view/hand.cpp` (`add_gun_commands`): held at 0, the game's frame again |
| cmd.gun_model | `gun_model <name>` | debug: show a named view model | done | `games/shooters_edge/view/hand.cpp` (`add_gun_commands`): a weapon of the game's own set by its folder; no name the game's again |
| cmd.fog | `fog [r g b density] [sky_factor]` | prints or sets the global fog | done | `plugins/classic/app/look.cpp` |
| cmd.loc | `loc add/del/set/list/save` | edits the map's place names | done | `games/shooters_edge/client/place_names.cpp` |
| cmd.pushmenu | `pushmenu <menu> [args]` | opens a menu page by name | done | `games/shooters_edge/menu/game_menu.cpp`: the game's pages by name, Quake II's names mapped |
| cmd.popmenu | `popmenu` | closes the top menu page | done | `games/shooters_edge/menu/game_menu.cpp` |
| cmd.forcemenuoff | `forcemenuoff` | closes every menu page | done | `games/shooters_edge/menu/game_menu.cpp` |
| cmd.screenshot | `screenshot [format]` | writes a screenshot in `gl_screenshot_format` | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `engine/capture/screenshot.cpp`; `gl_screenshot_format` and `_quality` are synonyms of ours; the file name template missing |
| cmd.screenshotpng | `screenshotpng [file] [compression]` | PNG screenshot | done | `engine/capture/screenshot.cpp`; held by `tests/capture_screenshot_test.cpp` |
| cmd.screenshotjpg | `screenshotjpg [file] [quality]` | JPEG screenshot | done | `engine/capture/screenshot.cpp`; held by `tests/capture_screenshot_test.cpp` |
| cmd.screenshottga | `screenshottga [file]` | TGA screenshot | done | `engine/capture/screenshot.cpp`; our design: written as a PNG (no TGA writer); held by `tests/capture_screenshot_test.cpp` |
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
| cmd.cvarlist | `cvarlist [-achlmnrstuv] [wildcard]` | lists settings by flags | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `engine/console/console_builtins.cpp`; flag switches not compared |
| cmd.toggle | `toggle <cvar> [v1 v2 ...]` | flips 0/1 or cycles listed values | done | `engine/console/script.cpp`: with values, the next after the one matched; "can't cycle" when none is |
| cmd.inc | `inc <cvar> [n]` | adds | done | `engine/console/script.cpp` |
| cmd.dec | `dec <cvar> [n]` | subtracts | done | `engine/console/script.cpp` |
| cmd.reset | `reset <cvar>` | resets to default | done | `engine/console/console_builtins.cpp` |
| cmd.resetall | `resetall` | resets every setting | done | `engine/console/console_more.cpp`: read-only settings and `game` kept |
| cmd.quit | `quit [reason]` | exits; a hosted game's players told "Server quit: <reason>" | done | `games/shooters_edge/client/main.cpp` (`listen_farewell`, `client/listen.cpp`) |
| cmd.recycle | `recycle [reason]` | exits like `quit`, a hosted game's players told "Server restarted" and to reconnect (nothing restarts in place) | done | `games/shooters_edge/client/main.cpp` over the match's own farewell (`listen_farewell`) |
| cmd.lasterror | `lasterror` | prints the last error | done | `engine/console/console_more.cpp` |
| cmd.path | `path` | prints the search path | done | `engine/vfs/search_commands.cpp`: each source, the winner first, with its count of files |
| cmd.dir | `dir <path> [ext]` | lists files in the search path | done | `engine/vfs/search_commands.cpp` |
| cmd.fdir | `fdir <wildcard>` | lists files by wildcard | done | `engine/vfs/search_commands.cpp` |
| cmd.whereis | `whereis <path> [all]` | says which pack or folder a file comes from | done | `engine/vfs/search_commands.cpp`: the winner, and with `all` every source holding it |
| cmd.fs_stats | `fs_stats` | file-system counters | not-taken | `client-gaps.md`: developer tools left out |
| cmd.link | `link <from> <to>` | links one path to another | done | `games/shooters_edge/client/game_folder.cpp` (`add_path_command`) over `classic/content/fs_words.hpp`: a hard link made or moved, none listed |
| cmd.unlink | `unlink <from>` | removes a link | done | `games/shooters_edge/client/game_folder.cpp` over `classic/content/fs_words.hpp`: one link, or `-a` every one |
| cmd.softlink | `softlink <name> <target>` | a soft link in the search path | done | `games/shooters_edge/client/game_folder.cpp` over `classic/content/fs_words.hpp`: tried only when the path itself is missing |
| cmd.softunlink | `softunlink [-ah] <name>` | removes a soft link | done | `games/shooters_edge/client/game_folder.cpp` over `classic/content/fs_words.hpp`: `-a`, `-h` as upstream |
| cmd.fs_restart | `fs_restart` | restarts the file system | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/game_folder.cpp`: the banner and the search path listed, the links kept; the folders are not opened again in the window (the dedicated server does, net/q2_game.cpp) |
| cmd.bsplist | `bsplist` | lists loaded maps | not-taken | `client-gaps.md`: developer tools left out |
| cmd.z_stats | `z_stats` | memory statistics | not-taken | `client-gaps.md`: developer tools left out |
| cmd.net_restart | `net_restart` | restarts networking | done | a match hosted in the window opened again on `sv_port`, the last good port when that fails (`listen_net_restart`, `client/listen.cpp`, over `engine/server/net_restart`); a joined game's own link is left alone |
| cmd.net_stats | `net_stats` | prints network counters | done | `engine/net/net_commands.cpp`: packets and bytes each way since the socket opened (`engine/net/transport.cpp` counts them) |
| cmd.showip | `showip` | prints the local addresses | done | `engine/net/net_commands.cpp` (`add_client_showip`): the host name and addresses, then "Client UDP socket bound to <address>" while joined |
| cmd.dns | `dns <name>` | resolves a host name | done | `engine/net/net_commands.cpp` |
| cmd.tests | `error`, `errordrop`, `freeze`, `crash`, `doublefree`, `printjunk`, `bsptest`, `wildtest`, `normtest`, `infotest`, `snprintftest`, `modeltest`, `imagetest`, `soundtest`, `activate`, `mdfourtest`, `mdfoursum`, `extcmptest`, `nextpathtest`, `extract`, `utf8test` | the test build's self-tests and fault triggers | not-taken | `client-gaps.md`: developer tools left out |

## Client settings (cvars)

done 231, missing 9, not-taken 110

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
| cvar.con_font | `con_font` | the console's font picture (default `conchars`) | not-taken | Tail (decided 2026-11-23), cosmetic: the console draws our charsets; a pack picture as its font is not taken. |
| cvar.con_background | `con_background` | the console's backdrop picture (default `conback`) | not-taken | Tail (decided 2026-11-23), cosmetic: quake II's `con_background` (default conback): a picture under `pics/` (tga, png, jpg or pcx) drawn behind the console from the next frame, read once (`menu/classic_console.hpp`'s `background`, `client/picture_files.hpp`'s `read_console_back`); a missing one says so and the game's own is drawn, but the setting is not put back to conback and a leading slash (a path from the root) is refused |
| cvar.con_scroll | `con_scroll` | bit sum: a new line or a key press scrolls the console to the bottom (default 0) | done | ours the engine's `con_scroll` (`engine/console/view.hpp`): scrolled back, 1 a new line and 2 a key typed bring the newest lines back |
| cvar.con_history | `con_history` | lines of typed history kept (default 128) | done | `engine/console/view.cpp` (M18k4 round 35) |
| cvar.con_timestamps | `con_timestamps` | stamps every console line with the time (default 0) | done | Quake II's own (`client/cvar_layers.cpp`, over the classic layer's chat-only one): every console line keeps the local time beside its text (`console/view.hpp`'s `ViewLine::stamp`), drawn before it on the console, not on the notify lines, written by condump, never searched; ours does not wrap lines, so one stamp a line |
| cvar.con_timestampsformat | `con_timestampsformat` | the stamp's strftime format (default `%H:%M:%S `) | done | `console/view.hpp`'s `stamp_format` (strftime's words, default `%H:%M:%S `; empty: no stamp) |
| cvar.con_timestampscolor | `con_timestampscolor` | the stamp's colour (default `#aaa`) | not-taken | Tail (decided 2026-11-23), cosmetic: `console/view.hpp`'s `stamp_colour`, read by `ui/paint.hpp`'s `colour_from_text` (#rgb, #rrggbb, #rrggbbaa, the eight names or 0..7); a value that does not read draws light grey without the warning and without putting the setting back |
| cvar.con_auto_chat | `con_auto_chat` | a typed line without a slash: 0 a command, 1 say, 2 say_team (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` (M18k4 round 32) |
| cvar.cl_demosnaps | `cl_demosnaps` | seconds between seek snapshots while a demo plays (default 10) | done | Quake II's spelling of `demo_snapshots` (`plugins/shooter/demo_transport.cpp`; the snapshots in `client/timedemo.cpp`) |
| cvar.cl_demomsglen | `cl_demomsglen` | the largest message a recorded demo frame holds (default 1390) | not-taken | `client-gaps.md` (C38): not taken |
| cvar.cl_demowait | `cl_demowait` | 1 holds a finished demo on its last frame (default 0) | done | `games/shooters_edge/client/demo_playback.cpp` |
| cvar.cl_demosuspendtoggle | `cl_demosuspendtoggle` | `suspend` toggles rather than only pauses (default 1) | not-taken | `client-gaps.md` round 27: `suspend` not taken |
| cvar.cl_http_downloads | `cl_http_downloads` | fetch files over HTTP when the server names a URL (default 1) | done | Quake II's layer (`client/q2_session.cpp`) over `net/q2_http` and `engine/net/http_fetch` (#1305) |
| cvar.cl_http_filelists | `cl_http_filelists` | read a `.filelist` of extra files for a map or mod (default 1) | done | `net/q2_http`: `.filelist` files followed, packs first (#1305) |
| cvar.cl_http_max_connections | `cl_http_max_connections` | parallel HTTP fetches (default 2) | done | `net/q2_http`: up to that many files at once (#1305) |
| cvar.cl_http_proxy | `cl_http_proxy` | an HTTP proxy for downloads (default empty) | done | `engine/net/http_fetch`'s proxy (#1305) |
| cvar.cl_http_default_url | `cl_http_default_url` | a download URL used when the server names none (default empty) | done | `net/q2_http`: the address when the server names none (#1305) |
| cvar.cl_http_insecure | `cl_http_insecure` | 1 skips TLS certificate checks (default 0) | done | the classic layer's (`net/qw_remote.cpp`): a secure address's certificate unchecked (`classic/content/web_downloads.hpp`, `net/tls_stream.hpp`, #1309) |
| cvar.cl_http_debug | `cl_http_debug` | prints HTTP transfer details (default 0) | done | `net/q2_http`: each download's address said as it starts (#1305) |
| cvar.cl_http_blocking_timeout | `cl_http_blocking_timeout` | seconds before a stalled HTTP fetch is dropped (default 15) | done | the stall timeout of a web list of servers (`menu/servers_page.hpp`, #1305) |
| cvar.in_enable | `in_enable` | use the input layer at all (default 1) | done | Same meaning in our words (the player-text rule makes our words the intended ones). quake II's (`client/q2_session.hpp`'s `mouse`): 0 never takes the mouse, so it never turns the view, the keys working either way; "Mouse input disabled." is not printed |
| cvar.in_grab | `in_grab` | 0 never grab the mouse in a window, 1 in a game, 2 also let go in demos and chase (default 1) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/q2_session.hpp` (`q2_takes_mouse`): 0 takes the mouse only while the game covers the screen; 2 is the same as 1 (a watched demo lets the mouse go in both, by our `in_mouse_release`) |
| cvar.cl_nodelta | `cl_nodelta` | 1 asks the server for full frames, never deltas (default 0) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/q2_session.cpp`; `net/q2_client.hpp`'s `nodelta` acknowledges no frame, so each comes whole; read when you join, not live |
| cvar.cl_maxpackets | `cl_maxpackets` | packets a second on the modern protocols, moves batched (default 30) | done | `client/q2_session.cpp`; net/q2_client.cpp holds batched moves to the gap (1000/value ms, rounded down to a whole share of 100 ms; under 10 taken as 10; 0 no cap); reliable data, voice and `cl_instantpacket` send at once |
| cvar.cl_fuzzhack | `cl_fuzzhack` | sends moves with a small jitter to beat a server's speed check (default 0) | not-taken | a ping trick: packets held back (at most two moves in a row) with the sequence moved on, so the server reads loss and takes the moves from the next packet's backups; ours sends every packet it builds |
| cvar.cl_packetdup | `cl_packetdup` | each move sent again in the next packet (default 1) | done | `client/q2_session.cpp`; net/q2_client.cpp: 0..3 earlier packets' moves again in each batched packet |
| cvar.cl_showpackets | `cl_showpackets` | prints each move packet sent (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` over the channel's trace (`plugins/classic/protocol/channel.hpp`'s `sent`): each packet sent printed with its sequences and size |
| cvar.cl_instantpacket | `cl_instantpacket` | a button press sent at once (default 1) | done | `client/q2_session.cpp`; net/q2_client.cpp: attack or use just pressed sends the packet past the cap |
| cvar.cl_batchcmds | `cl_batchcmds` | batch several moves a packet (default 1) | done | `client/q2_session.cpp`; net/q2_client.cpp: 36's moves as clc 10/11 (net/q2_net_ext.hpp), else clc 2 without its checksum |
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
| cvar.m_side | `m_side` | sideways move per horizontal mouse count while strafing (default 1) | done | `plugins/shooter/controls.cpp`'s setting, declared `same` with Quake II's default 1 (`games/shooters_edge/client/cvar_layers.cpp`) |
| cvar.m_filter | `m_filter` | average this frame's and last frame's mouse move (default 0) | done | `plugins/shooter/mouse_feel.cpp` |
| cvar.m_accel | `m_accel` | mouse acceleration (default 0) | done | `plugins/shooter/mouse_feel.cpp` (Quake II's meaning, `client-gaps.md` round 6) |
| cvar.m_autosens | `m_autosens` | scale the mouse by the field of view (default 0) | done | `plugins/shooter/controls.cpp` |
| cvar.loc_trace | `loc_trace` | 1 picks the nearest place that can be seen (default 0) | done | `games/shooters_edge/client/place_names.cpp` |
| cvar.loc_draw | `loc_draw` | draws each place point as a marker (default 0) | done | `games/shooters_edge/client/place_names.cpp` (`place_markers`), drawn by `client/world_fx.cpp`; every game (Quake's marker the player's head) |
| cvar.loc_dist | `loc_dist` | the farthest a place counts, units (default 500) | done | `games/shooters_edge/client/place_names.cpp` |
| cvar.cl_gun | `cl_gun` | 0 hides the held gun, 2 right, 3 left (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gunalpha | `cl_gunalpha` | the held gun's opacity (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` into `view/hand.hpp`'s `alpha`, the hand's opacity (`view/hand.cpp`), drawn see-through by the view model pass |
| cvar.cl_gunfov | `cl_gunfov` | the gun's own field of view (default 90) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_x | `cl_gun_x` | moves the gun right, units (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_y | `cl_gun_y` | moves the gun forward (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_gun_z | `cl_gun_z` | moves the gun up (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_footsteps | `cl_footsteps` | 0 no footsteps (and tells the server, on the modern protocol), 1 each surface's own step set when one is found, 2 always the default step sound (default 1) | done | `plugins/classic/q2_footsteps.hpp` (the level's `.mat` materials and step sets, the surface under the stepper), heard in `games/shooters_edge/client/demo_sound.cpp` (`step_sound`), joined, hosted and in demos: 0 starts no `player/step` sound and, on protocol 36, tells the server (client setting 11, at each `begin` and when it changes: `net/q2_client.cpp`'s `q2_client_settings`); 1 each surface's own set; 2 always the default set (M18v, sound slice) |
| cvar.cl_noskins | `cl_noskins` | 1 every player the stock male, 2 female figures the stock female (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_predict | `cl_predict` | predict the player's own move (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_kickangles | `cl_kickangles` | 0 drops the view kick the server sends; cheat-protected (default 1) | done | `games/shooters_edge/client/q2_view.cpp` (M18k4 round 34) |
| cvar.cl_warn_on_fps_rounding | `cl_warn_on_fps_rounding` | warns when `cl_maxfps` will be rounded to whole milliseconds (default 1) | done | `client/q2_session.cpp` (#1282): "cl_maxfps value N is inexact and will be rounded to M.", the hint the first time |
| cvar.cl_maxfps | `cl_maxfps` | moves (packets) a second on the asynchronous client, held between 10 and 125 (default 62) | done | `games/shooters_edge/net/q2_remote.cpp` (`own` row, `client-gaps.md` round 12); verify the 125 cap |
| cvar.cl_async | `cl_async` | 0 one rate for moves and frames, 1 frames drawn apart from the move rate, 2 the display's buffer swaps pace the drawing (needs a newer OpenGL) (default 1) | not-taken | Our design: ours is always asynchronous; no setting (`client-gaps.md` round 12) |
| cvar.r_maxfps | `r_maxfps` | caps drawn frames a second, 0 none (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_autopause | `cl_autopause` | pause single player and demos while the console or a menu is up (default 1) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `client/q2_session.cpp`, a game played alone (M18k4 round 35); a demo is not held under the menu |
| cvar.cl_rollhack | `cl_rollhack` | other players' models lean the classic renderer's (wrong) way (default 1) | done | Quake II's: 1 turns every other player's figure's roll the other way (`client/world_models.cpp`), never the player's own |
| cvar.cl_noglow | `cl_noglow` | 1 stops items' light pulsing (default 0) | done | `client/cvar_layers.cpp`, over `r_itempulse` (M18k4 round 35) |
| cvar.cl_nobob | `cl_nobob` | 1 stops the view bobbing (default 0) | done | Quake II's `cl_nobob` (default 0): items the game marks to float (the bob bit) lie still (`client/world_models.cpp`, the picture's `items_still`); the view's bob untouched |
| cvar.cl_nolerp | `cl_nolerp` | 1 draws entities at the newest frame without interpolation (default 0) | done | Checked 2026-11-23: `cl_nolerp` is bound in the remote both games join through (`net/qw_remote.cpp`), so a Quake II server joined reads it. |
| cvar.cl_shownet | `cl_shownet` | prints received messages: 1 sizes, 2 kinds, 3 entities (default 0) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `net/q2_client.cpp`'s `read_payload` prints each message's size at 1 and above; the kinds (2) and entities (3) not listed |
| cvar.cl_showmiss | `cl_showmiss` | prints each prediction miss (default 0) | done | `net/q2_predict.cpp` counts a miss (a new frame puts the newest move elsewhere than the last base did) and `net/q2_remote.cpp` prints "prediction miss on <frame>: <units>"; bound in `client/cvar_layers.cpp` |
| cvar.showclamp | `showclamp` | prints when the client's time is clamped to the frames it has (default 0) | not-taken | a debug build's report only (absent from a release); ours keeps no settings that only a debug build has |
| cvar.cl_timeout | `cl_timeout` | seconds of silence before the connection is dropped (default 120) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.rcon_address | `rcon_address` | where `rcon` goes when not connected (default empty) | done | `games/shooters_edge/net/qw_remote.cpp` |
| cvar.cl_thirdperson | `cl_thirdperson` | draws the view from behind the player; the camera stops at walls and at moving doors and platforms (traced where they are drawn), and the player's own model fades as the camera is pushed close; cheat-protected (default 0) | done | `games/shooters_edge/client/q2_session.cpp` (`view_behind`, a cheat; joined and hosted): stops at the world's walls and at the map's doors and platforms where they were last drawn (`client/world_draw.hpp`'s `world_movers`); the player's own figure fades as the camera is pushed in (`own_figure_alpha`: solid from 60 units, gone within 20) |
| cvar.cl_thirdperson_angle | `cl_thirdperson_angle` | the third-person camera's angle round the player (default 0) | done | `games/shooters_edge/client/q2_session.cpp` |
| cvar.cl_thirdperson_range | `cl_thirdperson_range` | the third-person camera's distance (default 60) | done | `games/shooters_edge/client/q2_session.cpp` |
| cvar.cl_disable_particles | `cl_disable_particles` | bit sum: 1 grenade blast, 2 grenade trail, 4 rocket blast, 8 rocket trail, 16 blood, 32 blaster trail particles off (default 0) | done | `plugins/classic/q2_fx_switches.hpp`, applied in `games/shooters_edge/client/world_fx.cpp` |
| cvar.cl_disable_explosions | `cl_disable_explosions` | bit sum: 1 grenade, 2 rocket explosion models off (default 0) | done | `plugins/classic/q2_fx_switches.hpp`: the ball of fire left out, its light kept (`client/world_fx.cpp`) |
| cvar.cl_dlight_hacks | `cl_dlight_hacks` | bit sum: 1 rocket light recoloured, 2 smaller blast lights, 4 no muzzle light (default 0) | not-taken | Tail (decided 2026-11-23), cosmetic: `plugins/classic/q2_fx_switches.hpp`: 2 and 4 taken; 1 (a rocket's light recoloured) not, its colour unread |
| cvar.cl_smooth_explosions | `cl_smooth_explosions` | explosions fade smoothly over their whole life, which also fixes some explosions' length (default 1) | done | Quake II's (`classic/q2_fx_switches.hpp`'s q2_blast_age): 1 (the default) the explosions' balls and lights fade smoothly as ours always did; 0 they move on by whole tenths of a second, the game's frames (`client/world_fx.cpp`) |
| cvar.cl_lerp_lightstyles | `cl_lerp_lightstyles` | 1 blends light styles between their letters instead of stepping (default 0) | done | Quake II's: 1 blends each light style linearly from letter to letter (`brush/app/draw_data.hpp`'s style_level_lerped), 0 steps them ten a second |
| cvar.cl_gibs | `cl_gibs` | 0 draws no gibs and tells the server (default 1) | done | `client/cvar_layers.cpp`, our `cl_gibfilter` the other way round (M18k4 round 35); the server is told nothing on protocol 34 |
| cvar.cl_flares | `cl_flares` | 0 hides the flares the game sends (extended protocol) (default 1) | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's flares |
| cvar.cl_updaterate | `cl_updaterate` | asks the server for this many frames a second (extended protocol) (default 0) | not-taken | Our design: `engine/net/net_cvars.cpp` is our own protocol's; not sent to a Quake II server |
| cvar.cl_chat_notify | `cl_chat_notify` | 0 keeps chat out of the notify lines (default 1) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_chat_sound | `cl_chat_sound` | 0 none, 1 `misc/talk.wav`, 2 `misc/talk1.wav`, or a sound's name (default 1) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_chat_filter | `cl_chat_filter` | 1 strips unprintable characters from chat (default 0) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.cl_disconnectcmd | `cl_disconnectcmd` | a command run at each disconnect (default empty) | done | `client/q2_session.cpp` (M18k4 round 35) |
| cvar.cl_changemapcmd | `cl_changemapcmd` | a command run at each map change (default empty) | done | `client/q2_session.cpp` (M18k4 round 35) |
| cvar.cl_beginmapcmd | `cl_beginmapcmd` | a command run on entering each map (default empty) | done | `client/q2_session.cpp` (M18k4 round 35) |
| cvar.cl_ignore_stufftext | `cl_ignore_stufftext` | 1 refuses server-pushed commands not on a whitelist file (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` over `plugins/classic/protocol/stuff_filter`: 1 runs only what `stufftext-whitelist.txt` names, 2 says each refusal too |
| cvar.cl_allow_vid_restart | `cl_allow_vid_restart` | 1 lets a typed `vid_restart` run (default 0) | not-taken | ours applies every video setting the moment it changes, so a typed `vid_restart` is always refused as Quake II's 0 refuses it ("Manual 'vid_restart' command ignored." and the reason once, `client/window_cvars.cpp`); a server's is dropped quietly |
| cvar.cl_protocol | `cl_protocol` | 0 the best the server offers; 34, 35, 36 forced (default 0) | done | `client/q2_session.cpp`; net/q2_client.cpp `choose_protocol`: 0 is 36; 36 and 35 taken when the challenge's `p=` offers them, falling to the next one down; else 34 |
| cvar.gender_auto | `gender_auto` | set `gender` from the skin's model (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.cl_vwep | `cl_vwep` | draw other players' weapons (default 1) | done | synonym of `r_drawvweps` (`client/cvar_layers.cpp`) |
| cvar.password | `password` | user info: the join password | done | `games/shooters_edge/net/q2_client.cpp` |
| cvar.spectator | `spectator` | user info: join as spectator (or its password) | done | `games/shooters_edge/net/q2_client.cpp` |
| cvar.name | `name` | user info: the player's name (default `unnamed`) | missing | Missing, rank 1 (gameplay, daily use): `net/q2_client.cpp` sends it at the connect only; a change while joined not sent (C30) LIFT: Quake's remote already resends `setinfo` when rate or password moves (`net/qw_remote.cpp`, `user_info_step`); extend it there for both games. |
| cvar.skin | `skin` | user info: model/skin (default `male/grunt`) | missing | Missing, rank 1 (gameplay, daily use): sent at the connect only (C30) LIFT: same `user_info_step` as `name`. |
| cvar.rate | `rate` | user info: bytes a second the server may send (default 15000) | done | `plugins/classic` shared user info; a change while joined resends it (`q2_client_userinfo`) |
| cvar.msg | `msg` | user info: the lowest print level wanted (default 1) | missing | Missing, rank 1 (gameplay, daily use): always sent as 1 (C30) LIFT: same `user_info_step` as `name`; Quake's server already honours `msg`. |
| cvar.hand | `hand` | user info: 0 right, 1 left, 2 centre (default 0) | missing | Missing, rank 1 (gameplay, daily use): `client/cvar_layers.cpp`; sent at the connect only (C30) LIFT: same `user_info_step` as `name`. |
| cvar.fov | `fov` | user info: the field of view (default 90) | missing | Missing, rank 1 (gameplay, daily use): synonym of `cl_fov`; the field the server sends back is not drawn (C3), sent at the connect only (C30) LIFT: same `user_info_step` as `name`. |
| cvar.gender | `gender` | user info: male, female, none (default male) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.uf | `uf` | user info: flags (4 own FOV in demos, 32 recorded FOV, others for the server) (default empty) | done | flags 4 and 32 read for recordings, `client/q2_view.cpp` (M18k4 round 34); not sent in the user info |
| cvar.version | `version` | the program's name and version, read-only; part of a hosted server's info, not of the user info | not-taken | Our design: ours sends a version in the user info (`games/shooters_edge/net/q2_client.cpp`), which the original does not |
| cvar.vid_ref | `vid_ref` | read-only: the renderer (gl) | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.vid_driver | `vid_driver` | the video driver to use | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.vid_fullscreen | `vid_fullscreen` | 0 windowed, else the mode number from `vid_modelist` (default 0) | done | the engine's `engine/platform/window_cvars.hpp`, every game: 0 a window, N the whole screen in mode N of `vid_modelist` (`desktop` the desktop's own), the display's closest mode (`set_fullscreen`) |
| cvar._vid_fullscreen | `_vid_fullscreen` | the mode Alt+Enter goes back to (default 1) | done | `engine/platform/window_cvars.hpp`: archived, kept from the last `vid_fullscreen` above 0; Alt+Enter goes back to it (`alt_enter`) |
| cvar.vid_geometry | `vid_geometry` | the window's size and place, `WxH+X+Y` (default 640x480) | done | ours the engine's `vid_geometry` (`engine/platform/window_cvars.hpp`): `WxH`, `WxH+X+Y` or `+X+Y`; its default "" keeps the window the game opens (not 640x480) |
| cvar.vid_modelist | `vid_modelist` | the full-screen modes offered (default `modelist`, the desktop's) | done | `engine/platform/window_cvars.hpp` (`full_mode_of`): `desktop`, WxH, WxH@Hz, WxH@Hz:bpp, WxH:bpp@Hz; the default `desktop` and the display's own modes (`display_modes`); a bad entry is the desktop's mode, said as a debug line; a new list takes effect at once while full-screen |
| cvar.vid_hwgamma | `vid_hwgamma` | gamma by the display hardware (default 0) | done | `engine/platform/window_cvars.hpp`: the window's platform layer has no gamma ramp, so 1 prints "...hardware gamma not supported" and is set back to 0, as on a display without one; `vid_gamma` stays in the picture |
| cvar.vid_flip_on_switch | `vid_flip_on_switch` | Windows: switch to the desktop mode when the window loses focus (default 0) | done | Quake II's spelling of the engine's `vid_minimize_on_focus_loss` (`platform/window_cvars.hpp`, every platform), its default 0 (`client/cvar_layers.cpp`) |
| cvar.viewsize | `viewsize` | the 3D view's size 40..100 percent, border tiled (default 100) | done | Quake II's own (`client/q2_view.cpp`, `own` over the HUD's 100..120): 40..100, the world drawn in `q2_view_rect`'s centred rectangle by the post pass (`render/post.hpp`'s `set_post_view`, from `client/world_draw.cpp`); `sizeup` and `sizedown` step it by 10 in its range |
| cvar.scr_showpause | `scr_showpause` | 1 draws the pause picture, 2 "[PAUSED]" on the demo bar (default 1) | not-taken | Tail (decided 2026-11-23), cosmetic: a synonym of our `showpause` (`games/shooters_edge/client/cvar_layers.cpp`): 2's "[PAUSED]" on the demo bar is not taken |
| cvar.scr_centertime | `scr_centertime` | seconds a centre print stays, faded 300 ms (default 2.5) | not-taken | Tail (decided 2026-11-23), cosmetic: `client/cvar_layers.cpp`; no fade, line cut differs (C9) |
| cvar.scr_printspeed | `scr_printspeed` | letters a second of a typewriter centre print (extended protocol) (default 16) | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's typewriter centre print |
| cvar.scr_demobar | `scr_demobar` | 1 a progress bar while a demo plays, 2 also the time (default 1) | done | Quake II's own setting over the play bar: 0 none, 1 always with the share played, 2 also the clock; stands in for `demo_bar` while Quake II is loaded (`games/shooters_edge/client/q2_view.cpp`, `demo_playback.cpp`; the bar's look is ours) |
| cvar.scr_font | `scr_font` | the HUD's font picture (default `conchars`) | done | `client/q2_view.cpp` (Q2Screen::font), drawn by `client/q2_bar.cpp`'s `font_picture`: the status bar's and the layouts' text, the inventory and the centre prints (hosted, joined and recorded: `draw_recorded_q2_bar`'s centre) in pics/<name>.pcx at once, and the chat lines on screen in the same letters (`q2_font_image`); a missing one prints "Couldn't load pics/<name>.pcx: not found" and is set back to conchars |
| cvar.scr_scale | `scr_scale` | the 2D scale, 0 auto by window height (default 0) | done | `games/shooters_edge/client/q2_bar.cpp` `q2_bar_scale` (M18k4 round 34) |
| cvar.crosshair | `crosshair` | 0 none, N the `chN` picture (default 0) | done | `engine/hud/crosshair.hpp` (Quake II's layer `same`) |
| cvar.netgraph | `netgraph` | 1 latency bars, 2 packet sizes (default 0) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: a synonym of our `r_netgraph` (`games/shooters_edge/client/cvar_layers.cpp`): 2's packet sizes are not drawn |
| cvar.timegraph | `timegraph` | a frame-time graph (default 0) | done | `games/shooters_edge/client/q2_bar.cpp` (`draw_graph`) over `engine/hud/bottom_graph.hpp`: a column a frame along the bottom, a unit tall a millisecond |
| cvar.debuggraph | `debuggraph` | a development graph (default 0) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/q2_bar.cpp` (`draw_graph`) over `engine/hud/bottom_graph.hpp`: the graph drawn; ours feeds it no development values of its own |
| cvar.graphheight | `graphheight` | the graph's height (default 32) | done | `games/shooters_edge/client/q2_bar.cpp` (`draw_graph`) over `engine/hud/bottom_graph.hpp` |
| cvar.graphscale | `graphscale` | the graph's vertical scale (default 1) | done | `games/shooters_edge/client/q2_bar.cpp` (`draw_graph`) over `engine/hud/bottom_graph.hpp` |
| cvar.graphshift | `graphshift` | the graph's vertical offset (default 0) | done | `games/shooters_edge/client/q2_bar.cpp` (`draw_graph`) over `engine/hud/bottom_graph.hpp` |
| cvar.scr_chathud | `scr_chathud` | 0 off, 1 last chat lines on screen, 2 in the alternate colour (default 0) | done | `games/shooters_edge/client/chat_lines.cpp` (M18k4 round 32) |
| cvar.scr_chathud_lines | `scr_chathud_lines` | chat lines shown (default 4) | done | (M18k4 round 32) |
| cvar.scr_chathud_time | `scr_chathud_time` | seconds a chat line stays, 0 for ever (default 0) | done | (M18k4 round 32) |
| cvar.scr_chathud_x | `scr_chathud_x` | the chat lines' x (default 8) | missing | Missing, rank 2 (found by the verification sweep, 2026-11-23: no such setting is registered; the chat lines' place is our `hud_chat_x`/`hud_chat_y` in shares of the screen, so this needs Quake II's pixels mapped onto them) |
| cvar.scr_chathud_y | `scr_chathud_y` | the chat lines' y, negative from the bottom (default -64) | missing | Missing, rank 2 (found by the verification sweep, 2026-11-23: no such setting is registered; the chat lines' place is our `hud_chat_x`/`hud_chat_y` in shares of the screen, so this needs Quake II's pixels mapped onto them) |
| cvar.ch_health | `ch_health` | crosshair coloured by health: red full, green rising from 33 to 66, blue from 66 to 99, black when dead (default 0) | done | `engine/hud/crosshair.cpp`'s `crosshair_health` (red, green from 33 to 66, blue from 66 to 99, black when dead), bound in `client/cvar_layers.cpp` |
| cvar.ch_red | `ch_red` | crosshair red (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_green | `ch_green` | crosshair green (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_blue | `ch_blue` | crosshair blue (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.ch_alpha | `ch_alpha` | crosshair opacity (default 1) | done | synonym of `crosshairalpha` (`client/cvar_layers.cpp`) |
| cvar.ch_scale | `ch_scale` | crosshair picture scale, 0.1 to 9 (default 1) | done | synonym of `crosshairsize` (`client/cvar_layers.cpp`); verify the 0.1 to 9 clamp |
| cvar.ch_x | `ch_x` | crosshair x offset (default 0) | done | synonym of `cl_crossx` (`client/cvar_layers.cpp`) |
| cvar.ch_y | `ch_y` | crosshair y offset (default 0) | done | synonym of `cl_crossy` (`client/cvar_layers.cpp`) |
| cvar.scr_draw2d | `scr_draw2d` | 0 no 2D, 1 no status bar, 2 all, 3 layouts only with F1 (default 2) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: 0, 1, 2 and 3 (M18k4 round 34); F1 held does not bring the layouts back at 3 |
| cvar.scr_showturtle | `scr_showturtle` | network trouble words, 2 more of them (default 1) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/q2_bar.cpp`'s `add_q2_turtle` on `DemoState::q2_trouble` (`client/demo_q2.cpp`): BADFRAME, OLDFRAME, NODELTA, and SUPPRESSED at 2; CLIENTPRED, OLDENT, CLIENTDROP and SERVERDROP not told yet |
| cvar.scr_lag_x | `scr_lag_x` | the lag graph's x (default -1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_lag_y | `scr_lag_y` | the lag graph's y (default -1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_lag_draw | `scr_lag_draw` | 1 the lag graph, 2 on a dark box (default 0) | done | `engine/hud/net_graph.cpp` `draw_lag_graph` (M18k4 round 34) |
| cvar.scr_lag_min | `scr_lag_min` | the lag graph's bottom, ms (default 0) | done | (M18k4 round 34) |
| cvar.scr_lag_max | `scr_lag_max` | the lag graph's top, ms (default 200) | done | (M18k4 round 34) |
| cvar.scr_alpha | `scr_alpha` | the opacity of every 2D element but the crosshair (default 1) | done | `client/q2_bar.cpp` (M18k4 round 34) |
| cvar.scr_showstats | `scr_showstats` | debug: prints the player's stats on screen (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.scr_showpmove | `scr_showpmove` | debug: the move state on screen (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.scr_hit_marker_time | `scr_hit_marker_time` | ms a hit marker shows (default 500) | done | `plugins/classic/q2_fx_switches.hpp` (`q2_hit_marker_alpha`) |
| cvar.cl_muzzleflashes | `cl_muzzleflashes` | 0 hides the newer game's muzzle flash models (the flash lights stay) (default 1) | not-taken | Tail (decided 2026-11-23), cosmetic: `plugins/classic/q2_fx_switches.hpp`: others' and monsters' gun fire; the held weapon's own flash still drawn |
| cvar.cl_muzzlelight_time | `cl_muzzlelight_time` | how long a muzzle flash light lasts, 0 to 1000 ms (default 16) | done | `plugins/classic/q2_fx_switches.hpp`: our flash light's fade stretched by it (16 as drawn) |
| cvar.cl_hit_markers | `cl_hit_markers` | 0 off, 1 a hit marker, 2 a marker and a sound (default 2) | not-taken | Tail (decided 2026-11-23), cosmetic: `plugins/classic/q2_fx_switches.hpp`, read from temporary entity 128 (`games/shooters_edge/client/demo_q2.cpp`), drawn by `client/q2_bar.cpp`'s `draw_q2_hit_marker` and heard in `client/demo_sound.cpp`, joined and in recordings: drawn whether or not a crosshair is up, unscaled by `ch_scale` and unmoved by `ch_x`/`ch_y`; the hosted game's rules send no report yet |
| cvar.cl_railtrail_type | `cl_railtrail_type` | 0 classic, 1 core beam, 2 core and spiral (default 0) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_railtrail_time | `cl_railtrail_time` | seconds the rail trail lasts (default 1.0) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_railcore_color | `cl_railcore_color` | the rail core's colour (default red) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_railcore_width | `cl_railcore_width` | the rail core's width (default 2) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_railspiral_color | `cl_railspiral_color` | the rail spiral's colour (default blue) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_railspiral_radius | `cl_railspiral_radius` | the rail spiral's radius (default 3) | done | `plugins/classic/q2_fx_switches.hpp`, drawn in either look by `games/shooters_edge/client/world_fx.cpp` (`q2_rail_trail`) and `app/weapon_fx.cpp` (`q2_rail_trail_look`) |
| cvar.cl_testblend | `cl_testblend` | debug: a fixed test tint (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testparticles | `cl_testparticles` | debug: a field of test particles (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testentities | `cl_testentities` | debug: a ring of test models (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_testlights | `cl_testlights` | debug: test lights; cheat-protected (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_stats | `cl_stats` | debug: prints the view's entity, light and particle counts (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.cl_lights | `cl_lights` | 0 adds no dynamic lights (default 1) | done | a synonym of `r_dynamic` (M18k4 round 35) |
| cvar.cl_particles | `cl_particles` | 0 adds no particles (default 1) | done | a synonym of `r_particles` (M18k4 round 35) |
| cvar.cl_entities | `cl_entities` | 0 adds no entities (default 1) | done | a synonym of `r_drawentities` (M18k4 round 35) |
| cvar.cl_blend | `cl_blend` | 0 drops the full-screen tints (default 1) | done | a synonym of `gl_polyblend` (M18k4 round 35) |
| cvar.cl_adjustfov | `cl_adjustfov` | 1 hor+ on wide screens, 0 vert- (default 1) | done | `games/shooters_edge/client/q2_view.cpp` (M18k4 round 34) |
| cvar.ui_sortdemos | `ui_sortdemos` | the demo browser's sort column, negative descending (default 1) | done | a synonym of ours `menu_demo_sort` (`games/shooters_edge/client/cvar_layers.cpp`) |
| cvar.ui_listalldemos | `ui_listalldemos` | the demo browser lists every folder (default 0) | not-taken | ours browses any folder on the machine for recordings (`menu/demos_page.hpp`), the game's folder among them; a recording inside a pack is not offered |
| cvar.ui_sortservers | `ui_sortservers` | the server browser's sort column (default 0) | done | Quake II's own (`menu/servers_page.hpp`'s `game_sort`, bound by `client/cvar_layers.cpp`) in place of our `sb_sortservers`: 0 the order found, 1 name, 2 game, 3 map, 4 players (the busiest first), 5 ping, minus the other way, ties by name the same way then by address, the silent last; LEFT, RIGHT and D move the sort without writing the setting |
| cvar.ui_colorservers | `ui_colorservers` | colour the server list: grey for a password, anticheat 2 or a no-fake server, green under 30 ms, white otherwise (default 0) | done | a synonym of ours `sb_colorservers` (`games/shooters_edge/client/cvar_layers.cpp`) |
| cvar.ui_pingrate | `ui_pingrate` | server pings a second, 0 from `rate` (default 0) | done | Quake II's own (`menu/servers_page.hpp`'s `game_ping_rate`) in place of our `sb_infospersec`: 0 the player's `rate` / 450 held to 1..100 (33 at 15000), 1..100 that many a second; ours asks at that pace on every try, where its second and third passes slow to two thirds and a third |
| cvar.ui_scale | `ui_scale` | the menus' scale, 0 auto (default 0) | not-taken | Our design: quake II's own (`menu/classic_art.hpp`'s `scale`, kept by `GameMenu::set_art`, bound by `client/cvar_layers.cpp`): 1..10 that many screen px a classic px, the menus centred; 0 fills the window by whole numbers where Quake II steps 1, 2, 4 by its height (or the desktop's DPI) |
| cvar.ui_debug | `ui_debug` | debug: outlines menu items (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.ui_open | `ui_open` | 1 opens the main menu at start-up (default 0) | not-taken | Shooter's Edge's shell opens its own title menu at start-up whatever game is loaded; Quake II's 0, the bare console, would leave a new player at a prompt; the console is one key away |
| cvar.adr | `adr0` .. `adr15` | the address book: 16 saved server addresses (saved) | done | `adr0` .. `adr15` (`games/shooters_edge/client/cvar_layers.cpp`), archived, the address book's rows (`menu/q2_pages.cpp`) |
| cvar.s_khz | `s_khz` | the mixing rate in kHz (default 44, saved) | not-taken | ours mixes at 48 kHz always and the platform converts to whatever the device runs at (`engine/audio/voice.hpp`'s `kRate`), so a sound's bits never depend on the device |
| cvar.s_mixahead | `s_mixahead` | seconds the mixer runs ahead (default 0.1, saved) | done | Quake II's own in `client/cvar_layers.cpp`, saved, for its sound page and configs; nothing reads it, as its OpenAL path never does: the device pulls the sound a chunk at a time (`platform/audio.hpp`) (M18v, sound slice) |
| cvar.s_testsound | `s_testsound` | debug: a test tone (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.s_swapstereo | `s_swapstereo` | swap left and right (default 0) | done | `client/room_ear.cpp` in the classic layer (Quake's too), `engine/audio/synth.cpp` swaps (M18k4 round 35) |
| cvar.s_driver | `s_driver` | the sound driver | not-taken | the platform picks the system's sound driver (SDL's `SDL_AUDIO_DRIVER` names another); `soundinfo` says which |
| cvar.s_enable | `s_enable` | 0 none, 1 software, 2 OpenAL (default 1) | done | the engine's `s_enable` (`platform/window_cvars.hpp`, every game): 0 no sound at all (the heard volume 0), 1 the sound on, 2 the same (its OpenAL mixer is our one mixer) |
| cvar.s_volume | `s_volume` | the effects volume (default 0.7, saved) | done | synonym of `volume` (`client/cvar_layers.cpp`) |
| cvar.s_ambient | `s_ambient` | 0 no loops, 1 all, 2 only loops with a model, 3 only the player's own (default 1) | done | `plugins/classic/q2_fx_switches.hpp` (`ambient`), heard in `games/shooters_edge/client/demo_sound.cpp` (`entity_loops`) |
| cvar.s_show | `s_show` | debug: lists the sounds playing (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.s_auto_focus | `s_auto_focus` | 0 always play, 1 not minimised, 2 only focused (default 0) | done | the engine's `engine/platform/window_cvars.hpp` (`heard_volume`), every game |
| cvar.s_underwater | `s_underwater` | muffle sound under water (default 1) | done | `games/shooters_edge/client/room_ear.cpp` over the synth's muffle (M18k4 round 34) |
| cvar.s_underwater_gain_hf | `s_underwater_gain_hf` | the muffle's treble gain (default 0.25) | done | (M18k4 round 34) |
| cvar.ogg_enable | `ogg_enable` | play background music in any of the supported formats (`.flac`, `.opus`, `.ogg`, `.mp3`, `.wav`) (default 1) | done | Quake II's own, bound to the music's switch (`engine/audio/music_console.hpp`'s `enabled`, `cd on|off`) in `games/shooters_edge/client/cvar_layers.cpp`; on again plays the last track |
| cvar.ogg_volume | `ogg_volume` | the music volume (default 1) | done | a synonym of our `bgmvolume` (`games/shooters_edge/client/cvar_layers.cpp`) |
| cvar.ogg_shuffle | `ogg_shuffle` | shuffle every track in `music/` (default 0) | done | Quake II's own, bound to the music's `shuffle` (`engine/audio/music_console.hpp`: every track the game has, in a random order, each once through) |
| cvar.ogg_menu_track | `ogg_menu_track` | the track played while disconnected, 0 none (default 0) | done | Quake II's own, bound to the music's `menu_track`, started on the title by `play_menu_music` (`engine/audio/music_console.hpp`, `games/shooters_edge/client/main.cpp`) |
| cvar.ogg_remap_tracks | `ogg_remap_tracks` | remap the mission packs' track numbers to the digital release's file names (default 1) | done | `plugins/classic/content/soundtrack.hpp`'s `q2_remap_track` (rogue 2..11 as 12..21, xatrix by its table) in the music's track lookup (`games/shooters_edge/client/take_in.cpp`), bound in `client/q2_session.cpp` |
| cvar.al_device | `al_device` | the OpenAL output device, empty the default; `?` lists the devices ("Available devices:") (default empty) | done | its spelling of the engine's `s_device` (`platform/window_cvars.hpp`, every game): "" the system's default, a name moves the sound there at once (`platform/audio.hpp`'s `move_audio`), `?` lists "Available devices:" and puts the setting back, a name not there is said once and the sound stays; `s_devices` lists them too |
| cvar.al_merge_looping | `al_merge_looping` | OpenAL: merge copies of one looped sound so they play as loud as on the software mixer; 0 off, 1 auto (only in plain stereo), 2 on (default 1) | done | `plugins/classic/q2_loop_merge.hpp`, heard in `client/demo_sound.cpp` (`merged_loops`): the entities carrying one looped sound heard as one voice, each ear's shares summed and held to 1, the gain their total held to 1 and the pan their balance; 0 each its own voice, 1 merged while the output is plain stereo (not under `s_hrtf` 2's two ears), 2 always (M18v, sound slice) |
| cvar.al_hrtf | `al_hrtf` | OpenAL head-related sound (default 0) | done | Quake II's spelling of s_hrtf (`client/room_ear.hpp`), not kept: 0 (the default) off; 1 as the sound device chooses, off, as it names no headphones; 2 on: each sound heard by two ears (`audio/synth.hpp`'s Synth::binaural), the far ear about 0.66 ms later, softer and duller through the head's shadow; the player's own sounds and the music sit in the middle and are heard as before |
| cvar.gl_screenshot_format | `gl_screenshot_format` | jpg, png or tga (default jpg) | done | a synonym of ours `scr_format` (`games/shooters_edge/client/cvar_layers.cpp`), the same default |
| cvar.gl_screenshot_async | `gl_screenshot_async` | write screenshots off the main thread (default 1) | not-taken | ours always packs and writes every format off the game's thread (`engine/capture/screenshot.hpp`), its 2 |
| cvar.gl_screenshot_quality | `gl_screenshot_quality` | JPEG quality (default 90) | done | a synonym of ours `scr_quality`, the same default |
| cvar.gl_screenshot_compression | `gl_screenshot_compression` | PNG compression (default 6) | done | a synonym of ours `scr_compression` (`engine/capture/screenshot.hpp`): 0 stores a png's rows, 1..9 packed; the same default |
| cvar.gl_screenshot_template | `gl_screenshot_template` | the file name, X's numbered (default `quakeXXX`) | done | a synonym of ours `scr_template` (`engine/capture/screenshot.hpp`), declared `same` with Quake II's default `quakeXXX`: the first free number in its X's |
| cvar.r_override_textures | `r_override_textures` | load replacement textures (default 1) | not-taken | Our design: ours replaces by classic enhance (`plugins/classicenhance`), not this switch |
| cvar.r_texture_overrides | `r_texture_overrides` | bit sum of which kinds may be replaced (default -1) | not-taken | replacement pictures, skins and walls come from classic enhance's cache (`plugins/classicenhance`), never loose png, jpg or tga files in the game's folders |
| cvar.r_glowmaps | `r_glowmaps` | load glow maps (default 1) | not-taken | Our design: ours has glow maps from classic enhance; no switch by this name |
| cvar.gl_partscale | `gl_partscale` | particle size (default 2) | done | Quake II's (`classic/q2_fx_switches.hpp`, `client/world_draw.cpp`): each speck's size times `q2_speck_scale`, 1 within 20 units and 1 + 0.004 a unit of its depth past them (5 at 1000), times the setting / 2; a puff of smoke keeps its own; another game's 0 leaves every speck as made |
| cvar.gl_partstyle | `gl_partstyle` | 0 blended, 1 additive particles (default 0) | done | Quake II's: 1 adds the specks' light to what is behind them (`render/particles.hpp`'s ParticleSettings::additive), 0 lays them over it |
| cvar.gl_partshape | `gl_partshape` | 0 soft dot, 1 square, 2 sharp dot (default 0) | done | Quake II's: the specks' shape on their quad (`render/particles.hpp`'s ParticleShape: 0 a soft round dot, 1 a hard square of the middle 5/8 at 60 %, 2 a flat disc at 60 % with a narrow soft rim); another game's the engine's soft capsule |
| cvar.gl_beamstyle | `gl_beamstyle` | 0 flat beams, 1 another look (default 0) | done | Quake II's: 1 draws a laser and the rail trail's core as their core alone, as wide as their width, with no glowing sheath (`app/weapon_fx.hpp`'s round_beam); 0 the soft strip with its glow; the core's own edge stays a pixel soft |
| cvar.gl_celshading | `gl_celshading` | outlines on models, width (default 0) | not-taken | Tail (decided 2026-11-23), cosmetic: quake II's (`classic/q2_fx_switches.hpp`'s q2_outline_px): a black ink line round each solid model (not see-through, no power shell), the setting's pixels (0..10) times 1 - distance / 700, fading with it and gone past about 693 units; drawn as the model's back faces pushed out by half the width (`render/morph.hpp`'s MorphDraw::outline, `client/world_models.cpp`); the held weapon is not outlined |
| cvar.gl_dotshading | `gl_dotshading` | models shaded by precomputed directions (default 1) | done | Quake II's: 0 lights a model evenly (no shade toward its fixed direction, `client/world_models.cpp`), 1 the game's shading from the fixed direction over the map's +x |
| cvar.gl_shadows | `gl_shadows` | 1 blob shadows under models, 2 fading with height (default 0) | done | `client/model_shadow.hpp` (`shadow_of`), drawn by `client/world_models.cpp` as `render/morph.hpp`'s flat shadow (`MorphDraw::flat`): each animated model's black copy squashed a unit above the floor under it at half strength, leaning along its facing; 2 fades it out by four of its radii above the floor; none under sprites, brush models, see-through models, shells, the held weapon, or a floor steeper than sixty degrees; saved |
| cvar.gl_modulate | `gl_modulate` | brightens lightmaps and models (default 1) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_modulate_world | `gl_modulate_world` | the world's share of `gl_modulate` (default 1) | done | Quake II's (`classic/q2_fx_switches.hpp`): times `gl_modulate` on the walls' light alone (`classic/app/look.cpp`'s world_light_scale), either look |
| cvar.gl_coloredlightmaps | `gl_coloredlightmaps` | lightmap saturation (default 1) | not-taken | Tail (decided 2026-11-23), cosmetic: quake II's: the walls' lightmap light moved toward its luminance (0.2126, 0.7152, 0.0722) in the world's shader (`brush/app/bsp.wgsl`'s lightmap), at once; the models' light, read from the lightmaps on the CPU, keeps its colour |
| cvar.gl_brightness | `gl_brightness` | adds to lightmaps (default 0) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_dynamic | `gl_dynamic` | 0 no dynamic lights, 1 all, 2 only switchable styles (default 1) | done | synonym of `r_dynamic` (`client/cvar_layers.cpp`) |
| cvar.gl_draworder | `gl_draworder` | the alpha below which translucent entities are drawn after the rest, sorted back to front (default 1) | not-taken | ours sorts its own see-through passes (the liquids, the glass, the beams and the specks each in their pass, the held weapon always last); no alpha threshold to set |
| cvar.gl_clearcolor | `gl_clearcolor` | the colour behind the world where nothing is drawn (with `gl_clear`) (default black) | not-taken | only read with `gl_clear`, which is not taken: our frame is cleared and every pixel drawn each frame, so no colour shows where nothing is drawn |
| cvar.gl_dlight_falloff | `gl_dlight_falloff` | softer dynamic-light edges (default 1) | done | Quake II's: 1 fades a dynamic light on the walls to nothing 51.2 units short of its reach, 0 cuts it off where less than 64 units are left (`brush/app/bsp_draw.hpp`'s Options::dlight_falloff); another game's straight to its reach |
| cvar.gl_modulate_entities | `gl_modulate_entities` | the models' share of `gl_modulate` (default 1) | not-taken | Tail (decided 2026-11-23), cosmetic: quake II's: times `gl_modulate` on the models' light (`look.cpp`'s model_light, `client/world_models.cpp`), either look; the held weapon keeps its own light |
| cvar.gl_doublelight_entities | `gl_doublelight_entities` | models lit twice as bright, as the classic renderer did (default 1) | done | Quake II's: 1 lays the models' gl_modulate x gl_modulate_entities on them twice (`model_light`); nothing at the defaults |
| cvar.gl_glowmap_intensity | `gl_glowmap_intensity` | glow map strength (default 0.75) | done | Quake II's (`classic/q2_fx_switches.hpp`'s q2_glow_map_gain): a model's skin `<skin>_glow.png` or `.tga` beside its picture is its glow map (`classic/content/open.cpp`), each texel lighting itself by the glow's brightness over its own (`app/classic_models.hpp`'s glow_map_marks, `render/morph.hpp`'s glow_mark), times twice the setting (default 0.75), whatever the light; a full-colour picture of the skin's name stands in for the skin too. The held weapon's glow map is not read |
| cvar.gl_flarespeed | `gl_flarespeed` | how fast flares fade in and out (default 8) | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's flares |
| cvar.gl_fontshadow | `gl_fontshadow` | shadows under 2D text (default 0) | not-taken | Tail (decided 2026-11-23), cosmetic: `games/shooters_edge/client/q2_bar.cpp`: the status bar's and the screen's letters drawn first in black, 1 or 2 units down and right (`client/q2_view.cpp`); the console's letters not yet |
| cvar.gl_shaders | `gl_shaders` | 1 the shader backend, 0 legacy (default 1) | not-taken | `client-gaps.md`: the window's renderer is its own |
| cvar.gl_md5_load | `gl_md5_load` | load MD5 replacement models (default 1) | not-taken | replacement models come from classic enhance's cache as glTF (`plugins/classicenhance`), not `md5/` files beside the pack's |
| cvar.gl_md5_use | `gl_md5_use` | draw MD5 replacements (default 1) | not-taken | as `gl_md5_load`: no `md5/` replacements are read |
| cvar.gl_md5_distance | `gl_md5_distance` | beyond this distance the MD2 is drawn (default 2048) | not-taken | as `gl_md5_load`: no `md5/` replacements are read |
| cvar.gl_damageblend_frac | `gl_damageblend_frac` | the damage tint drawn as a frame this wide (extended protocol) (default 0.2) | missing | C21; a competitive adjustment (the damage tint kept to the screen's edge), so taken under the rule of 2026-11-23 |
| cvar.gl_waterwarp | `gl_waterwarp` | warp the view under water (default 0) | done | Quake II's: under a liquid the whole 3D view ripples in the post pass (`render/post.hpp`'s PostRipple: 0.25 % of the screen, five waves, a radian a second), the HUD not |
| cvar.gl_fog | `gl_fog` | draw the fog the game sets (default 1) | done | Quake II's: 0 draws no fog, neither the map's nor the `fog` command's (`client/world_draw.cpp`'s world_fog); the newer protocol's fog is the protocol rows' |
| cvar.gl_bloom | `gl_bloom` | bloom (default 0) | not-taken | Visuals come from classic enhance (decided 2026-11-23): a modern client's bloom switch; ours is `ce_bloom`/`r_bloom` |
| cvar.gl_bloom_sigma | `gl_bloom_sigma` | bloom blur width (default 4) | not-taken | ours blooms through its mip chain (`render/bloom.hpp`, `r_bloom` the share), with no blur width to set |
| cvar.gl_swapinterval | `gl_swapinterval` | vertical sync (default 1, saved) | missing | Missing, rank 2 (a Quake II name for a setting ours has; the settings lift): ours `vid_vsync` (`engine/platform/pacing.cpp`); no synonym (C21) |
| cvar.gl_drawsky | `gl_drawsky` | 0 skips the sky (default 1) | done | Quake II's: 0 draws the sky's faces solid black (the world's fast sky in black, `classic/app/look.cpp`'s world_options), either look |
| cvar.gl_clear | `gl_clear` | clear the screen each frame (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_novis | `gl_novis` | 0 uses visibility, 1 draws every leaf (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lockpvs | `gl_lockpvs` | keeps the last visibility; cheat-protected (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lightmap | `gl_lightmap` | draw lightmaps only; cheat-protected (default 0) | done | synonym of `r_lightmap` (`client/cvar_layers.cpp`) |
| cvar.r_fullbright | `r_fullbright` | no lighting; cheat-protected (default 0) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_vertexlight | `gl_vertexlight` | light the world by vertex (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_lightgrid | `gl_lightgrid` | light models from the map's light grid (default 1) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_polyblend | `gl_polyblend` | draw the screen tints (default 1) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_texturemode | `gl_texturemode` | nearest, linear, bilinear, trilinear filtering (default trilinear, saved) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_anisotropy | `gl_anisotropy` | anisotropic filtering (default 1) | done | Quake II's: the most taps along a slanted surface's long axis when the textures are filtered (1 none, up to 16; `brush/app/bsp.wgsl`'s filtered), the world's textures alone; another game keeps the renderer's own 4 |
| cvar.gl_picmip | `gl_picmip` | texture size halvings (default 0) | done | `plugins/classic/app/look.cpp` |
| cvar.gl_saturation | `gl_saturation` | texture saturation (default 1) | done | Quake II's: the walls' and floors' texels moved toward their luminance (0.2126, 0.7152, 0.0722) in the world's shader (`brush/app/bsp.wgsl`'s repainted), never the liquids, sky, models or light; at once, no texture reload |
| cvar.intensity | `intensity` | texture brightness factor (default 2) | done | `games/shooters_edge/client/cvar_layers.cpp` |
| cvar.gl_invert | `gl_invert` | inverts texture colours (default 0) | done | Quake II's: the walls' and floors' texels a negative of their sRGB colour, after the saturation, the same faces alone |
| cvar.vid_gamma | `vid_gamma` | gamma (default 1, saved) | done | synonym of `gamma` (`client/cvar_layers.cpp`) |
| cvar.gl_gamma_scale_pics | `gl_gamma_scale_pics` | apply gamma to 2D pictures (default 0) | done | Quake II's (`client/q2_view.hpp`'s Q2Screen::gamma_pics): the status bar's and the layouts' pictures read again with `gamma` (vid_gamma) laid on their colours, as the world's grade lays it on the 3D picture; 0 they keep their own |
| cvar.gl_bilerp_chars | `gl_bilerp_chars` | smooth the font (default 0) | not-taken | Tail (decided 2026-11-23), cosmetic: `games/shooters_edge/client/q2_bar.cpp`: the status bar's and the screen's letters smoothed; the console's not yet |
| cvar.gl_bilerp_pics | `gl_bilerp_pics` | smooth 2D pictures (default 1) | done | Quake II's: the status bar's and the layouts' pictures, 0 all sharp, 1 those of 64 units and up across or down smoothed (the icons and digits sharp), 2 all smoothed (`client/q2_bar.cpp`); the menus' pictures keep their own |
| cvar.gl_bilerp_skies | `gl_bilerp_skies` | smooth the sky (default 1) | done | Quake II's: 0 reads the map's sky box sharp, each texel a square (`brush/app/bsp.wgsl`'s sky_box_at); 1 smoothed |
| cvar.gl_upscale_pcx | `gl_upscale_pcx` | upscale 8-bit pictures by hq2x/hq4x (default 0) | not-taken | Our design: ours upscales by classic enhance's trained upscaler; no switch by this name |
| cvar.hqx | `hqx_y`, `hqx_cb`, `hqx_cr` | the hq2x upscaler's colour thresholds (48, 7, 6) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_downsample_skins | `gl_downsample_skins` | halve big skins (default 1) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.gl_cubemaps | `gl_cubemaps` | load cube-map skies (default 0) | not-taken | ours reads the map's six sky box faces always (`brush/app/sky_box.hpp`), the same picture as its cube map from the `env/` faces; single-file `sky/` layouts are not read |
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
| cvar.gl_multisamples | `gl_multisamples` | MSAA samples (default 0; menu 2, 4, 8) | not-taken | ours smooths edges by its temporal upscale (`docs/rendering.md` TAA-U), with no multisampled targets to ask for |
| cvar.basedir | `basedir` | the install folder; command line only | not-taken | Our design: ours finds the install itself; `basedir` is a word of the take-in (`client/take_in.cpp`) |
| cvar.homedir | `homedir` | the writable folder; command line only | done | any game's here (`client/take_in.hpp`): `+set homedir <folder>` on the command line is the folder the game writes in (the games taken in, the settings, screenshots, recordings, downloads), made when missing; read only once the game runs. Ours keeps one tree: a game is copied into the data folder, so nothing needs a second search |
| cvar.libdir | `libdir` | the game-library folder; command line only | not-taken | ours loads no native game library: Quake II's rules are our own code and a mod's compiled module runs in our VM (`plugins/progs`) |
| cvar.sys_console | `sys_console` | Unix: the terminal console's mode | not-taken | ours writes the client's console to no terminal; the dedicated server's terminal is its own |
| cvar.win_noalttab | `win_noalttab` | Windows: disable Alt+Tab (default 0) | not-taken | ours leaves the system's own keys (Alt+Tab, the Windows key) to the system on every platform |
| cvar.win_disablewinkey | `win_disablewinkey` | Windows: disable the Windows key (default 0) | not-taken | as `win_noalttab`: the system's own keys stay the system's |
| cvar.win_noresize | `win_noresize` | Windows: a window that cannot be resized (default 0) | done | `engine/platform/window_cvars.hpp` (every game, every desktop), applied by `follow_window_cvars` through `platform::set_window_style`: the edges no longer resize it |
| cvar.win_notitle | `win_notitle` | Windows: no title bar (default 0) | done | `engine/platform/window_cvars.hpp` (every game, every desktop), applied by `follow_window_cvars` through `platform::set_window_style`: no title bar, so no border either |
| cvar.win_alwaysontop | `win_alwaysontop` | Windows: keep the window on top (default 0) | done | `engine/platform/window_cvars.hpp` (every game, every desktop), applied by `follow_window_cvars` through `platform::set_window_style`: kept above every other window |
| cvar.win_noborder | `win_noborder` | Windows: no border (default 0) | done | `engine/platform/window_cvars.hpp` (every game, every desktop), applied by `follow_window_cvars` through `platform::set_window_style`: no border or title bar |
| cvar.sys_exitonerror | `sys_exitonerror` | Windows: exit at once on a fatal error (default 0) | not-taken | ours never waits on a box after a fatal error: the window prints what failed and exits at once, on every system |
| cvar.sys_viewlog | `sys_viewlog` | Windows: show the system console window | not-taken | ours opens no system console window; the in-game console and `logfile` hold what it would show |
| cvar.sys_disablecrashdump | `sys_disablecrashdump` | Windows: no crash dump | not-taken | ours writes no crash dumps, so there is none to turn off |
| cvar.gl_allow_software | `gl_allow_software` | Windows: allow the software GL driver (default 0) | not-taken | `client-gaps.md`: rendering internals left out |
| cvar.map_visibility_patch | `map_visibility_patch` | patch known maps' broken visibility (default 1) | done | synonym of `sv_vispatch` (`net/q2_game.cpp`) |
| cvar.map_noareas | `map_noareas` | debug: every area counts as joined (default 0) | done | Quake II's server setting (`server/q2_commands.cpp`): 1 ignores the area portals, every area joined to every other for sight, hearing and the area bits sent (`server/q2_areas.cpp`); a hosted game library's too (`server/q2_native_calls.cpp`) |
| cvar.map_override_path | `map_override_path` | a folder of entity overrides for maps (default empty) | done | synonym of `sv_entpath` (`net/q2_game.cpp`) |
| cvar.z_perturb | `z_perturb` | debug: fills freed memory (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.host_speeds | `host_speeds` | debug: prints frame timings (default 0) | not-taken | `client-gaps.md`: developer tools left out |
| cvar.developer | `developer` | prints developer messages (default 0) | not-taken | Tail (decided 2026-11-23): developer prints of the client; not built. |
| cvar.timescale | `timescale` | game speed; cheat-protected (default 1) | done | `client/q2_session.cpp`, a cheat, on a game this window runs (M18k4 round 35) |
| cvar.fixedtime | `fixedtime` | fixed frame time; cheat-protected (default 0) | done | `client/q2_session.cpp`, a cheat, on a game this window runs (M18k4 round 35) |
| cvar.logfile | `logfile` | 1 writes the console to a new log file, 2 or more adds to it (default 0) | done | `client/console_log.cpp` over the server's log (`engine/server/console_log.hpp`), in Quake II's layer: the same meanings as the server's |
| cvar.logfile_flush | `logfile_flush` | 0 buffered, 1 each line flushed, 2 or more unbuffered (default 0) | done | `client/console_log.cpp` over `engine/server/console_log.hpp` |
| cvar.logfile_name | `logfile_name` | the log's name (default `console`) | done | `client/console_log.cpp`, `logs/<name>.log` beside the settings (M18k4 round 35) |
| cvar.logfile_prefix | `logfile_prefix` | each log line's prefix, `console_prefix`'s forms (default `[%Y-%m-%d %H:%M] `) | done | `client/console_log.cpp`: the server's `terminal_prefix` (`<?>`, `@`, the time) |
| cvar.console_prefix | `console_prefix` | a prefix on the system console's lines (default empty) | not-taken | ours writes the client's console to no terminal; the dedicated server has it (`engine/server/macros.hpp`'s `terminal_prefix`) |
| cvar.dedicated | `dedicated` | read-only: a dedicated server | done | `engine/server/spawn.cpp` |
| cvar.cl_running | `cl_running` | read-only: the client is running | done | `client/q2_session.cpp`, read-only (M18k4 round 35) |
| cvar.cl_paused | `cl_paused` | read-only: the client is paused | done | `client/q2_session.cpp`, read-only (M18k4 round 35) |
| cvar.sv_running | `sv_running` | read-only: a server is running | done | `client/q2_session.cpp`, read-only (M18k4 round 35) |
| cvar.sv_paused | `sv_paused` | read-only: the server is paused | done | `client/q2_session.cpp`, read-only (M18k4 round 35) |
| cvar.timedemo | `timedemo` | play a demo as fast as possible and print frames per second; cheat-protected (default 0) | done | `games/shooters_edge/client/demo_playback.cpp` |
| cvar.com_date_format | `com_date_format` | the `$com_date` format (default `%Y-%m-%d`) | done | `engine/console/clock_macros.cpp` |
| cvar.com_time_format | `com_time_format` | the `$com_time` format (default `%H.%M`) | done | `engine/console/clock_macros.cpp`: `%H:%M`, as on every system but Windows |
| cvar.com_debug_break | `com_debug_break` | debug: break into the debugger on an error | not-taken | `client-gaps.md`: developer tools left out |
| cvar.com_fatal_error | `com_fatal_error` | debug: errors are fatal | not-taken | `client-gaps.md`: developer tools left out |
| cvar.allow_download | `allow_download` | client: 0 fetch nothing, -1 refuse even `download` (default 1, saved) | done | `games/shooters_edge/client/q2_session.cpp`, read at each join (`net/qw_remote.cpp`) into `net/q2_downloads.hpp`'s switches: 0 fetches nothing joining, -1 refuses a typed `download` too (`net/q2_remote.cpp`) |
| cvar.allow_download_players | `allow_download_players` | client: fetch player figures (default 1) | done | `net/q2_downloads.hpp` (`q2_download_allowed`): `players/` held back from what joining fetches (`net/q2_client.cpp`) |
| cvar.allow_download_models | `allow_download_models` | client: fetch models (default 1) | done | `net/q2_downloads.hpp`: `models/` and `sprites/` |
| cvar.allow_download_sounds | `allow_download_sounds` | client: fetch sounds (default 1) | done | `net/q2_downloads.hpp`: `sound/` |
| cvar.allow_download_maps | `allow_download_maps` | client: fetch maps (default 1) | done | `net/q2_downloads.hpp`: `maps/` |
| cvar.allow_download_textures | `allow_download_textures` | client: fetch wall textures and skies (default 1) | done | `net/q2_downloads.hpp` gates `textures/` and `env/`; joining fetches the sky's six sides and the map's wall textures (`net/q2_fetch.hpp`) |
| cvar.allow_download_pics | `allow_download_pics` | client: fetch pictures (default 1) | done | `net/q2_downloads.hpp` gates `pics/`; joining fetches the level's pictures (`net/q2_fetch.hpp` `q2_fetch_level`) |
| cvar.allow_download_others | `allow_download_others` | fetch any other file (default 0) | done | `net/q2_downloads.hpp`: every other folder, off by default |
| cvar.rcon_password | `rcon_password` | the password `rcon` sends | done | `games/shooters_edge/net/qw_admin.cpp` |
| cvar.sys_forcegamelib | `sys_forcegamelib` | a game library path forced; command line only | not-taken | ours loads no native game library (as `libdir`) |
| cvar.sys_allow_unsafe_savegames | `sys_allow_unsafe_savegames` | load saves of another build; command line only (default 0) | not-taken | ours reads only its own save files, each checked by its header and checksums (`engine/save`), so there is no other build's save to allow |
| cvar.sys_history | `sys_history` | lines of the terminal's history kept | not-taken | ours writes the client's console to no terminal; the in-game console keeps its own history |
| cvar.fs_autoexec | `fs_autoexec` | run `autoexec.cfg` on a game change (default 1) | done | Quake II's `fs_autoexec` (default 1): on a game change default.cfg (with `fs_default`, ours, in both games), the game's own settings, each game folder's autoexec.cfg (the base's, then the mod's) and postexec.cfg run in that order (`client/install_configs.cpp`'s `run_game_settings`); ours keeps each game's settings in its own file, run whatever `fs_autoexec` says |
| cvar.fs_debug | `fs_debug` | debug: prints file opens | not-taken | `client-gaps.md`: developer tools left out |
| cvar.fs_fuzz | `fs_fuzz_factor`, `fs_fuzz_filter` | debug: fail file reads at random | not-taken | `client-gaps.md`: developer tools left out |
| cvar.game | `game` | the mod folder; latched, server info | not-taken | Our design: games and mods are switched by our own words (`client/game_folder.cpp`), not a `game` setting. |
| cvar.com_completion_mode | `com_completion_mode` | how Tab completion lists matches (default 1) | done | Quake II's Tab (`client/cvar_layers.cpp` over `console/view.hpp`'s `list_mode`): the word completed to what the matches share, the line echoed and the matches listed one a line (0), in columns (1) or in columns under a heading for each kind (2); ours lists the first word's matches only, in columns as many as fit 78 letters |
| cvar.com_completion_treshold | `com_completion_treshold` | matches above which Tab asks first (default 50) | done | `console/view.hpp`'s `list_threshold`: past it the first Tab says "Press TAB again to display all N possibilities.", the second lists |
| cvar.showpackets | `showpackets` | prints every packet (default 0) | done | `plugins/classic/protocol/channel.hpp` (`client-gaps.md` round 21) |
| cvar.showdrop | `showdrop` | prints dropped packets (default 0) | done | `plugins/classic/protocol/channel.hpp` |
| cvar.qport | `qport` | the client's port inside the connection (random) | done | `qport` (classic layer, both games): 0 one picked from the name (ours keeps it from run to run, upstream's from the clock), 1..65535 that number, from the next connect (`net/qw_remote.cpp`); 2 bytes on protocol 34 |
| cvar.net_maxmsglen | `net_maxmsglen` | the largest packet asked of the server (default 1390) | done | `client/q2_session.cpp`; sent in the connect line on 35 and 36 (0 as 0, else 512..4086) |
| cvar.net_chantype | `net_chantype` | 1 the newer net channel on modern protocols (default 1) | done | `client/q2_session.cpp`; asked on 36; the server's `nc=` answer decides (plugins/classic/protocol/channel.hpp's newer channel) |
| cvar.net_ip | `net_ip` | the local address to bind | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours has it for the server (`engine/server/main.cpp`), not the client |
| cvar.net_ip6 | `net_ip6` | the local IPv6 address | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: quake II's: the IPv6 address the client's IPv6 socket is bound to ("" any), read at each join, not at once |
| cvar.net_port | `net_port` | the server's port (default 27910) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours has it for the server (`engine/server/main.cpp`) |
| cvar.net_clientport | `net_clientport` | the client's own port (default any) | done | Quake II's `net_clientport` (default -1, any free port): the client's socket on that port, opened again the frame it moves with the connection carrying on; a port that will not open says so and any free port is used (`net/qw_remote.cpp`'s `own_port`); ours keeps the setting's value rather than putting it back to -1 |
| cvar.net_dropsim | `net_dropsim` | debug: drop this share of packets (default 0) | not-taken | Our design: ours simulates networks in `engine/net` (labs), not by this setting |
| cvar.net_log | `net_log_enable`, `net_log_name`, `net_log_flush` | write a network log | not-taken | a debug build's packet log only (absent from a release); ours keeps no settings that only a debug build has, and its own captures cover the need (`engine/net`'s pcap captures) |
| cvar.net_enable_ipv6 | `net_enable_ipv6` | IPv6 networking | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: quake II's (`client/cvar_layers.cpp`, read by `net/qw_remote.cpp`): 0 refuses to join an IPv6 address ("IPv6 is turned off; set net_enable_ipv6 to 1 to join ..."), 1 and 2 join one from an IPv6 socket (`own_v6`), every game's `connect` reading `[addr]:port` and a bare IPv6 address; names resolve to IPv4 only, and a listen server never listens on IPv6 (2's half) |
| cvar.net_ignore_icmp | `net_ignore_icmp` | ignore ICMP errors (default 0) | not-taken | ours always passes over the system's unreachable errors (`engine/net/udp_socket.cpp`), as its 1 does; a server that went away is found by `cl_timeout` instead |

## Macros ($name in typed and bound text)

done 26, missing 0, not-taken 4

A `$name` in a typed line, a bind, `say` or a `draw` is replaced by the macro's value.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| macro.loc_here | `$loc_here` | the name of the place the player stands in | done | `games/shooters_edge/client/place_names.cpp` |
| macro.loc_there | `$loc_there` | the name of the place the player looks at | done | `games/shooters_edge/client/place_names.cpp` |
| macro.cl_mapname | `$cl_mapname` | the map's file name | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_server | `$cl_server` | the server's address | done | `games/shooters_edge/net/qw_remote.cpp`: the server joined, as asked |
| macro.cl_timer | `$cl_timer` | time on this level, m:ss or h:mm:ss | done | `games/shooters_edge/client/team_chat.cpp` (`engine/console/clock_macros.cpp` `level_clock`) |
| macro.cl_demopos | `$cl_demopos` | the playing demo's position, m:ss.t | done | `games/shooters_edge/client/demo_playback.cpp` (`tenths_clock`) |
| macro.cl_ups | `$cl_ups` | the player's speed along the ground, units a second (predicted when predicting) | done | `games/shooters_edge/client/place_seen.cpp` |
| macro.cl_fps | `$cl_fps` | moves (client frames) a second | done | `games/shooters_edge/net/qw_remote_macros.cpp`: one move a frame, so the moves' rate |
| macro.r_fps | `$r_fps` | drawn frames a second | done | `engine/platform/pacing.cpp`: frames drawn in the last second |
| macro.cl_mps | `$cl_mps` | moves a second sent | done | `games/shooters_edge/net/qw_remote_macros.cpp`: the moves sent, the last whole second (`engine/net/rates.cpp`) |
| macro.cl_pps | `$cl_pps` | packets a second sent | done | `games/shooters_edge/net/qw_remote_macros.cpp`: the packets sent, the last whole second (`engine/net/rates.cpp`) |
| macro.cl_ping | `$cl_ping` | the round trip, ms | done | `games/shooters_edge/net/qw_remote_macros.cpp` |
| macro.cl_lag | `$cl_lag` | packets lost, percent with 2 decimals | done | `games/shooters_edge/net/qw_remote_macros.cpp` |
| macro.cl_health | `$cl_health` | the health stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_ammo | `$cl_ammo` | the ammo stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_armor | `$cl_armor` | the armour stat | done | `games/shooters_edge/client/team_chat.cpp` |
| macro.cl_weaponmodel | `$cl_weaponmodel` | the held weapon's model name | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/net/qw_remote_macros.cpp`: on a Quake II server joined; a game played in the window reads "" |
| macro.cl_numentities | `$cl_numentities` | entities in the frame | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/net/qw_remote_macros.cpp`: on a server joined; a game played in the window reads 0 |
| macro.cl_surface | `$cl_surface` | the texture name and flags in the line of sight | done | `games/shooters_edge/client/face_hit.cpp` (`surface_ahead`), `client/place_seen.cpp`; every game |
| macro.gl_viewcluster | `$gl_viewcluster` | the view's visibility cluster number | not-taken | `client-gaps.md`: developer tools left out |
| macro.gl_viewleaf | `$gl_viewleaf` | the view's leaf number (a debug build's) | not-taken | `client-gaps.md`: developer tools left out |
| macro.com_date | `$com_date` | today's date by `com_date_format` | done | `engine/console/clock_macros.cpp` |
| macro.com_time | `$com_time` | the time by `com_time_format` | done | `engine/console/clock_macros.cpp` |
| macro.com_uptime | `$com_uptime` | how long the program has run | done | `engine/console/clock_macros.cpp` |
| macro.com_uptime_long | `$com_uptime_long` | the same in words | done | `engine/console/clock_macros.cpp` |
| macro.random | `$random` | a digit 0..9 | done | `engine/console/clock_macros.cpp` |
| macro.com_maplist | `$com_maplist` | every map's name (the start-server page lists them) | done | `games/shooters_edge/client/game_adapter.cpp` |
| macro.net_uprate | `$net_uprate` | bytes a second sent | done | `games/shooters_edge/net/qw_remote_macros.cpp` (`engine/net/rates.cpp`, the transport's counts) |
| macro.net_dnrate | `$net_dnrate` | bytes a second received | done | `games/shooters_edge/net/qw_remote_macros.cpp` (`engine/net/rates.cpp`, the transport's counts) |
| macro.cvars | `$<any setting>` | any setting's value expands the same way (`$$` defers it in menu lines) | done | Checked 2026-11-23: the console language expands `$name` of a setting in typed and bound text (`engine/console/script.cpp`, `expand_text`). |

## Menu pages and rows

done 146, missing 4, not-taken 12

The menu as upstream ships it: its menu script's pages and rows (`q2pro.menu`), and the three built-in pages (player setup, the demo browser, the server browser). Ours: Quake II runs on the classic menu's pages with Quake II's plates (`games/shooters_edge/menu/main_menu.cpp`, `q2_menu`); C5 to C7 describe the difference.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| menu.look.background | menu background | black behind every page; the in-game page tinted blue at about 1/8 | not-taken | Our design: ours draws the classic menu's backdrop with Quake II's plates (C5) |
| menu.look.font | menu font and cursor | the pack's `conchars` letters, `ch1` as the cursor | not-taken | Tail (decided 2026-11-23), cosmetic: quake II's plates and our carved letters; the `ch1` cursor not matched. |
| menu.look.colours | row colours | normal and selected rows light blue, disabled grey | not-taken | Our design: ours the classic menu's colours (C5) |
| menu.look.weapon | player-setup weapon | the figure on the player page holds the railgun's world model | missing | no figure drawn (C5) |
| menu.esc.disconnected | ESCAPE while not in a game | opens the main page | done | `games/shooters_edge/menu/main_menu.cpp` |
| menu.esc.ingame | ESCAPE in a game | opens the compact in-game page (below) over the world | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.esc.layout | ESCAPE with a layout up | sends `putaway` (closes the help computer or inventory); held, opens the in-game page | done | `games/shooters_edge/menu/game_menu.cpp` (`Host::screen_up`, `Host::escape_held`): ESCAPE sends `putaway`; held on half a second (a key's first repeat), the in-game page opens |
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
| menu.load.slots | load game | 16 slots: the autosave, a gap, then 15, each showing the save's name | done | `menu/q2_pages.cpp` (`MenuTable::load_slots`): the autosave `save0`, a gap, then `save1` to `save15`, each showing its save's title; an empty one dimmed |
| menu.save.slots | save game | 15 slots (no autosave) | done | `menu/q2_pages.cpp` (`MenuTable::save_slots`): `save1` to `save15` |
| menu.mp.q2servers | multiplayer: browse q2servers.com | the server browser filled from an HTTP list | done | `games/shooters_edge/menu/q2_pages.cpp`: the row opens the browser on `+http://q2servers.com/?raw=2` alone (`pushmenu servers` with sources, `menu/servers_page.hpp`'s `use_sources`; the list fetched as 6-byte records); the same row on the in-game page |
| menu.mp.addressbook | multiplayer: browse address book | the browser over the address book, `servers.lst` and the LAN | done | `games/shooters_edge/menu/q2_pages.cpp`: the row opens the browser on `favorites://` (`adr0` on), `file:///servers.lst` (a list in the game's files) and `broadcast://` (the LAN) alone; a word it cannot use says "Ignoring invalid master URL: ..." |
| menu.mp.demos | multiplayer: browse demos | the demo browser | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.mp.startserver | multiplayer: start server | the start-server page | done | ours NEW GAME (host page) |
| menu.ss.map | start server: initial map | every map found (`$com_maplist`) | done | ours MAP |
| menu.ss.rules | start server: rules | deathmatch or cooperative | done | Checked 2026-11-23: GAME TYPE and TEAMPLAY rows set the rules the server starts with. |
| menu.ss.timelimit | start server: time limit | a number field, "0 - no time limit" | done | ours TIME LIMIT (a choice) |
| menu.ss.fraglimit | start server: frag limit | a number field, "0 - no frag limit" | done | ours FRAG LIMIT |
| menu.ss.maxplayers | start server: max players | a number field | done | ours MAX PLAYERS |
| menu.ss.hostname | start server: host name | a text field | done | HOST NAME (`games/shooters_edge/menu/main_menu.cpp`): the server's `hostname` shown beside it, typed in the console (`sv hostname <name>`), as NAME is |
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
| menu.opt.player | options: player setup | opens the player page | missing | Missing, rank 1 (gameplay, daily use): `games/shooters_edge/menu/q2_pages.cpp`: name, model, skin and handedness; the figure drawn missing |
| menu.opt.input | options: input setup | opens the input page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.keys | options: key bindings | opens the key page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.weapons | options: weapon bindings | opens the weapon-key page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.video | options: video setup | opens the video page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.sound | options: sound setup | opens the sound page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.effects | options: effects setup | opens the effects page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.screen | options: screen setup | opens the screen page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.downloads | options: download options | opens the download page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.opt.addressbook | options: address book | opens the address book | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: not on OPTIONS: our menu has no text field for `adr0`..`adr15` |
| menu.pl.name | player: name | a text field for `name` | done | ours SETUP NAME |
| menu.pl.model | player: model | every figure in `players/` | done | `games/shooters_edge/menu/figures.cpp`, `menu/main_menu.cpp`: male, female, then by name; written to `skin` |
| menu.pl.skin | player: skin | the chosen figure's skins | done | `games/shooters_edge/menu/figures.cpp`, `menu/main_menu.cpp`: a skin with its icon; a new model at its first skin |
| menu.pl.hand | player: handedness | right, left, center (`hand`) | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.pl.figure | player: the figure drawn | the chosen figure and skin turning, holding the weapon | missing | C5 |
| menu.pl.none | player: no figures | "No player models found." when none | done | `games/shooters_edge/menu/main_menu.cpp` (`apply_event`): "No player models found." and the page does not open |
| menu.in.sens | input: mouse sens | a number field for `sensitivity` | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.in.autosens | input: auto sens | `m_autosens` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.in.filter | input: mouse filter | `m_filter` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.in.freelook | input: free look | `freelook` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
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
| menu.vid.mode | video: video mode | windowed or a listed full-screen mode | done | `games/shooters_edge/menu/q2_pages.cpp` (`Entry::modes_from`): windowed, then each mode `vid_modelist` offers (`desktop`, WxH@Hz), the choice's number written to `vid_fullscreen` |
| menu.vid.fov | video: fov scaling | `cl_adjustfov` vert- or hor+ | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.vsync | video: vertical sync | `gl_swapinterval` | missing | Missing, rank 2 (a Quake II name for a setting ours has; the settings lift): `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.msaa | video: anti-aliasing | no, 2x, 4x, 8x MSAA | not-taken | Visuals come from classic enhance (decided 2026-11-23): a modern client's MSAA row (`gl_multisamples` not taken); ours smooths edges by TAA-U |
| menu.vid.gamma | video: texture gamma | `vid_gamma` 1.3 to 0.3 | not-taken | Our design: ours BRIGHTNESS on OPTIONS (`gamma`) |
| menu.vid.hwgamma | video: hardware gamma | `vid_hwgamma` | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live, `vid_hwgamma` registered |
| menu.vid.picmip | video: texture quality | `gl_picmip` 3 to 0 | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.filter | video: texture filter | nearest, linear, bilinear, trilinear | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.aniso | video: anisotropic filter | no to 16x | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live, `gl_anisotropy` registered |
| menu.vid.saturation | video: texture saturation | `gl_saturation` 0 to 1 | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live, `gl_saturation` registered |
| menu.vid.intensity | video: texture intensity | `intensity` 1x, 2x, 3x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.lmsat | video: lightmap saturation | `gl_coloredlightmaps` | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live, `gl_coloredlightmaps` registered |
| menu.vid.lmbright | video: lightmap brightness | `gl_brightness` 0 to 0.3 | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.vid.backend | video: rendering backend | legacy or shaders | not-taken | `client-gaps.md`: the window's renderer is its own |
| menu.snd.engine | sound: sound engine | no sound, software, OpenAL | done | `games/shooters_edge/menu/q2_pages.cpp` over the engine's `s_enable` (`platform/window_cvars.hpp`): no sound, software, OpenAL (2 the same one mixer) |
| menu.snd.latency | sound: sound latency | `s_mixahead` 0.05 to 0.2 | done | `games/shooters_edge/menu/q2_pages.cpp`: `s_mixahead` 0.05 to 0.2, the setting kept (cvar.s_mixahead) |
| menu.snd.volume | sound: sound volume | `s_volume` 0 to 1 | done | `games/shooters_edge/menu/q2_pages.cpp`: `s_volume`, Quake II's spelling of `volume` |
| menu.snd.music | sound: music volume | `ogg_volume` | done | `games/shooters_edge/menu/q2_pages.cpp`: `ogg_volume`, Quake II's spelling of `bgmvolume` |
| menu.snd.enable | sound: enable music | `ogg_enable` | done | `games/shooters_edge/menu/q2_pages.cpp`: `ogg_enable` (off pauses the music, on resumes it) |
| menu.snd.shuffle | sound: shuffle tracks | `ogg_shuffle` | done | `games/shooters_edge/menu/q2_pages.cpp`: `ogg_shuffle` |
| menu.snd.underwater | sound: underwater effect | `s_underwater` | done | `games/shooters_edge/menu/q2_pages.cpp`: `s_underwater` (`client/room_ear.hpp`) |
| menu.snd.ambient | sound: ambient sounds | no, yes, only player's own (writes 2) | done | `games/shooters_edge/menu/q2_pages.cpp`: `s_ambient` no, yes, "only player's own" (writing 2, as its page does) |
| menu.snd.chatbeep | sound: chat beep | disabled, default, alternative | done | `games/shooters_edge/menu/q2_pages.cpp`: `cl_chat_sound` disabled, default, alternative |
| menu.fx.dlight | effects: dynamic lighting | no, yes, only switchable | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.cel | effects: entity cel-shading | no, 1x, 2x, 3x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.glow | effects: entity glowing | `cl_noglow` inverted | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.shadows | effects: ground shadows | no, yes, fade | done | `games/shooters_edge/menu/q2_pages.cpp`: the row on `gl_shadows` (no 0, yes 1, fade 2) |
| menu.fx.blend | effects: screen blending | `gl_polyblend` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.warp | effects: screen warping | `gl_waterwarp` | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live, `gl_waterwarp` registered |
| menu.fx.grenade | effects: grenade explosions | `cl_disable_explosions` bit 0 inverted | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.rocket | effects: rocket explosions | `cl_disable_explosions` bit 1 inverted | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.fx.rail | effects: railgun trail setup... | opens the rail page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.rail.type | rail: rail trail type | default, core only, core and spiral | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.time | rail: rail trail duration | 0.1 to 3 s | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.width | rail: core width | 1 to 6 | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.radius | rail: spiral radius | 1 to 6 | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.corecolor | rail: core color | black, red, green, yellow, blue, cyan, magenta, white | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.rail.spiralcolor | rail: spiral color | the same eight | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.size | screen: screen size | `viewsize` 40 to 100 by 10 | done | `games/shooters_edge/menu/q2_pages.cpp`: SCREEN SIZE 40 to 100 by 10 on Quake II's own `viewsize` |
| menu.scr.pinggraph | screen: ping graph | `scr_lag_draw` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.demobar | screen: demo bar | no, yes, verbose | done | `games/shooters_edge/menu/q2_pages.cpp`: the row turns `scr_demobar` through 0, 1, 2 |
| menu.scr.hudalpha | screen: HUD opacity | `scr_alpha` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.conalpha | screen: console opacity | `con_alpha` | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.hudscale | screen: HUD scale | auto, 1x, 2x, 4x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.conscale | screen: console scale | auto, 1x, 2x, 4x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.menuscale | screen: menu scale | auto, 1x, 2x, 4x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.scr.crosshair | screen: crosshair setup... | opens the crosshair page | done | `games/shooters_edge/menu/q2_pages.cpp` |
| menu.ch.type | crosshair: crosshair type | none, cross, dot, angle | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ch.scale | crosshair: crosshair scale | 0.5x, 1x, 2x | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.ch.health | crosshair: color by health | `ch_health` | done | `games/shooters_edge/menu/q2_pages.cpp`: the row is live while Quake II is loaded, `ch_health` registered in its layer |
| menu.ch.rgba | crosshair: red, green, blue, alpha channel | four sliders 0 to 1 | done | Checked 2026-11-23: its setting is registered now, so the row is lit. `games/shooters_edge/menu/q2_pages.cpp`: the row is on its page, dimmed until the setting is registered |
| menu.dl.allow | downloads: allow downloading | `allow_download` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.maps | downloads: maps | `allow_download_maps` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.players | downloads: player models/skins | `allow_download_players` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.models | downloads: models | `allow_download_models` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.sounds | downloads: sounds | `allow_download_sounds` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.textures | downloads: textures | `allow_download_textures` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.pics | downloads: pics | `allow_download_pics` | done | `games/shooters_edge/menu/q2_pages.cpp`, the setting registered by `client/q2_session.cpp` |
| menu.dl.http | downloads: HTTP downloads | `cl_http_downloads` | done | `games/shooters_edge/menu/q2_pages.cpp`: the row sets `cl_http_downloads`, registered by `client/q2_session.cpp` |
| menu.ab.fields | address book | 16 text fields `adr0` to `adr15` | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/menu/q2_pages.cpp`: MULTIPLAYER > address book lists `adr0` to `adr15` (Quake II's cvars, `client/cvar_layers.cpp`) with each address beside it; a row opens the console to type it, no text field on the page; JOIN A GAME asks them (`menu/servers_page.cpp`) |
| menu.game.page | the in-game page | compact list: browse q2servers.com, browse address book, browse demos, single player, options, disconnect, quit | done | `games/shooters_edge/menu/q2_pages.cpp`: the page and ESCAPE in a level, its two browse rows on their sources, and ours (LAN, masters and hub) before them |
| menu.demos.list | demo browser | lists `demos/` (all folders with `ui_listalldemos`): `.dm2`, `.dm2.gz`, `.mvd2`, `.mvd2.gz` | not-taken | Our design: ours `menu/demos_page.cpp` (folders anywhere, newest first) (C40) |
| menu.demos.columns | demo browser columns | name, Date, Size, Map, POV; sorted by `ui_sortdemos` | not-taken | Our design: ours `menu/demos_page.cpp`: name, age and size on each row, sorted by `menu_demo_sort` (`vfs::sort_files`; Quake II spells it `ui_sortdemos`, by name by default); no Map or POV column |
| menu.servers.list | server browser | servers from an HTTP list, favourites, `servers.lst`, broadcast | not-taken | Our design: ours `menu/servers_page.cpp` (LAN, masters, hub) (C41) |
| menu.servers.columns | server browser columns | Hostname, Mod (the game folder, `baseq2` when empty), Map, Players, RTT; the selection's info as Key and Value, its players as Frg, RTT, Name | not-taken | Our design: ours its own columns (C41) |
| menu.servers.colours | server browser colours | with `ui_colorservers` 1: grey for a password, anticheat or no-fake server, green under 30 ms, white otherwise | done | `games/shooters_edge/menu/servers_page.cpp` (`row_colour`): `sb_colorservers`, which Quake II spells `ui_colorservers` |
| menu.script | the menu script file | `q2pro.menu`: pages and rows defined as text a player or mod can change | done | `menu/menu_script.cpp`: `q2pro.menu` on the game's search path read over Quake II's pages when the game loads (`menu_script` 1, the default); every row word (values, strings, pairs, range, toggle with a bit, bind, action, bitmap, savegame, loadgame, blank, field), `include`, `$` and `$$` expansion, comments and joined lines; a script's page replaces ours of its name, a new one is added; player setup, LOAD, SAVE and the browsers stay ours; the look's words (background, font, cursor, colours, banner, plaque, style) are read and checked, ours draws its own look; a field is typed in the console |

## Server messages the client parses (svc)

done 26, missing 0, not-taken 1

Every message a server (or a recording) sends the client, and what the client does with it. Ours reads Quake II's protocol 34 in `games/shooters_edge/client/demo_q2.cpp` for both a joined server and a recording; the protocols above 34 (35, 36) are not spoken (C42).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| svc.bad | `svc_bad` (0) | never valid; the connection drops with an "illegible server message" error naming the byte | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours refuses with its own words (`client/demo_q2.cpp`) |
| svc.muzzleflash | `svc_muzzleflash` (1) | a player's weapon fired (or entered, left, respawned): a coloured light at the player for a moment and the weapon's sound; the silenced bit plays it at 0.2 volume | done | `client/demo_q2.cpp` reads every value (`q2_flash_fx`) |
| svc.muzzleflash2 | `svc_muzzleflash2` (2) | a monster fired: a light at the monster's gun (offset from the monster's flash table) and the weapon's sound | done | `client/demo_q2.cpp` reads every value (`q2_flash_fx`) |
| svc.temp_entity | `svc_temp_entity` (3) | a one-off effect (table "Temporary entities") | done | read; every base-game kind takes its own route (its puff, named effect, cable or explosion and the sound the client plays itself), `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp`; what each kind still lacks is on its te. row (porting/shooters-edge/q2-client-te.md) |
| svc.layout | `svc_layout` (4) | the layout program (scoreboard, help computer) shown while the layout bit is up | done | `games/shooters_edge/hud/q2_layout.cpp` |
| svc.inventory | `svc_inventory` (5) | every item's count, drawn by the inventory screen | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.nop | `svc_nop` (6) | nothing | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.disconnect | `svc_disconnect` (7) | the server dropped us: "Server disconnected" | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.reconnect | `svc_reconnect` (8) | the server changes level: "Server disconnected, reconnecting", then connects again at once | done | Checked 2026-11-23: `net/q2_client.cpp` takes a stuffed `reconnect` and connects again when the server restarts. |
| svc.sound | `svc_sound` (9) | a positioned sound: index, volume, attenuation, time offset, entity and channel, place | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.print | `svc_print` (10) | a line at a level (pickup, medium, high, chat); chat filtered, kept out of notify by `cl_chat_notify`, played with the chat sound, scanned for addresses (`followip`) and triggers | done | printed; a chat line through `client/chat_lines.cpp`: `cl_chat_filter`, `cl_chat_notify`, `cl_chat_sound`, the addresses kept for `followip`, and `trigger` on the other lines (`tests/shooters_edge_chat_ignore_test.cpp`) |
| svc.stufftext | `svc_stufftext` (11) | console text the server runs on the client | done | the signon's lines answered by the client (`net/q2_client.cpp` `run_stuffed`: `cmd`, `precache`, `changing`, `reconnect`, `connect`); every other line the filter lets through (`cl_ignore_stufftext`) runs in the player's console as if typed (`Q2ClientEnd::to_run`, `Remote::stuffed_run`, `client/main.cpp`); `tests/shooters_edge_q2_net_test.cpp` |
| svc.serverdata | `svc_serverdata` (12) | the protocol, the server's count, attract-loop flag, game folder, player number, map title; prints the map title between rule lines | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.configstring | `svc_configstring` (13) | one configstring changed (table "Configstrings") | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.spawnbaseline | `svc_spawnbaseline` (14) | an entity's baseline for later deltas | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.centerprint | `svc_centerprint` (15) | a centre print; also printed in the console and run past triggers | not-taken | Tail (decided 2026-11-23), cosmetic: C9 (no fade, line cut differs) |
| svc.download | `svc_download` (16) | a piece of a requested file with the percent done | done | every kind the level names is fetched in stages (`net/q2_fetch.hpp`, `net/q2_client.cpp` `queue_next_stage`), each path once a connection |
| svc.playerinfo | `svc_playerinfo` (17) | the player state, inside a frame | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.packetentities | `svc_packetentities` (18) | the frame's entities, inside a frame | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.deltapacketentities | `svc_deltapacketentities` (19) | the stock protocol's delta entities | done | `games/shooters_edge/client/demo_q2.cpp` |
| svc.frame | `svc_frame` (20) | a server frame: number, delta source, area bits, then the player state and entities | done | read (`client/demo_q2.cpp`); the area bits kept as `DemoState::areas` and handed to the world's draw (`client/world_draw.cpp` `set_areas`) |
| svc.zpacket | `svc_zpacket` (21) | a deflate-compressed run of messages (protocols 35 and up) | done | `games/shooters_edge/net/q2_proto_in.cpp`: raw deflate inflated (at most 32768 bytes) and read again; one inside another refused |
| svc.zdownload | `svc_zdownload` (22) | a compressed download piece (35 and up) | done | `net/q2_proto_in.cpp`: 35's pieces inflated each to its size; 36's one stream gathered and inflated whole at 100 % (net/q2_client.cpp keeps it) |
| svc.gamestate | `svc_gamestate` (23) | every configstring and baseline in one go (36 and up) | done | `net/q2_proto_in.cpp`: configstrings to the end marker, then baselines, as protocol 34's messages |
| svc.setting | `svc_setting` (24) | a server setting (protocols 35 and up): whether player updates are sent, and the server's frame rate | done | `net/q2_proto_in.cpp`: the server's frame rate kept; like a default client, the frame stays 100 ms |
| svc.configstringstream | `svc_configstringstream` (25) | configstrings in a stream (protocol 36's newer versions, and recordings of it) | done | `net/q2_proto_in.cpp` |
| svc.baselinestream | `svc_baselinestream` (26) | baselines in a stream (protocol 36's newer versions) | done | `net/q2_proto_in.cpp` |

## Temporary entities (svc_temp_entity)

done 21, missing 18, not-taken 26

Each one-off effect a server sends, what the client draws and plays for it. The window's own game draws its own effects from its rules' events (`app/weapon_fx.hpp`, `app/monster_fx.hpp`, `client/world_fx.cpp`); the status here is for a joined server and a recording, where the original's effect must come from the message.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| te.gunshot | `TE_GUNSHOT` (0) | a bullet hit: dark particles and a small smoke-and-flash sprite, sometimes a ricochet sound | missing | Missing, rank 2 (seen on a joined server and in demos): 40 specks of 0x00..0x07 and the 1-in-16 ricochets now as the client's (`client/q2_temp_fx.hpp`); left: the smoke puff and flash models |
| te.blood | `TE_BLOOD` (1) | red blood particles along the direction | done | 60 specks of 0xe8..0xef off the face, 0..31 out along its normal (`fx::q2_specks`), from a server, a demo and our own game; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp`, `client/listen.cpp` |
| te.blaster | `TE_BLASTER` (2) | a blaster bolt's hit: orange particles, a flash model lit for a moment, `weapons/lashit.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): named `te_blaster`: its yellow sparks, light and `weapons/lashit.wav` (`client/demo_q2.cpp`, `client/world_fx.cpp`); left: the explode model turned to the face |
| te.railtrail | `TE_RAILTRAIL` (3) | the rail's spiral and core of particles (or a beam by `cl_railtrail_type`), `weapons/railgf1a.wav` at the end | done | named `te_railtrail`: the beam and the line circling it, `weapons/railgf1a.wav` at its end; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp` |
| te.shotgun | `TE_SHOTGUN` (4) | a pellet hit: fewer particles and a smoke puff, no ricochet | missing | Missing, rank 2 (seen on a joined server and in demos): 20 specks of 0x00..0x07, silent, now as the client's (`client/q2_temp_fx.hpp`); left: the smoke puff and flash models |
| te.explosion1 | `TE_EXPLOSION1` (5) | a large explosion: the explosion model, orange light, `weapons/rocklx1a.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): its sound `weapons/rocklx1a.wav` now played (`client/q2_temp_fx.hpp`); left: the explosion model's animation, skins and light curve |
| te.explosion2 | `TE_EXPLOSION2` (6) | a grenade-style explosion with its light and `weapons/grenlx1a.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): its sound `weapons/grenlx1a.wav` now played; left: the explosion model's 19 frames from 30, skins and light curve |
| te.rocket_explosion | `TE_ROCKET_EXPLOSION` (7) | a rocket's blast: model, particles (off by `cl_disable_particles` 4), light, sound | missing | Missing, rank 2 (seen on a joined server and in demos): its sound now played; left: the explosion model's animation, skins and light curve |
| te.grenade_explosion | `TE_GRENADE_EXPLOSION` (8) | a grenade's blast: model, particles, light, sound | missing | Missing, rank 2 (seen on a joined server and in demos): its sound now played; left: the explosion model's animation, skins and light curve |
| te.sparks | `TE_SPARKS` (9) | a few bright sparks | done | 6 specks of 0xe0..0xe7 off the face, silent; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp`, `client/listen.cpp` |
| te.splash | `TE_SPLASH` (10) | a splash of count particles coloured by kind (sparks, water, mud, slime, lava, blood); sparks play `world/spark5..7.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): drawn from the wire as the listen server draws it (`client/demo_q2.cpp`: `sq_splash` in its kind's colour); the sparks' sound not played; no test reads it off the wire |
| te.bubbletrail | `TE_BUBBLETRAIL` (11) | a line of bubbles between two points | done | bubbles every 32 units rising, read in `client/demo_q2.cpp`, drawn by `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.screen_sparks | `TE_SCREEN_SPARKS` (12) | green sparks where a power screen took a hit, `weapons/lashit.wav` | done | 40 specks of 0xd0..0xd7 and `weapons/lashit.wav`, one such sound at a time; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp`, `client/listen.cpp` |
| te.shield_sparks | `TE_SHIELD_SPARKS` (13) | the same for a power shield | done | 40 specks of 0xb0..0xb7 and the screen sparks' one-at-a-time `weapons/lashit.wav`; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp`, `client/listen.cpp` |
| te.bullet_sparks | `TE_BULLET_SPARKS` (14) | a bullet hit with sparks and a ricochet | missing | Missing, rank 2 (seen on a joined server and in demos): 6 specks of 0xe0..0xe7 and the ricochets now as the client's (`client/q2_temp_fx.hpp`); left: the smoke puff and flash models |
| te.laser_sparks | `TE_LASER_SPARKS` (15) | a laser's sparks: count, colour | missing | Missing, rank 2 (seen on a joined server and in demos): drawn from the wire (`sq_laser_sparks` in its colour, `client/demo_q2.cpp`); no test reads it off the wire |
| te.parasite_attack | `TE_PARASITE_ATTACK` (16) | the parasite's drain: a beam model from the monster to the target | missing | Missing, rank 2 (seen on a joined server and in demos): named, drawn by our cable routine from the entity (`client/demo_q2.cpp`, `fx::monster_cable`); left: the segment model laid every 30 units with its random roll |
| te.rocket_explosion_water | `TE_ROCKET_EXPLOSION_WATER` (17) | a rocket's blast under water: the model and `weapons/xpld_wat.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): its sound `weapons/xpld_wat.wav` now played; left: the explosion model's animation, skins and light curve |
| te.grenade_explosion_water | `TE_GRENADE_EXPLOSION_WATER` (18) | a grenade's blast under water | missing | Missing, rank 2 (seen on a joined server and in demos): its sound `weapons/xpld_wat.wav` now played; left: the explosion model's animation, skins and light curve |
| te.medic_cable_attack | `TE_MEDIC_CABLE_ATTACK` (19) | the medic's healing cable beam | missing | Missing, rank 2 (seen on a joined server and in demos): named, drawn by our cable routine from the entity (`client/demo_q2.cpp`, `fx::monster_cable`); left: the segment model laid every 30 units with its random roll |
| te.bfg_explosion | `TE_BFG_EXPLOSION` (20) | the BFG ball's burst sprite and green light | missing | Missing, rank 2 (seen on a joined server and in demos): read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_bigexplosion | `TE_BFG_BIGEXPLOSION` (21) | the BFG's big green particle burst | done | the explosion and the named `te_bfg_bigexplosion` green burst, no sound; `client/demo_q2.cpp`, `client/q2_temp_fx.hpp`, `client/world_fx.cpp` |
| te.bosstport | `TE_BOSSTPORT` (22) | the boss's teleport: a particle swirl and `misc/bigtele.wav` | missing | Missing, rank 2 (seen on a joined server and in demos): the teleport burst and `misc/bigtele.wav` heard everywhere (`client/q2_temp_fx.hpp`); left: its own particle swirl |
| te.bfg_laser | `TE_BFG_LASER` (23) | the BFG's green laser line for a frame | missing | Missing, rank 2 (seen on a joined server and in demos): read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grapple_cable | `TE_GRAPPLE_CABLE` (24) | the CTF grapple's cable from the player's hand | missing | Missing, rank 2 (seen on a joined server and in demos): read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.welding_sparks | `TE_WELDING_SPARKS` (25) | sparks with a flickering light (mission pack) | done | its count of specks in its colour and a flickering yellow-white light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.greenblood | `TE_GREENBLOOD` (26) | green blood (mission pack monsters) | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic blood (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bluehyperblaster | `TE_BLUEHYPERBLASTER` (27) | a blue hyperblaster's hit (mission pack) | done | 40 blue specks thrown along its way; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.plasma_explosion | `TE_PLASMA_EXPLOSION` (28) | the phalanx's plasma blast (mission pack) | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.tunnel_sparks | `TE_TUNNEL_SPARKS` (29) | a ring of sparks (mission pack) | done | its specks rising; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.blaster2 | `TE_BLASTER2` (30) | a green blaster's hit (mission pack) | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.railtrail2 | `TE_RAILTRAIL2` (31) | the newer game's lighter rail line | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flame | `TE_FLAME` (32) | never sent; both refuse it | done | `client/demo_q2.cpp` refuses it too |
| te.lightning | `TE_LIGHTNING` (33) | a lightning beam between two entities (mission pack) | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.debugtrail | `TE_DEBUGTRAIL` (34) | a debug line | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.plain_explosion | `TE_PLAIN_EXPLOSION` (35) | an explosion model with no particles | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flashlight | `TE_FLASHLIGHT` (36) | a light that follows an entity for a frame (mission pack) | done | a white light of 400 units for its frame; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.forcewall | `TE_FORCEWALL` (37) | a force wall: a line of coloured particles | done | a falling line of specks in its colour, every 4 units; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.heatbeam | `TE_HEATBEAM` (38) | the player's heat beam: a spiral beam from the gun | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.monster_heatbeam | `TE_MONSTER_HEATBEAM` (39) | a monster's heat beam | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.steam | `TE_STEAM` (40) | a puff or a lasting jet of steam with an id | not-taken | Tail (decided 2026-11-23), cosmetic: a puff draws its count along its jet (`client/world_fx_q2.cpp`, `app/effects.hpp`); a lasting jet (an id with a time) draws its first puff only |
| te.bubbletrail2 | `TE_BUBBLETRAIL2` (41) | a lighter bubble trail | done | bubbles every 8 units with the lash hit sound; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.moreblood | `TE_MOREBLOOD` (42) | a bigger burst of blood | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic blood (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.heatbeam_sparks | `TE_HEATBEAM_SPARKS` (43) | the heat beam's sparks | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.heatbeam_steam | `TE_HEATBEAM_STEAM` (44) | the heat beam's steam | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.chainfist_smoke | `TE_CHAINFIST_SMOKE` (45) | the chainfist's smoke | done | a puff of rising smoke; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.electric_sparks | `TE_ELECTRIC_SPARKS` (46) | blue electric sparks | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.tracker_explosion | `TE_TRACKER_EXPLOSION` (47) | the disruptor's dark burst and a dark light | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.teleport_effect | `TE_TELEPORT_EFFECT` (48) | a teleport burst | done | named `sq_teleport` (`client/demo_q2.cpp`) |
| te.dball_goal | `TE_DBALL_GOAL` (49) | the dodgeball goal's burst | done | the teleport burst; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.widowbeamout | `TE_WIDOWBEAMOUT` (50) | the widow's beam-out effect with an id | done | 300 specks a frame on a sphere growing to 45 units over 2.1 s, one per id; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.nukeblast | `TE_NUKEBLAST` (51) | the nuke's expanding blast | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.widowsplash | `TE_WIDOWSPLASH` (52) | the widow's splash | done | 256 specks in a sphere round it; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.explosion1_big | `TE_EXPLOSION1_BIG` (53) | a bigger explosion model | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion1_np | `TE_EXPLOSION1_NP` (54) | an explosion with no particles | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.flechette | `TE_FLECHETTE` (55) | a flechette's hit | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bluehyperblaster_2 | `TE_BLUEHYPERBLASTER_2` (56) | the newer game's blue bolt hit | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.bfg_zap | `TE_BFG_ZAP` (57) | the newer game's BFG zap line | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.berserk_slam | `TE_BERSERK_SLAM` (58) | the berserker's ground slam | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic spike (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.grapple_cable_2 | `TE_GRAPPLE_CABLE_2` (59) | the newer game's grapple cable | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.power_splash | `TE_POWER_SPLASH` (60) | a power armour's splash round an entity | done | 256 green specks round the entity's box as it stands, moving out; `client/demo_q2.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp` |
| te.lightning_beam | `TE_LIGHTNING_BEAM` (61) | the newer game's lightning beam | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic beam (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion1_nl | `TE_EXPLOSION1_NL` (62) | an explosion with no light | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.explosion2_nl | `TE_EXPLOSION2_NL` (63) | a grenade explosion with no light | not-taken | Tail (decided 2026-11-23), mission packs and later releases only: read; drawn as our generic explosion (`client/demo_q2.cpp`, `client/demo_fx.cpp`); verify the look, light and sound against the original |
| te.damage_dealt | `TE_DAMAGE_DEALT` (128) | damage this player dealt: the hit marker and, at `cl_hit_markers` 2 (the default), `weapons/marker.wav` | done | `client/demo_q2.cpp` reads temporary entity 128 into the hits and queues `weapons/marker.wav`; `client/q2_bar.cpp` `draw_q2_hit_marker` draws the marker, `client/demo_sound.cpp` plays the sound only at `cl_hit_markers` 2; joined and in recordings (`tests/shooters_edge_q2_screen_test.cpp`) |

## Muzzle flashes (svc_muzzleflash, a player's)

done 19, missing 0, not-taken 1

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
| mz.dlight_hack | `cl_dlight_hacks` 4 | removes the machine gun's and chain guns' muzzle light | done | `client/world_fx.cpp`: `cl_dlight_hacks` 4 drops the light of a gun's flash (`plugins/classic/q2_fx_switches.hpp`) |
| mz.muzzlefx | the muzzle-flash model | the newer game's flash model drawn at the muzzle of the player's own view weapon for a moment, its size and offset by weapon (`cl_muzzleflashes`); other players' flashes get no model | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's flash model |

## Muzzle flashes (svc_muzzleflash2, a monster's)

done 32, missing 0, not-taken 1

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
| mz2.muzzlefx | the muzzle-flash model | the newer game's flash model drawn at the gun for a moment, scaled by the monster | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's flash model; the original game's flash lights are drawn |

## Entity effect bits (EF_)

done 32, missing 0, not-taken 1

Bits of an entity's effects field a server sends; the client draws trails, lights, shells and frame cycles from them. Ours: `games/shooters_edge/client/q2_look.hpp` reads them for Quake II's entities.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ef.rotate | `EF_ROTATE` (1) | turns the model about its yaw on the client's clock (items) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.gib | `EF_GIB` (2) | a blood trail behind it (green with `EF_GREENGIB`) | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.bob | `EF_BOB` (4) | the newer game's item bob | done | rises and sinks 5 units every 2.5 s (`client/q2_look.cpp`, `client/world_models.cpp`); `cl_nobob` draws it still (`client/cvar_layers.cpp`) |
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
| ef.spinninglights | `EF_SPINNINGLIGHTS` (1<<23) | a spinning red light at the front (mission pack) | done | upside down, 500 degrees a second, a red light 64 units ahead (`client/world_models.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp`) |
| ef.plasma | `EF_PLASMA` (1<<24) | plasma bolt: orange light (mission pack) | done | a pink light, drawn at 0.6 (`client/q2_look.cpp`, `client/world_fx_q2.cpp`, `app/effects.hpp`) |
| ef.trap | `EF_TRAP` (1<<25) | the trap's light and particles (mission pack) | done | a rising column and 8 thrown specks 32 units over it, a flickering yellow light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.tracker | `EF_TRACKER` (1<<26) | disruptor bolt: a dark light and the tracker trail (mission pack) | done | two black strands woven about its line and a darkening light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.double | `EF_DOUBLE` (1<<27) | double damage: a yellow shell | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.spheretrans | `EF_SPHERETRANS` (1<<28) | a see-through sphere (mission pack) | done | drawn at 0.3, 0.6 with the tracker trail (`client/q2_look.cpp`) |
| ef.tagtrail | `EF_TAGTRAIL` (1<<29) | the tag token's trail (mission pack) | done | a yellow column of specks and a yellow light; `client/world_fx_q2.cpp`, `app/effects.hpp` |
| ef.half_damage | `EF_HALF_DAMAGE` (1<<30) | half damage: a shell | done | `games/shooters_edge/client/q2_look.hpp` |
| ef.trackertrail | `EF_TRACKERTRAIL` (1<<31) | a tracker's dark trail and pulsing light (mission pack) | not-taken | Tail (decided 2026-11-23), cosmetic: with the tracker a pulsing darkening light, alone a black shell of 300 specks and a darkening light (`client/world_fx_q2.cpp`, `app/effects.hpp`); the model is not yet drawn black |
| ef.lights | the lights per bit | each bit's light radius and colour, added to the view's dynamic lights (`cl_lights`) | done | Checked 2026-11-23: the missiles' lights by effect bit (`q2_look.hpp`). |

## Entity render bits (RF_) and view bits (RDF_)

done 22, missing 3, not-taken 7

Bits of an entity's render field, and of the player state's view flags.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| rf.minlight | `RF_MINLIGHT` (1) | always some light (the view weapon) | done | never darker than a tenth (`client/world_models.cpp`) |
| rf.viewermodel | `RF_VIEWERMODEL` (2) | not drawn from the player's own eyes | not-taken | Our design: ours hides the viewer's own body by its own rule; the bit not read |
| rf.weaponmodel | `RF_WEAPONMODEL` (4) | drawn only from the eyes | not-taken | Our design: ours draws its own view weapon; the bit not read |
| rf.fullbright | `RF_FULLBRIGHT` (8) | drawn at full brightness | done | full light whatever the room (`client/world_models.cpp`) |
| rf.depthhack | `RF_DEPTHHACK` (16) | depth squeezed so it never enters walls (the view weapon) | not-taken | Our design: ours draws the view weapon on top by its own pass |
| rf.translucent | `RF_TRANSLUCENT` (32) | drawn at 0.7 | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.framelerp | `RF_FRAMELERP` (64) | blend between the old and new frame | not-taken | Our design: frames are always interpolated, so the bit has nothing to switch. |
| rf.beam | `RF_BEAM` (128) | a laser beam of the frame's width in the skin's colour, at 0.3 | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.customskin | `RF_CUSTOMSKIN` (256) | the skin is a picture index | missing | not read |
| rf.glow | `RF_GLOW` (512) | the item's light pulses | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_red | `RF_SHELL_RED` (1024) | a red shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_green | `RF_SHELL_GREEN` (2048) | a green shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_blue | `RF_SHELL_BLUE` (4096) | a blue shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.noshadow | `RF_NOSHADOW` (8192) | casts no blob shadow | done | `client/q2_look.cpp` (`Q2Look::no_shadow`) onto `Placed::no_shadow`: such a model casts none (`client/model_shadow.hpp`) |
| rf.castshadow | `RF_CASTSHADOW` (16384) | the newer game's shadow caster | done | the game's client reads it nowhere; ours the same |
| rf.ir_visible | `RF_IR_VISIBLE` (32768) | drawn flat red while infra-red goggles are on | done | client/q2_look.cpp `q2_infra_red`: under the goggles the model is its skin times pure red, lit by nothing (`client/world_models.cpp`), unless a shell or full light holds it |
| rf.shell_double | `RF_SHELL_DOUBLE` (65536) | double damage's shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.shell_half | `RF_SHELL_HALF_DAM` (131072) | half damage's shell | done | `games/shooters_edge/client/q2_look.hpp` |
| rf.use_disguise | `RF_USE_DISGUISE` (1<<18) | drawn with the disguise skin (mission pack) | done | `client/q2_look.hpp` (`kRfUseDisguise`, `q2_disguised`): a player's figure in its folder's `disguise` skin, from a recording, a joined server or our own (`client/demo_video.cpp`, `client/world_models.cpp`); a folder without one wears its own |
| rf.shell_lite_green | `RF_SHELL_LITE_GREEN` (1<<19) | a light green shell | done | (0.56, 0.93, 0.56); the shell bits now mix in turn (`client/q2_look.cpp`) |
| rf.custom_light | `RF_CUSTOM_LIGHT` (1<<20) | a light of the entity's own colour | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's coloured entity light |
| rf.flare | `RF_FLARE` (1<<21) | a flare sprite that fades by sight (`cl_flares`, `gl_flarespeed`) | not-taken | Visuals come from classic enhance (decided 2026-11-23): the remaster's flares |
| rf.old_frame_lerp | `RF_OLD_FRAME_LERP` (1<<22) | the old frame is sent, not guessed | done | the game's client reads it nowhere; ours the same |
| rf.dot_shadow | `RF_DOT_SHADOW` (1<<23) | a dot shadow | done | the game's client reads it nowhere, so nothing is drawn for it; ours the same |
| rf.low_priority | `RF_LOW_PRIORITY` (1<<24) | dropped first when there are too many | done | read only by a server choosing what to send; nothing for a client to draw |
| rf.no_lod | `RF_NO_LOD` (1<<25) | no level-of-detail model | done | ours has no lower-detail models: every model is drawn whole, as the bit asks |
| rf.stair_step | `RF_STAIR_STEP` (1<<26) | the stair step is smoothed for this entity | done | the game's client reads it nowhere; ours the same |
| rdf.underwater | `RDF_UNDERWATER` (1) | the view is under water: the warp (`gl_waterwarp`) and the sound muffle (`s_underwater`) | not-taken | Tail (decided 2026-11-23), cosmetic: ours tints the view under water; warp and muffle missing (C21, C28) |
| rdf.noworldmodel | `RDF_NOWORLDMODEL` (2) | a view with no world (the player-setup figure) | missing | no player figure page (C5) |
| rdf.irgoggles | `RDF_IRGOGGLES` (4) | infra-red goggles on | done | client/demo_q2.cpp keeps the view's bits in `DemoState::rdflags`; `client/demo_video.cpp` marks the warm models |
| rdf.uvgoggles | `RDF_UVGOGGLES` (8) | the other goggles bit | done | the game's client reads it nowhere; ours the same |
| rdf.teleport | `RDF_TELEPORT_BIT` (16) | the extended protocol's teleport mark: no view interpolation this frame | missing | protocol 34 only (C42) |

## Configstrings

done 10, missing 0, not-taken 6

The indexed strings a server sets (stock protocol 34 indices; the extended protocols move every range further on). Ours keeps them in `games/shooters_edge/client/demo_q2.cpp` (`set_string`) for a joined server and a recording; both layouts are read.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cs.name | `CS_NAME` (0) | the level's title: printed with the server data, shown on the loading screen | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours takes the title from the server data; index 0 not kept |
| cs.cdtrack | `CS_CDTRACK` (1) | the music: empty or 0 stops, N plays `music/trackNN` (or `NN`), the mission packs' numbers remapped by `ogg_remap_tracks`, text plays `music/<text>` | done | client/demo_q2.cpp: empty or 0 stops, a number plays that track (remapped for the mission packs by `ogg_remap_tracks`), text plays `music/<text>` (`DemoState::cdname`, `client/take_in.cpp` `demo_music` over `play_level_named`); the same name without case plays on (M18v, sound slice) |
| cs.sky | `CS_SKY` (2) | the sky box's name; changing it changes the sky | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours reads the sky from the map's world entity (`client/demo_content.cpp`); index 2 not kept |
| cs.skyaxis | `CS_SKYAXIS` (3) | the sky's turning axis | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: the map's own `skyaxis` turns the sky (`client/world_draw.cpp`); a server's differing configstring is not kept |
| cs.skyrotate | `CS_SKYROTATE` (4) | the sky's turning speed, degrees a second | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: the map's own `skyrotate` turns the sky (`client/world_draw.cpp`); a server's differing configstring is not kept |
| cs.statusbar | `CS_STATUSBAR` (5) | the status bar's layout program (strings 5 to 28 joined) | done | `games/shooters_edge/client/demo_q2.cpp`, `client/q2_bar.cpp` |
| cs.airaccel | `CS_AIRACCEL` (29) | the server's air acceleration, used by prediction | done | `games/shooters_edge/client/demo_q2.cpp` (`air_accelerate`) |
| cs.maxclients | `CS_MAXCLIENTS` (30) | the player slots; above 1 also turns the cheat settings off | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: kept (`demo_q2.cpp`); the cheat lock not found |
| cs.mapchecksum | `CS_MAPCHECKSUM` (31) | the map's checksum: a mismatch drops the connection ("Local map version differs from server: %i != %s"); in a recording only a warning | done | kept (`client/demo_q2.cpp`); `client/q2_session.cpp` `q2_map_version`, checked in `client/main.cpp` as the map loads: a server's different map drops the game with "Local map version differs from server", a recording's only warns (`tests/shooters_edge_q2_session_test.cpp`) |
| cs.models | `CS_MODELS` (32..) | the models by index; `maps/<map>.bsp` first, `*n` brush models | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.sounds | `CS_SOUNDS` (288..) | the sounds by index; `*name` a figure's own sound | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.images | `CS_IMAGES` (544..) | the pictures by index (`pics/<name>.pcx`) for the layouts | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.lights | `CS_LIGHTS` (800..) | the light styles as letter strings, animated at 10 a second; the game switches them | done | kept from a server or a recording (the settings strings' first 64 from the skins' base less 512, classic and extended), reset at each level; `client/demo_q2.cpp`, `client/demo_client.cpp`, `client/demo.hpp` |
| cs.items | `CS_ITEMS` (1056..) | the item names by index, for the inventory | done | `games/shooters_edge/client/demo_q2.cpp` (`item_names`) |
| cs.playerskins | `CS_PLAYERSKINS` (1312..) | each slot's `name\model/skin`, with the visible weapons | done | `games/shooters_edge/client/demo_q2.cpp` |
| cs.general | `CS_GENERAL` (1568..) | free strings a layout prints (`stat_string`) | not-taken | Tail (decided 2026-11-23), mods only: the general strings range is not kept; only a mod using `stat_string` would show it. |

## The layout language (status bar and layouts)

done 27, missing 0, not-taken 0

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
| lay.rstring | `rstring <text>` | text right-aligned | done | ends at the cursor (`hud/q2_layout.cpp`) |
| lay.rstring2 | `rstring2 <text>` | text right-aligned, alternate colour | done | ends at the cursor, each letter's top bit flipped (`hud/q2_layout.cpp`) |
| lay.stat | `stat_string`, `stat_string2`, `stat_cstring`, `stat_cstring2`, `stat_rstring`, `stat_rstring2` `<stat>` | the configstring whose index a stat holds, in each of the six text forms | done | all six forms (`hud/q2_layout.cpp`) |
| lay.if | `if <stat>` ... `endif` | runs the words between only when the stat is not 0 | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.color | `color <colour>` | sets the colour of the text that follows | done | every form, tinting what follows to the program's end, its alpha under `scr_alpha`'s (`hud/q2_layout.cpp`, `client/q2_bar.cpp`) |
| lay.health_bars | `health_bars <stat> <string>` | a boss's name centred and two health bars from the stat's two bytes (the newer game's) | done | hud/q2_layout.cpp `health_bars <stat> <string>`: the name centred, two bars a third of the screen wide and 4 high at the cursor's y + 12 and + 20 from the stat's two bytes (0 hidden, 1 empty, 255 full), red over grey, solid (`LayoutOp::Kind::fill`, drawn by client/q2_bar.cpp) |
| lay.flash | stat-flashes field | the field picture behind a number while its flash bit is set | done | `games/shooters_edge/hud/q2_layout.cpp` |
| lay.scope | scope pictures | a `scope?x` picture placed at the centred 320 by 240 box is drawn stretched to 320 by 240 times `ch_scale`, centred and moved by `ch_x`/`ch_y` | done | hud/q2_layout.cpp: `pic` of `scope?x` at the centred box drawn 320 by 240 times `ch_scale`, centred on the screen and moved by `ch_x`/`ch_y` (client/q2_bar.cpp passes the crosshair's settings) |
| lay.errors | a bad index in a layout | drops the connection: "invalid stat index for pic", "invalid client index" and the like | done | Same meaning in our words (the player-text rule makes our words the intended ones): a bad layout is refused or skipped with our own line. |

## HUD and screen elements

done 18, missing 0, not-taken 11

What the client draws in 2D over the view, in its drawing order, and the settings that govern each (the settings themselves are rows of the settings table).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| hud.scale | the 2D scale | every 2D element drawn on a virtual screen: the window divided by `scr_scale` (auto 1, 2 or 4 by height) | done | `q2_bar_scale` by the window, `scr_scale` (M18k4 round 34); the desktop's DPI floor not read |
| hud.draw2d | hiding the 2D | `scr_draw2d` 0 none, 1 no status bar, 3 layouts only while F1 is held | done | `scr_draw2d` (`client/q2_view.cpp`, `client/q2_bar.cpp`); at 3 F1 held shows the layouts (`Q2Bar::f1`, set in `client/main.cpp`) |
| hud.crosshair | the crosshair picture | `chN` at the centre plus `ch_x`/`ch_y`, scaled by `ch_scale`, its colour and alpha its own, hidden by the status flags' hide-crosshair bit (extended protocol) | not-taken | Tail (decided 2026-11-23), cosmetic: `engine/hud/crosshair.hpp`; the hide bit missing (C27) |
| hud.hitmarker | the hit marker | the `marker` picture, red, fading over `scr_hit_marker_time`; on by default | done | `client/q2_bar.cpp` `draw_q2_hit_marker`: the pack's `marker` in red at the centre, faded by `scr_hit_marker_time` (`plugins/classic/q2_fx_switches.hpp` `q2_hit_marker_alpha`); `tests/shooters_edge_q2_screen_test.cpp` |
| hud.graphs | net, time and debug graphs | bars along the bottom (`netgraph`, `timegraph`, `debuggraph`) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `timegraph` and `debuggraph` along the bottom (`client/q2_bar.cpp` `draw_graph`, `client/q2_view.cpp`, `graphheight`/`graphscale`/`graphshift`); `netgraph` is our `r_netgraph` drawn in its corner, not along the bottom, and 2's packet sizes not drawn |
| hud.statusbar | the status bar | the `CS_STATUSBAR` program run each frame, unless the hide-HUD layout bit | done | `games/shooters_edge/client/q2_bar.cpp`, `hud/q2_layout.cpp` |
| hud.layout | the layout | the server's layout (scoreboard, help computer) while the layout bit is up; in a demo, F1 shows it whenever held | done | `hud/q2_layout.cpp`; F1 held in a recording shows the last layout sent (`client/q2_bar.cpp`, `Q2Bar::f1` 2) |
| hud.inventory | the inventory screen | the item list with counts, the selected one marked, while the inventory bit is up | done | `games/shooters_edge/server/q2_screens.cpp` (the rules' screen) and `client/q2_bar.cpp` |
| hud.centerprint | the centre print | centred on a quarter of the height, `scr_centertime`, faded over 300 ms, a queue of 4 on the extended protocol, typewriter prints at `scr_printspeed` | not-taken | Tail (decided 2026-11-23), cosmetic: ours the stock client's cut and place, no fade (C9) |
| hud.net | the lost-connection picture | the `net` picture blinking in the lag graph's box when 128 moves are unanswered | done | `client/q2_bar.cpp` (M18k4 round 34) |
| hud.laggraph | the lag graph | 48 columns of round trips, lost and held frames coloured (`scr_lag_*`) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `engine/hud/net_graph.cpp` (M18k4 round 34); a frame held back for the rate not told apart |
| hud.draw | drawn values | `draw` objects: a setting or macro at a place, every frame | done | `engine/hud/draw_strings.cpp` (`draw`, `undraw`, the list and its words), added for the game in `hud/layout.cpp` (`tests/hud_draw_strings_test.cpp`) |
| hud.chathud | chat lines on screen | the last `scr_chathud_lines` chat lines (`scr_chathud*`) | done | `games/shooters_edge/hud/layout.cpp` chat feed (M18k4 round 32); under Quake II's own bar drawn alone of the HUD (`keep_chat_only`) in the bar's letters |
| hud.turtle | network trouble words | CLIENTPRED, BADFRAME, OLDFRAME, OLDENT, NODELTA (and 3 more at 2) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: client/q2_bar.cpp `add_q2_turtle` under `scr_showturtle`: SUPPRESSED (at 2), BADFRAME, OLDFRAME, NODELTA, in the game's order one letter in; CLIENTPRED and CLIENTDROP need the newer protocols' frame bits, OLDENT can't happen (each frame keeps its own entities); SERVERDROP (a gap in the server's packets) missing |
| hud.pause | the pause picture | the pack's `pause` centred while paused (`scr_showpause`) | done | `games/shooters_edge/client/scores.cpp` (the plaque) |
| hud.loading | the loading plaque | the pack's `loading` centred while a level loads, unless the console or a menu is up; the sounds stop; gives up after 120 s ("Loading plaque timed out.") | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: the plaque drawn, given up after 120 s with its line (`games/shooters_edge/client/map_load.cpp`); the music's stop missing (C24) |
| hud.demobar | the demo bar | the share of the file read as a bar and a percentage, with `scr_demobar 2` the time; "[PAUSED]" | not-taken | Our design: ours `demo_bar` and the timeline (C39) |
| hud.notify | the notify lines | the last `con_notifylines` printed lines at the top for `con_notifytime` | done | `engine/console/view.cpp` |
| hud.chatinput | the chat input line | "say:" or "say_team:" and the typed text at the top | done | `engine/console/talk.cpp` |
| hud.blend | the screen tint | the player state's blend (damage red, pickup yellow, water), off by `cl_blend`/`gl_polyblend`; the extended protocol's damage tint as a frame `gl_damageblend_frac` wide | not-taken | Tail (decided 2026-11-23), cosmetic: the tint landed (`q2-gaps2-client.md` checked); the damage frame missing (C21) |
| hud.viewsize | the screen-size border | a view under 100 % centred with the `backtile` picture round it | done | `client/q2_bar.cpp`'s `draw_tile_clear`: the border round the smaller view tiled with pics/backtile, a tile its own size in the window's pixels |
| hud.locdraw | place markers | `loc_draw` draws each place point in the world | done | `games/shooters_edge/client/world_fx.cpp` (`place_markers`): the ancient head, the nearest bobbing under a white light |
| hud.cinematic | cinematics | a `.cin` (or `.ogv`, `.mkv`, `.mp4`) full-screen with its sound, the known releases' letterbox cropped; any key skips | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `.cin` from the packs (`client/cinematic.cpp`); other formats and the crop missing (C25) |
| hud.endpic | the end pictures | a cinematic name ending `.pcx` shows a still picture until a key | done | `games/shooters_edge/client/cinematic.cpp` (`q2-gaps2-client.md` checked) |
| hud.console | the console | slides down to `con_height` at `scr_conspeed` over `conback`, `con_alpha`, `con_scale`; the program's version at the lower right; a yellow arrow prompt (`#` in `remotemode`) | done | `engine/console/view.cpp`: `con_height`, `con_speed`, `con_scale`, `remotemode`'s `#` prompt; `con_alpha` our `con_bgalpha` (`client/cvar_layers.cpp`) |
| hud.consoleload | the console's loading lines | while connecting: the file downloading with a bar and "nn% (size)", or "Loading <map>...", models, images, clients, sounds in turn | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: the console's foot (`engine/console/view.hpp` `set_foot`, filled by `net/qw_remote.cpp` `remote_foot`): the file downloading with its bar and percent and size, or "Loading <map>..."; the models, images, clients and sounds in turn missing (C23) |
| hud.conclock | the console's clock | `con_clock` | done | `engine/ui/console.cpp` (M18k4 round 32) |
| hud.alpha | 2D opacity | `scr_alpha` for every 2D element but the crosshair | done | `client/q2_bar.cpp` (M18k4 round 34) |
| hud.debugstats | debug overlays | `scr_showstats` (the stats as numbers), `scr_showpmove` (the move state) | not-taken | `client-gaps.md`: developer tools left out |

## Messages printed to the player

done 130, missing 0, not-taken 17

Lines the client prints in the console (and the notify lines), quoted as the reference prints them (`%s`, `%d` stand for the value; `\n` line ends dropped). A row is done when our tree prints the same words. The project's rule that a player's lines read like a game's words (CLAUDE.md, Players are people) may make some differences deliberate; since the regrade of 2026-11-23 a line with the same meaning in our words is done. Internal protocol faults are one row at the end.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| msg.connect.challenge | "Requesting challenge... %i" | each try to get a challenge while connecting | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.connect.request | "Requesting connection... %i" | each try to connect | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.connect.usage | "Usage: %s <server>" | `connect` with no address | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.connect.protoarg | "Second argument to `%s' is now ignored. Set protocol via `cl_protocol' variable." | a second `connect` argument | done | `net/qw_remote.cpp`, in our words (no backquotes: player text) |
| msg.connect.badaddr | "Bad server address" | an address that does not resolve | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.connect.connected | "Connected to %s (protocol %d)." | the server accepted us | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.connect.anticheat | "Loading anticheat, this may take a few moments..." | the server asked for anticheat | not-taken | said only by a build that links the closed anticheat module; this game has none and says so instead (msg.connect.anticheatreq) |
| msg.connect.noanticheat | "Trying to connect without anticheat." | anticheat could not load | not-taken | said only by a build that links the closed anticheat module |
| msg.connect.anticheatreq | "Anticheat required by server, but no anticheat support linked in." | the server insists on anticheat | done | `net/q2_client.cpp`: ` ac=2` or more in the connect answer says "The server asks for anticheat, which this game does not have; joining without it." and joins |
| msg.connect.passiverecv | "Received passive connect from %s." | a server connected to us in passive mode | done | `games/shooters_edge/net/q2_pingservers.cpp` (`passive_step`) |
| msg.connect.notconnected | "Can't "%s", not connected" | a forwarded word typed while not connected | done | `engine/console/console_more.cpp` (`complete`): the same words |
| msg.follow | "Following %s..." | `followip` | done | `games/shooters_edge/client/chat_lines.cpp`: `followip` says it |
| msg.follow.none | "No IP address to follow." | `followip` with nothing seen | done | Same meaning in our words (the player-text rule makes our words the intended ones). `games/shooters_edge/client/chat_lines.cpp`: ours says "No address has been seen in chat yet." (sixteen kept, counted from 1) |
| msg.passive.off | "No longer listening for passive connections." | `passive` off | done | `games/shooters_edge/net/q2_pingservers.cpp` |
| msg.passive.on | "Listening for passive connections at %s." | `passive` on | done | `games/shooters_edge/net/q2_pingservers.cpp` |
| msg.reconnect.none | "No server to reconnect to." | `reconnect` with no last server | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.reconnect.loop | "Can not reconnect to loopback." | `reconnect` to the local server | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.reconnect | "Reconnecting..." | `reconnect` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.changing | "Changing map..." | the server changes map | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.disconnected | "Disconnected from server" | the player disconnected (an error line) | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.serverdisc | "Server disconnected" | the server dropped us | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.serverdisc.re | "Server disconnected, reconnecting" | the server asked us to reconnect | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.reset | "Server connection was reset." | the connection was refused | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.timeout | "Server connection timed out." | `cl_timeout` passed | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.protover | "Requested protocol version %d, but server returned %d." | a forced `cl_protocol` the server does not speak | done | `net/q2_client.cpp`: the connection dropped with it |
| msg.demoproto | "Demo uses unsupported protocol version %d." | a demo of an unknown protocol | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.r1q2enh | "'Enhanced' R1Q2 servers are not supported" | an enhanced protocol-35 server | done | `net/q2_proto_in.cpp`: the connection dropped with "This client does not support the enhanced form of protocol 35" |
| msg.maptitle | "(the map title between two rule lines)" | the server data: the level's title in a ruled block | done | Same meaning in our words: the level's name printed in our own form. |
| msg.rcon.usage | "Usage: %s <command>" | `rcon` alone | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.rcon.nopass | "You must set 'rcon_password' before issuing an rcon command." | `rcon` without a password | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.rcon.noaddr | "You must either be connected, or set the 'rcon_address' cvar to issue rcon commands." | `rcon` with nowhere to send | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.rcon.badaddr | "Bad address: %s" | a bad `rcon_address`, `serverstatus` or `remotemode` address | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.status.head | "Status response from %s" | `serverstatus` answer | done | `games/shooters_edge/net/qw_serverstatus.cpp` |
| msg.status.cols | "Num Score Ping Name" | `serverstatus` player table | done | `games/shooters_edge/net/qw_serverstatus.cpp` |
| msg.status.usage | "Usage: %s [address]" | `serverstatus` misuse | done | `games/shooters_edge/net/qw_serverstatus.cpp` |
| msg.dl.progress | "[UDP] %s [%s] [%d remaining file%s]" | a download finished, with how many remain | done | `games/shooters_edge/net/q2_client.cpp` (`take_downloads`): [DONE], [FAIL] (refused) and [STOP] (cut off) with the files left |
| msg.dl.open | "[UDP] Couldn't open %s for writing: %s" | a download's file cannot be written | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.dl.rename | "[UDP] Couldn't rename %s to %s: %s" | the finished `.tmp` cannot be renamed | not-taken | the `.tmp` is not renamed: a file fetched whole is kept in its archive or loose (`classic/content/downloads.hpp`), and its `.tmp` removed |
| msg.dl.mustconnect | "Must be connected to a server." | `download` while not connected | done | `games/shooters_edge/net/q2_remote.cpp` (`net/q2_client.cpp` `q2_download`) |
| msg.dl.disabled | "Downloads are permanently disabled." | `download` with `allow_download -1` | done | `games/shooters_edge/net/q2_remote.cpp` (`net/q2_client.cpp` `q2_download`) |
| msg.dl.usage | "Usage: download <filename>" | `download` alone | done | `games/shooters_edge/net/q2_remote.cpp` (`net/q2_client.cpp` `q2_download`) |
| msg.dl.fail | "Couldn't download %s: %s" | the server refused a file | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.dl.mapload | "Couldn't load %s: %s" | the map cannot be loaded (missing or damaged): the connection drops | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.dl.mapversion | "Local map version differs from server: %i != %s" | the map on disk is not the server's: the connection drops | done | `games/shooters_edge/client/q2_session.cpp` (`q2_map_version`), checked as the level loads: the game is left |
| msg.dl.demomapversion | "Local map version differs from demo: %i != %s" | the map on disk is not the recording's: a warning, playback goes on | done | `games/shooters_edge/client/q2_session.cpp` (`q2_map_version`), checked as the level loads: the recording plays on |
| msg.map.novis | "Map has no Visibility, expect bugs and bad performance!" | a map loaded with no visibility data (a warning) | done | `games/shooters_edge/client/map_load.cpp` (`kNoVisibility`): a Quake II map this window loads |
| msg.http.server | "[HTTP] Download server at %s" | the server named an HTTP download URL | done | `games/shooters_edge/net/q2_http.cpp` (`q2_http_connect`): said at connect, with the refusals of an address not http or https and of one too long |
| msg.http.progress | "[HTTP] %s [%s, %s/sec] [%d remaining file%s]" | an HTTP download finished | done | `games/shooters_edge/net/q2_http.cpp` (`q2_http_step`, `q2_size_text`): each file fetched said with its size, speed and the files left; a failed one with its status or reason |
| msg.http.fail | "[HTTP] Failed to fetch '%s': %s" | an HTTP download failed | done | `games/shooters_edge/menu/servers_page.cpp` (`web_frame`): a web list of servers in `sb_masters` that fails, with its status or reason |
| msg.http.filelist | "[HTTP] Filelist is requesting a .%s file '%s'" | a filelist names a file of a guarded kind | done | `games/shooters_edge/net/q2_http.cpp` (`read_list`): a pack a file list names is said and fetched first, alone |
| msg.http.illegal | "[HTTP] Illegal file type '%s' in filelist." | a filelist names a forbidden kind | done | `games/shooters_edge/net/q2_http.cpp` (`read_list`): with the illegal path and `@` on a pack refused too |
| msg.demo.notrec | "Not recording a demo." | `stop`, `suspend` or `resume` while not recording | done | the same words in `games/shooters_edge/client/listen_record.cpp` |
| msg.demo.stopped | "Stopped demo (%s)." | `stop`, with the statistics | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.demo.begin | "Begin client demo recording." | recording starts | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.demo.already | "Already recording (%s)." | `record` while recording | done | the same words in `games/shooters_edge/client/listen_record.cpp` |
| msg.demo.nofile | "Missing filename argument." | `record` or `writeconfig` without a name | done | `engine/console/console_more.cpp` (`writeconfig`); `record` keeps its own |
| msg.demo.mustlevel | "You must be in a level to record." | `record` while not in a level | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.demo.to | "Recording client demo to %s." | recording starts | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.demo.resumed | "Resumed demo recording." | `resume` | not-taken | `client-gaps.md` round 27: suspend and resume not taken |
| msg.demo.alreadyresumed | "Demo recording is already resumed." | `resume` while not suspended | not-taken | `client-gaps.md` round 27 |
| msg.demo.suspended | "Suspended demo recording." | `suspend` | not-taken | `client-gaps.md` round 27 |
| msg.demo.alreadysusp | "Demo recording is already suspended." | `suspend` twice | not-taken | `client-gaps.md` round 27 |
| msg.demo.toomany | "Too many demo frames don't fit into %u bytes!" | a frame too big for `cl_demomsglen` (with "Try to increase 'cl_demomsglen' value and restart recording.") | not-taken | `client-gaps.md`: `cl_demomsglen` not taken |
| msg.demo.finished | "Demo finished" | a demo played to its end | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.demo.cantread | "Couldn't read %s: %s" | `demo` of a file that cannot be read | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours says it in its own words (verify `client/demo_playback.cpp`) |
| msg.demo.nomvd | "MVD support was not compiled in." | an MVD demo without MVD support | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.seek.usage | "Usage: %s [+-]<timespec\|percent>[%%]" | `seek` misuse | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.notplaying | "Not playing a demo." | `seek` with no demo | done | the same words in `plugins/shooter/demo_transport.cpp` (`DemoCalls::playing`) |
| msg.seek.pct | "Invalid percentage." | `seek` with a bad percentage | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.size | "Unknown file size, can't seek." | `seek` by percent on a stream | done | "The demo's length is not known, so it cannot jump by a share of it." when the demo could not be scanned (`client/demo_playback.cpp`) |
| msg.seek.rel | "Invalid relative timespec." | `seek` with a bad relative time | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.abs | "Invalid absolute timespec." | `seek` with a bad time | done | the same words in `plugins/shooter/demo_transport.cpp` |
| msg.seek.back | "Couldn't seek backwards without snapshots!" | `seek` back with `cl_demosnaps 0` | not-taken | ours with no snapshots seeks back by reading again from the start, so it never refuses |
| msg.timedemo | "%u frames, %3.1f seconds: %3.1f fps" | a `timedemo` ended | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.added | "Added location %s at %s" | `loc add` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.deleted | "Deleted location %s at %s" | `loc del` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.renamed | "Renamed location at %s to %s" | `loc set` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.none | "No closest location." | `loc del` or `set` with none near | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.nolist | "No locations to list." | `loc list` with none | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.listed | "%d location%s listed" | `loc list` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.nowrite | "No locations to write." | `loc save` with none | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.wrote | "Wrote %d location%s to %s" | `loc save` | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.usage | "Usage: %s <add\|del\|set\|list\|save>" | `loc` misuse | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.loc.line | "Line %d is incomplete in %s" | a bad line in a `.loc` file | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.mustlevel | "Must be in a level." | `loc`, `aashot` and others while not in a level | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.con.usage | "Usage: %s <filename>" | `condump`, `demo`, `dumpstatusbar` misuse | done | Same meaning in our words (the player-text rule makes our words the intended ones). `games/shooters_edge/client/q2_bar.cpp`: `dumpstatusbar` and `dumplayout` say it; ours `condump` writes `condump.txt` without a name, `demo` has its own words |
| msg.con.dumped | "Dumped console text to %s." | `condump` | done | `engine/console/view.cpp` (#1248) |
| msg.con.chatlevel | "You must be in a level to chat." | `messagemode` while not in a level | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.con.remoteusage | "Usage: %s <address> <password>" | `remotemode` misuse | done | `engine/console/view.cpp`: `remotemode` with no address given and none set |
| msg.con.badvalue | "Invalid value '%s' for '%s'" | a bad colour or value for a setting | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.bind.unbindusage | "unbind <key> : remove commands from a key" | `unbind` alone | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.bind.usage | "bind <key> [command] : attach a command to a key" | `bind` alone | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.bind.badkey | ""%s" isn't a valid key" | `bind` or `unbind` of an unknown key | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.bind.show | ""%s" = "%s"" | `bind <key>` of a bound key | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.bind.notbound | ""%s" is not bound" | `bind <key>` of a free key | done | the same words in `engine/console/console_binds.cpp` |
| msg.ignore.none | "No ignore filters." | `ignoretext` with none | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.list | "Current ignore filters:" | `ignoretext` lists them with "(n hits)" | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.short | "Match string "%s" is too short." | `ignoretext` of a too-short pattern | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.notfound | "Can't find ignore filter "%s"" | `unignoretext` of an unknown one | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.ignore.removed | "Removed %d ignore filter%s." | `unignoretext` | done | the same words in `engine/console/chat_ignore.cpp` |
| msg.stuff.ignored | "Ignored stufftext: %s" | a pushed command refused by `cl_ignore_stufftext` | done | `plugins/classic/protocol/stuff_filter.cpp` (`stuff_ignored_line`), said by `games/shooters_edge/net/q2_remote.cpp` for a Quake II server joined |
| msg.writeconfig.help | "Save current configuration into file." | `writeconfig` help | done | `engine/console/console_more.cpp` |
| msg.writeconfig.wrote | "Wrote %s." | `writeconfig`, `aashot`, `dumpstatusbar` done | done | `engine/console/console_more.cpp` |
| msg.write.error | "Error writing %s" | a file could not be written | done | `engine/console/console_more.cpp` |
| msg.dump.mustlevel | "Must be in a level to dump." | `dumpclients`, `dumpstatusbar` outside a level | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.dump.none | "No %s to dump." | `dumplayout` with none | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.dump.done | "Dumped %s program to %s." | `dumpstatusbar`, `dumplayout` | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.skins.mustlevel | "Must be in a level to load skins." | `skins` outside a level | done | `games/shooters_edge/net/q2_remote.cpp` |
| msg.skins.line | "client %d: %s --> %s/%s" | `skins` per player | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/net/q2_remote.cpp`: the look asked on the right |
| msg.userinfo | "User info settings:" | `userinfo` | done | `engine/console/console_more.cpp` |
| msg.play.usage | "Usage: %s <sound> [...]" | `play` alone | done | `games/shooters_edge/client/menu_sound.cpp` |
| msg.vidrestart | "Manual `vid_restart' command ignored." | a typed `vid_restart` (with an explanation that video settings apply themselves) | done | `games/shooters_edge/client/window_cvars.cpp` |
| msg.fps.round | "%s value `%d' is inexact and will be rounded to `%d'." | a `cl_maxfps` that is not whole milliseconds (with "(Set `cl_warn_on_fps_rounding' to `0' to disable this warning.)") | done | `games/shooters_edge/client/q2_session.cpp` (`q2_fps_warning`, `cl_warn_on_fps_rounding`): said without the quote marks; `r_maxfps` is not rounded to whole milliseconds here, so it never warns |
| msg.mouse.off | "Mouse input disabled." | the mouse could not start | done | `client/main.cpp`: the system refusing the mouse says once "The mouse could not be taken, so it does not turn the view." |
| msg.draw.none | "No draw strings registered." | `draw` or `undraw` with none | done | `engine/hud/draw_strings.cpp` |
| msg.draw.list | "Name X Y (table of draw strings)" | `draw` alone lists them | done | `engine/hud/draw_strings.cpp` |
| msg.draw.usage | "Usage: %s <name> <x> <y> [color]" | `draw` misuse | done | `engine/hud/draw_strings.cpp` |
| msg.draw.color | "Unknown color '%s'" | `draw` with a bad colour | done | `engine/hud/draw_strings.cpp` |
| msg.draw.deleted | "Deleted all draw strings." | `undraw all` | done | `engine/hud/draw_strings.cpp` |
| msg.draw.notfound | "Draw string '%s' not found." | `undraw` of an unknown name | done | `engine/hud/draw_strings.cpp` |
| msg.sky.usage | "Usage: sky <basename> [rotate] [axis x y z]" | `sky` alone | done | the same words in `games/shooters_edge/client/render_words.cpp` |
| msg.sky.nomap | "No map loaded." | `sky` or `timerefresh` outside a level | done | the same words in `games/shooters_edge/client/render_words.cpp`, for both words |
| msg.timerefresh | "%f seconds (%f fps)" | `timerefresh` result | done | the same form in `games/shooters_edge/client/render_words.cpp` (`refresh_line`) |
| msg.loadtimeout | "Loading plaque timed out." | a level that took over 120 s to load | done | the same words in `games/shooters_edge/client/map_load.cpp` (`take_load`, `kPlaqueMostS`), the spinner in the plaque's place |
| msg.fog.none | "No fog." | `fog` with none set | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.fog.usage | "Usage: %s <r g b density> [sky_factor]" | `fog` misuse | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.menu.nomenu | "No such menu: %s" | `pushmenu` of an unknown page | done | `games/shooters_edge/menu/game_menu.cpp` |
| msg.menu.usage | "Usage: %s <menu>" | `pushmenu` alone | done | `games/shooters_edge/menu/game_menu.cpp` |
| msg.menu.noplayers | "No player models found." | the player page with no figures | done | `games/shooters_edge/menu/main_menu.cpp` (`apply_event`): said, and the player page does not open |
| msg.menu.script | "Unknown keyword '%s'" | a bad line in the menu script (and its other warnings) | done | `menu/menu_script.cpp`: each told in the console with the file and line and what was done, the same outcomes ("'x' is not a menu script word, so the line is skipped." inside a page, "... so the rest of the file is not read." outside one; a row's form, a bad bit or width, a page with no rows or no end, a missing include, includes too deep, an unclosed quote, a line too long) |
| msg.servers.http | "Can't fetch '%s', no HTTP support compiled in." | an HTTP server list without HTTP | not-taken | HTTP is always built in, and a secure (https) list is fetched over TLS (`engine/net/tls_stream.hpp`) |
| msg.servers.badaddr | "Bad server address: %s" | a bad line in a server list | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours does it but says it in its own words (the exact line not found) |
| msg.snd.notstarted | "Sound system not started." | `soundinfo` or `soundlist` with no sound | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/client/menu_sound.cpp`: said by `soundinfo` with no sound device; `soundlist` is still missing |
| msg.ogg.stopped | "Playback stopped." | `ogg info` with no music, and `ogg stop` | done | `engine/audio/music_console.cpp` |
| msg.ogg.cin | "Can't play music in cinematic mode." | `ogg play` during a cinematic | done | `engine/audio/music_console.cpp` (`MusicConsole::held`, set by the window while a cinematic shows): `ogg play` refused |
| msg.ogg.usage | "Usage: %s <info\|play\|stop\|next\|pause>" | `ogg` alone or with an unknown word | done | `engine/audio/music_console.cpp` |
| msg.ogg.notrack | "No such track: %s" | `ogg play` of a track that is not found | done | `engine/audio/music_console.cpp` |
| msg.ogg.playusage | "Usage: %s %s <track>" | `ogg play` with no track | done | `engine/audio/music_console.cpp` |
| msg.al.init | "OpenAL initialized." | the OpenAL backend started | not-taken | one sound path for every game (the platform's device under the engine's synth); `s_enable 2` is the same as 1, so no second backend starts |
| msg.al.fail | "Failed to initialize OpenAL: %s" | the OpenAL backend failed | not-taken | no second sound backend (msg.al.init) |
| msg.al.devices | "Available devices:" / "No devices available" | `al_device ?` lists the OpenAL output devices | done | `al_device` is the engine's `s_device`: `?` or `s_devices` prints "Available devices:" and each, or "No sound devices." (`engine/platform/window_cvars.cpp`) |
| msg.gtv.listen | "Listening for GTV connections." | `gtv start` | not-taken | experimental and off in released builds (a build option, left out of the release packages); watching a match is ours through TV (`engine/tv`, `setv`) and the server's multiview stream (`net/q2_mvd_server.hpp`, `sv_mvd_enable 2`), so a playing client never serves its own game (decided by the owner, 2026-11-23) |
| msg.gtv.accepted | "Accepted MVD client [%s]" | a TV viewer joined the client's relay | not-taken | experimental and off in released builds (a build option, left out of the release packages); watching a match is ours through TV (`engine/tv`, `setv`) and the server's multiview stream (`net/q2_mvd_server.hpp`, `sv_mvd_enable 2`), so a playing client never serves its own game (decided by the owner, 2026-11-23) |
| msg.aashot.full | "All scoreshot slots are full." | `aashot` with 1000 files already | done | `games/shooters_edge/client/q2_bar.cpp` |
| msg.cin.open | "Couldn't open %s: %s" | a cinematic that cannot be opened (and its other errors) | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours says it in its own words (verify `client/cinematic.cpp`) |
| msg.serverbug | "SERVER BUG: %s on entity %d last seen %d frames ago" | a server sent a delta for an entity the client never had (developer) | not-taken | a debugging build's check only (a positionless sound or a player's gun flash naming an entity outside the frame, with `developer`); a released client never prints it |
| msg.faults | "%s: bad index / bad entity / invalid areabits length / illegible server message: %d ..." | the parse errors that drop the connection, each naming the function and the value | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours refuses with its own words naming the byte (`client/demo_q2.cpp`) |

## Demo playback and recording features

done 13, missing 0, not-taken 10

What a player can do with client recordings (`.dm2`) and multi-view recordings (`.mvd2`) in the client.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| demo.play.dm2 | play a `.dm2` | `demo <name>` plays a client recording at its own pace, the view the recorded player's | done | `games/shooters_edge/client/demo_q2.cpp`, `client/demo_playback.cpp` |
| demo.play.gz | play a gzipped recording | `.dm2.gz` and `.mvd2.gz` read directly | done | `playdemo` unpacks a packed recording first, `.gz` among them (`client/demo_pack.hpp`, `engine/vfs/unpack.hpp`; `client/main.cpp`) |
| demo.play.mvd2 | play a multi-view recording | an `.mvd2` handed to the built-in server, which plays it as a broadcast | done | `games/shooters_edge/client/demo_mvd2.cpp` |
| demo.play.paths | where demos are read | `demos/` unless the name starts with `/` (then from the root of the search path); Tab completes | not-taken | Our design: ours from the demo page and any path (C40) |
| demo.play.fov | the field of view while playing | the player's own `fov` replaces a recorded field of 90 or more; `uf` 4 always own, 32 always recorded | done | `client/q2_view.hpp` (`q2_shown_fov`): the player's own `fov` over a recorded 90 or more, `uf` 4 and 32 (`tests/shooters_edge_q2_view_test.cpp`) |
| demo.play.f1 | the scoreboard while playing | holding F1 shows the recorded layout whether or not it was up | done | `client/q2_bar.cpp`: F1 held in a recording (`Q2Bar::f1` 2, `client/main.cpp`) shows the last layout whether or not it was up |
| demo.play.shift | free look while playing | holding SHIFT turns the view by the player's own mouse | done | `client/demo_free_look.cpp`: an offset on the recorded look while held, dropped when let go |
| demo.play.pause | pausing a demo | `pause` or the console/menu (`cl_autopause`) pauses playback; "[PAUSED]" on the bar | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours pauses by the transport (`plugins/shooter/demo_transport.cpp`); the auto-pause missing (C24) |
| demo.play.seek | seeking | `seek` forward and back by frames, seconds, mm:ss.ff or percent, back from the snapshots every `cl_demosnaps` s | done | `seek` (`plugins/shooter/demo_transport.cpp`) with every form; a percent is of the demo's length, not its bytes; back from the last snapshot (`Timedemo::snaps`, every `cl_demosnaps` s) |
| demo.play.bar | the progress bar | `scr_demobar` | not-taken | Our design: our demo bar and timeline (C39) |
| demo.play.end | the end of a demo | prints "Demo finished" and disconnects, or holds the last frame with `cl_demowait` | done | `games/shooters_edge/client/demo_playback.cpp` |
| demo.play.timedemo | timing a demo | `timedemo 1` plays as fast as frames draw and prints frames, seconds and frames a second | done | `games/shooters_edge/client/demo_playback.cpp` |
| demo.play.browser | the demo browser | the menu's demo page (table "Menu pages and rows") | not-taken | Our design: the menu's demo page (C40) |
| demo.play.compat | old recordings | a recording of an older kind (protocol 34 without the extended frames) plays; the bar is not drawn for it | done | Checked 2026-11-23: protocol 34 and the extended layouts play (`client/demo_q2.cpp`). |
| demo.rec.start | `record <name>` | records into `demos/<name>.dm2` from the current state (a late start plays); can record while playing another demo | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `client/listen_record.cpp`, `net/q2_client.cpp`; recording while playing a demo not found |
| demo.rec.stats | statistics | `record` while recording and `stop` print the frames, size and dropped messages | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: client/listen_record.cpp: `record` while recording says `Already recording (<size>).`, `stop` says `Stopped demo (<size>).` in decimal units (`q2_demo_size`); the time recorded and the dropped counts missing (ours drops nothing: it copies the server's messages whole) |
| demo.rec.gzip | `record -z` | gzip-compressed recording | done | client/listen_record.cpp `record -z` adds `.gz`; net/qw_match_record.cpp writes the file through gzip (engine/core/deflate.hpp's stream), the size said what went in |
| demo.rec.size | `record -e` / `-s` | the extended 4086-byte or standard 1390-byte frames | not-taken | `client-gaps.md` (C38): `cl_demomsglen` not taken |
| demo.rec.downgrade | protocol downgrade | a recording made on a modern protocol is written as protocol 34; too-large frames dropped, reliable messages kept first | not-taken | Our design: ours speaks 34 only, so nothing to downgrade |
| demo.rec.suspend | `suspend` / `resume` | pause and resume a recording | not-taken | `client-gaps.md` round 27 |
| demo.rec.auto | recording by a trigger | `cl_beginmapcmd "record ..."` starts a recording on each map | done | `client/q2_session.cpp`: `cl_beginmapcmd` (and `cl_changemapcmd`, `cl_disconnectcmd`) run on each map joined or hosted (`tests/shooters_edge_q2_session_test.cpp`) |
| demo.gtv | the client's own TV relay | `gtv start` serves the player's own game to TV viewers over TCP (only in a build that asks for it) | not-taken | experimental and off in released builds (a build option, left out of the release packages); watching a match is ours through TV (`engine/tv`, `setv`) and the server's multiview stream (`net/q2_mvd_server.hpp`, `sv_mvd_enable 2`), so a playing client never serves its own game (decided by the owner, 2026-11-23) |
| demo.aashot | text scoreboard shots | `aashot` writes the scoreboard as text; `aadump` prints it | done | `client/q2_bar.cpp`: `aashot [name]` writes the layout as text to `scoreshots/`, `aadump` prints it |

## Keys: the fixed keys and the default bindings

done 10, missing 0, not-taken 1

Keys the client handles itself (a player cannot unbind them) and where the default bindings come from.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| key.console | the console key | ` and ~ (without SHIFT) always toggle the console | done | Checked 2026-11-23: the key under ESCAPE toggles the console (`engine/console/view.cpp`). |
| key.altenter | Alt+Enter | toggles full screen, always | done | `engine/platform/window_cvars.cpp` `alt_enter` |
| key.escape.menu | ESCAPE | closes the console (or opens the main menu before a game), leaves chat, opens the in-game page in a game, the main page out of one | done | `games/shooters_edge/menu/game_menu.cpp`: in a level the in-game page (`MenuTable::in_level`) |
| key.escape.putaway | ESCAPE with a screen up | puts away the help computer or inventory (`putaway`); held, opens the menu | done | `games/shooters_edge/menu/game_menu.cpp`: ESCAPE with a screen up sends `putaway`; held on half a second, the menu's in-game page opens |
| key.fkeys.menu | F1 to F12 in a menu | still run their bindings | done | `engine/ui/console_keys.cpp` (`ConsoleKeys::menu`, set by the window while a menu is up): F1 to F12 and the console key alone run their binds; a key let go always releases |
| key.cinematic | any button key in a cinematic | skips it | done | `games/shooters_edge/client/cinematic.cpp` |
| key.shift.demo | SHIFT in a demo | takes the mouse for free look | done | `client/demo_free_look.cpp`, the capture in `client/main.cpp` |
| key.f1.demo | F1 in a demo | shows the recorded layout | done | `games/shooters_edge/client/q2_bar.cpp` (`Q2Bar::f1`, set by the window): held in a recording, the last layout sent; with `scr_draw2d 3` it shows the layouts that hides |
| key.buttons | button keys | a `+` binding gets the key and time; its release runs the `-` form even in the console | done | `engine/console/binds.cpp` |
| key.keypad | the keypad in the console | the keypad's keys type their characters in the console | done | Checked 2026-11-23: the keypad keys are ordinary keys to bind and type (`engine/console/view.cpp`). |
| key.defaults | the default bindings | none built in: the game's `default.cfg` (in the pack) binds the keys, then `config.cfg`; `bindlist` shows them | not-taken | Our design: the pack's `default.cfg` runs in Quake II's start order, and the window's move keys are kept over it on purpose (`client/install_configs.cpp`). |

## Sound and music

done 13, missing 0, not-taken 0

How the client makes sound, beyond the settings themselves.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| snd.spatial | positioned sounds | each sound placed by its entity (moving with it) or a point, by volume and attenuation, on 8 channels a entity; the listener at the view | done | `games/shooters_edge/client/demo_sound.cpp`, `client/demo_heard.cpp` |
| snd.loops | entity loop sounds | an entity's sound field loops while it is in the frame, merged by sound | done | landed; `s_ambient` 0 to 3 chooses which loops are heard (`client/demo_sound.cpp`, `plugins/classic/q2_fx_switches.hpp`) |
| snd.underwater | under water | every sound high-shelf filtered at 5 kHz (`s_underwater`) | done | the eye in a liquid, `engine/audio/synth.cpp` `muffle` (M18k4 round 34) |
| snd.footsteps | footsteps | a step set by the surface under the entity (`.mat` files, up to 15 steps), never the same twice in a row | done | `plugins/classic/q2_footsteps.hpp`: `textures/<name>.mat` names a material (15 characters, letters, digits, `_`, `-`; else "Bad material"), `player/steps/<material>N.wav` up to 15, the ladder's own set, the default's fallback; the surface found by a flat box swept down (9 under the box, 75 with none), shallow water over its floor; never the step heard last (M18v, sound slice) |
| snd.landing | landings | `player/land1.wav` on a short fall | done | `games/shooters_edge/client/demo_q2.cpp` |
| snd.chat | the chat beep | `misc/talk.wav` (or `talk1.wav`) on a chat line | done | `cl_chat_sound` (M18k4 round 32) |
| snd.backends | the sound back-ends | the software mixer (any rate, `s_khz`) or OpenAL (HRTF) | done | one mixer (`engine/audio`, 48 kHz) that does what the OpenAL path adds: the output device (`al_device`), the two ears (`al_hrtf`), the muffle under water, the looped sounds merged (`al_merge_looping`); `s_khz` not taken (cvar.s_khz) |
| snd.focus | sound and the window | `s_auto_focus` mutes when minimised or unfocused | done | `s_auto_focus` (`engine/platform/window_cvars.hpp` `heard_volume`), every game |
| snd.figure | a figure's own sounds | a `*name` sound plays the figure's own `players/<model>/name`, else the male one | done | `client/demo_sound.cpp` `DemoSounds::own`: a `*name` plays `players/<model>/name`, else the male one |
| snd.music.track | the level's music | configstring 1: empty or 0 stops, N `music/trackNN` or `music/NN` (remapped for the mission packs), text `music/<text>`, tried as `.flac`, `.opus`, `.ogg`, `.mp3`, `.wav`, loose files in the search path and the install and home folders; repeats | done | ours plays the world's `sounds` track in its own game (`engine/audio/music_console.cpp`) and the track a joined server or a recording names (`client/take_in.cpp` `demo_music`), a number or a file's name (`play_level_named` over `classic/content/soundtrack.hpp`'s `music_file`: under the game's `music/` folder, any case, extension optional; FLAC, the one format the music is kept in) |
| snd.music.shuffle | shuffle | `ogg_shuffle` plays every file in `music/` shuffled | done | `ogg_shuffle` bound to the music player's shuffle (`client/cvar_layers.cpp`, `engine/audio/music_console.hpp`): every kept track in a random order |
| snd.music.stop | when music stops | at a map change, the loading plaque and a cinematic; while disconnected the `ogg_menu_track` plays (none by default) | done | `engine/audio/music_console.cpp`: a level's track changes only when its name does; a movie stops the music and forgets it, the next level's track starting afresh; `ogg_enable` 0 pauses it where it is and 1 resumes it (`off_pauses`); while disconnected `ogg_menu_track` plays (0 none) (M18v, sound slice) |
| snd.cinematic | cinematic sound | a cinematic's own audio track | done | `games/shooters_edge/client/cinematic.cpp` |

## Picture and video

done 16, missing 0, not-taken 4

How the client sets up and draws the picture, beyond the settings themselves. Ours draws with its own renderer (`engine/render`, `plugins/classic/app/look.cpp`); the GL driver's internals are not taken (`client-gaps.md`).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| vid.window | the window | windowed by `vid_geometry` or a listed full-screen mode | not-taken | Our design: ours full screen on or off (`engine/platform/window_cvars.cpp`) |
| vid.gamma | gamma | `vid_gamma` applied to textures, or by the display (`vid_hwgamma`) | not-taken | Our design: ours `gamma` (synonym `vid_gamma`); no hardware gamma |
| vid.hor | wide screens | the 4:3 field kept vertically (hor+) by `cl_adjustfov` | done | `q2-gaps2-client.md` checked (hor+) |
| vid.fov.server | the server's field of view | the player state's field drawn, interpolated (zoom, fixed FOV) | done | `client/present.cpp`: the player state's field of view eased between frames (`q2_fov_y`, `cl_adjustfov`) |
| vid.areas | areas behind shut doors | a leaf whose area is not joined is not drawn | done | a leaf in an area the frame does not show draws nothing: off a recording or a joined server's frame, and from the listen server's open portals (`brush/map.cpp` `potentially_visible`, `client/demo_q2.cpp`, `server/q2_areas.cpp`) |
| vid.pvs2 | the second PVS point | the PVS joined with that of a point 16 units below (above in water) | done | `plugins/brush/map.cpp` `second_view_leaf` (a version 38 map: 16 units below the eye, above when its leaf holds contents; not solid, another cluster), joined in `potentially_visible` by the cull (`brush/app/bsp_cull.cpp`); `tests/brush_areas_test.cpp` |
| vid.particles | particles | squares facing the view, grown with distance, `gl_partscale`, `gl_partstyle`, `gl_partshape` | done | ours draws the classic particles facing the view, sized by `gl_partscale`'s rule with depth, blended or added by `gl_partstyle`, shaped by `gl_partshape` |
| vid.shadows | blob shadows | `gl_shadows` | done | `gl_shadows` (`client/model_shadow.hpp`), and the menu's ground shadows row (no, yes, fade) |
| vid.shells | colour shells | a second copy pushed out, flat colour, 0.3 | done | `games/shooters_edge/client/q2_look.hpp` |
| vid.ir | infra-red goggles | IR-visible models drawn flat red | done | client/q2_look.cpp `q2_infra_red`, client/world_models.cpp (red, no light, no minimum light) |
| vid.lightstyles | light styles | each style's letters stepped at 10 a second, or blended between letters with `cl_lerp_lightstyles` | done | a server's or a recording's styles reach the window and step at 10 a second as our own game's do (`client/world_fx.cpp`, `client/world_draw.cpp`, `ba::set_light_style`); `cl_lerp_lightstyles` is its own settings row |
| vid.dlights | dynamic lights | effect and flash lights added each frame (`gl_dynamic`, `cl_lights`, `cl_muzzlelight_time`) | done | the wire's flashes read (`client/demo_q2.cpp`, `client/q2_flash_fx.cpp`) and lit with the effects' lights (`client/world_fx.cpp`, `cl_muzzlelight_time`); `gl_dynamic` and `cl_lights` our `r_dynamic` (`client/cvar_layers.cpp`) |
| vid.vweps | other players' weapons | the weapon model from the figure's folder (`cl_vwep`) | done | `plugins/classic/vweps.cpp`; from a recording or a joined server, a player's second model 255 is his weapon by the skin's high byte, and any other second, third and fourth model drawn at his place (`client/demo_video.cpp`, `DemoEntity::more`) |
| vid.skins | player skins | `players/<model>/<skin>.pcx`, missing ones the stock figure (`cl_noskins`) | done | `games/shooters_edge/client/cvar_layers.cpp` (`cl_noskins`); a player's model 255 drawn as the figure and skin his slot's look names (`client/demo_video.cpp` `q2_own_look`, `EffectModel::figure_q2`), from a recording or a joined server too |
| vid.gun | the view weapon | the gun model from the player state, its frame interpolated, by `hand`, `cl_gun`, `cl_gunfov`, `cl_gun_x/y/z`, `cl_gunalpha` | done | landed; `cl_gunalpha` (`view/hand.hpp`, `client/cvar_layers.cpp`) |
| vid.bob | the view | bob, roll, kicks, falls from the player state | done | `plugins/classic/q2_view.cpp` |
| vid.kick | the server's kick | the player state's kick angles added (`cl_kickangles`) | done | added; `cl_kickangles` 0 drops it (`client/present.cpp`, `client/q2_view.hpp`) |
| vid.thirdperson | third person | `cl_thirdperson` with range and angle (cheat); the player fades as the camera comes close | done | `client/q2_session.cpp` (`cl_thirdperson`, `_range`, `_angle`; a cheat; joined and hosted): stopped by doors and platforms too; the player's figure fades as the camera comes close (`own_figure_alpha`) |
| vid.screenshot | screenshots | `screenshot` named `quakeXXX` in `gl_screenshot_format`, written off the main thread | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: C22 |
| vid.replacements | replacement content | `.png`/`.jpg`/`.tga` textures over `.pcx`/`.wal`, MD5 models over MD2 | not-taken | Our design: ours replaces by classic enhance (`plugins/classicenhance`), not loose replacement files; verify |

## Connection, network, downloads and the rest

done 15, missing 1, not-taken 2

Behaviour the client offers outside the screen.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| net.protocols | protocols | 34 (stock), 35 (R1Q2), 36 (Q2PRO) by `cl_protocol` | done | `net/q2_client.cpp` + `net/q2_proto_in.cpp`: 35 and 36 joined, read as protocol 34 by the one reader, recordings written as 34; the server is told the client records, so it leaves nothing out |
| net.extended-limits | protocol extensions while joined | an extended server's flags (36, minor 1024 on) read: the extended configstrings and limits, the wider entity fields, 23-bit coordinates, 64 stats, word gun index, the centred print levels; recordings at 3434 to 3436 | done | `net/q2_proto_in.cpp` (`Q2ProtoIn::ext`): the stream written as the recording of its level (3434, or 3435 for extensions 2 and the fog level, whose fog is read past: the look is ours), entities and baselines carried as they are, the player state through `server/q2_wire.hpp`'s `q2_write_player_ext`; `client/demo_q2.cpp` shows print levels 4 and 5 centred; prediction reads the timer in milliseconds (`net/q2_predict.cpp`) |
| net.predict | prediction | the player's move predicted from the last frame by the shared move code; misses smoothed | done | `games/shooters_edge/net/q2_predict.cpp` |
| net.userinfo | user info while joined | a changed name, skin, hand, fov, rate sent at once | missing | Missing, rank 1 (gameplay, daily use): `net/qw_remote.cpp` `user_info_step`: a changed `rate` or `password` sends the user info at once; a changed name, skin, hand or fov not (C30) LIFT: Quake's `user_info_step` (`net/qw_remote.cpp`) is the shared place. |
| net.cheatlock | the cheat lock | with more than one player slot, cheat-protected settings forced back each frame | done | `engine/console/cvar.hpp`'s `hold_cheats` (any game), held by `net/q2_remote.cpp` while a joined Quake II server has more than one player slot: every cheat back to its default and each set refused whatever the local sv_cheats says; let go when the game is left (`remote_disconnect`) |
| net.stufftext | server-pushed commands | run in the client's console, refused unless whitelisted with `cl_ignore_stufftext` | done | as svc.stufftext: a stuffed `rate`, `set name`, `spectator 0`, a mod's binds run as typed, refused only by `cl_ignore_stufftext`'s whitelist |
| net.triggers | message triggers | `trigger` runs a command when a server line matches; never in demos or on a relay | done | `client/chat_lines.cpp`: `trigger`, `untrigger`, on the lines that are not chat and the centre message; never through a relay |
| net.autoreply | version reply | a chat line ending "!version" makes the client say its version after 1 to 2 s, at most once in 2 minutes | done | `client/chat_lines.cpp` (`chat_line_teamplay`, `chat_reply_due`): on a Quake II server another player's chat line ending in `!version` is answered with the version 1 to 2 s later, at most once in two minutes; `cl_queries` 0 answers none |
| net.recentaddr | addresses in chat | addresses seen in chat remembered for `followip` | done | `client/chat_lines.cpp`: the last 16 addresses chat lines named, joined by `followip [n]` |
| net.download.order | what is fetched | the map, its models (and their skins and frames), sounds, pictures, figures (models, weapons, skins, icons, sounds), the sky, then the wall textures | done | `net/q2_fetch.hpp`: the map and models; then the models' skins and frames, sounds, pictures, figures (model, `weapon.md2` and the level's view weapons, `weapon.pcx`, skin, icon, sounds), the sky; then the map's wall textures; `tests/shooters_edge_q2_net_test.cpp` (`joining_fetches_the_level_stage_by_stage`) |
| net.download.resume | resuming | a `.tmp` resumed from its length, renamed when whole | done | `plugins/classic/content/downloads.hpp` (`HeldDownloads`, `held_in_folder`, any classic game): each piece added to `<path>.tmp` in the game's folder as it comes; asked for again, the rest from its length (`download <file> <offset>`, `net/q2_client.cpp`); dropped once the file is whole and kept; `tests/shooters_edge_q2_net_test.cpp` |
| net.download.http | HTTP downloads | from the server's URL, with filelists, two at once, falling back to UDP | done | `net/q2_http.cpp`: the server's `dlserver`, `cl_http_filelists`, up to 4 at once, back to the channel on failure (`tests/shooters_edge_q2_http_test.cpp`) |
| net.passive | passive connections | wait for a server to connect to us | done | `net/q2_pingservers.cpp` `passive` (`net/qw_remote.cpp`): a server's `passive_connect` joined (`tests/shooters_edge_q2_pingservers_test.cpp`) |
| net.browser | the server list | HTTP list, address book, `servers.lst`, LAN broadcast; pinged at `ui_pingrate` | not-taken | Our design: ours LAN, masters, hub (C41) |
| net.config.write | the saved config | on quit `config.cfg` (`q2config.cfg` on Windows) gets binds and saved settings | done | `engine/console/console_builtins.cpp` (`cfg_save`) |
| net.config.order | the start order | `default.cfg`, the config, `autoexec.cfg`, `postexec.cfg`, the command line, `postinit.cfg` | done | `plugins/classic/content/load_order.cpp`: defaults, the config, the folders' autoexecs, `postexec.cfg`, ours, the command line, `postinit.cfg` |
| net.completion | Tab completion | commands, settings, aliases, macros and each command's own arguments (maps, demos, sounds, settings' values) | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours completes commands and settings (`engine/console`); arguments not compared |
| net.history | the typed history | kept across runs (`con_history`) | done | `engine/console/view.cpp` (`load_history`, `save_history`): `.conhistory` beside the config, one line each, oldest first, read when the config has run and written with the settings; blanks and a repeated line left out; `con_history` 0 keeps none across runs |
