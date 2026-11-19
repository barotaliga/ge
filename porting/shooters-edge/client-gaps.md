# Client settings: the gap list against the classic clients (working note)

WORKING AREA: deleted when porting ends. Which settings and commands of the first game's modern
client (`client-inventory.md`) and the sequel's stock and modern clients
(`sequel-client-inventory.md`) the window has, and the ranked candidates for the next rounds,
gameplay and daily use first. The behaviour of each is in those two inventories (clean-room
notes); a building session reads only them, never the source.

**How the list was made (2026-11-18).** Every name in the two inventories' tables, matched without
case against the window's own (its `cvarlist` and `cmdlist` at start, plus every name the tree
registers by a literal: commands registered after the start configs run do not show in the first).
A name the window forwards to the server (`kill`, `use`, `drop`, `weapnext`, ...: the sequel's game
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
| `fov` | a synonym of `cl_fov` in both games' layers; no `same` 90 row (`cl_fov 0`, the rig's own, is 90 and keeps the zoom's lens; a set value replaces both); the sequel's user info carries it (the sequel thread's `ClientConfig::fov`) | `game:classic`, `game:sequel` | 2 |
| `cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed`, `cl_upspeed`, `cl_movespeedkey` | the same; the first game's 400 walking (the modern client's), the sequel's `same` 200 (`cl_backspeed` too: the sequel has none of its own, its back key asks the forward's speed); running asks `cl_movespeedkey` (2) times it | shooter plugin (`controls.hpp`) | 2 |
| `cl_pitchspeed`, `cl_anglespeedkey`, `+lookup`, `+lookdown`, `+klook`, `+mlook`, `freelook`, `m_forward`, `centerview`, `force_centerview` | the same; `freelook` 1 in both (the stock sequel's 0 not kept: every config of today's players has it on) | shooter plugin | 2 |

