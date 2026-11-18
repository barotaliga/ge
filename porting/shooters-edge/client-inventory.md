# Client inventory: every setting and command of the classic multiplayer client

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of every setting (console variable) and every console command the classic multiplayer client (ezQuake) registers in its default build, the user info keys it sends, its options menu and first-run state, the text its listing commands print, how it handles the stop-sound message, and which commands it forwards to the server; plus, for the stop-sound question only, what the classic server program (MVDSV) does. Written from their source by a reader session; the building session reads only this. No code, no internal identifiers.

Read from ezQuake at commit `5eb5920` (github.com/QW-Group/ezquake-source, default branch, committed 2026-11-18, with its protocol submodule at `8371618`) and MVDSV at commit `70f1135` (github.com/QW-Group/mvdsv, default branch, committed 2026-11-18). Nothing was measured by running either program: every fact is **R** (read directly) or **D** (derived by combining what was read). Where the source does not settle a point, the text says so. The tables are enumerated mechanically from every registration in the source, so a row with no letter is **R**; the one-line meanings are our own words.

Terms and marks:
- **Setting**: a console variable (`name value`). **Command**: a console word that runs an action. **User info**: the key/value string the client sends the server about the player. **Server info**: the server's string.
- **Default build**: the project's standard configuration. It compiles both renderers (the modern one and the classic one, so `vid_renderer` exists), the built-in (listen) server, the native-program and old-progs mod support, antilag weapon prediction and simple projectiles, and the third-person camera. It does NOT compile the IRC client, the CD-audio commands, the server-side weapon extension, the accurate-timing and scale extensions. TrueType font settings exist only in a build with the font library, and voice chat only in a build with its speech codec; whether a given release has them is a build choice the source does not settle. Rows that exist only on one platform or in one option say so. (R)
- Marks on settings: **menu** the options menu shows it (§5); **rules** a ruleset locks, caps or announces it (§1.19); **info** it is user info; **ro** read-only to the player; **latch** takes effect only after the subsystem restarts (`vid_restart`, `s_restart`); **reload** changing it reloads textures; **nosave** never written to the config; **voice** voice-chat builds only; **ttf** TrueType builds only; **win**, **mac**, **linux** that platform only; **classic** classic renderer only; **both-renderers** only when both renderers are compiled (true by default).
- Marks on commands: **fwd** sent to the server unchanged; **spec** meaningful only while spectating; **demo** meaningful only during demo or broadcast playback; **dev** developer builds; **voice**, **ttf** as above.
- **cfg**: there is no per-setting "goes in the config" list. Saving writes every setting whose value differs from its default (all of them with `cfg_save_unchanged 1`), skipping read-only, init-only and player-created ones (those are written with `set`), and never writing `cl_delay_packet`, `cl_proxyaddr`, `hud_planmode` and `con_bindphysical`. `cfg_save_userinfo` 2 (default) writes every user info setting; 1 leaves out `team`, `skin`, `spectator`, `topcolor` and `bottomcolor`; 0 none. So every row without **ro** or **nosave** is a cfg setting. (R)
- Names are matched without case. Where the source spells a name with capitals (`cl_iDrive`) the table keeps that spelling. (R)

## 1. Every setting, by area

Client settings: 1155 in §1.1–1.17, plus 1405 per-element HUD settings (§1.18), plus 168 of the built-in server (§1.20). A default shown as (empty) is the empty string.

### 1.1 Input and mouse (41 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_keypad` | `1` | 0–2 | 0: keypad keys act as the main keys; 1: every keypad key binds on its own; 2: own keys in game, main keys in the console |  |
| `freelook` | `1` | 0, 1 | 1: the mouse always looks up and down (mouse look on without +mlook) | menu |
| `in_builtinkeymap` | `0` | 0, 1 | 1: use the old fixed keyboard layout instead of the system's |  |
| `in_grab_windowed_mouse` | `1` | 0, 1 | 1: capture the mouse in a window too |  |
| `in_ignore_deadkeys` | `1` | 0–2 | Ignore the system's dead-key composition while a dead key is held (macOS only) | mac |
| `in_ignore_touch_events` | `1` | 0, 1 | 1: ignore touch-screen pointer events |  |
| `in_ignore_unfocused_keyb` | `0` | 0, 1 | 1: drop key events that arrive just after the window regains focus (Linux only) | linux |
| `in_raw` | `1` | 0, 1 | 1: read the mouse as raw relative motion | menu |
| `in_release_mouse_modes` | `2` | 0, 1, 2, 4, 8, 16, 32 | Bit mask of when a windowed client frees the cursor: 1 demo playback, 2 console, 4 typing chat, 8 menu, 16 HUD editor, 32 demo controls |  |
| `joyadvanced` | `0` | 0, 1 | 1: map joystick axes by the joyadvaxis settings |  |
| `joyadvaxisr` | `0` |  | What the joystick's R axis does (same codes as joyadvaxisx) |  |
| `joyadvaxisu` | `0` |  | What the U axis does |  |
| `joyadvaxisv` | `0` |  | What the V axis does |  |
| `joyadvaxisx` | `0` | 0–5 | What the X axis does: 0 none, 1 forward, 2 look up/down, 3 strafe, 4 turn, 5 up/down |  |
| `joyadvaxisy` | `0` |  | What the Y axis does |  |
| `joyadvaxisz` | `0` |  | What the Z axis does |  |
| `joyflysensitivity` | `-1.0` |  | Scale of the axis that moves up and down |  |
| `joyflythreshold` | `0.15` |  | Dead zone of that axis (0 to 1) |  |
| `joyforwardsensitivity` | `-1.0` |  | Scale of the forward axis |  |
| `joyforwardthreshold` | `0.15` |  | Dead zone of the forward axis |  |
| `joyindex` | `0` |  | Which joystick device to use (from 0) |  |
| `joyname` | `joystick` |  | A name the player gives the joystick |  |
| `joypitchsensitivity` | `1.0` |  | Scale of the look up/down axis |  |
| `joypitchthreshold` | `0.15` |  | Dead zone of that axis |  |
| `joysidesensitivity` | `-1.0` |  | Scale of the strafe axis |  |
| `joysidethreshold` | `0.15` |  | Dead zone of the strafe axis |  |
| `joystick` | `0` | 0, 1 | 1: use a joystick |  |
| `joyyawsensitivity` | `-1.0` |  | Scale of the turn axis |  |
| `joyyawthreshold` | `0.15` |  | Dead zone of the turn axis |  |
| `lookspring` | `0` | 0, 1 | 1: the view recentres when mouse look is released |  |
| `lookstrafe` | `0` |  | 1: mouse left/right strafes while mouse look is on |  |
| `m_accel` | `0` | 0–1 step 0.1 | Mouse acceleration: extra gain proportional to mouse speed; 0 off | menu |
| `m_accel_offset` | `0` |  | Speed (counts per millisecond) the mouse must pass before acceleration starts |  |
| `m_accel_power` | `2` |  | Exponent of the acceleration curve (2 linear) |  |
| `m_accel_senscap` | `0` |  | Upper limit of the accelerated sensitivity; 0 none |  |
| `m_filter` | `0` |  | 1: average each mouse sample with the previous one |  |
| `m_forward` | `1` |  | Scale of mouse forward/back motion when it moves the player |  |
| `m_pitch` | `0.022` |  | Degrees of pitch per mouse count (negative inverts) | menu |
| `m_side` | `0.8` |  | Scale of mouse sideways motion when it strafes |  |
| `m_yaw` | `0.022` |  | Degrees of yaw per mouse count | menu |
| `sensitivity` | `12` | 1–20 step 0.25 | Mouse sensitivity, multiplies m_yaw and m_pitch | menu |

### 1.2 Movement, weapons and the player's own buttons (20 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `allow_scripts` | `2` | 0–2 | What movement scripting the client allows: 0 none, 1 simple, 2 all; announced to others; locked during a match | menu rules ro |
| `b_switch` | (empty) | 1–8 | Highest weapon number the server may switch you to when you pick up a backpack [userinfo] | menu info |
| `cl_anglespeedkey` | `1.5` |  | Turn-speed multiplier while +speed is held |  |
| `cl_backspeed` | `400` |  | Backward speed the client asks for; the server caps it | menu |
| `cl_forwardspeed` | `400` |  | Forward speed the client asks for; the server caps it | menu |
| `cl_iDrive` | `0` | 0, 1 | 1: holding both strafe keys moves toward the last pressed instead of stopping; announced; locked during a match | rules |
| `cl_movespeedkey` | `2.0` |  | Move-speed multiplier while +speed is held |  |
| `cl_nodelta` | `0` | 0, 1 | 1: ask the server for full (undeltaed) updates |  |
| `cl_pitchspeed` | `150` |  | Degrees per second of +lookup / +lookdown |  |
| `cl_safestrafe` | `0` |  | Minimum frames between opposite strafe changes, enforced locally (0 off) | rules |
| `cl_safestrafe_print` | `0` |  | 1: print the frames between left/right strafe changes |  |
| `cl_sidespeed` | `400` |  | Strafe speed the client asks for; the server caps it | menu |
| `cl_smartjump` | `1` | 0, 1 | 1: +jump swims up in liquids and rises as a spectator | menu |
| `cl_upspeed` | `400` |  | Up/down speed in liquids or as a spectator |  |
| `cl_weaponforgetorder` | `0` | 0, 1 | 0: a weapon command remembers its list as the preferred order; 1: it picks the best at the moment and forgets the list |  |
| `cl_weaponhide` | `0` | 0–2 | On releasing attack, switch back to the shotgun (or axe): 0 off, 1 on, 2 only in deathmatch mode 1 | menu |
| `cl_weaponhide_axe` | `0` | 0, 1 | 1: the dummy weapon is the axe instead of the shotgun |  |
| `cl_weaponpreselect` | `0` | 0–4 | With the weapon command: 0 switch at once; 1 switch when attack is next pressed; 2 like 1 but switch at once while attack is held; 3 and 4 like 1 and 2 only in deathmatch mode 1 | menu |
| `cl_yawspeed` | `140` |  | Degrees per second of +left / +right |  |
| `w_switch` | (empty) | 1–8 | Highest weapon number the server may switch you to when you pick a weapon up [userinfo] | menu info |

### 1.3 View: field of view, bob, roll, kick, tints (45 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_bob` | `0` |  | How far the view model (or head) bobs while walking |  |
| `cl_bobcycle` | `0.0` |  | Length in seconds of one bob cycle |  |
| `cl_bobhead` | `0` | 0, 1 | 1: the bob moves the camera instead of the weapon |  |
| `cl_bobup` | `0.0` |  | Fraction of the cycle spent rising |  |
| `cl_bonusflash` | `0` | 0, 1 | 1: flash the screen on item pickup | menu |
| `cl_rollalpha` | `20` |  | How fast the strafe roll builds; locked to 20 by several rulesets | rules |
| `cl_rollangle` | `0` | 0–10 step 1 | Degrees the view rolls when strafing; capped by rulesets | menu |
| `cl_rollspeed` | `200` | 0–30 step 2 | Speed at which roll reaches its full angle | menu |
| `default_fov` | `90` |  | Field of view to restore when a server sets fov back to 90 (0 off) |  |
| `fov` | `90` | 40–140 step 2 | Field of view in degrees | menu |
| `gl_contrast` | `1.0` | 1–5 step 0.1 | Screen contrast | menu |
| `gl_cshiftpercent` | `100` |  | Strength of screen tints in percent |  |
| `gl_gamma` | `1.0` | 0.3–3.0 step 0.1 | Screen gamma (applied in the palette or in post-processing, see vid_software_palette) | menu |
| `r_drawviewmodel` | `1` | 0–1 step 0.05 | Opacity of the held weapon (0 hidden, 1 solid) | menu |
| `r_drawviewmodel_invisible` | `0` | 0, 1 | 1: draw the weapon while you carry the ring |  |
| `r_nearclip` | `2` |  | Near clipping distance |  |
| `r_viewmodellastfired` | `0` | 0, 1 | 1: in demos show the last weapon fired, not the one held |  |
| `r_viewmodeloffset` | (empty) | -10–10 step 1 | Shifts the held weapon sideways (and up/forward with three numbers) | menu |
| `r_viewmodelSize` | `1` | 0.1–1 step 0.05 | Scale of the held weapon | menu |
| `r_viewpreselgun` | `0` | 0, 1 | 1: show the weapon chosen by preselection |  |
| `scr_fovmode` | `0` | 0, 1 | How the field of view keeps its aspect when the view is shrunk |  |
| `v_centermove` | `0.15` |  | Distance moved before the view recentres (with lookspring) |  |
| `v_centerspeed` | `500` |  | Speed of that recentring |  |
| `v_contentblend` | `0.2` |  | Tint strength inside water, slime and lava |  |
| `v_damagecshift` | `0.2` | 0–1 step 0.1 | Red flash strength when hurt, 0 to 1 | menu |
| `v_dlightcolor` | `1` | 0, 1 | 1: light near you tints the view in its own colour |  |
| `v_dlightcshift` | `1` | 0–2 | Dynamic lights tint the view: 0 off, 1 on, 2 on except lights around players |  |
| `v_dlightcshiftpercent` | `0.5` |  | Strength of that tint |  |
| `v_gunkick` | `0` |  | Weapon kick when firing (0 off) | menu |
| `v_idlescale` | `0` |  | Strength of the idle sway |  |
| `v_ipitch_cycle` | `1` |  | Speed of the idle pitch sway |  |
| `v_ipitch_level` | `0.3` |  | Size of the idle pitch sway |  |
| `v_iroll_cycle` | `0.5` |  | Speed of the idle roll sway |  |
| `v_iroll_level` | `0.1` |  | Size of the idle roll sway |  |
| `v_iyaw_cycle` | `2` |  | Speed of the idle yaw sway |  |
| `v_iyaw_level` | `0.3` |  | Size of the idle yaw sway |  |
| `v_kickpitch` | `0.0` | 0–10 step 0.5 | Pitch kick when hit | menu |
| `v_kickroll` | `0.0` | 0–10 step 0.5 | Roll kick when hit | menu |
| `v_kicktime` | `0.0` | 0–10 step 0.5 | Duration of the hit kick | menu |
| `v_pentcshift` | `0.5` |  | Tint strength with the pentagram |  |
| `v_quadcshift` | `0.5` |  | Tint strength with quad damage |  |
| `v_ringcshift` | `0.5` |  | Tint strength with the ring |  |
| `v_suitcshift` | `0.5` |  | Tint strength with the biosuit |  |
| `v_viewheight` | `0` | -7–4 step 0.5 | Raises or lowers the eye (only within the classic small range) | menu |
| `viewsize` | `100` | 30–120 step 5 | Size of the 3D view (30 to 120; above 100 hides parts of the HUD) | menu |

### 1.4 Crosshair (9 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_crossx` | `0` |  | Horizontal offset of the crosshair in pixels |  |
| `cl_crossy` | `0` |  | Vertical offset of the crosshair in pixels |  |
| `crosshair` | `4` | 0–7 step 1 | Crosshair shape: 0 off, 1 the plus character, 2 the classic cross, 3 small cross, 4 dot, 5 and 6 other shapes, 7 like 6 | menu |
| `crosshairalpha` | `1` | 0.1–1 step 0.1 | Crosshair opacity, 0 to 1 | menu |
| `crosshaircolor` | `255 0 0` |  | Crosshair colour as R G B |  |
| `crosshairimage` | (empty) |  | Picture file used as the crosshair instead of the built-in ones |  |
| `crosshairscale` | `0` | 0–5 | Resolution of the built-in crosshairs: 0 8x8, 1 16x16, ... 5 256x256 |  |
| `crosshairscalemethod` | `0` | 0, 1 | How a crosshair picture is scaled (the values are not settled by the reference read) |  |
| `crosshairsize` | `1` | 0.2–3 step 0.2 | Crosshair size multiplier | menu |

### 1.5 HUD: classic status bar, element HUD globals, tracker, team info, autoid (160 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_clock` | `0` | 0–2 | Classic clock: 0 off, 1 time on the server, 2 time of day | menu |
| `cl_clock_format` | `0` | 0–3 | Time-of-day format: 0 24 h with seconds, 1 12 h, 2 12 h with seconds, 3 24 h |  |
| `cl_clock_x` | `0` |  | Clock column |  |
| `cl_clock_y` | `-1` |  | Clock row (negative counts from the bottom) |  |
| `cl_gameclock` | `0` | 0–4 | Match clock: 0 off, 1 counts up, 2 counts down, 3 and 4 the same as minutes:seconds | menu |
| `cl_gameclock_offset` | `0` |  | Seconds added for mods that do not synchronise the match clock |  |
| `cl_gameclock_style` | `0` | 0–3 | Match clock colour: 0 white, 1 red, 2 and 3 green |  |
| `cl_gameclock_x` | `0` |  | Match clock column |  |
| `cl_gameclock_y` | `-3` |  | Match clock row |  |
| `cl_hud` | `1` | 0, 1 | 1: allow the string HUD; locked off by several rulesets | rules |
| `cl_hudswap` | `0` | 0, 1 | 1: inventory on the left | menu |
| `cl_sbar` | `0` | 0, 1 | 1: classic opaque status bar | menu |
| `hud_digits_trim` | `1` | 0–2 | A number wider than its digits: 0 shows nines, 1 keeps the low digits, 2 keeps the high digits |  |
| `hud_editor_allowalign` | `1` | 0, 1 | 1: the HUD editor may change alignment |  |
| `hud_editor_allowmove` | `1` | 0, 1 | 1: the HUD editor may move elements |  |
| `hud_editor_allowplace` | `1` | 0, 1 | 1: the HUD editor may re-anchor elements |  |
| `hud_editor_allowresize` | `1` | 0, 1 | 1: the HUD editor may resize elements |  |
| `hud_fps_min_reset_interval` | `30` |  | Seconds before the lowest-fps reading resets |  |
| `hud_frametime_max_reset_interval` | `30` |  | Seconds before the worst-frame-time reading resets |  |
| `hud_name_remove_prefixes` | (empty) |  | Prefixes cut from names in the tracker and team info |  |
| `hud_performance_average` | `1` |  | 1: the fps and frame-time readings use the windowed minimum/maximum logic |  |
| `hud_planmode` | `0` | 0, 1 | 1: show every element for editing; never saved | nosave |
| `hud_sortrules_includeself` | `1` | 0–2 | You: 0 sorted normally, 1 always first, 2 first or second |  |
| `hud_sortrules_playersort` | `1` | 0–3 | Player order: 0 name; 1 frags; 2 team; 3 team then frags |  |
| `hud_sortrules_teamsort` | `0` | 0, 1 | How teams are sorted (0 or 1) |  |
| `hud_tp_need` | `0` | 0, 1 | 1: elements colour low values by the tp_need settings |  |
| `r_chaticons_alpha` | `0.8` |  | Opacity of chat and away icons over players |  |
| `r_drawhud` | `1` |  | 1: draw the HUD |  |
| `r_tracker` | `1` | 0, 1 | 1: the frag tracker on screen |  |
| `r_tracker_align_right` | `1` | 0, 1 | 1: tracker aligned right | menu |
| `r_tracker_color_bad` | `900` |  | Colour of an enemy team's frag (three digits 0 to 9) |  |
| `r_tracker_color_fragonme` | `900` |  | Colour of a frag on you |  |
| `r_tracker_color_good` | `090` |  | Colour of your team's frag |  |
| `r_tracker_color_myfrag` | `090` |  | Colour of your frag |  |
| `r_tracker_color_quadfrag` | (empty) |  | Colour of a quad killer's name |  |
| `r_tracker_color_quadfrag_colorfix` | `0` | 0, 1 | 1: the quad colour applies to both names |  |
| `r_tracker_color_quadfrag_noicon` | `0` | 0, 1 | 1: hide the quad icon when the quad colour is used |  |
| `r_tracker_color_suicide` | `900` |  | Colour of a suicide |  |
| `r_tracker_color_tkbad` | `009` |  | Colour of your team's teamkill |  |
| `r_tracker_color_tkgood` | `990` |  | Colour of an enemy's teamkill |  |
| `r_tracker_colorfix` | `0` |  | 1: colour both names in a line |  |
| `r_tracker_flags` | `0` | 0, 1 | 1: count flag takes, captures and drops | menu |
| `r_tracker_frags` | `1` | 0–2 | Which frags it shows: 0 none, 1 all, 2 only your own | menu |
| `r_tracker_frame_color` | `0 0 0 0` |  | Background colour and opacity |  |
| `r_tracker_images_scale` | `1` |  | Size of weapon pictures |  |
| `r_tracker_inconsole` | `0` | 0–3 | Tracker lines also in the console: 0 no; 1 to 3 console with notify-line variants |  |
| `r_tracker_inconsole_colored_weapon` | `0` | 0, 1 | 1: coloured weapon names in console lines |  |
| `r_tracker_messages` | `20` | 0–10 step 1 | Most lines shown at once | menu |
| `r_tracker_name_width` | `0` |  | Longest name shown (0 no limit) |  |
| `r_tracker_own_frag_prefix` | `You fragged ` |  | Text before lines about your own frags |  |
| `r_tracker_pickups` | `0` | 0, 1 | 1: also list item pickups |  |
| `r_tracker_positive_enemy_suicide` | `0` |  | 1: enemy suicides drawn in the good colour |  |
| `r_tracker_positive_enemy_vs_enemy` | `0` |  | 1: enemy-on-enemy frags drawn in the good colour |  |
| `r_tracker_proportional` | `0` | 0, 1 | 1: TrueType font |  |
| `r_tracker_reverse` | `0` | 0, 1 | 1: newest line on top |  |
| `r_tracker_row_spacing` | `0` |  | Extra pixels between lines |  |
| `r_tracker_scale` | `1` | 0.1–2 step 0.1 | Text size | menu |
| `r_tracker_streaks` | `0` |  | 1: announce kill streaks | menu |
| `r_tracker_string_died` | ` (died)` |  | Text for a death by the world |  |
| `r_tracker_string_enemy` | `enemy` |  | Word for an enemy |  |
| `r_tracker_string_inconsole_prefix` | (empty) |  | Prefix of tracker lines in the console |  |
| `r_tracker_string_suicides` | ` (suicides)` |  | Text for a suicide |  |
| `r_tracker_string_teammate` | `teammate` |  | Word for a teammate |  |
| `r_tracker_time` | `4` | 0.5–6 step 0.5 | Seconds a line stays | menu |
| `r_tracker_weapon_first` | `0` | 0, 1 | 1: weapon before the names |  |
| `r_tracker_x` | `0` |  | Tracker column |  |
| `r_tracker_y` | `0` |  | Tracker row |  |
| `scr_autoid` | `5` | 0–5 | Info over players when spectating: 0 off, 1 name, 2 plus bars, 3 plus armour type, 4 plus a rocket-launcher mark, 5 plus best weapon | menu |
| `scr_autoid_armorbar_green_armor` | `25 170 0 255` |  | Armour bar colour for green armour |  |
| `scr_autoid_armorbar_red_armor` | `255 0 0 255` |  | Armour bar colour for red armour |  |
| `scr_autoid_armorbar_yellow_armor` | `255 220 0 255` |  | Armour bar colour for yellow armour |  |
| `scr_autoid_barlength` | `16` |  | Bar length |  |
| `scr_autoid_healthbar_bg_color` | `180 115 115 100` |  | Health bar background |  |
| `scr_autoid_healthbar_mega_color` | `255 0 0 255` |  | Health bar colour above 100 |  |
| `scr_autoid_healthbar_normal_color` | `80 0 0 255` |  | Health bar colour |  |
| `scr_autoid_healthbar_two_mega_color` | `255 100 0 255` |  | Health bar colour above 200 |  |
| `scr_autoid_healthbar_unnatural_color` | `255 255 255 255` |  | Health bar colour for impossible values |  |
| `scr_autoid_namelength` | `0` |  | Longest name (0 no limit) |  |
| `scr_autoid_proportional` | `0` | 0, 1 | 1: TrueType font |  |
| `scr_autoid_scale` | `1` |  | Size |  |
| `scr_autoid_weaponicon` | `1` | 0, 1 | 1: weapon as an icon |  |
| `scr_autoid_weapons` | `2` | 1, 2, 4, 8, 16, 32, 64 | Lowest weapon (bit value) shown at the highest detail level |  |
| `scr_centerSbar` | `1` | 0, 1 | 1: centre the classic status bar |  |
| `scr_centershift` | `0` |  | Moves centre prints up or down |  |
| `scr_centerspeed` | `8` |  | Characters per second of the end-of-episode text |  |
| `scr_centertime` | `2` | 0–2 | Seconds a centre print stays |  |
| `scr_coloredfrags` | `0` | 0, 1 | 1: colour names in frag messages by team/enemy colours |  |
| `scr_coloredfrags_enemy` | (empty) |  | Enemy name colour in those messages |  |
| `scr_coloredfrags_normalize` | `0` | 0, 1 | 1: translate names to plain characters in those messages |  |
| `scr_coloredfrags_team` | (empty) |  | Team name colour in those messages |  |
| `scr_coloredText` | `1` | 0–2 | Colour codes in text: 0 off, 1 on, 2 (variant) | menu |
| `scr_compactHud` | `0` | 0–4 | Compact classic HUD: 0 off, 1 to 4 layouts |  |
| `scr_compactHudAlign` | `0` | 0, 1 | 1: compact HUD numbers aligned right |  |
| `scr_damage_floating` | `0` |  | 1: floating damage numbers where hits land |  |
| `scr_damage_hitbeep` | `0` |  | 1: a beep each time you deal damage |  |
| `scr_damage_offset_ingame` | `14` |  | Height offset of damage numbers when playing |  |
| `scr_damage_offset_spectator` | `28` |  | Height offset of damage numbers when spectating |  |
| `scr_damage_proportional` | `0` | 0, 1 | 1: TrueType font for damage numbers |  |
| `scr_damage_scale` | `1` |  | Size of damage numbers |  |
| `scr_drawHFrags` | `1` | 0–3 | Frag cells across the classic bar (0 to 3) |  |
| `scr_drawVFrags` | `1` | 0–2 | Frag column at the right of the classic bar (0 to 2) |  |
| `scr_newhud` | `0` | 0–2 | 0 classic status bar, 1 element HUD, 2 both | menu |
| `scr_notifyalways` | `0` | 0, 1 | 1: notify lines show during intermission too |  |
| `scr_qtvbuffer` | `0` | 0, 1 | 1: show the broadcast buffer |  |
| `scr_qtvbuffer_x` | `0` |  | Its column |  |
| `scr_qtvbuffer_y` | `-10` |  | Its row |  |
| `scr_sbar_drawammo` | `1` | 0, 1 | 1: classic bar draws ammo |  |
| `scr_sbar_drawammocounts` | `1` | 0, 1 | 1: draws the ammo counts |  |
| `scr_sbar_drawammoicon` | `1` | 0, 1 | 1: draws the ammo icon |  |
| `scr_sbar_drawarmor` | `1` | 0, 1 | 1: draws armour |  |
| `scr_sbar_drawarmor666` | `1` | 0, 1 | 1: armour reads 666 with the pentagram |  |
| `scr_sbar_drawarmoricon` | `1` | 0, 1 | 1: draws the armour icon |  |
| `scr_sbar_drawfaceicon` | `1` | 0, 1 | 1: draws the face |  |
| `scr_sbar_drawguns` | `1` | 0, 1 | 1: draws the weapons row |  |
| `scr_sbar_drawhealth` | `1` | 0, 1 | 1: draws health |  |
| `scr_sbar_drawitems` | `1` | 0, 1 | 1: draws keys and powerups |  |
| `scr_sbar_drawsigils` | `1` | 0, 1 | 1: draws the sigils |  |
| `scr_sbar_lowammo` | `5` |  | Ammo count below which the number turns to the warning colour |  |
| `scr_showcrosshair` | `1` | 0, 1 | 1: crosshair also in menus and on the scoreboard |  |
| `scr_shownick_frame_color` | `10 0 0 120` |  | Frame colour of the name label a server shows on request |  |
| `scr_shownick_name_width` | `6` |  | Name width in that label |  |
| `scr_shownick_order` | `%p%n %a/%H %w` |  | Format of that label |  |
| `scr_shownick_proportional` | `0` | 0, 1 | 1: TrueType font |  |
| `scr_shownick_scale` | `1` |  | Size |  |
| `scr_shownick_show_ammo` | `0` | 0, 1 | 1: ammo beside the weapon |  |
| `scr_shownick_time` | `0.8` |  | Seconds it stays |  |
| `scr_shownick_x` | `0` |  | Column |  |
| `scr_shownick_y` | `0` |  | Row |  |
| `scr_spectatorMessage` | `1` | 0, 1 | 1: the hint line at the bottom in free spectator mode |  |
| `scr_teaminfo` | `1` | 0, 1 | Team info table: 0 off, 1 when spectating or in demos | menu |
| `scr_teaminfo_align_right` | `1` | 0, 1 | 1: aligned right |  |
| `scr_teaminfo_armor_style` | `3` | 0–4 | Armour: 0 hidden, 1 white with icon, 2 white, 3 in its colour, 4 white with a letter |  |
| `scr_teaminfo_flag_style` | `1` | 1, 2 | Flag shown as 1 icon, 2 coloured text |  |
| `scr_teaminfo_frame_color` | `10 0 0 120` |  | Frame colour |  |
| `scr_teaminfo_loc_width` | `5` |  | Location width |  |
| `scr_teaminfo_low_health` | `25` |  | Health at or below which it turns red |  |
| `scr_teaminfo_name_width` | `6` |  | Name width |  |
| `scr_teaminfo_order` | `%p%n $x10%l$x11 %a/%H %w` |  | Row format |  |
| `scr_teaminfo_powerup_style` | `1` | 1–3 | Powerup style (1 to 3) |  |
| `scr_teaminfo_proportional` | `0` | 0, 1 | 1: TrueType font |  |
| `scr_teaminfo_scale` | `1` |  | Size |  |
| `scr_teaminfo_show_ammo` | `0` | 0, 1 | 1: ammo beside the weapon |  |
| `scr_teaminfo_show_countdown` | `1` | 0, 1 | 1: respawn times instead of powerups in wipeout mode |  |
| `scr_teaminfo_show_enemies` | `0` | 0, 1 | 1: also enemies |  |
| `scr_teaminfo_show_self` | `2` | 0–2 | Tracked player's own row: 0 never, 1 always, 2 only in demos and broadcasts |  |
| `scr_teaminfo_weapon_style` | `1` | 0, 1 | Weapon as 0 icon, 1 word |  |
| `scr_teaminfo_x` | `0` |  | Column |  |
| `scr_teaminfo_y` | `0` |  | Row |  |
| `scr_tracking` | `Tracking: %t %n, JUMP for next` (the letters in the coloured half) |  | Format of the tracking line when spectating |  |
| `show_fps` | `0` | 0, 1 | 1: show the frame rate (classic display) | menu |
| `show_fps_x` | `-5` |  | Its column |  |
| `show_fps_y` | `-1` |  | Its row |  |
| `show_speed` | `0` | 0–2 | Speed readout: 0 off, 1 measured, 2 predicted |  |
| `show_speed_x` | `-1` |  | Its column |  |
| `show_speed_y` | `1` |  | Its row |  |
| `show_velocity_3d` | `0` | 0–2 | Velocity arrow: 0 off, 1 full with projections, 2 horizontal only |  |
| `show_velocity_3d_offset_down` | `5` |  | How far below the eye the arrow is drawn |  |
| `show_velocity_3d_offset_forward` | `2.5` |  | How far ahead of the eye the arrow is drawn |  |
| `showpause` | `1` | 0, 1 | 1: pause picture while paused |  |
| `showturtle` | `0` |  | Frame rate below which a turtle icon shows (0 off) |  |

