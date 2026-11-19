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

## Each game's

| | The first game | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Adapter | `client/classic_adapter.cpp` | `client/q2_adapter.cpp` | a file of its own | a file of its own |
| Layer | `add_first` in `client/cvar_layers.cpp` | `add_q2` | `add_<game>` | `add_<game>` |
| Status bar | its pictures in fixed places | a layout program from the server (`hud/q2_layout.hpp`, `client/q2_bar.hpp`, `server/q2_bar.hpp`) | its client half's HUD (bytecode or native) | its own bar |
| Menu letters | carved letters cut from its strips (`menu/carved_letters.hpp`) | plate letters cut as ink (`menu/plate_letters.hpp`) | its own font pictures | its own font lumps |
| Menu pages | `menu::classic_menu()` | its own pages | its own (multiplayer only) | its own |

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
| The drop-down console's scripting, `if`, regex, aliases with arguments | the first game's later clients | `engine/console` |
| Chat ignore lists, team chat codes, the point-at word | the first game's later clients | `engine/console/chat_ignore.hpp`, `team_chat.hpp` |
| Quick weapon, weapon groups, weapon preselection | the first game's later clients | `plugins/shooter`, both games' keys |
| Raw mouse, mouse filter and acceleration | both games' later clients | `mouse_feel.hpp` |
| HUD layout as data the player moves | ours | `engine/hud` |
| One config for the series | ours | `client/settings.hpp` |

| Drawback left behind | How |
| --- | --- |
| A setting named two ways in two games | one name of ours, each spelling a synonym while its game is loaded |
| A newer game's look leaking into an older one by a shared name | the layers: `own` meanings, nothing merged |
| Menus in a stand-in font | each game's own letters, cut from its pack |

## Gaps

- `toggle`: it flips each named setting in one game and cycles values in the other; there is no
  per-game command layer yet to give one name both meanings.
- The third game's client half (its HUD and prediction in bytecode).

## Labs and tests

`se-menu`, `se-hud`, `se-q2-hud`, `se-pictures`, `se-console-words`, `se-keymap`,
`se-client`; `tests/shooters_edge_cvar_layers_test.cpp`.

## Lessons

- `client/main.cpp` sits at the hard line cap: each new `GameBindings` field costs a line; new
  wiring goes in a file of its own.
- Lifting a feature to a shared layer often means hooking the other game's path into it: the ignore
  lists applied only on the first game's servers until they were hooked into `q2_remote`.
- A command registered after the console starts is unknown to `+` lines on the command line.
- A game wraps an engine command by keeping its function, not by copying its body.
