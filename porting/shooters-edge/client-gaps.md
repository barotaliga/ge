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
8. **Mouse**: `m_filter`, `m_accel`, `m_accel_power`, `m_accel_offset`, `m_accel_senscap`,
   `in_raw`, the sequel's `m_autosens`.
9. **The sequel's player**: `hand` (0 right, 1 left, 2 centre: user info, the gun's side and
   whether it shows), `gender`, `gender_auto`, `cl_vwep`, `cl_noskins`, `cl_predict`.
10. **The view**: `v_viewheight`, `v_gunkick`, `r_viewmodelsize`, `r_viewmodeloffset`,
    `r_viewmodellastfired`, `cl_bobhead`, `cl_bonusflash`, `v_dlightcshift`, `+zoom`,
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