### 1.6 Scoreboard (27 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `scr_scoreboard_afk` | `1` | 0, 1 | 1: mark players whose game is not focused |  |
| `scr_scoreboard_afk_style` | `1` | 1, 2 | 1 time column reads AFK, 2 time shown in red |  |
| `scr_scoreboard_borderless` | `1` | 0, 1 | 1: no border round the scoreboard |  |
| `scr_scoreboard_centered` | `1` | 0, 1 | 1: centre the scoreboard horizontally |  |
| `scr_scoreboard_classic` | `0` | 0–2 | 0 modern look, 1 classic look, 2 the original look |  |
| `scr_scoreboard_drawtitle` | `1` | 0, 1 | 1: draw the title row |  |
| `scr_scoreboard_fadescreen` | `0` |  | How much the screen darkens behind the scoreboard |  |
| `scr_scoreboard_fillalpha` | `0.7` |  | Opacity of the colour bars |  |
| `scr_scoreboard_fillcolored` | `2` |  | How rows are coloured by player colours (0 to 2) |  |
| `scr_scoreboard_forcecolors` | `1` | 0, 1 | 1: rows use the forced team/enemy colours |  |
| `scr_scoreboard_highlightself` | `1` | 0, 1 | 1: highlight your own row |  |
| `scr_scoreboard_login_color` | `255 255 192` |  | Tint of the login-name column |  |
| `scr_scoreboard_login_flagfile` | `flags` |  | Picture holding country flags for logged-in players |  |
| `scr_scoreboard_login_indicator` | `&cffc*&r` |  | Mark beside logged-in players without a flag |  |
| `scr_scoreboard_login_names` | `1` |  | 1: show the login-name column |  |
| `scr_scoreboard_posx` | `0` |  | Horizontal offset |  |
| `scr_scoreboard_posy` | `0` |  | Vertical offset |  |
| `scr_scoreboard_proportional` | `0` | 0, 1 | 1: TrueType font |  |
| `scr_scoreboard_qtv_name` | `qtv` (the letters in the coloured half) |  | What broadcast viewers are called (default: qtv in the coloured half) |  |
| `scr_scoreboard_showclock` | `0` | 0, 1 | 1: show the clock |  |
| `scr_scoreboard_showflagstats` | `0` | 0, 1 | 1: show flag statistics |  |
| `scr_scoreboard_showfrags` | `1` | 0, 1 | 1: show the frags column |  |
| `scr_scoreboard_showmapname` | `0` | 0–3 | Map name in the title: 0 off, 1 long and short, 2 long, 3 short |  |
| `scr_scoreboard_showqtvusers` | `1` | 0, 1 | 1: list broadcast viewers (sets the qul info key) |  |
| `scr_scoreboard_spectator_name` | `spectator` (the letters in the coloured half) |  | What spectators are called (default: spectator in the coloured half) |  |
| `scr_scoreboard_teamsort` | `1` | 0, 1 | 1: sort by team, 0 by frags |  |
| `scr_scoreboard_wipeout` | `1` | 0, 1 | 1: a mode-specific scoreboard in the wipeout arena mode |  |

### 1.7 Console and chat (54 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_chatmode` | `2` | 0–2 | Console input: 0 always commands; 1 always chat; 2 run it if the first word is a command, else chat | menu |
| `cl_floodprot` | `0` | 0, 1 | 1: refuse to send chat faster than cl_fp_messages per cl_fp_persecond (prints a refusal) |  |
| `cl_fp_messages` | `4` |  | Chat lines allowed in the flood window |  |
| `cl_fp_persecond` | `4` |  | Length of the flood window in seconds |  |
| `cl_parseWhiteText` | `1` | 0–2 | Braces in chat turn text white (0 to 2) |  |
| `cl_savehistory` | `1` | 0, 1 | 1: keep the console's command history in a file between sessions |  |
| `cl_sayfilter_coloredtext` | `0` | 0–2 | Remove colour codes from your own outgoing chat: 0 off, 1 colours, 2 colours and white | menu |
| `cl_sayfilter_sendboth` | `0` | 0, 1 | 1: send team chat twice, coloured and plain, marked for clients that filter | menu |
| `cl_textencoding` | `0` | 0–2 | Encoding of non-classic characters in outgoing chat: 0 Cyrillic 8-bit, 1 UTF-8, 2 an escape form |  |
| `con_bindphysical` | `0` | 0, 1 | 0: binds follow the keyboard layout's characters; 1: physical key positions; never saved | nosave |
| `con_clearnotify` | `1` | 0, 1 | 1: clear the notify lines when the console closes |  |
| `con_completion_changed_mark` | `1` | 0, 1 | 1: mark changed settings with an asterisk in completion lists |  |
| `con_completion_color_changed_mark` | `f30` |  | Colour of that mark (three hex digits) |  |
| `con_completion_color_colon` | `fff` |  | Colour of the colon in completion lists |  |
| `con_completion_color_name` | `8ff` |  | Colour of names in completion lists |  |
| `con_completion_color_quotes_current` | `ff8` |  | Colour of the quotes round current values |  |
| `con_completion_color_quotes_default` | `ff8` |  | Colour of the quotes round default values |  |
| `con_completion_color_title` | `ff3` |  | Colour of completion section titles |  |
| `con_completion_color_value_current` | `fff` |  | Colour of current values |  |
| `con_completion_color_value_default` | `fff` |  | Colour of default values |  |
| `con_completion_format` | `2` | 0–5 | Completion list layout: 0 old; 1 with current and default values; 2 current only; 3 default only; 4 both when they differ; 5 no values | menu |
| `con_completion_padding` | `2` |  | Spaces between completion columns |  |
| `con_deadkey` | `1` | 0, 1 | 1: the console key is also a dead key; send a backspace after it (Windows only) | win |
| `con_fragmessages` | `1` | 0–2 | Frag messages: 0 not printed, 1 console and notify lines, 2 console only |  |
| `con_funchars_mode` | `0` | 0, 1 | 1: typing with Ctrl gives coloured characters | menu |
| `con_hide_chat_input` | `1` | 0, 1 | 1: do not echo your own chat input into the console |  |
| `con_highlight` | `0` | 0–3 | Highlight lines naming you: 0 off, 1 whole line white, 2 prefix with con_highlight_mark, 3 both |  |
| `con_highlight_mark` | (empty) |  | Text marking highlighted lines in styles 2 and 3 |  |
| `con_margin_left` | `0` |  | Spaces of left margin in the console |  |
| `con_mm2_only` | `0` | 0, 1 | 1: notify lines show only team chat |  |
| `con_notify` | `1` | 0, 1 | 1: show the notify lines |  |
| `con_notifylines` | `4` | 0–16 step 1 | Number of notify lines (up to 16) | menu |
| `con_notifytime` | `3` | 0.5–16 step 0.5 | Seconds a notify line stays | menu |
| `con_prompt_charcode` | `93` |  | Character code of the console prompt |  |
| `con_proportional` | `0` | 0, 1 | 1: console text in the TrueType font |  |
| `con_shift` | `-10` |  | Vertical offset of the console background |  |
| `con_tilde_mode` | `0` | 0–2 | 0 tilde always toggles the console; 1 tilde types its character in input lines; 2 like 1 but closes an empty console |  |
| `con_timestamps` | `0` | 0–2 | Time stamps before chat lines: 0 off, 1 hours and minutes, 2 with seconds | menu |
| `con_wordwrap` | `1` | 0, 1 | 1: wrap console lines at word boundaries |  |
| `ignore_flood` | `0` | 0–2 | Drop repeated lines: 0 off, 1 everyone and spectator chat, 2 team chat too | menu |
| `ignore_flood_duration` | `4` | 0–10 step 1 | Seconds a line counts as repeated | menu |
| `ignore_mode` | `0` | 0, 1 | 1: also drop team chat from ignored players |  |
| `ignore_no_weapon` | `0` | 0, 1 | 1: hide the server's no-weapon message |  |
| `ignore_not_enough_ammo` | `0` | 0, 1 | 1: hide the server's not-enough-ammo message |  |
| `ignore_opponents` | `0` | 0–2 | Drop opponents' chat: 0 off, 1 always, 2 during a match | menu |
| `ignore_qizmo_spec` | `0` | 0, 1 | 1: drop chat from spectators on the classic proxy | menu |
| `ignore_qtv` | `0` | 0, 1 | 1: drop chat from broadcast viewers |  |
| `ignore_spec` | `0` | 0–2 | Drop spectator chat: 0 off, 1 unless you spectate, 2 always | menu |
| `msg_filter` | `0` | 0–3 | Hide chat by kind: 0 none, 1 everyone and spectator chat, 2 team chat, 3 all | menu |
| `scr_conalpha` | `0.8` | 0–1 step 0.1 | Opacity of the console background | menu |
| `scr_conback` | `1` | 0–2 | Console background: 0 the picture, 1 the map preview while loading, 2 always the map preview | menu |
| `scr_conpicture` | `conback` |  | Picture used as the console background |  |
| `scr_consize` | `0.5` | 0–1 step 0.1 | Console height as a fraction of the screen | menu |
| `scr_conspeed` | `9999` | 1000–9999 step 1000 | Speed the console slides | menu |

