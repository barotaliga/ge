# Quake and QuakeWorld: the ledger

WORKING AREA. The single source of truth for what Quake and QuakeWorld have and Shooter's Edge
lacks (Niclas, 2026-11-21): every console command and setting of both stock clients and servers,
every builtin, protocol message, menu page, entity class, item, weapon number, impulse, message a
player sees, info key, deathmatch and teamplay rule and monster, one row each, marked against our
tree. Built by three clean-room readers from the stock id release (the Quake engine, the
QuakeWorld client and server and the QuakeWorld game program); no code, facts in our words.
Modern clients and servers are inventoried apart (`client-inventory.md`, `server-inventory.md`).

Statuses: `done` (ours does it, the file named), `missing` (ours lacks it or differs noticeably;
rank 1 noticed at once, 2 sometimes, 3 rare or cosmetic), `not-taken` (left out on purpose, the
reason named). The missing count only goes down: a PR that fills rows flips them to `done` here,
and a row found later is added here first. Progress is reported as the missing count.

| Section | Rows | Done | Missing (rank 1 / 2 / 3) | Not taken |
| --- | --- | --- | --- | --- |
| Client | 508 | 319 | 26 (0 / 0 / 26) | 163 |
| Server | 316 | 267 | 14 (0 / 0 / 14) | 35 |
| Game | 245 | 226 | 1 (0 / 0 / 1) | 18 |
| Tools | 1 | 1 | 0 (0 / 0 / 0) | 0 |
| **All** | **1070** | **813** | **41 (0 / 0 / 41)** | **216** |

