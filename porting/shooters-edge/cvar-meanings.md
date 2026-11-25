# Settings that share a name across the three classic games

Which of the console settings Shooter's Edge names exist in the first game (the 1996 release
and its multiplayer client and server), Quake II and the third game, and in the modern
multiplayer client of the first game; with each one's default, values, flags and effect, whether
a shared name means the same thing in each game, the same concept under different spellings, the
shadow settings, and the modern client's defaults against the original's. So each game binds its
own meaning and keeps its own classic default in our one console. From a clean-room reader
(README.md, the rules), 2026-11-18.

Sources read, each at its default branch head: id Software's Quake source release (GPL-2.0: the
1996 single-player engine in its software and GL builds, and the QuakeWorld client and server with
its game code); id Software's Quake II source release (GPL-2.0; its readme calls it 3.19); id
Software's Quake III Arena source release (GPL-2.0); ezQuake (GPL-2.0, head of 2026-11-17),
including the variable reference it ships with.

Every fact is marked **[read]** (stated by the source or by ezQuake's own shipped variable
reference), **[derived]** (worked out by combining facts read) or **[measured]** (nothing here was
run). Nothing here is copied from the source.

**Abbreviations.** **Q1** = the 1996 single-player engine (software build "SW", GL build "GL").
**QW** = the first game's multiplayer release (client "QWc", server "QWs"). **Q2** = Quake II.
**Q3** = the third game. **EZ** = the modern multiplayer client of the first game (it also contains
a built-in server; its server rows are marked "EZ server").

**Flag words used below.**
- **saved**: written to the player's config when the game writes it (Q1/QW: the "archive" mark; Q2/Q3: the archive flag). Q1 and QW write saved settings to their config on quit [read].
- **EZ saving** (applies to every EZ row, so not repeated): EZ has no per-setting archive mark; its config writer saves every setting whose value differs from its default (or every setting
  when `cfg_save_unchanged 1`), skipping read-only, start-up-only and player-created ones; the
  player-info settings are saved per `cfg_save_userinfo` (default 2, all of them) [read].
- **notify** (Q1 only): when changed, every connected player is told the new value [read].
- **serverinfo** / **userinfo**: the value is published in the server's info string (visible to
  server browsers, to clients and to game code) or in the player's info string sent to the server
  [read].
- **latched** (Q2, Q3): a change is held and takes effect at the next map start (or renderer
  restart for renderer settings) [read].
- **cheat** (Q3): can be changed only when the server allows cheats [read]. **forced-0 in MP**:
  the client resets it to its default every frame/connect unless single-player (QW, Q2) [read].
- **read-only**: the player cannot set it [read].
- **Case**: Q1, QW and Q2 look setting names up case-sensitively; Q3 and EZ case-insensitively
  [read]. So Q3's `sv_maxPing` is our `sv_maxping`, EZ's `r_fullbrightSkins` is our
  `r_fullbrightskins` [derived].

---

## Each of our setting names, game by game

Our setting names (Shooter's Edge's console) that are a setting in none of the four sources are not listed. Where a
name exists in a game only as a console COMMAND (not a setting), the row says so.

