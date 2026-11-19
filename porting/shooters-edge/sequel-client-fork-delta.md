# The sequel's chosen client fork: what it adds to and changes from the modern client

WORKING AREA: deleted when porting ends. A clean-room note, in our words, with no code. The project owner chose the Action Quake community's fork of the modern sequel client as the main reference for the sequel's client. This note records how that fork's CLIENT differs from the modern client our notes already describe (`sequel-client-inventory.md` § 8 and `client-mouse-and-sequel-player.md`): every client setting and console command it adds, drops, or gives another default, values or meaning, grouped by subject. Server-only settings are left out unless the client sends or reads them.

Read from the fork's default branch (`aqtion`), github.com/actionquake/q2pro, GPL-2.0, commit `6400f7e`, committed 2026-11-16, its source and its own manuals, and compared with the modern client's default branch head, github.com/q2pro/q2pro, commit `601a8df`, committed 2025-12-11, the commit our earlier notes read. Nothing was measured by running either program. Every fact is marked **[R]** (read directly) or **[D]** (derived by combining what was read). Where the source does not settle a point, the text says so.

Terms:
- **AQ build**: the fork's build options turn its Action Quake additions on by default, so they are in the program a player gets. The Discord additions are behind a separate build option that is off by default. [R]
- **Protocol 38**: the fork's own network protocol, a superset of the modern client's protocol 36, with its own minor versions (3011 to 3017 at this commit). Several additions below work only over it. [R]
- **Absent**: a name the modern client has and the fork does not. The source alone does not settle whether the fork removed it or never took a later upstream change. The fork's own manual also lacks each absent name, which points to "never took it". [D]
- Names not listed here behave as in the modern client. In particular `sensitivity` keeps its default 3: a second, later registration with default 0 does not change it, because the first registration's default wins. [D]

---

## Mouse (5)

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `cl_zoom_autosens` | 0, saved | 0, 1 | 1 swaps `sensitivity` while the sniper rifle is zoomed: each drawn frame the client looks at the current horizontal field of view and sets `sensitivity` to `cl_zoom_2x` at exactly 45 degrees, `cl_zoom_4x` at exactly 20, `cl_zoom_6x` at exactly 10, and back to the saved value at 90 or wider or at any other field | R |
| `cl_zoom_2x` | 0, saved | a sensitivity | The sensitivity used at the 2x zoom. 0 means "do not use one for this level" | R |
| `cl_zoom_4x` | 0, saved | a sensitivity | The same at the 4x zoom | R |
| `cl_zoom_6x` | 0, saved | a sensitivity | The same at the 6x zoom | R |
| `oldsens` | the `sensitivity` at start-up, not saved | a sensitivity | The value the zoom swap restores. Its text is set once, when the input settings are registered, and the swap never rewrites the text, so with `cl_zoom_autosens` 1 a `sensitivity` changed later in the session snaps back to this value on the next unzoomed frame unless the player also sets `oldsens` | D |

Notes on the zoom swap:
- A level whose setting is 0 does not keep the previous level's sensitivity, as the fork's manual says it does: it falls through to restoring `oldsens`. [D]
- The test is for exact field values, and the field is blended between frames during a zoom change, so the swap lands only once the blend settles. [D]
- It runs every drawn frame while on, in any game, not only Action Quake. The 45, 20 and 10 degree fields are the mod's sniper zoom levels. [D]
- `m_autosens` (modern client) is still there and is applied on top. The source does not say how the two are meant to combine. [R]

## View and weapon (22)