(Built 2026-11-21 against main at #1237 with 217 missing (9 / 49 / 159); the first fill, Quake's
deathmatch rules on the arena, took it to 129; one tools row found while building it makes 130.
The second, the shared settings, masters and spectator lines, took it to 120; the third, the
rest of the deathmatch rules, the server's lines and the idle sway, to 88; the fourth, joining a
NetQuake server, Quake's tell, ping and version, the runes and gates, the menu words, user and
msg, the ambience and mouse settings and the rows other threads had already built, to 46; Quake
II's console fill (sizeup, sizedown, play, playvol, stopsound, built once for every game) to 41.)

## CLIENT: the Quake and QuakeWorld clients, item by item

Reader 1 of the ledger (questions.md). Every console command and setting both stock clients register,
enumerated from their registrations (the Quake engine as one program, so its built-in server's own
settings appear here too and point at the SERVER section for their meaning; protocol words the
client sends to the server are listed and left to SERVER). A held command and its release are one row
(`+forward` stands for `+forward` and `-forward`). `q1`: the Quake engine; `qw`: the QuakeWorld
client; `both`: both register it. Defaults are the stock values. Our side was marked by our tree's
registrations and the window's own command and setting lists at start (`se +cmdlist +cvarlist`),
and from client-gaps.md and q1-sp-gaps.md where those notes already settled a row.

### 1. Console commands

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C1 | `cd` (both) | the music: play, loop, stop, pause, resume, eject, info | done | engine/audio/music_console.cpp (the kept soundtrack in place of the CD) |  |
| C2 | `+moveup` / `-moveup` (both) | swim up while held | done | plugins/shooter/controls.cpp |  |
| C3 | `+movedown` / `-movedown` (both) | swim down while held | done | plugins/shooter/controls.cpp |  |
| C4 | `+left` / `-left` (both) | turn left while held, at cl_yawspeed | done | plugins/shooter/controls.cpp |  |
| C5 | `+right` / `-right` (both) | turn right while held, at cl_yawspeed | done | plugins/shooter/controls.cpp |  |
| C6 | `+forward` / `-forward` (both) | move forward while held | done | plugins/shooter/controls.cpp |  |
| C7 | `+back` / `-back` (both) | move back while held | done | plugins/shooter/controls.cpp |  |
| C8 | `+lookup` / `-lookup` (both) | look up while held, at cl_pitchspeed | done | plugins/shooter/controls.cpp |  |
| C9 | `+lookdown` / `-lookdown` (both) | look down while held, at cl_pitchspeed | done | plugins/shooter/controls.cpp |  |
| C10 | `+strafe` / `-strafe` (both) | the mouse and turn keys sidestep while held | done | plugins/shooter/controls.cpp |  |
| C11 | `+moveleft` / `-moveleft` (both) | step left while held | done | plugins/shooter/controls.cpp |  |
| C12 | `+moveright` / `-moveright` (both) | step right while held | done | plugins/shooter/controls.cpp |  |
| C13 | `+speed` / `-speed` (both) | run (or walk, with always run) while held | done | plugins/shooter/controls.cpp (cl_run is our always-run) |  |
| C14 | `+attack` / `-attack` (both) | fire while held (button 0) | done | plugins/shooter/controls.cpp |  |
| C15 | `+use` / `-use` (both) | the use button while held (button 2 in the move; read by some mods) | done | plugins/shooter/controls.cpp (+use held but never sent: the classic games send only attack and jump) | 3 |
| C16 | `+jump` / `-jump` (both) | jump (and swim up) while held | done | plugins/shooter/controls.cpp |  |
| C17 | `impulse` (both) | send an impulse number with the next move (weapons, cheats, mod commands) | done | games/shooters_edge/client/controls.cpp (also takes a list) |  |
| C18 | `+klook` / `-klook` (both) | the forward and back keys look up and down while held | done | plugins/shooter/controls.cpp |  |
| C19 | `+mlook` / `-mlook` (both) | the mouse looks up and down while held | done | plugins/shooter/controls.cpp |  |
| C20 | `entities` (q1) | list the client's entities with model, frame and place | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C21 | `disconnect` (both) | leave the server or stop the demo | done | games/shooters_edge/net/qw_remote.cpp |  |
| C22 | `record` (both) | record a demo of the game being played (a map may be named to start it) | done | games/shooters_edge/client/listen_record.cpp, net/qw_match_record.hpp |  |
| C23 | `stop` (both) | stop recording | done | games/shooters_edge/client/listen_record.cpp |  |
| C24 | `playdemo` (both) | play a demo at its own pace | done | games/shooters_edge/client/demo_playback.cpp |  |
| C25 | `timedemo` (both) | play a demo as fast as it draws and print the frame rate | done | games/shooters_edge/client/demo_playback.cpp, client/timedemo.cpp |  |
| C26 | `stuffcmds` (both) | run the command line's + words (quake.rc calls it) | not-taken | the window runs the command line's + words itself after its start configs (client/main.cpp) |  |
| C27 | `exec` (both) | run a config file | done | engine/console/console_builtins.cpp |  |
| C28 | `echo` (both) | print the words to the console | done | engine/console/console_builtins.cpp, client/team_chat.cpp |  |
| C29 | `alias` (both) | make a command from others, or list them | done | engine/console/console_builtins.cpp |  |
| C30 | `cmd` (both) | send the rest of the line to the server as a command | done | games/shooters_edge/net/game_commands.cpp |  |
| C31 | `wait` (both) | hold the rest of the command buffer to the next frame | done | engine/console/console_builtins.cpp |  |
| C32 | `path` (both) | print the search path (folders and packs) | missing | no console command lists the search path (secli pak ls shows a folder's mounts outside the game) | 3 |
| C33 | `toggleconsole` (both) | the console down or up | done | engine/console/view.cpp |  |
| C34 | `messagemode` (both) | type a chat line to everyone | done | engine/console/talk.cpp |  |
| C35 | `messagemode2` (both) | type a chat line to the team | done | engine/console/talk.cpp |  |
| C36 | `clear` (both) | empty the console's scrollback | done | engine/console/view.cpp |  |
| C37 | `gl_texturemode` (both) | the texture filter by its GL name (nearest, linear and the mip forms) | done | plugins/classic/app/look.cpp (a setting of ours, not a command) |  |
| C38 | `timerefresh` (both) | spin the view once round and time the frames | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C39 | `envmap` (both) | write six views of the place as an environment map | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C40 | `pointfile` (both) | draw the leak trail of a map that failed to seal | not-taken | a mapper's tool: our map compiler names leaks itself (secli map check, the map-build lab) |  |
| C41 | `screenshot` (both) | save a picture of the screen | done | engine/capture/screenshot.cpp |  |
| C42 | `sizeup` (both) | one step larger view (less status bar) | done | games/shooters_edge/hud/layout.cpp: viewsize up 10, held by its range | - |
| C43 | `sizedown` (both) | one step smaller view (more status bar) | done | games/shooters_edge/hud/layout.cpp: viewsize down 10 | - |
| C44 | `force_centerview` (both) | level the view at once | done | plugins/shooter/controls.cpp |  |
| C45 | `vid_nummodes` (both) | count the video modes | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C46 | `vid_describecurrentmode` (both) | describe the current video mode | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C47 | `vid_describemode` (both) | describe one video mode | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C48 | `vid_describemodes` (both) | list every video mode | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C49 | `status` (q1) | the server's name, map and every player's frags, time and address | done | games/shooters_edge/net/qw_admin.cpp (on a game you host) |  |
| C50 | `quit` (both) | leave the game | done | games/shooters_edge/client/main.cpp |  |
| C51 | `god` (q1) | cheat: take no damage | done | games/shooters_edge/client/listen.cpp (campaign), server/commands.cpp |  |
| C52 | `notarget` (q1) | cheat: monsters do not see you | done | games/shooters_edge/client/listen.cpp, server/commands.cpp |  |
| C53 | `fly` (q1) | cheat: fly | done | games/shooters_edge/client/listen.cpp, server/commands.cpp |  |
| C54 | `map` (q1) | start a game on a map (the window becomes the server) | done | games/shooters_edge/client/main.cpp |  |
| C55 | `restart` (q1) | start the map again | done | games/shooters_edge/client/listen.cpp (campaign), server/commands.cpp |  |
| C56 | `changelevel` (q1) | go to another map keeping the players | done | games/shooters_edge/client/listen.cpp, server/commands.cpp |  |
| C57 | `changelevel2` (q1) | the mission pack's change of level that keeps the level's state | not-taken | not in the stock build (a build switch the stock release leaves off) |  |
| C58 | `connect` (both) | join a server by address | done | games/shooters_edge/net/qw_remote.cpp (QuakeWorld and Quake II servers) |  |
| C59 | `reconnect` (both) | join the same server again (and the next signon after a map change) | done | games/shooters_edge/net/qw_remote.cpp |  |
| C60 | `name` (q1) | your name | done | engine/console/view.cpp (a setting; q1 has a command) |  |
| C61 | `noclip` (q1) | cheat: pass through walls | done | games/shooters_edge/client/listen.cpp, server/commands.cpp |  |
| C62 | `version` (both) | print the build's version and date | done | net/qw_remote.cpp (`version` prints "Version ShootersEdge 0.1"; no build date, so builds stay reproducible: docs/porting/client.md) | 3 |
| C63 | `please` (q1) | id staff only: grant the god bits by address | not-taken | not in the stock build (built only with the staff switch) |  |
| C64 | `say` (both) | chat to everyone | done | engine/console/talk.cpp |  |
| C65 | `say_team` (both) | chat to your team | done | engine/console/talk.cpp |  |
| C66 | `tell` (q1) | a private line to one player by name | done | net/q1_remote.cpp (forwarded to a NetQuake server), client/listen.cpp (to the window's own Quake server) | 3 |
| C67 | `color` (both) | your shirt and pants colours in one word | done | games/shooters_edge/client/settings.cpp (`color`: low four bits, past 13 held at 13; a change while joined goes out as setinfo, net/qw_remote.cpp) |  |
| C68 | `kill` (both) | suicide | done | net/qw_remote.cpp (`remote_rules_word`: kill, pause and serverinfo go to the QuakeWorld server joined) |  |
| C69 | `pause` (both) | pause or resume the game | done | net/qw_remote.cpp (as C68) |  |
| C70 | `spawn` (q1) | signon word the client sends; typed, it is refused | not-taken | a protocol word, not a player command (the SERVER section reads it) |  |
| C71 | `begin` (q1) | signon word the client sends | not-taken | a protocol word (SERVER section) |  |
| C72 | `prespawn` (q1) | signon word the client sends | not-taken | a protocol word (SERVER section) |  |
| C73 | `kick` (q1) | remove a player from the game you host | done | games/shooters_edge/net/qw_admin.cpp |  |
| C74 | `ping` (q1) | every player's ping on the server | done | net/q1_remote.cpp, client/listen.cpp (forwarded as tell is) | 3 |
| C75 | `load` (q1) | load a saved game (s0..s11) | done | games/shooters_edge/client/game_folder.cpp |  |
| C76 | `save` (q1) | save the game to a slot | done | games/shooters_edge/client/game_folder.cpp |  |
| C77 | `give` (q1) | cheat: weapons, ammo, health, armour or keys | done | games/shooters_edge/client/listen.cpp, server/commands.cpp |  |
| C78 | `startdemos` (q1) | the demos the title loop plays in turn | done | games/shooters_edge/client/main.cpp |  |
| C79 | `demos` (q1) | go back to the title demo loop | done | games/shooters_edge/client/game_adapter.cpp (ours lists the recordings; kept by choice, q1-sp-gaps.md) |  |
| C80 | `stopdemo` (q1) | stop the demo playing | done | games/shooters_edge/client/main.cpp |  |
| C81 | `viewmodel` (q1) | point a viewthing entity at a model (model viewer) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge (our models lab views models) |  |
| C82 | `viewframe` (q1) | the viewthing's frame | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C83 | `viewnext` (q1) | the viewthing's next frame | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C84 | `viewprev` (q1) | the viewthing's previous frame | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C85 | `mcache` (q1) | list the model cache | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C86 | `toggle_auxlook` (q1) | DOS aux device look toggle | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C87 | `joyadvancedupdate` (both) | re-read the advanced joystick axes | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C88 | `bind` (both) | set or show what a key runs | done | engine/console/console_binds.cpp |  |
| C89 | `unbind` (both) | clear a key | done | engine/console/console_binds.cpp |  |
| C90 | `unbindall` (both) | clear every key | done | engine/console/console_binds.cpp |  |
| C91 | `togglemenu` (both) | the menu open or closed (Escape) | done | games/shooters_edge/menu/game_menu.cpp |  |
| C92 | `menu_main` (both) | open the main menu | done | games/shooters_edge/menu/game_menu.cpp |  |
| C93 | `menu_singleplayer` (q1) | open SINGLE PLAYER | done | menu/game_menu.cpp (menu_singleplayer) | 3 |
| C94 | `menu_load` (q1) | open LOAD | done | menu/game_menu.cpp (menu_load) | 3 |
| C95 | `menu_save` (q1) | open SAVE | done | menu/game_menu.cpp (menu_save) | 3 |
| C96 | `menu_multiplayer` (q1) | open MULTIPLAYER | done | menu/game_menu.cpp (menu_multiplayer) | 3 |
| C97 | `menu_setup` (q1) | open SETUP | done | menu/game_menu.cpp (menu_setup) | 3 |
| C98 | `menu_options` (both) | open OPTIONS | done | games/shooters_edge/menu/game_menu.cpp |  |
| C99 | `menu_keys` (both) | open CUSTOMIZE CONTROLS | done | menu/game_menu.cpp (menu_keys: the game's own key page, else CUSTOMIZE CONTROLS) | 3 |
| C100 | `menu_video` (both) | open the video page | done | menu/game_menu.cpp (menu_video) | 3 |
| C101 | `help` (both) | open the help pages (ours: what a name means) | done | menu HELP page (menu/main_menu.cpp); the console word help explains a setting instead |  |
| C102 | `menu_quit` (both) | open the quit question | done | games/shooters_edge/menu/game_menu.cpp |  |
| C103 | `net_stats` (q1) | the network layer's counters | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C104 | `ban` (q1) | ban an address on the listen server (NetQuake transport) | not-taken | the NetQuake transport is not served by the window; our server's bans are the SERVER section's |  |
| C105 | `test` (q1) | query a NetQuake server's players (network test) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C106 | `test2` (q1) | query a NetQuake server's rules (network test) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C107 | `slist` (q1) | search the LAN for NetQuake games | not-taken | covered by JOIN A GAME's server list (menu/servers_page.cpp), which finds LAN and master servers; NetQuake servers are not joined (see C-connect-nq) |  |
| C108 | `listen` (q1) | allow or forbid network players on your game | done | the host page's SERVER row (menu/main_menu.cpp) opens the game to others |  |
| C109 | `maxplayers` (q1) | the player count of the next game | done | the host page's MAX PLAYERS (maxclients); no maxplayers word |  |
| C110 | `port` (q1) | the UDP port of the next game | missing | no port word in the window (the dedicated server takes a port) | 3 |
| C111 | `+showscores` / `-showscores` (both) | the scoreboard while held | done | games/shooters_edge/client/controls.cpp |  |
| C112 | `play` (both) | play a sound file once | done | games/shooters_edge/client/menu_sound.cpp: each sound once, "Couldn't load sound/<name>" when missing | - |
| C113 | `playvol` (both) | play a sound file at a volume | done | games/shooters_edge/client/menu_sound.cpp | - |
| C114 | `stopsound` (both) | stop every sound | done | games/shooters_edge/client/menu_sound.cpp: every sound stopped, the music plays on | - |
| C115 | `soundlist` (both) | list the loaded sounds | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C116 | `soundinfo` (both) | the sound device's format | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C117 | `sbinfo` (q1) | the DOS Sound Blaster's settings | not-taken | platform: DOS sound card |  |
| C118 | `vid_testmode` (both) | try a video mode for a few seconds | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C119 | `gamma` (both) | the brightness curve (a command on some platforms) | done | plugins/classic/app/look.cpp (a setting) |  |
| C120 | `vid_debug` (both) | print the video driver's state | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C121 | `vid_forcemode` (both) | switch to a video mode at once | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C122 | `vid_windowed` (both) | go to the windowed mode | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays); vid_fullscreen 0 does it |  |
| C123 | `vid_fullscreen` (both) | go to the full-screen mode | done | engine/platform/window_cvars.cpp (a setting of ours, not a command) |  |
| C124 | `vid_minimize` (both) | minimize the window | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C125 | `v_cshift` (both) | a tint of the view by the console (four numbers) | done | client/main.cpp (`v_cshift`), app/effects.cpp (the open-air tint, Screen::air) |  |
| C126 | `bf` (both) | the bonus flash, as the server stuffs it on a pickup | done | plugins/classic/app/effects.cpp (cl_bonusflash; read from stuffed text, client-gaps.md) |  |
| C127 | `centerview` (both) | level the view smoothly (v_centerspeed) | done | plugins/shooter/controls.cpp |  |
| C128 | `flush` (both) | empty the model cache | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C129 | `changing` (qw) | the server's word before a map change: stop drawing, keep the connection | done | games/shooters_edge/net/qw_client.cpp |  |
| C130 | `rerecord` (qw) | reconnect and record from the start | missing | none | 3 |
| C131 | `skins` (qw) | fetch every player's skin again | done | games/shooters_edge/net/qw_client.cpp (as the server stuffs it) |  |
| C132 | `allskins` (qw) | one skin on everyone, or each their own | done | plugins/classic/app/look.cpp |  |
| C133 | `rcon` (qw) | a command to the server's console by password | done | games/shooters_edge/net/qw_admin.cpp |  |
| C134 | `packet` (qw) | send a connectionless packet to an address | done | games/shooters_edge/net/qw_client.cpp (as stuffed); not a typed word |  |
| C135 | `user` (qw) | one player's user info | done | net/qw_remote.cpp (`user <name|id>`, from the user info the server last told; net/qw_client.cpp keeps each slot's user id) | 3 |
| C136 | `users` (qw) | every player's user id, frags and name | done | games/shooters_edge/net/qw_admin.cpp |  |
| C137 | `setinfo` (qw) | set or list your user info | done | engine/console/console_builtins.cpp |  |
| C138 | `fullinfo` (qw) | the whole user info in one string (the server stuffs it at connect) | missing | not a command here; the user info is built from settings at connect (net/qw_remote.cpp) | 3 |
| C139 | `fullserverinfo` (qw) | the whole server info from the server | done | games/shooters_edge/net/qw_client.cpp |  |
| C140 | `download` (qw) | download a file from the server | done | games/shooters_edge/net/qw_client.cpp (the missing files fetched at signon); not a typed word |  |
| C141 | `nextul` (qw) | send the next piece of an upload the server asked for | not-taken | we refuse uploads (protocol.md, clc upload) |  |
| C142 | `stopul` (qw) | stop an upload | not-taken | we refuse uploads |  |
| C143 | `serverinfo` (qw) | show the server info | done | engine/console/console_builtins.cpp |  |
| C144 | `windows` (qw) | switch to the desktop | not-taken | platform: Windows only |  |
| C145 | `togglechat` (qw) | show or hide chat lines in the notify area | done | client/cvar_layers.cpp (Quake's synonym of toggleconsole, which it is in that game: it does not hide chat) | 3 |
| C146 | `snap` (qw) | a screenshot sent to the server (an admin asks) | not-taken | we answer no uploads (scr_allowsnap 0 in effect) |  |
| C147 | `+showteamscores` / `-showteamscores` (qw) | the team scoreboard while held | done | games/shooters_edge/client/controls.cpp |  |
| C148 | `connect` to a NetQuake server (q1) | join a server of the single-player protocol (version 15) by address | done | net/q1_remote.cpp (a server on port 26000 is joined on protocol 15: signon, moves, string commands, the goodbye; tests/shooters_edge_q1_remote_test.cpp) | 2 |

### 2. Settings (cvars)

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C149 | `chase_back` (q1, 100) | the chase camera's distance behind | done | engine/camera/cvars.cpp cl_chasedist; cam_dist (client/demo_playback.cpp) for the third person |  |
| C150 | `chase_up` (q1, 16) | the chase camera's height | done | engine/camera/cvars.cpp cl_chaseheight |  |
| C151 | `chase_right` (q1, 0) | the chase camera's side offset | missing | no side offset | 3 |
| C152 | `chase_active` (q1, 0) | the view from a chase camera behind yourself | done | cam_thirdperson (client/demo_playback.cpp) in the same meaning |  |
| C153 | `cl_upspeed` (both, 200) | swim up and down speed asked | done | plugins/shooter/controls.cpp |  |
| C154 | `cl_forwardspeed` (both, 200) | forward speed asked | done | plugins/shooter/controls.cpp |  |
| C155 | `cl_backspeed` (both, 200) | back speed asked | done | plugins/shooter/controls.cpp |  |
| C156 | `cl_sidespeed` (both, 350) | sidestep speed asked | done | plugins/shooter/controls.cpp |  |
| C157 | `cl_movespeedkey` (both, 2.0) | running multiplies the speeds by this | done | plugins/shooter/controls.cpp |  |
| C158 | `cl_yawspeed` (both, 140) | turn rate of the turn keys | done | plugins/shooter/controls.cpp |  |
| C159 | `cl_pitchspeed` (both, 150) | look rate of the look keys | done | plugins/shooter/controls.cpp |  |
| C160 | `cl_anglespeedkey` (both, 1.5) | running multiplies the key turn rates by this | done | plugins/shooter/controls.cpp |  |
| C161 | `_cl_name` (q1, player) | the saved name an old config carries | missing | not read: an old config.cfg's name is lost (ours keeps name) | 3 |
| C162 | `_cl_color` (q1, 0) | the saved colours an old config carries | missing | not read: an old config.cfg's colours are lost (ours keeps topcolor and bottomcolor) | 3 |
| C163 | `cl_shownet` (both, 0) | print each message's size or name as it is read | not-taken | a developer or profiling tool: ours are framestats, the labs and ge (showpackets is ours) |  |
| C164 | `cl_nolerp` (q1, 0) | draw entities where the packet says, no smoothing | done | games/shooters_edge/net/qw_remote.cpp |  |
| C165 | `lookspring` (both, 0) | the view recentres when mouse look is released | done | plugins/shooter/controls.cpp |  |
| C166 | `lookstrafe` (both, 0) | mouse look also sidesteps | done | plugins/shooter/controls.cpp |  |
| C167 | `sensitivity` (both, 3) | mouse speed | done | plugins/shooter/controls.cpp |  |
| C168 | `m_pitch` (both, 0.022) | mouse up-down scale (negative inverts) | done | plugins/shooter/controls.cpp |  |
| C169 | `m_yaw` (both, 0.022) | mouse turn scale | done | plugins/shooter/controls.cpp |  |
| C170 | `m_forward` (both, 1) | mouse forward-back scale with mouse look off | done | plugins/shooter/controls.cpp |  |
| C171 | `m_side` (both, 0.8) | mouse sidestep scale with +strafe | done | plugins/shooter/controls.cpp (the mouse's sideways counts times sensitivity times m_side, 0.8) | 3 |
| C172 | `registered` (both, 0) | whether the full game's pack is loaded (the program reads it) | done | games/shooters_edge/client/installs.cpp, server/qc_server.cpp |  |
| C173 | `cmdline` (q1, 0) | the command line, readable by the program | not-taken | nothing in play reads it |  |
| C174 | `con_notifytime` (both, 3) | how long the notify lines stay | done | engine/console/view.cpp |  |
| C175 | `d_subdiv16` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C176 | `d_mipcap` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C177 | `d_mipscale` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C178 | `gl_nobind` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C179 | `gl_max_size` (both, 1024) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C180 | `gl_picmip` (both, 0) | drop texture detail by mip levels | done | plugins/classic/app/look.cpp |  |
| C181 | `gl_subdivide_size` (both, 128) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C182 | `r_norefresh` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C183 | `r_drawentities` (both, 1) | draw the models (0 hides them) | done | plugins/classic/app/look.cpp |  |
| C184 | `r_drawviewmodel` (both, 1) | draw the held weapon | done | games/shooters_edge/view/hand.cpp |  |
| C185 | `r_speeds` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C186 | `r_fullbright` (both, 0) | draw the world with no lightmaps (cheat) | done | plugins/classic/app/look.cpp |  |
| C187 | `r_lightmap` (both, 0) | draw the lightmaps alone | done | plugins/classic/app/look.cpp |  |
| C188 | `r_shadows` (both, 0) | blob shadows under models | missing | no model blob shadows (ours: lights' shadows, ce_shadows) | 3 |
| C189 | `r_mirroralpha` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C190 | `r_wateralpha` (both, 1) | see-through water | done | plugins/classic/app/look.cpp |  |
| C191 | `r_dynamic` (both, 1) | dynamic lights on the world | done | plugins/classic/app/look.cpp |  |
| C192 | `r_novis` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C193 | `gl_finish` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C194 | `gl_clear` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C195 | `gl_cull` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C196 | `gl_texsort` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C197 | `gl_smoothmodels` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C198 | `gl_affinemodels` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C199 | `gl_polyblend` (both, 1) | the screen tints drawn at all | done | plugins/classic/app/look.cpp |  |
| C200 | `gl_flashblend` (both, 1) | dynamic lights drawn as glowing spheres instead of lighting | missing | no glow spheres (client-gaps.md: waits for them) | 3 |
| C201 | `gl_playermip` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C202 | `gl_nocolors` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C203 | `gl_keeptjunctions` (both, 0/1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C204 | `gl_reporttjunctions` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C205 | `gl_doubleeys` (q1, 1) | the eyes model drawn larger | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C206 | `viewsize` (both, 100) | the view size and how much status bar shows | done | games/shooters_edge/hud/layout.cpp |  |
| C207 | `fov` (both, 90) | the field of view | done | synonym of cl_fov (client/cvar_layers.cpp) |  |
| C208 | `scr_conspeed` (both, 300) | how fast the console slides | done | games/shooters_edge/client/game_start.cpp |  |
| C209 | `scr_centertime` (both, 2) | how long a centre print stays | done | plugins/classic/app/look.cpp, client/cvar_layers.cpp |  |
| C210 | `showram` (both, 1) | the low-memory icon | not-taken | no fixed memory cache to run out of |  |
| C211 | `showturtle` (both, 0) | the slow-frame turtle icon | missing | none | 3 |
| C212 | `showpause` (both, 1) | the pause picture | done | games/shooters_edge/client/window_cvars.cpp |  |
| C213 | `scr_printspeed` (both, 8) | how fast the finale text types out | done | client/window_cvars.cpp, client/scores.cpp (characters a second, 8 by default) | 3 |
| C214 | `gl_triplebuffer` (both, 1) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C215 | `vid_mode` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C216 | `vid_redrawfull` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C217 | `vid_waitforrefresh` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C218 | `m_filter` (both, 0) | average the mouse over two frames | done | plugins/shooter/mouse_feel.cpp |  |
| C219 | `gl_ztrick` (both, 1) | a depth buffer speed trick | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C220 | `in_mouse` (q1, 1) | use the mouse at all (Linux) | not-taken | platform: SDL takes the mouse (in_raw is ours) |  |
| C221 | `in_dgamouse` (q1, 1) | the X server's direct mouse (Linux) | not-taken | platform |  |
| C222 | `_vid_default_mode` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C223 | `_vid_default_mode_win` (both, 3) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C224 | `vid_wait` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C225 | `vid_nopageflip` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C226 | `_vid_wait_override` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C227 | `vid_config_x` (both, 800) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C228 | `vid_config_y` (both, 600) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C229 | `vid_stretch_by_2` (both, 1) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C230 | `_windowed_mouse` (both, 0) | take the mouse in a window | not-taken | platform: the window always takes the mouse in play |  |
| C231 | `host_framerate` (q1, 0) | fixed time per frame (slow or fast motion) | done | games/shooters_edge/client/window_cvars.cpp |  |
| C232 | `host_speeds` (both, 0) | print the frame's time per part | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C233 | `sys_ticrate` (q1, 0.05) | the dedicated server's frame time | not-taken | the window's server steps by its own fixed clock (SERVER section) |  |
| C234 | `serverprofile` (q1, 0) | print the server's time per frame | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C235 | `fraglimit` (q1, 0) | frags that end the match | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp, menu host page |  |
| C236 | `timelimit` (q1, 0) | minutes that end the match | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp, menu host page |  |
| C237 | `teamplay` (q1, 0) | team rules | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp, menu host page |  |
| C238 | `samelevel` (q1, 0) | stay on the same map | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp |  |
| C239 | `noexit` (q1, 0) | the exit kills in deathmatch | missing | no noexit setting on the window's server | 3 |
| C240 | `developer` (both, 0) | print developer messages (the program's dprint lines) | missing | not a setting in the window (only the module VM's server has it) | 3 |
| C241 | `skill` (q1, 1) | the next map's skill | done | the window's own server (the SERVER section checks the meaning): net/qw_coop_game.cpp, menu SKILL |  |
| C242 | `deathmatch` (q1, 0) | deathmatch rules | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp, menu GAME TYPE |  |
| C243 | `coop` (q1, 0) | cooperative rules | done | the window's own server (the SERVER section checks the meaning): client/coop_mode.cpp |  |
| C244 | `pausable` (q1, 1) | whether pause works in a network game | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp |  |
| C245 | `temp1` (q1, 0) | a spare setting some mods read | missing | none | 3 |
| C246 | `joystick` (both, 0) | use the joystick | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C247 | `joybuttons` (q1, 4) | DOS joystick buttons | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C248 | `auxlook` (q1, 1) | DOS aux look | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C249 | `joyname` (both, joystick) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C250 | `joyadvanced` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C251 | `joyadvaxisx` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C252 | `joyadvaxisy` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C253 | `joyadvaxisz` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C254 | `joyadvaxisr` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C255 | `joyadvaxisu` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C256 | `joyadvaxisv` (both, 0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C257 | `joyforwardthreshold` (both, 0.15) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C258 | `joysidethreshold` (both, 0.15) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C259 | `joypitchthreshold` (both, 0.15) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C260 | `joyyawthreshold` (both, 0.15) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C261 | `joyforwardsensitivity` (both, -1.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C262 | `joysidesensitivity` (both, -1.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C263 | `joypitchsensitivity` (both, 1.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C264 | `joyyawsensitivity` (both, -1.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C265 | `joywwhack1` (both, 0.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C266 | `joywwhack2` (both, 0.0) | a joystick axis or threshold | not-taken | the old joystick axis table: devices, axes and bindings are engine/input (gamepads, sticks, wheels) and stronger |  |
| C267 | `net_messagetimeout` (q1, 300) | drop a silent NetQuake connection after this | not-taken | the NetQuake transport is not used by the window (SERVER section for ours) |  |
| C268 | `hostname` (q1, UNNAMED) | the server's name | done | the window's own server (the SERVER section checks the meaning): net/qw_game.cpp |  |
| C269 | `_config_com_port` (q1, 0x3f8) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C270 | `_config_com_irq` (q1, 4) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C271 | `_config_com_baud` (q1, 57600) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C272 | `_config_com_modem` (q1, 1) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C273 | `_config_modem_dialtype` (q1, T) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C274 | `_config_modem_clear` (q1, ATZ) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C275 | `_config_modem_init` (q1, empty) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C276 | `_config_modem_hangup` (q1, AT H) | the serial or modem link's settings | not-taken | platform: DOS serial and modem play |  |
| C277 | `idgods` (q1, 0) | id staff god mode | not-taken | not in the stock build |  |
| C278 | `r_draworder` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C279 | `r_timegraph` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C280 | `r_graphheight` (both, 10/15) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C281 | `r_clearcolor` (both, 2) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C282 | `r_waterwarp` (both, 1) | the view sways under water | done | plugins/classic/app/look.cpp |  |
| C283 | `r_polymodelstats` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C284 | `r_dspeeds` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C285 | `r_drawflat` (both, 0) | surfaces drawn in flat colours | done | plugins/classic/app/look.cpp |  |
| C286 | `r_ambient` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C287 | `r_reportsurfout` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C288 | `r_maxsurfs` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C289 | `r_numsurfs` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C290 | `r_reportedgeout` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C291 | `r_maxedges` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C292 | `r_numedges` (both, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C293 | `r_aliastransbase` (both, 200) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C294 | `r_aliastransadj` (both, 100) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C295 | `bgmvolume` (both, 1) | music volume | done | engine/audio/music_console.cpp |  |
| C296 | `volume` (both, 0.7) | sound volume | done | engine/platform/window_cvars.cpp |  |
| C297 | `nosound` (both, 0) | no sound at all | not-taken | covered by volume 0 |  |
| C298 | `precache` (both, 1) | load every sound at map load | not-taken | sounds are loaded as the game needs them |  |
| C299 | `loadas8bit` (both, 0) | load sounds at 8 bits | not-taken | platform memory saving |  |
| C300 | `bgmbuffer` (both, 4096) | the music buffer size | not-taken | platform |  |
| C301 | `ambient_level` (both, 0.3) | how loud the water and sky ambience is | done | client/cvar_layers.cpp, sound/ambience.hpp (the leaf's level times ambient_level; 0 silences at once) | 3 |
| C302 | `ambient_fade` (both, 100) | how fast the ambience swells and fades | done | client/cvar_layers.cpp, sound/ambience.cpp (the level moves fade/255 of full a second) | 3 |
| C303 | `snd_noextraupdate` (both, 0) | mix sound only once a frame | not-taken | platform: our mixer runs on its own thread |  |
| C304 | `snd_show` (both, 0) | print the sounds playing | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C305 | `_snd_mixahead` (both, 0.1) | how far ahead sound is mixed | not-taken | platform |  |
| C306 | `sys_linerefresh` (both, 0) | Linux refresh test | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C307 | `vid_fullscreen_mode` (both, 3) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C308 | `vid_windowed_mode` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C309 | `block_switch` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C310 | `vid_window_x` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C311 | `vid_window_y` (both, 0) | a video mode or window setting | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C312 | `pixel_multiply` (q1, 2) | Sun XIL pixel scale | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays) |  |
| C313 | `lcd_x` (both, 0) | stereo shutter glasses eye split | not-taken | platform: old stereo hardware |  |
| C314 | `lcd_yaw` (q1, 0) | stereo shutter glasses eye angle | not-taken | platform: old stereo hardware |  |
| C315 | `scr_ofsx` (q1, 0) | move the eye by a fixed amount across (debug) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C316 | `scr_ofsy` (q1, 0) | move the eye by a fixed amount sideways (debug) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C317 | `scr_ofsz` (q1, 0) | move the eye by a fixed amount up (debug) | not-taken | a developer or profiling tool: ours are framestats, the labs and ge |  |
| C318 | `cl_rollspeed` (both, 200) | side speed that gives the full roll | done | engine/camera/cvars.cpp |  |
| C319 | `cl_rollangle` (both, 2.0) | the view's roll when sidestepping | done | engine/camera/cvars.cpp |  |
| C320 | `cl_bob` (both, 0.02) | how much the view bobs while walking | done | engine/camera/cvars.cpp |  |
| C321 | `cl_bobcycle` (both, 0.6) | the bob's period | done | engine/camera/cvars.cpp |  |
| C322 | `cl_bobup` (both, 0.5) | the share of the bob spent rising | done | engine/camera/cvars.cpp |  |
| C323 | `v_kicktime` (both, 0.5) | how long a hit kicks the view | done | engine/camera/cvars.cpp |  |
| C324 | `v_kickroll` (both, 0.6) | how far a hit rolls the view | done | engine/camera/cvars.cpp |  |
| C325 | `v_kickpitch` (both, 0.6) | how far a hit pitches the view | done | engine/camera/cvars.cpp |  |
| C326 | `v_iyaw_cycle` (both, 2) | the idle sway's yaw period | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C327 | `v_iroll_cycle` (both, 0.5) | the idle sway's roll period | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C328 | `v_ipitch_cycle` (both, 1) | the idle sway's pitch period | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C329 | `v_iyaw_level` (both, 0.3) | the idle sway's yaw size | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C330 | `v_iroll_level` (both, 0.1) | the idle sway's roll size | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C331 | `v_ipitch_level` (both, 0.3) | the idle sway's pitch size | done | games/shooters_edge/client/present.cpp (bound in the classic layer onto engine/camera/first_person.hpp's idle sway axes) | 3 |
| C332 | `v_idlescale` (both, 0) | how much the view sways when idle | done | engine/camera/cvars.cpp |  |
| C333 | `crosshair` (both, 0) | draw the crosshair | done | engine/hud/crosshair.cpp |  |
| C334 | `cl_crossx` (both, 0) | crosshair offset across | done | engine/hud/crosshair.cpp |  |
| C335 | `cl_crossy` (both, 0) | crosshair offset down | done | engine/hud/crosshair.cpp |  |
| C336 | `gl_cshiftpercent` (both, 100) | how strong every tint is | done | plugins/classic/app/look.cpp |  |
| C337 | `v_centermove` (both, 0.15) | how far you walk before the view recentres (lookspring) | done | plugins/shooter/controls.cpp |  |
| C338 | `v_centerspeed` (both, 500) | how fast the view recentres | done | plugins/shooter/controls.cpp |  |
| C339 | `gamma` (both, 1) | brightness | done | plugins/classic/app/look.cpp |  |
| C340 | `cl_hightrack` (qw, 0) | a spectator tracks the top fragger | done | games/shooters_edge/client/demo_playback.cpp |  |
| C341 | `cl_chasecam` (qw, 0) | a spectator tracking a player sees through his eyes | done | the tracked player's eyes are the view (client/demo_eyes.cpp) |  |
| C342 | `cl_camera_maxpitch` (qw, 10) | the old tracking camera's pitch limit | not-taken | that camera is replaced by cam_thirdperson and the eyes |  |
| C343 | `cl_camera_maxyaw` (qw, 30) | the old tracking camera's yaw limit | not-taken | as cl_camera_maxpitch |  |
| C344 | `cl_nodelta` (qw, 0) | ask for full entity lists, never deltas | not-taken | a developer or profiling tool: ours are framestats, the labs and ge (a network debugging switch) |  |
| C345 | `rcon_password` (qw, empty) | the password rcon sends | done | games/shooters_edge/net/qw_remote.cpp |  |
| C346 | `rcon_address` (qw, empty) | where rcon goes when not joined | done | games/shooters_edge/net/qw_remote.cpp |  |
| C347 | `cl_timeout` (qw, 60) | drop the server after this silence | done | games/shooters_edge/net/qw_remote.cpp |  |
| C348 | `cl_sbar` (qw, 0) | the old opaque status bar instead of the overlay | done | hud_sbar in the same meaning (games/shooters_edge/hud) |  |
| C349 | `cl_hudswap` (qw, 0) | the status bar's side panels on the left | missing | none | 3 |
| C350 | `cl_maxfps` (qw, 0) | the frame cap (and the moves a second) | done | engine/platform/pacing.cpp; the moves are cl_physfps (client-gaps.md) |  |
| C351 | `entlatency` (qw, 20) | assumed entity lag | not-taken | our interpolation has its own rule (net/qw_remote.cpp) |  |
| C352 | `cl_predict_players` (qw, 1) | predict the other players' moves | done | games/shooters_edge/net/qw_remote.cpp |  |
| C353 | `cl_predict_players2` (qw, 1) | a second variant of the other players' prediction | not-taken | one rule, cl_predict_players |  |
| C354 | `cl_solid_players` (qw, 1) | other players block your predicted move | done | games/shooters_edge/net/qw_remote.cpp |  |
| C355 | `localid` (qw, empty) | an id a local server matches to trust the client | missing | none | 3 |
| C356 | `password` (qw, empty) | the server password sent in the user info | done | client/settings.cpp (`password`, user info, the classic layer), net/qw_remote.cpp (sent at the join and again when changed) |  |
| C357 | `spectator` (qw, empty) | join as a spectator (or the spectator password) | done | games/shooters_edge/net/qw_remote.cpp (1 only; a spectator password is not sent) |  |
| C358 | `name` (qw, unnamed) | your name (user info) | done | engine/console/view.cpp, net/qw_client.cpp |  |
| C359 | `team` (qw, empty) | your team (user info) | done | the team command (engine/console/talk.cpp) and net/qw_client.cpp |  |
| C360 | `skin` (qw, empty) | your skin (user info) | done | games/shooters_edge/client/settings.cpp |  |
| C361 | `topcolor` (qw, 0) | shirt colour (user info) | done | games/shooters_edge/client/settings.cpp |  |
| C362 | `bottomcolor` (qw, 0) | pants colour (user info) | done | games/shooters_edge/client/settings.cpp |  |
| C363 | `rate` (qw, 2500) | bytes a second the server may send (user info) | done | client/settings.cpp (`rate`, the classic layer, 2500; Quake II says 15000), net/qw_remote.cpp (setinfo rate when changed) |  |
| C364 | `noaim` (qw, 0) | ask the server for no aim help (user info) | done | client/settings.cpp, net/qw_client.cpp (`noaim` in the user info, told again when it changes) | 3 |
| C365 | `msg` (qw, 1) | the lowest print level you want (user info) | done | client/settings.cpp, net/qw_client.cpp (`msg` 0..3, default 1, in the user info) | 3 |
| C366 | `show_fps` (qw, 0) | the frame rate in a corner | done | scr_showfps in the same meaning |  |
| C367 | `cl_nopred` (qw, 0) | no prediction of your own move | done | games/shooters_edge/net/qw_remote.cpp |  |
| C368 | `pushlatency` (qw, -999) | how far ahead the prediction runs | not-taken | our prediction runs to the newest move by its own rule |  |
| C369 | `cl_warncmd` (qw, 0) | warn about unknown commands in configs | not-taken | our console always names an unknown word |  |
| C370 | `r_netgraph` (qw, 0) | the packet graph | done | engine/hud/net_graph.hpp |  |
| C371 | `scr_allowsnap` (qw, 1) | allow the server's screenshot request | not-taken | we never upload (as 0) |  |
| C372 | `showpackets` (qw, 0) | print each packet sent and read | done | games/shooters_edge/net/qw_remote.cpp |  |
| C373 | `showdrop` (qw, 0) | print each dropped packet | done | games/shooters_edge/net/qw_remote.cpp |  |
| C374 | `qport` (qw, 0) | the port number that survives a NAT rebinding | done | chosen from the name at connect (net/qw_remote.cpp), not a setting |  |
| C375 | `r_zgraph` (qw, 0) | a renderer switch or diagnostic | not-taken | renderer internals: our renderer is its own (docs/rendering.md); nothing a player sets |  |
| C376 | `baseskin` (qw, base) | the skin used when a player's is missing | done | plugins/classic/app/look.cpp |  |
| C377 | `noskins` (qw, 0) | do not fetch or draw skins | done | plugins/classic/app/look.cpp |  |
| C378 | `crosshaircolor` (qw, 79) | the crosshair's colour | done | engine/hud/crosshair.cpp |  |
| C379 | `v_contentblend` (qw, 1) | strength of the water, slime and lava tint | done | plugins/classic/app/look.cpp |  |

### 3. Menu pages and their rows

Quake's menu (q1); the QuakeWorld client keeps main, options, keys, video, help and quit, and its
SINGLE PLAYER and MULTIPLAYER pages are only notices that it plays on the Internet. Ours: games/shooters_edge/menu/.

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C380 | Main (q1, qw) | SINGLE PLAYER, MULTIPLAYER, OPTIONS, HELP, QUIT | done | menu/main_menu.cpp, classic_view.cpp (the pack's own pictures); ours adds SELECT GAME and DEMOS |  |
| C381 | Single player: NEW GAME (q1) | start the campaign on start (asks first when a game runs) | done | menu/main_menu.cpp (newgame question, q1-sp-gaps.md B18) |  |
| C382 | Single player: LOAD, SAVE (q1) | twelve slots with the level name and kills | done | menu/main_menu.cpp, client/game_folder.cpp (B17 landed) |  |
| C383 | QW single player and multiplayer notices (qw) | pages that say QuakeWorld is for Internet play and where to find servers | not-taken | our pages do the real thing (servers page, host page) |  |
| C384 | Multiplayer: JOIN A GAME (q1) | the transport page, then the LAN or serial page, then the found games | done | menu/servers_page.cpp (LAN, masters and named servers) |  |
| C385 | Multiplayer: NEW GAME (q1) | host a game | done | menu/main_menu.cpp host page |  |
| C386 | Multiplayer: SETUP (q1) | Hostname, Your name, Shirt color, Pants color with the player picture, Accept Changes | done | menu/main_menu.cpp setup page (name, colours; applied at once) |  |
| C387 | Setup: Hostname row (q1) | the server name typed in setup | missing | no hostname row on the setup page | 3 |
| C388 | Setup: the coloured player picture (q1) | the figure in the chosen shirt and pants beside the rows | missing | not drawn: the setup page has no figure in the chosen colours | 3 |
| C389 | Net page: serial, modem, IPX, TCP/IP (q1) | choose a transport | not-taken | platform: one transport (UDP) |  |
| C390 | Serial and modem config pages (q1) | port, IRQ, baud, dial | not-taken | platform: DOS serial and modem |  |
| C391 | LAN config page (q1) | the port, search for local games, join a game at an address | done | menu/servers_page.cpp, connect |  |
| C392 | Game options: begin game, max players (q1) | start the hosted game, its size | done | host page START, MAX PLAYERS |  |
| C393 | Game options: game type, teamplay, skill (q1) | cooperative or deathmatch, team rule, difficulty names | done | host page GAME TYPE, TEAMPLAY, SKILL |  |
| C394 | Game options: frag limit, time limit (q1) | in steps, none at 0 | done | host page FRAG LIMIT, TIME LIMIT |  |
| C395 | Game options: episode and level (q1) | the map chosen by episode then level name, mission packs' lists too | missing | ours: one MAP row by name; no episode step | 3 |
| C396 | Search and server list (q1) | searching for LAN games, then the list to pick | done | menu/servers_page.cpp |  |
| C397 | Options (q1, qw) | Customize controls, Go to console, Reset to defaults, Screen size, Brightness, Mouse Speed, CD Music Volume, Sound Volume, Always Run, Invert Mouse, Lookspring, Lookstrafe, Video Options, Use Mouse | done | menu/main_menu.cpp options page (every row but Use Mouse) |  |
| C398 | Options: Use Mouse (q1, qw) | take the mouse in a window | not-taken | platform: the window always takes the mouse in play |  |
| C399 | Options: Use old status bar, HUD on left side (qw) | cl_sbar and cl_hudswap | missing | ours: hud_sbar on the video page; no left-side row (cl_hudswap missing) | 3 |
| C400 | Keys (q1, qw) | attack, change weapon, jump / swim up, walk forward, backpedal, turn left, turn right, run, step left, step right, sidestep, swim up, swim down; two keys each, Enter to set, Backspace to clear | done | menu/controls_page.cpp |  |
| C401 | Keys: look up, look down, center view, mouse look, keyboard look (q1, qw) | five more rows of the classic page | missing | not on our CUSTOMIZE CONTROLS page | 3 |
| C402 | Video (q1, qw) | the mode list and test | not-taken | platform video modes: the window, its size and its display are engine/platform (vid_fullscreen, vid_displays); our video page holds the looks |  |
| C403 | Help (q1, qw) | the help pictures page by page, and the ordering screens | done | menu/classic_view.cpp (help0 to help5 from the pack) |  |
| C404 | Quit (q1, qw) | a box with one of the joke lines and y or n; the shareware ordering screen | missing | ours asks LEAVE SHOOTER'S EDGE? (the classic box, not the game's own lines) | 3 |

### 4. Server-to-client messages

The single-player protocol (q1, version 15) and the QuakeWorld protocol (qw). Ours reads qw in
net/qw_svc.cpp and net/qw_client.cpp; q1 recordings (.dem) are read by client/demo_convert_single.cpp,
and the campaign on our own host hands the client the same events without the wire (client/listen.cpp).

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C405 | bad (0) (both) | never sent; an error to read | done | net/qw_svc.cpp |  |
| C406 | nop (1) (both) | keepalive | done | net/qw_svc.cpp, demo_convert_single.cpp |  |
| C407 | disconnect (2) (both) | the server dropped you | done | net/qw_svc.cpp |  |
| C408 | updatestat (3) (both) | one stat (q1: a long; qw: a byte) | done | net/qw_svc.cpp, demo_convert_single.cpp |  |
| C409 | version (4) (q1) | the protocol number | done | demo_convert_single.cpp |  |
| C410 | setview (5) (both) | the entity the view rides | done | demo_convert_single.cpp (qw never sends it) |  |
| C411 | sound (6) (both) | a sound on an entity's channel at a point, with volume and attenuation | done | net/qw_svc.cpp, demo_convert_single.cpp |  |
| C412 | time (7) (q1) | the server time of this frame | done | demo_convert_single.cpp |  |
| C413 | print (8) (both) | console text (qw: with a level, 3 is chat) | done | net/qw_svc.cpp |  |
| C414 | stufftext (9) (both) | a console line to run | done | net/qw_client.cpp |  |
| C415 | setangle (10) (both) | the server sets your view | done | net/qw_svc.cpp |  |
| C416 | serverinfo (11, q1) / serverdata (11, qw) (both) | q1: protocol, players, game type, level name, models, sounds; qw: protocol, server count, game folder, your slot, level name, movement numbers | done | net/qw_signon.cpp, demo_convert_single.cpp |  |
| C417 | lightstyle (12) (both) | a light style's pattern of letters | done | net/qw_svc.cpp |  |
| C418 | updatename (13) (q1) | a player's name | done | demo_convert_single.cpp (into the slot's user info) |  |
| C419 | updatefrags (14) (both) | a player's frags | done | net/qw_svc.cpp |  |
| C420 | clientdata (15) (q1) | your view height, ideal pitch, punch, velocity, items, ground and water bits, weapon frame and model, armour, health, ammo counts, active weapon | done | demo_convert_single.cpp |  |
| C421 | stopsound (16) (both) | stop an entity's channel | done | net/qw_svc.cpp |  |
| C422 | updatecolors (17) (q1) | a player's shirt and pants | done | demo_convert_single.cpp |  |
| C423 | particle (18) (q1) | a burst of particles of a colour with a direction | missing | the campaign draws the program's particles; a recorded .dem keeps only the blood colours and drops the rest | 3 |
| C424 | damage (19) (both) | armour and blood taken and where from (the hurt tint and kick) | done | net/qw_svc.cpp, demo_convert_single.cpp |  |
| C425 | spawnstatic (20) (both) | a fixed model with no number | done | net/qw_svc.cpp |  |
| C426 | spawnbaseline (22) (both) | an entity's first state | done | net/qw_svc.cpp |  |
| C427 | temp_entity (23) (both) | an effect (table below) | done | net/qw_svc.cpp |  |
| C428 | setpause (24) (both) | paused or not | done | net/qw_svc.cpp |  |
| C429 | signonnum (25) (q1) | the signon stage reached | done | demo_convert_single.cpp |  |
| C430 | centerprint (26) (both) | a message in the middle of the screen | done | net/qw_svc.cpp |  |
| C431 | killedmonster (27) (both) | one more monster killed | done | net/qw_svc.cpp |  |
| C432 | foundsecret (28) (both) | one more secret found | done | net/qw_svc.cpp |  |
| C433 | spawnstaticsound (29) (both) | an ambient loop at a point | done | net/qw_svc.cpp |  |
| C434 | intermission (30) (both) | the level is over (qw: the camera's place and angles) | done | net/qw_svc.cpp, demo_convert_single.cpp |  |
| C435 | finale (31) (both) | the episode's end text | done | net/qw_svc.cpp |  |
| C436 | cdtrack (32) (both) | the music track (q1: and the loop track) | done | net/qw_svc.cpp (the loop track dropped) |  |
| C437 | sellscreen (33) (both) | the shareware ordering screen | done | client/campaign_view.cpp |  |
| C438 | cutscene (34, q1) / smallkick (34, qw) (both) | q1: a text screen without the intermission tally; qw: a small view kick | done | demo_convert_single.cpp (cutscene as finale); net/qw_remote.cpp (kick) |  |
| C439 | bigkick (35) (qw) | a bigger view kick | done | net/qw_remote.cpp |  |
| C440 | updateping (36) (qw) | a player's ping | done | net/qw_svc.cpp |  |
| C441 | updateentertime (37) (qw) | when a player entered | done | net/qw_svc.cpp |  |
| C442 | updatestatlong (38) (qw) | a stat of 32 bits | done | net/qw_svc.cpp |  |
| C443 | muzzleflash (39) (qw) | a player fired: the flash light | done | net/qw_svc.cpp |  |
| C444 | updateuserinfo (40) (qw) | a player's user id and visible info | done | net/qw_svc.cpp |  |
| C445 | download (41) (qw) | a piece of a file with the percent done | done | net/qw_svc.cpp, qw_client.cpp |  |
| C446 | playerinfo (42) (qw) | a player's place, move, frame, model, effects, weapon frame | done | net/qw_client.cpp |  |
| C447 | nails (43) (qw) | the nails in flight, packed | done | net/qw_client.cpp |  |
| C448 | chokecount (44) (qw) | packets the server held back | done | net/qw_svc.cpp |  |
| C449 | modellist (45), soundlist (46) (qw) | the precache lists in parts | done | net/qw_signon.cpp |  |
| C450 | packetentities (47), deltapacketentities (48) (qw) | the visible entities, whole or as a delta from a frame | done | net/qw_client.cpp |  |
| C451 | maxspeed (49), entgravity (50) (qw) | your move's speed cap and gravity share | done | net/qw_client.cpp |  |
| C452 | setinfo (51) (qw) | one key of a player's info | done | net/qw_svc.cpp |  |
| C453 | serverinfo (52) (qw) | one key of the server info | done | net/qw_svc.cpp |  |
| C454 | updatepl (53) (qw) | a player's packet loss | done | net/qw_svc.cpp |  |
| C455 | the entity update (high bit set) (q1) | a changed entity: the bits name the fields (model, frame, colours, skin, effects, origin, angles, no-lerp) | done | demo_convert_single.cpp |  |

### Temporary entities (svc temp_entity)

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C456 | spike, superspike (0, 1) (both) | a nail hits a wall: sparks and a ricochet sound | done | net/qw_client.cpp, world_draw.cpp |  |
| C457 | gunshot (2) (both) | bullet puffs (qw: with a count) | done | net/qw_client.cpp |  |
| C458 | explosion (3) (both) | a rocket blast: particles, light, sound | done | client/world_draw.cpp |  |
| C459 | tarexplosion (4) (both) | the spawn's blast | done | client/world_draw.cpp |  |
| C460 | lightning1, lightning2, lightning3 (5, 6, 9) (both) | a bolt from an entity between two points (the shambler, the gun, Chthon) | done | client/world_draw.cpp (beams) |  |
| C461 | wizspike, knightspike (7, 8) (both) | the scrag's and the death knight's hits | done | client/world_draw.cpp |  |
| C462 | lavasplash (10) (both) | Chthon's lava splash | done | client/world_draw.cpp |  |
| C463 | teleport (11) (both) | the teleport sparkle | done | client/world_draw.cpp |  |
| C464 | explosion2 (12) (q1) | a blast in a range of palette colours (mission packs) | missing | drawn as a plain explosion; the colours are lost | 3 |
| C465 | beam (13) (q1) | a grappling beam between two points | missing | dropped from a recorded .dem; not drawn | 3 |
| C466 | implosion, railtrail (14, 15) (q1) | effects behind a build switch the stock release leaves off | not-taken | not in the stock build |  |
| C467 | blood (12) (qw) | blood with a count | done | net/qw_client.cpp |  |
| C468 | lightningblood (13) (qw) | blood where the bolt hits | done | net/qw_client.cpp |  |

### 5. The screen and the view

| id | item (the name a player, mapper or mod sees) | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| C469 | Status bar (both) | health with the face and its pain and power looks, armour, ammo, the weapons owned with the one in hand lit, the four ammo counts, keys, powerups, the sigils | done | games/shooters_edge/hud (hud_style classic, the pack's pictures) |  |
| C470 | Status bar in deathmatch (both) | the frag boxes of the top players beside the bar | done | games/shooters_edge/hud |  |
| C471 | Solo panel on TAB (q1) | monsters, secrets, time and level name in single player | done | client/scores.cpp (q1-sp-gaps.md B10, B16) |  |
| C472 | Deathmatch scoreboard (both) | every player's colours, frags and name (qw: ping, loss, time, team, spectators) | done | menu/scoreboard.cpp, score_modern.cpp |  |
| C473 | Team scoreboard (qw) | teams with their total frags and players | done | menu/scoreboard.cpp (+showteamscores) |  |
| C474 | Intermission screen (both) | COMPLETED, time, secrets and kills pictures; the scoreboard in deathmatch | done | menu/level_screens.cpp (B7 landed) |  |
| C475 | Intermission camera (both) | the view at the level's camera with the idle sway, no weapon | done | client/listen.cpp (B9 landed) |  |
| C476 | Finale screen (both) | the episode's text typed out over the view, the finale title | done | menu/level_screens.cpp (B8 landed) |  |
| C477 | Centre print (both) | centred lines, a third of the way down, held scr_centertime | done | plugins/classic/app/look.cpp (B11 landed) |  |
| C478 | Notify lines (both) | the newest console lines at the top for con_notifytime | done | engine/console/view.cpp |  |
| C479 | Chat input line (both) | say: or say_team: typed at the top | done | engine/console/talk.cpp |  |
| C480 | Console (both) | slides down, scrollback with Page Up and Down, history, Tab completion, the pack's console background and version | done | engine/console/view.cpp, menu/classic_console.cpp |  |
| C481 | Loading plaque (both) | LOADING over the screen while a map loads | done | client/campaign_view.cpp (B21 landed) |  |
| C482 | Pause picture (both) | PAUSED in the middle | done | client/scores.cpp (showpause) |  |
| C483 | Net and turtle icons (both) | a warning when the server is silent, and a slow-frame turtle | missing | none drawn | 3 |
| C484 | Disc icon (q1) | shown while reading files | not-taken | files load in the background |  |
| C485 | Crosshair (both) | a plus in the middle | done | engine/hud/crosshair.cpp |  |
| C486 | Frame rate (qw) | show_fps in the corner | done | scr_showfps |  |
| C487 | Net graph (qw) | per-packet bars of lag and loss | done | engine/hud/net_graph.hpp |  |
| C488 | Download progress (qw) | the file name and a bar while downloading | missing | files are fetched with no bar or name on screen (net/qw_client.cpp) | 3 |
| C489 | Damage tint (both) | a red to amber flash by how much armour took | done | plugins/classic/app/effects.cpp (B5 landed) |  |
| C490 | Bonus flash (both) | a gold flash on a pickup | done | plugins/classic/app/effects.cpp (B3 landed) |  |
| C491 | Contents tints (both) | water, slime and lava colour the view | done | plugins/classic/app/effects.cpp (B6 landed) |  |
| C492 | Powerup tints (both) | quad blue, pentagram red, ring grey, suit green | done | plugins/classic/app/effects.cpp (B2 landed) |  |
| C493 | View bob, roll, kick (both) | the stride's bob, the side roll, the hit kick | done | engine/camera/cvars.cpp |  |
| C494 | Idle sway (both) | the view drifts in three axes by v_idlescale (the intermission's too) | done | engine/camera/cvars.cpp (the shape fixed, see the v_i rows) |  |
| C495 | Weapon bob and drift (both) | the held weapon bobs and lags the turn | done | engine/viewmodel, client/present.cpp |  |
| C496 | Step smoothing (both) | the eye eases up a stair instead of jumping | done | engine/camera (first-person rig) |  |
| C497 | Pitch drift (q1) | walking with keys levels the view toward the slope | done | plugins/shooter/controls.cpp (B20 landed) |  |
| C498 | Death view (both) | the view rolls 80 and drops, the weapon gone | done | client/present.cpp (B10 landed) |  |
| C499 | Chase camera (q1) | chase_active behind yourself | done | cam_thirdperson |  |
| C500 | Spectator tracking (qw) | a spectator rides a player (jump cycles), tracks the top fragger | done | client/demo_eyes.cpp, demo_autotrack.hpp |  |
| C501 | Dynamic lights and trails (both) | rocket, grenade and blood trails; muzzle, quad and pentagram glows; flag glows (qw) | done | client/world_draw.cpp, plugins/classic/app/effects.cpp |  |
| C502 | Skins (qw) | player skins downloaded, colour-translated by top and bottom colour | done | plugins/classic/app/look.cpp, net/qw_client.cpp |  |
| C503 | Prediction (qw) | your own move predicted, the others' run ahead | done | net/qw_remote.cpp |  |
| C504 | Ambient sound (both) | water and sky loops by the leaf you stand in | done | games/shooters_edge/sound/ambience.cpp |  |
| C505 | Demo loop at start (q1) | demo1 to demo3 played full screen until a key brings the menu | done | client/main.cpp (B15 landed) |  |
| C506 | Demos: .dem recorded and played (q1) | a single-player recording with its signon | done | client/demo_convert_single.cpp, demo_convert_single_out.cpp |  |
| C507 | Demos: .qwd recorded and played (qw) | a client recording of a QuakeWorld game | done | net/qw_match_record.hpp, client/demo_playback.cpp |  |
| C508 | Screenshot files (both) | quakeNN.pcx (GL: .tga) in the game folder | done | engine/capture/screenshot.cpp (our own format and folder) |  |

Count: 508 rows, 319 done, 26 missing (rank 3 26), 163 not-taken.

## SERVER: the Quake and QuakeWorld servers, row by row

Reader 2's section of the ledger (`questions.md`). Every row is a name a player, an operator or a
mod sees, enumerated from the registrations of the stock releases: the Quake engine's server and
host (q1), the QuakeWorld dedicated server (qw). Facts in our own words. "Ours" names a file of
barotaliga/ge; a setting known under another name of ours says which.

Statuses: `done`, `missing` (rank 1 noticed at once, 2 sometimes, 3 rare or cosmetic),
`not-taken` (with the reason).

Where our server sits: network play (co-op and deathmatch alike) speaks the QuakeWorld protocol
(`games/shooters_edge/net/qw_server.cpp`, `qw_signon.cpp`, `qw_game.cpp`); the program runs on our
QC VM (`games/shooters_edge/server/qc_server.cpp`, `commands.cpp`, the `bi_*.cpp` builtins put at
their numbers by `catalogue.cpp`); the operator's console and settings are the engine's server core
(`engine/server/server.cpp`, `admin.cpp`, `bans.cpp`, `guards.cpp`, `main.cpp`). A console line the
core does not know goes to the rules' console (`qw_game.cpp` hands it to `commands.cpp`, which takes
`map`, `changelevel`, `restart`, `set`, `serverinfo`, `localinfo` and any setting by name). The
classic names of settings are synonyms of ours by `qw_game.cpp`'s table of setting names.

### S-A. Operator console commands (QuakeWorld server)

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S1 | `logfile` (qw) | toggles copying the console text to `qconsole.log` in the game folder ("Logging text to ...", "File logging off.") | done | `sv_logfile` names the file the server log is added to (`engine/server/admin.cpp`): a setting, not a toggle | |
| S2 | `fraglogfile` (qw) | toggles a frag log, the first unused `frag_N.log` (N 0..999); every `logfrag` line is written to it ("Logging frags to ...", "Can't open any logfiles.") | missing | frag lines are kept in memory (`server/bi_settings.cpp` logfrag, read by `net/qw_team.cpp`) but never written to a file | 3 |
| S3 | `snap <userid>` (qw) | asks one client for a screenshot, uploaded into the game folder's `snap/` as `<userid>-NN.pcx` (00..99); the operator (also over rcon) is told when it arrives | done | net/qw_admin.cpp (qw_snap: `snap`, the blocks kept as <userid>-NN in snap_dir) | 3 |
| S4 | `snapall` (qw) | the same for every client | done | net/qw_admin.cpp (`snapall`) | 3 |
| S5 | `kick <userid>` (both) | drops the player; everyone reads "<name> was kicked", he reads "You were kicked from the game". q1: `kick <name>` or `kick # <n>` with an optional reason, the player reads "Kicked by <who>: <reason>" | done | `engine/server/admin.cpp` and `net/qw_admin.cpp` (`kick <id or name>`) | |
| S6 | `status` (both) | the operator's table: the server's address, CPU use, mean response time, packets a frame, then each client's frags, user id, address, name, rate, ping, loss and qport (a narrower layout over rcon, "(s)" for a spectator, CONNECTING or ZOMBIE). q1: the host name, version, map, players and each one's frags, time and address | done | `engine/server/admin.cpp`, `net/qw_admin.cpp` (our own columns) | |
| S7 | `map <name>` (both) | refuses a map whose file is missing ("Can't find maps/<name>.bsp"), else tells clients `changing`, loads it and tells them `reconnect`. q1: also ends a running game and resets the spawn parms | done | `server/commands.cpp` (the rules' console), reached from the operator console by `net/qw_game.cpp` | |
| S8 | `setmaster <address...>` or `none` (qw) | sets up to the master list (port 27000 when none given), pings each ("Master server at ...", "Sending a ping.") and sends a heartbeat at once; `none` turns it off | done | plugins/classic/protocol/masters.cpp, games/shooters_edge/net/qw_game.cpp (`setmaster`, `heartbeat`) |  |
| S9 | `say <text>` (operator, both) | a chat line from the console, "console: <text>" to every player | done | `server/commands.cpp` (run_console_line: "console: " in a multiplayer game, "<hostname> " in a single-player one) | 3 |
| S10 | `heartbeat` (qw) | sends the heartbeat to the masters now | done | net/qw_game.cpp (masters_line: `heartbeat` forces the next one) | 3 |
| S11 | `quit` (both) | tells every client "server shutdown" and exits; a master is told the server is going | done | `engine/server/main.cpp` (`quit`), `net/qw_game.cpp` (destroy: qw_shutdown, then the masters) | 3 |
| S12 | `god <userid>` (qw, needs the `-cheats` start switch) | toggles one player's godmode, he reads "godmode ON" or "godmode OFF"; without the switch "You must run the server with -cheats to enable this command." (and `*cheats ON` in the server info) | missing | `server/commands.cpp`: a single-player program's `god` works with the original's words; under a multiplayer program `god` from a player with cheats on does nothing, and no operator form by user id | 3 |
| S13 | `give <userid> <item> <n>` (qw, cheats) | gives a weapon (2..8), shells, nails, rockets, cells or health | missing | as S12: works for a single-player program, nothing under a multiplayer one | 3 |
| S14 | `noclip <userid>` (qw, cheats) | toggles one player's noclip, "noclip ON" or "noclip OFF" | done | `server/commands.cpp` toggles noclip for the asking player with cheats on (the words only in the single-player form) | |
| S15 | `serverinfo [<key> <value>]` (qw) | lists the server info, or sets a key (a star key refused: "Star variables cannot be changed."), a setting of that name too, and tells every client | done | `engine/console/console_builtins.cpp`, `server/commands.cpp` | |
| S16 | `localinfo [<key> <value>]` (qw) | the server's private key list the program reads with `infokey(world, ...)` | done | `server/commands.cpp` (`set_local_info`, with the program's hook) | |
| S17 | `user <userid>` (qw) | prints one player's user info | done | net/qw_admin.cpp (`user <id|name>`: a key a line, padded to 20) | 3 |
| S18 | `gamedir <dir>` (qw) | switches the game folder (a single name, no path: "Gamedir should be a single filename, not a path") and sets `*gamedir` | done | `sv_mod` (`engine/server/server.cpp`) and the `mod` command (`engine/server/main.cpp`) | |
| S19 | `sv_gamedir <dir>` (qw) | changes only the `*gamedir` the server advertises | missing | `*gamedir` follows the mod folder (`net/qw_game.cpp`); not set on its own | 3 |
| S20 | `floodprot <msgs> <seconds> <silence>` (qw) | the chat flood rule (up to 10 messages tracked; stock 4 in 4 s, then 10 s silent) | done | `sv_floodprot`, `floodprot` a synonym (`net/qw_game.cpp`, `engine/server/guards.cpp`); our default is "10 1 1" | |
| S21 | `floodprotmsg <text>` (qw) | the line a flooder reads instead of the default | done | `sv_floodprotmsg` (synonym) | |
| S22 | `addip <ip>` (qw) | adds an address filter; missing trailing parts match anything | done | `engine/server/bans.cpp`, `admin.cpp` | |
| S23 | `removeip <ip>` (qw) | removes a filter written the same way ("Removed.", "Didn't find ...") | done | `engine/server/bans.cpp` | |
| S24 | `listip` (qw) | prints the filters | done | `engine/server/bans.cpp` | |
| S25 | `writeip` (qw) | writes `addip` lines to `listip.cfg` in the game folder | done | `engine/server/bans.cpp` (`sv_banfile`) | |
| S26 | `edict <n>` (both) | prints one entity's fields | not-taken | developer tool: the VM's own tracing (`plugins/progs/tool.cpp`) | |
| S27 | `edicts` (both) | prints every entity | not-taken | developer tool | |
| S28 | `edictcount` (both) | counts entities by kind | not-taken | developer tool | |
| S29 | `profile` (both) | the program's busiest functions | not-taken | covered by `ge qc profile` (`plugins/progs/tool.cpp`) | |
| S30 | `stuffcmds` (both) | runs the `+` commands of the command line | done | `+set` and the command line's settings (`engine/server/main.cpp`, `config.cpp`) | |
| S31 | `exec <file>` (both) | runs a config file; `server.cfg` runs at start | done | `engine/console/console_builtins.cpp`, `engine/server/config.cpp` | |
| S32 | `echo` (both) | prints its arguments | done | `engine/console/console_builtins.cpp` | |
| S33 | `alias` (both) | names a command line | done | `engine/console/console_builtins.cpp` | |
| S34 | `wait` (both) | holds the rest of the buffer a frame | done | `engine/console/console_builtins.cpp` | |
| S35 | `path` (both) | prints the search path | not-taken | developer listing | |
| S36 | `flush` (both) | empties the model cache | not-taken | no such cache in our tree | |
| S37 | the start map (qw) | without a `map` on the command line the server loads `start`; failing that it stops ("Couldn't spawn a server") | done | `sv_map` (`engine/server/server.cpp`) | |
| S38 | the operator's console on the terminal (both) | lines typed on the server's terminal run as console commands | missing | the dedicated server takes lines from its commands file (`--commands`, `engine/server/main.cpp`) and rcon, not the terminal | 3 |

### S-B. Quake host and network commands (q1 only)

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S39 | `notarget` (q1) | toggles the no-target flag, "notarget ON/OFF" (single player or co-op) | done | `server/commands.cpp` | |
| S40 | `fly` (q1) | toggles fly mode, "flymode ON/OFF" | done | `server/commands.cpp` | |
| S41 | `restart` (q1) | reloads the map with the parms it started with | done | `server/commands.cpp` | |
| S42 | `changelevel <map>` (q1) | goes to the next map keeping the players' parms ("Only the server may changelevel" from a client) | done | `server/commands.cpp`, `bi_settings.cpp` | |
| S43 | `connect <address>` (q1) | joins a server | done | client side: `net/qw_remote.cpp` (the client reader's row) | |
| S44 | `reconnect` (q1) | joins the server again after a level change | done | `net/qw_remote.cpp`, `qw_client.cpp` | |
| S45 | `name <text>` (q1) | sets the player's name; on a server everyone reads "<old> renamed to <new>" | done | the `name` key of the user info (`net/qw_signon.cpp`, `engine/server/names.hpp`) | |
| S46 | `version` (q1) | prints the build's version and date | done | engine/server/main.cpp (`version` prints the game's ServerGame::version_text) | 3 |
| S47 | `tell <name> <text>` (q1) | a private line to one player, "<sender>: <text>" | done | server/commands.cpp (quake_tell: whole name ignoring case, the first in slot order, cut to 62 bytes; the console's refusal) | 3 |
| S48 | `color <shirt> [pants]` (q1) | sets the player's colours (0..13) | done | `topcolor` and `bottomcolor` in the user info (`net/qw_client.cpp`) | |
| S49 | `ping` (q1) | prints "Client ping times:" and each player's ping | done | `pings` (`net/qw_signon.cpp`) feeds the scoreboard; the client shows them | |
| S50 | `load <name>` (q1) | loads a saved game ("Loading game from ...", "Savegame is version N, not 5") | done | `client/game_folder.cpp`, `server/campaign_save.cpp` | |
| S51 | `save <name>` (q1) | saves the game; refused in multiplayer, at intermission, with a dead player or with a path ("Can't save multiplayer games.") | done | `client/game_folder.cpp`, `server/campaign_save.cpp` | |
| S52 | `startdemos`, `demos`, `stopdemo` (q1) | the attract loop's demos (the client reader's rows) | done | `client/main.cpp` | |
| S53 | `viewmodel`, `viewframe`, `viewnext`, `viewprev` (q1) | a model viewer on the map's viewthing entity | not-taken | developer tool: our models lab | |
| S54 | `mcache` (q1) | prints the model cache | not-taken | developer tool | |
| S55 | `slist` (q1) | lists the LAN's servers | done | the server browser (`net/qw_browse.cpp`, the menu) | |
| S56 | `listen <0 or 1>` (q1) | whether the game takes network players | done | hosting from the window (`engine/server/spawn.hpp`, `listen_console.cpp`) | |
| S57 | `maxplayers <n>` (q1) | the player count for the next map; 1 stops listening | done | `sv_maxclients` (synonym `maxclients`) | |
| S58 | `port <n>` (q1) | the UDP port (default 26000) | done | `sv_port` (`engine/server/listen_console.cpp`) | |
| S59 | `net_stats` (q1) | the network layer's counters | not-taken | developer counters: `netstatus` | |
| S60 | `ban <address> [mask]` or `off` (q1) | one address-and-mask ban ("You have been banned." to a refused connect) | done | `ban` and the address filters (`engine/server/bans.cpp`) | |
| S61 | `test <host>`, `test2 <host>` (q1) | developer probes of a server's player and rule lists | not-taken | developer tools | |
| S62 | `cmd <text>` (both) | sends a line to the server as a client command | done | `net/game_commands.cpp`, `qw_client.cpp` | |
| S63 | `changelevel2` (q1) | the unit-based level change | not-taken | compiled out of the stock release | |
| S64 | `please` (q1) | a staff-only privilege grant | not-taken | compiled out of the stock release | |

### S-C. Settings (cvars) of both servers

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S65 | `sv_aim` (both; q1 0.93, qw 2) | the autoaim cone's cosine; 2 in qw means no autoaim | done | `server/bi_world.cpp`: the single-player form aims by it, the multiplayer form returns the view straight (as qw's 2 does) | |
| S66 | `sv_mintic` (qw, 0.03) | the shortest physics frame | done | `plugins/shooter/settings.cpp` | |
| S67 | `sv_maxtic` (qw, 0.1) | the longest physics frame | done | `plugins/shooter/settings.cpp` | |
| S68 | `developer` (both, 0) | extra console messages; q1's `dprint` only shows with it | missing | no switch on our server: `dprint` lines always reach the console (`server/bi_message.cpp`), as qw does | 3 |
| S69 | `timeout` (qw, 65) | seconds without a packet before a client is dropped, everyone told "<name> timed out" | done | `sv_timeout` (synonym), `net/qw_server.cpp` | |
| S70 | `zombietime` (qw, 2) | seconds a dropped slot stays taken so the last reliable message is resent | missing | a dropped slot is freed at once; the disconnect is sent twice instead (`net/qw_server.cpp`) | 3 |
| S71 | `rcon_password` (qw) | the remote console's password; empty: no rcon | done | `engine/server/server.cpp`, `net/qw_admin.cpp` | |
| S72 | `password` (qw) | the players' join password ("none" or empty: none) | done | `sv_password` (synonym), `net/qw_server.cpp` | |
| S73 | `spectator_password` (qw) | the spectators' join password | done | `sv_spectatorpassword` (synonym) | |
| S74 | `allow_download` (qw, 1) | any downloads at all | done | `sv_download` (synonym), `net/qw_signon.cpp` | |
| S75 | `allow_download_skins` (qw, 1) | files under `skins/` | done | `sv_download_skins` | |
| S76 | `allow_download_models` (qw, 1) | files under `progs/` | done | `sv_download_models` | |
| S77 | `allow_download_sounds` (qw, 1) | files under `sound/` | done | `sv_download_sounds` | |
| S78 | `allow_download_maps` (qw, 1) | files under `maps/`, never from a pack | done | `sv_download_maps` | |
| S79 | `sv_highchars` (qw, 1) | 0 strips characters outside plain printable ASCII from a joining player's user info | missing | no such switch | 3 |
| S80 | `sv_phs` (qw, 1) | sounds and multicasts limited to who can hear them | done | `server/bi_message.cpp`, `qc_server.cpp` | |
| S81 | `pausable` (both, 1) | whether players may pause | done | `server/commands.cpp`, `qc_server.cpp` | |
| S82 | `fraglimit` (both, server info) | frags that end the match, read by the program | done | `server/qc_server.cpp` (published in the server info) | |
| S83 | `timelimit` (both, server info) | minutes of a match | done | `server/qc_server.cpp` | |
| S84 | `teamplay` (both, server info) | the team rules the program reads | done | `server/qc_server.cpp`, `net/qw_server.cpp` | |
| S85 | `samelevel` (both; server info in qw) | stay on the same map | done | `server/qc_server.cpp` | |
| S86 | `maxclients` (qw, 8, server info) | the player slots (capped at 32) | done | `sv_maxclients` (synonym) | |
| S87 | `maxspectators` (qw, 8, server info) | the spectator slots | done | `sv_maxspectators` (synonym); not published in the server info (see S202) | |
| S88 | `deathmatch` (both; q1 0, qw 1) | the deathmatch mode the program reads | done | `server/qc_server.cpp` | |
| S89 | `spawn` (qw, server info) | a published number nothing in the server or the stock program reads | not-taken | no reader | |
| S90 | `watervis` (qw, server info) | tells clients they may see through water | done | `sv_watervis` (synonym), `net/qw_game.cpp` | |
| S91 | `hostname` (both) | the server's name | done | `sv_hostname` (synonym) | |
| S92 | `filterban` (qw, 1) | 1: the filters ban; 0: only filtered addresses may enter | done | `sv_banmode` (synonym), `engine/server/admin.cpp` | |
| S93 | `sv_maxvelocity` (both, 2000) | the speed cap per axis | done | `plugins/shooter/settings.cpp` | |
| S94 | `sv_gravity` (both, 800) | gravity | done | `plugins/shooter/settings.cpp` | |
| S95 | `sv_stopspeed` (both, 100) | the friction's floor speed | done | `plugins/shooter/settings.cpp` | |
| S96 | `sv_maxspeed` (both, 320) | the run speed | done | `plugins/shooter/settings.cpp` | |
| S97 | `sv_spectatormaxspeed` (qw, 500) | a spectator's speed | done | `plugins/shooter/settings.cpp` | |
| S98 | `sv_accelerate` (both, 10) | ground acceleration | done | `plugins/shooter/settings.cpp` | |
| S99 | `sv_airaccelerate` (qw, 0.7) | air acceleration | done | `plugins/shooter/settings.cpp` | |
| S100 | `sv_wateraccelerate` (qw, 10) | water acceleration | done | `plugins/shooter/settings.cpp` | |
| S101 | `sv_friction` (both, 4) | ground friction | done | `plugins/shooter/settings.cpp` | |
| S102 | `sv_waterfriction` (qw, 4) | water friction | done | `plugins/shooter/settings.cpp` | |
| S103 | `cl_rollspeed` (qw server, 200) | the sideways speed at which a player's model leans fully | done | fixed at the default, not a setting (`plugins/shooter/server.hpp`, `framemove.hpp`) | |
| S104 | `cl_rollangle` (qw server, 2) | the lean's angle (the model shows four times it) | done | fixed at the default, as S103 | |
| S105 | `sv_spectalk` (qw, 1) | 0: a spectator's chat reaches only spectators | done | `engine/server/chat.cpp`; ours mutes only during a match | |
| S106 | `sv_mapcheck` (qw, 1) | drops a client whose map checksum differs ("Map model file does not match ...") | missing | the checksum is computed (`net/qw_mapsum.hpp`) but the server never compares a client's | 3 |
| S107 | `sys_nostdout` (qw) | silences the terminal | not-taken | platform switch | |
| S108 | `sys_extrasleep` (qw) | extra sleep a frame | not-taken | platform switch | |
| S109 | `registered` (both) | 1 when the full game's pack is present; the program reads it | done | `server/qc_server.cpp` | |
| S110 | `showpackets` (qw) | prints every packet | not-taken | developer trace: `netstatus` and our captures | |
| S111 | `showdrop` (qw) | prints dropped packets | not-taken | developer trace | |
| S112 | `qport` (qw binary) | a client's port id, unused by the server's own sends | not-taken | the server reads each client's from its packets (`net/qw_server.cpp`) | |
| S113 | `host_framerate` (q1) | a fixed frame time (slow motion) | not-taken | the campaign steps at a fixed time so replays hold (q1-sp-gaps A20) | |
| S114 | `host_speeds` (q1) | prints the frame's time split | not-taken | developer timing: our profiler | |
| S115 | `sys_ticrate` (q1, 0.05) | the dedicated server's frame time | not-taken | `sv_mintic` sets our frame | |
| S116 | `serverprofile` (q1) | prints the server's time a frame | not-taken | developer timing | |
| S117 | `noexit` (q1, server rule) | the program reads it to block level exits in deathmatch | done | server/qc_server.cpp (registered for programs to read) | 3 |
| S118 | `skill` (q1, 1) | the monsters' skill | done | `server/qc_server.cpp` | |
| S119 | `coop` (q1, 0) | co-op | done | `server/qc_server.cpp` | |
| S120 | `temp1` (q1) | a spare setting programs read | done | server/qc_server.cpp | 3 |
| S121 | `edgefriction` (q1, 2) | extra friction at a ledge | done | `plugins/shooter/settings.cpp` | |
| S122 | `sv_idealpitchscale` (q1, 0.8) | how far the view tilts on slopes | done | `server/qc_server.cpp`, `client_data.cpp` | |
| S123 | `sv_nostep` (q1, 0) | turns off stepping up stairs | done | `plugins/shooter/settings.cpp` | |
| S124 | `nomonsters` (q1) | a setting programs may read | done | server/qc_server.cpp | 3 |
| S125 | `gamecfg` (q1) | a setting programs may read | done | server/qc_server.cpp | 3 |
| S126 | `scratch1`..`scratch4` (q1) | four spare settings for programs | done | server/qc_server.cpp (scratch1..scratch4) | 3 |
| S127 | `savedgamecfg` (q1, archived) | a program's setting kept in the config | done | server/qc_server.cpp (kept in the window's config) | 3 |
| S128 | `saved1`..`saved4` (q1, archived) | four program settings kept in the config | done | server/qc_server.cpp (saved1..saved4, kept in the window's config) | 3 |
| S129 | `net_messagetimeout` (q1, 300) | seconds of silence before a connection is closed | done | `sv_timeout` | |
| S130 | `_config_com_port`, `_config_com_irq`, `_config_com_baud`, `_config_com_modem`, `_config_modem_dialtype`, `_config_modem_clear`, `_config_modem_init`, `_config_modem_hangup` (q1) | serial and modem play | not-taken | platform (serial links) | |
| S131 | `idgods` (q1) | staff privilege | not-taken | staff-only | |
| S132 | `cmdline` (q1, server info) | the command line as a readable setting | not-taken | no reader | |

### S-D. Client-to-server messages

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S133 | `clc_nop` (both) | nothing (keeps the link alive) | done | `net/qw_server.cpp` | |
| S134 | `clc_delta` (qw) | the frame a client holds, so entities come as a delta from it | done | `net/qw_server.cpp`, `qw_delta.cpp` | |
| S135 | `clc_move` (qw) | a checksum byte, the client's loss percent, then three commands (two older, one new) each a delta from the one before; one per packet (a second ends the packet), the checksum keyed by the sequence (a wrong one drops the rest), after a loss of fewer than 20 packets the lost commands are run first | done | `net/qw_server.cpp` (`move_checksum`), `plugins/shooter/client.hpp` | |
| S136 | the command's length (qw) | a command over 50 ms runs as two halves, the second without its impulse; the frame time capped at 0.1 s; the buttons dropped from the repeat of a lost command | done | `plugins/shooter/pmove.hpp`, `client.hpp` | |
| S137 | `clc_stringcmd` (both) | a client command line (S-E) | done | `net/qw_signon.cpp`, `server/commands.cpp` | |
| S138 | `clc_tmove` (qw) | a spectator moves himself to a point (refused to players) | done | `net/qw_server.cpp` | |
| S139 | `clc_upload` (qw) | a piece of an upload (a screenshot for `snap`): size, percent, bytes; the server answers `nextul` or, unasked, "Upload denied" and `stopul` | done | net/qw_server.cpp, net/qw_admin.cpp (a screenshot's blocks taken when asked; nextul and stopul) | 3 |
| S140 | an unknown or unreadable message (both) | the client is dropped | done | `net/qw_server.cpp` | |
| S141 | `clc_disconnect` (q1) | the client leaves | not-taken | network play speaks the QuakeWorld protocol; protocol-15 clients reach only our TV face (`net/q1_tv_netgame.cpp`) | |
| S142 | `clc_move` (q1) | send time, view angles, forward, side and up moves, buttons, impulse | not-taken | as S141 | |
| S143 | `clc_stringcmd` (q1) | only these lines run from a client: status, god, notarget, fly, name, noclip, say, say_team, tell, color, kill, pause, spawn, begin, prespawn, kick, ping, give, ban; anything else is ignored | not-taken | as S141; the same commands under the QuakeWorld protocol are rows of S-B and S-E | |

### S-E. A client's commands to the QuakeWorld server

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S144 | `new` | starts the signon: protocol, spawn count, game folder (`qw` when unset), player number (bit 7 for a spectator), the level's name, the ten move numbers, the CD track, and `fullserverinfo` | done | `net/qw_signon.cpp` | |
| S145 | `modellist <count> <from>` | a page of the model list (half a message at most) and where the next starts; a stale count restarts with `new` | done | `net/qw_signon.cpp` | |
| S146 | `soundlist <count> <from>` | the same for sounds | done | `net/qw_signon.cpp` | |
| S147 | `prespawn <count> <n> [checksum]` | the signon pages; the first carries the map checksum (S106); the last asks the client for `spawn` | done | `net/qw_signon.cpp` (checksum not compared) | |
| S148 | `spawn <count> <from>` | every player's frags, ping, loss, time and info, the light styles, the entity cleared, gravity and maxspeed set, the level's totals, then `skins` | done | `net/qw_signon.cpp` | |
| S149 | `begin <count>` | the program's ClientConnect and PutClientInServer run (a spectator is put at the first player start and runs SpectatorConnect); a paused server tells him | done | `net/qw_signon.cpp`, `server/qc_server.cpp` | |
| S150 | `drop` | the client leaves; everyone reads "<name> dropped" (not for a spectator) | done | `net/qw_signon.cpp` (the program's own leaving line is printed, the server's "dropped" is not) | |
| S151 | `pings` | every player's ping and loss, for the scoreboard | done | `net/qw_signon.cpp` | |
| S152 | `rate [<n>]` | sets or shows the rate (500..10000): "Net rate set to N", "Current rate is N" | done | `server/commands.cpp`, `net/qw_signon.cpp` (bounds `sv_minrate`, `sv_maxrate`; no reply line) | |
| S153 | `kill` | the program's ClientKill; a dead player reads "Can't suicide -- allready dead!" | done | `server/commands.cpp` (the words in the single-player form only) | |
| S154 | `pause` | toggles the pause (S167) | done | `server/commands.cpp` | |
| S155 | `msg [<level>]` | sets or shows the lowest print level the player gets ("Msg level set to N") | done | net/qw_signon.cpp ("Current msg level is N", "Msg level set to N"; no range) | 3 |
| S156 | `say <text>` | public chat, "<name>: <text>" (S179) | done | `net/qw_signon.cpp`, `engine/server/chat.cpp` | |
| S157 | `say_team <text>` | team chat, "(<name>): <text>" to the same `team` key | done | `engine/server/chat.cpp` | |
| S158 | `setinfo [<key> <value>]` | changes a user info key (a star key refused), tells everyone; alone, lists them | done | `net/qw_signon.cpp` | |
| S159 | `serverinfo` | lists the server info to the client | done | `net/qw_signon.cpp` | |
| S160 | `download <path>` | starts a download (S182) | done | `net/qw_signon.cpp` | |
| S161 | `nextdl` | the next piece, 768 bytes at most, with the percent | done | `net/qw_signon.cpp` | |
| S162 | `ptrack [<slot>]` | a spectator follows a player ("Invalid client to track"); alone, stops | done | `net/qw_signon.cpp` | |
| S163 | `snap` | the client refuses a screenshot asked of it: everyone reads "<name> refused remote screenshot" | done | net/qw_signon.cpp ("<name> refused remote screenshot") | 3 |
| S164 | an unknown command | "Bad user command: <word>" | done | `server/commands.cpp` | |

### S-F. Connectionless packets

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S165 | `ping`, or `k` alone (qw) | answered with `l` | done | `net/qw_server.cpp` | |
| S166 | `l` (qw, an acknowledgement) | printed on the console with its sender | not-taken | nothing to answer | |
| S167 | `status` (qw) | the server info, then one line for each player (spectators left out): user id, frags, minutes connected, ping, "name", "skin", top and bottom colour (0..13) | done | `net/qw_server.cpp` (`sv_status`, `sv_statusrate`) | |
| S168 | `log [<sequence>]` (qw) | the frag log's last full buffer ("stdlog <n>" and the lines), or `m` when the asker has it or no frag log runs; the buffer turns over at 4 KB or after ten minutes | missing | not answered (`net/qw_server.cpp`) | 3 |
| S169 | `getchallenge` (qw) | `c` and a number kept per address | done | `net/qw_server.cpp` | |
| S170 | `connect <version> <qport> <challenge> "<userinfo>"` (qw) | accepted with `j`; refused with a print: "Server is version 2.40.", "Bad challenge.", "No challenge for address.", "requires a spectator password", "server requires a password", "server is full"; a second connect from the same address and qport while connecting is ignored, a reconnect drops the old slot | done | `net/qw_server.cpp` | |
| S171 | `rcon <password> <line>` (qw) | runs a console line, its output sent back; a wrong password reads "Bad rcon_password." and is logged | done | `net/qw_admin.cpp`, `engine/server/admin.cpp` | |
| S172 | anything else (qw) | logged as a bad connectionless packet | done | ignored (`net/qw_server.cpp`) | |
| S173 | a filtered address (qw) | every packet answered with a "banned." print and dropped | done | `engine/server/admin.cpp` ("You are banned from this server.") | |
| S174 | the heartbeat (qw, to the masters) | every 300 s and at once after `setmaster`: `a`, a sequence and the active player count; `C` when the server stops | done | plugins/classic/protocol/masters.cpp (`a` beats every 300 s, `C` at quit) |  |
| S175 | the connect request (q1, protocol 15 control packet) | the game's name, net version 3; accepted with a new port, refused with "Incompatible version.", "You have been banned.", "Server is full."; a repeat within 2 s gets the same accept | not-taken | network play speaks the QuakeWorld protocol; our TV face takes protocol-15 viewers (`net/q1_tv_netgame.cpp`) | |
| S176 | the server info request (q1) | address, host name, map, players, slots, net version | not-taken | as S175 | |
| S177 | the player info request (q1) | the n-th player's name, colours, frags, time and address | not-taken | as S175 | |
| S178 | the rule info request (q1) | the server's settings one by one, each after the last named | not-taken | as S175 | |

### S-G. Movement types and solids

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S179 | movetype 0 `none` | never moves, only thinks | done | `plugins/shooter/entity.hpp`, `physics.cpp` | |
| S180 | movetypes 1 and 2 (the angle kinds) | unused: the physics stops the map on them | not-taken | refused alike | |
| S181 | movetype 3 `walk` | the player move (q1: the server's own step; qw: the predicted move) | done | `plugins/shooter/framemove.cpp` (`sv_playermove`), `pmove` | |
| S182 | movetype 4 `step` | monsters: gravity when off the ground, else only thinks; landing plays the land sound | done | `plugins/shooter/physics.cpp` | |
| S183 | movetype 5 `fly` | the toss rules without gravity | done | `plugins/shooter/physics.cpp` | |
| S184 | movetype 6 `toss` | gravity, stops on the ground | done | `plugins/shooter/physics.cpp` | |
| S185 | movetype 7 `push` | doors, plats, trains: no clip against the world, pushes and crushes, `blocked` called | done | `plugins/shooter/physics.cpp` | |
| S186 | movetype 8 `noclip` | straight motion through everything | done | `plugins/shooter/physics.cpp` | |
| S187 | movetype 9 `flymissile` | fly, with a larger box against monsters | done | `plugins/shooter/physics.cpp` | |
| S188 | movetype 10 `bounce` | toss that rebounds | done | `plugins/shooter/physics.cpp` | |
| S189 | solid 0 `not` | no interaction | done | `plugins/shooter/entity.hpp` | |
| S190 | solid 1 `trigger` | touched, never blocks | done | `plugins/shooter/entity.hpp` | |
| S191 | solid 2 `bbox` | blocks as its box | done | `plugins/shooter/entity.hpp` | |
| S192 | solid 3 `slidebox` | blocks as its box, never ground to stand on | done | `plugins/shooter/entity.hpp` | |
| S193 | solid 4 `bsp` | blocks by its brush model (only with movetype push, else the map stops) | done | `plugins/shooter/entity.hpp` | |

### S-H. The program's entry points the server calls

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S194 | StartFrame, PlayerPreThink, PlayerPostThink, ClientKill, ClientConnect, PutClientInServer, ClientDisconnect, SetNewParms, SetChangeParms (both) | the program's hooks each frame and through a client's life | done | `plugins/progs/layout.cpp`, `server/qc_server.cpp` | |
| S195 | SpectatorConnect, SpectatorThink, SpectatorDisconnect (qw) | a spectator's hooks, when the program has them | done | `plugins/progs/layout.cpp`, `sim/qc_program.cpp` | |

### S-I. Info keys the server reads or writes

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S196 | `*version` (server info) | the server's version | done | `net/qw_server.cpp` | |
| S197 | `*progs` (server info) | the program's checksum | missing | not published | 3 |
| S198 | `*gamedir` (server info) | the game folder | done | `net/qw_game.cpp` | |
| S199 | `*cheats` (server info) | "ON" when started with cheats | missing | not published | 3 |
| S200 | `map` (server info) | the map's name | done | `net/qw_server.cpp` | |
| S201 | `needpass` (server info) | 1 a player password, 2 a spectator password, 3 both ("Updated needpass.") | done | `net/qw_server.cpp` | |
| S202 | server settings in the server info (`hostname`, `maxclients`, `maxspectators`, `fraglimit`, `timelimit`, `teamplay`, `deathmatch`, `samelevel`, `spawn`, `watervis`) | each copied into the server info when set | missing | all but `maxspectators` (and `spawn`, S89) are published (`net/qw_server.cpp`, `server/qc_server.cpp`) | 3 |
| S203 | `name` (user info) | trimmed of spaces and line ends; empty or "console" becomes "unnamed"; a taken name gets "(N)" in front; more than four changes inside 5 s kicks ("<name> was kicked for name spam", "You were kicked from the game for name spamming"); a change in play is announced "<old> changed name to <new>" | done | `net/qw_server.cpp`, `qw_signon.cpp`, `engine/server/names.hpp` (`sv_namelimit` refuses the change instead of kicking) | |
| S204 | `rate` (user info) | the rate, held to 500..10000 | done | `net/qw_signon.cpp` (`sv_minrate`, `sv_maxrate`) | |
| S205 | `msg` (user info) | the lowest print level sent to the player | done | `net/qw_signon.cpp` | |
| S206 | `spectator` (user info) | joins as a spectator; its value is the spectator password; removed and replaced by `*spectator 1` | done | `net/qw_server.cpp` | |
| S207 | `password` (user info) | the join password, removed once checked | done | `net/qw_server.cpp` | |
| S208 | `team` (user info) | who team chat reaches | done | `engine/server/chat.cpp` | |
| S209 | `skin`, `topcolor`, `bottomcolor` (user info) | reported in the status packet | done | `net/qw_server.cpp` | |
| S210 | `pmodel`, `emodel` (user info) | checksums of the player and eyes models; a mismatch is announced "<name> WARNING: non standard player/eyes model detected" | missing | not checked | 3 |
| S211 | `noaim` (user info) | above 0, no autoaim for that player | done | the multiplayer form never autoaims (`server/bi_world.cpp`) | |
| S212 | keys starting with `_` (user info) | kept from the other players | done | `net/qw_signon.cpp` (`info_public`) | |
| S213 | `ip`, `ping` (`infokey` on a player) | the address and ping, not keys of the info | done | `server/bi_settings.cpp` | |

### S-J. Builtins by number (q1 and qw)

The numbers 0..78 are shared; q1's program and qw's differ in a few argument shapes, said in the row.
Ours: `games/shooters_edge/server/catalogue.cpp` puts each at its number and reads the shape by the
program's form.

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S214 | #0 | unused: an error | done | `server/catalogue.cpp` | |
| S215 | #1 `makevectors` | the three direction vectors from angles | done | `server/bi_world.cpp` | |
| S216 | #2 `setorigin` | moves an entity and relinks it | done | `server/catalogue.cpp` | |
| S217 | #3 `setmodel` | sets the model by its precache; a brush model sets the size | done | `server/bi_world.cpp` | |
| S218 | #4 `setsize` | sets mins, maxs, size | done | `server/bi_world.cpp` | |
| S219 | #5 | unused (an old size builtin): an error | done | `server/catalogue.cpp` | |
| S220 | #6 `break` | a debugger stop | done | `server/bi_world.cpp` (an error) | |
| S221 | #7 `random` | 0..1 | done | `server/catalogue.cpp` (our seeded generator) | |
| S222 | #8 `sound` | starts a sound on an entity's channel; qw sends it only where it can be heard unless the channel's bit 8 or `sv_phs 0` | done | `server/bi_message.cpp` | |
| S223 | #9 `normalize` | the unit vector | done | `server/catalogue.cpp` | |
| S224 | #10 `error` | prints and stops the map | done | `server/bi_settings.cpp` | |
| S225 | #11 `objerror` | prints, frees self, stops | done | `server/bi_settings.cpp` | |
| S226 | #12 `vlen` | a vector's length | done | `server/catalogue.cpp` | |
| S227 | #13 `vectoyaw` | the yaw of a vector | done | `server/catalogue.cpp` | |
| S228 | #14 `spawn` | a new entity | done | `server/catalogue.cpp` | |
| S229 | #15 `remove` | frees an entity | done | `server/catalogue.cpp` | |
| S230 | #16 `traceline` | a line trace, the `trace_*` globals | done | `server/bi_world.cpp` | |
| S231 | #17 `checkclient` | a client a monster may see, rotated every 0.1 s | done | `server/bi_world.cpp` | |
| S232 | #18 `find` | the next entity whose string field matches | done | `server/bi_world.cpp` | |
| S233 | #19 `precache_sound` | adds a sound while loading | done | `server/bi_settings.cpp` | |
| S234 | #20 `precache_model` | adds a model while loading | done | `server/bi_settings.cpp` | |
| S235 | #21 `stuffcmd` | text to one client's console; qw drops the client on exactly "disconnect" and a newline | done | `server/bi_message.cpp` | |
| S236 | #22 `findradius` | the chain of entities within a radius | done | `server/bi_world.cpp` | |
| S237 | #23 `bprint` | a print to everyone (qw: with a level first) | done | `server/bi_message.cpp` | |
| S238 | #24 `sprint` | a print to one client (qw: with a level); a non-client prints "tried to sprint to a non-client" | done | `server/bi_message.cpp` | |
| S239 | #25 `dprint` | a console print (q1: only with `developer`) | done | `server/bi_message.cpp` (always printed, S68) | |
| S240 | #26 `ftos` | number to text | done | `server/catalogue.cpp` | |
| S241 | #27 `vtos` | vector to text | done | `server/catalogue.cpp` | |
| S242 | #28 `coredump` | prints every entity | done | `server/bi_world.cpp` | |
| S243 | #29 `traceon` | statement tracing on | done | `server/bi_world.cpp` | |
| S244 | #30 `traceoff` | tracing off | done | `server/bi_world.cpp` | |
| S245 | #31 `eprint` | prints one entity | done | `server/bi_world.cpp` | |
| S246 | #32 `walkmove` | a monster's step in a direction | done | `server/bi_monster.cpp` | |
| S247 | #33 | unused: an error | done | `server/catalogue.cpp` | |
| S248 | #34 `droptofloor` | drops self up to 256 units onto ground | done | `server/bi_world.cpp` | |
| S249 | #35 `lightstyle` | sets a light style, sent to every client | done | `server/bi_message.cpp` | |
| S250 | #36 `rint` | rounds | done | `server/catalogue.cpp` | |
| S251 | #37 `floor` | floor | done | `server/catalogue.cpp` | |
| S252 | #38 `ceil` | ceiling | done | `server/catalogue.cpp` | |
| S253 | #39 | unused: an error | done | `server/catalogue.cpp` | |
| S254 | #40 `checkbottom` | whether an entity stands on ground | done | `server/bi_monster.cpp` | |
| S255 | #41 `pointcontents` | the contents at a point | done | `server/bi_world.cpp` | |
| S256 | #42 | unused: an error | done | `server/catalogue.cpp` | |
| S257 | #43 `fabs` | absolute value | done | `server/catalogue.cpp` | |
| S258 | #44 `aim` | the autoaim direction by `sv_aim` (qw: skips players with `noaim`, and team-mates in teamplay) | done | `server/bi_world.cpp` | |
| S259 | #45 `cvar` | a setting's value | done | `server/bi_settings.cpp` | |
| S260 | #46 `localcmd` | text to the server's own console | done | `server/bi_settings.cpp` | |
| S261 | #47 `nextent` | the next entity in use | done | `server/bi_world.cpp` | |
| S262 | #48 `particle` (q1) / unused (qw) | a particle burst; in qw an error | done | `server/bi_message.cpp` | |
| S263 | #49 `changeyaw` | turns toward the ideal yaw | done | `server/bi_monster.cpp` | |
| S264 | #50 | unused: an error | done | `server/catalogue.cpp` | |
| S265 | #51 `vectoangles` | angles of a vector | done | `server/catalogue.cpp` | |
| S266 | #52 `WriteByte` | a byte to a message: the broadcast, one client, all reliably, the signon (only while loading), and in qw the multicast buffer | done | `server/bi_message.cpp` | |
| S267 | #53 `WriteChar` | a signed byte | done | `server/bi_message.cpp` | |
| S268 | #54 `WriteShort` | a short | done | `server/bi_message.cpp` | |
| S269 | #55 `WriteLong` | a long | done | `server/bi_message.cpp` | |
| S270 | #56 `WriteCoord` | a coordinate | done | `server/bi_message.cpp` | |
| S271 | #57 `WriteAngle` | an angle | done | `server/bi_message.cpp` | |
| S272 | #58 `WriteString` | a string | done | `server/bi_message.cpp` | |
| S273 | #59 `WriteEntity` | an entity number | done | `server/bi_message.cpp` | |
| S274 | #60 | unused in the stock tables: an error | done | ours `sin` there, an extension (`server/catalogue.cpp`) | |
| S275 | #61 | unused: an error | done | ours `cos`, an extension | |
| S276 | #62 | unused: an error | done | ours `sqrt`, an extension | |
| S277 | #63 | unused: an error | done | `server/catalogue.cpp` | |
| S278 | #64 | unused: an error | done | `server/catalogue.cpp` | |
| S279 | #65 | unused: an error | done | `server/catalogue.cpp` | |
| S280 | #66 | unused: an error | done | `server/catalogue.cpp` | |
| S281 | #67 `movetogoal` | a monster's chase step | done | `server/bi_monster.cpp` | |
| S282 | #68 `precache_file` | nothing, returns its argument | done | `server/catalogue.cpp` | |
| S283 | #69 `makestatic` | the entity becomes a static one and is freed | done | `server/bi_world.cpp` | |
| S284 | #70 `changelevel` | queues the level change once per map | done | `server/bi_settings.cpp` | |
| S285 | #71 | unused: an error | done | `server/catalogue.cpp` | |
| S286 | #72 `cvar_set` | sets a setting | done | `server/bi_settings.cpp` | |
| S287 | #73 `centerprint` | a centred print to one client | done | `server/bi_message.cpp` | |
| S288 | #74 `ambientsound` | a looping sound in the signon | done | `server/bi_message.cpp` | |
| S289 | #75 `precache_model2` | as #20 | done | `server/catalogue.cpp` | |
| S290 | #76 `precache_sound2` | as #19 | done | `server/catalogue.cpp` | |
| S291 | #77 `precache_file2` | as #68 | done | `server/catalogue.cpp` | |
| S292 | #78 `setspawnparms` | a client's saved parms into `parm1..16` | done | `server/bi_world.cpp` | |
| S293 | #79 `logfrag` (qw) | a frag line, backslash between killer and killed, to the frag log | done | `server/bi_settings.cpp` (the file and the `log` packet are S2, S168) | |
| S294 | #80 `infokey` (qw) | the world's server info (or local info), a player's user info, `ip`, `ping` | done | `server/bi_settings.cpp` | |
| S295 | #81 `stof` (qw) | text to number | done | `server/catalogue.cpp` | |
| S296 | #82 `multicast` (qw) | sends the multicast buffer to all, those who can hear, those who can see, each also reliably | done | `server/bi_message.cpp` | |

### S-K. The server's own words to players

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S297 | "server crashed: <error>" (qw) | every client is told why the server stopped, then disconnected | not-taken | our server never stops on a fault: a mod's error is caught and the game goes on (docs/porting/server.md § Gaps), so there is no crash to tell of | 3 |
| S298 | "server shutdown" (qw) | sent to every client on `quit` | done | `net/qw_server.cpp` (qw_shutdown) | 3 |
| S299 | "<name> timed out" (both) | everyone told after the silence of `timeout` | done | `net/qw_server.cpp` | |
| S300 | "<name> overflowed" (qw) | a client whose reliable stream overflowed is dropped and everyone told | done | `net/qw_server.cpp` (qw_frame) | 3 |
| S301 | "Pause not allowed.", "Spectators can not pause." (qw; q1 the first) | a refused pause | done | `server/commands.cpp` | |
| S302 | "<name> paused the game", "<name> unpaused the game" (both) | everyone told who paused | done | `server/commands.cpp` (request_pause; a multiplayer game words it by the state before, as QuakeWorld does) | 3 |
| S303 | "Server is paused." (qw) | a player who begins while paused | done | `server/commands.cpp` (tell_paused, from connect) | 3 |
| S304 | "Pause released since no players are left." (qw) | the server unpauses when the last player leaves | done | `server/commands.cpp` (request_pause) | 3 |
| S305 | chat lines (both) | "<name>: " public, "(<name>): " team, "[SPEC] <name>: " a spectator's (to spectators only with `sv_spectalk 0`, and always for his team chat), "console: " the operator's | done | `engine/server/chat.cpp` (the operator's is S9) | |
| S306 | flood protection (qw) | after the rule's count of chat lines in its seconds: "FloodProt: You can't talk for N seconds." (or the custom line), then "You can't talk for N more seconds" until it lifts; paused time does not count | done | `engine/server/guards.cpp` | |
| S307 | "Can't suicide -- allready dead!" (both) | `kill` while dead | done | `server/commands.cpp` (every game; spectators too) | 3 |
| S308 | "godmode ON/OFF", "notarget ON/OFF", "noclip ON/OFF", "flymode ON/OFF" (both) | the cheats' answers | done | `server/commands.cpp` (single-player form; see S12) | |
| S309 | "Kicked by <who>: <reason>" (q1) | a kick with its reason | done | `engine/server/admin.cpp` (our wording) | |
| S310 | "<name> entered the game" (q1, console) | the host's console notes a player's arrival | done | the server log (`net/qw_server.cpp`) | |
| S311 | "Spectator <name> connected", "Client <name> removed" (qw, console) | the operator's notes | done | the server log (`net/qw_server.cpp`) | |

### S-L. Downloads, logs and flood protection

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| S312 | the download rule (qw) | refused (an answer of size -1) when the path has "..", starts with "." or "/", has no folder, its kind is switched off, it is missing, or it is a map from a pack; the name is lowercased; "Downloading <file> to <name>" on the console | done | `net/qw_signon.cpp` (`download_allowed`, files read from the game folder) | |
| S313 | the download's pieces (qw) | 768 bytes a piece on the reliable stream, each with its percent, the next on `nextdl` | done | `net/qw_signon.cpp` (also the chunked form) | |
| S314 | the frag log's form (qw) | one line a frag, backslashes around killer and killed | done | `server/bi_settings.cpp` (kept in memory: S2, S168) | |
| S315 | the address filter's check (qw) | every packet from a filtered address is refused before anything else | done | `engine/server/admin.cpp` | |
| S316 | the server's statistics (qw) | CPU use, response time and packets a frame, measured over a run of frames, shown by `status` | done | `engine/server/admin.cpp` (our own counters) | |

Rows: 316 (some rows hold a group of names). Done: 267. Missing: 14 (rank 1: 0, rank 2: 0, rank 3: 14). Not-taken: 35.

## GAME: the QuakeWorld game program and Quake's monsters, row by row

Reader 3's section of the ledger (questions.md beside this file). Built from the stock release's
QuakeWorld game program, from porting/shooters-edge/qw-dm-rules.md and from the scratch answers
qw-dm2-answers.md; marked against our tree as on origin/main (commit 25887284). Facts in our
words; no reference file, routine or variable is named.

How to read the "ours" column:

- `rules/` is games/shooters_edge/rules/ and `profiles/` is games/shooters_edge/profiles/.
- Quake's own deathmatch rules run on the arena behind the local info `rules q1`, which Quake's
  packs write (rules/common_quake.cpp); the rows they fill name that file.
- "match mod" marks a row where our Quake words or rules deliberately follow the modern match mod
  (porting/shooters-edge/messages.md, deathmatch4.md) instead of the stock program; qw-dm-rules.md
  says those notes win, so such a row is not-taken.
- Rank (missing rows only): 1 noticed at once in normal play, 2 sometimes, 3 rare or cosmetic.

### 1. A player's life

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G1 | Spawn spot choice | a random deathmatch spot with no live player within 84 u, else any spot at random; the end spots of the list half as likely | done | rules/common_quake.cpp (quake_spawn_spot) | - |
| G2 | `testplayerstart` | a test spot that always wins; in practice freed at load, so never found | done | no such class in the arena; freed, as the original in practice | - |
| G3 | No deathmatch spot | the player appears at the world origin | not-taken | rules/arena_glue.cpp falls back to `info_player_start`, which is better and no stock DM map lacks spots | - |
| G4 | Spawn placement | 1 u above the spot, its angles snapped, velocity zero, view 22 u, health and max 100, air 12 s | done | rules/common_player.cpp | - |
| G5 | Spawn flash and sound | a teleport flash 20 u in front of the spot, then one of five `misc/r_tele` sounds 0.2 s later | done | rules/common_quake.cpp (quake_flash, tele_sound) | - |
| G6 | Telefrag at spawn | a box 1 u bigger than the player at the spot for 0.2 s kills anything with health in it for 50000 | done | rules/common_combat.cpp kills a live player within 48 u at the moment of spawning (same effect on players) | - |
| G7 | Default load-out | axe and shotgun in hand, 25 shells, no armour, 100 health | done | rules/arena_glue.cpp | - |
| G8 | Load-out carried over a map change | a player alive at the change keeps weapons, ammo and armour; health clamped 50 to 100, shells at least 25; keys and powers dropped; dead or new players start fresh | done | rules/common_quake.cpp (quake_change_parms, quake_carried_kit) | - |
| G9 | `start` map with episode progress | forces the default load-out | not-taken | the arena has no episodes; the campaign runs the pack's own program | - |
| G10 | Health clamp at death | health is held at no lower than -99 | done | rules/common_quake.cpp | - |
| G11 | Gibbing | below -40: a bouncing head (`progs/h_player.mdl`) and three gibs thrown by how far below zero the health went, gibs gone after 10 to 20 s | done | rules/common_quake.cpp (become_head, throw_piece) | - |
| G12 | Death toss | the body becomes non-solid, the view drops to 8 u below the origin, a slow body gets 0 to 300 u/s up | done | rules/common_quake.cpp (quake_die) | - |
| G13 | Death animations | the axe's own death, else one of five (the fifth twice as likely), 0.1 s a frame, the last frame held | done | rules/common_quake.cpp (quake_die, quake_dead_frame) | - |
| G14 | Run and stand cycles | rockrun or axrun while moving across the plan, stand or axstnd when still, 0.1 s a frame, a change restarts at the first frame | done | rules/common_player.cpp | - |
| G15 | Pain frames | pain1 to pain6 or axpain1 to axpain6, 0.6 s, refused while firing or under the Ring, restarted by a new hit | done | rules/common_quake.cpp (quake_pain) | - |
| G16 | Firing frames | the axe's four swings, shotatt, rockatt, nailatt and light frames with their view-model frames, the muzzle flash on each | done | rules/common_quake.cpp (quake_fire_frames) | - |
| G17 | Respawn rule | once the death frames end, all of attack, jump and use released, then any pressed; no automatic respawn | done | rules/common_quake.cpp (quake_dead_frame) | - |
| G18 | Dead body slowing | a dead body on the ground loses 20 u/s each command | done | rules/common_quake.cpp (quake_dead_frame); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G19 | Corpse queue | the last four bodies stay in the map, posed as they lay, taking no damage; a gibbed player leaves the head | done | rules/common_quake.cpp (quake_corpse) | - |
| G20 | Backpack drop | dropped on death by damage when any ammo is carried; holds the four ammo counts and the weapon in hand; removed after 120 s | done | rules/common_items.cpp (it leaves out the axe and shotgun, which no taker lacks) | - |
| G21 | Backpack throw | from 24 u below the origin at 300 u/s up and -100 to +100 u/s each way across | done | rules/common_items.cpp (backpack_drop) | - |
| G22 | No pack on `kill` or disconnect | neither goes through death, so no backpack | done | rules/common_quake.cpp (quake_kill) | - |
| G23 | Falling | landing faster than 300 u/s down: `player/land.wav`; faster than 650: 5 damage and `player/land2.wav`; in water `player/h2ojump.wav` and no damage | done | rules/common_quake.cpp (quake_land) | - |
| G24 | Jump sound | `player/plyrjmp8.wav` on the body on each jump from the ground | done | rules/arena_glue.cpp | - |
| G25 | Swim sound | `misc/water1.wav` or `water2.wav` at most once a second while jump is held at level 2 or more | done | rules/arena_glue.cpp, rules/common_player.cpp | - |
| G26 | Liquid entry and exit sounds | `player/inh2o.wav`, `inlava.wav` or `slimbrn2.wav` going in, `misc/outwater.wav` coming out; none dead or noclipping | done | rules/common_player.cpp (player_water) | - |
| G27 | Lava and slime damage | lava 10, 20, 30 by level every 0.2 s (1 s with the biosuit), slime 4, 8, 12 every 1 s (none with the biosuit) | done | rules/common_player.cpp over plugins/shooter/liquid_harm.hpp | - |
| G28 | Liquid clock restart | entering restarts the damage clock, so a second tick follows the first at once | done | rules/common_player.cpp | - |
| G29 | Drowning | 12 s of air; then once a second 4, 6, 8, 10, 12, 14, then 10, 12, 14 again; resets when the eyes come up | done | rules/common_player.cpp, plugins/shooter/liquid_harm.hpp | - |
| G30 | Gasps | `player/gasp2.wav` after air ran out, `gasp1.wav` after more than 3 s under | done | rules/common_player.cpp | - |
| G31 | Burn pain sounds | every damage in slime or lava plays `player/lburn1.wav` or `lburn2.wav`, not held by the pain clock | done | rules/common_quake.cpp (quake_pain) | - |
| G32 | Drown pain sounds | any damage under water plays `player/drown1.wav` or `drown2.wav` with one bubble | done | rules/common_quake.cpp (quake_pain) | - |
| G33 | Ordinary pain sounds | `player/pain1.wav` to `pain6.wav` (ends half as likely), `player/axhit1.wav` when hit by an axe, at most every 0.5 s; none while firing or under the Ring | done | rules/common_quake.cpp (quake_pain) | - |
| G34 | Death sounds | `player/death1.wav` to `death5.wav` (ends half as likely), map-wide; under water `player/h2odeath.wav` and five bubbles | done | rules/common_quake.cpp (quake_die) | - |
| G35 | Gib sounds | `player/gib.wav` or `player/udeath.wav`, map-wide; `player/teledth1.wav` for a telefrag | done | rules/common_quake.cpp (quake_die) | - |

### 2. Obituaries and frags

All at medium level. V the victim, K the killer.

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G36 | "V electrocutes himself." | the deathmatch 4 and 5 discharge that kills only the firer; V -1 | done | rules/arena_glue.cpp (death_words), profiles/words.hpp `self_electrocute` | 3 |
| G37 | "V was telefragged by K" | K +1 | done | rules/arena_glue.cpp, profiles/words.hpp | - |
| G38 | "Satan's power deflects V's telefrag" | the one teleporting onto a pentagram holder dies; V -1 | done | rules/common_combat.cpp (telefrag, score_death); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G39 | "V was telefragged by K's Satan's power" | both invulnerable: both lose it and die, each -1 | done | rules/common_combat.cpp (telefrag, score_death); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G40 | "K squished a teammate" | a door opened by a teammate crushes; K -1 | done | rules/common_map.cpp (door_blocked credits the opener); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G41 | "K squishes V" | a door opened by a player crushes another; K +1 | done | rules/common_map.cpp (door_blocked credits the opener); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G42 | "V was squished" | a plat, train or secret door crushes; V -1 | done | rules/common_map.cpp, rules/arena_glue.cpp | - |
| G43 | "V tries to put the pin back in" | own grenade; V -1 | done | rules/arena_glue.cpp | - |
| G44 | "V becomes bored with life" | own rocket, and any other own death | not-taken | match mod: ours draws "discovers blast radius" or "becomes bored with life" for a rocket, "somehow becomes bored with life" otherwise | - |
| G45 | "V discharges into the slime" / "into the lava" | own discharge in that liquid | done | rules/arena_glue.cpp | - |
| G46 | "V discharges into the water." | own discharge in water | not-taken | match mod: ours draws it or "heats up the water", without the full stop | - |
| G47 | Teamplay 2 teamkill lines | "K mows down a teammate", "K checks his glasses", "K gets a frag for the other team", "K loses another friend" (1/4 each); K -1 | done | rules/arena_glue.cpp, profiles/words.hpp (his or her by the `gender` key); reachable only once G157 lands | - |
| G48 | "V was nailed by K" | nail | not-taken | match mod: ours draws it or "was body pierced by" | - |
| G49 | "V was punctured by K" | super nail | not-taken | match mod: ours draws among punctured, perforated, ventilated, and "straw-cuttered" on a gib | - |
| G50 | "V eats K's pineapple" / "V was gibbed by K's grenade" | grenade, by health below -40 | done | rules/arena_glue.cpp | - |
| G51 | Quad rocket lines | "brutalized by K's quad rocket" 30%, "smeared by K's quad rocket" 30%, "K rips V a new one" 40% | done | rules/arena_glue.cpp (death_words: 30%, 30%, 40% under Quake's rules) | 3 |
| G52 | "V rides K's rocket" / "V was gibbed by K's rocket" | rocket, by health below -40 | done | rules/arena_glue.cpp | - |
| G53 | "V was ax-murdered by K" | killer holding the axe | done | rules/arena_glue.cpp | - |
| G54 | "V chewed on K's boomstick" | killer holding the shotgun | done | rules/arena_glue.cpp (a gib says "lead poisoned", match mod) | - |
| G55 | "V ate 2 loads of K's buckshot" | killer holding the super shotgun | done | rules/arena_glue.cpp (with quad "8 loads", match mod) | - |
| G56 | "V accepts K's shaft" | killer holding the lightning gun | done | rules/arena_glue.cpp (a gib says "natural disaster", match mod) | - |
| G57 | "V accepts K's discharge" | killer holding the lightning gun at level 2 or more | not-taken | match mod: ours draws it or "drains K's batteries" | - |
| G58 | Liquid world deaths by where the body lies | any world death in a liquid gets the liquid's line, whatever the cause | done | rules/arena_glue.cpp (death_words) | - |
| G59 | "V sleeps with the fishes" / "V sucks it down" | water | done | rules/arena_glue.cpp | - |
| G60 | "V gulped a load of slime" / "V can't exist on slime alone" | slime | done | rules/arena_glue.cpp | - |
| G61 | "V burst into flames" / "V turned into hot slag" / "V visits the Volcano God" | lava, the first below -15 health | done | rules/arena_glue.cpp | - |
| G62 | "V blew up" | an exploding box | done | rules/arena_glue.cpp, rules/common_set_pieces.cpp | - |
| G63 | "V fell to his death" | falling | not-taken | match mod: ours draws it or "cratered" (falling itself is G23) | - |
| G64 | "V was spiked" | a nail shooter | done | rules/arena_glue.cpp | - |
| G65 | "V was zapped" | a laser shooter | done | rules/arena_glue.cpp | - |
| G66 | "V ate a lavaball" | a fireball | done | rules/arena_glue.cpp, rules/common_set_pieces.cpp | - |
| G67 | "V tried to leave" | an exit that kills under `samelevel` 2 or 3 | done | rules/arena_world.cpp (changelevel_touch) | - |
| G68 | "V died" | any other world death | done | rules/arena_glue.cpp | - |
| G69 | "V suicides" | the `kill` command; V -2 | done | rules/common_quake.cpp (quake_kill) | - |
| G70 | Frag log | every frag change writes a line to the server's frag log | done | rules/arena_glue.cpp | - |

### 3. Join, leave, spectators

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G71 | "P entered the game" | high level, on connect | done | rules/common_player.cpp | - |
| G72 | "P left the game with N frags" | high level, with `player/tornoff2.wav` map-wide on the body | done | rules/common_player.cpp | - |
| G73 | "Spectator P entered the game" | medium level | done | games/shooters_edge/rules/arena_world.cpp (SpectatorConnect under `rules q1`) |  |
| G74 | "Spectator P left the game" | medium level | done | games/shooters_edge/rules/arena_world.cpp (SpectatorDisconnect under `rules q1`) |  |
| G75 | Spectator impulse 1 | jumps to the next deathmatch spot in turn, facing its angles | done | rules/arena_world.cpp (SpectatorThink); tests/shooters_edge_arena_quake_test.cpp | 3 |

### 4. Items

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G76 | Items placed | raised 6 u and dropped to the floor; removed if they fall out | done | rules/common_items.cpp (at spawn, not 0.2 s later) | - |
| G77 | Item respawn | back with `items/itembk2.wav` at the item | done | rules/common_items.cpp | - |
| G78 | Only live players take items | | done | rules/common_items.cpp | - |
| G79 | Health box | 25 (spawnflag 1: 15), refused at 100 or more, capped at 100; "You receive 25 health"; `items/health1.wav` or `items/r_item1.wav` | done | rules/common_items.cpp | - |
| G80 | Megahealth | +100 up to 250, refused at 250; after 5 s -1 a second above 100; back 20 s after the rot ends; `items/r_item2.wav` | done | rules/common_items.cpp | - |
| G81 | Health and armour respawn | 20 s in deathmatch 0, 1, 3, 4, 5; never in 2 | done | rules/common_quake.cpp (quake_item_respawn) | - |
| G82 | Armour values | green 0.3 and 100, yellow 0.6 and 150, red 0.8 and 200 | done | profiles/arsenal.hpp, rules/common_items.cpp | - |
| G83 | Armour pick-up rule | taken only when absorption times value is below the new one's; replaces outright | done | rules/common_items.cpp | - |
| G84 | Armour absorption | saved = ceil(absorption times damage), capped by the value (then lost); health loses the rest rounded up | done | rules/common_combat.cpp | - |
| G85 | "You got armor" | armour pickup line, `items/armor1.wav` | not-taken | match mod: profiles/items.hpp names the colours, so ours says "You got the Green Armor" | - |
| G86 | Ammo caps and boxes | caps 100/200/100/100; shells 20 (40), nails 25 (50), rockets 5 (10), cells 6 (12); refused at the cap | done | rules/common_items.cpp | - |
| G87 | Ammo respawn | 30 s (deathmatch 3 and 5: 15 s; 2: never) | done | rules/common_quake.cpp (quake_item_respawn) | - |
| G88 | Ammo pickup lines | "You got the shells" / "nails" / "rockets" / "cells", `weapons/lock4.wav` | done | rules/common_items.cpp, profiles/items.hpp | - |
| G89 | Switch after an ammo pickup | holding the best weapon before, the player switches to the new best | done | rules/common_items.cpp (ammo_touch); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G90 | Weapon pickups | super shotgun 5 shells, nailguns 30 nails, launchers 5 rockets, lightning gun 15 cells; `weapons/pkup.wav` | done | rules/common_items.cpp, profiles/arsenal.hpp | - |
| G91 | Weapon pickup lines | "You got the Double-barrelled Shotgun" / "nailgun" / "Super Nailgun" / "Grenade Launcher" / "Rocket Launcher" / "Thunderbolt" | done | profiles/items.hpp | - |
| G92 | Weapon respawn | 30 s in deathmatch 0 and 1 | done | rules/common_items.cpp | - |
| G93 | Weapon stay | deathmatch 2, 3 and 5: a weapon stays; one already owned cannot be touched | done | rules/arena_glue.cpp (weapon_may_take, weapon_stays) | - |
| G94 | `item_weapon` | an old ammo box picked by spawnflags | missing | no such class in the arena; no stock DM map is known to carry it | 3 |
| G95 | Powerup durations | all four 30 s | done | rules/common_items.cpp | - |
| G96 | Quad and biosuit respawn | 60 s | done | rules/common_items.cpp | - |
| G97 | Pentagram and Ring respawn | 300 s | done | rules/common_quake.cpp (quake_item_respawn) | - |
| G98 | Powerup pickup lines and sounds | "You got the Quad Damage" / "Pentagram of Protection" / "Ring of Shadows" / "Biosuit"; `items/damage.wav`, `protect.wav`, `inv1.wav`, `suit.wav` | done | rules/common_items.cpp, profiles/items.hpp | - |
| G99 | Powerup warnings | 3 s before the end: "Quad Damage is wearing off", "Protection is almost burned out", "Ring of Shadows magic is fading", "Air supply in Biosuit expiring" with `items/damage2.wav`, `protect2.wav`, `inv2.wav`, `suit2.wav` | done | rules/common_items.cpp, profiles/words.hpp | - |
| G100 | Warning flashes | a screen flash every second until the power ends | done | rules/common_quake.cpp (quake_power_frame) | - |
| G101 | Quad | damage times 4 for every hit the holder deals, own included, not a door's crush; blue glow | done | rules/common_combat.cpp, rules/common_items.cpp | - |
| G102 | Quad firing sound | `items/damage3.wav` on the body at most once a second | done | rules/common_quake.cpp (quake_quad_heard) | - |
| G103 | Pentagram order | knockback and armour wear still happen; only health is spared; red glow | done | rules/common_combat.cpp (quake_damage) | - |
| G104 | Pentagram hit sound | `items/protect3.wav` at most every 2 s | done | rules/common_combat.cpp (quake_damage) | - |
| G105 | Ring | the player is the eyes model, frame 0; no pain frames or sounds | done | rules/common_items.cpp (the pain half with G33) | - |
| G106 | Ring breath | `items/inv3.wav` at half volume, idle attenuation, every 1 to 4 s | done | rules/common_quake.cpp (quake_power_frame) | - |
| G107 | Biosuit | air refreshed every command, no slime damage, lava once a second | done | rules/common_items.cpp, rules/common_player.cpp | - |
| G108 | Quad and Ring drop (`dq`, `dr`) | with the server or local info key set, a dead holder drops it with the time left; it vanishes when the time runs out | done | rules/arena_glue.cpp (drop_on_death reads `dq` and `dr`); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G109 | Drop and recover lines | "V lost a quad with N seconds remaining", "V lost a ring with N seconds remaining", "P recovered a Quad with N seconds remaining!", "P recovered a Ring with N seconds remaining!" (deathmatch 4: OctaPower forms) | done | rules/common_items.cpp (power_drop, dropped_power_touch); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G110 | Backpack pickup line | "You get the W, N shells, N nails, N rockets, N cells", `weapons/lock4.wav` | done | rules/common_items.cpp | - |
| G111 | Backpack rocket floor | deathmatch 3 and 5: a pack with a launcher raises the taker to 5 rockets | done | rules/common_items.cpp (backpack_touch) | - |
| G112 | Pickup flash | every pickup flashes the taker's screen | done | rules/common_items.cpp | - |

### 5. Weapons

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G113 | Axe | 20 damage (deathmatch 4 and 5: 75), 64 u, 0.5 s, `weapons/ax1.wav` at the press, `player/axhit2.wav` and a puff on a wall | done | rules/common_weapons.cpp, profiles/arsenal.hpp (75 in mode 5 with G155) | - |
| G114 | Axe hit timing | the hit lands 0.2 s after the press, traced from the view at that moment | done | rules/common_weapons.cpp (quake_blade) | - |
| G115 | Shotgun | 1 shell, 0.5 s, 6 pellets of 4, spread 0.04 each way, 2048 u, `weapons/guncock.wav` | done | profiles/arsenal.hpp, rules/common_weapons.cpp | - |
| G116 | Super shotgun spread | 14 pellets of 4, 0.7 s; spread 0.14 across and 0.08 up | done | rules/common_weapons.cpp (quake_pellets) | - |
| G117 | Super shotgun with one shell | fires a normal shotgun blast | done | rules/common_weapons.cpp (quake_weapon_frame) | - |
| G118 | Pellet summing | pellets hitting the same target in a row are one hit, armour rounding once | done | rules/common_weapons.cpp (quake_pellets) | - |
| G119 | Nailgun | 1 nail every 0.1 s while held, 9 damage, 1000 u/s, alternately 4 u right and left, `weapons/rocket1i.wav`, gone after 6 s or silently in the sky | done | rules/common_weapons.cpp (quake_weapon_frame) | - |
| G120 | Super nailgun | 2 nails a shot, 18 damage, `weapons/spike2.wav` | done | profiles/arsenal.hpp | - |
| G121 | Super nailgun with one nail | fires one normal nail | done | rules/common_weapons.cpp (quake_weapon_frame) | - |
| G122 | Grenade launcher | 1 rocket, 0.6 s, 600 along the view plus 200 up plus up to 10 at random sideways and up, spinning, fuse 2.5 s, bursts on a player, else `weapons/bounce.wav`; splash 120 | done | profiles/arsenal.hpp, rules/common_weapons.cpp (no random scatter) | - |
| G123 | Rocket launcher | 1 rocket, 0.8 s, 1000 u/s, 5 s life, never hits its owner, silent in the sky, `weapons/sgun1.wav`; splash 120 sparing the thing hit; blast drawn 8 u back | done | profiles/arsenal.hpp, rules/common_weapons.cpp | - |
| G124 | Rocket direct hit | 100 + 20 times random (100 to 120) | done | rules/common_weapons.cpp (rocket_touch) | - |
| G125 | Explosion sound | the program starts none; the client plays `weapons/r_exp3.wav` with the explosion effect | done | rules/common_set_pieces.cpp (no second sound under Quake's rules) | 3 |
| G126 | Splash | damage less half the distance to the box centre, within damage + 40 u, halved for the attacker | done | rules/common_combat.cpp | - |
| G127 | Splash line of sight | the blast must reach the origin or one of four points 15 u off it, past players | done | rules/common_quake.cpp (quake_sees), rules/common_combat.cpp | - |
| G128 | Lightning gun beam | 30 a hit, 600 u, a cell every 0.1 s, beam from 16 u above the origin, three damage lines from the origin, no entity hit twice | done | rules/common_weapons.cpp (one line from the eye; the side lines are near-useless, see qw-dm-rules.md round 2, 17) | - |
| G129 | Lightning gun sounds | `weapons/lstart.wav` on the auto channel at the press, `weapons/lhit.wav` at most every 0.6 s | done | rules/common_weapons.cpp (quake_weapon_frame) | - |
| G130 | Discharge | at level 2 or more all cells go as splash of 35 per cell from the firer, the firer half | done | rules/common_weapons.cpp | - |
| G131 | Discharge self-kill chance | deathmatch 4 and 5: half the time the firer alone takes 4000 | done | rules/common_weapons.cpp (mode 4; mode 5 with G155) | - |
| G132 | Knockback | velocity gains the unit vector from the inflictor's centre times damage times 8, before armour | done | rules/common_combat.cpp | - |
| G133 | Rocket jump key `rj` | own damage gets an extra push of damage times `rj` when above 1; read at every spawn | done | rules/common_quake.cpp (quake_read), rules/common_combat.cpp | - |
| G134 | Out of ammo | each command past the fire time, an empty weapon (not the axe) switches to the best; pressing fire empty switches | done | rules/common_weapons.cpp | - |
| G135 | Best weapon order | lightning gun (1+ cell, level 0 or 1), super nailgun (2+), super shotgun (2+), nailgun, shotgun, axe; never a launcher | done | rules/common_quake.cpp (quake_best_weapon) | - |
| G136 | Pickup switch `w_switch` | a new weapon is taken in hand when its number is within the user info key `w_switch` (0 or missing: 8) and it ranks above the held one (LG, RL, SNG, GL, SSG, NG, rest); never the LG in liquid | done | rules/common_quake.cpp (quake_takes_in_hand), rules/common_items.cpp | - |
| G137 | Backpack switch `b_switch` | the same for a pack's weapon with the key `b_switch` | done | rules/common_quake.cpp (quake_takes_in_hand), rules/common_items.cpp | - |
| G138 | Impulses 1 to 8 | select that weapon; "no weapon." or "not enough ammo." to the player | done | rules/common_weapons.cpp (the lines lack the full stop) | - |
| G139 | Impulse 10 | next owned weapon with enough ammo, wrapping | done | rules/common_weapons.cpp | - |
| G140 | Impulse 12 | the same backwards | done | rules/common_weapons.cpp (weapon_impulse) | - |
| G141 | Impulse 9 | the all-weapons cheat, refused in every mode | done | rules/common_weapons.cpp ignores it | - |
| G142 | Impulse 11 | doubles the episode flags and adds one | not-taken | a development leftover with no use in a deathmatch | - |
| G143 | Impulses wait for the fire time | | done | rules/common_player.cpp | - |

### 6. Modes and settings

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G144 | `deathmatch`, `teamplay`, `fraglimit`, `timelimit` | read as cvars every server frame | done | rules/arena_world.cpp | - |
| G145 | deathmatch 2 | weapons stay; health, megahealth, armour and ammo never return; powerups do | done | rules/common_quake.cpp (quake_respawn) | - |
| G146 | deathmatch 3 | weapons stay; ammo back in 15 s; the pack rocket floor | done | rules/common_quake.cpp (quake_respawn), rules/common_items.cpp | - |
| G147 | deathmatch 4 spawn kit | 250 health, red 200, the pentagram for 3 s, 255 of every ammo, every weapon but the grenade launcher; the `axe` key leaves axe and shotgun only | not-taken | match mod: rules/common_dmm4.cpp gives every weapon, a 2 s shield by `dmm4_invinc_time` and `k_disallow_weapons` | - |
| G148 | deathmatch 4 map and ammo | no weapons or ammo on the map, no ammo used, no megahealth | done | rules/common_items.cpp, rules/common_weapons.cpp | - |
| G149 | deathmatch 4 grenades | fire time 1.1 s and 10 damage to the firer each shot | done | rules/common_weapons.cpp (quake_weapon_frame) | - |
| G150 | deathmatch 4 OctaPower | quad times 8; taking it zeroes armour and cells; at its end 255 cells, armour 1 at 0.8, health 100 | done | rules/common_dmm4.cpp, rules/common_combat.cpp | - |
| G151 | "OctaPower is wearing off" | the deathmatch 4 warning | done | rules/common_items.cpp (power_frame), profiles/words.hpp `power_octa_off`; tests/shooters_edge_arena_quake_test.cpp | 3 |
| G152 | "You got the OctaPower" | | done | rules/common_items.cpp | - |
| G153 | deathmatch 4 packs | +10 health, "You get 10 additional health"; 300 or more grants pentagram and quad for 30 s, zeroes cells, `boss1/sight1.wav`, "P attains bonus powers!!!" | not-taken | match mod: rules/common_dmm4.cpp says "gained bonus powers!!!" with `items/protect.wav` | - |
| G154 | deathmatch 4 healing under the pentagram | no health, armour or pack while any pentagram holds | done | rules/common_items.cpp | - |
| G155 | deathmatch 5 | 200 health, red 200, the pentagram 3 s, every weapon, 30/80/10/30 ammo; weapon stay; ammo 15 s; axe 75; the LG self-kill chance | done | rules/arena_glue.cpp (give_inventory), rules/common_weapons.cpp | - |
| G156 | teamplay 1 | no damage between teammates and none to oneself with a team; push and armour wear stay | done | rules/common_combat.cpp (quake_damage) | - |
| G157 | teamplay 2 | teammates can be hurt; a teamkill costs 1 frag | done | rules/common_combat.cpp (quake_damage) | - |
| G158 | teamplay 3 | no damage between teammates, self-damage allowed | done | rules/common_combat.cpp (quake_damage) | - |
| G159 | Team from the user info key `team` | an empty team is never a teammate | done | rules/arena_glue.cpp | - |
| G160 | A crush ignores teamplay | a door, plat or train hurts teammates too | done | rules/common_combat.cpp (quake_damage) | - |
| G161 | Gravity at map load | `sv_gravity` set to 800, 100 on e1m8 | done | rules/arena_world.cpp (worldspawn) | - |
| G162 | `temp1` | forces the death animation | not-taken | a developer's switch | - |
| G163 | `registered` | the `start` map's episode choice and `trigger_onlyregistered` | not-taken | the arena has neither; registration is the pack's own (plugins/classicenhance) | - |
| G164 | Match clock prints | the stock program prints none | not-taken | match mod: rules/arena_world.cpp prints the minutes left in the Quake words | - |

### 7. The end of a map

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G165 | Fraglimit and timelimit | a player's frags at the limit, or the map clock past minutes times 60, ends the map | done | rules/arena_world.cpp | - |
| G166 | Next map | the first `trigger_changelevel`'s map, else the same map | done | rules/arena_world.cpp (quake_next_map) | - |
| G167 | Rotation key | a server or local info key named after the current map gives the next map | done | rules/arena_world.cpp (quake_next_map) | - |
| G168 | `samelevel` 1 | the same map again | done | rules/arena_world.cpp (quake_next_map) | - |
| G169 | Intermission camera | 0.1 s after the end: CD track 3, then the intermission message with a random `info_intermission` origin and angles (else `info_player_start`) | done | rules/arena_world.cpp (quake_intermission) | - |
| G170 | Players at the intermission | every player invisible, untouchable, unhurtable and frozen | done | rules/arena_world.cpp (quake_intermission) | - |
| G171 | Leaving the intermission | after 5 s the first press of attack, jump or use sends everyone on; no timeout; a connect moves everyone on at once | done | rules/arena_world.cpp (quake_intermission_frame); a connect during it does not move everyone on | - |
| G172 | Dead at the end | the dead stay down; they can press to move on; the default load-out next map | done | rules/arena_world.cpp (no respawn after the end) | - |
| G173 | `trigger_changelevel` in deathmatch | "P exited the level" and the map ends for everyone to its map | done | rules/arena_world.cpp (changelevel_touch) | - |
| G174 | `samelevel` 2 and 3 | any exit kills the toucher instead (3: except on `start`) | done | rules/arena_world.cpp (changelevel_touch) | - |
| G175 | `kill` command | refused to the dead ("Can't suicide -- allready dead!"), else "V suicides", -2, respawn at once with the default kit | done | rules/common_quake.cpp (quake_kill) | - |
| G176 | `god`, `give`, `noclip` | server commands refused without `-cheats` | not-taken | the server's, in the SERVER section | - |

### 8. Map entities the program spawns

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G177 | `info_player_deathmatch` | a spawn spot | done | rules/arena_world.cpp | - |
| G178 | `info_player_start` | the intermission fallback | done | rules/arena_world.cpp | - |
| G179 | `info_player_coop`, `info_player_start2` | kept, unused | done | ours has no spawn for them, so they are freed; nothing differs | - |
| G180 | `info_intermission` | an intermission camera | done | rules/arena_world.cpp (quake_intermission_spot) | - |
| G181 | `info_teleport_destination` | the destination, 27 u up | done | rules/common_map.cpp | - |
| G182 | `info_null`, `info_notnull` | removed, kept | done | freed by the server; nothing targets them in deathmatch | - |
| G183 | `path_corner` | a train's corner | done | rules/arena_world.cpp | - |
| G184 | `item_health` | G79, G80 | done | rules/common_items.cpp | - |
| G185 | `item_armor1`, `item_armor2`, `item_armorInv` | G82 | done | rules/common_items.cpp | - |
| G186 | `item_shells`, `item_spikes`, `item_rockets`, `item_cells` | G86 | done | rules/common_items.cpp | - |
| G187 | The six `weapon_*` pickups | G90 | done | rules/common_items.cpp | - |
| G188 | The four `item_artifact_*` | G95 to G107 | done | rules/common_items.cpp | - |
| G189 | `item_key1`, `item_key2` | keys by world type (silver and gold key, runekey, keycard; `misc/medkey.wav`, `runekey.wav`, `basekey.wav`), never respawn, refused when held, lost at death and map change, taken by the door | done | rules/common_quake.cpp (quake_key_start, quake_key_touch) | - |
| G190 | Key doors | spawnflag 16 silver, 8 gold; "You need the silver key" (runekey, keycard) with `doors/medtry.wav`, `runetry.wav` or `basetry.wav` every 2 s; opening plays the use sound and the door stays open | done | rules/common_map.cpp (door_trigger_touch, func_door) | - |
| G191 | `item_sigil` | a rune: "You got the rune!", `misc/runekey.wav`, adds to the episode flags, fires targets | done | rules/arena_world.cpp (item_sigil under Quake's rules) | 3 |
| G192 | `func_door` | speed 100, wait 3, lip 8, dmg 2, sounds 0 to 4, linked doors, trigger field, shot open, message every 2 s, toggle, start open | done | rules/common_map.cpp | - |
| G193 | `func_door` crush credit | the opener is the killer (G40, G41) | done | rules/common_map.cpp (door_go_up keeps the opener, door_blocked credits it) | 3 |
| G194 | `func_door_secret` | the two-leg secret door with its sounds 1 to 3, shootable when unnamed, 1 s pauses, `wait` 5 | done | rules/common_set_pieces.cpp | - |
| G195 | `func_plat` | speed 150, sounds 1 or 2, travel by height, the inset field, 3 s wait renewed while stood on, 1 damage on a crush | done | rules/common_map.cpp | - |
| G196 | `func_train` | speed 100, dmg 2, sounds 1, waits at corners, crush at most twice a second | done | rules/common_map.cpp | - |
| G197 | `func_button` | speed 40, wait 1, lip 4, sounds 0 to 3, shot when it has health, players only by touch | done | rules/common_map.cpp | - |
| G198 | `func_wall`, `func_illusionary` | a solid brush (use toggles its frame); a non-solid one | done | rules/common_map.cpp | - |
| G199 | `func_episodegate`, `func_bossgate` | a wall shown by the episode flags | done | rules/common_map.cpp (shared with the campaign) | 3 |
| G200 | `trigger_multiple`, `trigger_once` | players only, wait 0.2 s, once removed 0.1 s after | done | rules/common_map.cpp | - |
| G201 | Trigger `angle` | fires only for a player facing within 90 degrees of it | done | rules/common_map.cpp (trigger_touch); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G202 | Trigger `sounds` | 1 `misc/secret.wav`, 2 `misc/talk.wav`, 3 `misc/trigger1.wav` when it fires | done | rules/common_map.cpp (trigger_start, trigger_fire) | - |
| G203 | Trigger `message` | centre-printed to the player with `misc/talk.wav` when it has no `sounds` | done | rules/common_util.cpp (use_targets) | - |
| G204 | Trigger `killtarget` | removes its targets and then fires none | done | rules/common_util.cpp (use_targets) | - |
| G205 | Shootable triggers (`health`) | solid, fired by the killing blow, restored after `wait` | done | rules/common_map.cpp (trigger_shootable, trigger_killed, trigger_ready); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G206 | `trigger_secret` | "You found a secret area!", `misc/secret.wav`, the secret counter | done | rules/arena_world.cpp (trigger_secret) | - |
| G207 | `trigger_relay` | fires its targets when used, with delay, message and killtarget | done | rules/arena_world.cpp (trigger_relay) | - |
| G208 | `trigger_counter` | count 2; "There are more to go...", "Only 3 more to go...", "Only 2 more to go...", "Only 1 more to go...", then "Sequence completed!" | done | rules/arena_world.cpp (trigger_counter) | - |
| G209 | `trigger_teleport` | flash and a random `misc/r_tele` sound at both ends, arrival at 300 u/s held 0.7 s, telefrag at the destination | done | rules/common_map.cpp (teleport_touch) | - |
| G210 | Teleporter hum and flags | `ambience/hum1.wav` unless spawnflag 2; spawnflag 1 players only; a named one works 0.2 s after use | done | rules/common_map.cpp (trigger_teleport): the hum; a named one's 0.2 s window is still missing | - |
| G211 | `trigger_push` | velocity set to 10 times `speed` (1000) along its angle, for live things and grenades | done | rules/common_map.cpp (grenades are not pushed) | - |
| G212 | Push extras | spawnflag 1 pushes once; `ambience/windfly.wav` on the auto channel every 1.5 s | done | rules/common_map.cpp (push_touch) | - |
| G213 | `trigger_hurt` | 5 damage at most once a second | done | rules/common_map.cpp | - |
| G214 | `trigger_onlyregistered` | fires once for a registered game, else its message every 2 s | done | rules/common_map.cpp (shared with the campaign) | 3 |
| G215 | `trigger_setskill`, `trigger_monsterjump` | removed; affects only monsters | done | freed by the server; no monsters in deathmatch | - |
| G216 | `misc_explobox`, `misc_explobox2` | 20 health, a 160 blast within 200 u, drawn 32 u up, "V blew up" | done | rules/common_set_pieces.cpp | - |
| G217 | `misc_fireball` | lava balls every 3 to 8 s, 20 damage, "V ate a lavaball" | done | rules/common_set_pieces.cpp | - |
| G218 | `trap_spikeshooter`, `trap_shooter` | nails at 500 u/s (9, super 18), `weapons/spike2.wav`; laser at 600 u/s for 15 with `enforcer/enfire.wav` and `enfstop.wav` | done | rules/common_set_pieces.cpp | - |
| G219 | `misc_teleporttrain` | a spinning non-solid `progs/teleport.mdl` riding its path | done | rules/common_map.cpp (shared with the campaign) | 3 |
| G220 | `misc_noisemaker` | every 0.5 s a burst of `enforcer` sounds | not-taken | a test entity; no stock map uses it | - |
| G221 | `viewthing`, `noclass` | a model viewer; a warning and removal | not-taken | developer-only | - |
| G222 | `air_bubbles`, `event_lightning` | removed in deathmatch | done | freed by the server; rules/common_set_pieces.cpp | - |
| G223 | The nine `ambient_*` sounds | suck_wind, drone, flouro_buzz, drip, comp_hum, thunder, light_buzz, swamp1, swamp2 as fixed loops | done | rules/common_set_pieces.cpp | - |
| G224 | `light`, `light_fluoro` switching | a named light with style 32 or more toggles on use; spawnflag 1 starts off | done | rules/common_map.cpp (light, light_fluoro, light_use); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G225 | `light_fluoro`, `light_fluorospark` hums | `ambience/fl_hum1.wav` and `buzz1.wav` loops | done | rules/common_map.cpp (light_fluoro, light_fluorospark); tests/shooters_edge_arena_quake_test.cpp | 3 |
| G226 | Flames and globe | `progs/flame.mdl`, `flame2.mdl` frames 0 and 1, `s_light.spr`, static | done | rules/common_map.cpp | - |
| G227 | Flame crackle | `ambience/fire1.wav` loop at each flame | done | rules/common_map.cpp (flame_start) | - |
| G228 | `monster_*` in deathmatch | the fifteen classes removed as they spawn | done | the arena has no spawn for them, so the server frees them | - |
| G229 | Spawnflag 2048 | not in deathmatch: always removed | done | the server's load rule (SERVER section) | - |

### 9. Quake's monsters (the campaign, on the pack's progs.dat)

Each runs from the pack's own program on our VM (games/shooters_edge/client/game_folder.cpp).
The note porting/shooters-edge/q1-sp-gaps.md names no missing builtin or physics for them, and the
se-coop-sweep lab plays every map with no program fault.

| id | item | what it does | status | ours (file) or why not | rank |
| --- | --- | --- | --- | --- | --- |
| G230 | `monster_army` | the grunt | done | games/shooters_edge/client/game_folder.cpp | - |
| G231 | `monster_dog` | the rottweiler | done | as G230 | - |
| G232 | `monster_ogre` | the ogre | done | as G230 | - |
| G233 | `monster_ogre_marksman` | the ogre placed for higher skill | done | as G230 | - |
| G234 | `monster_knight` | the knight | done | as G230 | - |
| G235 | `monster_hell_knight` | the death knight | done | as G230 | - |
| G236 | `monster_wizard` | the scrag | done | as G230 | - |
| G237 | `monster_demon1` | the fiend | done | as G230 | - |
| G238 | `monster_shambler` | the shambler | done | as G230 | - |
| G239 | `monster_zombie` | the zombie | done | as G230 | - |
| G240 | `monster_fish` | the rotfish | done | as G230 | - |
| G241 | `monster_tarbaby` | the spawn | done | as G230 | - |
| G242 | `monster_shalrath` | the vore | done | as G230 | - |
| G243 | `monster_enforcer` | the enforcer | done | as G230 | - |
| G244 | `monster_boss` | Chthon | done | as G230 | - |
| G245 | `monster_oldone` | Shub-Niggurath | done | as G230 | - |

### Count

Rows 245: done 226, missing 1 (rank 3), not-taken 18.

## TOOLS: what building the stock sources with ours shows

| id | item | what it does | status | ours | rank |
| --- | --- | --- | --- | --- | --- |
| T1 | A call with fewer arguments than declared | the stock QuakeWorld game program's source has one; its compiler took it | done | plugins/progs/qc_expressions.cpp (too few arguments warn and build; too many still refused) | 3 |