### View, bob, roll, kick, tint

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `cl_bob` | Q1, QWc | 0.02 | any real | none | Vertical view (and held-weapon) bob amplitude per unit of horizontal speed; the bob is the speed times this, mixed 30 % steady and 70 % sine, clamped to between 7 units down and 4 up [read] |
| `cl_bob` | EZ | 0 | any real | EZ saving | Same rule, but only on ground (keeps the last value in the air), never for spectators, uses the predicted velocity and its own clock [read] |
| `cl_bobcycle` | Q1, QWc | 0.6 | seconds | none | Length of one bob cycle in seconds [read] |
| `cl_bobcycle` | EZ | 0 | seconds; 0 or less turns bob off | EZ saving | Same; zero or negative disables the bob [read] |
| `cl_bobup` | Q1, QWc | 0.5 | fraction 0 to 1 | none | Fraction of the cycle spent on the rising half [read] |
| `cl_bobup` | EZ | 0 | fraction | EZ saving | Same [read] |
| `cl_rollangle` | Q1 | 2.0 | degrees | none | Strafe roll: the view rolls by up to this many degrees, linearly with sideways speed up to `cl_rollspeed`; the same value times 4 rolls the player's model as others see it [read] |
| `cl_rollangle` | QWc | 2.0 | degrees | none | Own view's strafe roll only [read] |
| `cl_rollangle` | QWs | 2.0 | degrees | none | Server-side copy: rolls the player's model as others see it, by 4 times the computed roll [read] |
| `cl_rollangle` | EZ | 0 | degrees; result capped at 45; while playing under a ruleset that restricts it, clamped 0 to 5 | EZ saving | Own view's strafe roll [read] |
| `cl_rollspeed` | Q1, QWc, QWs, EZ | 200 | units per second | none / EZ saving | Sideways speed at which the full `cl_rollangle` is reached [read] |
| `v_kickpitch` | Q1, QWc | 0.6 | multiplier | none | View pitch kick on taking damage: damage count times the hit direction's forward component times this [read] |
| `v_kickpitch` | EZ | 0 | multiplier | EZ saving | Same rule [read] |
| `v_kickroll` | Q1, QWc | 0.6 | multiplier | none | View roll kick on taking damage, by the hit direction's sideways component [read] |
| `v_kickroll` | EZ | 0 | multiplier | EZ saving | Same [read] |
| `v_kicktime` | Q1, QWc | 0.5 | seconds | none | Duration of the damage kick; the kick fades linearly over it [read] |
| `v_kicktime` | EZ | 0 | seconds; must be above 0 for any kick | EZ saving | Same [read] |
| `v_idlescale` | Q1, QWc | 0 | multiplier; 0 is off | none | Idle sway: scales three sine sways of the view (and opposite on the weapon) in roll, pitch and yaw [read] |
| `v_idlescale` | EZ | 0 | multiplier | EZ saving | Same; in Team Fortress games a player's own change while connected is refused (only the server may set it) [read] |
| `crosshair` | Q1 | 0 | 0 off, nonzero on | saved | Draws the `+` character at the view's centre [read] |
| `crosshair` | QWc | 0 | 0 off, 2 a dot-pattern crosshair in `crosshaircolor`, any other nonzero the `+` character | saved | Crosshair style [read] |
| `crosshair` | Q2 | 0 | 0 off, 1 to 3 the picture `ch1` to `ch3`; out of range becomes 3 | saved | Crosshair picture number [read] |
| `crosshair` | EZ | 4 | 0 off, 1 the `+` character, 2 the classic dot crosshair, 3 small cross, 4 dot, 5 to 7 further shapes | EZ saving | Crosshair style [read] |
| `crosshaircolor` | QWc | 79 | palette index 0 to 255 | saved | Colour of style 2 [read] |
| `crosshaircolor` | EZ | "255 0 0" | an RGB triple | EZ saving | Colour of the crosshair [read] |
| `crosshairsize` | EZ | 1 | scale, at most 20 | EZ saving | Scales the crosshair [read] |
| `crosshairalpha` | EZ | 1 | 0 to 1 | EZ saving | Crosshair opacity [read] |
| `fov` | Q1, QWc | 90 | 10 to 170 (set back into range) | none | Horizontal field of view in degrees across the view rectangle [read] |
| `fov` | Q2 | 90 | 1 to 160 at the server (below 1 becomes 90); 90 forced when the server's dmflags fixed-FOV bit is set in deathmatch | saved, userinfo | Horizontal field of view; the server reads it from the player's info and sends it back as the view's FOV; above 90 the held weapon is not drawn [read] |
| `fov` | EZ | 90 | 10 to 140 | EZ saving | Horizontal FOV as it would be on a 4:3 screen; on a wider screen the vertical FOV is kept and the horizontal grows; a server-sent value is not stored as the player's own [read] |
| `viewsize` | Q1, QWc | 100 | 30 to 120 | saved | Size of the view; 100 full view with the full status bar, 110 bar without inventory, 120 no bar [read] |
| `viewsize` | Q2 | 100 | 40 to 100 | saved | Size of the view as a percentage of the screen [read] |
| `viewsize` | EZ | 100 | 30 to 120 | EZ saving | Same as Q1 [read] |
| `gamma` | Q1, QWc | 1 | exponent; 1 is identity | saved | Brightness curve: output = 255 times ((input + 0.5) / 255.5) to the power `gamma`, so ABOVE 1 IS DARKER. SW applies it to the palette. Q1 GL and QW GL build the table but the 1996 GL build never sends it to the screen (the GL build's gamma is fixed at start-up by a command-line value, 0.7 by default on non-3dfx cards) [read] |
| `gl_cshiftpercent` | Q1 GL, QW GL | 100 | percent | none | Scales every screen tint (contents, damage, bonus, powerups); 0 turns them all off [read] |
| `gl_cshiftpercent` | EZ | 100 | percent | EZ saving | Same; needs `gl_polyblend 1` [read] |
| `gl_polyblend` | Q1 GL, QW GL, Q2 GL | 1 | 0 or 1 | none | Draws the full-screen tint quad (damage, powerups, underwater) [read] |
| `gl_polyblend` | EZ | 1 | 0 or 1 | EZ saving | Same; 0 also suppresses all tints except the contents tint [read] |
| `v_contentblend` | QWc | 1 | 0 off, nonzero on | none | Tint while inside water, slime or lava [read] |
| `v_contentblend` | EZ | 0.2 | 0 to 1 | EZ saving | Strength of the contents tint as a fraction; 0 off [read] |
| `v_damagecshift` | EZ | 0.2 | 0 to 1 | EZ saving | Strength of the red damage tint [read] |
| `v_quadcshift`, `v_suitcshift`, `v_ringcshift`, `v_pentcshift` | EZ | 0.5 each | 0 to 1 | EZ saving | Strength of the tint while holding Quad, the suit, the ring, the pentagram [read] |
| `r_drawviewmodel` | Q1, QWc | 1 | 0 or 1 | none | Draws the held weapon; hidden while invisible and (GL) in chase view; SW also hides it when fov is above 90 [read] |
| `r_drawviewmodel` | EZ | 1 | 0 to 1 opacity; 2 hides it when fov is above 90 | EZ saving | Held weapon's opacity [read] |
| `cl_muzzleflash` | EZ | 1 | 0 none, 1 all, 2 all but one's own | EZ saving | Muzzle-flash lights [read] |
| `scr_centertime` | Q1, QWc, EZ | 2 | seconds | none | How long a centred message stays [read] |
| `scr_centertime` | Q2 | 2.5 | seconds | none | Same [read] |
| `scr_conspeed` | Q1, QWc | 300 | pixels of console height per second | none | Speed the console drops and rises [read] |
| `scr_conspeed` | Q2, Q3 | 3 | screen heights per second (the console is half a screen, so 3 opens it in one sixth of a second) | none | Same purpose, different unit [read; derived for the timing] |
| `scr_conspeed` | EZ | 9999 | pixels per second at a 320-pixel-high screen, scaled to the real height | EZ saving | Same purpose; the default is effectively instant [read; derived for "instant"] |
| `con_notifytime` | Q1, QWc, Q2, Q3, EZ | 3 | seconds | none | How long each notify line stays at the top of the screen [read] |
| `con_notifylines` | EZ | 4 | at most 15 | EZ saving | Number of notify lines shown [read] |

### Rendering

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `r_fullbright` | Q1 SW, Q1 GL | 0 | 0 or 1 | none | SW: every surface lit fully. GL: the lightmap pass is skipped, so textures show unlit at full texture brightness [read] |
| `r_fullbright` | QWc | 0 | | forced-0 in MP, always | Present but reset to 0 every frame in both renderers: no effect [read] |
| `r_fullbright` | Q2 | 0 | 0 or 1 | forced-0 when not single-player | GL skips the lightmap blend; software lights everything fully [read] |
| `r_fullbright` | Q3 | 0 | 0 or 1 | cheat, latched | At map load every surface's lightmap is replaced by white [read] |
| `r_fullbright` | EZ | 0 | 0 or 1 | EZ saving | World lightmaps ignored; honoured only in demo playback or on a local server whose info says cheats are on [read] |
| `r_lightmap` | Q1 GL | 0 | 0 or 1 | none | 1 draws the lightmaps without blending them over the textures, so only the light is seen [read] |
| `r_lightmap` | QW GL | 0 | | forced 0 every frame | No effect [read] |
| `r_lightmap` | Q3 | 0 | 0, 1, 2 | none (not cheat) | 1 draws only the lightmaps; 2, at map load, false-colours lightmaps by intensity [read] |
| `r_drawflat` | Q1 SW | 0 | 0 or 1 | none | Draws each surface in one flat colour [read] |
| `r_drawflat` | QW SW | 0 | | forced 0 every frame | No effect [read] |
| `r_drawflat` | Q2 | (not a renderer setting) | | forced-0 when not single-player | Only on Q2's multiplayer reset list; the renderers do not read it (Q2's software flat mode is `sw_drawflat`) [read] |
| `r_drawflat` | EZ | 0 | 0 textures, 1 walls and floors coloured, 2 floors only, 3 walls only | EZ saving; not changeable during a match | Solid colours `r_wallcolor` and `r_floorcolor` by surface slope [read] |
| `r_wallcolor` | EZ | "255 255 255" | RGB | EZ saving | Wall colour for `r_drawflat` [read] |
| `r_floorcolor` | EZ | "50 100 150" | RGB | EZ saving | Floor and ceiling colour for `r_drawflat` [read] |
| `r_dynamic` | Q1 GL, QW GL | 1 | 0 or 1 | none | Dynamic lights added into the world's lightmaps; has effect only when `gl_flashblend` is 0 [read] |
| `r_dynamic` | EZ | 2 in builds with the modern renderer, else 1 | 0 off, 1 on the CPU, 2 on the GPU | EZ saving | Dynamic lights on world surfaces [read] |
| `r_wateralpha` | Q1 GL | 1 | 0 to 1 | none | Opacity of liquid surfaces [read] |
| `r_wateralpha` | QW GL | 1 | 0 to 1 | none | Same, but forced to 1 unless the server's info has a nonzero `watervis` [read] |
| `r_fastsky` | Q3 | 0 | 0 or 1 | saved | Sky not drawn: the screen is cleared to black where sky is, and portal views are skipped [read] |
| `r_fastsky` | EZ | 0 | 0 or 1 | EZ saving | Sky drawn as the solid colour `r_skycolor` [read] |
| `r_skycolor` | EZ | "40 80 150" | RGB | EZ saving | Colour for `r_fastsky` [read] |
| `r_skyname` | EZ | empty | a sky box name | EZ saving | Replaces the map's own sky box [read] |
| `r_bloom` | EZ | 0 | 0 or 1 | saved | Light bloom [read] |
| `r_fullbrightskins` | EZ (spelled `r_fullbrightSkins`) | 1 | 0 to 1 | EZ saving | How fully lit player skins are, limited by the server's `fbskins` info (0 in Team Fortress when absent); changing it announces the value to everyone [read] |
| `r_lerpmodels` | Q2 GL, Q2 software | 1 | 0 or 1 | none | Interpolates model animation between frames; 0 snaps [read] |
| `r_netgraph` | QWc, EZ | 0 | 0 or 1 | none | Network graph of latency and loss [read] |
| `gl_picmip` | Q1 GL, QW GL | 0 | integer | none | Each step halves the width and height of every texture uploaded afterwards (2D pictures included); a change applies to textures loaded later [read] |
| `gl_picmip` | Q2 GL | 0 | integer | none | Same halving, but only for mipmapped (world and model) textures; the video menu's texture-quality slider sets it to 3 minus the slider [read] |
| `gl_picmip` | EZ | 0 | integer | EZ saving; reloads textures | World textures only (models, liquids, icons have their own settings) [read] |
| `gl_texturemode` | Q1 GL, QW GL | (COMMAND, not a setting) | one of six OpenGL filter names | | Sets texture filtering; starts as GL_LINEAR_MIPMAP_NEAREST [read] |
| `gl_texturemode` | Q2 GL | GL_LINEAR_MIPMAP_NEAREST | the six filter names | saved | Texture filtering [read] |
| `gl_texturemode` | EZ | GL_LINEAR_MIPMAP_LINEAR | the six filter names | EZ saving | Texture filtering [read] |
| `vid_fullscreen` | Q2 | 0 | 0 windowed, 1 full screen | saved | Window mode [read] |
| `vid_fullscreen` | EZ | 1 | 0 or 1 | EZ saving; applied on video restart | Window mode [read] |
| `vid_vsync` | EZ | 0 | 0 or 1 | EZ saving | Vertical sync; overrides `cl_maxfps` [read] |
| `vid_vsync_lag_fix` | EZ | 0 | 0 or 1 | EZ saving | Experimental input-delay reduction under vsync [read] |
| `vid_vsync_lag_tweak` | EZ | 1.0 | real; 0.2 or below may lower the frame rate, 10 or above disables the fix | EZ saving | Strength of that reduction [read] |