The new crosshair. All names below are new, not saved, and act only with `xhair_enabled` 1. [R]

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `xhair_enabled` | 0 | 0, 1 | 1 replaces the picture crosshair with a drawn one of four lines and a dot, in screen pixels (not scaled with the HUD), coloured like the picture crosshair (`ch_red`, `ch_green`, `ch_blue`, `ch_health`). It is drawn even when `crosshair` is 0, when the status bar asks to hide the crosshair, and under a sniper scope | R, D |
| `xhair_dot` | 1 | 0, 1 | Draw the centre dot | R |
| `xhair_length` | 4 | pixels | Length of each line | R |
| `xhair_gap` | 10 | pixels | Base distance from the centre to where each line starts | R |
| `xhair_thickness` | 1 | pixels | Width of the lines and the size of the dot | R |
| `xhair_x` | 0 | pixels | Horizontal offset from the screen centre | R |
| `xhair_y` | 0 | pixels | Vertical offset from the screen centre | R |
| `xhair_elasticity` | 1 | a factor | How quickly the drawn gap and length follow their target: lower is stiffer, higher springier | R |
| `xhair_fast_transition` | 1 | 0, 1 | 1 jumps straight to the target when it is far away (a gap more than 200 pixels off or a length more than 50 off), so a weapon switch does not animate slowly | R |
| `xhair_firing_error` | 1 | a factor, 0 off | The gap opens and the lines grow on the weapon's firing frame, more with sustained fire, scaled by this value | R |
| `xhair_movement_error` | 1 | a factor, 0 off | The gap widens while the player moves faster than 20 units a second along the ground, and narrows while ducked | R |
| `xhair_deployed_weapon_gap` | 1 | 0, 1 | 1 adds a per-weapon gap read from the view model shown: the M4, the pistol, the MP5, the dual pistols, the M3 shotgun (wide) and the hand cannon (widest) each have their own. For the knife, the sniper rifle, grenades and any unknown view model the lines are pushed far out of sight, leaving only the dot | R, D |

