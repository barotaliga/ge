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
| `hand`, `gender`, `gender_auto` | the same, Quake II's user info; `gender_auto` picks from the skin's figure when joining; `hand 2` draws no weapon (round 10). The gun on the left (`hand 1`, the fork's `cl_gun` 2 and 3) waits for the view round: no mirrored view model yet | `game:q2` | 6 |
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
10. **The view**: `v_viewheight`, `+zoom` and `v_gunkick` done; `r_viewmodelsize`, `r_viewmodeloffset`,
    `r_viewmodellastfired`, `cl_bobhead`, `cl_bonusflash`, `v_dlightcshift`,
    `scr_fovmode`.
11. **Teamplay**: `tp_point`, `tp_took`, `tp_pickup` (the sets), `set_tp`, `filter`, the
    `locations_*` commands, `re_trigger*` (the regex triggers), `mapgroup`, `skygroup`.
12. **Network and prediction**: `cl_physfps` and `cl_c2spps` done; `cl_independentphysics`,
    `cl_predict_*`, `cl_earlypackets`, `cl_solid_players`, `cl_nolerp_on_entity`,
    `rcon_password`, `rcon_address`, `cl_timeout`, `showdrop`, `showpackets`, `qport`.
13. **Crosshair**: `crosshairimage`, `crosshairscale`, `cl_crossx`, `cl_crossy`; Quake II's
    modern `ch_*`.
14. **Demos and the rest**: the demo, broadcast and match-tool settings (158), the server
    browser's (39), screenshots' (5), the console's own looks.