| `weapon <n> [n ...]` (10 and 12 step), `impulse` with a list, `cl_weaponpreselect`, `cl_weaponhide`, `cl_weaponhide_axe`, `cl_weaponforgetorder` | the same (`weapon-choice.md`); the list is not yet reset on a new map (a later round) | classic (`client/weapon_choice.hpp`) | 3 |
| `w_switch`, `b_switch` | the same, user info sent on joining a first-game server; changing it while joined still needs `setinfo`; our own server does not read them yet | `game:classic` | 3 |
| `+showscores`, `+showteamscores` | the same, TAB bound to `+showscores` by default | shell (`client/controls.hpp`) | 3 |
| `ignore`, `ignore_id`, `unignore`, `unignore_id`, `ignorelist`, `ignore_team`, `unignore_team`, `unignoreall`, `unignoreall_team`, `ignore_mode`, `ignore_opponents`, `ignore_spec`, `ignore_flood`, `ignore_flood_duration` | the same (`chat-ignore.md`); a number is the player's place, not the server's user id (the window does not keep user ids yet) | engine (`console/chat_ignore.hpp`), read on a classic server joined (`client/chat_ignore.hpp`) | 4 |
| `ignore_qtv` | a synonym of our `ignore_tv` (engine) | `game:classic` | 4 |
| `ignore_no_weapon`, `ignore_not_enough_ammo` | the same | shooter plugin (`weapon_lines.hpp`) | 4 |
| `cvarlist_re`, `cmdlist_re`, `cvar_reset_re`, `toggle_re`, `unset_re`, `unalias_re`; `unset <name>` for a setting `set` made | the same; a pattern is matched anywhere in the name without case; `unset_re` never touches a game's own value | engine (`console/console_patterns.cpp`) | 5 |
| `toggle a b c` | open: one name, two meanings. The first game's modern client flips each name; the sequel's modern client cycles `a` through the values `b c` (our `cycle`) (`client-mouse-and-sequel-player.md`). Ours still flips one name; the meaning per game waits for a command layer | | 5 |
| `m_filter`, `m_accel`, `m_accel_power`, `m_accel_offset`, `m_accel_senscap`, `in_raw` | the same (`client-mouse-and-sequel-player.md`); the sequel's `m_accel` keeps its own meaning (counts a frame, held within 0..1); `in_raw` takes effect the next time the window takes the mouse | shooter plugin (`mouse_feel.hpp`), the sequel's meaning set by `game:sequel` | 6 |
| `hand`, `gender`, `gender_auto` | the same, the sequel's user info; `gender_auto` picks from the skin's figure when joining; `hand 2` draws no weapon (round 10). The gun on the left (`hand 1`, the fork's `cl_gun` 2 and 3) waits for the view round: no mirrored view model yet | `game:sequel` | 6 |
| `cl_predict` | the sequel's own setting, bound to the remote's prediction beside our `cl_nopred` (0 draws the server's place; the first game always predicts unless `cl_nopred 1`) | `game:sequel` | 7 |
| `ignoretext`, `unignoretext`, `ignorenick`, `unignorenick` | the same (`client-mouse-and-sequel-player.md` § 10), for every game; the ignore lists now apply on a sequel server joined too | engine (`console/chat_ignore.hpp`) | 8 |
| `mul`, `div` | the same (the chosen fork's, `sequel-client-fork-delta.md`); `div` by 0 keeps the value | engine (`console/script.cpp`) | 8 |
| `cl_predict_crouch` | the same (the chosen fork's): the predicted move's eye height replaces the frame's; 1 (the default) on our servers, which tell the move they run (svc 122), 2 on every server | `game:sequel` | 9 |
| `version` (user info) | the client's name and version sent to a sequel server on joining (`kProgramVersion`, the first game's server info too) | sequel client | 9 |
| `cl_noskins` | the sequel's own beside the shared `noskins`: 0 and 1 mean what `noskins` 0 and 1 do in the sequel, but 2 does not (`noskins 2` keeps skins and fetches none), so they are two settings: 1 every player the stock male figure, 2 a female figure's player (female, crackhor) the stock female, every other the stock male (`client-mouse-and-sequel-player.md`) | `game:sequel` (picture's `stock_figures`) | 10 |
| `+zoom`, `-zoom` | the same (`client-view-and-zoom.md` § 1): while held, `cl_fov` 35 at once and `sensitivity` times 35 over the lens it had (90 for `cl_fov 0`); release puts both back as they were. No default key (ALT is `+strafe` here). In both games: the sequel's modern client has none of its own | shooter plugin (`controls.hpp`) | 11 |
| `v_viewheight` | the same (`client-view-and-zoom.md` § 2): raises or lowers the eye by -7..4 units over both heights, and any value but 0 stops the vertical bob. The engine camera's `FirstPersonRow::eye_lift_m` (`CameraCvars::viewlift`), bound in the shell for both games; Quake II's modern client has none of its own | engine camera, shell (`present.cpp`) | 11 |
| `m_autosens` | the same (`client-mouse-and-sequel-player.md` § `m_autosens`): 0 off; 91..179 the field it is tuned at, any other value 90; the mouse's counts scaled across by the lens over the tuned one in degrees and up and down by their 4:3 heights. With it on, `+zoom` leaves `sensitivity` alone. In both games (the first game's client has none) | shooter plugin (`controls.hpp`) | 11 |
| `r_viewmodelsize` | the same (`client-view-and-zoom.md` § 4): 0..1, the gun's depth axis scaled by 0.5 + value / 2 about its origin (engine `PlaceRow::depth`) | `game:classic` (`cvar_layers.cpp`) | 11 |
| `r_viewmodeloffset` | the same (§ 5): up to three numbers in units, right, DOWN, forward, missing ones 0; one thing with our `cl_gunx/y/z` in two units, so it adds to them (`view::game_offset_m`) | `game:classic` | 11 |
| `cl_gun_x`, `cl_gun_y`, `cl_gun_z` | the same (§ 9): units, right, FORWARD, up; added to our `cl_gunx/y/z` | `game:q2` | 11 |
| `cl_gunfov` | the same (§ 9): the gun's lens, degrees across at 4:3, 90 by default, 0 the view's, else 30..160; our `r_viewmodel_fov` (vertical) wins when set | `game:q2` | 11 |
| the sequel's `cl_maxfps` (the move rate, 10..250) | open: our `cl_maxfps` is the frame cap; the sequel's spelling means the move rate (its `r_maxfps` is our frame cap). Both wait for the network round's move rate, `cl_physfps` in the shooter layer; the sequel then declares `cl_maxfps` `own` (pointed at the move rate, a synonym cannot shadow a live setting) and `r_maxfps` a synonym of our frame cap, a test holding both | | 12 |
| `cvar_reset`, `viewalias` | the first game's spellings of our `reset` and `alias <name>` (command synonyms) | `game:classic` | 5 |
| `set_ex`, `set_ex2`, `set_tp`, `macrolist` | the same: `set_ex` fills the percent codes and fun characters as `echo` does, `set_ex2` leaves the fun characters, `set_tp` sets a `tp_` setting only (the strict rulesets' safe buffer, which allows only it, is not built) | game (`client/team_chat.hpp`) | 5 |

## Ranked candidates

Gameplay and the configs players carry first; each row is a round or part of one.

1. **`fov` and `default_fov`** (both games: `fov` 90, the sequel's also user info): ours is
   `cl_fov` (0 = the rig's own), so every config's `fov 110` is passed over today. A synonym in each
   game's layer and the game's default 90; the sequel sends it in its user info.
2. **The move keys' speeds**: `cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed`, `cl_upspeed`,
   `cl_movespeedkey`, `cl_anglespeedkey`, `cl_pitchspeed` (modern client 400 each, 2.0, and
   ezQuake's turn rates; the sequel 200 forward, side and up, no back speed, 1.5 and 150): the speed the client asks for, the server capping it. Shooter
   plugin settings with each game's default.
3. **Looking**: `freelook` (modern client 1; the sequel 0), `+mlook`, `+klook`, `+lookup`,
   `+lookdown`, `centerview`, `force_centerview`, `m_forward`, `m_side`, `v_centermove`,
   `v_centerspeed`. Shooter plugin.
4. **Weapon choice**: the `weapon <n> [n ...]` command (the best owned of a list; every modern
   config's weapon keys), `cl_weaponpreselect`, `cl_weaponhide`, `cl_weaponhide_axe`,
   `cl_weaponforgetorder`; `w_switch` and `b_switch` (user info the server reads on a pickup).
5. **The scoreboard keys**: `+showscores`, `+showteamscores` (TAB is bound to them in configs).
6. **Chat**: `ignore`, `unignore`, `ignorelist`, `ignore_team`, `unignoreall` (first game);
   `ignoretext`, `unignoretext`, `ignorenick`, `unignorenick` (the sequel's modern client);
   `cl_chat_sound`, `cl_chat_notify`, `cl_chat_filter`, `scr_chathud*`.
7. **Scripting a config needs**: `unset`, `cvar_reset`, `set_ex`, `set_tp`, `viewalias`, `toggle_re`,
   `cvarlist_re`, `cmdlist_re`, `unalias_re`, `macrolist`.
8. **Mouse** (done; `m_autosens` with the view round; the chosen fork's `cl_zoom_autosens` left
   for its scopes, with Action Quake).
9. **The sequel's player**: `hand`, `gender`, `gender_auto` and `cl_predict` done; `cl_vwep`
   (other players' weapons drawn), `cl_noskins` (its 2 keeps the female figure, unlike our
   `noskins 2`) and the gun's side on screen (`hand` 1 mirrored, 2 hidden; our view model is
   centred by default) next, with the view round. The chosen fork's own client additions are in
   `sequel-client-fork-delta.md` (its crosshair `xhair_*`, hit markers off by default, the
   game's HUD elements, Action Quake's commands): rank them into the rounds below.
10. **The view**: `v_viewheight` and `+zoom` done; `v_gunkick`, `r_viewmodelsize`, `r_viewmodeloffset`,
    `r_viewmodellastfired`, `cl_bobhead`, `cl_bonusflash`, `v_dlightcshift`,
    `scr_fovmode`.
11. **Teamplay**: `tp_point`, `tp_took`, `tp_pickup` (the sets), `set_tp`, `filter`, the
    `locations_*` commands, `re_trigger*` (the regex triggers), `mapgroup`, `skygroup`.
12. **Network and prediction**: `cl_c2spps`, `cl_physfps`, `cl_independentphysics`,
    `cl_predict_*`, `cl_earlypackets`, `cl_solid_players`, `cl_nolerp_on_entity`,
    `rcon_password`, `rcon_address`, `cl_timeout`, `showdrop`, `showpackets`, `qport`.
13. **Crosshair**: `crosshairimage`, `crosshairscale`, `cl_crossx`, `cl_crossy`; the sequel's
    modern `ch_*`.
14. **Demos and the rest**: the demo, broadcast and match-tool settings (158), the server
    browser's (39), screenshots' (5), the console's own looks.
