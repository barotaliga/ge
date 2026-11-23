# Client: the window, the adapter, controls, the console, the HUD, the menus

The player's side of a classic game: the window that loads one game at a time through its adapter,
the keys and mouse, the console and its layers, the HUD in the game's own pictures, the menus in
the game's own letters, and the text a player reads. Settings and commands are their own page
([`settings.md`](settings.md)).

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/platform/` | SDL3: the window, raw mouse, keys, gamepads, touch, the audio device |
| Engine | `engine/input/` | Actions, bindings, rebinding, devices (`docs/input.md`) |
| Engine | `engine/console/` (`console.hpp`, `cvar.hpp`, `binds.hpp`, `cfg.hpp`, `view.hpp`, `chat_ignore.hpp`, `talk.hpp`, `words.hpp`) | The console: cvars in LAYERS, the language, binds, cfg files and the startup order, the drop-down view, chat ignore lists, talk commands, a setting's words |
| Engine | `engine/hud/` (`hud.hpp`, `layout.hpp`, `crosshair.hpp`, `feed.hpp`, `corner_readouts.hpp`, `net_graph.hpp`, `frame_graph.hpp`, `pictures.hpp`) | The HUD kit: elements and their places as `hud_` cvars, the crosshair, the feed, corners, graphs (`docs/hud.md`) |
| Engine | `engine/ui/`, `engine/charset/` | The 2D layer, menus as a model and a view, bitmap fonts |
| Engine | `engine/camera/` | The first-person rig and its cvars (`cl_fov`, `cl_bob`, `v_kick`, `cl_landdip`) |
| Engine | `engine/capture/screenshot.hpp`, `gallery.hpp` | Screenshots and the gallery |
| Shooter kit | `plugins/shooter/controls.hpp`, `mouse_feel.hpp`, `demo_transport.hpp`, `quick_weapon.hpp`, `weapon_groups.hpp`, `weapon_lines.hpp` | Any shooter's keys and mouse, the mouse's feel, the demo transport, the quick weapon, weapon groups |
| Classic plugin | `plugins/classic/app/look.hpp` | The picture's settings with a game's defaults (`picture.md`) |
| Shell | `games/shooters_edge/client/game_adapter.hpp` | THE ADAPTER: one table of calls (mods, programs, maps, textures, models, pictures, sounds, configs, place names, menu, modes, held weapons, demos) the window, console, menus and labs ask a game's files through |
| Shell | `games/shooters_edge/client/cvar_layers.hpp` | The window's cvar layers: engine, shooter, classic, shell, `game:<id>` swapped on load; `GameBindings` binding each game's spellings into the window's structs |
| Shell | `games/shooters_edge/client/settings.hpp`, `controls.hpp`, `weapon_choice.hpp`, `team_chat.hpp`, `team_point.hpp` | The series config, user info, the player's keys as turn and command, weapon choice, team chat's codes |
| Shell | `games/shooters_edge/client/present.hpp`, `view.hpp`, `corners.hpp`, `scores.hpp`, `title.hpp` | The view frame by frame, the HUD's status, corners, the scoreboard, the title |
| Shell | `games/shooters_edge/hud/` (`layout.hpp`, `status.hpp`, `crosshair.hpp`, `classic_art.hpp`) | The HUDs: the classic status bar and other looks (`hud_style`), drawn from the game's own pictures |
| Shell | `games/shooters_edge/menu/` (`main_menu.hpp`, `game_menu.hpp`, `classic_view.hpp`, `classic_console.hpp`, `games_page.hpp`, `servers_page.hpp`, `controls_page.hpp`) | The main menu model, the classic pages, the console in the game's letters, SELECT GAME, the browser, controls |
| Shell | `games/shooters_edge/client/touch_play.hpp` | The game played by touch |
| Classic plugin | `plugins/classic/content/files.hpp` (`search_path_text`) | The console's `path` for every classic game: the folders and archives, the first searched at the top |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Adapter | `client/qw_adapter.cpp` | `client/q2_adapter.cpp` | a file of its own | a file of its own |
| Layer | `add_first` in `client/cvar_layers.cpp` | `add_q2` | `add_<game>` | `add_<game>` |
| Status bar | its pictures in fixed places | a layout program from the server (`hud/q2_layout.hpp`, `client/q2_bar.hpp`, `server/q2_bar.hpp`), its scale, switches and lag graph (`client/q2_view.hpp`) | its client half's HUD (bytecode or native) | its own bar |
| Menu letters | carved letters cut from its strips (`menu/carved_letters.hpp`) | plate letters cut as ink (`menu/plate_letters.hpp`) | its own font pictures | its own font lumps |
| Menu pages | `menu::classic_menu()` | its own pages (`menu/q2_pages.cpp`: its setup pages, key rows, deathmatch flags by bit, the in-game page ESCAPE opens in a level), with `q2pro.menu` read over them (`menu/menu_script.hpp`, `menu_script`) | its own (multiplayer only) | its own |
| Save slots' word | "--- UNUSED SLOT ---" (`kUnusedSlot`) | `kEmptySlot` (`menu::q2_menu()`) | its own | its own |
| Field of view | the player's own `fov` | the server's, drawn between frames (`client/q2_view.hpp`: `cl_adjustfov`, `uf`) | its own | its own |
| Plaques | the pack's pause and loading pictures (`menu::draw_plaque`, `showpause`) | its own | its own | its own |
| Map commands, game clock, console log | the map commands (every game's) | `client/q2_session.hpp` (`cl_beginmapcmd`, `timescale`, `cl_autopause`, the read-only state), `client/console_log.hpp` (`logfile`) | its own | its own |
| Own view from behind | `cam_thirdperson` over demos and watching | `cl_thirdperson` (a cheat), its angle and range, joined and hosted (`client/q2_session.hpp`'s `view_behind`) | its own | its own |
| Effect switches | `cl_gibfilter`, `cl_muzzleflash`, `r_rocketlight` ... | `classic/q2_fx_switches.hpp` (`cl_disable_particles`, `cl_disable_explosions`, `cl_dlight_hacks`, `cl_muzzlelight_time`, `cl_muzzleflashes`) | its own | its own |
| Footsteps and loops heard | the sounds as sent | `cl_footsteps` 0 and `s_ambient` (`client/demo_sound.hpp`'s `switches_from`) | its own | its own |
| Rail trail beams | the game's own specks | `cl_railtrail_*`, `cl_railcore_*`, `cl_railspiral_*` (`client/world_fx.cpp`'s `q2_rail_trail`) | its own | its own |
| Sound out of focus | always heard | `s_auto_focus` (`platform/window_cvars.hpp`, every game) | the engine's | the engine's |
| Music | `cd`, `bgmvolume` | `ogg_enable`, `ogg_volume`, `ogg_shuffle`, `ogg_menu_track` over the engine's music (`audio/music_console.hpp`) | its own | its own |
| Hit markers | none | `cl_hit_markers`, `scr_hit_marker_time` on temporary entity 128 (`client/q2_bar.cpp`'s `draw_q2_hit_marker`), sent by our server to a client that asks (`*ge_ext` bit 1024) | its own | its own |
| Download switches | the server's `sv_download_*` | `allow_download` and its kinds gate what joining fetches (`net/q2_downloads.hpp`), `cl_nodelta` | its own | its own |
| See-through weapon | none | `cl_gunalpha` (`view/hand.hpp`'s `alpha`) | its own | its own |
| Network trouble words | the turtle (`showturtle`) and the net picture in the view's corner (`client/trouble_icons.hpp`) | `scr_showturtle` (`client/q2_bar.cpp`'s `add_q2_turtle`) | its own | its own |
| The window's style | always a usual window | `win_noborder`, `win_notitle`, `win_alwaysontop`, `win_noresize` (`platform/window_cvars.hpp`) | the engine's | the engine's |
| The window's size and place | as the game opens it | `vid_geometry`, `WxH+X+Y` (`platform/window_cvars.hpp`) | the engine's | the engine's |
| Console scroll-back | stays where scrolled | `con_scroll` (`console/view.hpp`, every game) | the engine's | the engine's |
| Packets sent shown | `showpackets` | `cl_showpackets` over the channel's trace (`classic/protocol/channel.hpp`) | its own | its own |
| Held weapon's sway | ours (`cl_gunsway`) | the server's gun angles and place (`client/held.cpp`'s `q2_gun_sway` over `viewmodel/place.hpp`'s `game_turn`, `game_shift_m`), ours set aside | its own | its own |
| Configs on a game change | `fs_default`'s default.cfg, the game's own settings | the same, then `fs_autoexec`'s autoexecs and postexec.cfg (`client/install_configs.cpp`) | the game's own settings | the engine's |
| The connection's number | `qport` (from the name) | `qport` (from the name) | `qport`, 0 from the name (`net/qw_remote.cpp`) | the classic layer's |
| Floating items still | none | `cl_nobob` (the picture's `items_still`, `client/world_models.cpp`) | its own | its own |
| The client's own port | any free port | `net_clientport`, moved at once (`net/qw_remote.cpp`'s `own_port`) | its own | its own |
| The console's picture | the pack's `conback` | `con_background`, a picture under `pics/` (`menu/classic_console.hpp`) | its own | its own |
| The console's stamps | `con_timestamps` on chat lines (`client/chat_lines.hpp`) | its own `con_timestamps` on every line, `con_timestampsformat`, `con_timestampscolor` (`console/view.hpp`'s stamps) | its own | its own |
| Tab in the console | ours: the top suggestion | `com_completion_mode`, `com_completion_treshold`: the shared part, the matches listed (`console/view.hpp`'s `list_mode`) | its own | its own |
| The server browser's sort and pace | `sb_sortservers` (codes), `sb_infospersec` | `ui_sortservers` (one column either way), `ui_pingrate` (from `rate`) in their place (`menu/servers_page.hpp`'s `game_sort`, `game_ping_rate`) | its own | its own |
| The menus' size | the largest whole number that fits | `ui_scale` 1..10 that many times (`menu/classic_art.hpp`'s `scale`) | its own | its own |
| The sound device | `s_device`, `s_devices` lists them (`platform/window_cvars.hpp`, every game) | `al_device` its spelling | the engine's | the engine's |
| No sound | `s_enable` 0 (`platform/window_cvars.hpp`, every game) | the same | the engine's | the engine's |
| IPv6 servers | `connect [addr]:port` from an IPv6 socket (`net/qw_remote.cpp`) | `net_enable_ipv6` 0 refuses one, `net_ip6` the socket's address | the same join | the same join |
| Particles' size, shape, blend | each as made, a soft capsule, laid over | `gl_partscale` (grown with depth), `gl_partshape`, `gl_partstyle` (`render/particles.hpp`'s ParticleShape, `additive`) | as made | as made |
| Beams, sky, textures, fog | the look's own | `gl_beamstyle`, `gl_drawsky`, `gl_bilerp_skies`, `gl_saturation`, `gl_invert`, `gl_anisotropy`, `gl_fog` (`classic/q2_fx_switches.hpp`) | the look's own | the look's own |
| Under water | the view breathes (`r_waterwarp`) | also `gl_waterwarp`, the view rippled (`render/post.hpp`'s PostRipple) | `r_waterwarp` | `r_waterwarp` |
| The light on walls and models | `gl_modulate` (the walls) | `gl_modulate_world`, `gl_modulate_entities`, `gl_doublelight_entities`, `gl_dotshading`, `gl_coloredlightmaps`, `gl_dlight_falloff`, `cl_lerp_lightstyles` (`classic/q2_fx_switches.hpp`) | the classic's | the classic's |
| Ink lines round models | none | `gl_celshading` (`classic/q2_fx_switches.hpp`, drawn by `render/morph.hpp`'s MorphDraw::outline) | none | none |
| Explosions stepped by frames | none | `cl_smooth_explosions` 0 (`classic/q2_fx_switches.hpp`'s q2_blast_age) | none | none |
| Status bar pictures | sharp | `gl_bilerp_pics`, `gl_gamma_scale_pics` (`client/q2_bar.cpp`) | sharp | sharp |
| Development graph | none | `timegraph`, `debuggraph`, `graphheight`, `graphscale`, `graphshift` along the bottom (`client/q2_bar.cpp` over `hud/bottom_graph.hpp`) | its own | its own |
| Command packets | `localid`: a server browser on this machine sends commands that carry it (`net/qw_client.cpp`) | none | its own | its own |
| Setup figure | the pack's figure in the chosen shirt and pants (`menu/classic_art.cpp`'s `player_in`) | its figure rows (`menu/q2_pages.cpp`) | its own | its own |
| Screenshot names | by the time | `gl_screenshot_template` (`quakeXXX`) and `gl_screenshot_compression` over `scr_template` and `scr_compression` (`capture/screenshot.hpp`) | by the time | the engine's |

## Adding a game

1. The adapter: every call of the table, the shared ones from `adapter_parts.hpp`. Nothing in the
   window may name a file of the game.
2. Its layer (`add_<game>` in `cvar_layers.cpp`): its spellings as synonyms, its defaults as `same`
   rows, its own meanings as `own`, its own settings; `GameBindings` for each game-meaning the
   window's structs hold. Reset every field the previous game set, so switching games leaks nothing.
3. Its status bar from its own pictures, its HUD snapshot filled from its rules or its server's
   status (`hud/status.hpp`), its weapon keys for the quick weapon and groups (`hud::KeyedWeapons`).
4. Its menu letters cut from its own pack into an alphabet, the missing ones pieced from the ones it
   has; its pages as a `menu::MenuTable`.
5. Its console font and background, its words for the player (player text reads like a game's).

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| The drop-down console's scripting, `if`, regex, aliases with arguments | Quake's later clients | `engine/console` |
| Chat ignore lists, team chat codes, the point-at word | Quake's later clients | `engine/console/chat_ignore.hpp`, `team_chat.hpp` |
| Quick weapon, weapon groups, weapon preselection | Quake's later clients | `plugins/shooter`, both games' keys |
| Raw mouse, mouse filter and acceleration | both games' later clients | `mouse_feel.hpp` |
| HUD layout as data the player moves | ours | `engine/hud` |
| One config for the series | ours | `client/settings.hpp` |
| `_echo`, `text`, `complete`, `setu`, `sets`, `resetall`, `writeconfig`, the clock macros, `draw` strings | Quake II's later client | `engine/console`, `engine/hud/draw_strings.hpp` |
| Alt+Enter for full screen, whatever the binds | Quake II's later client | `engine/platform/window_cvars.hpp` |
| The lag graph and the lost-connection picture | Quake II's client | `engine/hud/net_graph.hpp` (`draw_lag_graph`), both games' packets |
| Sound muffled under water (`s_underwater`, off in Quake) | Quake II's client | `engine/audio/synth.hpp`'s muffle, `client/room_ear.hpp` |
| Speakers swapped (`s_swapstereo`) | both games' later clients | `engine/audio/synth.hpp`'s `swap_stereo`, `client/room_ear.hpp` |
| Two ears for headphones (`s_hrtf`, Quake II's `al_hrtf`): the far ear later, softer and duller | Quake II's later client | `engine/audio/synth.hpp`'s `binaural`, `client/room_ear.hpp` |
| The folder the game writes in (`+set homedir`) | Quake II's later client, any game's here | `client/take_in.hpp` |
| Typed history kept to a length (`con_history`) | Quake II's client | `engine/console/view.hpp` |
| The string commands (`alias_in`, `cvar_in`, `set_bind_str`, `hash` ...), `msg_trigger` levels, `$triggermatch` | Quake's later clients | `engine/console/script_strings.cpp`, `script.cpp` |
| Alias arguments `$0`, `$@`, `${1-3}`, `%1`; `if ... eq/ne/isin` | Quake II's later client | `engine/console/lex.cpp`, `script.cpp` |
| The team's `$` macros, `$ledpoint`, `$ledstatus`, the match's `f_` hooks, fake names, flood protection, `cl_nofake`, the sound trigger, `f_version` answers | Quake's later clients | `client/team_macros.hpp`, `team_chat.hpp`, `chat_lines.hpp` (the classic layer) |
| `loc_draw` markers at the place points, `$cl_surface` | Quake II's later client | `client/place_names.hpp` (`place_markers`), `client/world_fx.cpp`, `client/face_hit.hpp` |
| Map commands (`cl_beginmapcmd`, `cl_changemapcmd`, `cl_disconnectcmd`), `$loc_here`, `$loc_there` | Quake II's later client | `client/q2_session.hpp` (`add_map_command_cvars`), `place_names.hpp` |

| Drawback left behind | How |
| --- | --- |
| A setting named two ways in two games | one name of ours, each spelling a synonym while its game is loaded |
| A newer game's look leaking into an older one by a shared name | the layers: `own` meanings, nothing merged |
| Menus in a stand-in font | each game's own letters, cut from its pack |

## Gaps

- `toggle` with values now cycles them in every game (`toggle <name> <v1> <v2> ...`), which only
  adds to its meaning: given one name, it still flips 0 and 1.
- Quake II's menu: the player figure, model and skin rows, the address book (our menu has no text
  field), 16 load and 15 save slots with the autosave, and the rows whose settings are not
  registered yet (dimmed until they are).
- The third game's client half (its HUD and prediction in bytecode).
- `version` prints the game's version alone, not the build's time and date as the classic clients
  do: a build stamp would make every build differ and empty the compiler cache. The menu words
  (`menu_singleplayer`, `menu_load`, `menu_save`, `menu_multiplayer`, `menu_setup`, `menu_keys`,
  `menu_video`) open the shell's pages; `menu_keys` opens a game's own key page where its menu has
  one. `user` prints a joined QuakeWorld player's user info from what the server last told;
  `msg` and `noaim` go in the user info; `ambient_level` and `ambient_fade` set the water and
  wind's loudness and how fast they swell (the fade in 255ths of full a second); `m_side` scales
  the mouse's sidestep in every shooter; `+use` is held but never sent, as in the classic games.

## Labs and tests

`se-menu`, `se-hud`, `se-q2-hud`, `se-pictures`, `se-console-words`, `se-keymap`,
`se-client`; `tests/shooters_edge_cvar_layers_test.cpp`.

## Lessons

- `client/main.cpp` sits at the hard line cap: each new `GameBindings` field costs a line; new
  wiring goes in a file of its own.
- Lifting a feature to a shared layer often means hooking the other game's path into it: the ignore
  lists applied only on Quake's servers until they were hooked into `q2_remote`.
- A command registered after the console starts is unknown to `+` lines on the command line.
- A game wraps an engine command by keeping its function, not by copying its body.