### Input, rate, frame rate

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `sensitivity` | Q1, QWc | 3 | multiplier | saved | Mouse counts are multiplied by this, then turned into degrees by `m_yaw`/`m_pitch` (0.022 degrees per count each) [read] |
| `sensitivity` | Q2 | 3 | multiplier | saved | Same rule, same 0.022 [read] |
| `sensitivity` | Q3 | 5 | multiplier | saved | Same rule and 0.022; plus `cl_mouseAccel` adds speed-dependent gain, and a zoom scales it [read] |
| `sensitivity` | EZ | 12 | multiplier | EZ saving | Same rule and 0.022 [read] |
| `cl_yawspeed` | Q1, QWc, Q2, Q3, EZ | 140 | degrees per second | Q3 saved; others none | Turn rate of the turn-left/turn-right keys (times `cl_anglespeedkey`, 1.5, while running) [read] |
| `cl_run` | Q2 | 0 | 0 or 1 | saved | Always run; the run key then walks [read] |
| `cl_run` | Q3 | 1 | 0 or 1 | saved | Same [read] |
| `lookspring` | Q1, QWc, Q2, EZ | 0 | 0 or 1 | saved (EZ saving) | Re-centres the view pitch when mouse look is released [read] |
| `lookstrafe` | Q1, QWc, Q2, EZ | 0 | 0 or 1 | saved (EZ saving) | While mouse look is held, sideways mouse motion strafes instead of turning [read] |
| `cl_maxfps` | QWc | 0 | 0 or frames per second | saved | Client frame rate cap (the client sends one packet per frame, so also its packet rate [derived]); always held between 30 and 72; 0 uses `rate` divided by 80, held between 30 and 72 [read] |
| `cl_maxfps` | Q2 | 90 | frames per second | none | Client frame rate cap, no clamp [read] |
| `cl_maxfps` | EZ | 0 | 0 or frames per second | EZ saving | Drawing frame rate cap: 0 is unlimited, any other value at least 30; physics and packets are limited separately by the server's `maxfps` info (72 when absent) [read] |
| `rate` | QWc | 2500 | bytes per second | saved, userinfo | Most the server sends this client per second; the server holds it to 500 to 10000 [read] |
| `rate` | Q2 | 25000 | bytes per second | saved, userinfo | Same; the server holds it to 100 to 15000 (5000 when absent) [read] |
| `rate` | Q3 | 3000 | bytes per second | saved, userinfo | Same; the server holds it to 1000 to 90000 (3000 when absent), then to `sv_maxrate` [read] |
| `rate` | EZ | 25000 | bytes per second | userinfo | Same [read] |
| `cl_nolerp` | Q1, EZ | 0 | 0 or 1 | none | 1 stops interpolating entities between server messages [read] |
| `cl_nopred` | QWc, EZ | 0 | 0 or 1 | none | 1 stops predicting one's own movement (other players still predicted in EZ) [read] |
| `cl_predict_players` | QWc, EZ | 1 | 0 or 1 | none | Predicts other players' positions [read] |
| `show_speed` | EZ | 0 | 0 off, 1 measured, 2 predicted | EZ saving | On-screen horizontal speed in units per second [read] |