Changed or other view settings:

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `ch_hide_while_zoomed` | 1 | 0, 1 | New. 1 hides the picture crosshair while the status bar or layout draws a sniper scope picture | R |
| `ch_scale` | 1 | a scale | Meaning changed: the crosshair picture is now drawn at HALF of this value, so 2 gives what the modern client draws at 1. The sniper scope picture (the mod's 2x, 4x and 6x pictures, also when kept in a scopes sub-folder) is now recognised wherever the layout places it, drawn centred at its own picture size times half this value and shifted by `ch_x` and `ch_y`. The modern client drew the scope only at its classic position, at 320 by 240 times the full value | R |
| `cl_hit_markers` | 0 | 0, 1, 2 | Default changed from 2 to 0: hit markers are off unless the player turns them on. Values as in the modern client | R |
| `cl_muzzlelight_time` | absent | | The muzzle flash light always lasts 16 milliseconds | R |
| `cl_lerp_lightstyles` | absent | | Light styles always step, never blend | R |
| `cl_smooth_explosions` | absent | | Explosions fade the classic way. The BFG's light still blends, but only on servers with the protocol's extended limits | R |
| `cl_disable_particles` | 0 | bit mask | The 32 bit (blaster trails) is gone. Blaster trails are always drawn. The other bits are as in the modern client | R |
| `cl_gun` | 1 | 0 to 3 | Values 2 and 3 (gun always right or always left) still move the gun, but no longer move the start of beams such as the grapple cable, which follow `hand` only. `cl_gunfov` likewise no longer adjusts where beams start | R |
| `cl_thirdperson` | 0 | 0, 1 | The chase camera is stopped by the world only, not by moving doors and platforms, and the player's own model never fades when the camera is pushed close to it | R |
| `fog` (command) | | | With no arguments, a user fog or a map fog that has only a sky share and no density now prints "No fog." | R |

## HUD and scoreboard (2)

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `scr_lag_draw_scale` | 1 | whole numbers, the menu offers 1, 2, 3 | New. Draws the ping graph (`scr_lag_draw`) and the lost-connection picture this many times larger. The graph keeps 48 samples but draws more columns at a scale above 1, and its bars keep their unscaled height. What the extra columns show is not settled by the source | R, D |
| Game-defined HUD elements | | | New, no setting. Over protocol 38 (minor version 3012 and later) the server sends up to 200 HUD elements: text, a picture, a number or a filled box, each with a colour, a size, an anchor on the screen, blink and hide flags, or a place in the world (drawn where that point falls on screen, sized by distance, fading near the crosshair). They are drawn after the layout and under `scr_alpha`. Nothing lets the player turn them off but `scr_draw2d` | R |

Scoreboard: the client adds no scoreboard setting. The mod's board columns are the server's `scoreboard` setting, not the client's. [R]

## Chat and ignore (0)

The client's chat and ignore settings and commands are exactly the modern client's. The mod has its own player ignore commands, answered by the game library: they are listed under the game-mod group. [R]

## Demos and recording (1)

No demo or recording setting or command is added, dropped or changed. [R]

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `gtv` (command) | | `start`, `stop`, `status` | The client's live relay of the player's own game to a game TV viewer over TCP is now built by default (the modern client's build leaves it out unless asked). Which port and who may connect are governed by the shared network settings, which this reading did not follow | R |

`cl_indicators` also acts during demo playback: see the game-mod group. [R]

## Network and prediction (5)

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `cl_protocol` | 0 | 0, 34, 35, 36, 38 | 38 is new. With 0 the client asks for 38 first, then falls back to 36, 35, 34 by what the server offers. A local game run with an out-of-range value uses 38 | R |
| `cl_maxfps` | 62 | 10 to 250 | The upper clamp of the move (packet and physics) rate is 250 instead of 125 | R |
| `cl_async` | 1 | 0, 1 | Value 2 (let the GPU's buffer swaps pace the drawing) is absent. 2 behaves like 1 | R, D |
| `cl_predict_crouch` | 1 | 0, 1, 2 | New. While moves are predicted, the eye height is predicted too, so ducking moves the view at once. 0 off (the eye follows the server). 1 only when the server announces its standing and crouched eye heights. 2 always: the announced heights, or else the predicted move's own eye height | R |
| `version` | the program's version string, read-only | | Now also user info: every server is sent the client's version | R |

Also:
- A server that announces eye heights does so with three read-only settings set by the game library, `g_view_predict`, `g_view_high` and `g_view_low`. The mod sets them to on, 22 and 8. They reach the client only over protocols 36 and 38. [R]
- A server message the client does not know now prints, before the usual error, a line asking the player to update the client through Steam. [R]

## Downloads (3)

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `allow_download_maps` | as the modern client | | When downloads run, a map already on disk whose checksum differs from the server's is deleted and downloaded again, once per connection. A second mismatch ends the connection with a message that the server runs another version than it offers. When a map is missing and downloads are off, the error names `allow_download` and `allow_download_maps` and says both should be 1 | R, D |
| `r_override_textures`, `r_texture_overrides` | 1, -1 | as the modern client | Now also drive downloads: with `r_override_textures` 2, or with the wall-texture bit (16) of `r_texture_overrides` set, the client asks the server for a JPEG replacement of every wall texture the map uses | R, D |
| `sv_downloadserver` | the mod's asset web site | a URL | A server setting, listed because the client fetches from it: the fork's server points clients at the project's own asset host by default, where the modern server's default is empty | R |

## Sound (7)

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `cl_footsteps` | 1 | 0, 1, 2 | 2 now also plays only the first four steps of a surface's set (the classic four) and the single classic landing sound. At 1 a short fall plays one of eight landing sounds at random. A surface's set holds at most 9 steps (the modern client allows 15) | R |
| `cl_enhanced_footsteps` | 0 | 0, 1 | New, but read by nothing at this commit: the twelve-step and eight-landing behaviour the fork's manual gives it is keyed on `cl_footsteps` instead | R, D |
| `al_merge_looping` | absent | | Looped sounds under OpenAL each play as their own source | R |
| `ogg_menu_track` | absent | | No music while disconnected. Music stops whenever the client clears its state (a map change or a disconnect) and starts again only once a map is loaded | R |
| `ogg_remap_tracks` | absent | | Mission pack track numbers are not remapped | R |
| `ogg` (command) | | `info`, `play`, `stop`, `next` | `pause` and `toggle` are gone, and `play` takes no fade option | R |
| Player muzzle flash sounds | | | No setting. Only six of the stock player flashes make a sound, each re-purposed as an Action Quake weapon: blaster as the pistol, machine gun as the MP5, rocket launcher as the M4, shotgun as the M3, super shotgun as the hand cannon (heard farther away) and hyperblaster as the sniper rifle. Every other stock flash (railgun, grenade launcher, BFG, chaingun, the mission packs' weapons, the log-in, log-out and respawn flashes) is silent and draws no effect. Every player flash lights yellow, and the re-release flash models are not drawn on player flashes. The stock sequel played through this client sounds and looks wrong | R, D |

## Game-mod specific: Action Quake (25)

Client settings. [R]

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `cl_mk23_sound` | 0 | 0 to 7 | Which fire sound the pistol plays: 0 the stock one, 1 to 7 a numbered alternative (the stock sound's name with the digit appended). Out of range counts as 0 | R |
| `cl_mp5_sound` | 0 | 0 to 7 | The same for the MP5 | R |
| `cl_m4_sound` | 0 | 0 to 7 | The same for the M4 | R |
| `cl_m3_sound` | 0 | 0 to 7 | The same for the M3 shotgun | R |
| `cl_hc_sound` | 0 | 0 to 7 | The same for the hand cannon | R |
| `cl_ssg_sound` | 0 | 0 to 7 | The same for the sniper rifle | R |
| `cl_indicators` | 1 | 0, 1, 2 | Team indicator arrows over players. In the client, 0 hides every entity drawn as an indicator while a demo plays. The mod reads the value too: 0 never sends indicators, 1 sends them only while the player is not on a team (spectating), 2 also while on a team when the server allows it | R |

The setting sync (new, protocol 38 at minor version 3013 and later): [R]
- At connect the server sends a list of up to 32 setting names, each with a default of up to 63 characters. The client creates any it lacks with that default and prints "CL adding cvarsync" with each name and value.
- From then on the client tells the server the value of each listed setting whenever it changes, and at once for any whose value differs from the server's default. The game library reacts at once.
- A setting created this way is not saved to the config unless the player saves it. [D]
- On older protocols the mod reads the same names from the user info instead, so a player there sets them with `setu`. [R]
- The mod's list, with its defaults:

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `cl_antilag` | 1 | 0, 1 | 0 opts this player's own shots out of the server's lag compensation. The server prints the new state when it changes | R |
| `cl_indicators` | 1 | as above | Synced so the mod knows which indicators to send | R |
| `cl_xerp` | 0 | 0, 1, 2 | Asks the server to extrapolate other players and moving things for this player: 0 off, 1 full, 2 conservative. A server may forbid it or force a value, and says so when forcing | R |
| `cl_spectatorhud` | 1 | 0, 1 | While spectating, the team overlay. Protocol 38 clients get the new form | R |
| `cl_spectatorkillfeed` | 0 | 0, 1 | While spectating, kill messages in an easily parsed form | R |

Other things the mod reads from the client:

| Name | Default | Values | What it does for a player | |
| --- | --- | --- | --- | --- |
| `limp_nopred` | not registered, set by the player in user info | 0, 1, unset | Movement prediction while limping from a leg wound: 0 the classic jitter, 1 turn prediction off while limping, unset turn it off only above the server's ping threshold | R |
| `cvarsync` (typed, forwarded) | | a name and a value | The server's answer to a player who types it: sets one listed setting's value server-side and prints "cvarsync: set" with the name and value. The source does not settle what one argument alone prints | R |

The mod's own player commands that pair with the client settings above. These are the game library's, answered by the server, and are listed from the fork's mod manual, not from a reading of the mod's source: [R]

| Command | What it does |
| --- | --- |
| `lens` | Change the sniper zoom: no argument cycles, a number (1, 2, 4 or 6, others to the nearest) picks a level, `in` and `out` step |
| `punch` | A short-range punch that can knock a weapon out of a hand, where the server allows it |
| `ignore` | Ignore a player by name: no messages from them |
| `ignorelist` | List the players with their ids |
| `ignorenum` | Ignore a player by id |
| `ignoreclear` | Clear the ignore list |
| `voice` | Play a named sound to every player, where the server allows it |
| `menu` | Open the mod's menu |
| `time` | Print the time left on the map |
| `roundtimeleft` | Print roughly the time left in the round |
| Chat variables | The mod expands `%K` (last killed), `%P` (last damaged), `%D` (where you last hit), `%L` and `%S` (your place, the place you look at), `%E` (enemy in sight), `%F` (that enemy's weapon), `%me` |

The mod's full command set is wider (weapons, items, teams, voting, radio) and is outside this note.

## Other (26)

| Name | Default | Values | What changed | |
| --- | --- | --- | --- | --- |
| `game` | `action` | a folder | The build's default game folder is the mod's, and the base folder is `baseaq` instead of `baseq2`. The program calls itself AQtion | R |
| `div` (command) | | a setting and a number | New: divides a numeric setting by the number (default 1). Nothing guards a 0 | R, D |
| `mul` (command) | | a setting and a number | New: multiplies a numeric setting by the number (default 1). `inc` and `dec` are unchanged | R |
| `utf8test` (command) | absent | | A test-build command only | R |
| `use`, `drop`, `give` (commands) | | | Still forwarded, but Tab no longer completes item names | R |
| `steamid` | "0", read-only, user info | | New. Read-only, so only the command line can set it, and it is sent to servers. The mod reads it | R, D |
| `steamcloudappenabled` | "0", read-only, user info | | New. With `steamclouduserenabled` and a `steamid`, all not "0", the client adds a Steam Cloud folder for that account and game under its home folder to the search path | R |
| `steamclouduserenabled` | "0", read-only, user info | | New, as above | R |
| `cl_discord` | 0 | 0, 1 | New, Discord builds only, read at start-up: 1 joins Discord's rich presence and lobbies (invite and join by Discord) and registers the five names below | R |
| `cl_discord_id` | 0, read-only, user info | | The Discord account id, sent to servers. The mod reads it | R |
| `cl_discord_username` | empty, read-only | | The Discord name | R |
| `cl_discord_discriminator` | 0, read-only | | The Discord name's number | R |
| `cl_discord_avatar` | 0, read-only | | The Discord picture's hash | R |
| `cl_discord_accept_join_requests` | 0 | 0, 1 | 1 accepts Discord join requests without asking | R |
| `cl_extern_ip` | empty, read-only | | New, Discord builds only: at start-up the client asks a public what-is-my-address web service and stores the answer, so a Discord invite can carry the host's public address | R |
| `ui_colorservers` | 1 | 0, 1 | Default changed from 0 to 1. The colours changed too: grey for a password, anticheat or no-fake server, magenta for a server with bots, green at a ping up to `ui_colorpingmax`, yellow above three times that, cyan for a lag-compensated server in between, white otherwise | R |
| `ui_colorpingmax` | 50 | milliseconds | New: the green ping bound above | R |
| Server browser columns | | | The second column is "Bots" (Yes or No) instead of the mod's name, and the players column shows players over slots | R |
| Menus | | | The sound page labels `s_volume` as "effects volume". The effects page adds `gl_dynamic_lightstyles` and `gl_dynamic_muzzleflash` under dynamic lighting. The screen page adds a lag graph scale (`scr_lag_draw_scale`) | R |
| `gl_dynamic_lightstyles` | -1 | -1, 0, 1, 2 | New: overrides `gl_dynamic` for light styles only. -1 follows it, 0 none (fullbright), 1 all, 2 switchable lights only | R |
| `gl_dynamic_muzzleflash` | -1 | -1, 0, 1 | New: overrides `gl_dynamic` for muzzle flash and explosion lights. -1 follows it (on only at `gl_dynamic` 1), 0 off, 1 on | R |
| `gl_showedges` | 0, cheat | | New, registered but read by nothing at this commit | R |
| `gl_vert_diff` | 0 | | New, registered but read by nothing at this commit | R |
| `gl_draworder` | absent | | Translucent entities are sorted by model and skin only, not by distance or alpha | R |
| `gl_clearcolor` | absent | | No setting for the colour behind the world | R |
| `imagelist` (command) | | | Each line now starts with the picture's handle number | R |

Server-only, not counted: the fork's server adds the out-of-band queries `statusx` and `rulesx` (the server's status and rules in chunks, behind `sv_status_ext`, default 0) and the setting `sv_load_ent`. The fork's client does not use the queries. [R]

---

## Counts

Mouse 5, view and weapon 22, HUD and scoreboard 2, chat and ignore 0, demos and recording 1, network and prediction 5, downloads 3, sound 7, game-mod specific 25 (7 client settings, the sync itself, 4 further synced settings, `limp_nopred`, `cvarsync`, 10 of the mod's commands and its chat variables), other 26: **96**.
