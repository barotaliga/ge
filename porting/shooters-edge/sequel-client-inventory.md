# Sequel client inventory: every setting and command of the stock client, and what a modern client adds

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of the console surface of the sequel's stock client program: every setting (console variable) and every console command it registers, the commands the game library answers when a player types them, the user info it sends, the stock menus and what they set, the shape of the commands that print, and the settings and commands of Q2PRO that a player of the sequel uses daily. Written from the source by a reader session; the building session reads only this. No code, no internal identifiers.

Read from id Software's Quake II source release (GPL-2.0, github.com/id-Software/Quake-2, default branch head, commit `372afde`, committed 2012-01-31; the program calls itself version 3.19) and, for question 8 only, from Q2PRO (GPL-2.0, github.com/q2pro/q2pro, default branch head, commit `601a8df`, committed 2025-12-11, its source and its own client manual). Nothing here was measured by running either program: every fact is marked **R** (read directly) or **D** (derived by combining what was read). Where the source does not settle a point, the text says so. Shared-name meanings across the games are in `cvar-meanings.md`, the config order in `installs.md`; they are not repeated here.

Terms used throughout:
- **Flags**: **A** saved to `config.cfg` (archive), **U** user info (sent to the server), **S** server info, **L** latched (a change waits for the next map while a server runs), **N** no-set (only the command line or the program sets it). **menu** marks a setting a stock menu page changes.
- **Stock build** means the Windows program with the OpenGL renderer loaded, the reference build of this release. The software renderer, the Linux layers and the server and game halves of the same program are listed separately.
- **Forwarded** means sent to the server as a text command over the connection; the server passes words it does not know to the game library.

---

## How settings and commands behave in general (applies to every row)