### Player identity

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `name` | Q1 | (COMMAND; the setting is `_cl_name`, default "player", saved) | | | The player's name [read] |
| `name` | QWc | "unnamed" | text | saved, userinfo | Player name [read] |
| `name` | Q2 | "unnamed" | text | saved, userinfo | Player name [read] |
| `name` | Q3 | "UnnamedPlayer" | text | saved, userinfo | Player name [read] |
| `name` | EZ | "player" | text, validated on change | userinfo | Player name [read] |
| `skin` | QWc | empty | a skin picture name | saved, userinfo | Which skin picture one's figure wears [read] |
| `skin` | Q2 | "male/grunt" | model folder / skin | saved, userinfo | Both the player model and its skin [read] |
| `skin` | EZ | empty | skin name | userinfo, kept on config reset | As QW [read] |
| `model` | Q3 | "sarge" | model / skin | saved, userinfo | The player model and skin (as Q2's `skin`) [read] |
| `topcolor`, `bottomcolor` | QWc | 0 | colour rows 0 to 13 | saved, userinfo | Shirt and trousers colours [read] |
| `topcolor`, `bottomcolor` | EZ | empty | colour rows | userinfo, kept on config reset | Same [read] |
| `team` | QWc | empty | text | saved, userinfo | Team name [read] |
| `team` | EZ | empty | text | userinfo, kept on config reset | Same [read] |
| `baseskin` | QWc, EZ | "base" | skin name | none (EZ: reloads pictures) | Skin used for players whose own skin is missing [read] |
| `noskins` | QWc, EZ | 0 | 0, 1, 2 or more | none | 1 every player wears the base skin; 2 or more keeps skins already present but downloads no new ones [read] |
| `enemyskin`, `teamskin` | EZ | empty | skin name | EZ saving | Force every enemy's / teammate's skin [read] |
| `password` | QWc | empty | text | userinfo (not saved) | Password sent when joining [read] |
| `password` | QWs | empty | text | none | The server's join password [read] |
| `password` | Q2 | empty | text | userinfo | Sent when joining; the game library compares it with its own `password` on the server (one setting serves both roles on a listen server) [read; derived for the listen-server overlap] |
| `password` | Q3 | empty | text | userinfo | Sent when joining; the server's password is `g_password` [read] |
| `password` | EZ (client and server) | empty | text | client: userinfo; both: never set by a server | As QW [read] |
| `cl_fakename_suffix` | EZ | ": " | text | EZ saving | Text placed after the fake name in fake-name messages [read] |

### Server, rules and match

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `deathmatch` | Q1 | 0 | 0, 1, 2 | none | Deathmatch rules number, read by the game code at map start [read] |
| `deathmatch` | QWs | 1 | 1 to 5 in the shipped game code | serverinfo | Rule variant: 1 classic (weapons are taken, items return); 2 the old rules (weapons stay, health and other items do not come back); 3 weapons stay and items return; 4 everyone starts with all weapons, full armour, 250 health and 3 s of invulnerability; 5 a lighter armed start (200 health) with weapons staying [read] |
| `deathmatch` | Q2 | 0 | 0 or nonzero | latched, then serverinfo | On or off; the variants are the `dmflags` bits; a dedicated server forces 1 unless `coop` [read] |
| `deathmatch` | EZ server | 3 | 1 to 4 (as QWs; 4 is the arena start) | serverinfo | Rule variant [read] |
| `coop` | Q1 | 0 | 0 or 1 | none | Cooperative play [read] |
| `coop` | Q2 | 0 | 0 or 1 | latched | Cooperative play; turned off when `deathmatch` is also set [read] |
| `coop` | EZ server | 0 | 0 or 1 | none | Next map starts cooperative; forced off when `deathmatch` is set [read] |
| `teamplay` | Q1 | 0 | 0, 1, 2 | notify | Team rules number for the game code [read] |
| `teamplay` | QWs, EZ server | 0 | values interpreted by the game code; in the QW game code 0 none, 1 no damage from teammates or from oneself, 2 teammates can be hurt (team kills are penalised), 3 no damage from teammates but one's own damage counts | serverinfo | Team rules [read] |
| `fraglimit` | Q1 | 0 | frags; 0 none | notify | Frags that end the match [read] |
| `fraglimit` | QWs, Q2, EZ server | 0 | frags; 0 none | serverinfo | Same [read] |
| `fraglimit` | Q3 | 20 | frags; 0 none | serverinfo, saved, survives restart | Same [read] |
| `timelimit` | Q1 | 0 | minutes; 0 none | notify | Match length [read] |
| `timelimit` | QWs, Q2, EZ server | 0 | minutes | serverinfo | Same [read] |
| `timelimit` | Q3 | 0 | minutes | serverinfo, saved, survives restart | Same [read] |
| `dmflags` | Q2 | 0 | bit field | serverinfo | Deathmatch options; bit 8 no falling damage, 32768 fixed FOV of 90; the other bits (1 no health, 2 no items, 4 weapons stay, 16 instant items, 32 same level, 64 and 128 teams by skin or model, 256 no friendly fire, 512 spawn farthest, 1024 force respawn, 2048 no armour, 4096 allow exit, 8192 infinite ammo, 16384 drop quad) are set from the deathmatch-flags menu [read] |
| `dmflags` | Q3 | 0 | bit field | serverinfo, saved | Only three bits: 8 no falling damage, 16 fixed FOV of 90, 32 no footstep sounds [read] |
| `skill` | Q1 | 1 | 0 to 3 (rounded and clamped at map start) | none | Difficulty [read] |
| `skill` | Q2 | 1 | 0 to 3 | latched | Difficulty [read] |
| `skill` | EZ server | 1 | 0 easy, 1 normal, 2 hard, 3 nightmare | none | Difficulty [read] |
| `samelevel` | Q1 | 0 | 0 or 1 | notify | Stay on the same map after a level ends [read] |
| `samelevel` | QWs | 0 | 0; 1 same map; 2 the exit kills; 3 the exit kills except on the start map | serverinfo | Level change rule [read] |
| `samelevel` | EZ server | 1 | as QWs | none | Same [read] |
| `pausable` | Q1, QWs | 1 | 0 or 1 | none | Whether players may pause [read] |
| `pausable` | EZ server | 0 | 0 or 1 | none | Same [read] |
| `hostname` | Q1 | "UNNAMED" | text | none | Server name [read] |
| `hostname` | QWs, EZ server | "unnamed" | text | serverinfo | Server name [read] |
| `hostname` | Q2 | "noname" | text | serverinfo, saved | Server name [read] |
| `sv_hostname` | Q3 | "noname" | text | serverinfo, saved | Server name [read] |
| `maxclients` | QWs | 8 | up to 32 | serverinfo | Player slots [read] |
| `maxclients` | Q2 | 1 | up to 256 | serverinfo, latched | Player slots; set to 8 in deathmatch when 1 or less, to 4 in coop, and to 1 in single-player [read] |
| `maxclients` | EZ server | 24 | | serverinfo | Player slots [read] |
| `sv_maxclients` | Q3 | 8 | | serverinfo, latched | Player slots [read] |
| `maxspectators` | QWs, EZ server | 8 | | serverinfo | Spectator slots [read] |
| `maxspectators` | Q2 | 4 | | serverinfo | Spectator slots [read] |
| `spectator_password` | QWs, EZ server | empty | text | none | Password for joining as a spectator [read] |
| `spectator_password` | Q2 | empty | text | marked userinfo | Same, in the game library [read] |
| `maxfps` | EZ server | 77 | packets per second | serverinfo | Most packets per second a client may send; clients limit their physics frames to it [read] |
| `game` | Q2 | empty | a folder name | latched, serverinfo | The mod folder layered over the base game [read] |
| `registered` | Q1, QWc | 0 | 0 or 1 | none (set by the engine) | 1 when the registered game's data is found [read] |
| `registered` | EZ | 1 | | read-only | Always registered [read] |
| `sv_cheats` | Q3 | 1 at creation; the `map` command sets 0, `devmap` sets 1 | 0 or 1 | read-only, sent to clients with the server's system info | Whether cheat settings may be changed [read] |
| `sv_cheats` | EZ server | 0 | 0 or 1 | none | Allows cheats; takes effect on the next map [read] |
| `sv_fps` | Q3 | 20 | frames per second; below 1 is reset | not saved | Server simulation rate [read] |
| `sv_timeout` | Q3 | 200 | seconds | not saved | Silence after which a client is dropped [read] |
| `sv_maxping` | Q3 | 0 | milliseconds; 0 none | serverinfo, saved | A non-local client whose ping is above it is refused at connect [read] |
| `sv_maxping` | EZ server | 0 | milliseconds | none | A player above it is told and kept as a spectator [read] |
| `sv_maxrate` | Q3 | 0 | bytes per second; 0 none | serverinfo, saved | Caps each client's `rate` [read] |
| `sv_maxrate` | EZ server | 0 | bytes per second; 0 none | none | Same [read] |
| `sv_mintic` | QWs | 0.03 | seconds | none | Shortest server frame [read] |
| `sv_mintic` | EZ server | 0.013 | seconds | none | Same [read] |
| `sv_maxtic` | QWs, EZ server | 0.1 | seconds | none | Longest server frame [read] |
| `sv_phs` | QWs, EZ server | 1 | 0 or 1 | none | Uses the map's hearable-set table to limit who receives sounds and messages [read] |
| `sv_spectalk` | QWs, EZ server | 1 | 0 or 1 | none | Whether players see spectators' chat [read] |
| `sv_specprint` | EZ server | 0 | bit field | none | Default set of messages a spectator is shown [read] |
| `sv_unfake` | EZ server | 1 | 0 or 1 | none | Replaces carriage-return characters in public chat to stop faked messages [read] |
| `sv_speedcheck` | EZ server | 1 | 0 or 1 | none | Detects players whose clock runs fast [read] |
| `sv_login` | EZ server | 0 | 0 off; 1 login mandatory; other values not settled by this reading | none | Account login for players [read; partly] |
| `sv_maxpitch` | EZ server | 80 | degrees | none | Largest downward look allowed; newer clients follow it, older ones keep 80 [read] |
| `sv_minpitch` | EZ server | -70 | degrees | none | Largest upward look allowed (negative is up); older clients keep -70 [read] |

### Movement (all server-side)

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `sv_gravity` | Q1 | 800 | units per second squared | notify | Gravity [read] |
| `sv_gravity` | QWs, EZ server | 800 | units per second squared | none | Gravity; sent to clients at map load and read only then, so a change applies from the next map [read] |
| `sv_gravity` | Q2 | 800 | units per second squared | none (game library) | Gravity, read every frame [read] |
| `sv_friction` | Q1 | 4 | per second | notify | Ground friction [read] |
| `sv_friction` | QWs, EZ server | 4 | per second | none | Same; applied from next map [read] |
| `sv_stopspeed` | Q1, QWs, EZ server | 100 | units per second | none | Below this speed friction acts as if at this speed (stops quickly) [read] |
| `sv_maxspeed` | Q1 | 320 | units per second | notify | Top wished speed [read] |
| `sv_maxspeed` | QWs, EZ server | 320 | units per second | none | Same; applied from next map [read] |
| `sv_accelerate` | Q1, QWs, EZ server | 10 | per second | none | Ground acceleration; ALSO used in the air (with the wished speed capped at 30) [read] |
| `sv_airaccelerate` | QWs | 0.7 | | none | Sent to clients but NOT used by the player move: air acceleration uses `sv_accelerate` [read] |
| `sv_airaccelerate` | EZ server | 10 | | none | Same: sent, unused by the player move [read] |
| `sv_airaccelerate` | Q2 | 0 | 0 or nonzero | latched; deathmatch only | A switch: 0 gives Quake II's air move (normal acceleration rule at factor 1, no 30 cap); nonzero gives the capped air move, but at the fixed factor 10, not at this value [read] |
| `sv_wateraccelerate` | QWs, EZ server | 10 | per second | none | Acceleration in water [read] |
| `sv_waterfriction` | QWs, EZ server | 4 | per second | none | Friction in water [read] |
| `sv_spectatormaxspeed` | QWs, EZ server | 500 | units per second | none | Spectator top speed [read] |
| `sv_maxvelocity` | Q1, QWs, EZ server, Q2 | 2000 | units per second | none | Each velocity component is clamped to plus or minus this [read] |
| `edgefriction` | Q1 | 2 | multiplier | none | Friction multiplier near a drop-off (QW and EZ fix it at 2 with no setting) [read] |
| `sv_nostep` | Q1 | 0 | 0 or 1 | none | 1 stops the player stepping up stairs [read] |
| `sv_idealpitchscale` | Q1 | 0.8 | multiplier | none | Scale of the automatic view pitch on slopes [read] |

### Sound

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `volume` | Q1, QWc | 0.7 | 0 to 1 | saved | Sound effects volume [read] |
| `volume` | EZ | 0.7 | 0 to 1 | EZ saving | Same [read] |
| `bgmvolume` | Q1, QWc | 1 | 0 to 1 | saved | CD music volume [read] |
| `bgmvolume` | EZ | 1 | 0 to 1 | EZ saving | Music volume [read] |

### Server browser and team messages (EZ only)

| Name | Game | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- | --- |
| `sb_hideempty` | EZ | 1 | 0 or 1 | EZ saving | Server browser hides empty servers [read] |
| `sb_hidefull` | EZ | 0 | 0 or 1 | EZ saving | Server browser hides full servers [read] |
| `tp_loadlocs` | EZ | 1 | 0 or 1 | EZ saving | Loads the map's place-name file for team messages [read] |
| `tp_name_*` (armor, armortype_ga/ya/ra, at, axe, backpack, cells, enemy, eyes, ga, gl, health, lg, mh, nails, ng, none, nothing, pent, pented, quad, quaded, ra, ring, rl, rlg, rockets, rune1 to rune4, separator, sg, shells, sng, someplace, ssg, suit, teammate, weapon, ya) | EZ | the item's short word: "armor", "at", "axe", "cells", "gl", "health", "nails", "ng", "nothing", "rox" (rockets), "sg", "shells", "sng", "someplace", "ssg", "suit", "weapon", "/" (separator), "mega" (mh), "pack" (backpack), "resistance", "strength", "haste", "regeneration" (runes 1 to 4); armour, powerups, rl, lg, enemy and eyes default to their short word in a colour; none and teammate are empty | text | EZ saving | The words team-message macros put in place of each item, place or state [read] |
| `tp_need_health` | EZ | 50 | amount | EZ saving | Below this, the "need" macro reports health [read] |
| `tp_need_ga` | EZ | 60 | amount | EZ saving | Armour threshold for the "need" macro [read] |
| `tp_need_cells` | EZ | 13 | amount | EZ saving | Cells threshold [read] |
| `tp_need_shells` | EZ | 0 | amount | EZ saving | Shells threshold [read] |
| `tp_need_rl` | EZ | 1 | 0 or 1 | EZ saving | Report a missing rocket launcher [read] |
| `tp_need_weapon` | EZ | "87" | weapon numbers as digits | EZ saving | Weapons whose absence the "need" macro reports [read] |
| `tp_poweruptextstyle` | EZ | 0 | 0 long names, 1 short | EZ saving | Powerup words in the built-in report [read] |
| `tp_tooktimeout` | EZ | 15 | seconds | EZ saving | How long the "last item taken" macro remembers [read] |
| `tp_weapon_order` | EZ | "78654321" | weapon numbers best first | EZ saving | Order for "best weapon" in team messages (the shipped reference text quotes "78564321"; the code's default is "78654321") [read] |

---

## Same concept or a different one under the same name

- `crosshair`: **DIFFERENT CONCEPT.** One name, three value spaces: Q1 any nonzero shows `+`; QW
  value 2 is a different dot pattern; Q2 the number picks picture 1 to 3; EZ 0 to 7 picks among its
  own shapes with 4 (a dot) as default [read]. Only "0 is off" is shared [derived].
- `crosshaircolor` (QW, EZ): **DIFFERENT CONCEPT** in its value: QW a palette index (79), EZ an
  RGB triple [read].
- `gamma`: exists only in Q1 and QW, same concept there (exponent; above 1 darker) [read]. In
  the 1996 GL builds it has no visible effect [read]. The other games use other names (§ One concept, several spellings),
  and Q3's runs the opposite way [read].
- `fov`: **SAME CONCEPT** (horizontal degrees of the drawn view) in Q1, QW, Q2, EZ, with different
  limits: 10 to 170 (Q1, QW), 1 to 160 and server-enforced (Q2), 10 to 140 (EZ) [read]. Two traps
  [derived]: Q2's is a userinfo value the SERVER applies (and can force to 90 by dmflags); EZ's is
  the 4:3-equivalent, so on a wide screen EZ's real horizontal angle is wider than the number.
- `sensitivity`: **SAME CONCEPT** (multiplier on mouse counts, 0.022 degrees per count per unit at
  default `m_yaw`/`m_pitch`) in all four; defaults 3, 3, 5 (Q3), 12 (EZ) [read]. Q3 adds optional
  acceleration [read].
- `cl_bob`, `cl_bobcycle`, `cl_bobup`: **SAME CONCEPT** in Q1, QW and EZ (same formula), EZ
  defaults all 0 (bob off) and bobs only on ground [read]. Q2 and Q3 do not have these names; their
  `bob_up` / `cg_bobup` are a different concept (§ Other shared names a port would trip on) [read].
- `cl_rollangle`: **SAME CONCEPT** for the own view (degrees at full roll speed) in Q1, QWc, EZ
  [read]. Two traps [read]: in Q1 the one setting also rolls the player's model as others see it
  (times 4); in QW the server holds its own `cl_rollangle` for that. EZ caps at 45 degrees, and
  rulesets may clamp to 5 [read].
- `cl_rollspeed`: **SAME CONCEPT**, 200 everywhere [read].
- `v_kickpitch`, `v_kickroll`, `v_kicktime`: **SAME CONCEPT** in Q1, QW, EZ; EZ defaults 0 (no
  kick) [read]. Q2 and Q3 have no such settings [read].
- `v_idlescale`: **SAME CONCEPT** in Q1, QW, EZ; EZ refuses players' changes in Team Fortress [read].
- `r_fullbright`: **SAME INTENT** (ignore the world's lighting) in all five, but **different
  reach** [read]: Q1 works freely; QW is dead (always reset); Q2 works only in single-player; Q3
  needs cheats and a map reload, and makes lightmaps white; EZ needs a cheat-enabled local server or a demo. GL builds draw unlit
  textures; software builds light fully [read].
- `r_drawflat`: **DIFFERENT CONCEPT** in EZ: Q1 SW colours each surface in its own flat colour; EZ uses two chosen colours by slope and has modes 2 and 3 [read]. QW and Q2 carry the name with
  no effect [read].
- `r_lightmap`: Q1 GL and Q3 value 1 are **THE SAME CONCEPT** (lightmaps only) [read]; Q3 adds
  value 2 (false colour) [read]; QW's is dead [read].
- `r_dynamic`: **SAME CONCEPT** (dynamic lights on world surfaces) in Q1 GL, QW GL and EZ; EZ adds
  value 2 (computed on the GPU) and defaults to it in modern builds [read].
- `r_fastsky`: **DIFFERENT IN WHAT IS SEEN**: Q3 shows black (and drops portal views); EZ shows
  `r_skycolor` [read]. Same intent (skip the sky) [derived].
- `r_wateralpha`: Q1 GL and QW GL only, **SAME CONCEPT**; QW needs the server's `watervis` [read].
  EZ renamed it (§ One concept, several spellings) [read].
- `r_drawviewmodel`: **DIFFERENT VALUE MEANING** in EZ: Q1 and QW on/off; EZ an opacity 0 to 1
  with 2 meaning "hide above fov 90" [read].
- `scr_conspeed`: **DIFFERENT UNITS**: Q1 and QW pixels per second (300); Q2 and Q3 screen heights
  per second (3); EZ pixels per second normalised to a 320-pixel screen (9999) [read]. A value
  copied across games is wrong by a factor of the screen height [derived].
- `scr_centertime`: **SAME CONCEPT** (seconds); 2 in Q1, QW, EZ, 2.5 in Q2; Q3's is `cg_centertime`
  (3, cheat) [read].
- `gl_picmip`: **SAME CONCEPT** (each step halves texture width and height) [read]; scope
  differs: Q1/QW GL every later upload including pictures, Q2 GL mipmapped textures only, EZ world
  textures only [read].
- `gl_polyblend`: **SAME CONCEPT** in Q1 GL, QW GL, Q2 GL, EZ [read].
- `gl_texturemode`: **SAME CONCEPT** (OpenGL filter name), but a COMMAND in Q1 and QW and a saved
  setting in Q2 and EZ; defaults GL_LINEAR_MIPMAP_NEAREST (Q1, QW, Q2) and GL_LINEAR_MIPMAP_LINEAR
  (EZ) [read].
- `gl_cshiftpercent`: **SAME CONCEPT** in Q1 GL, QW GL, EZ [read].
- `v_contentblend` (QW, EZ): **DIFFERENT VALUE MEANING**: QW a switch; EZ a 0-to-1 strength
  (0.2) [read]. The other `v_*cshift` names exist only in EZ [read].
- `cl_run` (Q2, Q3): **SAME CONCEPT**; defaults 0 (Q2) and 1 (Q3) [read].
- `cl_yawspeed`: **SAME CONCEPT**, 140 degrees per second everywhere [read].
- `m_*`: `m_invert` exists in no game [read]. In all four, inverting the mouse is a negative
  `m_pitch`; `m_pitch` and `m_yaw` default to 0.022 everywhere [read]. `m_forward` and `m_side`
  default to 1 and 0.8 in Q1, QW, EZ (Q2's main client registers `m_side` at 1) but 0.25 each in Q3
  [read].
- `cl_nopred` (QW, EZ): **SAME CONCEPT** [read]. Q2 spells the opposite sense `cl_predict` (1
  on), Q3 `cg_nopredict` (same sense as `cl_nopred`) [read].
- `cl_maxfps`: **DIFFERENT CONCEPT** in detail: QW caps the client frame AND packet rate between
  30 and 72 and treats 0 as "from rate"; Q2 caps frames with no clamp, default 90; EZ caps drawing
  only, 0 is unlimited, physics follow the server's `maxfps` [read]. Q3's is `com_maxfps` (85)
  [read].
- `rate`: **SAME CONCEPT** (bytes per second the server may send) in QW, Q2, Q3, EZ; defaults
  2500, 25000, 3000, 25000; server clamps 500 to 10000, 100 to 15000, 1000 to 90000 [read].
- `name`: **SAME CONCEPT** (userinfo name) in QW, Q2, Q3, EZ; in Q1 it is a command over
  `_cl_name` [read].
- `skin`: **DIFFERENT CONCEPT**: QW and EZ a skin picture for the one player model; Q2 "model/skin"
  choosing the whole figure (Q3 calls that `model`) [read].
- `password`: **DIFFERENT ROLES under one name**: QW and EZ have a client one (userinfo) and a
  separate server one; Q2's single setting is both; Q3's is client-only (the server's is
  `g_password`) [read].
- `skill`: **SAME CONCEPT** (0 to 3 difficulty) in Q1, Q2, EZ; Q2 latches it [read].
- `deathmatch`: **DIFFERENT CONCEPT**: in the first game a rule-variant number the game code
  interprets (QW code: 1 to 5); in Q2 an on/off switch with the variants in `dmflags`; Q3 has no
  `deathmatch` (it uses `g_gametype`) [read].
- `coop`: **SAME CONCEPT** in Q1, Q2, EZ [read].
- `dmflags` (Q2, Q3): **DIFFERENT CONCEPT**: different bit meanings except 8 (no falling damage); fixed FOV is 32768 in Q2 and 16 in Q3; Q3 has only three bits [read].
- `fraglimit`, `timelimit`: **SAME CONCEPT** (frags; minutes) in all; Q3's fraglimit defaults to
  20, all others 0 [read].
- `teamplay`: **SAME CONCEPT** in Q1, QW, EZ; values are defined by the game code [read].
- `samelevel`: Q1 a plain switch; QW and EZ overload 2 and 3 to mean "the exit kills"; EZ defaults
  to 1 [read]. **DIFFERENT VALUE MEANING above 1** [derived].
- `hostname`, `maxclients`, `maxspectators`, `pausable`, `spectator_password`: **SAME CONCEPT**
  where present; defaults differ (see § Each of our setting names, game by game) [read].
- `sv_gravity`, `sv_maxvelocity`: **SAME CONCEPT** in Q1, QW, Q2, EZ (units per second squared; units per second) [read]. In QW and EZ a change applies from the next map; in Q1 and Q2 at once
  [read].
- `sv_maxspeed`, `sv_accelerate`, `sv_friction`, `sv_stopspeed`: **SAME CONCEPT** in Q1, QW, EZ; Q2 has fixed values and no settings (300 top speed, 10 acceleration, 6 friction, 100 stop speed),
  Q3 uses `g_speed` (320) [read].
- `sv_airaccelerate`: **DIFFERENT CONCEPT** and a trap: in QW and EZ it is sent but has no effect
  on the player move; in Q2 it is only an on/off switch for the capped air move (deathmatch, from
  the next map); in no game does its number set the air acceleration of the player move [read].
- `sv_maxrate`, `sv_maxping` (Q3, EZ): `sv_maxrate` **SAME**; `sv_maxping` **DIFFERENT
  CONSEQUENCE**: Q3 refuses the connection, EZ keeps the player as a spectator [read].
- `sv_cheats` (Q3, EZ): **DIFFERENT CONCEPT** in control: read-only in Q3, set by the `map` and
  `devmap` commands; an ordinary server setting in EZ [read].
- `sv_rollangle`: not in our list's games except Q2; there it rolls the player's model as others
  see it (times 4), default 2, with `sv_rollspeed` 200 [read]. It matches QW's server-side
  `cl_rollangle`, not the client one [derived].
- `cl_shownet`: in Q1, QW, Q2, Q3, EZ; **SAME INTENT** (print incoming network messages for
  debugging), with levels that differ by game [read].
- `volume`, `bgmvolume`: **SAME CONCEPT** in Q1, QW, EZ (0 to 1) [read].
- `vid_fullscreen` (Q2, EZ): **SAME CONCEPT**; defaults 0 (Q2) and 1 (EZ) [read].
- `con_notifytime`: **SAME CONCEPT**, 3 s everywhere [read].
- `viewsize`: **SAME CONCEPT** (view size) but different range: Q1, QW, EZ 30 to 120 (110 and 120
  remove parts of the status bar); Q2 40 to 100 [read].
- `lookspring`, `lookstrafe`: **SAME CONCEPT** in Q1, QW, Q2, EZ [read].
- `r_netgraph`, `noskins`, `baseskin`, `team`, `topcolor`, `bottomcolor`, `cl_nolerp`,
  `cl_predict_players`, `sv_mintic`, `sv_maxtic`, `sv_phs`, `sv_spectalk`, `sv_spectatormaxspeed`,
  `sv_wateraccelerate`, `sv_waterfriction`, `registered`: first-game family only (Q1/QW and EZ),
  **SAME CONCEPT**, defaults as in § Each of our setting names, game by game [read].

---

## Other shared names a port would trip on

- `r_shadows`: Q1 GL, QW GL, EZ (classic renderer only): **SAME CONCEPT** in all three (a flat
  50 % black silhouette under models; § Shadows in each game) [read]. Q3's renderer has a shadow mode too, but
  reads it from the setting named `cg_shadows`, so there is no `r_shadows` in Q3 [read]. Not
  ambiguous by name; the trap is that Q3's shadow mode is shared between the client game and the
  renderer under the `cg_` name [derived].
- `gl_shadows`: Q2 only [read]. `cg_shadows`: Q3 only [read].
- `r_lerpmodels`: Q2 only (in our list; see § Each of our setting names, game by game). EZ's nearest is `r_lerpframes` [read].
- `r_speeds`: all five [read]. Q1, QW, Q2, EZ: nonzero prints per-frame drawing time and counts.
  Q3: cheat-protected, and values 1 to 6 each select a different set of counters [read].
  **DIFFERENT VALUE MEANING** in Q3 [derived].
- `r_novis`: all five; **SAME CONCEPT** (ignore the map's visibility data and draw everything); Q3's is cheat-protected [read].
- `gl_flashblend`: Q1 GL, QW GL (default 1), Q2 GL (default 0), EZ (default 0) [read]. Same idea
  (draw dynamic lights as glowing spheres instead of lighting surfaces), but in EZ the values mean
  0 no glow spheres, 1 spheres on explosions and powerup/flag carriers, 2 also on flying rockets,
  and EZ's surface lighting is governed by `r_dynamic` [read]. **DIFFERENT VALUE MEANING** in EZ
  [derived].
- `cl_footsteps`: Q2 only (1 plays footstep sounds) [read]. Q3's is `cg_footsteps` (1, cheat)
  [read].
- `hand`: Q2 only: 0 right-handed, 1 left-handed (weapon mirrored), 2 centred (weapon not drawn); saved, userinfo [read].
- `crosshairsize` (EZ, a scale, default 1) versus `cg_crosshairSize` (Q3, pixels on a 640-wide
  virtual screen, default 24) [read]: **DIFFERENT UNITS** [derived]. `cg_drawCrosshair` (Q3, default 4):
  0 off, other values pick one of 10 pictures by the value modulo 10 [read].
- `gamma` / `vid_gamma` / `r_gamma` / `gl_gamma`: see § One concept, several spellings. **Direction trap**: Q1 `gamma` and
  Q2 `vid_gamma` darken as they rise (exponent on the input); Q3 `r_gamma` brightens as it rises
  (exponent 1 over the value, clamped 0.5 to 3.0) [read]. EZ's `gl_gamma` (0.3 to 3) applies only
  with its software-palette mode [read].
- `s_volume` / `volume`: different names, same concept (§ One concept, several spellings).
- `in_mouse`: Q1 (only in its Linux GLX build), Q2, Q3; saved in Q2 and Q3 [read]. Q1 and Q2:
  0 or 1, mouse off or on. Q3 Windows: 0 off, 1 DirectInput, -1 the plain system mouse (and the
  game falls back to -1 by itself when DirectInput fails); latched there [read]. **DIFFERENT VALUE
  MEANING** in Q3 [derived].
- `cl_predict` (Q2, 1 = predict) versus `cl_nopred` (QW, EZ, 1 = do not predict) and
  `cg_nopredict` (Q3): opposite polarity under different names [read].
- `sv_cheats`: see § Same concept or a different one under the same name. Q2's equivalent is `cheats` (latched, serverinfo, default 0) [read].
- `bob_up` (Q2, game library, 0.005) and `cg_bobup` (Q3, 0.005, cheat): **DIFFERENT CONCEPT from
  `cl_bobup`**: they are the bob's height per unit of speed (the role of Q1's `cl_bob`), not a
  fraction of the cycle [read]. Q2's `bob_pitch` and `bob_roll` (0.002 each) and `run_pitch`
  (0.002) and `run_roll` (0.005) tilt the view by speed; Q3's `cg_bobpitch`, `cg_bobroll` (0.002),
  `cg_runpitch` (0.002), `cg_runroll` (0.005) are the client-side equivalents [read].
- `timeout`: QW and EZ server 65 s, Q2 server 125 s: same concept (drop a silent client) [read].
- `cl_sidespeed`: Q1/QW 350, Q2 200, EZ 400; `cl_forwardspeed` 200 (Q1, QW, Q2) and 400 (EZ); `cl_backspeed` 200 (Q1, QW) and 400 (EZ): same concept, different defaults [read].
- `m_forward` / `m_side`: Q3 defaults 0.25 each against 1 and 0.8 elsewhere [read].

---

## One concept, several spellings

| Concept | Names and defaults |
| --- | --- |
| Brightness curve | Q1/QW `gamma` 1 (rises darker); Q2 `vid_gamma` 1 (rises darker; the video menu offers 0.5 to 1.3, GL bakes it into textures at load); Q3 `r_gamma` 1, 1.2 on the Mac (rises brighter); EZ `gl_gamma` 1 [read] |
| Sound effects volume | Q1/QW/EZ `volume` 0.7; Q2 `s_volume` 0.7; Q3 `s_volume` 0.8 [read] |
| Music volume | Q1/QW/EZ `bgmvolume` 1; Q2 `cd_volume` 1 (Linux build); Q3 `s_musicvolume` 0.25 [read] |
| Field of view | Q1/QW/Q2/EZ `fov` 90; Q3 `cg_fov` 90 [read] |
| Crosshair on and style | Q1/QW/Q2/EZ `crosshair` (0, 0, 0, 4); Q3 `cg_drawCrosshair` 4 [read] |
| Crosshair size | EZ `crosshairsize` 1 (scale); Q3 `cg_crosshairSize` 24 (virtual pixels) [read] |
| Held weapon drawn | Q1/QW/EZ `r_drawviewmodel` 1; Q2 `cl_gun` 1 (and `hand` 2 hides it); Q3 `cg_drawGun` 1 [read] |
| Centre-print time | Q1/QW/EZ `scr_centertime` 2; Q2 `scr_centertime` 2.5; Q3 `cg_centertime` 3 [read] |
| Texture shrink | Q1/QW/Q2/EZ `gl_picmip` 0; Q3 `r_picmip` 1 (saved, latched) [read] |
| Texture filter | Q2/EZ `gl_texturemode` (Q1/QW a command); Q3 `r_textureMode` GL_LINEAR_MIPMAP_NEAREST [read] |
| Dynamic lights on surfaces | Q1/QW/EZ `r_dynamic` (1, 1, 2 or 1); Q2 `gl_dynamic` 1; Q3 `r_dynamiclight` 1 [read] |
| Lightmaps only | Q1 GL/Q3 `r_lightmap` 0; Q2 `gl_lightmap` 0 [read] |
| Flat-shaded world | Q1 SW/EZ `r_drawflat` 0; Q2 software `sw_drawflat` 0 [read] |
| Liquid opacity | Q1 GL/QW GL `r_wateralpha` 1; EZ `gl_turbalpha` 1 [read] |
| Frame-rate cap | QW/Q2/EZ `cl_maxfps` (0, 90, 0); Q3 `com_maxfps` 85 [read] |
| Own-movement prediction | QW/EZ `cl_nopred` 0 (1 = off); Q2 `cl_predict` 1 (0 = off); Q3 `cg_nopredict` 0 (1 = off) [read] |
| Model and skin | QW/EZ `skin` (skin only) with fixed model; Q2 `skin` "male/grunt"; Q3 `model` "sarge" [read] |
| Server name | Q1 `hostname` "UNNAMED"; QW/EZ `hostname` "unnamed"; Q2 `hostname` "noname"; Q3 `sv_hostname` "noname" [read] |
| Player slots | QW `maxclients` 8; Q2 `maxclients` 1 (8 in deathmatch); EZ `maxclients` 24; Q3 `sv_maxclients` 8; Q1 the `maxplayers` command [read] |
| Server's join password | QW/EZ `password` (server side); Q2 `password`; Q3 `g_password` [read] |
| Cheats allowed | Q3/EZ `sv_cheats`; Q2 `cheats` 0 [read] |
| Gravity | Q1/QW/Q2/EZ `sv_gravity` 800; Q3 `g_gravity` 800 [read] |
| Top walking speed | Q1/QW/EZ `sv_maxspeed` 320; Q3 `g_speed` 320; Q2 fixed 300 (no setting) [read] |
| Model roll seen by others | QW server `cl_rollangle` 2 / `cl_rollspeed` 200; Q2 `sv_rollangle` 2 / `sv_rollspeed` 200; Q1 shares the client's `cl_rollangle` [read] |
| Bob height per unit speed | Q1/QW/EZ `cl_bob` (0.02, 0.02, 0); Q2 `bob_up` 0.005 (a different formula); Q3 `cg_bobup` 0.005 [read] |
| Footstep sounds | Q2 `cl_footsteps` 1; Q3 `cg_footsteps` 1 (cheat), also dmflags bit 32 [read] |
| Mod folder | Q2 `game`; Q3 `fs_game` [read] |
| Server's drop time for a silent client | QW/EZ `timeout` 65; Q2 `timeout` 125; Q3 `sv_timeout` 200 [read] |

---

## Shadows in each game

**First game, 1996 release (Q1).** Software build: no shadow setting and no shadows [read]. GL
build: `r_shadows`, default 0, not saved; any nonzero value turns it on [read]. What is seen
[read]: under every animated model (monsters, players, items with models), a flattened copy of
the model's current pose, drawn untextured in black at 50 % opacity onto a horizontal plane just
above the floor point found below the model, leaning along the model's own facing direction (it
does not follow any light). The held weapon goes through the same drawing path with no exclusion,
so it casts one too [derived]. Because the shape is a flat plane at one height, it does not bend
over steps or edges [derived].

**First game, multiplayer release (QW).** Identical GL `r_shadows`, default 0; none in software
[read].

**Modern client (EZ).** `r_shadows`, default 0, on/off [read]. The same flattened 50 % silhouette
leaning along the model's facing, scaled with the model's own scale; skipped for fully lit models,
translucent models and models flagged not to cast one [read]. Only its classic renderer draws it; in its modern renderer the setting does nothing [read].

**Quake II (Q2).** GL renderer: `gl_shadows`, default 0, saved [read]. Nonzero draws the same kind
of flattened black silhouette at 50 % opacity under each animated model, leaning along the
model's facing; translucent models and the held weapon cast none [read]. Software renderer: no
shadows [read].

**Third game (Q3).** `cg_shadows`, default 1, saved; the renderer reads the same setting [read].
Shadows are cast only by player figures [read].
- 0: none; also no wading splash marks [read].
- 1 (default): a blob: a round dark mark picture laid on the ground under the player, 24 units in
  radius, only when the ground is within 128 units below, fading as the player rises
  (fully dark on the ground, gone at 128 units); none while the invisibility powerup is held
  [read].
- 2: stencil shadow volumes from the player model's real shape onto whatever lies behind it; needs a display with at least 4 stencil bits, skipped in fog, not cast by the viewer's own
  first-person body [read].
- 3: a projected flat shadow of the player model on the plane under it (the first-game style),
  which also works for the viewer's own body [read].
Any nonzero value also lays a splash mark at the water surface around a player wading [read].

---

## The modern client's defaults against the 1996 release

From the client settings in § Each of our setting names, game by game (1996 release value, then EZ value) [read]:

| Name | 1996 release (Q1 / QW client) | EZ |
| --- | --- | --- |
| `cl_bob` | 0.02 | 0 |
| `cl_bobcycle` | 0.6 | 0 |
| `cl_bobup` | 0.5 | 0 |
| `cl_rollangle` | 2.0 | 0 |
| `v_kickpitch` | 0.6 | 0 |
| `v_kickroll` | 0.6 | 0 |
| `v_kicktime` | 0.5 | 0 |
| `crosshair` | 0 | 4 |
| `crosshaircolor` | 79 (palette index; QW only) | "255 0 0" (RGB) |
| `sensitivity` | 3 | 12 |
| `scr_conspeed` | 300 | 9999 (and the unit is rescaled) |
| `v_contentblend` | 1 (QW only; a switch) | 0.2 (a strength) |
| `rate` | 2500 (QW) | 25000 |
| `name` | "unnamed" (QW; Q1's `_cl_name` "player") | "player" |
| `topcolor`, `bottomcolor` | 0 (QW) | empty |
| `r_dynamic` | 1 | 2 in builds with the modern renderer, 1 otherwise |
| `gl_texturemode` | a command, starting at GL_LINEAR_MIPMAP_NEAREST | a setting, GL_LINEAR_MIPMAP_LINEAR |
| `gamma` | 1 | absent; EZ's is `gl_gamma` 1 |
| `r_wateralpha` | 1 | absent; EZ's is `gl_turbalpha` 1 |

Unchanged between them [read]: `cl_rollspeed` 200, `v_idlescale` 0, `fov` 90 (range differs),
`viewsize` 100, `scr_centertime` 2, `con_notifytime` 3, `gl_cshiftpercent` 100, `gl_polyblend` 1,
`gl_picmip` 0, `r_fullbright` 0, `r_drawflat` 0, `r_drawviewmodel` 1, `r_netgraph` 0, `cl_maxfps`
0 (meaning differs), `cl_nopred` 0, `cl_predict_players` 1, `cl_nolerp` 0, `cl_yawspeed` 140,
`lookspring` 0, `lookstrafe` 0, `volume` 0.7, `bgmvolume` 1, `noskins` 0, `baseskin` "base".

Not client settings but differing in EZ's built-in server, for completeness [read]: `deathmatch`
1 (QW server) to 3, `maxclients` 8 to 24, `pausable` 1 to 0, `samelevel` 0 to 1,
`sv_airaccelerate` 0.7 to 10 (unused by the player move in both), `sv_mintic` 0.03 to 0.013,
`registered` 0 to 1 (read-only). Outside our list, `gl_flashblend` changes from 1 to 0 and
`cl_forwardspeed`, `cl_backspeed`, `cl_sidespeed` from 200, 200, 350 to 400 each [read].