### 1.8 Network and prediction (66 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_allow_downloads` | `bsp,lmp,loc,mdl,mvd,pcx,spr,wad,wav` |  | File extensions the client accepts from servers | ro |
| `cl_allow_uploads` | `0` | 0, 1 | 1: allow uploads to the server | ro |
| `cl_c2sdupe` | `0` |  | 1: send each movement packet twice |  |
| `cl_c2sImpulseBackup` | `3` |  | How many extra copies of a packet carrying an impulse to send | menu |
| `cl_c2spps` | `0` |  | Cap on movement packets per second sent to the server (0 no cap); forced to 0 by rulesets with independent physics | rules |
| `cl_chunksperframe` | `30` |  | Download chunks requested per frame with chunked downloads |  |
| `cl_crypt_rcon` | `1` | 0, 1 | 1: send remote-console commands with a time-stamped hash instead of the password in clear | ro |
| `cl_debug_antilag_send` | `0` | 0, 1 | 1: send opponents' positions for the server to store |  |
| `cl_delay_packet` | `0` |  | Adds this many ms to your ping (0 to twice the cap); announced; refused during a match; never saved | ro nosave |
| `cl_delay_packet_deviation` | `0` |  | Random variation added to that delay, in ms | ro |
| `cl_delay_packet_target` | `0` |  | Delay to reach this ping instead of adding a fixed amount, in ms | ro |
| `cl_earlypackets` | `1` | 0, 1 | 1: read network packets every rendered frame, not only on physics frames | menu |
| `cl_independentPhysics` | `1` | 0, 1 | 1: render frames independently of the physics/network rate; change only while disconnected | rules |
| `cl_lerp_monsters` | `1` | 0, 1 | 1: interpolate monster movement |  |
| `cl_maxfps` | `0` |  | Frame rate cap while connected (0: no cap); rulesets cap the physics rate at 77 | menu rules |
| `cl_maxfps_menu` | `0` |  | Frame rate cap while disconnected (minimum 30; 0 no cap) |  |
| `cl_net_clientport` | `27001` |  | UDP port the client opens |  |
| `cl_nolerp` | `0` | 0, 1 | 1: do not interpolate entity movement | menu |
| `cl_nolerp_on_entity` | `0` | 0, 1 | 1: no interpolation while standing on a moving entity |  |
| `cl_nopred` | `0` | 0, 1 | 1: no prediction of your own movement (debugging) |  |
| `cl_nopred_weapon` | `0` | 0, 1 | 1: no prediction of your own weapon firing |  |
| `cl_pext` | `1` | 0, 1 | 1: offer the protocol extensions to servers | ro |
| `cl_pext_256packetentities` | `1` | 0, 1 | 1: offer the extension for 256 entities per packet | ro |
| `cl_pext_alpha` | `1` | 0, 1 | 1: offer the per-entity transparency extension | ro |
| `cl_pext_chunkeddownloads` | `1` | 0, 1 | 1: offer chunked (faster) downloads | ro |
| `cl_pext_colourmod` | `1` | 0, 1 | 1: offer the per-entity colour extension | ro |
| `cl_pext_floatcoords` | `1` | 0, 1 | 1: offer full-precision coordinates | ro |
| `cl_pext_lagteleport` | `1` | 0, 1 | 1: offer the fix that turns movement through a teleporter for high-ping players | ro |
| `cl_pext_limits` | `1` | 0, 1 | 1: offer the raised entity and model limits | ro |
| `cl_pext_other` | `0` | 0, 1 | 1: offer the other extensions too | ro |
| `cl_pext_replacementdeltas` | `1` | 0, 1 | 1: offer the replacement-delta entity encoding | ro |
| `cl_pext_simpleprojectiles` | `1` | 0, 1 | 1: offer simple projectiles (nails and others sent as start and speed) | ro |
| `cl_pext_warndemos` | `1` | 0, 1 | 1: warn when recording a demo older clients cannot play | ro |
| `cl_pext_weaponprediction` | `1` | 0, 1 | 1: offer weapon prediction | ro |
| `cl_physfps` | `0` |  | Physics (packet) rate with independent physics (0: the server's maxfps) |  |
| `cl_physfps_spectator` | `77` |  | Physics rate while spectating |  |
| `cl_portpingprobe_delay` | `0` |  | Ms between port probes when connecting |  |
| `cl_portpingprobe_enable` | `0` | 0, 1 | 1: probe several ports when connecting (finds a better route) |  |
| `cl_portpingprobe_port_probes` | `1` |  | Probes per port |  |
| `cl_portpingprobe_probes` | `500` |  | Total probes |  |
| `cl_predict_beam` | `1` | 0, 1 | 1: draw your own lightning beam from the predicted state |  |
| `cl_predict_buffer` | `1` |  | Frames predicted weapon effects wait before playing |  |
| `cl_predict_half` | `0` | 0, 1 | 1: predict other players only half a move |  |
| `cl_predict_jump` | `1` | 0, 1 | 1: play your own jump sound at once |  |
| `cl_predict_players` | `1` | 0, 1 | 1: predict other players' movement |  |
| `cl_predict_projectiles` | `1` | 0, 1 | 1: spawn your own projectiles at once |  |
| `cl_predict_smoothview` | `1` |  | 1: smooth out corrections of your predicted view |  |
| `cl_predict_weaponsound` | `1` |  | 1: play your own weapon sounds at once |  |
| `cl_proxyaddr` | (empty) |  | Proxy (or chain of proxies) to connect through; never saved | ro nosave |
| `cl_remote_capabilities` | a list of 60 names, see question 8 |  | Commands and settings a server may run on the client | ro |
| `cl_shownet` | `0` | 0–2 | Print received messages: 0 off, 1 sizes, 2 with message names |  |
| `cl_solid_players` | `1` | 0, 1 | 1: other players are solid in prediction |  |
| `cl_sproj_xerp` | `0` | 0, 1 | Extrapolate others' simple projectiles by your latency (capped at 150 ms) |  |
| `cl_sv_packetsync` | `1` | 0, 1 | 1: the built-in server handles packets as they arrive |  |
| `cl_timeout` | `60` |  | Seconds without packets before the client disconnects | ro |
| `cl_useproxy` | `0` | 0, 1 | 1: connect through a running proxy when one is detected | ro |
| `mtu` | (empty) |  | Largest packet size to suggest to the server [userinfo] | info |
| `net_tcp_timeout` | `2000` |  | Timeout of TCP connections in ms |  |
| `qport` | `0` |  | Port number sent in the connect request (0: random) |  |
| `r_netgraph` | `0` | 0, 1 | 1: draw the netgraph (latency and loss per packet) |  |
| `r_netstats` | `0` | 0, 1 | 1: draw a block of network statistics |  |
| `rate` | `25000` |  | Bytes per second the server may send you [userinfo] | menu info |
| `rcon_address` | (empty) |  | Server remote-console commands go to | ro |
| `rcon_password` | (empty) |  | Remote-console password | ro |
| `showdrop` | `0` | 0, 1 | 1: print dropped packets |  |
| `showpackets` | `0` | 0–3 | Print packets: 0 off, 1 all, 2 incoming, 3 outgoing |  |

### 1.9 Demos, recording, broadcasts and match tools (158 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cam_dist` | `100` |  | Third-person camera distance (spectating and demos) |  |
| `cam_lockdir` | `0` | 0, 1 | 1: lock the third-person camera's direction |  |
| `cam_lockpos` | `0` | 0, 1 | 1: lock the third-person camera's position |  |
| `cam_thirdperson` | `0` | 0, 1 | 1: third-person camera when spectating or in demos |  |
| `cam_zoomaccel` | `2000` |  | Acceleration of the third-person camera's zoom with forward/back |  |
| `cam_zoomspeed` | `300` |  | Top speed of that zoom |  |
| `cl_chasecam` | `1` | 0, 1 | 1: first-person view when tracking; 0 chase camera | menu |
| `cl_debug_antilag_ghost` | `0` | 0–2 | A ghost at: 0 none, 1 the rewound position, 2 the client position |  |
| `cl_debug_antilag_lines` | `0` | 0, 1 | 1: draw lines between those positions |  |
| `cl_debug_antilag_self` | `0` | 0, 1 | 1: include the tracked player |  |
| `cl_debug_antilag_view` | `0` | 0–2 | Where players are drawn in supporting demos: 0 normal, 1 rewound, 2 client position |  |
| `cl_debug_weapon_view` | `0` | 0, 1 | 1: show weapon-choice debug messages stored in demos |  |
| `cl_demo_qwd_delta` | `1` | 0, 1 | 1: write delta-compressed entities into client demos |  |
| `cl_democlock` | `0` | 0, 1 | 1: show demo time elapsed |  |
| `cl_democlock_x` | `0` |  | Its column |  |
| `cl_democlock_y` | `-2` |  | Its row |  |
| `cl_demoPingInterval` | `5` | 0, 5 | Seconds between ping requests while recording |  |
| `cl_demoplay_flash` | `0.33` |  | How strongly a flash grenade whites the screen in demos |  |
| `cl_demospeed` | `1` |  | Demo playback speed (1 normal) |  |
| `cl_demoteamplay` | `0` | 0, 1 | 1: teamplay settings apply in single-player demos |  |
| `cl_fix_mvd` | `0` | 0, 1 | 1: work around broken multi-view demos |  |
| `cl_hightrack` | `0` |  | 1: track the player with the most frags |  |
| `cl_multiview` | `0` | 0–4 step 1 | Views at once in multi-view demos (0 to 4) | menu |
| `cl_mvdisplayhud` | `1` | 0–5 | Small HUD per view: 0 none, 1 text, 2 names, 3 icons and bars, 4 with numbers, 5 with a dark backing | menu |
| `cl_mvhudflip` | `0` | 0, 1 | 1: flip the small HUDs | menu |
| `cl_mvhudpos` | `bottom center` |  | Where the small HUDs sit |  |
| `cl_mvhudvertical` | `0` | 0, 1 | 1: small HUDs vertical | menu |
| `cl_mvinset` | `0` | 0, 1 | 1: one big view and an inset | menu |
| `cl_mvinset_offset_x` | `0` |  | Inset horizontal offset |  |
| `cl_mvinset_offset_y` | `0` |  | Inset vertical offset |  |
| `cl_mvinset_right` | `1` |  | 1: inset on the right |  |
| `cl_mvinset_size_x` | `0.333` |  | Inset width as a fraction |  |
| `cl_mvinset_size_y` | `0.333` |  | Inset height as a fraction |  |
| `cl_mvinset_top` | `1` |  | 1: inset at the top |  |
| `cl_mvinsetcrosshair` | `1` | 0, 1 | 1: crosshair in the inset | menu |
| `cl_mvinsethud` | `1` | 0, 1 | 1: HUD in the inset | menu |
| `cl_startupdemo` | (empty) |  | Demo played at startup | ro |
| `demo_autotrack` | `0` | 0, 1 | 1: follow the camera choices a server recorded into the demo |  |
| `demo_benchmarkdumps` | `1` | 0, 1 | 1: append timedemo results to a log file |  |
| `demo_capture_background_threads` | `0` |  | Threads that write captured frames |  |
| `demo_capture_codec` | `0` |  | Video codec for capture (Windows only) | win |
| `demo_capture_dir` | `capture` |  | Folder for captures | ro |
| `demo_capture_fps` | `30.0` |  | Capture frame rate |  |
| `demo_capture_mp3` | `0` | 0, 1 | 1: compress captured sound as MP3 (Windows only) | win |
| `demo_capture_mp3_kbps` | `128` |  | MP3 bit rate (Windows only) | win |
| `demo_capture_steadycam` | `0` |  | 1: input during capture does not move the view |  |
| `demo_capture_vid_maxlen` | `0` |  | Split captured video every this many MB (Windows only) | win |
| `demo_dir` | (empty) |  | Folder for demos and automatic recordings | menu ro |
| `demo_format` | `qwz` |  | Format of automatic recordings: qwd, qwz or mvd | menu |
| `demo_getpings` | `1` | 0, 1 | 1: always record pings into demos |  |
| `demo_jump_rewind` | `-10` |  | Seconds before a jump target playback resumes |  |
| `demo_jump_skip_messages` | `1` | 0, 1 | 1: do not print messages while jumping |  |
| `demo_playlist_loop` | `0` | 0, 1 | 1: loop the demo playlist |  |
| `demo_playlist_track_name` | (empty) |  | Player name to track in playlist demos |  |
| `demo_spawnwarn` | `0` | 0, 1 | 1: warn when the watched player crosses a spawn point or teleport exit |  |
| `demo_spawnwarn_text` | `you crossed a spawn` |  | Text of that warning |  |
| `file_browser_archive_color` | `255 170 0 255` |  | Colour of archives in the file browser |  |
| `file_browser_dir_color` | `170 80 0 255` |  | Colour of folders |  |
| `file_browser_file_color` | `255 255 255 255` |  | Colour of files |  |
| `file_browser_interline` | `0` |  | Spacing between rows |  |
| `file_browser_scrollnames` | `1` | 0, 1 | 1: scroll long names |  |
| `file_browser_selected_color` | `0 150 235 255` |  | Colour of the selection |  |
| `file_browser_show_date` | `1` | 0, 1 | 1: date column |  |
| `file_browser_show_size` | `1` | 0, 1 | 1: size column |  |
| `file_browser_show_status` | `1` | 0, 1 | 1: status bar |  |
| `file_browser_show_time` | `0` | 0, 1 | 1: time column |  |
| `file_browser_sort_archives` | `0` | 0, 1 | 1: sort archives by the chosen column too |  |
| `file_browser_sort_mode` | `1` | 1, 2, 3, 4, 14 | Sort order (column codes) |  |
| `file_browser_strip_names` | `1` | 0, 1 | 1: trim spaces in names |  |
| `match_auto_logconsole` | `1` | 0–2 | Log the console during a match: 0 off, 1 keep on match_save, 2 save at the end | menu |
| `match_auto_logupload` | `0` | 0, 1 | 1: upload the match log | ro |
| `match_auto_logupload_token` | (empty) |  | Token for that upload | ro |
| `match_auto_logurl` | `http://stats.quakeworld.nu/logupload` |  | Address for that upload | ro |
| `match_auto_minlength` | `30` |  | Shortest automatic recording kept, seconds |  |
| `match_auto_record` | `0` | 0–2 | Record each match: 0 off, 1 keep on match_save, 2 save at the end | menu |
| `match_auto_spectating` | `0` | 0, 1 | 1: record automatically as a spectator too |  |
| `match_auto_unminimize` | `1` | 0, 1 | 1: restore the window when a countdown starts |  |
| `match_challenge` | `0` | 0, 1 | 1: report match start and end to a ladder | ro |
| `match_challenge_url` | `http://stats.quakeworld.nu/post-challenge` |  | Address for those reports | ro |
| `match_format_2on2` | `2on2/%n - [%k%v%l] - [%M]` |  | Name pattern of automatic recordings for 2on2 |  |
| `match_format_3on3` | `tdm/%n - [%Oon%E_%t%v%e] - [%M]` |  | Name pattern for 3on3 |  |
| `match_format_4on4` | `tdm/%n - [%Oon%E_%t%v%e] - [%M]` |  | Name pattern for 4on4 |  |
| `match_format_arena` | `arena/%n - %p%v%e - [%F_frags] - [%M]` |  | Name pattern for arena |  |
| `match_format_coop` | `coop/%n - [%C_player_coop] - [%M]` |  | Name pattern for coop |  |
| `match_format_duel` | `duel/%n - %p%v%e - [dmm%D] - [%M]` |  | Name pattern for duel |  |
| `match_format_ffa` | `ffa/%n - [%C_player_ffa] - [%M]` |  | Name pattern for free for all |  |
| `match_format_multiteam` | `tdm/%n - [%a_%b] - [%M]` |  | Name pattern for more than two teams |  |
| `match_format_race` | `race/%n - [race] - [%M]` |  | Name pattern for race |  |
| `match_format_solo` | `solo/%n - [%M]` |  | Name pattern for solo |  |
| `match_format_tdm` | `tdm/%n - [%Oon%E_%t%v%e] - [%M]` |  | Name pattern for other team games |  |
| `match_format_tf_clanwar` | `tfwar/%n - [%Oon%E_%t%v%e] - [%M]` |  | Name pattern for class-based team matches |  |
| `match_format_tf_duel` | `tfduel/%n - %p%v%e [%M]` |  | Name pattern for class-based duels |  |
| `match_ladder_id` | `1` |  | Ladder id (always 1) |  |
| `match_name_and` | `_&_` |  | Separator between names |  |
| `match_name_nick` | (empty) |  | Your name in patterns (empty: your player name) |  |
| `match_name_on` | `on` |  | Separator in team-size counts |  |
| `match_name_spec` | `(SPEC)` |  | Added after your name as a spectator |  |
| `match_name_versus` | `_vs_` |  | Separator between sides |  |
| `mvd_autoadd_items` | `0` | 0, 1 | 1: add respawn clocks for every item at match start |  |
| `mvd_autohud` | `0` | 0–2 | HUD for multi-view demos: 0 unchanged; 1 a config by match type; 2 always a custom config | menu |
| `mvd_autotrack` | `0` | 0–4 | Follow the best player in demos: 0 off; 1 rules by game type; 2 the custom rules; 3 one rule per multi-view slot; 4 a simple powerup-first rule | menu |
| `mvd_autotrack_1on1` | `%a * %A + 50 * %W + %p + %f` |  | Formula scoring players in duels |  |
| `mvd_autotrack_1on1_values` | `1 2 3 2 3 5 8 8 1 2 3 0 0 0` |  | Weights used by that formula |  |
| `mvd_autotrack_2on2` | `%a * %A + 50 * %W + %p + %f` |  | Formula for 2on2 |  |
| `mvd_autotrack_2on2_values` | `1 2 3 2 3 5 8 8 1 2 3 500 900 1000` |  | Its weights |  |
| `mvd_autotrack_4on4` | `%a * %A + 50 * %W + %p + %f` |  | Formula for 4on4 |  |
| `mvd_autotrack_4on4_values` | `1 2 4 2 4 6 10 10 1 2 3 500 900 1000` |  | Its weights |  |
| `mvd_autotrack_custom` | `%a * %A + 50 * %W + %p + %f` |  | Formula for the custom mode |  |
| `mvd_autotrack_custom_values` | `1 2 3 2 3 4 6 6 1 2 3 500 900 1000` |  | Its weights |  |
| `mvd_autotrack_instant` | `0` | 0, 1 | 1: switch to the best player at once |  |
| `mvd_autotrack_lockteam` | `0` | 0, 1 | 1: switch only within one team | menu |
| `mvd_info` | `0` | 0, 1 | 1: a table of every player's status in demos |  |
| `mvd_info_setup` | `%p%n [%l] %h/%a %w` |  | Row format of that table |  |
| `mvd_info_show_header` | `0` | 0, 1 | 1: header row |  |
| `mvd_info_x` | `0` |  | Column |  |
| `mvd_info_y` | `0` |  | Row |  |
| `mvd_moreinfo` | `0` | 0, 1 | 1: more messages in demos (pickups and others) | menu |
| `mvd_multitrack_1` | `%f` |  | Formula for view 1 in multi-view tracking |  |
| `mvd_multitrack_1_values` | `1 2 3 2 3 5 8 8 1 2 3 0 0 0` |  | Its weights |  |
| `mvd_multitrack_2` | `%W` |  | Formula for view 2 |  |
| `mvd_multitrack_2_values` | `1 2 3 2 3 5 8 8 1 2 3 0 0 0` |  | Its weights |  |
| `mvd_multitrack_3` | `%h` |  | Formula for view 3 |  |
| `mvd_multitrack_3_values` | `1 2 3 2 3 5 8 8 1 2 3 0 0 0` |  | Its weights |  |
| `mvd_multitrack_4` | `%A` |  | Formula for view 4 |  |
| `mvd_multitrack_4_values` | `1 2 3 2 3 5 8 8 1 2 3 0 0 0` |  | Its weights |  |
| `mvd_pc_pent_1` | (empty) |  | Position and angles of pentagram camera 1 |  |
| `mvd_pc_pent_2` | (empty) |  | Pentagram camera 2 |  |
| `mvd_pc_pent_3` | (empty) |  | Pentagram camera 3 |  |
| `mvd_pc_quad_1` | (empty) |  | Position and angles of quad camera 1 |  |
| `mvd_pc_quad_2` | (empty) |  | Quad camera 2 |  |
| `mvd_pc_quad_3` | (empty) |  | Quad camera 3 |  |
| `mvd_pc_view_1` | (empty) |  | Which powerup camera view 1 uses |  |
| `mvd_pc_view_2` | (empty) |  | View 2 |  |
| `mvd_pc_view_3` | (empty) |  | View 3 |  |
| `mvd_pc_view_4` | (empty) |  | View 4 |  |
| `mvd_powerup_cam` | `0` | 0, 1 | 1: cut to powerup cameras 5 s before a powerup spawns |  |
| `mvd_sortitems` | `1` | 0, 1 | 1: sort respawn clocks by next spawn |  |
| `mvd_status` | `0` | 0, 1 | 1: a table about the tracked player | menu |
| `mvd_status_x` | `0` |  | Column |  |
| `mvd_status_y` | `0` |  | Row |  |
| `qtv_adjustbuffer` | `1` | 0–2 | Broadcast buffer: 0 fixed speed; 1 adjust speed to stay within a band; 2 adjust to stay near the target | menu |
| `qtv_adjusthighstart` | `1` |  | Buffer level (times the target) above which playback speeds up |  |
| `qtv_adjustlowstart` | `0.3` |  | Level below which playback slows |  |
| `qtv_adjustmaxspeed` | `999` |  | Fastest adjusted speed (percent) |  |
| `qtv_adjustminspeed` | `0` |  | Slowest adjusted speed (percent) |  |
| `qtv_allow_pause` | `0` | 0, 1 | 1: demo speed applies to broadcasts |  |
| `qtv_api_url` | `http://qtvapi.quakeworld.nu/api/v1/servers` |  | Address of the service that maps servers to broadcasts | ro |
| `qtv_buffertime` | `0.5` |  | Seconds of broadcast to buffer |  |
| `qtv_chatprefix` | `$[{QTV}$] ` |  | Prefix of your broadcast chat lines |  |
| `qtv_event_changename` | ` &cFF0changed name to&r ` |  | Text when a viewer changes name |  |
| `qtv_event_join` | ` &c2F2joined&r` |  | Text when a viewer joins |  |
| `qtv_event_leave` | ` &cF22left&r` |  | Text when a viewer leaves |  |
| `qtv_event_msglevel` | `2` | 0–2 | Viewer join/leave lines: 0 hidden, 1 shown, 2 hidden once a match starts |  |
| `qtv_gamechatprefix` | `$[{QTV>game}$] ` |  | Prefix of lines sent from the broadcast into the game |  |
| `qtv_prebuffertime` | `0` |  | Seconds buffered before playback starts |  |
| `qtv_proxyaddr` | (empty) |  | Relay to reach broadcasts through | ro |
| `qtv_say_team` | `0` | 0, 1 | 1: team chat allowed to the broadcast |  |
| `qtv_skipchained` | `1` | 0, 1 | 1: shorter form of relayed chat lines |  |

### 1.10 Sound and voice (41 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `bgmvolume` | `1` |  | Volume of the music |  |
| `cl_staticSounds` | `1` | 0, 1 | 1: play the map's fixed ambient sounds | menu |
| `cl_voip_capturingvol` | `0.5` |  | Game volume scale while your microphone is capturing | voice |
| `cl_voip_demorecord` | `1` |  | Meant to write voice into demos (registered but not read) | voice |
| `cl_voip_micamp` | `2` |  | Microphone gain | voice |
| `cl_voip_play` | `1` | 0, 1 | 1: play other players' voice | voice |
| `cl_voip_send` | `0` | 0–2 | Send voice: 0 off, 1 voice activated, 2 always (bit 2 is also set while +voip is held) | ro voice |
| `cl_voip_showmeter` | `1` | 0–2 | Voice level meter: 0 hidden, 1 while sending, 2 always | voice |
| `cl_voip_showmeter_x` | `0` |  | Horizontal place of the voice meter | voice |
| `cl_voip_showmeter_y` | `0` |  | Vertical place of the voice meter | voice |
| `cl_voip_vad_delay` | `0.3` |  | Seconds voice keeps sending after voice activation ends | ro voice |
| `cl_voip_vad_threshhold` | `15` |  | Level that triggers voice activation | ro voice |
| `demo_voice_enable` | `1` | 0, 1 | 1: play voice tracks synchronised with a demo |  |
| `demo_voice_team_filter` | `1` | 0, 1 | 1: play only the voice tracks of the watched player's team |  |
| `s_ambientfade` | `100` |  | How fast ambient volume changes as you move |  |
| `s_ambientlevel` | `0.3` |  | Volume of the ambient water and sky sounds |  |
| `s_audiodevice` | `0` |  | Which output device to use (index or name) | latch |
| `s_chat_custom` | `1` | 0–2 | Chat sounds: 0 none, 1 for everyone and team chat, 2 for team chat only | menu |
| `s_desiredsamples` | `0` |  | Mixing buffer size in samples (0 automatic) | menu |
| `s_khz` | `11` | 11, 22, 44, 48 | Mixing rate in kHz: 11, 22, 44 or 48 | menu |
| `s_linearresample` | `0` | 0–2 | Resample sounds to the mixing rate with a better filter (0 off; 1 and 2 qualities) | latch |
| `s_linearresample_stream` | `0` |  | The same for voice streams |  |
| `s_loadas8bit` | `0` | 0, 1 | 1: keep sounds as 8-bit to save memory |  |
| `s_mm1_file` | `misc/talk.wav` |  | Sound played for an everyone chat line |  |
| `s_mm1_volume` | `1` | 0–1 step 0.1 | Its volume | menu |
| `s_mm2_file` | `misc/talk.wav` |  | Sound played for a team chat line |  |
| `s_mm2_volume` | `1` | 0–1 step 0.1 | Its volume | menu |
| `s_nosound` | `0` | 0, 1 | 1: no sound at all |  |
| `s_otherchat_file` | `misc/talk.wav` |  | Sound for other chat lines |  |
| `s_otherchat_volume` | `1` | 0–1 step 0.1 | Its volume | menu |
| `s_precache` | `1` | 0, 1 | 1: load all sounds when a map loads |  |
| `s_qizmo_enhanced_stereo` | `0` | 0, 1 | 1: stereo panning and timing like the classic proxy's |  |
| `s_qizmo_enhanced_stereo_offset` | `4` |  | Right-channel delay in samples for that effect |  |
| `s_raw_volume` | `1` |  | Volume of voice playback |  |
| `s_show` | `0` | 0–2 | Show the sounds playing: 0 off, 1 count, 2 list |  |
| `s_silent_racing` | `0` | 0, 1 | 1: do not hear other players while racing |  |
| `s_spec_file` | `misc/talk.wav` |  | Sound for a spectator chat line |  |
| `s_spec_volume` | `1` | 0–1 step 0.1 | Its volume | menu |
| `s_swapstereo` | `0` | 0, 1 | 1: swap left and right |  |
| `sys_inactivesound` | `0` | 0, 1 | 1: keep playing sound while the window is not focused | menu |
| `volume` | `0.7` | 0–1 step 0.05 | Master sound volume | menu |

### 1.11 Picture: renderer, textures, video mode, fonts (193 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `font_capitalize` | `0` | 0, 1 | 1: TrueType text in capitals | ttf |
| `font_facepath` | (empty) |  | The TrueType font file in use | ttf |
| `font_gradient_alternate_color1` | `175 120 52` |  | Top colour of the alternate text style | ttf |
| `font_gradient_alternate_color2` | `75 52 22` |  | Bottom colour of the alternate text style | ttf |
| `font_gradient_alternate_percent` | `0.4` |  | Gradient slope of the alternate style | ttf |
| `font_gradient_normal_color1` | `255 255 255` |  | Top colour of normal text | ttf |
| `font_gradient_normal_color2` | `107 98 86` |  | Bottom colour of normal text | ttf |
| `font_gradient_normal_percent` | `0.2` |  | Gradient slope of normal text | ttf |
| `font_gradient_number_color1` | `255 255 150` |  | Top colour of number text | ttf |
| `font_gradient_number_color2` | `218 132 7` |  | Bottom colour of number text | ttf |
| `font_gradient_number_percent` | `0.2` |  | Gradient slope of number text | ttf |
| `font_outline_width` | `2` |  | Outline width of TrueType text | ttf |
| `gl_alphafont` | `1` | 0, 1 | 1: the console font uses its alpha channel |  |
| `gl_anisotropy` | `16` | 0–16 step 1 | Anisotropic filtering level | menu |
| `gl_brush_polygonoffset` | `2.0` |  | Depth offset for brush entities against z-fighting; limited by rulesets | rules |
| `gl_brush_polygonoffset_factor` | `0.05` |  | Factor of that offset |  |
| `gl_caustics` | `0` | 0, 1 | 1: caustic light under liquids |  |
| `gl_charsets_min` | `1` | 0, 1 | 1: look for fewer charset files at startup |  |
| `gl_clear` | `0` | 0, 1 | 1: clear the screen each frame | menu |
| `gl_clearColor` | `0 0 0` |  | Colour of that clear |  |
| `gl_colorlights` | `1` | 0, 1 | 1: coloured glows from powerups and flags | menu |
| `gl_consolefont` | `povo5` |  | Charset picture used for text |  |
| `gl_detail` | `0` | 0, 1 | 1: a detail texture layer on walls |  |
| `gl_ext_texture_compression` | `0` | 0, 1 | 1: compress textures in video memory |  |
| `gl_externalTextures_bmodels` | `1` | 0, 1 | 1: load replacement textures for brush entities | reload |
| `gl_externalTextures_world` | `1` | 0, 1 | 1: load replacement textures for the world | reload |
| `gl_fb_bmodels` | `1` | 0, 1 | 1: fullbright pixels on brush models |  |
| `gl_fb_models` | `1` | 0, 1 | 1: fullbright pixels on models (never on eyes and backpacks under a ruleset) | rules |
| `gl_finish` | `0` | 0, 1 | 1: wait for the GPU each frame |  |
| `gl_flashblend` | `0` | 0–2 | Glow bubbles: 0 none, 1 on explosions and powerups, 2 also round rockets | menu |
| `gl_lerpimages` | `1` | 0, 1 | 1: smoother scaling of non-power-of-two pictures | reload |
| `gl_lightmode` | `1` | 0, 1 | 1: overbright lighting | menu |
| `gl_loadlitfiles` | `1` | 0–3 | Coloured lighting: 0 off, 1 files and in-map, 2 files only, 3 in-map only |  |
| `gl_luma_level` | `1` |  | Strength of glow (luma) textures | reload |
| `gl_lumatextures` | `1` |  | 1: glow textures where the server allows | menu |
| `gl_max_size` | `2048` |  | Largest texture side; a ruleset sets a minimum of 512 | menu rules reload |
| `gl_miptexLevel` | `0` | 0–3 step 1 | Downscale textures like the classic mip cap (0 to 3) | menu reload |
| `gl_modulate` | `1` | 0.5–3.0 step 0.1 | Lightmap brightness | menu |
| `gl_multisamples` | `0` |  | Anti-aliasing samples of the window | latch |
| `gl_no24bit` | `0` | 0, 1 | 1: no replacement 24-bit textures | reload |
| `gl_nocolors` | `0` | 0, 1 | 1: no shirt/pants colours on 8-bit skins |  |
| `gl_oldlitscaling` | `0` | 0, 1 | 1: scale coloured light the old, darker way |  |
| `gl_outline` | `0` | 0–3 | Outlines: 0 off, 1 models, 2 world, 3 both; refused under one ruleset | menu rules |
| `gl_outline_color_enemy` | (empty) |  | Outline colour of enemies (empty: model colour) |  |
| `gl_outline_color_model` | `0 0 0` |  | Outline colour of models |  |
| `gl_outline_color_team` | (empty) |  | Outline colour of teammates |  |
| `gl_outline_color_world` | `0 0 0` |  | Outline colour of the world |  |
| `gl_outline_scale_model` | `1` |  | Outline thickness on models; limited by rulesets | rules |
| `gl_outline_use_player_color` | `0` | 0, 1 | 1: outline players in their shirt/pants colours |  |
| `gl_outline_world_depth_threshold` | `4` |  | Depth threshold for world edges |  |
| `gl_outline_world_normal_threshold` | `0.997` |  | Normal threshold for world edges |  |
| `gl_picmip` | `0` |  | Drop this many mip levels from world textures; a ruleset caps it at 3 | rules reload |
| `gl_playermip` | `0` |  | Drop this many mip levels from player skins |  |
| `gl_polyblend` | `1` | 0, 1 | 1: full-screen tints (damage, powerups, liquids) |  |
| `gl_powerupshells` | `1` | 0, 1 | Shell over players with powerups (strength); some rulesets limit it | menu rules |
| `gl_powerupshells_base1level` | `0.05` |  | Base level of the first shell layer |  |
| `gl_powerupshells_base2level` | `0.1` |  | Base level of the second shell layer |  |
| `gl_powerupshells_effect1level` | `0.75` |  | Effect level of the first shell layer |  |
| `gl_powerupshells_effect2level` | `0.4` |  | Effect level of the second shell layer |  |
| `gl_reverse_z` | `1` | 0, 1 | 1: reversed depth buffer with no far plane | latch |
| `gl_rl_globe` | `0` | 0–3 | Rocket light look independent of gl_flashblend (0 to 3) |  |
| `gl_scaleAlphaTextures` | `0` | 0, 1 | 1: texture detail settings also scale fence textures | reload |
| `gl_scaleModelSimpleTextures` | `0` | 0, 1 | 1: they also scale simple item icons | reload |
| `gl_scaleModelTextures` | `0` | 0, 1 | 1: they also scale model textures | reload |
| `gl_scaleskytextures` | `0` | 0, 1 | 1: they also scale sky textures | reload |
| `gl_scaleTurbTextures` | `1` | 0, 1 | 1: they also scale liquid textures | reload |
| `gl_shaftlight` | `1` | 0, 1 | 1: lightning beam is fullbright |  |
| `gl_simpleitems` | `0` | 0, 1 | 1: items drawn as flat icons | menu |
| `gl_simpleitems_orientation` | `2` | 0–3 | How icons face: 0 by view, 1 upright toward you, 2 straight at you, 3 rotating | menu |
| `gl_simpleitems_size` | `16` |  | Icon size (up to 16) |  |
| `gl_smoothmodels` | `1` | 0, 1 | 1: smooth shading on models |  |
| `gl_spec_xray` | `0` | 0, 1 | 1: see players through walls in demos and broadcasts |  |
| `gl_spec_xray_distance` | `1500` |  | Range of that |  |
| `gl_squareparticles` | `0` | 0, 1 | 1: square particles |  |
| `gl_subdivide_size` | `64` |  | Sky and liquid polygon subdivision size |  |
| `gl_textureless` | `0` | 0, 1 | 1: walls drawn in each texture's average colour; refused under one ruleset | menu rules |
| `gl_texturemode` | `GL_LINEAR_MIPMAP_LINEAR` |  | Texture filter mode (nearest, linear, mip variants) | menu |
| `gl_texturemode_viewmodels` | `GL_LINEAR` |  | Filter mode for the held weapon |  |
| `gl_triplebuffer` | `1` | 0, 1 | 1: triple-buffer the HUD layer |  |
| `gl_turbalpha` | `1` |  | Liquid opacity |  |
| `gl_vbo_clientmemory` | `0` | 0, 1 | 1: dynamic geometry from client memory | latch |
| `image_jpeg_quality_level` | `75` |  | JPEG screenshot quality |  |
| `image_png_compression_level` | `1` |  | PNG screenshot compression |  |
| `r_bloom` | `0` | 0, 1 | 1: bloom glow (classic renderer only) | menu classic |
| `r_bloom_alpha` | `0.5` |  | Bloom opacity | classic |
| `r_bloom_darken` | `3` |  | Bloom darkening passes | classic |
| `r_bloom_diamond_size` | `8` |  | Bloom kernel size | classic |
| `r_bloom_fast_sample` | `0` | 0, 1 | 1: faster bloom | classic |
| `r_bloom_intensity` | `1` |  | Bloom strength | classic |
| `r_bloom_sample_size` | `256` |  | Bloom sample texture size | classic |
| `r_drawdisc` | `1` | 0, 1 | 1: disk activity icon |  |
| `r_drawentities` | `1` | 0, 1 | 1: draw entities |  |
| `r_drawflame` | `1` | 0, 1 | 1: draw torches and flames | menu |
| `r_drawflat` | `0` | 0–3 | Flat colours: 0 off, 1 walls and floors, 2 floors only, 3 walls only; refused under one ruleset | menu rules |
| `r_drawflat_mode` | `0` | 0–2 | Flat colour style: 0 solid, 1 blended with the texture, 2 tinted monochrome texture |  |
| `r_drawvweps` | `1` | 0, 1 | 1: draw others' weapon models |  |
| `r_drawworld` | `1` |  | 0: skip the world in timedemos only |  |
| `r_dynamic` | `2` | 0–2 | Dynamic lights on surfaces: 0 off, 1 computed on the CPU, 2 on the GPU | menu both-renderers |
| `r_farclip` | `8192` | 4096–8192 step 4096 | Far clipping distance | menu |
| `r_fastsky` | `0` | 0, 1 | 1: sky in one colour | menu |
| `r_fastturb` | `0` | 0, 1 | 1: liquids in one colour | menu |
| `r_floorcolor` | `50 100 150` |  | Floor colour for flat mode |  |
| `r_fullbright` | `0` | 0, 1 | 1: no lighting (works only where the server allows) | menu |
| `r_fx_fog` | `0` | 0–2 | Fog: 0 off, 1 on, 2 only in liquids |  |
| `r_fx_fog_color_air` | `153 128 103` |  | Fog colour in air |  |
| `r_fx_fog_color_lava` | `255 64 0` |  | Fog colour in lava |  |
| `r_fx_fog_color_slime` | `128 255 0` |  | Fog colour in slime |  |
| `r_fx_fog_color_water` | `32 64 128` |  | Fog colour in water |  |
| `r_fx_fog_density` | `0.125` |  | Density for exponential fog |  |
| `r_fx_fog_end` | `800.0` |  | End distance for linear fog |  |
| `r_fx_fog_model` | `2` | 0–2 | Fog model: 0 linear, 1 and 2 exponential |  |
| `r_fx_fog_sky` | `0.3` |  | How much fog shades the sky |  |
| `r_fx_fog_start` | `50.0` |  | Start distance for linear fog |  |
| `r_fx_fog_usemap` | `2` | 0–2 | Use the map's own fog: 0 never, 1 always, 2 only in local single player |  |
| `r_instagibTrail` | `1` | 0–13 | Rail trail type in instagib (0 to 13) |  |
| `r_lavacolor` | `80 0 0` |  | Lava colour for one-colour liquids |  |
| `r_lerpframes` | `1` | 0, 1 | 1: interpolate model animation frames |  |
| `r_lerpmuzzlehack` | `1` | 0, 1 | 1: no interpolation on muzzle flash frames |  |
| `r_lightdecayrate` | `2` |  | How fast lights shrink (at least 1) |  |
| `r_lightflicker` | `1` | 0, 1 | 1: lights flicker |  |
| `r_lightmap_lateupload` | `0` | 0, 1 | 1: upload lightmaps as surfaces draw |  |
| `r_lightmap_packbytexture` | `2` |  | How surfaces are grouped when packing lightmaps |  |
| `r_novis` | `0` | 0, 1 | 1: ignore the map's visibility data |  |
| `r_railTrail` | `1` | 0–12 | Rail trail type (0 to 12) |  |
| `r_remove_collinear_vertices` | `0` | 0, 1 | 1: drop vertices that make zero-area triangles |  |
| `r_shadows` | `0` | 0, 1 | 1: simple model shadows |  |
| `r_shaftalpha` | `1` |  | Lightning beam opacity |  |
| `r_skycolor` | `40 80 150` |  | Sky colour for one-colour sky |  |
| `r_skyname` | (empty) |  | Sky box to use instead of the map's sky |  |
| `r_skywind` | `1` |  | Speed scale of an animated sky box |  |
| `r_slimecolor` | `10 60 10` |  | Slime colour for one-colour liquids |  |
| `r_smoothalphahack` | `0` | 0, 1 | HUD text alpha blending style |  |
| `r_smoothcrosshair` | `1` | 0, 1 | 1: filter the crosshair texture |  |
| `r_smoothimages` | `1` | 0, 1 | 1: filter HUD pictures |  |
| `r_smoothtext` | `1` | 0, 1 | 1: filter HUD text |  |
| `r_speeds` | `0` |  | 1: print rendering timings and counts |  |
| `r_telecolor` | `255 60 60` |  | Teleport colour for one-colour liquids |  |
| `r_wallcolor` | `255 255 255` |  | Wall colour for flat mode |  |
| `r_watercolor` | `10 50 80` |  | Water colour for one-colour liquids |  |
| `scr_centerMenu` | `1` | 0, 1 | 1: centre the menu vertically |  |
| `scr_cursor_alpha` | `1` |  | Menu cursor opacity |  |
| `scr_cursor_iconoffset_x` | `0` |  | Menu cursor picture offset |  |
| `scr_cursor_iconoffset_y` | `0` |  | Menu cursor picture offset |  |
| `scr_cursor_scale` | `0.2` |  | Menu cursor size |  |
| `scr_cursor_sensitivity` | `1` | 0.10–3 step 0.10 | Menu cursor speed | menu |
| `scr_menualpha` | `0.7` | 0, 1 | Darkening behind the menu |  |
| `scr_menudrawhud` | `0` | 0, 1 | 1: draw the HUD behind the menu |  |
| `scr_scaleMenu` | `2` | 0 | Menu scale |  |
| `sys_highpriority` | (empty) | -1–1 | Process priority: -1 low, 0 normal, 1 high | menu ro |
| `vid_24bit_depth` | `1` | 0, 1 | 1: ask for a 24-bit depth buffer | latch |
| `vid_colorbits` | `0` |  | Colour depth of the mode | menu latch |
| `vid_conheight` | `0` | 240–1538 step 4 | Height of the text layer (0 automatic) | menu |
| `vid_conscale` | `2.0` |  | Scale of the text layer when width and height are 0 |  |
| `vid_conwidth` | `0` | 320–2048 step 8 | Width of the text layer (0 automatic) | menu |
| `vid_displayfrequency` | `0` |  | Refresh rate of the full-screen mode | latch |
| `vid_displaynumber` | `0` |  | Display used in full screen | latch |
| `vid_flashonactivity` | `1` |  | 1: flash the taskbar entry on server activity | menu |
| `vid_framebuffer` | `2` | 0–2 | Off-screen rendering: 0 none, 1 one buffer, 2 separate buffers for scene and HUD | menu latch |
| `vid_framebuffer_blit` | `0` | 0, 1 | 1: copy the buffer to the screen instead of swapping |  |
| `vid_framebuffer_depthformat` | `0` | 0–4 | Depth precision: 0 best, 1 16-bit float, 2 24-bit, 3 32-bit, 4 32-bit float | latch |
| `vid_framebuffer_fxaa` | `0` | 0–17 step 1 | FXAA preset (0 off, 1 to 17) | menu |
| `vid_framebuffer_hdr` | `0` | 0, 1 | 1: 16-bit float colour | menu latch |
| `vid_framebuffer_hdr_tonemap` | `0` | 0, 1 | 1: tone map in HDR | menu |
| `vid_framebuffer_height` | `0` |  | Custom buffer height (minimum 200) |  |
| `vid_framebuffer_multisample` | `0` | 0–16 step 1 | Anti-aliasing samples of the buffer | menu |
| `vid_framebuffer_scale` | `1` | 0.25–2.0 step 0.25 | Buffer scale against the window | menu |
| `vid_framebuffer_smooth` | `1` | 0, 1 | 1: filter when scaling the buffer |  |
| `vid_framebuffer_sshotmode` | `0` | 0, 1 | Screenshots at buffer or window size |  |
| `vid_framebuffer_width` | `0` |  | Custom buffer width (minimum 320) |  |
| `vid_fullscreen` | `1` | 0, 1 | 1: full screen | menu latch |
| `vid_gammacorrection` | `0` | 0–2 | sRGB: 0 off, 1 ask, 2 require | latch |
| `vid_gl_core_profile` | `0` | 0, 1 | 1: core profile for the classic renderer | latch |
| `vid_grab_keyboard` | `0` | 0, 1 | 1: grab system hotkeys | latch |
| `vid_height` | `0` |  | Full-screen height | menu latch |
| `vid_minimize_on_focus_loss` | 1 (0 on macOS) | 0, 1 | 1: minimise when full screen loses focus (default 0 on macOS) | latch |
| `vid_reload_auto` | `1` | 0, 1 | 1: graphics changes restart video by themselves |  |
| `vid_renderer` | `1` | 0, 1 | Renderer: 0 classic, 1 modern (needs GL 4.3) | menu latch both-renderers |
| `vid_showextensions` | `0` | 0, 1 | 1: print the GL extensions with the graphics info |  |
| `vid_software_palette` | `1` | 0, 1 | 1: gamma and contrast applied by the renderer; forced to 1 by one ruleset | menu rules latch |
| `vid_usedesktopres` | `1` | 0, 1 | 1: full screen at the desktop resolution | menu latch |
| `vid_verbose` | `0` | 0, 1 | 1: print graphics info on every restart |  |
| `vid_vsync` | `0` | 0, 1 | 1: vertical sync | menu |
| `vid_vsync_lag_fix` | `0` | 0, 1 | 1: experimental reduction of vsync delay | menu |
| `vid_vsync_lag_tweak` | `1.0` |  | Tuning of that reduction |  |
| `vid_width` | `0` |  | Full-screen width | menu latch |
| `vid_win_borderless` | `0` | 0, 1 | 1: borderless window | latch |
| `vid_win_displaynumber` | `0` |  | Display used for the window |  |
| `vid_win_height` | `480` |  | Window height | latch |
| `vid_win_save_pos` | `1` | 0, 1 | 1: remember where the window was moved |  |
| `vid_win_save_size` | `1` | 0, 1 | 1: remember the window size |  |
| `vid_win_width` | `640` |  | Window width | latch |
| `vid_xpos` | `3` |  | Window horizontal position |  |
| `vid_ypos` | `39` |  | Window vertical position |  |

### 1.12 Particles and effects (106 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_backpackfilter` | `0` | 0, 1 | 1: hide backpacks | menu |
| `cl_camera_death` | `0` | 0, 1 | 1: camera floats over your body after death |  |
| `cl_camera_tpp` | `0` | 0–2 | 0 first person, 1 and 2 two third-person cameras |  |
| `cl_camera_tpp_distance` | `-56` |  | Third-person camera distance |  |
| `cl_camera_tpp_height` | `24` |  | Third-person camera height |  |
| `cl_deadbodyFilter` | `0` | 0–3 | Hide corpses: 0 no, 1 after they land, 2 at once, 3 at once unless spectating | menu |
| `cl_fakeshaft` | `1` | 0–1 step 0.05 | Smoothing of your own lightning beam, 0 to 1 (2: special mode for rewinding servers); announced | menu rules |
| `cl_fakeshaft_extra_updates` | `1` | 0, 1 | 1: update the beam every frame |  |
| `cl_gibFilter` | `0` | 0, 1 | 1: hide gibs | menu |
| `cl_hidenails` | `0` |  | 1: hide nails in flight | menu |
| `cl_hiderockets` | `0` |  | 1: hide rockets in flight | menu |
| `cl_model_bobbing` | `1` | 0, 1 | 1: rotating items bob up and down |  |
| `cl_muzzleflash` | `1` | 0–2 | Muzzle flashes: 0 off, 1 on, 2 off for yourself | menu |
| `cl_newlerp` | `0` |  | Experimental smoothing of projectiles (0 off; fraction) |  |
| `cl_novweps` | `0` | 0, 1 | 1: refuse the visible-weapon extension |  |
| `cl_r2g` | `0` | 0, 1 | 1: rockets drawn as grenades | menu |
| `cl_useimagesinfraglog` | `0` | 0, 1 | 1: weapon pictures in the tracker | menu |
| `gl_bounceparticles` | `1` | 0, 1 | 1: sparks bounce off walls |  |
| `gl_buildingsparks` | `0` |  | 1: destroyed buildings keep sparking (class-based team mod) |  |
| `gl_clipparticles` | `1` | 0, 1 | 1: thin out blended particles near the eye |  |
| `gl_coronas` | `0` |  | 1: glows on explosions and lights |  |
| `gl_coronas_tele` | `0` | 0, 1 | 1: glow on teleports |  |
| `gl_cutf_tesla_effect` | `0` |  | 1: lightning effect while a tesla or shambler charges |  |
| `gl_detpacklights` | `0` |  | 1: lights on detpacks; forced off by one ruleset | rules |
| `gl_extratrails` | `0` |  | 1: trails on extra objects |  |
| `gl_inferno_speed` | `1000` |  | Speed of the flame trail |  |
| `gl_inferno_trail` | `2` |  | Type of the flame trail |  |
| `gl_lighting_color` | `0` |  | 1: models lit by coloured light nearby |  |
| `gl_lighting_vertex` | `0` |  | 1: per-vertex lighting on models |  |
| `gl_lightning` | `0` | 0, 1 | 1: particle lightning beams | menu rules |
| `gl_lightning_color` | `120 140 255` |  | Colour of those beams |  |
| `gl_lightning_size` | `3` |  | Width of those beams |  |
| `gl_lightning_sparks` | `0` |  | 1: sparks where the beam hits walls | rules |
| `gl_lightning_sparks_size` | `300` |  | Size of those sparks |  |
| `gl_nailtrail` | `0` | 0, 1 | 1: white trail behind nails |  |
| `gl_nailtrail_plasma` | `0` | 0, 1 | 1: blue plasma trail behind nails |  |
| `gl_nailtrail_turb` | `0` | 0, 1 | Bubble style of nails under water (0 or 1) |  |
| `gl_part_blobs` | `0` | 0, 1 | Particle style for blob explosions |  |
| `gl_part_blood` | `0` | 0, 1 | Particle style for blood |  |
| `gl_part_bloodtrails` | `1` | 0, 1 | Particle style for gib trails |  |
| `gl_part_bubble` | `1` | 0, 1 | 1: particle bubbles when drowning |  |
| `gl_part_cache` | `1` | 0, 1 | 1: cache particle work | latch |
| `gl_part_detpackexplosion_fire_color` | (empty) |  | Fire colour of detpack explosions |  |
| `gl_part_detpackexplosion_ray_color` | (empty) |  | Ray colour of detpack explosions |  |
| `gl_part_explosions` | `0` | 0, 1 | Particle style for explosions |  |
| `gl_part_gunshots` | `0` | 0, 1 | Particle style for gunshots |  |
| `gl_part_inferno` | `0` | 0, 1 | Particle style for flames |  |
| `gl_part_lavasplash` | `0` | 0, 1 | Particle style for lava splashes |  |
| `gl_part_spikes` | `0` | 0, 1 | Particle style for nail impacts |  |
| `gl_part_telesplash` | `0` | 0–2 | Particle style for teleports (0 to 2) |  |
| `gl_part_tracer1_color` | `0 124 0` |  | Colour of trail tracer 1 |  |
| `gl_part_tracer1_size` | `3.75` |  | Size of tracer 1 |  |
| `gl_part_tracer1_time` | `0.5` |  | Duration of tracer 1 |  |
| `gl_part_tracer2_color` | `255 77 0` |  | Colour of tracer 2 |  |
| `gl_part_tracer2_size` | `3.75` |  | Size of tracer 2 |  |
| `gl_part_tracer2_time` | `0.5` |  | Duration of tracer 2 |  |
| `gl_part_trails` | `0` | 0, 1 | Particle style for trails |  |
| `gl_particle_blobs` | `0` |  | Size of particle blob explosions (0 classic) |  |
| `gl_particle_blood` | `0` |  | Size of particle blood (0 classic) |  |
| `gl_particle_blood_color` | `1` |  | Blood particle colour |  |
| `gl_particle_blood_type` | `1` |  | Blood particle type |  |
| `gl_particle_deatheffect` | `0` | 0–2 | Extra blood on gibs (0 to 2) |  |
| `gl_particle_explosions` | `0` |  | Alternate explosion particles |  |
| `gl_particle_fire` | `0` |  | 1: particle flames on torches |  |
| `gl_particle_firecolor` | (empty) |  | Colour of those flames |  |
| `gl_particle_fulldetail` | `0` | 0, 1 | 1: full depth detail for particle effects | latch |
| `gl_particle_gibtrails` | `0` |  | 1: bright blood gib trails |  |
| `gl_particle_gunshots` | `0` |  | 1: alternate gunshot impacts | rules |
| `gl_particle_gunshots_type` | `1` |  | Type of those impacts |  |
| `gl_particle_muzzleflash` | `0` |  | 1: particle muzzle flashes on others |  |
| `gl_particle_shockwaves` | `0` | 0, 1 | Volumetric shockwave on explosions |  |
| `gl_particle_shockwaves_flat` | `0` |  | 1: flat orange shockwave |  |
| `gl_particle_sparks` | `0` | 0, 1 | 1: beam-like sparks | rules |
| `gl_particle_spikes` | `0` |  | 1: alternate nail impacts | rules |
| `gl_particle_spikes_type` | `1` |  | Type of those impacts |  |
| `gl_particle_telesplash` | `0` |  | 1: alternate teleport effect |  |
| `gl_particle_trail_detail` | `1` |  | Detail of particle beams and sparks |  |
| `gl_particle_trail_length` | `1` |  | Trail length multiplier |  |
| `gl_particle_trail_time` | `1` |  | Trail life multiplier |  |
| `gl_particle_trail_type` | `1` |  | Particle type for alternate effects |  |
| `gl_particle_trail_width` | `3` |  | Trail width |  |
| `gl_surface_lava` | `0` | 0–4 | Bubbles over lava (0 to 4) |  |
| `gl_surface_slime` | `0` | 0–4 | Bubbles and fumes over slime (0 to 4) |  |
| `gl_turb_effects` | `1` | 0, 1 | 1: impacts under water make bubbles |  |
| `gl_turb_fire` | `1` | 0, 1 | 1: explosions under water make bubbles |  |
| `gl_turb_trails` | `0` |  | 1: special trails under water |  |
| `gl_weather_rain` | `0` |  | Rain outdoors (the value is its density) |  |
| `gl_weather_rain_fast` | `0` | 0–2 | Cheaper rain (0 to 2) |  |
| `r_damagestats` | `0` |  | 1: recent damage shown above health and armour |  |
| `r_drawparticles` | `1` | 0, 1 | 1: draw particles; turning it off is ignored while a ruleset restricts particles | rules |
| `r_explosionLight` | `1` |  | Size of the light round explosions |  |
| `r_explosionLightColor` | `0` | 0–8 | Colour of that light (0 to 8) |  |
| `r_explosionType` | `1` | 0–10 | Explosion effect (0 to 10) | menu |
| `r_flagColor` | `0` | 0–5 | Glow colour of carried flags (0 to 5) |  |
| `r_grenadeTrail` | `1` | 0–12 | Grenade trail type (0 to 12) | menu |
| `r_lgbloodColor` | `225` |  | Palette colour of blood from the lightning gun |  |
| `r_particles_count` | `2048` |  | Most particles alive at once |  |
| `r_powerupGlow` | `1` | 0–2 | Powerup glow: 0 off, 1 on, 2 off on yourself | menu |
| `r_rlbloodColor_big` | `73` |  | Palette colour of big rocket blood |  |
| `r_rlbloodColor_small` | `225` |  | Palette colour of small rocket blood |  |
| `r_rocketLight` | `1` | 0–1 step 0.05 | Rocket light strength (0 to 1) | menu |
| `r_rocketLightColor` | `0` | 0–7 | Rocket light colour (0 to 7) |  |
| `r_rocketTrail` | `1` | 0–12 | Rocket trail type (0 to 12) | menu |
| `r_sgbloodColor` | `73` |  | Palette colour of blood from other weapons |  |
| `r_shiftbeam` | `0` |  | Moves the start of the lightning beam; locked to 0 by rulesets | rules |
| `r_telesplash` | `1` | 0, 1 | 1: teleport splash |  |

### 1.13 Skins, models and colours (34 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `baseskin` | `base` |  | Skin shown for a player whose skin file is missing | reload |
| `cl_name_as_skin` | `0` | 0–2 | Use the player's name (1) or user id (2) as the skin name |  |
| `enemybothskin` | (empty) |  | Skin for enemies with quad and pentagram | menu |
| `enemybottomcolor` | `-1` | -1–16 | Forced pants colour of enemies (-1 off) | menu |
| `enemyforceskins` | `0` | 0–3 | Force skins on enemies: 0 off; 1 skin named after the player; 2 after the user id; 3 skins e1, e2, ... | menu |
| `enemypentskin` | (empty) |  | Skin for enemies with the pentagram | menu |
| `enemyquadskin` | (empty) |  | Skin for enemies with quad | menu |
| `enemyskin` | (empty) |  | Forced skin of enemies | menu |
| `enemytopcolor` | `-1` | -1–16 | Forced shirt colour of enemies (-1 off) | menu |
| `gl_custom_grenade_color` | (empty) |  | Colour grenades are tinted (empty: their texture) |  |
| `gl_custom_grenade_fullbright` | `1` | 0, 1 | 1: the grenade colour is drawn fullbright |  |
| `gl_custom_grenade_tf` | `1` | 0, 1 | 1: ignore the grenade colour settings in the class-based team mod |  |
| `gl_custom_lg_color` | (empty) |  | Colour of the lightning beam (empty: its texture) |  |
| `gl_custom_lg_fullbright` | `1` | 0, 1 | 1: the beam colour is fullbright |  |
| `gl_custom_lgpack_color` | `64 64 255` |  | Tint of backpacks that hold a lightning gun |  |
| `gl_custom_rlpack_color` | `255 64 64` |  | Tint of backpacks that hold a rocket launcher |  |
| `gl_custom_rocket_color` | (empty) |  | Colour of rockets (empty: their texture) |  |
| `gl_custom_rocket_fullbright` | `1` | 0, 1 | 1: the rocket colour is fullbright |  |
| `gl_custom_spike_color` | (empty) |  | Colour of nails (empty: their texture) |  |
| `gl_custom_spike_fullbright` | `1` | 0, 1 | 1: the nail colour is fullbright |  |
| `noskins` | `0` | 0–2 | 0 skins on; 1 every player gets the base skin; 2 skins on but none downloaded | reload |
| `r_enemyskincolor` | (empty) |  | Solid R G B colour painted on enemies (empty off; see r_skincolormode) |  |
| `r_fullbrightSkins` | `1` | 0–1 step 0.05 | How fullbright player skins are, 0 to 1; capped by the server's fbskins key; announced in chat when changed | menu rules |
| `r_skincolormode` | `0` | 0–5 | How the team/enemy skin colours apply: 0 solid colour, 1 and 3 no effect, 2 blend with the skin, 4 add, 5 hue the skin |  |
| `r_skincolormodedead` | `-1` | -1 | r_skincolormode used for dead players (-1: same as living) |  |
| `r_teamskincolor` | (empty) |  | Solid R G B colour painted on teammates |  |
| `skin` | (empty) |  | Your skin name [userinfo] | menu info |
| `teambothskin` | (empty) |  | Skin for teammates with quad and pentagram | menu |
| `teambottomcolor` | `-1` | -1–16 | Forced pants colour of teammates (-1 off) | menu |
| `teamforceskins` | `0` | 0–3 | Force skins on teammates: 0 off; 1 skin named after the player; 2 after the user id; 3 skins t1, t2, ... | menu |
| `teampentskin` | (empty) |  | Skin for teammates with the pentagram | menu |
| `teamquadskin` | (empty) |  | Skin for teammates with quad | menu |
| `teamskin` | (empty) |  | Forced skin of teammates | menu |
| `teamtopcolor` | `-1` | -1–16 | Forced shirt colour of teammates (-1 off) | menu |

### 1.14 Teamplay (88 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `cl_fakename` | (empty) |  | Name prefixed to your team chat | menu |
| `cl_fakename_suffix` | `: ` |  | Text after that name | menu |
| `cl_nofake` | `2` | 0–2 | Expose faked names in chat: 0 off, 1 all, 2 only from enemies |  |
| `cl_parseFunChars` | `1` | 0, 1 | 1: $-sequences turn into special characters in typed text |  |
| `cl_parseSay` | `1` | 0, 1 | 1: %-macros expand in typed chat |  |
| `re_trigger_match_0` | (empty) |  | Read-only: part 0 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_1` | (empty) |  | Read-only: part 1 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_2` | (empty) |  | Read-only: part 2 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_3` | (empty) |  | Read-only: part 3 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_4` | (empty) |  | Read-only: part 4 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_5` | (empty) |  | Read-only: part 5 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_6` | (empty) |  | Read-only: part 6 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_7` | (empty) |  | Read-only: part 7 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_8` | (empty) |  | Read-only: part 8 of the last regular-expression trigger match (0 the whole match) | ro |
| `re_trigger_match_9` | (empty) |  | Read-only: part 9 of the last regular-expression trigger match (0 the whole match) | ro |
| `teamlock` | `0` |  | When spectating: 0 team follows the tracked player, 1 locked, or a team name to lock |  |
| `tp_forceTriggers` | `0` | 0, 1 | 1: event triggers run outside team games too |  |
| `tp_loadlocs` | `1` | 0, 1 | 1: load location files for team messages |  |
| `tp_msgtriggers` | `1` | 0, 1 | 1: message triggers on |  |
| `tp_name_armor` | `armor` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_armortype_ga` | `{&c0b0g&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_armortype_ra` | `{&cf00r&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_armortype_ya` | `{&cff0y&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_at` | `at` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_axe` | `axe` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_backpack` | `{&cf2apack&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_cells` | `cells` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_disp` | `dispenser` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_enemy` | `{&cf00enemy&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_eyes` | `{&cff0eyes&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_filter` | (empty) |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_flag` | `flag` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ga` | `{&c0b0ga&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_gl` | `gl` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_health` | `health` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_lg` | `{&c2aalg&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_mh` | `{&c0a0mega&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_nails` | `nails` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ng` | `ng` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_none` | (empty) |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_nothing` | `nothing` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_pent` | `{&cf00pent&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_pented` | `{&cf00pented&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_quad` | `{&c05fquad&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_quaded` | `{&c05fquaded&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ra` | `{&cf00ra&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ring` | `{&cff0ring&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rl` | `{&cf13rl&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rlg` | `{&cf13rl&cfff}{&c2aag&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rockets` | `rox` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rune1` | `{&c0f0resistance&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rune2` | `{&cf00strength&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rune3` | `{&cff0haste&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_rune4` | `{&c0ffregeneration&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_sentry` | `sentry gun` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_separator` | `/` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_sg` | `sg` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_shells` | `shells` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_sng` | `sng` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_someplace` | `someplace` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ssg` | `ssg` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_status_blue` | `$B` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_status_green` | `$G` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_status_red` | `$R` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_status_white` | `$W` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_status_yellow` | `$Y` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_suit` | `suit` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_teammate` | (empty) |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_weapon` | `weapon` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_name_ya` | `{&cff0ya&cfff}` |  | Word used for this item in team messages (team-messages.md) |  |
| `tp_need_cells` | `13` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_ga` | `60` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_health` | `50` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_nails` | `0` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_ra` | `120` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_rl` | `1` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_rockets` | `5` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_shells` | `0` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_weapon` | `87` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_need_ya` | `80` |  | Threshold for the need macro (team-messages.md) |  |
| `tp_pointpriorities` | `0` | 0, 1 | 1: point at the most important item, not the nearest to the crosshair |  |
| `tp_pointtimeout` | `15` |  | Seconds the last pointed item is remembered |  |
| `tp_poweruptextstyle` | `0` | 0, 1 | Powerup names in built-in messages: 0 long, 1 short |  |
| `tp_soundtrigger` | `~` |  | Character that starts a sound in team chat |  |
| `tp_tookpriorities` | `0` | 0, 1 | 1: the last-took macro prefers items by tp_took order |  |
| `tp_tooktimeout` | `15` |  | Seconds the last item taken is remembered |  |
| `tp_triggers` | `1` | 0, 1 | 1: event triggers on |  |
| `tp_weapon_order` | `78654321` |  | Weapon order, best first, for the weapon macros |  |

### 1.15 Server browser (39 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `sb_autohide` | `1` | 0–2 | Hide the browser after connecting: 0 never, 1 always, 2 in some cases only |  |
| `sb_autoupdate` | `1` | 0, 1 | 1: refresh marked sources automatically |  |
| `sb_findroutes` | `0` | 0, 1 | 1: look for the lowest-ping route through proxies |  |
| `sb_hidedead` | `1` | 0, 1 | 1: hide servers that do not answer |  |
| `sb_hideempty` | `1` | 0, 1 | 1: hide empty servers |  |
| `sb_hidefull` | `0` | 0, 1 | 1: hide full servers |  |
| `sb_hidehighping` | `0` | 0, 1 | 1: hide servers above sb_pinglimit |  |
| `sb_hidenotempty` | `0` | 0, 1 | 1: hide servers with players |  |
| `sb_ignore_proxy` | (empty) |  | Proxies left out of route finding |  |
| `sb_info_filter` | (empty) |  | Filters on server info keys (+key=value, -key=value) |  |
| `sb_inforetries` | `3` |  | Status query retries |  |
| `sb_infospersec` | `100` |  | Status queries per second |  |
| `sb_infotimeout` | `1000` |  | Status query timeout in ms |  |
| `sb_listcache` | `0` | 0, 1 | 1: cache the list of live servers between sessions |  |
| `sb_liveupdate` | `2` |  | Seconds between refreshes of the open server's info |  |
| `sb_mastercache` | `1` | 0, 1 | 1: cache master server answers |  |
| `sb_masterretries` | `3` |  | Master query retries |  |
| `sb_mastertimeout` | `1000` |  | Master query timeout in ms |  |
| `sb_nosockraw` | `0` | 0, 1 | 1: ping with game packets instead of ICMP |  |
| `sb_pinglimit` | `80` |  | Ping limit for sb_hidehighping |  |
| `sb_pings` | `3` |  | Pings sent per server |  |
| `sb_pingspersec` | `150` |  | Pings per second |  |
| `sb_pingtimeout` | `1000` |  | Ping timeout in ms |  |
| `sb_proxinfopersec` | `10` |  | Proxies queried per second |  |
| `sb_proxretries` | `3` |  | Proxy query retries |  |
| `sb_proxtimeout` | `1000` |  | Proxy query timeout in ms |  |
| `sb_showaddress` | `0` | 0, 1 | 1: show the address column |  |
| `sb_showfraglimit` | `0` | 0, 1 | 1: show the fraglimit column |  |
| `sb_showgamedir` | `0` | 0, 1 | 1: show the game directory column |  |
| `sb_showmap` | `1` | 0, 1 | 1: show the map column |  |
| `sb_showping` | `1` | 0, 1 | 1: show the ping column |  |
| `sb_showplayers` | `1` | 0, 1 | 1: show the players column |  |
| `sb_showproxies` | `0` | 0–2 | Proxies in the list: 0 hidden, 1 shown, 2 only proxies |  |
| `sb_showtimelimit` | `0` | 0, 1 | 1: show the timelimit column |  |
| `sb_sortplayers` | `92` |  | Sort order of the players list (column codes) |  |
| `sb_sortservers` | `32` |  | Sort order of the server list (column codes) |  |
| `sb_sortsources` | `3` |  | Sort order of the sources list |  |
| `sb_sourcevalidity` | `30` |  | Minutes a master's list stays valid |  |
| `sb_status` | `1` | 0, 1 | 1: two status lines at the bottom |  |

### 1.16 Screenshots (5 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `match_auto_sshot` | `0` | 0, 1 | 1: take a screenshot when a match ends | menu |
| `scr_allowsnap` | `1` | 0, 1 | 1: the client answers a server's request for a remote screenshot; locked to 1 by some rulesets | rules ro |
| `sshot_autoname` | `0` | 0, 1 | 1: screenshot names start with the map name |  |
| `sshot_dir` | (empty) |  | Folder screenshots are saved in | menu ro |
| `sshot_format` | png |  | Screenshot file type: png, jpg, tga or pcx | menu |

### 1.17 Everything else (player info, configs, menus, system) (69 settings)

| Setting | Default | Values | What it does | Marks |
| --- | --- | --- | --- | --- |
| `allow_f_cmdline` | `1` | 0, 1 | 1: answer a request for your command line | ro |
| `allow_f_system` | `1` | 0, 1 | 1: answer a request for your system details | ro |
| `bottomcolor` | (empty) | 0–16 | Your pants colour, 0 to 16 [userinfo] | menu info |
| `cfg_backup` | `0` | 0, 1 | 1: keep the old config when saving | menu ro |
| `cfg_legacy_exec` | `1` | 0–3 | Run the old config files at startup: 0 no, 1 from the mod folder, 2 and 3 in further cases | menu ro |
| `cfg_save_aliases` | `1` | 0, 1 | 1: cfg_save writes aliases | menu ro |
| `cfg_save_binds` | `1` | 0, 1 | 1: cfg_save writes binds | menu ro |
| `cfg_save_cmdline` | `1` | 0, 1 | 1: cfg_save writes the command line as a comment | menu ro |
| `cfg_save_cmds` | `1` | 0, 1 | 1: cfg_save writes the other state commands | menu ro |
| `cfg_save_cvars` | `1` | 0, 1 | 1: cfg_save writes settings | menu ro |
| `cfg_save_onquit` | `0` | 0, 1 | 1: save the config on quitting | menu ro |
| `cfg_save_sysinfo` | `0` | 0, 1 | Not used | ro |
| `cfg_save_unchanged` | `0` | 0, 1 | 1: cfg_save writes every setting, not only changed ones | menu ro |
| `cfg_save_userinfo` | `2` | 0–2 | Player info settings in the config: 0 none, 1 all but team, skin, spectator and colours, 2 all | menu ro |
| `cfg_use_gamedir` | `0` | 0, 1 | 1: save configs into the mod folder | menu ro |
| `cfg_use_home` | `0` | 0, 1 | 1: save configs into the user's home folder | menu ro |
| `cl_cmdline` | (empty) |  | The command line used (read-only) | ro |
| `cl_confirmquit` | `0` | 0, 1 | 1: ask before quitting | menu |
| `cl_curlybraces` | `0` | 0, 1 | 1: braces group commands in scripts |  |
| `cl_filterdrawviewmodel` | `0` | 0, 1 | 1: servers cannot change r_drawviewmodel |  |
| `cl_loadFragfiles` | `1` | 0, 1 | 1: load frag message definitions |  |
| `cl_mediaroot` | `0` | 0–2 | Demo, screenshot and log folders are relative to: 0 the game folder, 1 the home folder, 2 absolute | menu ro |
| `cl_onload` | `menu` |  | First screen: menu, console, browser, or a command | ro |
| `cl_parseFrags` | `1` | 0, 1 | 1: parse frag messages for statistics |  |
| `cl_restrictions` | `0` | 0, 1 | 1: restrict triggers when spectating and in demos too |  |
| `cl_username` | (empty) |  | Account name for logging in; loads a token file and sends it | ro |
| `cl_verify_qwprotocol` | `1` | 0–2 | Check the link-protocol registration at startup (Windows only) | win |
| `cl_warncmd` | `1` | 0, 1 | 1: print "Unknown command" |  |
| `cl_warnexec` | `1` | 0, 1 | 1: print config names as they run |  |
| `cl_window_caption` | `1` | 0–3 | Window title: 0 name and address, 1 server state and players, 2 fixed name, 3 version or players and map | menu |
| `cl_window_caption_delimiter` | ` \| ` |  | Separator in that title |  |
| `cl_www_address` | `https://badplace.eu/` |  | Address of the login service | ro |
| `cvar_viewdefault` | `1` | 0, 1 | 1: typing a setting's name also shows its default |  |
| `cvar_viewhelp` | `1` | 0, 1 | 1: typing a setting's name also shows its help |  |
| `cvar_viewlatched` | `1` | 0, 1 | 1: typing a setting's name also shows a pending value |  |
| `developer` | `0` | 0, 1 | 1: extra debug messages |  |
| `fs_cache` | `1` | 0, 1 | 1: cache file lists |  |
| `fs_savegame_home` | `1` |  | 1: saved games go to the home folder |  |
| `gender` | (empty) |  | Your gender for pronouns in mod messages; the menu offers m, f, n or empty [userinfo] | menu info |
| `localid` | (empty) |  | Secret that local tools put on remote-control packets |  |
| `log_dir` | (empty) |  | Folder for logs | menu ro |
| `log_readable` | `1` | 0, 1 | 1: logs without special characters | menu |
| `mapname` | (empty) |  | The current map (read-only) | ro |
| `menu_advanced` | `0` | 0, 1 | 1: the options menu shows advanced rows |  |
| `menu_botmatch_gamedir` | `fbca` |  | Mod folder used for bot matches from the menu |  |
| `menu_botmatch_mod_old` | `1` | 0, 1 | 1: settings that suit older mods for bot matches |  |
| `menu_ingame` | `1` | 0, 1 | 1: the in-game menu on Escape | menu |
| `menu_marked_bgcolor` | `20 20 20 128` |  | Colour of the selected menu row |  |
| `menu_marked_fade` | `4` |  | Flash speed of the selected row (0 none) |  |
| `msg` | `1` | 0–4 | Least important server message you receive: 0 all, 1 to 4 fewer [userinfo] | info |
| `name` | `player` |  | Your name [userinfo] | menu info |
| `noaim` | `1` | 0, 1 | 1: no server-side aim help [userinfo] | info |
| `password` | (empty) |  | Password to join a server [userinfo] | info ro |
| `qizmo_dir` | `qizmo` |  | Folder of the classic proxy | menu ro |
| `qwdtools_dir` | `qwdtools` |  | Folder of the demo tools | menu ro |
| `railcolor` | (empty) |  | Your rail trail colour [userinfo] | info |
| `ruleset` | `default` |  | Tournament ruleset: default, smackdown, thunderdome, mtfl, qcon, smackdrive; change only while disconnected; sets the *rsn info key | menu rules |
| `spectator` | (empty) | 0, 1 | Join as a spectator (non-empty and not 0) [userinfo] | info |
| `sv_www_address` | (empty) |  | Address the built-in server checks in with |  |
| `sv_www_authkey` | (empty) |  | Key for that check-in | ro |
| `sv_www_checkin_period` | `60` |  | Seconds between check-ins (with a fixed minimum) |  |
| `sys_disable_alt_enter` | `0` | 0, 1 | 1: Alt+Enter does not toggle full screen |  |
| `sys_disableWinKeys` | `0` | 0–2 | Block the Windows keys: 0 no, 1 always, 2 in full screen (Windows only) | ro win |
| `sys_fontsdir` | `/usr/local/share/fonts/` |  | Folder searched for TrueType fonts (Linux and macOS) |  |
| `sys_inactivesleep` | `1` | 0, 1 | 1: sleep while minimised |  |
| `sys_update_check` | `1` | 0, 1 | 1: check for a newer release | ro |
| `sys_yieldcpu` | `0` | 0, 1 | 1: give the processor back while waiting for the next frame |  |
| `team` | (empty) |  | Your team [userinfo] | menu info |
| `topcolor` | (empty) | 0–16 | Your shirt colour, 0 to 16 [userinfo] | menu info |

### 1.18 Element HUD: the per-element settings (1405 settings over 83 elements)

The element HUD (shown when `scr_newhud` is 1 or 2) is a set of 83 named elements. Each element registers its own settings, named `hud_<element>_<setting>`. (R)

Settings every element has:

| Setting | Default | What it does |
| --- | --- | --- |
| `hud_<e>_order` | per element (table below) | Drawing order (elements are drawn sorted by it) |
| `hud_<e>_place` | per element | What it is anchored to: `screen`, `top`, `view`, `sbar`, `ibar`, `hbar`, `sfree`, `ifree`, `hfree`, or another element's name |
| `hud_<e>_item_opacity` | `1` | Opacity of its content, 0 to 1 |
| `hud_<e>_draw` | `1` | 0: the element is still laid out (others can anchor to it) but nothing is drawn |

Settings an element has when it supports them (nearly all do; an element without a `show` default in the table below has none):

| Setting | What it does |
| --- | --- |
| `hud_<e>_show` | 1 show, 0 hide |
| `hud_<e>_align_x`, `hud_<e>_pos_x` | Horizontal alignment to its anchor (`left`, `center`, `right`, `before`, `after`) and offset in pixels |
| `hud_<e>_align_y`, `hud_<e>_pos_y` | Vertical alignment (`top`, `center`, `bottom`, `before`, `after`, and `console` for the top) and offset |
| `hud_<e>_frame`, `hud_<e>_frame_color` | Background: 0 none; above 0 up to 1 a plain box of that opacity with a small margin; 2 a bordered text box with an 8-pixel margin. And the box colour |

Each element is also a command: `<element>` alone prints its description, whether it is shown and drawn, its frame, anchor, alignment and offset; `<element> <setting> [value]` prints or sets `hud_<element>_<setting>` (and prints "no such variable" for an unknown one). Elements marked `+`/`-` also get `+hud_<element>` and `-hud_<element>`, which set its `show` to 1 and 0, so a key can show it while held. (R)

Some elements draw only at certain moments: the scoreboard clock and map name only on the scoreboard, intermission or the final screen; the plus/minus elements may also draw over the scoreboard. The netgraph element's visibility is also tied to `r_netgraph`. (R)

Every element, what it shows, its `show` default and its own settings with their defaults:

| Element | Shows | `show` default | Its own settings (default) | `+`/`-` |
| --- | --- | --- | --- | --- |
| `ammo` | ammo of the current weapon | `1` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0`, `show_always`=`0`, `text_color_low`=(empty), `text_color_normal`=(empty) |  |
| `ammo1` | shells | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0`, `text_color_low`=(empty), `text_color_normal`=(empty) |  |
| `ammo2` | nails | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0`, `text_color_low`=(empty), `text_color_normal`=(empty) |  |
| `ammo3` | rockets | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0`, `text_color_low`=(empty), `text_color_normal`=(empty) |  |
| `ammo4` | cells | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0`, `text_color_low`=(empty), `text_color_normal`=(empty) |  |
| `armor` | armour value | `1` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `pent_666`=`1`, `proportional`=`0`, `hidezero`=`0` |  |
| `armordamage` | armour lost recently | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `duration`=`0.8`, `proportional`=`0` |  |
| `bar_armor` | armour as a bar | `0` | `height`=`16`, `width`=`64`, `direction`=`1`, `color_noarmor`=`128 128 128 64`, `color_ga_over`=`48 160 0 128`, `color_ga`=`32 128 0 128`, `color_ya`=`192 128 0 128`, `color_ra`=`128 0 0 128`, `color_unnatural`=`255 255 255 128` | yes |
| `bar_health` | health as a bar | `0` | `height`=`16`, `width`=`64`, `direction`=`0`, `color_nohealth`=`128 128 128 64`, `color_normal`=`32 64 128 128`, `color_mega`=`64 96 128 128`, `color_twomega`=`128 128 255 128`, `color_unnatural`=`255 255 255 128` | yes |
| `centerprint` | centre-printed messages | `0` | `scale`=`1`, `proportional`=`0`, `speed`=`8` | yes |
| `clock` | time of day | `0` | `big`=`1`, `style`=`0`, `scale`=`1`, `blink`=`1`, `format`=`0`, `proportional`=`0`, `content`=`0` | yes |
| `democlock` | demo time | `1` | `big`=`0`, `style`=`0`, `scale`=`1`, `blink`=`0`, `proportional`=`0` | yes |
| `face` | the face | `1` | `scale`=`1` |  |
| `fps` | frame rate | `1` | `show_min`=`0`, `style`=`0`, `title`=`1`, `scale`=`1`, `drop`=`70`, `proportional`=`0` | yes |
| `frags` | frags of every player | `0` | `cell_width`=`32`, `cell_height`=`8`, `rows`=`1`, `cols`=`4`, `space_x`=`1`, `space_y`=`1`, `teamsort`=`0`, `strip`=`1`, `vertical`=`0`, `hidefrags`=`0`, `shownames`=`0`, `showteams`=`0`, `wipeout`=`1`, `padtext`=`1`, `showself_always`=`1`, `extra_spec_info`=`ALL`, `fliptext`=`0`, `style`=`0`, `bignum`=`0`, `colors_alpha`=`1.0`, `maxname`=`16`, `notintp`=`0`, `fixedwidth`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `framestats` | per-frame renderer statistics | `0` | `scale`=`1`, `proportional`=`0`, `amfstats`=`0` | yes |
| `frametime` | frame time | `0` | `show_max`=`1`, `style`=`0`, `title`=`1`, `scale`=`1`, `spike`=`10`, `proportional`=`0` | yes |
| `gameclock` | match clock | `1` | `big`=`1`, `style`=`0`, `scale`=`1`, `blink`=`1`, `countdown`=`0`, `offset`=`0`, `proportional`=`0` | yes |
| `gamesummary` | pickup counts of the match | `0` | `style`=`0`, `onlytp`=`0`, `scale`=`1`, `format`=`RYM myr`, `circles`=`0`, `ratio`=`4`, `flash`=`1`, `proportional`=`0` | yes |
| `group1` | an empty frame or picture to place others on (group 1) | `0` | `name`=`group1`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group2` | an empty frame or picture to place others on (group 2) | `0` | `name`=`group2`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group3` | an empty frame or picture to place others on (group 3) | `0` | `name`=`group3`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group4` | an empty frame or picture to place others on (group 4) | `0` | `name`=`group4`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group5` | an empty frame or picture to place others on (group 5) | `0` | `name`=`group5`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group6` | an empty frame or picture to place others on (group 6) | `0` | `name`=`group6`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group7` | an empty frame or picture to place others on (group 7) | `0` | `name`=`group7`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group8` | an empty frame or picture to place others on (group 8) | `0` | `name`=`group8`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `group9` | an empty frame or picture to place others on (group 9) | `0` | `name`=`group9`, `width`=`64`, `height`=`64`, `picture`=(empty), `pic_alpha`=`1.0`, `pic_scalemode`=`0` |  |
| `gun` | current weapon | `0` | `wide`=`0`, `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun2` | shotgun | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun3` | super shotgun | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun4` | nailgun | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun5` | super nailgun | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun6` | grenade launcher | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun7` | rocket launcher | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `gun8` | lightning gun | `1` | `wide`=`0`, `style`=`0`, `scale`=`1`, `proportional`=`0`, `frame_hide`=`0` |  |
| `health` | health value | `1` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `proportional`=`0` |  |
| `healthdamage` | health lost recently | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`3`, `duration`=`0.8`, `proportional`=`0` |  |
| `iammo` | icon of the current ammo | `1` | `style`=`0`, `scale`=`1`, `show_always`=`0` |  |
| `iammo1` | shells icon | `0` | `style`=`2`, `scale`=`1` |  |
| `iammo2` | nails icon | `0` | `style`=`2`, `scale`=`1` |  |
| `iammo3` | rockets icon | `0` | `style`=`2`, `scale`=`1` |  |
| `iammo4` | cells icon | `0` | `style`=`2`, `scale`=`1` |  |
| `iarmor` | armour icon | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `itemsclock` | respawn clocks of major items | `0` | `timelimit`=`5`, `style`=`0`, `scale`=`1`, `filter`=(empty), `proportional`=`0`, `backpacks`=`0` |  |
| `key1` | silver key | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `key2` | gold key | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `keys` | which keys are held (for videos) | `0` | `scale`=`2`, `player`=(empty) |  |
| `net` | network statistics | `0` | `period`=`1`, `scale`=`1`, `proportional`=`0` | yes |
| `netgraph` | the netgraph | `0` | `swap_x`=`0`, `swap_y`=`0`, `inframes`=`0`, `scale`=`256`, `ploss`=`1`, `width`=`256`, `height`=`32`, `lostscale`=`1`, `alpha`=`1` | yes |
| `netproblem` | a network-problem icon | `1` | `scale`=`1` |  |
| `notify` | notify (chat) lines | `0` | `rows`=`4`, `cols`=`30`, `scale`=`1`, `time`=`4`, `proportional`=`0` | yes |
| `ownfrags` | your last frag, briefly | `1` | `color`=`255 255 255`, `timeout`=`3`, `scale`=`1.5`, `proportional`=`0` |  |
| `pent` | pentagram | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `ping` | ping and loss | `0` | `period`=`1`, `show_pl`=`1`, `show_min`=`0`, `show_max`=`0`, `show_dev`=`0`, `style`=`0`, `blink`=`1`, `scale`=`1`, `proportional`=`0` | yes |
| `qtv_buffer` | broadcast buffer | `0` | `scale`=`1`, `proportional`=`0` |  |
| `quad` | quad | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `radar` | an overhead map with players and items | `0` | `opacity`=`0.5`, `width`=`30%`, `height`=`25%`, `autosize`=`0`, `show_powerups`=`1`, `show_names`=`0`, `highlight_color`=`yellow`, `highlight`=`0`, `player_size`=`10`, `show_height`=`1`, `show_stats`=`1`, `fade_players`=`1`, `show_hold`=`0`, `weaponfilter`=`gl rl lg`, `itemfilter`=`backpack quad pent armor mega`, `otherfilter`=`projectiles gibs explosions shotgun`, `onlytp`=`0`, `scale`=`1`, `simpleitems`=`1`, `colornames`=`0`, `proportional`=`0` | yes |
| `ring` | ring | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `score_bar` | a formatted score line | `0` | `style`=`0`, `scale`=`1`, `format_small`=`&c69f%T&r:%t &cf10%E&r:%e $[%D$]`, `format_big`=`%t:%e:%Z`, `format_reversed_big`=(empty), `format_reversed_small`=(empty), `frag_length`=`0`, `proportional`=`0` | yes |
| `score_difference` | the score difference | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`0`, `colorize`=`1`, `proportional`=`0` |  |
| `score_enemy` | the enemy's score | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`0`, `colorize`=`0`, `proportional`=`0` |  |
| `score_position` | your rank | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`0`, `colorize`=`1`, `proportional`=`0` |  |
| `score_team` | your team's score | `0` | `style`=`0`, `scale`=`1`, `align`=`right`, `digits`=`0`, `colorize`=`0`, `proportional`=`0` |  |
| `scoreclock` | clock on the scoreboard and intermission | `1` | `scale`=`1`, `format`=`%d-%m-%Y %H:%M:%S`, `proportional`=`0` |  |
| `scoremapname` | map name on the scoreboard | `0` | `style`=`0`, `proportional`=`0`, `scale`=`1` |  |
| `sigil1` | rune 1 | `0` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `sigil2` | rune 2 | `0` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `sigil3` | rune 3 | `0` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `sigil4` | rune 4 | `0` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `speed` | speed as a bar | `0` | `xyz`=`0`, `width`=`160`, `height`=`15`, `opacity`=`1.0`, `tick_spacing`=`0.2`, `color_stopped`=`52`, `color_normal`=`100`, `color_fast`=`72`, `color_fastest`=`216`, `color_insane`=`229`, `vertical`=`0`, `vertical_text`=`1`, `text_align`=`1`, `style`=`0`, `scale`=`1`, `proportional`=`0` | yes |
| `speed2` | speed as a dial | `0` | `xyz`=`0`, `opacity`=`1.0`, `color_stopped`=`52`, `color_normal`=`100`, `color_fast`=`72`, `color_fastest`=`216`, `color_insane`=`229`, `radius`=`50.0`, `wrapspeed`=`500`, `orientation`=`0`, `scale`=`1`, `proportional`=`0` | yes |
| `static_text` | fixed text | `0` | `big`=`0`, `style`=`0`, `scale`=`1`, `text`=(empty), `textalign`=`left`, `proportional`=`0` |  |
| `suit` | biosuit | `1` | `style`=`0`, `scale`=`1`, `proportional`=`0` |  |
| `teamfrags` | frags of every team | `1` | `cell_width`=`32`, `cell_height`=`8`, `rows`=`1`, `cols`=`2`, `space_x`=`1`, `space_y`=`1`, `strip`=`1`, `vertical`=`0`, `shownames`=`0`, `padtext`=`1`, `fliptext`=`1`, `style`=`0`, `extra_spec_info`=`1`, `onlytp`=`0`, `bignum`=`0`, `colors_alpha`=`1.0`, `maxname`=`16`, `scale`=`1`, `proportional`=`0` |  |
| `teamholdbar` | team item control as a bar | `0` | `opacity`=`0.8`, `width`=`200`, `height`=`8`, `vertical`=`0`, `vertical_text`=`0`, `show_text`=`1`, `onlytp`=`0`, `scale`=`1`, `proportional`=`0` | yes |
| `teamholdinfo` | which team holds which items | `0` | `opacity`=`0.8`, `width`=`200`, `height`=`8`, `onlytp`=`0`, `style`=`1`, `itemfilter`=`quad ra ya ga mega pent rl quad`, `scale`=`1`, `proportional`=`0` | yes |
| `teaminfo` | teammates' status table | `0` | `layout`=`%p%n $x10%l$x11 %a/%H %w`, `align_right`=`0`, `loc_width`=`5`, `name_width`=`6`, `low_health`=`25`, `armor_style`=`3`, `weapon_style`=`0`, `show_ammo`=`0`, `show_countdown`=`1`, `show_enemies`=`0`, `show_self`=`1`, `show_headers`=`1`, `scale`=`1`, `powerup_style`=`1`, `flag_style`=`1`, `proportional`=`0`, `header_spacing`=`1` |  |
| `teamstackbar` | team stack as a bar | `0` | `opacity`=`0.8`, `width`=`200`, `height`=`8`, `vertical`=`0`, `vertical_text`=`0`, `show_text`=`1`, `onlytp`=`0`, `scale`=`1`, `simpleitems`=`1`, `proportional`=`0` | yes |
| `tracker` | frag tracker lines | `0` | `scale`=`1`, `proportional`=`0`, `name_width`=`0`, `align_right`=`0`, `image_scale`=`1` |  |
| `tracking` | who you track | `1` | `format`= "Tracking: %t %n, JUMP for next" with "Tracking:" and "JUMP" in the gold (high-bit) characters, `scale`=`1`, `proportional`=`0` | yes |
| `vidlag` | display latency | `0` | `style`=`0`, `scale`=`1`, `proportional`=`0` | yes |
| `weaponstats` | weapon accuracy | `0` | `format`=`&c990sg&r:%2 &c099ssg&r:%3 &c900rl&r:#7 &c009lg&r:%8`, `textalign`=`center`, `scale`=`1`, `proportional`=`0` |  |

Every element's placement defaults (anchor, alignment, offset, frame, order). A dash means the element has no such setting.

| Element | `place` | `align_x` / `pos_x` | `align_y` / `pos_y` | `frame` / `frame_color` | `order` |
| --- | --- | --- | --- | --- | --- |
| `ammo` | `health` | after / 32 | center / 0 | 0 / 0 0 0 | 0 |
| `ammo1` | `ibar` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `ammo2` | `ammo1` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `ammo3` | `ammo2` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `ammo4` | `ammo3` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `armor` | `face` | before / -32 | center / 0 | 0 / 0 0 0 | 0 |
| `armordamage` | `armor` | left / 0 | before / 0 | 0 / 0 0 0 | 0 |
| `bar_armor` | `armor` | left / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `bar_health` | `health` | right / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `centerprint` | `screen` | center / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `clock` | `top` | right / 0 | console / 0 | 0 / 0 0 0 | 8 |
| `democlock` | `top` | right / 0 | console / 8 | 0 / 0 0 0 | 7 |
| `face` | `screen` | center / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `fps` | `gameclock` | center / 0 | after / 0 | 0 / 0 0 0 | 9 |
| `frags` | `top` | right / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `framestats` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `frametime` | `fps` | center / 0 | after / 0 | 0 / 0 0 0 | 9 |
| `gameclock` | `top` | right / 0 | console / 0 | 0 / 0 0 0 | 8 |
| `gamesummary` | `top` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `group1` | `screen` | left / 0 | top / 0 | .5 / 0 0 0 | 0 |
| `group2` | `screen` | center / 0 | top / 0 | .5 / 0 0 0 | 0 |
| `group3` | `screen` | right / 0 | top / 0 | .5 / 0 0 0 | 0 |
| `group4` | `screen` | left / 0 | center / 0 | .5 / 0 0 0 | 0 |
| `group5` | `screen` | center / 0 | center / 0 | .5 / 0 0 0 | 0 |
| `group6` | `screen` | right / 0 | center / 0 | .5 / 0 0 0 | 0 |
| `group7` | `screen` | left / 0 | bottom / 0 | .5 / 0 0 0 | 0 |
| `group8` | `screen` | center / 0 | bottom / 0 | .5 / 0 0 0 | 0 |
| `group9` | `screen` | right / 0 | bottom / 0 | .5 / 0 0 0 | 0 |
| `gun` | `ibar` | center / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `gun2` | `ibar` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `gun3` | `gun2` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `gun4` | `gun3` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `gun5` | `gun4` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `gun6` | `gun5` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `gun7` | `gun6` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `gun8` | `gun7` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `health` | `face` | after / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `healthdamage` | `health` | left / 0 | before / 0 | 0 / 0 0 0 | 0 |
| `iammo` | `ammo` | before / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `iammo1` | `ibar` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `iammo2` | `iammo1` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `iammo3` | `iammo2` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `iammo4` | `iammo3` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `iarmor` | `armor` | before / 0 | center / 0 | 0 / 0 0 0 | 0 |
| `itemsclock` | `screen` | right / 0 | center / 0 | 0 / 0 0 0 | 1 |
| `key1` | `ibar` | top / 0 | left / 64 | 0 / 0 0 0 | 0 |
| `key2` | `key1` | left / 0 | after / 0 | 0 / 0 0 0 | 0 |
| `keys` | `screen` | right / 0 | center / 0 | 0.5 / 20 20 20 | 1 |
| `net` | `top` | left / 0 | center / 0 | 0.2 / 0 0 0 | 7 |
| `netgraph` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `netproblem` | `top` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `notify` | `top` | left / 0 | top / 0 | 0 / 0 0 0 | 8 |
| `ownfrags` | `screen` | center / 0 | top / 50 | 0.2 / 0 0 100 | 1 |
| `pent` | `ring` | left / 0 | after / 0 | 0 / 0 0 0 | 0 |
| `ping` | `screen` | left / 0 | bottom / 0 | 0 / 0 0 0 | 9 |
| `qtv_buffer` | `screen` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `quad` | `suit` | left / 0 | after / 0 | 0 / 0 0 0 | 0 |
| `radar` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `ring` | `key2` | left / 0 | after / 0 | 0 / 0 0 0 | 0 |
| `score_bar` | `screen` | center / 0 | console / 0 | 0.5 / 0 0 0 | 0 |
| `score_difference` | `score_enemy` | after / 0 | bottom / 0 | 0.5 / 0 0 0 | 0 |
| `score_enemy` | `score_team` | after / 0 | bottom / 0 | 0.5 / 32 4 0 | 0 |
| `score_position` | `score_difference` | after / 0 | bottom / 0 | 0.5 / 0 0 0 | 0 |
| `score_team` | `screen` | left / 0 | bottom / 0 | 0.5 / 4 8 32 | 0 |
| `scoreclock` | `screen` | center / 0 | bottom / -10 | 0 / 0 0 0 | 8 |
| `scoremapname` | `screen` | right / 0 | bottom / -10 | 0 / 0 0 0 | 8 |
| `sigil1` | `ibar` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `sigil2` | `sigil1` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `sigil3` | `sigil2` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `sigil4` | `sigil3` | after / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `speed` | `top` | center / 0 | bottom / -5 | 0 / 0 0 0 | 7 |
| `speed2` | `top` | center / 0 | bottom / 0 | 0 / 0 0 0 | 7 |
| `static_text` | `screen` | left / 0 | top / 0 | 0 / 0 0 0 | 0 |
| `suit` | `pent` | left / 0 | after / 0 | 0 / 0 0 0 | 0 |
| `teamfrags` | `ibar` | center / 0 | before / 0 | 0 / 0 0 0 | 0 |
| `teamholdbar` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `teamholdinfo` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `teaminfo` | `` | right / 0 | center / 0 | 0.2 / 20 20 20 | 0 |
| `teamstackbar` | `top` | left / 0 | bottom / 0 | 0 / 0 0 0 | 0 |
| `tracker` | `screen` | left / 0 | top / 48 | 0 / 0 0 0 | 0 |
| `tracking` | `face` | center / 0 | before / 0 | 0 / 0 0 0 | 9 |
| `vidlag` | `top` | right / 0 | top / 0 | 0 / 0 0 0 | 9 |
| `weaponstats` | `screen` | center / 0 | bottom / 0 | 0 / 0 0 0 | 0 |

### 1.19 Rulesets: what the **rules** mark means

`ruleset` (default `default`) names one of six rule sets a tournament can require: `default`, `smackdown`, `thunderdome`, `mtfl`, `qcon`, `smackdrive`. It can be changed only while disconnected, and the chosen name is sent as user info `*rsn`. (R)

| Ruleset | What it locks, caps or forbids |
| --- | --- |
| `default` | Nothing beyond the always-on rules below |
| `smackdown` | Locks `allow_scripts` 0, `cl_iDrive` 0, `cl_safestrafe` 0, `cl_hud` 0, `cl_rollalpha` 20, `r_shiftbeam` 0, `scr_allowsnap` 1. With independent physics on, forces `cl_c2spps` 0 and makes it read-only. Frame-rate cap 77 |
| `smackdrive` | As `smackdown` without the `cl_iDrive` and `cl_safestrafe` locks |
| `qcon` | Locks `allow_scripts`, `cl_iDrive`, `cl_hud`, `cl_rollalpha`, `r_shiftbeam` as `smackdown` (not `cl_safestrafe`, not `scr_allowsnap`); at most 10 `wait` commands in a row; no world-edge or model outlines outside demos. Cap 77 |
| `thunderdome` | Locks `allow_scripts`, `cl_iDrive`, `cl_hud` and `r_shiftbeam`; particle settings are not restricted here. Cap 77 |
| `mtfl` | Locks `r_drawflat` 0, `gl_detpacklights` 0, `gl_textureless` 0, `r_fullbrightSkins` 0, `vid_software_palette` 1, `cl_c2spps` 0; caps `gl_picmip` at 3; `gl_max_size` at least 512; blocks the player-count macros |

The four strict sets (`smackdown`, `smackdrive`, `qcon`, `thunderdome`) also: cap the physics rate at 77 frames a second (the default rule set's cap is 72) when independent physics is off and the server sets no `maxfps` of its own; refuse message triggers, `packet`, logging, the inter-process commands, `exec` and `set_calc`/`set_eval`/`set_ex` from triggers, and `play`; restrict the particle settings (all but `thunderdome`); disable `timerefresh` during a match ("Timerefresh is disabled during match"); keep `gl_outline_scale_model` within 0–1 (`smackdown`, `smackdrive`, `qcon`; otherwise 0–5). Outside `default`, the eyes and backpack models ignore custom textures, glow maps and `gl_fb_models`. A roll-angle restriction is declared but nothing reads it (D: no effect). (R)

Always on, whatever the rule set (R):
- `r_fullbrightSkins` is capped by the server info key `fbskins` (0 on team-fortress servers) and the change is announced in chat.
- `allow_scripts` cannot change while the server status says a match is under way ("left" in the status), and changes are announced.
- `cl_delay_packet` is limited to 0 to twice the server's cap, refused during a match unless the server is in standby (or team fortress), and announced.
- `cl_iDrive` is 0 or 1, changeable only in standby, announced. `cl_fakeshaft` changes are announced.
- `cl_independentPhysics` changes only while disconnected.

### 1.20 The built-in server (168 settings)

The client compiles in a listen/local server. Its settings are the dedicated server program's, with the same names, defaults and meanings (`server-inventory.md`, `server-settings.md`), and appear in the client's `cvarlist`. All of them except the 17 in the table below are in `server-inventory.md`. The full list (R):

`allow_download`, `allow_download_demos`, `allow_download_maps`, `allow_download_models`, `allow_download_other`, `allow_download_pakmaps`, `allow_download_skins`, `allow_download_sounds`, `coop`, `deathmatch`, `download_map_url`, `extralogname`, `filterban`, `frag_log_type`, `fraglimit`, `halflifebsp`, `hostname`, `maxclients`, `maxfps`, `maxspectators`, `maxvip_spectators`, `pausable`, `pext_ezquake_verfortrans`, `pm_airstep`, `pm_bunnyspeedcap`, `pm_ktjump`, `pm_pground`, `pm_rampjump`, `pm_slidefix`, `qconsole_log_say`, `qtv_maxstreams`, `qtv_password`, `qtv_pendingtimeout`, `qtv_reverse_address`, `qtv_sayenabled`, `qtv_stream_max_queue_size`, `qtv_streamport`, `qtv_streamtimeout`, `qwm_builddate`, `qwm_buildnum`, `qwm_fullname`, `qwm_homepage`, `qwm_name`, `qwm_platform`, `qwm_version`, `qws_builddate`, `qws_buildnum`, `qws_fullname`, `qws_homepage`, `qws_name`, `qws_platform`, `qws_version`, `registered`, `samelevel`, `serverdemo`, `skill`, `spectator_password`, `sv_accelerate`, `sv_admininfo`, `sv_airaccelerate`, `sv_allowlastscores`, `sv_antilag`, `sv_antilag_no_pred`, `sv_antilag_projectiles`, `sv_bigcoords`, `sv_bspversion`, `sv_calcphs`, `sv_cheats`, `sv_crypt_rcon`, `sv_debug_antilag`, `sv_debug_usercmd`, `sv_default_name`, `sv_demo_format`, `sv_demo_rd_predinfo`, `sv_demoCacheSize`, `sv_demoClearOld`, `sv_demoDir`, `sv_demoDirAlt`, `sv_demoExtraNames`, `sv_demofps`, `sv_demoIdlefps`, `sv_demoMaxDirSize`, `sv_demoMaxSize`, `sv_demopings`, `sv_demoPrefix`, `sv_demoRegexp`, `sv_demoSuffix`, `sv_demotxt`, `sv_demoUseCache`, `sv_downloadchunksperframe`, `sv_enable_cmd_minping`, `sv_extlimits`, `sv_forcenick`, `sv_forcenqprogs`, `sv_forcespec_onfull`, `sv_friction`, `sv_getrealip`, `sv_gravity`, `sv_hashpasswords`, `sv_kicktop`, `sv_kickuserinfospamcount`, `sv_kickuserinfospamtime`, `sv_loadentfiles`, `sv_loadentfiles_dir`, `sv_local_addr`, `sv_logdir`, `sv_login`, `sv_login_web`, `sv_mapcheck`, `sv_maxdownloadrate`, `sv_maxedicts`, `sv_maxlogsize`, `sv_maxping`, `sv_maxpitch`, `sv_maxrate`, `sv_maxspeed`, `sv_maxtic`, `sv_maxuploadsize`, `sv_maxvelocity`, `sv_minping`, `sv_minpitch`, `sv_mintic`, `sv_mod_extensions`, `sv_mod_msg_file`, `sv_nailhack`, `sv_onDemoRemove`, `sv_onRecordFinish`, `sv_paused`, `sv_pext_replacementdeltas`, `sv_phs`, `sv_pr2references`, `sv_progsname`, `sv_progtype`, `sv_rconlim`, `sv_reconnectlimit`, `sv_registrationinfo`, `sv_reliable_sound`, `sv_sayteam_to_spec`, `sv_serveme_fix`, `sv_serverip`, `sv_showdrop`, `sv_showpackets`, `sv_silentrecord`, `sv_specprint`, `sv_spectalk`, `sv_spectatormaxspeed`, `sv_speedcheck`, `sv_stopspeed`, `sv_timestamplen`, `sv_unfake`, `sv_use_dns`, `sv_voip`, `sv_voip_echo`, `sv_voip_record`, `sv_wateraccelerate`, `sv_waterfriction`, `sys_command_line`, `sys_restart_on_error`, `sys_select_timeout`, `teamplay`, `telnet_log_level`, `timelimit`, `timeout`, `vip_password`, `vip_values`, `vm_rtChecks`, `watervis`, `zombietime`.

The 17 not in the server inventory (R; the meaning is from how the program uses each, D where marked):

| Setting | Default | What it does |
| --- | --- | --- |
| `qtv_reverse_address` | (empty) | Host and TCP port of a broadcast proxy the server connects out to (a reverse broadcast); refused unless it is an address with a port |
| `sv_demoCacheSize` | `0` | Read-only: the memory the demo writer has for its cache, in KiB |
| `sv_demoClearOld` | `0` | When the demo folder passes its size limit, how many of the oldest demos to delete; 0 refuses to record and says to raise the limit |
| `sv_demoDirAlt` | (empty) | A second demo folder searched when a requested demo is not in the main one |
| `sv_demoExtraNames` | `0` | Above 0: put extra details in demo file names (D) |
| `sv_demoIdlefps` | `10` | The lowest frame rate a demo is written at while nothing changes, kept within 4 to 30 |
| `sv_demoMaxDirSize` | `102400` | Size limit of the demo folder in KiB (0 none) |
| `sv_demoMaxSize` | `20480` | Size limit of one demo in KiB; a demo passing it stops (0 none) |
| `sv_demoPrefix` | (empty) | Text put before every demo file name |
| `sv_demoRegexp` | a pattern matching `.mvd` and its packed forms | Which files the demo listing commands show |
| `sv_demoSuffix` | (empty) | Text put after every demo file name |
| `sv_demoUseCache` | `0` | 1: buffer demo writes in memory |
| `sv_onDemoRemove` | (empty) | A script run with the folder and name when a demo is removed |
| `sv_onRecordFinish` | (empty) | A script run when a demo finishes recording (not for demos thrown away) |
| `sv_showdrop` | `0` | 1: print dropped packets (the server side) |
| `sv_showpackets` | `0` | 1: print every packet (the server side) |
| `vm_rtChecks` | `1` | Bit mask of run-time safety checks in the compiled-module virtual machine |

### 1.21 Old names that still work

An old name typed in the console or found in a config is translated to the new one: the setting or command runs as if the new name had been typed. Tab completion offers the old names too (R). They are kept in a list of their own, so `cvarlist` and `cmdlist` do not show them and a save never writes them (D).

| Old name | Now |
| --- | --- |
| `loadpak`, `removepak`, `path`, `dir`, `locate` | `fs_loadpak`, `fs_removepak`, `fs_path`, `fs_dir`, `fs_locate` |
| `nosound`, `precache`, `loadas8bit` | `s_nosound`, `s_precache`, `s_loadas8bit` |
| `ambient_level`, `ambient_fade` | `s_ambientlevel`, `s_ambientfade` |
| `snd_show`, `snd_restart` | `s_show`, `s_restart` |
| `cl_chatsound` | `s_chat_custom` |
| `con_sound_mm1_file`, `con_sound_mm2_file`, `con_sound_spec_file`, `con_sound_other_file` | `s_mm1_file`, `s_mm2_file`, `s_spec_file`, `s_otherchat_file` |
| `con_sound_mm1_volume`, `con_sound_mm2_volume`, `con_sound_spec_volume`, `con_sound_other_volume` | `s_mm1_volume`, `s_mm2_volume`, `s_spec_volume`, `s_otherchat_volume` |
| `scr_weaponstats_order` | `hud_weaponstats_format` |
| `scr_weaponstats_frame_color`, `scr_weaponstats_scale`, `scr_weaponstats_x`, `scr_weaponstats_y` | `hud_weaponstats_frame_color`, `hud_weaponstats_scale`, `hud_weaponstats_x`, `hud_weaponstats_y` |
| `gl_smoothfont` | `r_smoothtext` |
| `gl_lighting_colour` | `gl_lighting_color` |
| `cl_truelightning` | `cl_fakeshaft` |
| `demotimescale` | `cl_demospeed` |
| `gamma`, `contrast` | `gl_gamma`, `gl_contrast` |
| `gl_gammacorrection` | `vid_gammacorrection` |
| `vid_framebuffer_palette` | `vid_software_palette` |
| `scr_printspeed` | `scr_centerspeed` |
| `loadloc`, `saveloc`, `addloc`, `removeloc`, `clearlocs` | `locations_loadfile`, `locations_savefile`, `locations_add`, `locations_remove`, `locations_clearall` |
| `play` with nothing after it | ignored silently |

Note: `hud_weaponstats_x` and `hud_weaponstats_y` are what the old names point at; the element's own offsets are `hud_weaponstats_pos_x`/`_pos_y`, so the two old position names lead nowhere (D).

## 2. Every command, by area

Client commands: 371 in §2.1–2.13 (a `+name` / `-name` pair is one row and counts as two), 69 of the built-in server (§2.14), 83 HUD element commands and 46 `+hud_`/`-hud_` commands (§2.15). Commands that only exist in an option the default build leaves out are not listed: the IRC client, the CD-audio commands, the file-system and shell helpers of the dedicated server program, its web calls and memory-cache reports (R).

How a typed line is resolved, in order: a command of that name; else a setting (the line shows or sets it); else an alias; else an old name (§1.21); else "Unknown command" is printed (only while `cl_warncmd` is 1 or `developer` is on). An unknown word is NOT sent to the server. Commands marked **fwd** have no handler of their own and are sent to the server as typed, with the command word in lower case. §8 lists every forwarding path. (R)

### 2.1 Movement, weapons and buttons (51 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `+attack` / `-attack` | While held, fire |  |
| `+attack2` / `-attack2` | While held, the second attack button (sent to the mod) |  |
| `+back` / `-back` | While held, move back |  |
| `centerview` | Level the view |  |
| `+fire` / `-fire` | While held, pick the best owned weapon from the listed weapon numbers and fire |  |
| `+fire_ar` / `-fire_ar` | Like +fire, but releasing one of several held keys returns to the weapon of the key still held |  |
| `force_centerview` | Level the view even where centerview is ignored |  |
| `+forward` / `-forward` | While held, move forward |  |
| `impulse` | impulse <n> [n ...]: send an impulse; with several, at once the best owned weapon of the list (no preselection) |  |
| `+jump` / `-jump` | While held, jump (one jump per press); swims up in liquids with cl_smartjump |  |
| `+klook` / `-klook` | While held, forward and back keys look up and down |  |
| `+left` / `-left` | While held, turn left |  |
| `+lookdown` / `-lookdown` | While held, look down |  |
| `+lookup` / `-lookup` | While held, look up |  |
| `+mlook` / `-mlook` | While held, the mouse looks up and down |  |
| `+movedown` / `-movedown` | While held, swim or fly down |  |
| `+moveleft` / `-moveleft` | While held, strafe left |  |
| `+moveright` / `-moveright` | While held, strafe right |  |
| `+moveup` / `-moveup` | While held, swim or fly up |  |
| `+right` / `-right` | While held, turn right |  |
| `rotate` | rotate <degrees>: turn the view by that much at once |  |
| `+showscores` / `-showscores` | While held, show the scoreboard |  |
| `+showteamscores` / `-showteamscores` | While held, show the team scoreboard |  |
| `+speed` / `-speed` | While held, run (or walk, when always-run is set) |  |
| `+strafe` / `-strafe` | While held, turn keys strafe |  |
| `+use` / `-use` | While held, the use button (sent to the mod) |  |
| `weapon` | weapon <n> [n ...]: choose the best owned weapon of the list (honours preselection settings) |  |
| `+zoom` / `-zoom` | While held, narrow the field of view (and scale sensitivity); restored on release |  |

### 2.2 Connecting, chat and the server (43 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `allskins` | Download every skin in use |  |
| `authenticate` | Log in to the server with the account token (again: cancel) |  |
| `calc_fov` | Convert an older wide-screen field of view to the current one |  |
| `cl_messages` | Print counts and sizes of server messages received |  |
| `cmd` | cmd <text>: send the text to the server as a command |  |
| `color` | color <shirt> [pants]: set your colours |  |
| `connect` | connect <address>: connect to a server |  |
| `connectbr` | connectbr <address>: connect by the best route |  |
| `disconnect` | Leave the server, demo or broadcast |  |
| `dns` | Look up a name or an address |  |
| `download` | download <file>: fetch a file from the server |  |
| `fly` | Toggle flying (sent to the server unchanged) | fwd |
| `fullinfo` | Set the whole player info string at once |  |
| `give` | give <item> [n]: cheat (sent to the server unchanged) | fwd |
| `god` | Toggle invulnerability (sent to the server unchanged) | fwd |
| `join` | join [address]: play (on the server or by reconnecting) |  |
| `kill` | Suicide (sent to the server unchanged) | fwd |
| `noclip` | Toggle no-clipping (sent to the server unchanged) | fwd |
| `observe` | observe [address]: spectate |  |
| `observebr` | observebr <address>: spectate by the best route |  |
| `packet` | packet <address> <text>: send a connectionless packet; refused during a match under some rulesets |  |
| `pause` | Pause a demo locally; otherwise ask the server to pause |  |
| `pext_list` | Print the protocol extensions in use |  |
| `qstat` | qstat <address>: query a server's status without connecting |  |
| `quit` | Quit |  |
| `qwurl` | qwurl <link>: connect by a game link |  |
| `rcon` | rcon <command>: run a command on the server with rcon_password |  |
| `reconnect` | Reconnect to the last server |  |
| `register_qwurl_protocol` | Register the game link scheme with the system (Windows and Linux) |  |
| `say` | say <text>: chat to everyone |  |
| `say_team` | say_team <text>: chat to the team |  |
| `serverinfo` | Print the server's info |  |
| `setinfo` | setinfo [key value]: set a player info key (no arguments: list) |  |
| `showskins` | List players and their skins |  |
| `skins` | Reload skins |  |
| `tcpconnect` | tcpconnect <address>: connect over TCP |  |
| `togglespec` | Switch between player and spectator on the current server | spec |
| `user` | user <name or id>: print a player's info |  |
| `userdir` | Add a user folder to the search path |  |
| `userinfo` | Print your player info |  |
| `users` | List players with user ids and frags |  |
| `version` | Print the version |  |
| `z_ext_list` | Print the movement-protocol extensions in use |  |

### 2.3 Settings, aliases, binds and scripting (46 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `alias` | alias <name> <commands>: define an alias (no arguments: list) |  |
| `alias_in` | Insert a setting's value into an alias |  |
| `alias_out` | Remove a setting's value from an alias |  |
| `aliasedit` | Put an alias in the input line to edit |  |
| `aliaslist` | List aliases |  |
| `bind` | bind <key> [commands]: bind a key (no command: show) |  |
| `bindedit` | Put a bind in the input line to edit |  |
| `bindlist` | List binds |  |
| `clipboard` | Copy text to the system clipboard |  |
| `cmdlist` | cmdlist [pattern]: list commands |  |
| `cmdlist_re` | The same with a regular expression |  |
| `cvar_in` | Insert one setting's value into another |  |
| `cvar_out` | Remove one setting's value from another |  |
| `cvar_reset` | Reset a setting to its default |  |
| `cvar_reset_re` | Reset settings matching a regular expression |  |
| `cvaredit` | Put a setting in the input line to edit |  |
| `cvarlist` | cvarlist [pattern]: list settings |  |
| `cvarlist_re` | The same with a regular expression |  |
| `eval` | Evaluate an expression and print it |  |
| `exec` | exec <file>: run a config |  |
| `hash` | Print the engine's hash of a string |  |
| `if` | if <a> <op> <b> then <cmds> [else <cmds>]: conditional |  |
| `if_exists` | if_exists <type> <name> <cmd1> [cmd2]: run by whether a thing exists |  |
| `inc` | inc <setting> [n]: add to a setting |  |
| `macrolist` | List $macros |  |
| `serverexec` | Run a config in the built-in server's buffer |  |
| `set` | set <name> <value>: set or create a setting |  |
| `set_alias_str` | Set a setting to an alias's text |  |
| `set_bind_str` | Set a setting to a key's bind |  |
| `set_calc` | Set a setting to a calculation |  |
| `set_eval` | Set a setting to an expression's value |  |
| `set_ex` | Set a setting with macro expansion |  |
| `set_ex2` | The same without special-character parsing |  |
| `set_tp` | Set a teamplay setting (only those) |  |
| `tempalias` | Define an alias not saved to the config |  |
| `toggle` | toggle <setting>: flip between 0 and 1 |  |
| `toggle_re` | Flip settings matching a regular expression |  |
| `unalias` | Remove an alias |  |
| `unalias_re` | Remove aliases matching a regular expression |  |
| `unaliasall` | Remove every alias |  |
| `unbind` | Unbind a key |  |
| `unbindall` | Unbind every key |  |
| `unset` | Remove a user-created setting |  |
| `unset_re` | Remove user-created settings by regular expression |  |
| `viewalias` | Print an alias |  |
| `wait` | Wait one frame |  |

### 2.4 Console, chat input and ignore lists (18 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `calendar` | Print the date and a month calendar |  |
| `clear` | Clear the console |  |
| `date` | Print the date and time |  |
| `echo` | Print text |  |
| `ignore` | ignore <name or id>: ignore a player's chat |  |
| `ignore_id` | Ignore by user id |  |
| `ignore_team` | Ignore a team |  |
| `ignorelist` | List ignored players and teams |  |
| `log` | log <file>: copy the console into a file (no argument: stop) |  |
| `messagemode` | Type a chat line to everyone |  |
| `messagemode2` | Type a chat line to the team |  |
| `messagemodeqtvtogame` | Type a line from the broadcast into the game | demo |
| `toggleconsole` | Open or close the console |  |
| `unignore` | Stop ignoring a player |  |
| `unignore_id` | Stop ignoring by id |  |
| `unignore_team` | Stop ignoring a team |  |
| `unignoreall` | Clear the ignore list |  |
| `unignoreall_team` | Clear the team ignore list |  |

### 2.5 Teamplay, locations and triggers (42 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `enemycolor` | enemycolor <shirt> [pants] or off: force enemy colours |  |
| `filter` | filter [#tag ... \| clear]: show only team messages ending with one of these tags (no argument: list) |  |
| `locations_add` | Add a named location at your position |  |
| `locations_clearall` | Clear the locations |  |
| `locations_loadfile` | Load a location file |  |
| `locations_remove` | Remove the nearest location |  |
| `locations_savefile` | Save the locations |  |
| `mapgroup` | Make maps share textures, locations and settings |  |
| `msg_trigger` | msg_trigger <alias> <text> [-l level]: run an alias when a message contains text |  |
| `re_trigger` | re_trigger <name> <regexp>: define a regular-expression trigger (no argument: list) |  |
| `re_trigger_delete` | Delete a trigger |  |
| `re_trigger_disable` | Disable triggers |  |
| `re_trigger_enable` | Enable triggers |  |
| `re_trigger_match` | Feed a string to a trigger |  |
| `re_trigger_options` | Set a trigger's options |  |
| `skygroup` | Make maps share a sky |  |
| `teamcolor` | teamcolor <shirt> [pants] or off: force teammate colours |  |
| `tp_msgcoming` | Team message: coming from here |  |
| `tp_msgenemypwr` | Team message: someone has a powerup |  |
| `tp_msggetpent` | Team message: get the pentagram |  |
| `tp_msggetquad` | Team message: get the quad |  |
| `tp_msghelp` | Team message: help here |  |
| `tp_msgitemsoon` | Team message: item here spawns soon |  |
| `tp_msgkillme` | Team message: kill me here |  |
| `tp_msglost` | Team message: died here, what was lost |  |
| `tp_msgneed` | Team message: what you need |  |
| `tp_msgnocancel` | Team message: no or cancel |  |
| `tp_msgpoint` | Team message: the item you point at |  |
| `tp_msgquaddead` | Team message: quad is over |  |
| `tp_msgreplace` | Team message: replace me here |  |
| `tp_msgreport` | Team message: your status |  |
| `tp_msgsafe` | Team message: this place is safe |  |
| `tp_msgslipped` | Team message: an enemy slipped by here |  |
| `tp_msgtfconced` | Team message for the class-based mod: concussed |  |
| `tp_msgtook` | Team message: the item you took |  |
| `tp_msgtrick` | Team message: trick here |  |
| `tp_msgutake` | Team message: you take the item here |  |
| `tp_msgwaiting` | Team message: waiting here |  |
| `tp_msgyesok` | Team message: yes or ok |  |
| `tp_pickup` | Items that count as picked up for the macros |  |
| `tp_point` | Items that can be pointed at |  |
| `tp_took` | Items reported by the took macro |  |

### 2.6 Demos, broadcasts, spectating and match tools (56 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `autotrack` | Toggle following the best player (spectating and demos) | spec |
| `cam_angles` | Set the free camera's angles | spec |
| `cam_pos` | Set the free camera's position | spec |
| `demo_capture` | demo_capture start <seconds> [name] / stop: capture frames (or a video) of demo playback | demo |
| `demo_controls` | Toggle the on-screen demo controls | demo |
| `demo_jump` | demo_jump [+\|-]<time>: seek in a demo (seconds or m:ss) | demo |
| `demo_jump_end` | Seek to just before a demo's end | demo |
| `demo_jump_mark` | Seek forward to the next mark in a demo | demo |
| `demo_jump_status` | demo_jump_status <condition>: play fast until the tracked player's state matches | demo |
| `demo_playlist_clear` | Clear the demo playlist | demo |
| `demo_playlist_next` | Next playlist demo | demo |
| `demo_playlist_prev` | Previous playlist demo | demo |
| `demo_playlist_stop` | Stop the playlist | demo |
| `demo_setspeed` | demo_setspeed <percent>: playback speed | demo |
| `demo_voice_info` | List loaded demo voice tracks | demo |
| `demo_voice_load` | Load a voice track for the demo | demo |
| `demo_voice_unload` | Unload one track or all | demo |
| `demo_voice_volume` | Show or set a track's volume | demo |
| `easyrecord` | easyrecord [name]: record a demo named by the match name patterns |  |
| `find` | find [name]: list players on all servers from the broadcast service (no name: all) |  |
| `find_and_follow` | find_and_follow <name>: join the server where that player is |  |
| `find_update` | Refresh that player list |  |
| `match_forcestart` | Act as if a match started (starts automatic recording) |  |
| `match_format_macrolist` | List the codes usable in recording name patterns |  |
| `match_save` | Keep the last automatic recording or log |  |
| `mvd_dumpstats` | Write statistics of the playing multi-view demo to an XML file | demo |
| `mvd_list_items` | List the respawn clocks in a demo | demo |
| `mvd_name_item` | Add or rename a respawn clock for an item | demo |
| `mvd_remove_item` | Remove a respawn clock | demo |
| `mvdrecord` | mvdrecord <name>: save the watched broadcast as a multi-view demo |  |
| `mvdstop` | Stop that recording |  |
| `observeqtv` | observeqtv [address]: watch the broadcast of a server |  |
| `playdemo` | playdemo <name>: play a demo (any supported format, archives too) |  |
| `qtv` | qtv <address>: watch a broadcast through the broadcast service |  |
| `+qtv_delay` / `-qtv_delay` | While held, pause a broadcast to grow its buffer; on release keep that as delay | demo |
| `-qtv_delay` | Release of the hold above | demo |
| `qtv_fixuser` | Override a viewer's info fields in a broadcast | demo |
| `qtv_query_demolist` | List a broadcast proxy's recorded demos |  |
| `qtv_query_sourcelist` | List a broadcast proxy's live streams |  |
| `qtv_update` | Refresh the broadcast list |  |
| `qtvplay` | qtvplay [stream@]host[:port] [password]: watch a broadcast |  |
| `qtvreconnect` | Reconnect to the last broadcast |  |
| `qtvusers` | List viewers of the current broadcast | demo |
| `record` | record <name> [server]: record a demo (on a listen server the server records) |  |
| `recordqwd` | Record a client demo even on a listen server |  |
| `stop` | Stop recording |  |
| `stopqwd` | Stop a client demo recording |  |
| `timedemo` | timedemo <name>: play a demo as fast as possible and print frames, seconds and fps |  |
| `timedemo2` | timedemo2 <name> [fps]: like timedemo at a fixed simulated rate |  |
| `track` | track <name or id>: spectate that player | spec |
| `track1` | track1 <name or id>: that player in multi-view slot 1 | spec |
| `track2` | Same for slot 2 | spec |
| `track3` | Same for slot 3 | spec |
| `track4` | Same for slot 4 | spec |
| `trackkiller` | Track whoever killed the tracked player | spec |
| `trackteam` | trackteam <team>: give every multi-view slot to one team | spec |

### 2.7 HUD and the HUD editor (26 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `align` | align <element> <x> <y>: set an element's alignment |  |
| `+cl_wp_stats` / `-cl_wp_stats` | While held, show the weapon statistics element |  |
| `-cl_wp_stats` | Release of the hold above |  |
| `hide` | hide <element or all>: hide elements |  |
| `hud262_add` | Add an element of the older string HUD (text, a setting's value or a picture) |  |
| `hud262_alpha` | Set its transparency |  |
| `hud262_bg` | Set its background colour |  |
| `hud262_blink` | Make it blink |  |
| `hud262_bringtofront` | Draw it and later ones last |  |
| `hud262_disable` | Hide string-HUD elements |  |
| `hud262_enable` | Show string-HUD elements |  |
| `hud262_list` | List string-HUD elements |  |
| `hud262_move` | Move one by character cells |  |
| `hud262_position` | Place one on the screen |  |
| `hud262_remove` | Delete one |  |
| `hud262_width` | Fix one's width |  |
| `hud_editor` | Open or close the HUD editor |  |
| `hud_export` | hud_export <file>: write the element HUD's settings to a config file |  |
| `hud_fps_min_reset` | Reset the lowest-fps and worst-frame-time readings |  |
| `hud_recalculate` | Re-place every element |  |
| `move` | move <element> <dx> <dy>: shift an element |  |
| `order` | order <element> [n \| backward \| forward \| front \| back]: change or print drawing order |  |
| `place` | place <element> <area or element>: anchor an element |  |
| `reset` | reset <element>: put an element back at the screen centre |  |
| `show` | show <element or all>: show elements |  |
| `togglehud` | togglehud <element or setting>: toggle an element or a 0/1 setting |  |

### 2.8 The config manager (4 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `cfg_load` | cfg_load <file>: reset to defaults, then run the config |  |
| `cfg_reset` | Reset settings, binds, aliases, triggers and teamplay state to defaults |  |
| `cfg_save` | cfg_save [file]: write the configuration (config.cfg when no name) |  |
| `troubleshoot` | Check settings for common causes of problems |  |

### 2.9 Menus and help (14 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `describe` | describe <name>: print the manual entry of a command or setting |  |
| `help` | Open the manual pages |  |
| `menu_demos` | Open the demo browser |  |
| `menu_help` | Open the help menu |  |
| `menu_load` | Open the load-game menu |  |
| `menu_main` | Open the main menu |  |
| `menu_multiplayer` | Open the multiplayer menu |  |
| `menu_options` | Open the options menu |  |
| `menu_quit` | Open the quit menu |  |
| `menu_save` | Open the save-game menu |  |
| `menu_singleplayer` | Open the single-player menu |  |
| `menu_slist` | Open the server browser |  |
| `togglemenu` | Open or close the menu |  |
| `toggleproxymenu` | Open or close the proxy menu |  |

### 2.10 Sound and voice (14 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `ignore_voip` | Mute a player's voice only | voice |
| `mutesound` | Toggle mute (keeps the volume to restore) |  |
| `play` | play <sound>: play a sound locally |  |
| `playvol` | playvol <sound> <volume>: play a sound at a volume |  |
| `s_audiodevicelist` | List playback and capture devices |  |
| `s_listdrivers` | List audio drivers |  |
| `s_restart` | Restart sound |  |
| `soundinfo` | Print the sound system's state |  |
| `soundlist` | List loaded sounds |  |
| `stopsound` | Stop all sounds |  |
| `stopsound_script` | Stop the sound on your own local script channel |  |
| `unignore_voip` | Unmute a player's voice | voice |
| `+voip` / `-voip` | While held, push to talk | voice |
| `-voip` | Stop talking | voice |

### 2.11 Video, screenshots and input devices (24 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `batteryinfo` | Print the battery state |  |
| `bf` | Flash the screen as on a pickup |  |
| `fontlist` | List TrueType fonts on the system | ttf |
| `fontload` | Load a TrueType font | ttf |
| `in_restart` | Restart input |  |
| `joyadvancedupdate` | Apply joystick axis settings |  |
| `loadcharset` | Load a charset picture as the font |  |
| `loadsky` | loadsky <name>: load a sky box |  |
| `screenshot` | screenshot [name]: save a screenshot |  |
| `sizedown` | Shrink the view |  |
| `sizeup` | Grow the view |  |
| `skywind` | Set sky box animation |  |
| `skywind_load` | Load the sky box animation settings |  |
| `skywind_lookdir` | Point the sky animation where you look |  |
| `skywind_rotate` | Turn the sky animation |  |
| `skywind_save` | Save the sky animation settings |  |
| `timerefresh` | Turn 360 degrees and print the frame rate; refused during a match under some rulesets |  |
| `v_cshift` | v_cshift <r> <g> <b> <percent>: tint the screen |  |
| `vid_displaylist` | List displays |  |
| `vid_gfxinfo` | Print graphics information |  |
| `vid_modelist` | List video modes |  |
| `vid_reload` | Reload textures and graphics |  |
| `vid_restart` | Restart video |  |
| `windows` | Minimise the game |  |

### 2.12 Server browser (9 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `addserver` | addserver <address>: add a server to the browser's own list |  |
| `sb_buildpingtree` | Build the proxy route tree for best-route connecting |  |
| `sb_pingsdump` | Print address and ping of listed servers |  |
| `sb_proxygetpings` | Ask a proxy for its pings to servers |  |
| `sb_refresh` | Refresh pings and status |  |
| `sb_sourceadd` | Add a server source (master or file) |  |
| `sb_sourcemark` | Mark a source as used |  |
| `sb_sourcesupdate` | Reload marked sources |  |
| `sb_sourceunmarkall` | Unmark all sources |  |

### 2.13 Files, integrity checks and developer tools (24 commands)

| Command | What it does | Marks |
| --- | --- | --- |
| `dev_dump_defaults` | Write every setting's default to a file (developer mode) | dev |
| `dev_gfxbenchmarklightmaps` | Developer: benchmark lightmap building | dev |
| `dev_gfxtexturedump` | Developer: dump textures (debug builds) | dev |
| `dev_gfxtexturelist` | Developer: list textures (debug builds) | dev |
| `dev_gfxtrace` | Developer: trace rendering calls (debug builds) | dev |
| `dev_help_issues` | Developer: list gaps in the manual | dev |
| `dev_help_verify_config` | Developer: check a config against the manual | dev |
| `dev_physicsnormalsave` | Developer: save a surface-normal test | dev |
| `dev_physicsnormalset` | Developer: set a surface-normal test | dev |
| `dev_physicsnormalshow` | Developer: show a surface-normal test | dev |
| `dev_pointfile` | Load a leak trace file and draw it | dev |
| `f_modified` | Print your answer to the modified-files check |  |
| `f_ruleset` | f_ruleset [check]: explain or ask for the ruleset fairness replies |  |
| `f_server` | Print the proxies in use |  |
| `fs_diff` | Compare two files |  |
| `fs_dir` | List files in the search path |  |
| `fs_loadpak` | Add a pack to the search path |  |
| `fs_locate` | Show where a file is found |  |
| `fs_path` | Print the search path |  |
| `fs_removepak` | Remove a pack from the search path |  |
| `fs_restart` | Rebuild the search path |  |
| `fs_search` | Search cached file names by suffix |  |
| `hunk_print` | Print the memory hunk layout |  |
| `loadfragfile` | Load frag message definitions |  |

### 2.14 The built-in server (69 commands)

Operator commands of the listen/local server compiled into the client; the same names and meanings as the dedicated server program's (server-inventory.md and servers.md cover them), so only the names: `acc_block`, `acc_create`, `acc_list`, `acc_remove`, `acc_unblock`, `addip`, `cancel`, `check_maps`, `cuff`, `devmap`, `edict`, `edictcount`, `edicts`, `floodprot`, `floodprotmsg`, `fraglogfile`, `gamedir`, `heartbeat`, `kick`, `listip`, `load`, `localinfo`, `logerrors`, `logfile`, `logplayers`, `logrcon`, `logtelnet`, `map`, `master_rcon_password`, `mod`, `modfraglogfile`, `mute`, `penaltylist`, `penaltyremove`, `profile`, `qtv_close`, `qtv_list`, `qtv_reverse_secret`, `qtv_status`, `removeip`, `save`, `setmaster`, `snap`, `snapall`, `status`, `sv_democancel`, `sv_demoeasyrecord`, `sv_demoembedinfo`, `sv_demoinfo`, `sv_demoinfoadd`, `sv_demoinforemove`, `sv_demolist`, `sv_demolistr`, `sv_demolistregex`, `sv_demonumremove`, `sv_demorecord`, `sv_demoremove`, `sv_demostop`, `sv_gamedir`, `sv_lastscores`, `sv_status`, `sv_usercmdtrace`, `svadmin`, `vip_addip`, `vip_listip`, `vip_removeip`, `vip_writeip`, `vminfo`, `writeip`. (R)

### 2.15 HUD element commands (83 + 46)

Each element's name is a command (§1.18): with no arguments it describes the element, with a setting name it prints or sets `hud_<element>_<setting>`. The 83 names (R): `ammo`, `ammo1`, `ammo2`, `ammo3`, `ammo4`, `armor`, `armordamage`, `bar_armor`, `bar_health`, `centerprint`, `clock`, `democlock`, `face`, `fps`, `frags`, `framestats`, `frametime`, `gameclock`, `gamesummary`, `group1`, `group2`, `group3`, `group4`, `group5`, `group6`, `group7`, `group8`, `group9`, `gun`, `gun2`, `gun3`, `gun4`, `gun5`, `gun6`, `gun7`, `gun8`, `health`, `healthdamage`, `iammo`, `iammo1`, `iammo2`, `iammo3`, `iammo4`, `iarmor`, `itemsclock`, `key1`, `key2`, `keys`, `net`, `netgraph`, `netproblem`, `notify`, `ownfrags`, `pent`, `ping`, `qtv_buffer`, `quad`, `radar`, `ring`, `score_bar`, `score_difference`, `score_enemy`, `score_position`, `score_team`, `scoreclock`, `scoremapname`, `sigil1`, `sigil2`, `sigil3`, `sigil4`, `speed`, `speed2`, `static_text`, `suit`, `teamfrags`, `teamholdbar`, `teamholdinfo`, `teaminfo`, `teamstackbar`, `tracker`, `tracking`, `vidlag`, `weaponstats`.

`+hud_<element>` shows and `-hud_<element>` hides these 23 elements, so a key can show one while held (R): `bar_armor`, `bar_health`, `centerprint`, `clock`, `democlock`, `fps`, `framestats`, `frametime`, `gameclock`, `gamesummary`, `net`, `netgraph`, `notify`, `ping`, `radar`, `score_bar`, `speed`, `speed2`, `teamholdbar`, `teamholdinfo`, `teamstackbar`, `tracking`, `vidlag`.

## 3. User info keys

The user info string is a set of `\key\value` pairs the client sends at connection and resends whenever one changes. Most keys are settings with the **info** mark: setting the setting changes the key. A key whose value is empty is left out of the string (D). Keys starting with `*` can be set only by the program, never by `setinfo`. The `setinfo` listing ends with a count of used space against the limit, printed as "[n/196]". (R)

| Key | Default | Written by | What it means |
| --- | --- | --- | --- |
| `name` | `player` | the `name` setting | Player name |
| `team` | (empty) | the `team` setting | Team name (not reset by a config reset) |
| `skin` | (empty) | the `skin` setting | Skin to draw this player with |
| `topcolor` | (empty) | the `topcolor` setting (also `color`) | Shirt colour 0–16 (not reset by a config reset) |
| `bottomcolor` | (empty) | the `bottomcolor` setting (also `color`) | Pants colour 0–16 (not reset by a config reset) |
| `spectator` | (empty) | the `spectator` setting; also `join` (clears it) and `observe` (sets it) | Non-empty: join as a spectator (not reset by a config reset) |
| `rate` | `25000` | the `rate` setting | Bytes a second the server may send |
| `msg` | `1` | the `msg` setting | Least important server message the player receives (0 all; higher values drop more) |
| `noaim` | `1` | the `noaim` setting | 1: refuse the server's automatic aim help |
| `w_switch` | (empty) | the `w_switch` setting | Best weapon the server may switch to on pickup |
| `b_switch` | (empty) | the `b_switch` setting | The same for backpacks |
| `gender` | (empty) | the `gender` setting | For the mod's messages; the menu offers `m`, `f`, `n` or empty |
| `railcolor` | (empty) | the `railcolor` setting | Colour of the player's rail trail on mods that have one |
| `mtu` | (empty) | the `mtu` setting | Largest packet the player wants from the server |
| `password` | (empty) | the `password` setting | Server password; cannot be set by the server and is never expanded by macros |
| `*client` | `ezQuake` and its revision | the program at start | Client name |
| `*ver` | `ezQuake v` with its version | the program at start | Client version |
| `*rsn` | the rule set's name | changing `ruleset` | The rule set in force |
| `prx` | the proxy chain | connecting through `cl_proxyaddr` | The proxies the connection passes through |
| `chat` | absent | the program, only while connected | Bit flags: 1 the player is out of the game (console, menu or chat input open); 2 the game window is minimised or not focused (away). Other players' clients draw an icon over the player from it. Removed when the flags are 0 |
| `qul` | `0`/`1` | the `scr_scoreboard_showqtvusers` setting, through `setinfo` | 1: the player wants the list of broadcast viewers |
| `pmodel`, `emodel` | the checksums of the player and eyes models | the program, before entering the map | Lets the server check the models are not altered; `setinfo` refuses both |
| `ec`, `exec_class`, `em`, `exec_map` | absent | the player with `setinfo` | Kept by the config like other keys (configs some mods run per class and map); a full reset removes them |

Only the connect request carries `*z_ext`, the client's protocol extension bits (without the visible-weapons bit when `cl_novweps` is 1). (R)

Other ways keys change (R): `setinfo key value` sets any key (except `pmodel`, `emodel` and starred ones) and sends it when connected; `setinfo` alone lists them; `fullinfo` replaces the whole string; `color` sets both colours. Save rules for these keys are in the legend at the top (`cfg_save_userinfo`).

## 4. The settings that change play first

The settings a player reaches for first, by what they change. Every row is also in §1 with its full values (R; the choice of rows is ours).

| Area | Setting | Default | What it does |
| --- | --- | --- | --- |
| Aim and look | `sensitivity` | `12` | Mouse sensitivity |
| Aim and look | `m_pitch` | `0.022` | Vertical mouse scale; negative inverts |
| Aim and look | `m_yaw` | `0.022` | Horizontal mouse scale |
| Aim and look | `freelook` | `1` | Mouse look always on |
| Aim and look | `in_raw` | `1` | Raw mouse input |
| Aim and look | `m_accel` | `0` | Mouse acceleration |
| Aim and look | `fov` | `90` | Field of view in degrees |
| Aim and look | `crosshair` | `4` | Crosshair shape (0 off) |
| Aim and look | `crosshairsize` | `1` | Crosshair scale |
| Aim and look | `crosshaircolor` | `255 0 0` | Crosshair colour (R G B) |
| Moving | `cl_forwardspeed` | `400` | Forward speed requested (the server caps it) |
| Moving | `cl_sidespeed` | `400` | Strafe speed requested |
| Moving | `cl_backspeed` | `400` | Backward speed requested |
| Moving | `cl_upspeed` | `400` | Swim up/down speed requested |
| Moving | `cl_movespeedkey` | `2.0` | Speed multiplier while +speed is held |
| Moving | `cl_smartjump` | `1` | Jump swims up in liquids |
| Moving | `cl_iDrive` | `0` | Pressing both directions keeps the later one |
| Moving | `allow_scripts` | `2` | How far movement scripts may run |
| Weapons | `w_switch` | (empty) | Highest weapon the server auto-switches to on pickup |
| Weapons | `b_switch` | (empty) | Same, for picking up a backpack |
| Weapons | `cl_weaponpreselect` | `0` | Weapon keys preselect instead of switching |
| Weapons | `cl_weaponhide` | `0` | Switch to the axe after firing |
| Weapons | `noaim` | `1` | Refuse the server's aim help |
| Seeing others | `r_fullbrightSkins` | `1` | Players drawn fully lit (capped by the server) |
| Seeing others | `enemyforceskins` | `0` | Force skins on enemies |
| Seeing others | `teamforceskins` | `0` | Force skins on teammates |
| Seeing others | `enemytopcolor` | `-1` | Forced enemy shirt colour (-1 off) |
| Seeing others | `teamtopcolor` | `-1` | Forced teammate shirt colour (-1 off) |
| Seeing others | `cl_deadbodyFilter` | `0` | Hide dead bodies |
| Seeing others | `cl_gibFilter` | `0` | Hide gibs |
| Seeing others | `cl_fakeshaft` | `1` | Straighten the lightning beam (announced) |
| View | `viewsize` | `100` | Screen size (100 normal, 120 no bars) |
| View | `cl_rollangle` | `0` | View roll when strafing |
| View | `v_kickpitch` | `0.0` | View kick when hit |
| View | `v_damagecshift` | `0.2` | Screen flash when hit |
| View | `r_drawviewmodel` | `1` | Held weapon opacity |
| View | `scr_newhud` | `0` | Classic bar, element HUD or both |
| Network and timing | `rate` | `25000` | Bytes a second the server may send |
| Network and timing | `cl_maxfps` | `0` | Frame cap while connected (0 none) |
| Network and timing | `cl_independentPhysics` | `1` | Draw frames apart from the physics/packet rate |
| Network and timing | `cl_physfps` | `0` | Physics/packet rate (0 the server's) |
| Network and timing | `cl_c2spps` | `0` | Packet-per-second cap |
| Network and timing | `cl_predict_players` | `1` | Predict other players |
| Network and timing | `cl_nopred` | `0` | Turn off own-movement prediction |
| Identity | `name` | `player` | Player name |
| Identity | `team` | (empty) | Team name |
| Identity | `topcolor` | (empty) | Shirt colour |
| Identity | `bottomcolor` | (empty) | Pants colour |
| Identity | `spectator` | (empty) | Join as a spectator |
| Identity | `ruleset` | `default` | Tournament rule set |
| Sound | `volume` | `0.7` | Master volume |

Facts behind those rows: with independent physics on (the default) the packet rate is `cl_physfps`, or the server's `maxfps` when that is 0; with it off, the frame cap is also the packet cap, 77 under the strict rule sets and 72 otherwise unless the server sets `maxfps` (R). The client sends the speeds as requested; the server's speed limit applies on its side (D). `w_switch` and `b_switch` are only sent as user info; the client never reads them, so the server's mod decides what they do (D). An empty colour is left out of the user info, so the server's own default applies (D).

## 5. Daily use: the options menu and the first run

The options menu (opened from the main menu; `menu_options`) has seven pages, in this order: Player, Graphics, View, Controls, Misc, System, Config. Each page is a list of rows under headings. Rows marked *adv* are shown only when the page's "Advanced Options" row is on (it sets `menu_advanced`, default 0). A setting row shows the setting's value and changes it in place; a slider row shows its range and step; a key row shows and rebinds the keys bound to a command; an action row runs something. Rows inside a build option say so. (R)

### Player

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Player Settings | Name | `name` (default `player`) |  |
| Player Settings | Teamchat Name | `cl_fakename` (default (empty)) |  |
| Player Settings | Teamchat Name Suffix | `cl_fakename_suffix` (default `: `) | adv |
| Player Settings | Gender | `gender` (default (empty)) |  |
| Player Settings | Team | `team` (default (empty)) |  |
| Player Settings | Skin | `skin` (default (empty)) | adv |
| Player Settings | Shirt Color | `topcolor` (default (empty)) |  |
| Player Settings | Pants Color | `bottomcolor` (default (empty)) |  |
| Player Settings | Fullbright Skins | `r_fullbrightSkins` (default `1`, slider 0–1 step 0.05) | adv |
| Player Settings | Ruleset | `ruleset` (default `default`) | adv |
| Weapon Handling | Gun Autoswitch | sets `w_switch` and `b_switch` together: on 8, off 1 (reads "on" when either is 0 or above 2) |  |
| Weapon Handling | Gun Preselect | `cl_weaponpreselect` (default `0`) |  |
| Weapon Handling | Gun Auto Hide | `cl_weaponhide` (default `0`) |  |
| Movement | Always Run | sets `cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed` together: on 400, off 200 (reads "on" above 200) |  |
| Movement | Smart Jump | `cl_smartjump` (default `1`) | adv |
| Movement | Movement Scripts | `allow_scripts` (default `2`) | adv |
| Team Skin & Colors | Shirt Color | `teamtopcolor` (default `-1`) |  |
| Team Skin & Colors | Pants Color | `teambottomcolor` (default `-1`) |  |
| Team Skin & Colors | Skin | `teamskin` (default (empty)) | adv |
| Team Skin & Colors | Force Skins | `teamforceskins` (default `0`) | adv |
| Team Skin & Colors | Quad Skin | `teamquadskin` (default (empty)) | adv |
| Team Skin & Colors | Pent Skin | `teampentskin` (default (empty)) | adv |
| Team Skin & Colors | Quad+Pent Skin | `teambothskin` (default (empty)) | adv |
| Enemy Skin & Colors | Shirt Color | `enemytopcolor` (default `-1`) |  |
| Enemy Skin & Colors | Pants Color | `enemybottomcolor` (default `-1`) |  |
| Enemy Skin & Colors | Skin | `enemyskin` (default (empty)) | adv |
| Enemy Skin & Colors | Force Skins | `enemyforceskins` (default `0`) | adv |
| Enemy Skin & Colors | Quad Skin | `enemyquadskin` (default (empty)) | adv |
| Enemy Skin & Colors | Pent Skin | `enemypentskin` (default (empty)) | adv |
| Enemy Skin & Colors | Quad+Pent Skin | `enemybothskin` (default (empty)) | adv |

### Graphics

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Presets | GFX Preset | cycles four presets (fastest, eyecandy, high eyecandy, faithful), each running a preset config file; the starting reading is guessed from `cl_deadbodyFilter` and `cl_muzzleflash` |  |
| Field Of View | Draw Distance | `r_farclip` (default `8192`, slider 4096–8192 step 4096) | adv |
| Field Of View | View Size (fov) | `fov` (default `90`, slider 40–140 step 2) |  |
| Field Of View | Screen Size | `viewsize` (default `100`, slider 30–120 step 5) |  |
| Field Of View | Rollangle | `cl_rollangle` (default `0`, slider 0–10 step 1) | adv |
| Field Of View | Rollspeed | `cl_rollspeed` (default `200`, slider 0–30 step 2) | adv |
| Field Of View | Gun Kick | `v_gunkick` (default `0`) | adv |
| Field Of View | Kick Pitch | `v_kickpitch` (default `0.0`, slider 0–10 step 0.5) | adv |
| Field Of View | Kick Roll | `v_kickroll` (default `0.0`, slider 0–10 step 0.5) | adv |
| Field Of View | Kick Time | `v_kicktime` (default `0.0`, slider 0–10 step 0.5) | adv |
| Field Of View | View Height | `v_viewheight` (default `0`, slider -7–4 step 0.5) | adv |
| Textures | Luma | `gl_lumatextures` (default `1`) | adv |
| Textures | Detail | `gl_max_size` (default `2048`) | adv |
| Textures | Miptex | `gl_miptexLevel` (default `0`, slider 0–3 step 1) | adv |
| Textures | No Textures | `gl_textureless` (default `0`) | adv |
| Player & Weapon Model | Powerup Luma | `gl_powerupshells` (default `1`) | adv |
| Player & Weapon Model | Weapon Opacity | `r_drawviewmodel` (default `1`, slider 0–1 step 0.05) | adv |
| Player & Weapon Model | Weapon Size | `r_viewmodelSize` (default `1`, slider 0.1–1 step 0.05) |  |
| Player & Weapon Model | Weapon Shift | `r_viewmodeloffset` (default (empty), slider -10–10 step 1) | adv |
| Player & Weapon Model | Weapon Muzzleflashes | `cl_muzzleflash` (default `1`) | adv |
| Player & Weapon Model | Outline | `gl_outline` (default `0`) | adv |
| Environment | Fullbright World | `r_fullbright` (default `0`) | adv |
| Environment | Simple Sky | `r_fastsky` (default `0`) | adv |
| Environment | Simple Walls | `r_drawflat` (default `0`) | adv |
| Environment | Simple Turbs | `r_fastturb` (default `0`) | adv |
| Environment | Simple Items | `gl_simpleitems` (default `0`) | adv |
| Environment | Simple Items Orientation | `gl_simpleitems_orientation` (default `2`) | adv |
| Environment | Draw Flame | `r_drawflame` (default `1`) | adv |
| Environment | Backpack Filter | `cl_backpackfilter` (default `0`) | adv |
| Environment | Gib Filter | `cl_gibFilter` (default `0`) |  |
| Environment | Dead Body Filter | `cl_deadbodyFilter` (default `0`) | adv |
| Projectiles | Explosion Type | `r_explosionType` (default `1`) |  |
| Projectiles | Rocket Model | `cl_r2g` (default `0`) | adv |
| Projectiles | Rocket Trail | `r_rocketTrail` (default `1`) |  |
| Projectiles | Rocket Light | `r_rocketLight` (default `1`, slider 0–1 step 0.05) | adv |
| Projectiles | Grenade Trail | `r_grenadeTrail` (default `1`) | adv |
| Projectiles | Fakeshaft | `cl_fakeshaft` (default `1`, slider 0–1 step 0.05) |  |
| Projectiles | Hide Nails | `cl_hidenails` (default `0`) | adv |
| Projectiles | Hide Rockets | `cl_hiderockets` (default `0`) | adv |
| Lighting | GL Bloom | `r_bloom` (default `0`) | classic renderer |
| Lighting | Powerup Glow | `r_powerupGlow` (default `1`) |  |
| Lighting | Damage Flash | `v_damagecshift` (default `0.2`, slider 0–1 step 0.1) |  |
| Lighting | Pickup Flash | `cl_bonusflash` (default `0`) | adv |
| Lighting | Colored Lights | `gl_colorlights` (default `1`) |  |
| Lighting | Fast Lights | `gl_flashblend` (default `0`) |  |
| Lighting | Dynamic Lights | `r_dynamic` (default `2`) |  |
| Lighting | Darken Map | `gl_lightmode` (default `1`) | adv |
| Lighting | Particle Shaft | `gl_lightning` (default `0`) | adv |

### View

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Head Up Display | HUD Type | `scr_newhud` (default `0`) |  |
| Head Up Display | Crosshair | `crosshair` (default `4`, slider 0–7 step 1) |  |
| Head Up Display | Crosshair Size | `crosshairsize` (default `1`, slider 0.2–3 step 0.2) |  |
| Head Up Display | Crosshair Alpha | `crosshairalpha` (default `1`, slider 0.1–1 step 0.1) | adv |
| Head Up Display | Overhead Name | `scr_autoid` (default `5`) | adv |
| New HUD | Gameclock | `hud_gameclock_show` (default `1`) |  |
| New HUD | Big Gameclock | `hud_gameclock_big` (default `1`) | adv |
| New HUD | Teaminfo Table | `scr_teaminfo` (default `1`) |  |
| New HUD | Own Frags Announcer | `hud_ownfrags_show` (default `1`) | adv |
| New HUD | Teamholdbar | `hud_teamholdbar_show` (default `0`) | adv |
| New HUD | Teamholdinfo | `hud_teamholdinfo_show` (default `0`) | adv |
| New HUD | Clock | `hud_clock_show` (default `0`) | adv |
| New HUD | FPS | `hud_fps_show` (default `1`) |  |
| New HUD | Radar | `hud_radar_show` (default `0`) | adv |
| Quake Classic HUD | Status Bar | `cl_sbar` (default `0`) |  |
| Quake Classic HUD | HUD Left | `cl_hudswap` (default `0`) |  |
| Quake Classic HUD | Show FPS | `show_fps` (default `0`) |  |
| Quake Classic HUD | Show Clock | `cl_clock` (default `0`) | adv |
| Quake Classic HUD | Show Gameclock | `cl_gameclock` (default `0`) |  |
| Tracker Messages | Messages | `r_tracker_messages` (default `20`, slider 0–10 step 1) |  |
| Tracker Messages | Flags | `r_tracker_flags` (default `0`) | adv |
| Tracker Messages | Frags | `r_tracker_frags` (default `1`) | adv |
| Tracker Messages | Streaks | `r_tracker_streaks` (default `0`) | adv |
| Tracker Messages | Time | `r_tracker_time` (default `4`, slider 0.5–6 step 0.5) | adv |
| Tracker Messages | Scale | `r_tracker_scale` (default `1`, slider 0.1–2 step 0.1) | adv |
| Tracker Messages | Use Images | `cl_useimagesinfraglog` (default `0`) | adv |
| Tracker Messages | Align Right | `r_tracker_align_right` (default `1`) | adv |
| Message Filtering | Ignore Opponents | `ignore_opponents` (default `0`) | adv |
| Message Filtering | Ignore Spectators | `ignore_spec` (default `0`) | adv |
| Message Filtering | Ignore Qizmo Observers | `ignore_qizmo_spec` (default `0`) | adv |
| Message Filtering | Ignore Flood | `ignore_flood` (default `0`) | adv |
| Message Filtering | Ignore Flood Duration | `ignore_flood_duration` (default `4`, slider 0–10 step 1) | adv |
| Message Filtering | Message Filtering | `msg_filter` (default `0`) | adv |
| Outgoing Filtering | Filter Colored Text | `cl_sayfilter_coloredtext` (default `0`) | adv |
| Outgoing Filtering | Send #u/#c Versions | `cl_sayfilter_sendboth` (default `0`) | adv |
| Console Options | Timestamps | `con_timestamps` (default `0`) | adv |
| Console Options | Chat Mode | `cl_chatmode` (default `2`) | adv |
| Console Options | Completion Format | `con_completion_format` (default `2`) | adv |
| Console Options | Size | `scr_consize` (default `0.5`, slider 0–1 step 0.1) | adv |
| Console Options | Speed | `scr_conspeed` (default `9999`, slider 1000–9999 step 1000) | adv |
| Console Options | Alpha | `scr_conalpha` (default `0.8`, slider 0–1 step 0.1) | adv |
| Console Options | Map Preview | `scr_conback` (default `1`) | adv |
| Console Options | Fun Chars Mode | `con_funchars_mode` (default `0`) | adv |
| Console Options | Colored Text | `scr_coloredText` (default `1`) | adv |
| Console Options | Notify Time | `con_notifytime` (default `3`, slider 0.5–16 step 0.5) | adv |
| Console Options | Notify Lines | `con_notifylines` (default `4`, slider 0–16 step 1) | adv |
| Console Options | Confirm Quit | `cl_confirmquit` (default `0`) | adv |
| Demo & Observing | Chasecam | `cl_chasecam` (default `1`) | adv |
| Demo Playback | Multiview | `cl_multiview` (default `0`, slider 0–4 step 1) |  |
| Demo Playback | Display HUD | `cl_mvdisplayhud` (default `1`) |  |
| Demo Playback | HUD Flip | `cl_mvhudflip` (default `0`) | adv |
| Demo Playback | HUD Vertical | `cl_mvhudvertical` (default `0`) | adv |
| Demo Playback | Inset View | `cl_mvinset` (default `0`) | adv |
| Demo Playback | Inset HUD | `cl_mvinsethud` (default `1`) | adv |
| Demo Playback | Inset Cross | `cl_mvinsetcrosshair` (default `1`) | adv |
| Multiview Demos | Toggle Autotrack | turns automatic tracking on or off |  |
| Multiview Demos | Autohud | `mvd_autohud` (default `0`) |  |
| Multiview Demos | Autotrack Type | `mvd_autotrack` (default `0`) |  |
| Multiview Demos | Autotrack Lock Team | `mvd_autotrack_lockteam` (default `0`) | adv |
| Multiview Demos | Moreinfo | `mvd_moreinfo` (default `0`) |  |
| Multiview Demos | Status | `mvd_status` (default `0`) | adv |

### Controls

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Mouse Settings | Freelook | `freelook` (default `1`) | adv |
| Mouse Settings | Sensitivity | `sensitivity` (default `12`, slider 1–20 step 0.25) |  |
| Mouse Settings | Menu Mouse Sensitivity | `scr_cursor_sensitivity` (default `1`, slider 0.10–3 step 0.10) | adv |
| Mouse Settings | Acceleration | `m_accel` (default `0`, slider 0–1 step 0.1) | adv |
| Mouse Settings | Invert Mouse | flips the sign of `m_pitch` |  |
| Mouse Settings | X-axis Sensitivity | `m_yaw` (default `0.022`) | adv |
| Mouse Settings | Y-axis Sensitivity | `m_pitch` (default `0.022`) | adv |
| Mouse Settings | Raw Mouse Input | `in_raw` (default `1`) | adv |
| Movement | Attack | key for `+attack` |  |
| Movement | Jump/Swim up | key for `+jump` |  |
| Movement | Move Forward | key for `+forward` |  |
| Movement | Move Backward | key for `+back` |  |
| Movement | Strafe Left | key for `+moveleft` |  |
| Movement | Strafe Right | key for `+moveright` |  |
| Movement | Turn Left | key for `+left` |  |
| Movement | Turn Right | key for `+right` |  |
| Movement | Swim Up | key for `+moveup` | adv |
| Movement | Swim Down | key for `+movedown` | adv |
| Movement | Zoom In/Out | key for `+zoom` | adv |
| Weapons | Previous Weapon | key for `weapon 12` |  |
| Weapons | Next Weapon | key for `weapon 10` |  |
| Weapons | Axe | key for `weapon 1` |  |
| Weapons | Shotgun | key for `weapon 2` |  |
| Weapons | Super Shotgun | key for `weapon 3` |  |
| Weapons | Nailgun | key for `weapon 4` |  |
| Weapons | Super Nailgun | key for `weapon 5` |  |
| Weapons | Grenade Launcher | key for `weapon 6` |  |
| Weapons | Rocket Launcher | key for `weapon 7` |  |
| Weapons | Thunderbolt | key for `weapon 8` |  |
| Chat settings | Chat | key for `messagemode` |  |
| Chat settings | Teamchat | key for `messagemode2` |  |
| Miscellaneous | Show Scores | key for `+showscores` |  |
| Miscellaneous | Screenshot | key for `screenshot` |  |
| Miscellaneous | Pause | key for `pause` | adv |
| Miscellaneous | Quit | key for `quit` | adv |
| Miscellaneous | Proxy Menu | key for `toggleproxymenu` | adv |
| Demo & Spec | Demo Controls | key for `demo_controls` |  |
| Demo & Spec | Autotrack | key for `autotrack` | adv |
| Demo & Spec | Play | key for `cl_demospeed 1;echo Playing demo.` | adv |
| Demo & Spec | Stop | key for `disconnect` | adv |
| Demo & Spec | Pause | key for `cl_demospeed 0;echo Demo paused.` | adv |
| Demo & Spec | Fast Forward | key for `cl_demospeed 5;echo Demo paused.` | adv |
| Teamplay | Report Status | key for `tp_msgreport` |  |
| Teamplay | Lost Location | key for `tp_msglost` |  |
| Teamplay | Location Safe | key for `tp_msgsafe` |  |
| Teamplay | Point At Item | key for `tp_msgpoint` |  |
| Teamplay | Took Item | key for `tp_msgtook` |  |
| Teamplay | Need Items | key for `tp_msgneed` |  |
| Teamplay | Yes/Ok | key for `tp_msgyesok` | adv |
| Teamplay | Coming From Location | key for `tp_msgcoming` | adv |
| Teamplay | Help Location | key for `tp_msghelp` |  |
| Teamplay | Get Quad | key for `tp_msggetquad` | adv |
| Teamplay | Get Pent | key for `tp_msggetpent` | adv |
| Teamplay | Enemy Quad Dead | key for `tp_msgquaddead` | adv |
| Teamplay | Enemy Has Powerup | key for `tp_msgenemypwr` | adv |
| Teamplay | Trick At Location | key for `tp_msgtrick` | adv |
| Teamplay | Replace At Location | key for `tp_msgreplace` | adv |
| Teamplay | You Take Item | key for `tp_msgutake` | adv |
| Teamplay | Waiting | key for `tp_msgwaiting` | adv |
| Teamplay | Enemy Slipped | key for `tp_msgslipped` | adv |

### Misc

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Match Tools | Auto Screenshot | `match_auto_sshot` (default `0`) |  |
| Match Tools | Screenshot Format | `sshot_format` (default `png`) |  |
| Match Tools | Auto Record Demo | `match_auto_record` (default `0`) |  |
| Match Tools | Demo Format | `demo_format` (default `qwz`) | Windows only |
| Match Tools | Auto Log Match | `match_auto_logconsole` (default `1`) |  |
| Match Tools | Log Readable | `log_readable` (default `1`) |  |
| Paths | Media Paths Type | `cl_mediaroot` (default `0`) |  |
| Paths | Screenshots Path | `sshot_dir` (default (empty)) |  |
| Paths | Demos Path | `demo_dir` (default (empty)) |  |
| Paths | Logs Path | `log_dir` (default (empty)) |  |
| Paths | Qizmo Path | `qizmo_dir` (default `qizmo`) |  |
| Paths | QWDTools Path | `qwdtools_dir` (default `qwdtools`) |  |
| Miscellaneous | Linear Interpolation | `cl_nolerp` (default `0`) | adv |
| Miscellaneous | In-Game Menu | `menu_ingame` (default `1`) | adv |

### System

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
| Video | Gamma | `gl_gamma` (default `1.0`, slider 0.3–3.0 step 0.1) |  |
| Video | Gamma control | `vid_software_palette` (default `1`) | adv |
| Video | Contrast | `gl_contrast` (default `1.0`, slider 1–5 step 0.1) |  |
| Video | Lightmap Intensity | `gl_modulate` (default `1`, slider 0.5–3.0 step 0.1) | adv |
| Video | Clear Video Buffer | `gl_clear` (default `0`) | adv |
| Video | Anisotropy Filter | `gl_anisotropy` (default `16`, slider 0–16 step 1) | adv |
| Video | Quality Mode | `gl_texturemode` (default `GL_LINEAR_MIPMAP_LINEAR`) | adv |
| Screen Settings | Use desktop resolution | `vid_usedesktopres` (default `1`) |  |
| Screen Settings | Resolution | picks a mode, applied by Apply Changes |  |
| Screen Settings | Vertical Sync | `vid_vsync` (default `0`) |  |
| Screen Settings | Vsync Lag Fix | `vid_vsync_lag_fix` (default `0`) | adv |
| Screen Settings | Bit Depth | 16 or 32 bits, applied by Apply Changes |  |
| Screen Settings | Fullscreen | full screen or window, applied by Apply Changes |  |
| Screen Settings | Apply Changes | restarts the renderer with the choices above |  |
| Renderer | Mode | `vid_renderer` (default `1`) | both renderers |
| Renderer | Apply Changes | restarts the renderer with the choices above | both renderers |
| Framebuffer | Mode | `vid_framebuffer` (default `2`) | adv |
| Framebuffer | HDR | `vid_framebuffer_hdr` (default `0`) | adv |
| Framebuffer | HDR Tonemap | `vid_framebuffer_hdr_tonemap` (default `0`) | adv |
| Framebuffer | Scale | `vid_framebuffer_scale` (default `1`, slider 0.25–2.0 step 0.25) | adv |
| Framebuffer | Multisample | `vid_framebuffer_multisample` (default `0`, slider 0–16 step 1) | adv |
| Framebuffer | FXAA | `vid_framebuffer_fxaa` (default `0`, slider 0–17 step 1) | adv |
| Framebuffer | Apply Changes | restarts the renderer with the choices above | adv |
| Font | Width | `vid_conwidth` (default `0`, slider 320–2048 step 8) | adv |
| Font | Height | `vid_conheight` (default `0`, slider 240–1538 step 4) | adv |
| Sound & Volume | Primary Volume | `volume` (default `0.7`, slider 0–1 step 0.05) |  |
| Sound & Volume | Mute/Unmute | key for `mutesound` |  |
| Sound & Volume | Self Volume Levels | `s_chat_custom` (default `1`) | adv |
| Sound & Volume | Chat Volume | `s_mm1_volume` (default `1`, slider 0–1 step 0.1) | adv |
| Sound & Volume | Team Chat Volume | `s_mm2_volume` (default `1`, slider 0–1 step 0.1) | adv |
| Sound & Volume | Spectator Volume | `s_spec_volume` (default `1`, slider 0–1 step 0.1) | adv |
| Sound & Volume | Other Volume | `s_otherchat_volume` (default `1`, slider 0–1 step 0.1) | adv |
| Sound & Volume | Static Sounds | `cl_staticSounds` (default `1`) | adv |
| Sound & Volume | Sounds When Minimized | `sys_inactivesound` (default `0`) | adv |
| Sound & Volume | Quality | `s_khz` (default `11`) |  |
| Sound & Volume | Buffer size | `s_desiredsamples` (default `0`) |  |
| Connection | Bandwidth Limit | `rate` (default `25000`) |  |
| Connection | Early Packets | `cl_earlypackets` (default `1`) | adv |
| Connection | Packetloss | `cl_c2sImpulseBackup` (default `3`) | adv |
| Connection | QTV Buffer Adjusting | `qtv_adjustbuffer` (default `1`) | adv |
| Miscellaneous | FPS Limit | cycles `cl_maxfps` through 0 ("no limit"), 72 and 77; any other value reads "custom" | adv |
| Miscellaneous | Process Priority | `sys_highpriority` (default (empty)) | adv, Windows only |
| Miscellaneous | Taskbar Flash | `vid_flashonactivity` (default `1`) | adv, Windows only |
| Miscellaneous | Taskbar Name | `cl_window_caption` (default `1`) | adv, Windows only |

### Config

| Heading | Row | What it changes | |
| --- | --- | --- | --- |
|  | Go To Console | opens the console |  |
| Load & Save | Reload Settings | loads the config again |  |
| Load & Save | Save Settings | saves the config |  |
| Load & Save | Save To Profile Dir | `cfg_use_home` (default `0`) | adv |
| Load & Save | Save To Mod Directory | `cfg_use_gamedir` (default `0`) | adv |
| Load & Save | Reset To Defaults | resets every setting and bind |  |
| Export & Import | Load Script | opens a list of scripts to run |  |
| Export & Import | Import config ... | loads another config |  |
| Export & Import | Export config ... | saves to another name |  |
| Config Saving Options | Reset Saving Options | resets the `cfg_` saving settings |  |
| Config Saving Options | Auto Save On Quit | `cfg_save_onquit` (default `0`) |  |
| Config Saving Options | Save Unchanged Opt. | `cfg_save_unchanged` (default `0`) |  |
| Config Saving Options | Backup Old File | `cfg_backup` (default `0`) | adv |
| Config Saving Options | Load Legacy | `cfg_legacy_exec` (default `1`) | adv |
| Config Saving Options | Aliases | `cfg_save_aliases` (default `1`) | adv |
| Config Saving Options | Binds | `cfg_save_binds` (default `1`) | adv |
| Config Saving Options | Cmdline | `cfg_save_cmdline` (default `1`) | adv |
| Config Saving Options | Init Commands | `cfg_save_cmds` (default `1`) | adv |
| Config Saving Options | Variables | `cfg_save_cvars` (default `1`) | adv |
| Config Saving Options | Userinfo | `cfg_save_userinfo` (default `2`) | adv |

### What the first run does

Start-up order (R):
1. Every setting takes its compiled default (the tables in §1), and the default keys are bound (below).
2. `default.cfg` runs when one is found, except the original game's own copy inside its first pack and the copy inside the popular installer's package (that one is run through `autoexec.cfg` instead).
3. The config is loaded: `config.cfg`, or the name after `+cfg_load` on the command line. It is looked for first in the player's home folder (with `cfg_use_home`, or always during start-up), in the mod's subfolder when `cfg_use_gamedir` is 1, then in the client's own `configs` folder under the game folder. On a first run there is none, so the console prints that it could not load `config.cfg` and the defaults stand. A config that is found first resets settings, aliases and binds, runs `default.cfg` and `autoexec.cfg` again when `cfg_legacy_exec` is 1 (the default), then runs the config with unknown-command warnings off.
4. `+set` lines of the command line run; then the subsystems start, and every setting's change handler runs once with its current value.
5. `autoexec.cfg` runs when present, then the rest of the command line; unknown-command warnings are switched on.

Default key bindings on a first run or after a reset (R):

| Key | Command | Key | Command |
| --- | --- | --- | --- |
| Escape | `togglemenu` | The backquote key (on macOS also the tilde) | `toggleconsole` |
| Pause | `toggleproxymenu` | Mouse 1, Ctrl | `+attack` |
| Mouse 2, Space | `+jump` | W, S, A, D | `+forward`, `+back`, `+moveleft`, `+moveright` |
| E, 0 | `impulse 10` (next weapon) | Q | `impulse 12` (previous weapon) |
| 1 to 9 | `impulse 1` to `impulse 9` | T, Y | `messagemode`, `messagemode2` |
| Alt | `+zoom` | Tab | `+showscores` |
| R | `tp_msgreport` | Z | `tp_msgtook` |
| X | `tp_msgsafe` | C | `tp_msghelp` |
| V | `tp_msgpoint` | F | `shownick` |
| F1 | `yes` | F2 | `agree` |
| F3 | `ready` | F4 | `break` |
| F5 | `join` | F6 | `observe` |
| F10 | `quit` | F12 | `screenshot` |

`yes`, `agree`, `ready` and `break` are not client commands; they are left for the server to define as aliases it sends (D: the client source does not settle this). A full reset (the menu's Reset To Defaults) also clears forced team and enemy colours, the message filter, the map and sky groups, and returns the teamplay pickup, took and point lists to their defaults. (R)

## 6. Commands that print

All facts in this section are **R** unless marked.

| Asked for | What the client has | What it prints |
| --- | --- | --- |
| `cvarlist [pattern]` | yes; `cvarlist_re` takes a regular expression | A title line "List of cvars:", then one line per setting: two flag columns (one for a user info setting, one for a server info setting) and the name; then a rule of dashes and a count "matched/total" with "matching" when a pattern was given and the word "variables" |
| `cmdlist [pattern]` | yes; `cmdlist_re` | "List of commands:", one name per line, a rule of dashes, then "matched/total commands" |
| `aliaslist` | yes | The aliases with their text |
| `find` / `apropos` | no `apropos`. `find [name]` is a different thing: it searches the players on every server known to the broadcast service | Each player's name (coloured, padded to 15) then " - " and the server's host:port; "Listing all players" without a name; "Found no players matching: ..." on no match; "Player list is being updated, please try again soon" while the list loads |
| a setting's name alone | yes | With `cvar_viewhelp` on, its manual entry first; then "name : default value is "x"" and, aligned under it, "current value is "y"", plus a line for an automatic value and one for a pending (latched) value when there is one. With `cvar_viewdefault` 0, only the short form ""name" is "value"" |
| `describe <name>` | yes | "name is a command" or "name is a variable", then the manual entry (description, values); with no argument, how to use it |
| `help` | yes | Opens the manual in the menu; prints nothing of its own |
| demo status | no single command. During a demo the HUD's demo clock and the `mvd_status` setting show it; `demo_jump_status` is a seek, not a report | |
| `ping` | no client command | The scoreboard shows pings; the client itself sends "pings" to the server every few seconds to refresh them (an unknown typed `ping` prints "Unknown command") |
| `pings` | no client command (see above) | |
| `serverinfo` | yes | The client's copy of the server info, one key per line: the key padded to 20 columns, then the value. When hosting, or not connected, the built-in server's own copy instead. A key without a value shows "MISSING VALUE"; an over-long key is cut and ends "..." |
| `user <name or id>` | yes, printed locally | That player's user info, in the same two-column form |
| `users` | yes, printed locally | Header "userid frags name" over "------ ----- ----", one line per player (id 6 wide, frags 4 wide, name), then "N total users" |
| `setinfo` alone | yes | The player's own user info in the two-column form, then "[used/196]" |
| `version` | yes | "ezQuake" with the version, then the build date and time; "debug build" for a debug build |
| fps reports | `timedemo`, `timedemo2`, `timerefresh`; and the `show_fps` setting and `fps` HUD element draw it on screen | `timedemo` prints "N frames S seconds F fps"; `timedemo2` adds that the rate was simulated at a given fps. Both then print the worst frame time with its frame, the average frame time and its standard deviation, frames counted against the demo's frames, wall time against the demo's time, the time spent outside rendering, and the worst frame time again as an fps with the frame count. `timerefresh` spins the view 128 frames and prints "S seconds (F fps)"; under the strict rule sets during a match it prints "Timerefresh is disabled during match" |

## 7. The stop-sound message (number 16)

**What the server does.** MVDSV defines message 16 (stop sound) in its protocol table but never sends it: no builtin, mod event or server event writes it (R). A mod that wants a sound cut on a channel starts another sound on the same entity and channel instead. The sound builtin turns the mod's 0–1 volume into 0–255 and refuses a volume outside 0–255, an attenuation outside 0–4 or a channel outside 0–15, with an error. Bit 8 of the channel (channels 8–15) means "send reliably to everyone" instead of by what can be heard from the sound's place, and the channel is then reduced to 0–7. A sound that was not precached prints "not precached" and is not sent. A volume of 255 is the default and is left out of the message; 0 is sent like any other value. (R)

**What the client does with message 16.** It reads one 16-bit number: the entity is that number divided by 8, the channel its remainder (0–7). While seeking in a demo the message is skipped. Otherwise it stops the first playing sound that matches both the entity and the channel; one sound only. (R) Its search covers the first 32 sound slots; the moving sounds live in slots 4 to 35 after four ambient slots, so a sound in the last four moving slots cannot be stopped this way (D).

**How a sound replaces another (what a server can rely on instead).** Starting a sound on an entity with a channel other than 0 replaces whatever that entity plays on the same channel; channel 0 never replaces anything (R). A sound started with volume 0 on the same entity and non-zero channel clears the old sound's slot and then plays nothing, so it works as a stop (D). Sounds of monsters never take over the local player's channels (R). The player's own commands: `stopsound` silences everything; `stopsound_script` stops channel 0 of the client's own local sound entity (the one `play` uses) (R).

## 8. Server commands a player types: forwarded or handled

Nothing unknown is forwarded: a word that is no command, setting, alias or old name prints "Unknown command" (with `cl_warncmd` 1 or `developer`) and goes nowhere. (R)

| What the player types | What the client does |
| --- | --- |
| `cmd <text>` | Sends the text to the server as typed, with three exceptions handled locally: `cmd snap` uploads a screenshot the server asked for; `cmd fileul` starts a file upload; `cmd techlogin`, when `cl_crypt_rcon` is 1, is sent with a time-stamped hash instead of the password |
| `kill`, `god`, `give`, `noclip`, `fly` | Forwarded unchanged (the word in lower case); they have no client action |
| `say`, `say_team` | Sent after local work: refused when it is a proxy-stuffed line; flood protection (`cl_floodprot`, at most `cl_fp_messages` lines per `cl_fp_persecond` seconds) prints a refusal; macros and coloured characters are expanded; the text is put in quotes; for `say_team`, `cl_fakename` is put in front |
| `setinfo key value` | Set locally, then sent; `pmodel` and `emodel` refused |
| `pause` | During a demo: pauses playback locally. Connected: forwarded |
| `join [address]`, `observe [address]` | With an address: connect there (through the proxy's connect when on one). Without: when the server announces that it understands them, send `cmd join` / `cmd observe`; otherwise reconnect. Both also set the `spectator` setting |
| `serverinfo`, `user`, `users` | Printed locally from the client's copy (§6); never sent |
| `rcon <command>` | Sent as a remote-console packet with `rcon_password` (or a time-stamped hash with `cl_crypt_rcon`) to the server or `rcon_address` |
| `ping`, `pings`, `info`, `ready`, `break`, `ptrack` | Not client commands. Typed bare they print "Unknown command"; the server's own aliases (sent at connection) usually define `ready`, `break` and the like as `cmd ...` (D). The client itself sends `pings` on a timer and `ptrack` when tracking a player (R) |
| `impulse`, movement buttons | Packed into the movement packets, not sent as text |

**What the server may run on the client.** Text the server pushes for execution may only run the commands named in `cl_remote_capabilities`. Its default list: `+attack`, `-attack`, `alias`, `bf`, `changing`, `cmd`, `color`, `download`, `exec`, `fullserverinfo`, `impulse`, `infoset`, `ktx_infoset`, `ktx_sinfoset`, `nextul`, `on_admin`, `on_connect`, `on_connect_ctf`, `on_connect_ffa`, `on_enter`, `on_enter_ctf`, `on_enter_ffa`, `on_matchend`, `on_matchstart`, `on_observe`, `on_observe_ctf`, `on_observe_ffa`, `on_spec_enter`, `on_spec_enter_ctf`, `on_spec_enter_ffa`, `on_spec_matchend`, `on_spec_matchstart`, `on_unadmin`, `packet`, `play`, `rate`, `reconnect`, `say`, `sinfoset`, `skin`, `skins`, `team`, `tempalias`, `track`, `wait`. Message triggers and teamplay macros have their own fixed lists. (R)

## Counts

| Settings | Count |
| --- | --- |
| Input and mouse | 41 |
| Movement, weapons and the player's own buttons | 20 |
| View: field of view, bob, roll, kick, tints | 45 |
| Crosshair | 9 |
| HUD: classic status bar, element HUD globals, tracker, team info, autoid | 160 |
| Scoreboard | 27 |
| Console and chat | 54 |
| Network and prediction | 66 |
| Demos, recording, broadcasts and match tools | 158 |
| Sound and voice | 41 |
| Picture: renderer, textures, video mode, fonts | 193 |
| Particles and effects | 106 |
| Skins, models and colours | 34 |
| Teamplay | 88 |
| Server browser | 39 |
| Screenshots | 5 |
| Everything else (player info, configs, menus, system) | 69 |
| Client settings, all areas | 1155 |
| Element HUD settings (83 elements) | 1405 |
| Built-in server settings | 168 |
| **All settings** | **2728** |

| Commands | Count |
| --- | --- |
| Movement, weapons and buttons | 51 |
| Connecting, chat and the server | 43 |
| Settings, aliases, binds and scripting | 46 |
| Console, chat input and ignore lists | 18 |
| Teamplay, locations and triggers | 42 |
| Demos, broadcasts, spectating and match tools | 56 |
| HUD and the HUD editor | 26 |
| The config manager | 4 |
| Menus and help | 14 |
| Sound and voice | 14 |
| Video, screenshots and input devices | 24 |
| Server browser | 9 |
| Files, integrity checks and developer tools | 24 |
| built-in server | 69 |
| Client commands, all areas | 371 |
| HUD element commands | 83 |
| `+hud_`/`-hud_` commands | 46 |
| **All commands** | **569** |

Plus 41 old names (§1.21).