- A setting's name is matched exactly, with case (`Sensitivity` is not `sensitivity`). A command's name and an alias's name are matched without case. (R)
- Typing a setting's name alone prints `"name" is "value"`. Typing it with arguments sets it to the first argument only; later words are dropped. `set` also takes a single value word. (R)
- The order a typed line is tried in: a registered command, then an alias, then a setting, then it is forwarded to the server. (R)
- A registered command with no local action (the "forward" commands of question 2) is executed as `cmd <the whole line>`, so it reaches the server whenever the client is connected or in the game. A word that matched nothing is forwarded only when the client is fully in the game, and never when it starts with `+` or `-`; otherwise the console prints `Unknown command "<word>"`. (R)
- A registration of a name that already exists keeps the first registration's default and ORs in the new flags. So for a name two parts register, the default of whichever part registers first wins. (R) Order at start-up: the shared core, then the platform, then the network, then the server half, then the client half (sound and video come before the client's own settings; the renderer loads inside video start-up). (R)
- `set <name> <value> [u|s]` creates or sets a setting; with `u` or `s` it replaces the setting's flags with user info or server info only. A setting nothing registered can be created this way and is then an ordinary unsaved setting. (R)
- A user info or server info value may not contain a backslash, a double quote or a semicolon; such a set is refused with a printed warning. Keys and values must be under 64 characters, the whole info string under 512; an empty value removes its key; characters outside printable ASCII are dropped. (R)
- A no-set setting refuses a console change with `<name> is write protected.` (R)
- A latched setting changed while a local server runs prints `<name> will be changed for next game.` and takes effect at the next map; with no server running it takes effect at once. Changing `game` while no server runs switches the game folder at once and runs that folder's `autoexec.cfg`. (R)
- Saving: on quit the client writes `config.cfg` in the current game folder: a header line, every key binding as `bind <key> "<command>"`, then every saved (A) setting as `set <name> "<value>"`. (R)
- Start-up config order: `default.cfg`, then `config.cfg`, then the command line's `+set` words, then (when the client starts) `autoexec.cfg`. With no `+` command on the command line the client runs `d1` (the demo loop) as its first command. (R)
- **Cheat-protected settings.** Every frame, when the server says it has more than one player slot, the client forces these back to their safe value: `timescale` 1, `timedemo` 0, `r_drawworld` 1, `cl_testlights` 0, `r_fullbright` 0, `r_drawflat` 0, `paused` 0, `fixedtime` 0, `sw_draworder` 0, `gl_lightmap` 0, `gl_saturatelighting` 0. In single player (one slot) they are free. (R) `r_drawflat` is never registered by either renderer (the software renderer's own name is `sw_drawflat`), so in a multiplayer game this check creates an unused `r_drawflat`, and under the GL renderer it creates an unused `sw_draworder` the same way. (D)

---

## 1. Every setting the stock client registers

**Totals.** The stock build (Windows, GL renderer loaded) registers **202** distinct setting names in its client and shared parts. The software renderer adds **15** more, the Linux layers **11** more. The server half of the same program adds **23**, the game library **23** more (all listed at the end of this answer). A few more names are created on use, not registered (listed under "anything else"). (D, from enumerating every registration in that configuration)

### Input and mouse (33)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `sensitivity` | 3 | any; menu 1 to 11 in steps of 0.5 | A, menu | Multiplies raw mouse movement before it turns the view |
| `m_pitch` | 0.022 | any; negative inverts | A, menu (invert mouse flips its sign) | Degrees of pitch per scaled mouse count |
| `m_yaw` | 0.022 | any | | Degrees of yaw per scaled mouse count |
| `m_forward` | 1 | any | | Forward move per vertical mouse count when the mouse is not looking |
| `m_side` | 1 | any | | Side move per horizontal mouse count while strafing with the mouse |
| `m_filter` | 0 | 0, 1 | | 1 averages this frame's mouse movement with the last frame's |
| `freelook` | 0 | 0, 1 | A, menu | 1 makes the mouse always look up and down (as if `+mlook` were held) |
| `lookspring` | 0 | 0, 1 | A, menu | Releasing `+mlook` with `freelook` off re-centres the pitch |
| `lookstrafe` | 0 | 0, 1 | A, menu | While `+mlook` is held, horizontal mouse strafes instead of turning |
| `in_mouse` | 1 | 0, 1 | A | Use the mouse at all |
| `in_initmouse` | 1 | 0, 1 | N | Command-line switch: 0 never opens the mouse |
| `in_joystick` | 0 | 0, 1 | A, menu (use joystick) | Use a joystick |
| `in_initjoy` | 1 | 0, 1 | N | Command-line switch: 0 never opens the joystick |
| `joy_name` | joystick | text | | The joystick's display name |
| `joy_advanced` | 0 | 0, 1 | | 1 uses the per-axis map below instead of the fixed two-axis map |
| `joy_advaxisx` | 0 | 0 to 5 | | X axis role: none, forward, look, side, turn, up |
| `joy_advaxisy` | 0 | 0 to 5 | | Y axis role, same values |
| `joy_advaxisz` | 0 | 0 to 5 | | Z axis role |
| `joy_advaxisr` | 0 | 0 to 5 | | R axis role |
| `joy_advaxisu` | 0 | 0 to 5 | | U axis role |
| `joy_advaxisv` | 0 | 0 to 5 | | V axis role |
| `joy_forwardthreshold` | 0.15 | 0 to 1 | | Dead zone of the forward axis |
| `joy_sidethreshold` | 0.15 | 0 to 1 | | Dead zone of the side axis |
| `joy_upthreshold` | 0.15 | 0 to 1 | | Dead zone of the up axis |
| `joy_pitchthreshold` | 0.15 | 0 to 1 | | Dead zone of the look axis |
| `joy_yawthreshold` | 0.15 | 0 to 1 | | Dead zone of the turn axis |
| `joy_forwardsensitivity` | -1 | any | | Scale (and sign) of the forward axis |
| `joy_sidesensitivity` | -1 | any | | Scale of the side axis |
| `joy_upsensitivity` | -1 | any | | Scale of the up axis |
| `joy_pitchsensitivity` | 1 | any | | Scale of the look axis |
| `joy_yawsensitivity` | -1 | any | | Scale of the turn axis |
| `v_centermove` | 0.15 | any | | Registered and never read (R) |
| `v_centerspeed` | 500 | any | | Registered and never read (R) |

(R for every row; the menu's range D from its slider of 2 to 22 halved.)

### Movement and the player's buttons (7)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `cl_run` | 0 | 0, 1 | A, menu (always run) | 1 inverts `+speed`: running is the default and the key walks |
| `cl_forwardspeed` | 200 | any | | Forward and back move per key, in units per second; doubled when running |
| `cl_sidespeed` | 200 | any | | Strafe move per key; doubled when running |
| `cl_upspeed` | 200 | any | | Jump or swim-up and crouch or swim-down move per key; doubled when running |
| `cl_yawspeed` | 140 | any | | Turn rate of `+left` and `+right`, degrees per second |
| `cl_pitchspeed` | 150 | any | | Look rate of `+lookup`, `+lookdown` and `+klook`, degrees per second |
| `cl_anglespeedkey` | 1.5 | any | | Turn and look rates are multiplied by this while `+speed` is held |

(R) There is no jump or crouch setting or button of its own: jump is `+moveup` and crouch is `+movedown`, the up axis the server reads as jump or duck. (R)

### View: field of view, the gun, the crosshair, the tints (9)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `fov` | 90 | game clamps to 1 to 160 | U, A | Horizontal field of view the game sets for this player (question 4) |
| `hand` | 0 | 0 right, 1 left, 2 centre | U, A, menu | Which side the gun is held on; 1 mirrors the gun, 2 hides it; also where the game spawns shots (question 4) |
| `crosshair` | 0 | 0 to 3 | A, menu (none, cross, dot, angle) | Draws picture `ch1` to `ch3` at the view's centre; out of range becomes 3 |
| `viewsize` | 100 | 40 to 100 below, steps of 10 | A, menu (screen size 30 to 120) | Size of the 3D view; under 100 shrinks it, 110 and 120 also hide parts of the status bar (see `sequel-hud.md`) |
| `cl_gun` | 1 | 0, 1 | | 0 hides the held gun |
| `cl_blend` | 1 | 0, 1 | | 0 drops the full-screen tint (damage, pickup, water) |
| `cl_stereo` | 0 | 0, 1 | | Stereo rendering (two eyes); the GL renderer turns it off when the card cannot |
| `cl_stereo_separation` | 0.4 | 0 to 1 | A | Eye separation in stereo |
| `cl_testblend` | 0 | 0, 1 | | Development: draws a fixed test tint |

(R)

### HUD and the status bar (5)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `scr_centertime` | 2.5 | seconds | | How long a centre print stays |
| `scr_showpause` | 1 | 0, 1 | | Draw the pause picture while paused |
| `scr_drawall` | 0 | 0, 1 | | Redraw the whole screen every frame (software renderer's dirty areas) |
| `scr_showturtle` | 0 | | | Registered and never read (R) |
| `scr_printspeed` | 8 | | | Registered and never read (R) |

(R) The status bar itself, the scoreboard and the inventory are drawn from the game's layout strings and have no client settings (`sequel-hud.md`).

### Console and chat (5)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `con_notifytime` | 3 | seconds | | How long a printed line stays in the four notify lines at the top |
| `scr_conspeed` | 3 | | | How fast the console slides open and shut |
| `msg` | 1 | 0 to 3 | U, A | The lowest message level the server sends this player (question 4) |
| `developer` | 0 | 0, 1 | | Print developer messages |
| `logfile` | 0 | 0, 1, 2 | | 1 writes the console to `qconsole.log` in the game folder, 2 does so and flushes every line |

(R) There is no chat setting beyond `msg`: chat lines print like any other line and play the chat sound the server's message carries. (R; the sound is the game's, see `sequel-hud.md`)

### Network, the netgraph and prediction (41)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `rate` | 25000 | bytes per second | U, A, menu (connect speed) | The most the server may send this client per second; the server clamps it to 100 to 15000 (question 4) |
| `cl_maxfps` | 90 | frames per second | | Caps the client's frame rate; the client sends one move packet per frame, so this is also its packet rate (D) |
| `cl_nodelta` | 0 | 0, 1 | | 1 asks the server for full frames, never deltas |
| `cl_predict` | 1 | 0, 1 | | Predict the player's own move between server frames |
| `cl_timeout` | 120 | seconds | | Drop the connection after this long with no packet from the server |
| `cl_shownet` | 0 | 0 to 3 | | Print received messages: 1 sizes, 2 each message's kind, 3 also entity numbers |
| `cl_showmiss` | 0 | 0, 1 | | Print prediction misses and their size |
| `showclamp` | 0 | 0, 1 | | Print when the client's time is clamped to the server frames it has |
| `netgraph` | 0 | 0, 1 | | Draw a bar per packet: its latency, with drops and suppressed packets marked |
| `timegraph` | 0 | 0, 1 | | Draw a frame-time bar graph |
| `debuggraph` | 0 | 0, 1 | | Enable the graph for development values (suppresses the net bars) |
| `graphheight` | 32 | pixels | | Height of the graph |
| `graphscale` | 1 | | | Vertical scale of the graph's values |
| `graphshift` | 0 | | | Vertical offset of the graph's values |
| `showpackets` | 0 | 0, 1 | | Print every packet sent and received with its sequences |
| `showdrop` | 0 | 0, 1 | | Print dropped and out-of-order packets |
| `qport` | random | 0 to 65535 | N | The client's port number inside the connection, so a NAT changing the UDP port does not break it |
| `net_shownet` | 0 | | | Registered on Windows and never read (R) |
| `noudp` | 0 | 0, 1 | N | Command-line switch: no IP networking |
| `noipx` | 0 | 0, 1 | N | Command-line switch: no IPX networking |
| `ip` | localhost | address | N | Local address to bind |
| `port` | 27910 | | N | The server's UDP port when no host port is given |
| `hostport` | 0 | | N | Server port override |
| `ip_hostport` | 0 | | N | Server port override for IP only |
| `clientport` | 27901 | | N | The client's own UDP port when no other is given |
| `ip_clientport` | 0 | | N | Client port override for IP only |
| `ipx_hostport` | 0 | | N | Server port override for IPX |
| `ipx_clientport` | 0 | | N | Client port override for IPX |
| `rcon_password` | empty | text | | Password `rcon` sends; the server half reads the same name as its own password |
| `rcon_address` | empty | address | | Where `rcon` goes when not connected |
| `password` | empty | text | U | Join password the server checks (question 4) |
| `spectator` | 0 | 0, 1, or a password | U | Join as a spectator (question 4) |
| `adr0` to `adr8` | empty | addresses | A, menu (address book) | Nine saved server addresses that `pingservers` also pings |

(R; `port`, `clientport` and the ip and ipx names are the Windows layer's and are registered when networking opens; Linux registers only `ip` and `port`. The connection logic itself is in `sequel-net.md`.)

### Demos and recording (1)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `timedemo` | 0 | 0, 1 | cheat-protected | Plays a demo as fast as frames can be drawn, then prints frames, seconds and frames per second when it ends |

(R) Everything else about demos is a command (question 2); playing one goes through the server half's `map` or `demomap` (`sequel-demo.md`).

### Sound (12)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `s_initsound` | 1 | 0, 1 | | 0 starts with no sound at all |
| `s_volume` | 0.7 | 0 to 1 | A, menu (effects volume, 0 to 10) | Master effects volume |
| `s_khz` | 11 | 11, 22, 44 | A, menu (sound quality) | Mixing rate in kHz |
| `s_loadas8bit` | 1 | 0, 1 | A, menu (sound quality) | Load sounds as 8-bit to save memory; the menu's "low" is 11 kHz 8-bit, "high" 22 kHz 16-bit |
| `s_mixahead` | 0.2 | seconds | A | How far ahead the mixer fills the buffer |
| `s_show` | 0 | 0, 1 | | Print the sounds playing each frame |
| `s_testsound` | 0 | 0, 1 | | Replace the mix with a test tone |
| `s_primary` | 0 | 0, 1 | A, menu (sound compatibility: max compatibility, max performance) | Windows: mix into the primary sound buffer |
| `s_wavonly` | 0 | 0, 1 | | Windows: use the plain wave device, never DirectSound |
| `cd_nocd` | 0 | 0, 1 | A, menu (CD music, inverted) | Turn off CD music |
| `cd_loopcount` | 4 | count | | Windows: plays of a track before switching to the loop track |
| `cd_looptrack` | 11 | track | | Windows: the track played after that |

(R)

### Picture: video, GL and renderer (56)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `vid_ref` | soft | soft, gl | A, menu (driver) | Which renderer to load; the menu's choices set it |
| `vid_xpos` | 3 | pixels | A | Window position across, in a window |
| `vid_ypos` | 22 | pixels | A | Window position down |
| `vid_fullscreen` | 0 | 0, 1 | A, menu | Full screen |
| `vid_gamma` | 1 | 0.5 to 1.3 from the menu | A, menu (brightness) | Picture gamma; the GL renderer applies it to textures at load (a change needs a renderer restart) |
| `win_noalttab` | 0 | 0, 1 | A | Windows: block Alt-Tab (on no menu page, R) |
| `gl_driver` | opengl32 | library name | A, menu (driver: default, 3Dfx, PowerVR) | The OpenGL library to load |
| `gl_mode` | 3 | 0 to 9 | A, menu (video mode) | Resolution: 320x240, 400x300, 512x384, 640x480, 800x600, 960x720, 1024x768, 1152x864, 1280x960, 1600x1200 |
| `sw_mode` | 0 | 0 to 9 | menu | The software renderer's mode, same list; saved only when that renderer has loaded (D) |
| `sw_stipplealpha` | 0 | 0, 1 | A, menu | Software: draw see-through surfaces as a stipple |
| `gl_picmip` | 0 | 0 to 3 | menu (texture quality, inverted) | Drops this many mip levels from every texture; not saved (D) |
| `gl_ext_palettedtexture` | 1 | 0, 1 | A, menu (8-bit textures) | Use paletted textures when the card has them |
| `gl_finish` | 0 | 0, 1 | A, menu (sync every frame) | Wait for the card to finish each frame |
| `gl_ext_multitexture` | 1 | 0, 1 | A | Use multitexture for lightmaps when available |
| `gl_ext_pointparameters` | 1 | 0, 1 | A | Draw particles as sized points when available |
| `gl_ext_compiled_vertex_array` | 1 | | A | Registered and never read (R) |
| `gl_ext_swapinterval` | 1 | | A | Registered and never read (R) |
| `gl_swapinterval` | 1 | 0, 1 | A | Wait for vertical sync |
| `gl_texturemode` | GL_LINEAR_MIPMAP_NEAREST | the GL filter names | A | Texture filtering |
| `gl_texturealphamode` | default | GL format names | A | Internal format of textures with alpha |
| `gl_texturesolidmode` | default | GL format names | A | Internal format of textures without alpha |
| `gl_modulate` | 1 | any | A | Brightens lightmaps and model light |
| `gl_shadows` | 0 | 0, 1 | A | Draw flat shadows under models (not see-through ones or the gun) |
| `gl_dynamic` | 1 | 0, 1 | | Dynamic lights on world surfaces |
| `gl_flashblend` | 0 | 0, 1 | | Draw dynamic lights as glowing blobs instead of lighting surfaces |
| `gl_polyblend` | 1 | 0, 1 | | Draw the full-screen tint |
| `gl_monolightmap` | 0 | 0 or a letter | | 0 colour lightmaps; a letter selects a one-channel lightmap mode for weak cards |
| `gl_saturatelighting` | 0 | 0, 1 | cheat-protected | Full-bright lightmaps |
| `gl_lightmap` | 0 | 0, 1 | cheat-protected | Draw lightmaps only, without textures |
| `gl_skymip` | 0 | 0, 1 | | Lower-detail sky |
| `gl_round_down` | 1 | 0, 1 | | Round texture sizes down, not up, to a power of two |
| `gl_playermip` | 0 | | | Registered and never read (R) |
| `gl_nosubimage` | 0 | | | Registered and never read (R) |
| `gl_vertex_arrays` | 0 | 0, 1 | A | Draw models through vertex arrays |
| `gl_cull` | 1 | 0, 1 | | Back-face culling |
| `gl_clear` | 0 | 0, 1 | | Clear the colour buffer every frame |
| `gl_ztrick` | 0 | 0, 1 | | Avoid depth clears by alternating depth ranges |
| `gl_showtris` | 0 | 0, 1 | | Development: draw triangle outlines |
| `gl_lockpvs` | 0 | 0, 1 | | Development: freeze the visible set |
| `gl_nobind` | 0 | 0, 1 | | Development: never change textures (speed test) |
| `gl_log` | 0 | 0, 1 | | Log every GL call to a file |
| `gl_bitdepth` | 0 | 0, 16, 32 | | Windows: colour depth of full screen; 0 keeps the desktop's |
| `gl_drawbuffer` | GL_BACK | GL_BACK, GL_FRONT | | Draw to the back or front buffer |
| `gl_allow_software` | 0 | 0, 1 | | Windows: accept an unaccelerated GL pixel format |
| `gl_3dlabs_broken` | 1 | 0, 1 | A | Work around one card family's driver bug |
| `intensity` | 2 | 1 or more | | Multiplies texture brightness at load (`sequel-light.md`) |
| `flushmap` | 0 | 0, 1 | | Reload the world model even when the same map loads again |
| `r_drawentities` | 1 | 0, 1 | | Draw entities |
| `r_drawworld` | 1 | 0, 1 | cheat-protected | Draw the world |
| `r_fullbright` | 0 | 0, 1 | cheat-protected | Draw without lighting |
| `r_lerpmodels` | 1 | 0, 1 | | Interpolate model frames |
| `r_novis` | 0 | 0, 1 | | Ignore visibility (draw everything) |
| `r_nocull` | 0 | 0, 1 | | Do not frustum-cull |
| `r_norefresh` | 0 | 0, 1 | | Draw no 3D view |
| `r_speeds` | 0 | 0, 1 | | Print per-frame drawing counts |
| `r_lightlevel` | 0 | written by the renderer | | The light at the player's position; the client sends it in every move (the game's monsters read it) |

(R; the menu ranges D from the slider limits.)

### Particles and effects (14)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `cl_particles` | 1 | 0, 1 | | Add particles to the view |
| `cl_lights` | 1 | 0, 1 | | Add dynamic lights to the view |
| `cl_entities` | 1 | 0, 1 | | Add entities to the view |
| `cl_footsteps` | 1 | 0, 1 | | Play footstep sounds |
| `gl_particle_size` | 40 | pixels | A | Particle size without point sizing |
| `gl_particle_min_size` | 2 | pixels | A | Smallest point-sized particle |
| `gl_particle_max_size` | 40 | pixels | A | Largest point-sized particle |
| `gl_particle_att_a` | 0.01 | | A | Point size falloff, constant term |
| `gl_particle_att_b` | 0.0 | | A | Point size falloff, distance term |
| `gl_particle_att_c` | 0.01 | | A | Point size falloff, distance-squared term |
| `cl_testparticles` | 0 | 0 to 1 | | Development: 4096 particles in front of the view at that opacity |
| `cl_testentities` | 0 | 0, 1 | | Development: 32 player models in front of the view |
| `cl_testlights` | 0 | 0, 1 | cheat-protected | Development: 32 coloured lights |
| `cl_stats` | 0 | 0, 1 | | Print counts of entities, lights and particles each frame |

(R)

### Skins, models and the player's look (7)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `name` | unnamed | text | U, A, menu | The player's name |
| `skin` | male/grunt | model/skin | U, A, menu (model and skin) | The player's model folder and skin |
| `gender` | male | male, female, none | U, A | Wording of death messages (question 4) |
| `gender_auto` | 1 | 0, 1 | A | Set `gender` from the skin's model folder whenever user info is sent, unless `gender` was typed since |
| `cl_noskins` | 0 | 0, 1 | | 1 draws every player as male/grunt |
| `cl_vwep` | 1 | 0, 1 | A | 0 shows only each player's default weapon model, never the one they hold |
| `cl_autoskins` | 0 | | | Registered and never read (R) |

(R) The automatic gender recognises the folders male and cyborg as male, and female and a folder spelled "crackhor" as female; the stock female cyborg folder is spelled without the c, so it is set to none. (R for the spellings, D for the consequence)

### The menu

The menu registers no setting of its own; it reads and writes the settings marked **menu** above and the server-half ones in question 6. (R)

### Anything else (12)

| Name | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `game` | empty | folder name | L, S | The mod folder; changing it with no server running switches folders at once |
| `basedir` | . | path | N | Install folder |
| `cddir` | empty | path | N | A second read-only search folder (the CD) |
| `dedicated` | 0 | | N | 1 only in the dedicated server build |
| `version` | the version, CPU, build date and platform, for example 3.19 x86 then the date then Win32 RELEASE | | S, N | The program's version |
| `paused` | 0 | 0, 1 | cheat-protected | Pause; set by `pause`, the menu and the console in single player |
| `timescale` | 1 | any | cheat-protected | Multiplies frame time |
| `fixedtime` | 0 | milliseconds | cheat-protected | Every frame takes exactly this long |
| `host_speeds` | 0 | 0, 1 | | Print time spent in server, client and drawing each frame |
| `log_stats` | 0 | 0, 1 | | Write per-frame counts and times to a stats file |
| `showtrace` | 0 | 0, 1 | | Print the number of traces and point checks per frame |
| `map_noareas` | 0 | 0, 1 | | Ignore area portals (doors stop splitting visibility) |

(R) Created on use, never registered by the client: `gamedir` (S, N, set by the program to the current mod folder), `mapname` (S, N, set by the server half to the map's name), `nextserver` (set by the server half for level changes), `gamerules` (set by the start server page to 0), plus the cheat check's `r_drawflat` and `sw_draworder` noted above. (R)

### Added by the software renderer (15)

| Name | Default | Flags | What it does |
| --- | --- | --- | --- |
| `sw_mode` | 0 | A | Video mode (saved once this renderer loads) |
| `sw_allow_modex` | 1 | A | Windows: allow mode X for the lowest mode |
| `sw_clearcolor` | 2 | | Palette index used to clear the view |
| `sw_drawflat` | 0 | | Development: flat-shaded surfaces |
| `sw_draworder` | 0 | cheat-protected | Development: show draw order |
| `sw_maxedges` | the text x, which reads as 0 | | Edge budget; 0 means the built-in size (D: the default is a literal letter, not a number) |
| `sw_maxsurfs` | 0 | | Surface budget; 0 means the built-in size |
| `sw_mipcap` | 0 | | Never use mip levels below this one |
| `sw_mipscale` | 1 | | Distance scale for choosing mip levels |
| `sw_reportedgeout` | 0 | | Print when edges run out |
| `sw_reportsurfout` | 0 | | Print when surfaces run out |
| `sw_surfcacheoverride` | 0 | | Force the surface cache size |
| `sw_waterwarp` | 1 | | Warp the view under water |
| `sw_polymodelstats` | 0 | | Print model drawing counts |
| `sw_lockpvs` | 0 | | Development: freeze the visible set |
| `r_dspeeds` | 0 | | Print detailed drawing times |

(R; 16 rows because `sw_mode` is already counted in the stock build.) Both renderers also register `hand`, `r_speeds`, `r_fullbright`, `r_drawentities`, `r_drawworld`, `r_lightlevel`, `r_lerpmodels`, `r_novis`, `vid_fullscreen`, `vid_gamma` and `sw_stipplealpha` or `vid_ref`, already listed. (R)

### Added by the Linux layers (11)

| Name | Default | Flags | What it does |
| --- | --- | --- | --- |
| `sndbits` | 16 | A | Sound sample size |
| `sndspeed` | 0 | A | Sound rate; 0 tries the usual rates |
| `sndchannels` | 2 | A | Sound channels |
| `snddevice` | /dev/dsp | A | Sound device |
| `cd_volume` | 1 | A | CD volume |
| `cd_dev` | /dev/cdrom | A | CD device |
| `nocdaudio` | 0 | N | Command-line switch: no CD |
| `nostdout` | 0 | | Do not echo the console to the terminal |
| `_windowed_mouse` | 0 | A | Grab the mouse in a window |
| `mdev` | /dev/mouse | | Console-mode mouse device |
| `mrate` | 1200 | | Console-mode mouse rate |

(R) The Linux renderers' input also asks for `m_filter`, `in_mouse`, `freelook`, `lookstrafe`, `sensitivity`, `m_pitch`, `m_yaw`, `m_forward` and `m_side`, the last with 0.8; whichever part registers first keeps its default, and the source does not settle the order on Linux. Linux's `vid_ref` default is `softx` when a display is present, else `soft`. (R)

### The server half of the same program (23 more)

| Name | Default | Flags | What it does |
| --- | --- | --- | --- |
| `deathmatch` | 0 | L | Deathmatch rules |
| `coop` | 0 | L | Co-operative rules |
| `skill` | 1 | L (the game adds it) | Difficulty 0 to 3 |
| `dmflags` | 16 | S | Deathmatch flags (the game asks for 0; the server's 16, instant powerups, wins) |
| `fraglimit` | 0 | S | Frags to end a match |
| `timelimit` | 0 | S | Minutes to end a match |
| `cheats` | 0 | S, L | Allow cheat commands in deathmatch |
| `protocol` | 34 | S, N | The protocol number |
| `maxclients` | 1 | S, L | Player slots (the game asks for 4; the server's 1 wins) |
| `hostname` | noname | S, A | Server name |
| `timeout` | 125 | | Seconds before a silent client is dropped |
| `zombietime` | 2 | | Seconds a dropped slot stays reserved |
| `sv_enforcetime` | 0 | | Check clients' move time against real time |
| `allow_download` | 0 | A, menu | Allow downloads at all (the client also reads these for its own downloads) |
| `allow_download_players` | 0 | A, menu | Player models and skins |
| `allow_download_models` | 1 | A, menu | Models |
| `allow_download_sounds` | 1 | A, menu | Sounds |
| `allow_download_maps` | 1 | A, menu | Maps |
| `sv_noreload` | 0 | | Do not reload the map when the level is re-entered |
| `sv_airaccelerate` | 0 | L | Air control for the move (`sequel-movement.md`) |
| `public` | 0 | | Announce to master servers |
| `sv_reconnect_limit` | 3 | A | Minimum seconds between reconnects from one address |
| `sv_projectiles` | 1 | | Registered when frames are first built; sends projectiles compactly |

(R) The server half also registers `rcon_password`, `showclamp`, `paused` and `timedemo`, already counted. (R)

### The game library in the same program (23 more)

| Name | Default | Flags | What it does |
| --- | --- | --- | --- |
| `gun_x` | 0 | | Development offset of the gun |
| `gun_y` | 0 | | Development offset of the gun |
| `gun_z` | 0 | | Development offset of the gun |
| `sv_rollspeed` | 200 | | Speed at which strafing rolls the view fully (`sequel-view.md`) |
| `sv_rollangle` | 2 | | Most roll in degrees |
| `sv_maxvelocity` | 2000 | | Speed clamp per axis |
| `sv_gravity` | 800 | | Gravity |
| `gamename` | baseq2 | S, L | The game library's name |
| `gamedate` | the build date | S, L | The game library's build date |
| `maxspectators` | 4 | S | Spectator slots |
| `maxentities` | 1024 | L | Entity limit |
| `spectator_password` | empty | U | Spectator password; the game flags it user info, so a listen host sends its own copy in its user info (D) |
| `filterban` | 1 | | 1 bans listed addresses, 0 allows only them |
| `g_select_empty` | 0 | A | Allow selecting a weapon with no ammo |
| `run_pitch` | 0.002 | | View pitch by speed |
| `run_roll` | 0.005 | | View roll by speed |
| `bob_up` | 0.005 | | View bob height |
| `bob_pitch` | 0.002 | | View bob pitch |
| `bob_roll` | 0.002 | | View bob roll |
| `flood_msgs` | 4 | | Chat lines allowed in the window below; 0 turns flood protection off |
| `flood_persecond` | 4 | | The window in seconds |
| `flood_waitdelay` | 10 | | Seconds a flooder is muted |
| `sv_maplist` | empty | | Rotation list of map names |

(R) The game also asks for `deathmatch`, `coop`, `skill`, `maxclients`, `dmflags`, `fraglimit`, `timelimit`, `cheats`, `password`, `dedicated` and `game`. (R)

---

## 2. Every console command the client registers

**Totals.** The stock build registers **129** command names in its client and shared parts: 110 with a local action and 19 that only forward. The server half adds **15** (16 on a dedicated server, which also has `say` and `quit` of its own). (D, from enumerating every registration)

### Movement and the player's buttons (32)

Each `+` command records which key pressed it; up to two keys may hold one button, a third prints `Three keys down for a button!`. A `-` command typed at the console (with no key) releases the button completely. The time the key was held is counted to the millisecond, so a tap shorter than a frame still moves for its share. (R)

| Name | Arguments | What it does |
| --- | --- | --- |
| `+forward` / `-forward` | key, time (added by the binding) | Move forward; with `+klook` held, looks down instead |
| `+back` / `-back` | same | Move back; with `+klook`, looks up |
| `+moveleft` / `-moveleft` | same | Strafe left |
| `+moveright` / `-moveright` | same | Strafe right |
| `+left` / `-left` | same | Turn left; with `+strafe`, strafes |
| `+right` / `-right` | same | Turn right; with `+strafe`, strafes |
| `+moveup` / `-moveup` | same | Up: jump, swim up |
| `+movedown` / `-movedown` | same | Down: crouch, swim down |
| `+lookup` / `-lookup` | same | Look up at `cl_pitchspeed` |
| `+lookdown` / `-lookdown` | same | Look down |
| `+strafe` / `-strafe` | same | Turn keys and mouse turn become strafes |
| `+speed` / `-speed` | same | Run (walk when `cl_run` is 1) |
| `+attack` / `-attack` | same | Fire; a press shorter than a frame still fires once |
| `+use` / `-use` | same | The use button bit (the stock game ignores it) |
| `+klook` / `-klook` | same | Forward and back keys look instead of move |
| `centerview` | none | Level the view |
| `impulse` | number | Sends a one-byte impulse with the next move (the stock game ignores it) |

### Input and platform (6 on Windows)

| Name | Arguments | What it does |
| --- | --- | --- |
| `+mlook` / `-mlook` | key | Mouse looks up and down while held (Windows and Linux) |
| `joy_advancedupdate` | none | Windows: apply the `joy_adv*` axis map |
| `vid_restart` | none | Reload the renderer and video mode |
| `vid_front` | none | Windows: bring the window to the front |
| `cd` | on, off, reset, remap, close, play n, loop n, stop, pause, resume, eject, info | CD music control |
| `force_centerview` | none | Linux only: level the view (not in the stock count) |

### View, gun and screen (9)

| Name | Arguments | What it does |
| --- | --- | --- |
| `sizeup` | none | `viewsize` up by 10 |
| `sizedown` | none | `viewsize` down by 10 |
| `sky` | name, [rotate degrees per second], [axis x y z] | Replace the sky box and its rotation |
| `gun_next` | none | Development: step the gun's shown frame forward and print it |
| `gun_prev` | none | Development: step it back |
| `gun_model` | [model] | Development: show another model as the gun; no argument restores |
| `viewpos` | none | Print the view's position and yaw |
| `timerefresh` | [any] | Draw 128 frames turning a full circle and print seconds and frames per second; with an argument, all in one frame |
| `loading` | none | Show the loading picture |

### Console, chat, binds and scripting (17)

| Name | Arguments | What it does |
| --- | --- | --- |
| `toggleconsole` | none | Open or close the console; in single player opening it pauses; with no connection it restarts the demo loop |
| `togglechat` | none | Switch between the console and the game |
| `messagemode` | none | Type a chat line; Enter sends it as `say "<text>"` |
| `messagemode2` | none | Same, sent as `say_team "<text>"` |
| `clear` | none | Empty the console's text |
| `condump` | name | Write the console's text to `<name>.txt` in the game folder |
| `bind` | key, [command] | Bind a key; with only a key, print its binding |
| `unbind` | key | Remove a key's binding |
| `unbindall` | none | Remove every binding |
| `bindlist` | none | Print every binding |
| `alias` | [name], [command text] | Define a command; with no arguments, list aliases; names under 32 characters |
| `exec` | file | Run a config file through the search path |
| `echo` | text | Print the text |
| `wait` | none | Run the rest of the command buffer next frame |
| `cmdlist` | none | List every command (question 7) |
| `set` | name, value, [u or s] | Set or create a setting |
| `cvarlist` | none | List every setting (question 7) |

### Network and connection (13)

| Name | Arguments | What it does |
| --- | --- | --- |
| `connect` | address | Stop any local server, disconnect, and connect (port 27910 if none) |
| `reconnect` | none | Reconnect to the last server; sent by servers at level changes |
| `disconnect` | none | Leave the server (it errors out to the console, which disconnects) |
| `pingservers` | none | Broadcast for servers on the LAN and ping `adr0` to `adr15`; answers fill the join page |
| `rcon` | command text | Send a command to a server with `rcon_password`; to `rcon_address` when not connected |
| `cmd` | text | Send the rest of the line to the server as a text command |
| `userinfo` | none | Print the user info being sent (question 7) |
| `download` | file | Ask the server for a file; refuses paths with two dots and files already present |
| `changing` | none | Sent by the server: a level change begins |
| `precache` | [spawn count] | Sent by the server: load the level and request missing files |
| `skins` | none | Reload every player's model and skin |
| `setenv` | name, [value...] | Set or print an environment variable |
| `pause` | none | Toggle pause; refused in multiplayer and with no local server |

### Demos and recording (2)

| Name | Arguments | What it does |
| --- | --- | --- |
| `record` | name | Record to `demos/<name>.dm2`; only in a level; the file starts at the next full (non-delta) frame |
| `stop` | none | Stop recording |

(R) Playing a demo: the server half's `map <name>.dm2` or `demomap <name>`. (R)

### Sound (5)

| Name | Arguments | What it does |
| --- | --- | --- |
| `play` | sound, [sound...] | Play sounds at full volume; `.wav` is added when there is no extension |
| `stopsound` | none | Stop every sound |
| `soundlist` | none | List loaded sounds (question 7) |
| `soundinfo` | none | Print the sound device's format |
| `snd_restart` | none | Restart sound and reload the level's sounds |

### Picture (4)

| Name | Arguments | What it does |
| --- | --- | --- |
| `screenshot` | none | Write `scrnshot/quake00` to `quake99` in the game folder (TGA under GL, PCX under software) |
| `imagelist` | none | List loaded textures (question 7) |
| `modellist` | none | List loaded models with their sizes and a total |
| `gl_strings` | none | GL only: print the vendor, renderer, version and extension strings |

### The menu (16)

| Name | What it opens |
| --- | --- |
| `menu_main` | The main menu |
| `menu_game` | Single player: easy, medium, hard, load, save, credits |
| `menu_loadgame` | Load game |
| `menu_savegame` | Save game |
| `menu_joinserver` | Join a network server |
| `menu_addressbook` | The address book |
| `menu_startserver` | Start a network server |
| `menu_dmoptions` | Deathmatch flags |
| `menu_playerconfig` | Player setup |
| `menu_downloadoptions` | Download options |
| `menu_credits` | Credits |
| `menu_multiplayer` | Multiplayer |
| `menu_video` | Video options |
| `menu_options` | Options |
| `menu_keys` | Customize controls |
| `menu_quit` | Quit confirmation |

(R)

### Files, memory and the rest (6)

| Name | Arguments | What it does |
| --- | --- | --- |
| `path` | none | Print the search path and links (question 7) |
| `link` | from, [to] | Redirect a path prefix to another; an empty target removes it |
| `dir` | [wildcard] | List files matching across the search folders |
| `z_stats` | none | Print memory in use |
| `error` | text | Development: raise an error with that text |
| `quit` | none | Disconnect and quit (the client's own `quit`) |

### Known to the client, only forwarded (19)

The client registers these names with no action only so that Tab completes them; typing one sends the whole line to the server (as `cmd`). (R)

`wave`, `inven`, `kill`, `use`, `drop`, `say`, `say_team`, `info`, `prog`, `give`, `god`, `notarget`, `noclip`, `invuse`, `invprev`, `invnext`, `invdrop`, `weapnext`, `weapprev`. What each does is question 3. `info` is answered by the server half (it prints the server info on the server's own console, not the player's, because the redirection to the client is turned off: D). `prog` is not answered by anything, so the stock game turns it into a chat line `prog ...` (D).

### Forwarded only because nothing local matched

The game library answers `score`, `help`, `players`, `weaplast`, `putaway`, `playerlist`, `invnextw`, `invprevw`, `invnextp`, `invprevp`, which the client does not know; they reach the server only when the client is fully in the game, and never complete on Tab. The server half answers `new`, `configstrings`, `baselines`, `begin`, `nextserver`, `disconnect`, `download` and `nextdl` from clients (the connection's own words, `sequel-net.md`). Any other word reaching the game becomes chat (question 3). (R)

### The server half's commands (15)

| Name | Arguments | What it does |
| --- | --- | --- |
| `map` | map, or a `.dm2`, `.cin` or `.pcx` | Start a map fresh (or play a demo, cinematic or picture) |
| `gamemap` | map | Change map keeping single-player state (level changes) |
| `demomap` | demo | Play a demo |
| `killserver` | none | Stop the local server |
| `save` | slot | Save a single-player game |
| `load` | slot | Load a single-player game |
| `status` | none | Print the server's players (question 7) |
| `serverinfo` | none | Print the server info (question 7) |
| `dumpuser` | user id | Print one player's user info |
| `kick` | user id or name | Kick a player |
| `heartbeat` | none | Tell master servers now |
| `setmaster` | addresses | Set master servers (dedicated only) |
| `serverrecord` | name | Record a server-side demo |
| `serverstop` | none | Stop it |
| `sv` | game command | Pass a command to the game library: `addip`, `removeip`, `listip`, `writeip`, `test` |

(R)

---

## 3. The game library's commands a player types

Every row reaches the game as a forwarded text command. (R) "Cheat" means: refused in deathmatch unless `cheats` is 1, with the line "You must run the server with '+set cheats 1' to enable this command."; always allowed in single player and co-op. (R) During an intermission every command below `help` in this table is ignored. (R) Any word the game does not know is said as chat, the word itself included. (R)

| Name | Arguments | What it does | Cheat |
| --- | --- | --- | --- |
| `say` | text | Chat to everyone as `<name>: <text>`; quotes around the whole text are stripped; cut to 150 characters | no |
| `say_team` | text | Chat to the same team as `(<name>): <text>`; when `dmflags` has no team bit it goes to everyone | no |
| `players` | none | Print each connected player's frags and name, then the count | no |
| `score` | none | Toggle the deathmatch scoreboard; does nothing outside deathmatch and co-op | no |
| `help` | none | In deathmatch, the scoreboard; otherwise toggle the help computer | no |
| `use` | item name | Use an item by name (weapons are items: `use rocket launcher`) | no |
| `drop` | item name | Drop an item | no |
| `give` | item name, or `all`, `health` [n], `weapons`, `ammo`, `armor`, `Power Shield`, or ammo name [n] | Give items | yes |
| `god` | none | Toggle no damage; prints `godmode ON` or `OFF` | yes |
| `notarget` | none | Toggle monsters ignoring the player | yes |
| `noclip` | none | Toggle flying through walls | yes |
| `inven` | none | Toggle the inventory screen | no |
| `invnext` | none | Select the next usable item (while chasing, the next player to chase) | no |
| `invprev` | none | Select the previous usable item (while chasing, the previous player) | no |
| `invnextw` | none | Next weapon in the inventory selection | no |
| `invprevw` | none | Previous weapon in the inventory selection | no |
| `invnextp` | none | Next powerup in the inventory selection | no |
| `invprevp` | none | Previous powerup in the inventory selection | no |
| `invuse` | none | Use the selected item | no |
| `invdrop` | none | Drop the selected item | no |
| `weapnext` | none | Switch to the next owned weapon, searching toward the item list's start | no |
| `weapprev` | none | Switch to the previous owned weapon, searching toward the item list's end | no |
| `weaplast` | none | Switch back to the weapon held before | no |
| `kill` | none | Suicide; ignored within 5 seconds of respawning | no |
| `putaway` | none | Close the scoreboard, help and inventory | no |
| `wave` | 0 to 4 | Gesture: 0 flip off, 1 salute, 2 taunt, 3 wave, other point; not while crouched or in a higher animation | no |
| `playerlist` | none | Print each player's time connected, ping, score and name, spectators marked | no |

(R) The direction of `weapnext` and `weapprev` is the stock item list's order: `weapnext` walks backward through it and `weapprev` forward. (R) Flood protection on chat: with `flood_msgs` lines in `flood_persecond` seconds the player is muted for `flood_waitdelay` seconds, told so, and told the seconds left on every try. (R)

---

## 4. User info the client sends by default

The client sends every setting flagged user info (U) as key and value pairs, at connect and again at the next move after any of them changes; an empty value is left out. (R) With the defaults, the keys sent are `name`, `skin`, `rate`, `msg`, `hand`, `fov`, `gender` and `spectator` (`password` only when set). The server adds `ip` (the client's address) before the game sees the string at connect. (R)

| Key | Default | Meaning | How the server or game reads it |
| --- | --- | --- | --- |
| `name` | unnamed | The player's name | Game: the name in chat, scores and death messages, copied into the player's skin configstring as `name\skin`. Server: the name with the high bit of every character cleared (R) |
| `skin` | male/grunt | Model folder and skin | Game: sent to every client as the player's look; the team in team play is the part before the slash with model teams, after it with skin teams (R) |
| `rate` | 25000 | Bytes per second the client can take | Server: clamped to 100 to 15000; a missing key means 5000 (R) |
| `msg` | 1 | Lowest message level wanted | Server: messages below it are not sent. Levels: 0 pickups, 1 deaths, 2 important, 3 chat. With the default 1, pickup messages are not sent (D) |
| `hand` | 0 | 0 right, 1 left, 2 centre | Game: shots start on that side, or in the middle for 2; with 2 (or a field of view over 91) the status bar's help icon shows the current weapon's icon instead (R) |
| `fov` | 90 | Field of view | Game: under 1 becomes 90, over 160 becomes 160; with the fixed-FOV flag in deathmatch always 90 (R) |
| `gender` | male | male, female, anything else | Game: f or F first letter words death messages as female, m or M as male, anything else as neuter (R) |
| `spectator` | 0 | Join as spectator | Game: only in deathmatch, any non-empty value other than 0; checked against `spectator_password` (empty or `none` means none needed) and `maxspectators`; refused with a reason (R) |
| `password` | empty | Join password | Game: must equal the server's `password` unless that is empty or `none` (R) |

(R) A user info string that fails validation is replaced by the game with name `badinfo` and skin male/grunt. (R)

---

## 5. Gameplay settings first

What changes what a player can DO, not how it looks. (D, from questions 1 to 3)

| Name | Why a player would miss it |
| --- | --- |
| `+moveup` and `+movedown` | Jump and crouch: the only way to do either |
| `+attack`, `weapnext`, `weapprev`, `weaplast`, `use <weapon>` | Firing and choosing weapons; `use` by name is how every weapon key is bound |
| `cl_run` and `+speed` | Running is double speed; without always-run every move key walks |
| `freelook` and `+mlook` | Aiming up and down with the mouse |
| `sensitivity`, `m_pitch`, `m_yaw` | Aim feel; a negative `m_pitch` inverts the mouse |
| `lookspring`, `lookstrafe`, `+klook`, `+strafe` | How keys and mouse share turning and strafing |
| `cl_forwardspeed`, `cl_sidespeed`, `cl_upspeed` | Key move speeds (the server caps them) |
| `cl_yawspeed`, `cl_pitchspeed`, `cl_anglespeedkey` | Keyboard turning |
| `hand` | Which side shots start from, and whether the gun shows |
| `fov` | How much the player sees (server may fix it) |
| `cl_predict` | Without it every move waits for the server |
| `cl_maxfps` | Also the move packet rate |
| `rate` | Too low and the server suppresses frames |
| `cl_noskins`, `cl_vwep` | Seeing which team or weapon an enemy has |
| `inven`, `invuse`, `invdrop`, `invnext`, `invprev`, `use`, `drop` | Items: power armour, quad, health; dropping in team games |
| `say`, `say_team`, `messagemode`, `messagemode2` | Talking |
| `kill` | Respawn when stuck |
| `score`, `help` | The scoreboard and the single-player objectives |
| `spectator`, `password` | Joining a protected or full server |
| `connect`, `reconnect`, `disconnect` | Getting into a game |
| `bind`, `alias`, `wait` | Every key and every script |

---

## 6. Daily use: the stock menus

**Main menu** (pictures, in order): game, multiplayer, options, video, quit. (R)

**Game**: easy, medium, hard (each force-sets `skill` 0, 1 or 2, sets `deathmatch`, `coop` and `gamerules` to 0, and starts a new game), a gap, load game, save game, a gap, credits. Load game shows 15 slots including the autosave slot; save game shows 14 (the autosave slot is left out). (R)

**Options**, in order:

| Row | Values | Setting |
| --- | --- | --- |
| effects volume | slider 0 to 10 | `s_volume` (tenths) |
| CD music | disabled, enabled | `cd_nocd` (inverted) |
| sound quality | low, high | low: `s_khz` 11 and `s_loadas8bit` 1; high: 22 and 0; restarts sound |
| sound compatibility | max compatibility, max performance | `s_primary` 0 or 1; restarts sound |
| mouse speed | slider 2 to 22 | `sensitivity` (half the slider) |
| always run | no, yes | `cl_run` |
| invert mouse | no, yes | the sign of `m_pitch` |
| lookspring | no, yes | `lookspring` |
| lookstrafe | no, yes | `lookstrafe` |
| free look | no, yes | `freelook` |
| crosshair | none, cross, dot, angle | `crosshair` 0 to 3 |
| use joystick | no, yes | `in_joystick` |
| customize controls | page | the keys page below |
| reset defaults | action | runs `default.cfg` |
| go to console | action | opens the console |

(R) Opening the options page clamps `cl_run`, `lookspring`, `lookstrafe`, `freelook` and `in_joystick` to 0 or 1 and `crosshair` to 0 to 3. (R)

**Customize controls**, in order (row label, command bound): attack `+attack`, next weapon `weapnext`, walk forward `+forward`, backpedal `+back`, turn left `+left`, turn right `+right`, run `+speed`, step left `+moveleft`, step right `+moveright`, sidestep `+strafe`, look up `+lookup`, look down `+lookdown`, center view `centerview`, mouse look `+mlook`, keyboard look `+klook`, up / jump `+moveup`, down / crouch `+movedown`, inventory `inven`, use item `invuse`, drop item `invdrop`, prev item `invprev`, next item `invnext`, help computer `cmd help`. Each row shows up to two keys. (R)

**Video** (two pages, software and OpenGL, switched by the driver row): driver (software, default OpenGL, 3Dfx OpenGL, PowerVR OpenGL), video mode (the ten resolutions of question 1), screen size (slider 3 to 12, `viewsize` times ten), brightness (slider 5 to 13, `vid_gamma` from 1.3 down to 0.5), fullscreen (no, yes); then the software page's stipple alpha, or the OpenGL page's texture quality (slider 0 to 3, `gl_picmip` 3 minus it), 8-bit textures, sync every frame; then reset to defaults and cancel. Leaving the page applies the changes. (R)

**Multiplayer**: join network server, start network server, player setup. (R)

**Join network server**: address book, a "connect to..." title, refresh server list (runs `pingservers`), then up to 8 servers found, each shown as the server's answer line (name right-aligned in 16 characters, map in 8, players over maximum). Duplicates are dropped. (R)

**Address book**: nine text fields, `adr0` to `adr8`. (R)

**Player setup**: name (text), model (the folders under `players` with skins), skin, handedness (right, left, center; an out-of-range `hand` is set to 0), connect speed (28.8 Modem 2500, 33.6 Modem 3200, Single ISDN 5000, Dual ISDN/Cable 10000, T1/LAN 25000, User defined leaves `rate` alone), download options. Name and skin are written when the page closes. (R)

**Download options**: allow downloading, maps, player models/skins, models, sounds; each a no/yes for the matching `allow_download` setting. (R)

**Start network server**, in order:

| Field | Values | Setting |
| --- | --- | --- |
| initial map | the list in `maps.lst` (long name, short name per line); an error when it is missing or empty | the map started |
| rules | deathmatch, cooperative (the second mission pack adds tag) | `deathmatch` and `coop` |
| time limit | number, 0 no limit | `timelimit` |
| frag limit | number, 0 no limit | `fraglimit` |
| max players | number; shows 8 when `maxclients` is 1 | `maxclients` |
| hostname | text | `hostname` |
| deathmatch flags | page | below |
| begin | action | sets the above, `gamerules` 0, then `map <map>` (co-op on the first map of a unit uses `gamemap` with that unit's start spot) |

(R)

**Deathmatch flags**, in order; the status line shows `dmflags` as a number:

| Row | Values | Flag (value) |
| --- | --- | --- |
| falling damage | no, yes | yes clears no-falling (8) |
| weapons stay | no, yes | weapons stay (4) |
| instant powerups | no, yes | instant items (16) |
| allow powerups | no, yes | yes clears no-items (2) |
| allow health | no, yes | yes clears no-health (1) |
| allow armor | no, yes | yes clears no-armour (2048) |
| spawn farthest | no, yes | spawn farthest (512) |
| same map | no, yes | same level (32) |
| force respawn | no, yes | force respawn (1024) |
| teamplay | disabled, by skin, by model | skin teams (64) or model teams (128) |
| allow exit | no, yes | allow exit (4096) |
| infinite ammo | no, yes | infinite ammo (8192) |
| fixed FOV | no, yes | fixed fov (32768) |
| quad drop | no, yes | quad drop (16384) |
| friendly fire | no, yes | yes clears no-friendly-fire (256) |
| remove mines, remove nukes, 2x/4x stacking off, remove spheres | no, yes | second mission pack only: 131072, 524288, 262144, 1048576 |

(R; the bit values are the protocol's, also in `sequel-rules.md`.)

---

## 7. Commands that print to the player

| Command | What it prints, in what shape |
| --- | --- |
| `cvarlist` | One line per setting, newest registration first (not sorted): four flag columns (`*` saved, `U` user info, `S` server info, then `-` no-set or `L` latched, blank otherwise), a space, the name, the value in quotes; then `<n> cvars` (R) |
| `cmdlist` | One command name per line, newest registration first; then `<n> commands` (R) |
| `path` | `Current search path:`, then one line per search entry, newest first: a pack shows its file and `(<n> files)`, a folder its path; a dashed line marks where the base entries begin; then a blank line, `Links:` and one `from : to` line per link (R) |
| `imagelist` | A dashed line; one line per texture: a type letter (M model skin, S sprite, W wall, P picture, blank other), the uploaded width and height, under GL `RGB` or `PAL`, and the name; then the total texels (GL says not counting mip levels) (R) |
| `soundlist` | One line per registered sound: `L` when it loops, the sample size in bits, the bytes and the name; or `placeholder` or `not loaded` for the name; then `Total resident:` and the bytes (R) |
| `serverinfo` | `Server info settings:`, then one key per line, the key padded to 20 columns, then the value (R) |
| `userinfo` | `User info settings:`, then the same two-column shape (R) |
| `pingservers` | `pinging broadcast...`, then `pinging <address>...` per address book entry (or `Bad address: ...`); each answer prints as `<address>: info` and the server's line (name in 16, map in 8, players/max), or the server's name and `wrong version` (R) |
| `players` | Forwarded; the game prints each connected player's frags (3 wide) and name, one per line, in ascending order of frags (lowest first: D from the sort), `...` when the list would pass about 1180 characters, then a blank line and `<n> players` (R) |
| `playerlist` | Forwarded; one line per player: minutes:seconds connected, ping, score, name, `(spectator)` when one; `And more...` past about 1350 characters (R) |
| `status` | Server half: `map : <name>`, a header and dashes, then per slot its number, frags, ping (or CNCT connecting, ZMBI dropped), name padded to 16, milliseconds since its last message, address padded to 22, its port in the connection (R) |
| `bindlist` | One `key "command"` line per bound key (R) |
| `alias` alone | `Current alias commands:` then `name : text` per alias (R) |

---

## 8. Q2PRO's additions a player of the sequel uses daily

Defaults are read from the source; where the project's own manual gives another default it is noted. (R for every row unless marked.)

| Name | Default | Effect |
| --- | --- | --- |
| **Chat** | | |
| `cl_chat_notify` | 1 | Show chat lines in the notify area at the top |
| `cl_chat_sound` | 1 | Sound per chat line: 0 none, 1 the normal talk sound, 2 an alternative one |
| `cl_chat_filter` | 0 | 1 strips unprintable characters from incoming chat (hidden-name tricks) |
| `scr_chathud` | 0 | Draw the last chat lines on screen: 1 normal colour, 2 alternative colour |
| `scr_chathud_lines` | 4 | How many chat lines (at most 32) |
| `scr_chathud_time` | 0 | Seconds each line stays; 0 never fades |
| `scr_chathud_x`, `scr_chathud_y` | 8, -64 | Placement in pixels from an edge; negative counts from the right or bottom |
| `con_auto_chat` | 0 | A console line without a leading slash: 0 a command, 1 chat, 2 team chat |
| `ignoretext`, `unignoretext` | | Add or remove wildcard filters that silently drop matching chat; `all` removes every one |
| `ignorenick`, `unignorenick` | | Same by exact player name, with name completion |
| `trigger`, `untrigger` | | Run a command when a non-chat server message matches a wildcard |
| `followip` | | Connect to the address most recently seen in chat (or the n-th back) |
| **HUD and screen** | | |
| `scr_draw2d` | 2 | 0 nothing 2D, 1 everything but the status bar, 2 everything |
| `scr_scale` | 0 | HUD scale; 0 picks one by resolution, 1 none |
| `scr_alpha` | 1 | HUD opacity |
| `scr_font` | conchars | HUD text font |
| `scr_showturtle` | 1 | Show network trouble (drops, uncompressed frames, suppression) at the lower left |
| `scr_lag_draw` | 0 | Small ping graph: 1 see-through, 2 on grey; placed by `scr_lag_x`, `scr_lag_y` (-1, -1), scaled by `scr_lag_min` 0 and `scr_lag_max` 200 |
| `ch_health` | 0 | Colour the crosshair by health |
| `ch_red`, `ch_green`, `ch_blue` | 1 | Crosshair colour when `ch_health` is off |
| `ch_alpha`, `ch_scale` | 1, 1 | Crosshair opacity and size |
| `ch_x`, `ch_y` | 0, 0 | Crosshair offset from the centre in pixels |
| `cl_hit_markers` | 2 | 0 off, 1 a hit marker, 2 a marker and a sound (servers that send hits) |
| `scr_hit_marker_time` | 500 | Milliseconds the marker shows |
| `draw`, `undraw` | | Put any setting's or macro's value on screen at x, y (negative from the far edge), for example frame rate or a clock |
| `cl_gun` | 1 | 0 hidden, 1 by `hand`, 2 always right, 3 always left |
| `cl_gunfov`, `cl_gunalpha` | 90, 1 | The gun's own field of view and opacity |
| `cl_adjustfov` | 1 | Widen the field of view for screens wider than 4 by 3 |
| **Scoreboard** | | The scoreboard is still the game's layout; Q2PRO adds no scoreboard setting of its own (R: none found) |
| **Mouse** | | |
| `m_autosens` | 0 | Scale mouse speed with the field of view; 90 to 179 names the field it is tuned at, other non-zero values mean 90 |
| `m_accel` | 0 | Mouse acceleration factor |
| `m_filter` | 0 | Average this and the last mouse sample |
| `in_grab` | 1 | Mouse grab in a window: 0 never, 1 in game, 2 also released while watching demos or chasing |
| `freelook` | 1 | Mouse look on by default (stock 0) |
| `cl_run` | 1 | Always run by default (stock 0) |
| **Demos** | | |
| `demo` | | Play `demos/<name>`; the extension may be left out; file names complete on Tab; `disconnect` stops |
| `seek` | | Jump in a playing demo: absolute, `+` forward or `-` back by a time (frames, seconds, minutes:seconds, with .frames) or a percentage of the file |
| `record` | | Record `demos/<name>.dm2`; options compress with gzip, or pick the standard 1390-byte or extended 4086-byte packet limit; prints statistics when already recording |
| `stop`, `suspend`, `resume` | | Stop with statistics; pause (or toggle, per `cl_demosuspendtoggle` 1) and resume recording |
| `cl_demosnaps` | 10 | Seconds between in-memory snapshots that make backward seeks possible; 0 none |
| `cl_demomsglen` | 1390 (manual) | Default demo packet limit |
| `cl_demowait` | 0 | Pause on the demo's last frame instead of ending |
| `scr_demobar` | 1 | Progress bar during playback: 1 with percentage, 2 also the time |
| `cl_autopause` | 1 | Pause single player and demos while the console or menu is open |
| **Auto-recording** | | No client setting records automatically (R: none found). The usual way is `cl_beginmapcmd` (a command run on entering each map, empty by default) with `record`; `cl_changemapcmd` and `cl_disconnectcmd` run at map change and disconnect. The `uf` user info flags ask the mod: 1 screenshot at match end, 2 record at match start, 8 mute player chat, 16 mute observer chat, 32 mute other messages, 4 and 64 choose whose field of view wins while chasing |
| **Server browser** | | The menu's server pages, opened from its multiplayer page; it browses a web list, the address book (`favorites`), a `servers.lst` file and the LAN |
| `ui_sortservers` | 0 | Sort: 1 name, 2 mod, 3 map, 4 players, 5 ping; negative descending |
| `ui_colorservers` | 0 | Green for low ping, grey for passworded and anticheat servers |
| `ui_pingrate` | 0 | Pings per second; 0 derives it from `rate` |
| `serverstatus` | | Print a server's info and players sorted by frags (the current server when no address) |
| **Locations and macros** | | |
| `loc_dist` | 500 | Farthest a location may be from the point, in units |
| `loc_trace` | 0 | 1 requires the location to be in sight |
| `loc_draw` | 0 | Show location points |
| `loc` | | add, del, set, list, save: edit `locs/<map>.loc` in game |
| macros | | `$loc_here`, `$loc_there` (location stood at, looked at), `$cl_health`, `$cl_armor`, `$cl_ammo`, `$cl_weaponmodel`, `$cl_timer`, `$cl_ping`, `$cl_fps`, `$cl_ups` and more expand in typed and bound text; `macrolist` lists them |
| **Console and completion** | | |
| `com_completion_mode` | 1 | How Tab lists several matches: 0 one per line, 1 columns, 2 columns grouped by commands, aliases and settings |
| `com_completion_treshold` | 50 | More matches than this ask for a second Tab before listing |
| Tab | | Completes the common prefix and lists matches; commands complete their arguments where they offer it (demo files, nicknames, config files) |
| `con_timestamps` | 0 | Time stamps on console lines (format `%H:%M:%S `) |
| `toggle`, `inc`, `dec`, `reset`, `resetall` | | Flip a setting between 0 and 1 or through listed values, add or subtract, restore defaults |
| `seta`, `setu`, `sets` | | Set and mark saved, user info or server info |
| `cvarlist` | | Filters by flag (saved, cheat, latched, modified, user info, server info and others) and by wildcard |
| **Network** | | |
| `rate` | 15000 | (stock 25000) |
| `cl_maxfps` | 62 | Move rate; the manual says 60 (R both; source wins) |
| `r_maxfps` | 0 | Drawing rate when `cl_async` is 1 (0 unlimited) |
| `cl_async` | 1 | Draw and move at separate rates |
| `cl_maxpackets` | 30 | Packets per second on Q2PRO's own protocol |
| `cl_protocol` | 0 | 0 the best the server offers, 34 stock, 35 and 36 the enhanced ones |
| `cl_noskins` | 0 | 1 only male/grunt, 2 only male/grunt and female/athena |
| `cl_ignore_stufftext` | 0 | Filter commands a server pushes into the console by a whitelist file |

---

## Counts

Settings of the stock build by area (question 1): input and mouse 33, movement 7, view 9, HUD 5, console and chat 5, network 41, demos 1, sound 12, picture 56, particles and effects 14, skins and look 7, menu 0, anything else 12: **202**. The software renderer adds 15, the Linux layers 11, the server half 23 and the game library 23, so a Windows listen program with both renderers loaded holds **263** registered names (202 + 15 + 23 + 23), plus the names created on use. (D)

Commands of the stock build by area (question 2): buttons 32, input and platform 6 (Windows; Linux has `force_centerview` and no `joy_advancedupdate` or `vid_front`), view and screen 9, console and scripting 17, network 13, demos 2, sound 5, picture 4, menu 16, files and the rest 6, forward-only 19: **129**. The server half adds 15, so a Windows listen program holds **144** command names. The software renderer registers the same names as the GL one less `gl_strings`. The game library answers 27 command words, the server half 9 more from clients, and the game 5 through `sv`. (R, D for the sums)
