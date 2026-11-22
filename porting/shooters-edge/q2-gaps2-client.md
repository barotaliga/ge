# Quake II client: the second gap list (working note)

WORKING AREA: deleted when porting ends. Clean-room facts in our words, with no code, from a reader
of the Action Quake community's fork of the modern Quake II client (github.com/actionquake/q2pro,
GPL-2.0, default branch, commit `6400f7e`, 2026-11-16: the client reference the project owner
chose, `q2-client-fork-delta.md`), compared with our tree on 2026-11-21. A building session reads
this note, never the source. It is Part C (the client) of the fresh Quake II gap analysis; Parts A
(the rules) and B (the server) are their readers' notes. Work through the numbered gaps below until
the list is empty, marking each landed row here as it lands.

Marks: [R] read in the source, [M] measured, [D] derived. "Not found" about ours means it was
searched for in our tree and not found. Ranks: **1** noticed at once in normal play, **2** noticed
sometimes, **3** rare or cosmetic. No gap in Part C reached rank 1.

The order to take them in: the rank 2 rows first (C3 the server's field of view, C4 the HUD scale,
C5 to C7 the menu pages and the keys page, C28 the underwater muffle, C29 the music on a joined
server and in recordings, C30 the user info while joined, C36 the downloads' missing kinds), then
C1 (the area list in the window's draw, the open item of M18v's area portals), then the rest.

---

## Question C1: drawing

### C1's own question: does a shut door hide the world and the things behind it?

Yes, in two separate places, and both depend on the same per-frame list of visible areas. [R]

**What the server sends.** Each frame it builds for a client, the server finds the leaf under that
client's view point (the origin plus the view offset; a chasing spectator's is the chased player's),
takes that leaf's area, and writes into the frame a bit list of every area in the same flood as it,
that is every area reachable from it through area portals that are open now (the game library opens
and shuts each portal as its door opens and shuts, `q2-areas.md`). The list is a length byte then
(areas + 7) / 8 bytes, one bit per area. [R]
- When the view point's area is 0 (the eye in a solid, or outside the map) or the server's debug
  setting `map_noareas` is 1, every bit of the list is set: everything counts as visible. [R]
- When the server has no collision model for the map (no area data at all) the list is empty; a
  client on the stock protocol 34 is then sent a single byte of 255 instead, a client on the modern
  client's own protocols gets the empty list. [R]
- Area 0 is never part of a flood, so its bit is clear whenever the eye stands in a real area. [D,
  from the flood rule in `q2-areas.md`]

**What the server leaves out of the frame (entities).** Before the PVS test, an entity other than
the client's own is dropped from the frame unless the client's area is joined to the entity's first
area, or, failing that, to its second area (a door's brush can straddle two areas and keep both).
The test is skipped when the server's `sv_novis` is 1, and on the extended protocols for an entity
the game marks as never culled. So a monster, a player, an item or a rocket in the PVS but behind a
shut door is not in the frame at all, and the client cannot draw it whatever it does. [R] (How our
own server cuts what it sends is Part B's.)

**What the client does with the list.** It keeps the list with the frame (a client recording
carries it in every frame too, so a demo hides the same areas) and hands it to the renderer with the
view. An empty list means "no area test this frame". [R]

**What the renderer hides.** The world is walked from the root of its tree, front to back. A leaf the
walk reaches (its cluster in the PVS of the eye, see below, and its box in the view's four frustum
planes) is skipped when it is solid, and also when its area's bit is clear in the frame's list. A
skipped leaf marks none of its faces for this frame. A face is drawn only when some leaf drawn this
frame marked it, so a face that only hidden leaves own is not drawn: walls, floors, translucent
world faces (glass, water surfaces) and sky faces alike. A hidden leaf's sky faces also add nothing
to the sky box's clipping, so the sky seen only through a hidden area is not drawn either. [R]
- Not hidden by the area list: brush entities (doors, lifts, trains; they are entities and are
  drawn whenever the server sent them), models, sprites, beams, particles and dynamic lights. A
  dynamic light still lights the faces it reaches; it only lights faces that are drawn. [R]
- The list is the newest server frame's. The predicted eye may already be a step into the next
  area while the list still describes the server's view point (at most one 100 ms server frame
  behind on a stock server). [D]
- `gl_novis 1` turns off the PVS only; the area test still applies. `gl_lockpvs 1` (cheat) keeps
  the last PVS; the area test still applies to it. [R]
- What a player sees: behind an opaque shut door nothing changes, the door hides the far side
  anyway. Behind a door brush that is see-through (a door whose faces are drawn translucent, or a
  window inside a door that holds an area portal) the far rooms are simply not drawn, and the
  screen there shows what the frame buffer held: the client does not clear the screen by default
  (`gl_clear 0`), so it is the sky box or the last frame's leftovers. [D]

**The PVS the renderer uses (the part before the area test).** The client finds the eye's leaf, and
a second point 16 units below the eye (or 16 units above when the eye's leaf has contents, such as
water); when the second point's leaf is not solid and its cluster differs, the two clusters' PVS are
joined. This keeps the far side of a water surface drawn when the eye is near it. The marks are
recomputed only when either cluster changes. A cluster of -1 (no visibility data, the eye outside)
or `gl_novis 1` draws every leaf. [R]

**Ours.** Our window draws a version 38 map by the eye's PVS on the CPU, then the frustum and a depth
pyramid (occlusion) in two phases on the GPU; it does not read the frame's area list, so it draws the
areas behind a shut door and lets the depth test hide them (`M18v.md`, 2026-11-21: "Open: the client
drawing only the joined areas"). The entities the server sends are the server's business (Part B).
Gaps C1 and C2 below.

### Gaps in drawing

**C1. Rank 3, world: the area list does not cut what the window draws.** As above: the original hides
every leaf whose area bit is clear in the frame's list (the joined server's, a recording's, our own
server's). Ours draws them and relies on occlusion. Visible only behind a see-through door that holds
an area portal, and in performance. To be the same: hide a leaf whose area bit is clear, from the
frame's list on a joined server and in a recording, and from our own flood (the server's, landed) in
the window's own game; an empty list hides nothing; area 0 shows everything. [R for the original]

**C2. Rank 3, world: the PVS from one point, not two.** The original joins the PVS of the eye's
cluster and of a point 16 units below it (above it when the eye is in water or another content
leaf). Ours takes the eye's leaf alone (`potentially_visible` of one leaf). Where a water surface
splits two clusters, ours can cull faces the original draws when the eye is within 16 units of the
surface. Check with a lab view at a pool's surface. [R original, D ours]

**C3. Rank 2, view: the field of view the server sends is not drawn.** The original draws the field
of view of the player state the server sends each frame (which the game sets from the player's `fov`
user info, 1 to 160, or 90 under the fixed-FOV deathmatch flag 32768, or whatever a mod's zoom sets),
interpolated between the last two frames. It is horizontal at 4:3; with `cl_adjustfov 1` (default)
the vertical angle of that 4:3 view is kept on a wider screen (hor+), with 0 the horizontal one is
kept. [R] In demo playback the player's own `fov` (clamped 1 to 160, under 1 meaning 90) replaces a
recorded field of 90 or more, so a recorded zoom (under 90) still shows; the user info flag `uf` 4
always uses the player's own, `uf` 32 always the recorded one. [R] Ours: our window draws its own
`cl_fov` (with `fov` a synonym); the player state's field is read off the wire and from recordings
but was not found to drive the view, so a mod's zoom, a server's fixed field of view and a recorded
zoom are not shown. [D ours] **Done (M18k4 round 34)**: the field of view sent is drawn (joined, recorded with `uf` 4 and 32, hosted with our `cl_fov` sent as `fov`), between two frames, `cl_adjustfov` (`client/q2_view.cpp`). The user info is still sent only at the connect (C30).

**C4. Rank 2, HUD: the scale of the status bar and every 2D element.** The original draws all 2D
(status bar, layouts, inventory, centre print, chat lines, crosshair, lag graph) on a virtual screen
of the window's size divided by a whole scale. `scr_scale` 0 (default) picks the scale by the
window's height when it is wider than tall: 4 from 2160 pixels, 2 from 1080, else 1 (by its width,
3840 and 1920, when it is taller than wide), and never less than the desktop's own DPI scale; any
other value is used as given, held within 1 to 10. The console has its own `con_scale`, the menus
`ui_scale`, both with the same 0 meaning auto. [R] Ours: the Quake II bar's scale is the window's
height divided by 400, at least 1 (`q2_bar_scale`), with no setting: at 1440 pixels ours is 3 and the
original 2, at 2160 ours 5 and the original 4, at 800 to 1079 ours 2 and the original 1. [R both] **Done (M18k4 round 34)**: `scr_scale` and the original's choice by the window for the bar, layouts, inventory, crosshair, centre print and lag graph; the desktop's DPI floor is not read; the console's `con_scale` landed in round 33, `ui_scale` not.

**C5. Rank 2, menus: Quake II's own pages and rows.** The original's menu is a data file in the base
game folder; its pages and rows, as a player sees them: [R]
- Main: the game's plaque and logo, then five pictures: game, multiplayer, options, video, quit.
- Game (the single-player page): easy, medium, hard, nightmare (each sets `skill` 0 to 3, `deathmatch`
  and `coop` 0, `maxclients` 1, then starts a new game), a gap, "load game...", "save game...".
- Load game: 16 slots, the autosave slot first, then a gap, then 15 slots. Save game: the 15 slots
  without the autosave.
- Multiplayer: "browse q2servers.com", "browse address book" (the address book, a `servers.lst` file
  and the LAN), "browse demos", "start server".
- Start server: "initial map" (every map file found), "rules" (deathmatch, cooperative), "time limit"
  (status line "0 - no time limit"), "frag limit" ("0 - no frag limit"), "max players", "host name",
  "deathmatch flags...", "begin game!" (deathmatch 1 unless co-op was chosen, then the map, forced).
- Deathmatch flags: falling damage, weapons stay, instant powerups, allow powerups, allow health,
  allow armor, spawn farthest, same map, force respawn, infinite ammo, fixed FOV, quad drop (12
  rows; the stock client's teamplay, allow exit and friendly fire rows are not on this page).
- Options: player setup, input setup, key bindings, weapon bindings, video setup, sound setup,
  effects setup, screen setup, download options, address book.
- Player setup: name, model, skin, handedness (right, left, center), with the chosen figure drawn
  holding the railgun's world model.
- Input setup: mouse sens (a number field), auto sens, mouse filter, free look, always run.
- Key bindings: attack, next weapon, previous weapon, walk forward, backpedal, run, step left, step
  right, up / jump, down / crouch, inventory, use item, drop item, prev item, next item, help
  computer, pause game, scoreboard, chat, then "legacy keys...". Legacy keys: turn left, turn right,
  sidestep, look up, look down, center view, mouse look, keyboard look. Weapon bindings: blaster,
  shotgun, super shotgun, machinegun, chaingun, grenade launcher, rocket launcher, hyperblaster,
  railgun, bfg10k, grenades (each bound to `use <the item's name>`).
- Video setup: video mode (windowed or a listed mode), fov scaling (vert-, hor+), vertical sync,
  anti-aliasing (no, 2x, 4x, 8x MSAA), texture gamma (1.3 down to 0.3), hardware gamma, texture
  quality, texture filter (nearest, linear, bilinear, trilinear), anisotropic filter (no to 16x),
  texture saturation, texture intensity (1x, 2x, 3x), lightmap saturation, lightmap brightness (0 to
  0.3), rendering backend (legacy, shaders).
- Sound setup: sound engine (no sound, software, OpenAL), sound latency (0.05 to 0.2 s), effects
  volume, music volume, enable music, shuffle tracks, underwater effect, ambient sounds (no, yes,
  only player's own), chat beep (disabled, default, alternative).
- Effects setup: dynamic lighting (no, yes, only switchable), lightstyles, muzzleflash, entity
  cel-shading (no, 1x, 2x, 3x), entity glowing, ground shadows (no, yes, fade), screen blending,
  screen warping, grenade explosions, rocket explosions, "railgun trail setup...".
- Railgun trail setup: rail trail type (default, core only, core and spiral), duration (0.1 to 3 s),
  core width (1 to 6), spiral radius (1 to 6), core colour and spiral colour (black, red, green,
  yellow, blue, cyan, magenta, white).
- Screen setup: screen size (40 to 100 by 10), ping graph, demo bar (no, yes, verbose), HUD opacity,
  console opacity, HUD scale, console scale, menu scale (auto, 1x, 2x, 4x), lag graph scale, then
  "crosshair setup...". Crosshair setup: crosshair type (none, cross, dot, angle), scale (0.5x, 1x,
  2x), color by health, red, green, blue and alpha channels.
- Download options: allow downloading, maps, player models/skins, models, sounds, textures, pics,
  then HTTP downloads.
- Address book: 16 text fields, `adr0` to `adr15`.
- The look: a black background, the `conchars` letters, the `ch1` picture as the cursor, rows in a
  light blue, disabled rows grey.
- An edge in the data: the sound page's "only player's own" writes `s_ambient 2`, which keeps only
  the loops of entities that have a model; "only the player's own" is value 3. [D]

Ours: Quake II runs on the classic menu's pages with Quake II's plates and banners (`q2_menu`):
SINGLE PLAYER has one NEW GAME (no difficulty rows: skill is a NEW GAME row of the multiplayer
host page, "how hard the monsters fight in a cooperative game"), LOAD and SAVE; SETUP has NAME only
on Quake II (its shirt and pants rows need Quake's colours), no model, skin or handedness; there are
no download, address book, crosshair, effects, rail trail, sound or input pages of Quake II's; our
own pages (VIDEO, HELD WEAPON, CLASSIC ENHANCED and its five) stand where Quake II's are. [R ours]

**C6. Rank 2, menus: ESCAPE in a game.** The original opens the "game" page while a game is running
(when one is defined): a compact list over the world tinted blue at about one eighth opacity:
browse q2servers.com, browse address book, browse demos, single player, options, disconnect, quit.
Disconnected, ESCAPE opens the main page. [R] Ours opens the main menu in both cases (not found
otherwise). [D ours]

**C7. Rank 2, input and menus: the key bindings page's rows are Quake's.** Ours lists ATTACK, MOVE
FORWARD, MOVE BACK, STEP LEFT, STEP RIGHT, JUMP (`+jump`), WALK / RUN, SIDESTEP, TURN LEFT, TURN
RIGHT, NEXT WEAPON (`impulse 10`), SWIM DOWN (`+movedown`), SWIM UP (`+moveup`), the chat and voice
rows, in Quake II too. Quake II's jump and crouch are `+moveup` and `+movedown`, its next weapon
`weapnext`, and its page adds previous weapon, inventory, use item, drop item, prev and next item,
help computer, pause, scoreboard, the legacy page and the weapon page (C5's lists). [R both]

**C8. Rank 3, view: the screen size.** The original's `viewsize` (saved, default 100) shrinks the 3D
view: held to 40 to 100, the view is that percentage of the screen's width and height, centred, and
the border is tiled with the pack's `backtile` picture; the status bar and the rest of the 2D stay on
the whole screen. `sizeup` and `sizedown` add and take 10 (the stock config binds them to `=` and
`-`). [R] Ours: `viewsize` is Quake's meaning (100 to 120, how much of the status bars is drawn), the
same name with another meaning, and `sizeup`/`sizedown` are not found. Quake II's needs its own
meaning in its layer (`own`). [R both]

**C9. Rank 3, HUD: the centre print.** The original's (one message at a time on the stock protocol,
a queue of 4 on the extended ones): drawn centred, its block of lines centred on a quarter of the
screen's height, no cut at 40 letters, shown for `scr_centertime` (2.5 s; any positive value held to
1 to 30 s; 0 never shows one) and faded out over a further 300 ms; the same text arriving again only
restarts its time; every centre print is also printed in the console and clears the notify lines.
[R] Ours follows the stock client (each line cut at 40 letters, the first at 35% of the height, or at
48 for more than 4 lines; `q2-hud.md`), with no fade. The two references differ here; the team picks
which to follow. [R both]

**C10. Rank 3, HUD: the chat lines on screen.** `scr_chathud` (0 off; 1 the last chat lines on the
screen in the plain colour; 2 in the alternate colour), `scr_chathud_lines` (4, at most 32),
`scr_chathud_time` (0 never fades; else seconds, each line fading over its last second),
`scr_chathud_x` 8 and `scr_chathud_y` -64 (pixels of the scaled screen; a negative x counts from the
right edge and right-aligns, a negative y from the bottom, the newest line lowest), and the command
`clearchathud`. Spectator chat from a multi-view server's relay is shown but not sounded. [R] Ours:
the notify lines (`con_notify`) only; the names are not found. [D ours] **Done (M18k4 round 32)**:
`scr_chathud`, `_lines`, `_time`, `clearchathud`; x and y not taken (the layout places it).

**C11. Rank 3, HUD: the lag graph and the lost-connection picture.** `scr_lag_draw` (0 off, 1 the
graph alone, 2 on a dark box) draws, at `scr_lag_x` and `scr_lag_y` (both -1: the lower right
corner; negative counts from the far edge), a 48 by 48 graph of the last 48 round trips, one column a
received packet, its height the round trip between `scr_lag_min` (0) and `scr_lag_max` (200)
milliseconds; a packet lost is a column in the "critical" colour, a frame the server held back for
the rate in the "warning" colour. `scr_lag_draw_scale` (1, the fork's) draws it larger. Whenever 128
or more moves are unanswered (about 2 s at 62 moves a second), the pack's `net` picture is drawn in
the graph's box, blinking: shown for 768 of every 1024 ms. [R] Ours: not found (no `net` picture, no
lag graph for Quake II). [D ours] **Done (M18k4 round 34)**: `scr_lag_*` (`engine/hud/net_graph.cpp`'s `draw_lag_graph`, `client/q2_bar.cpp`) and the blinking `net` picture; a frame the server held back for the rate is not told apart from one that came.

**C12. Rank 3, HUD: the network trouble words ("turtle").** `scr_showturtle` (1) writes, in the
alternate colour, from 11 lines above the bottom at the left edge, one word a line for each trouble
of the current frame: CLIENTPRED, BADFRAME, OLDFRAME, OLDENT, NODELTA; at 2 also SUPPRESSED,
CLIENTDROP, SERVERDROP. [R] Ours: not found. [D ours]

**C13. Rank 3, HUD: the graphs.** `netgraph` (0; 1 a bar per packet, its height the round trip in 30
ms steps up to 30, lost packets and suppressed frames as full bars in their own colours; 2 the
packet's size in 40-byte steps, coloured by size: under 200, 500, 800, 1200 bytes and more),
`timegraph`, `debuggraph`, `graphheight` (32), `graphscale` (1), `graphshift` (0): drawn along the
bottom of the scaled screen, one column a sample, newest at the right. [R] Ours: `r_netgraph` (the
first game's), no Quake II spellings. [R ours]

**C14. Rank 3, HUD: `draw` and `undraw`.** `draw <name> <x> <y> [colour]` puts a setting's value, or a
macro's (C29), on the screen every frame at x, y of the scaled screen (a negative x counts from the
right and right-aligns, a negative y from the bottom); the colour a name, `#rrggbb`, `alt` or `none`; `draw` alone lists them; drawing a name again moves it. `undraw <name>` or `undraw all` removes; "Draw string 'x' not found." and "Deleted all draw strings." are its answers. Players use it for a
clock, a frame rate, the speed (`draw cl_ups -60 -40`). [R] Ours: not found. [D ours]

**C15. Rank 3, HUD: hiding the HUD, its opacity, and the scoreboard of a demo.** `scr_draw2d` (2):
0 draws no 2D at all (for screenshots), 1 all but the status bar, 2 everything, 3 also leaves out the
layouts unless F1 is held. `scr_alpha` (1): the opacity of every 2D element but the crosshair. While
a demo plays, holding F1 shows the recorded layout (the scoreboard) whether or not the recording had
it up; holding SHIFT turns the view by the player's own mouse instead of the recorded angles. [R]
Ours: not found by these names; our HUD's own switches differ. [D ours] **Done (M18k4 round 34)**: `scr_draw2d` and `scr_alpha` over Quake II's bar, layouts, inventory, centre print and crosshair; F1 held at `scr_draw2d 3` and in a demo, and SHIFT's own look in a demo, are open.

**C16. Rank 3, effects and entities: the client's switches.** Each of these is in the original and
not found in ours by name (ours has neighbours by the first game's names: `cl_gibfilter`,
`cl_muzzleflash`, `r_rocketlight`, `r_explosionlight`, ...; a Quake II spelling would be a synonym or
an `own` row): [R]
- `cl_gibs` 1: 0 draws no gib and also tells the server not to send them (a client setting the
  server honours on the modern protocols).
- `cl_noglow` 0: 1 stops items' light pulsing.
- `cl_muzzleflashes` 1: 0 no muzzle flash lights or models.
- `cl_disable_particles` 0, a bit sum: 1 grenade explosion particles, 2 grenade trails, 4 rocket
  explosion particles, 8 rocket trails, 16 blood.
- `cl_disable_explosions` 0, a bit sum: 1 grenade explosions, 2 rocket explosions (the model).
- `cl_dlight_hacks` 0, a bit sum: 1 a rocket's light in another colour, 2 smaller explosion lights,
  4 no muzzle flash light.
- `cl_rollhack` 1: other players' models lean the opposite way to their roll, as the original GL
  renderer drew them (its sign mistake kept on purpose).
- `cl_kickangles` 1 (cheat): 0 drops the view kick the server sends.
- `cl_gunalpha` 1: the held weapon's opacity.
- `cl_entities`, `cl_particles`, `cl_lights`, `cl_blend` (all 1): 0 adds none of each to the view.
- `cl_flares` 1 (extended protocol only): 0 hides the flares the game sends.
- `cl_footsteps` 1: 0 no footsteps, also telling the server not to send them; 2 only the classic
  four steps and the one classic landing (C34).
Ours: the shells, the glow pulse, gibs and trails are landed (`q2-render-fx.md`,
`q2-client-effects.md`); the switches are not. [R ours] **Done (M18k4 round 34)**: `cl_kickangles` (a cheat), on a joined server, in a recording and hosted (`client/q2_view.cpp`). **Done (M18k4 round 35)**: `cl_gibs`, `cl_noglow`, `cl_entities`, `cl_particles`, `cl_lights`, `cl_blend`. **Done (M18k4 round 36)**: `cl_disable_particles`, `cl_disable_explosions`, `cl_muzzleflashes` (others' guns), `cl_dlight_hacks` (2 and 4).

**C17. Rank 3, effects: the railgun's trail settings.** `cl_railtrail_type` 0 (the classic spiral and
core of particles), 1 a solid beam core only, 2 the core and a spiral; `cl_railtrail_time` 1.0 s; `cl_railcore_color` red, `cl_railcore_width` 2; `cl_railspiral_color` blue, `cl_railspiral_radius`
3 (colours by name or `#rrggbb`). Our default trail is the classic one (landed); the settings are not
found. [R original, D ours]

**C18. Rank 3, picture: the particles' look and size.** A particle is a square facing the view, its
size 1 at up to 20 units away and growing by 0.004 a unit beyond, times `gl_partscale` (2) and the
particle's own scale. `gl_partstyle` 0 blends, 1 adds its light. `gl_partshape` 0 a soft round dot, 1
a hard square at 0.6 opacity, 2 a sharper round dot. `gl_beamstyle` 0 a flat beam, 1 another beam
look. [R] Ours: the classic particles (landed); these settings are not found and the size rule was
not compared. [D ours]

**C19. Rank 3, picture: shadows and model shading.** `gl_shadows` (0, saved): 1 a dark copy of each
model squashed onto the floor under it at half opacity, not on floors steeper than 60 degrees from
level, not for the held weapon; 2 also fading with height, gone once the model is four of its radii
above the floor. `gl_celshading` (0, 0 to 10): outlines on models, the line width scaled. `gl_dotshading`
(1): models shaded by the precomputed light directions turned with the model's yaw; 0 flat. [R] Ours:
models lit by the family's own light (landed, M18v); these switches not found. [R ours]

**C20. Rank 3, picture: infra-red goggles.** While the view's infra-red goggles flag is set (a mission
pack's item), every model with the IR-visible render bit (32768) is drawn flat red, unlit. [R] Ours:
not found (`q2-render-fx.md` records the bit; nothing draws it). [R ours]

**C21. Rank 3, picture: the renderer's settings by Quake II's names.** The original's names a Quake
II config carries, with their defaults, that ours does not have by these names (some are ours under
another name, which wants a synonym or an `own` row in Quake II's layer): `gl_swapinterval` 1,
`gl_texturemode` trilinear, `gl_anisotropy` 1, `gl_picmip` 0, `gl_saturation` 1, `gl_coloredlightmaps`
1, `gl_brightness` 0 (ours has it), `gl_modulate` 1 (ours has it), `intensity` 2 (ours has it),
`gl_dynamic` 1 (ours has it), `gl_dynamic_lightstyles` -1, `gl_dynamic_muzzleflash` -1,
`gl_polyblend` 1 (the screen tints), `gl_damageblend_frac` 0.2 (the extended protocol's damage tint
drawn as a frame round the screen, this share of it wide, 0 to 0.5), `gl_waterwarp` 0 (the view
warped under water; off by default), `gl_fog` 1, `gl_bloom` 0, `gl_glowmap_intensity` 0.75,
`r_fullbright` (cheat), `gl_lightmap` (cheat). [R] Rank 3: the defaults match our classic look except
where noted; the names matter for configs.

**C22. Rank 3, screenshots.** `screenshot [name]` writes in `gl_screenshot_format` (jpg in this
build, png in builds without JPEG), named by `gl_screenshot_template` (`quakeXXX`, the X's the first
free number), JPEG quality `gl_screenshot_quality` 90, PNG compression `gl_screenshot_compression` 6,
written off the main thread with `gl_screenshot_async` 1; `screenshotjpg`, `screenshotpng` and
`screenshottga` force a format. [R] Ours: the engine's `screenshot`; the format, template and the
forced-format commands not found. [D ours]

**C23. Rank 3, the console's settings and commands.** `con_alpha` 1, `con_scale` 0 (auto as C4),
`con_height` 0.5, `scr_conspeed` 3, `con_notifytime` 3 s, `con_notifylines` 4, `con_clock` 0 (a clock
in the console's corner), `con_background` `conback`, `con_scroll` 0, `con_history` (lines kept),
`con_timestampsformat` (`%H:%M:%S `), `con_timestampscolor` `#aaa`, `con_auto_chat` 0 (C31).
Commands: `condump <file>` (the console's text to a file), `clearnotify`, `clear`, `remotemode`
(type rcon commands in the console, its prompt `#` instead of the arrow). The prompt is a yellow
arrow; the program's name and version stand at the console's lower right in cyan. While connecting,
the console's foot shows the file being downloaded with a bar and "nn% (size)", or "Loading <map>...",
"Loading models...", "images", "clients", "sounds" in turn. [R] Ours: the classic console's look and
`con_notifytime`, `con_notifylines`, `con_height` are had; `con_alpha`, `con_scale`, `con_clock`,
`condump`, `clearnotify` (ours `con_clearnotify`), `remotemode` and the loading lines are not found.
[R ours] **Done (M18k4 rounds 32 and 33)**: `con_clock`, `condump`, `con_auto_chat`, `clearnotify`,
`remotemode`, `con_scale`, `con_alpha`, the download bar and "Loading <map>..."; not taken: the
version corner, the later loading steps, `con_background`, `con_scroll`, `con_history`,
`con_timestampsformat`, `con_timestampscolor`.

**C24. Rank 3, the loading plaque and the pause picture.** Starting to load a level stops every sound
and the music, then draws the pack's `loading` picture centred, unless the console or a menu is up,
and holds the screen until the level is in; after 120 s it gives up and prints "Loading plaque timed
out.". The `pause` picture is drawn centred while a single-player game is paused, with
`scr_showpause` 1; with 2 the demo bar says "[PAUSED]" instead. `cl_autopause` 1 pauses single
player and demos while the console or a menu is up and resumes them only if it paused them. [R] Ours:
the `loading` and `pause` plaques are drawn, and the window's own game pauses under the menu; the
plaque's stopping of the music and its timeout, and the `scr_showpause` and `cl_autopause` settings,
are not found. [R ours]

**C25. Rank 3, cinematics: other video files and the crop.** The original (in its usual build, with
the media decoder) looks for a cinematic `video/<name>` as `.ogv`, `.mkv`, `.mp4`, then `.cin`, in
that order, and only as loose files, never inside packs. It crops known black bars off six of the
Quake II family's videos when the file's size matches. Starting one stops the music. [R] Ours: `.cin`
played from the packs (`q2-cinematics.md`); the other formats, the loose-files rule and the crop not
found. [D ours]

**C26. Rank 3, HUD: hit markers.** With `cl_hit_markers` 1 or 2 (the fork's default 0), a hit the
server reports (extended protocol only) draws the pack's `marker` picture centred red for
`scr_hit_marker_time` (500 ms), fading by the square of the time gone; 2 also plays a sound. Our
servers send no such report; not found in ours. [R original]

**C27. Rank 3, the crosshair.** Landed in ours: `crosshair` N as the `chN` picture, `ch_x`, `ch_y`,
`ch_red`, `ch_green`, `ch_blue`, `ch_alpha`, `ch_health`. Still different: the fork draws the
picture at half of `ch_scale` (0.1 to 9), so 2 is the picture's own size; the crosshair is hidden
while the status flags ask it hidden (extended protocol) and while a scope picture is up
(`ch_hide_while_zoomed` 1); `ch_health` is a smooth ramp, red always full, green from 0 at 33 health to
full at 66, blue from 0 at 66 to full at 99, black when dead. [R] Ours draws `ch_scale` as our
`crosshairsize` (not halved) and was not compared on the ramp. [D ours]

---

## Question C2: sound, music, input, settings and commands

**C28. Rank 2, sound: the underwater muffle.** While the player's view is under water (the player
state's underwater flag) and `s_underwater` is 1 (default), every sound the client mixes passes a
high-shelf filter at 5000 Hz with the treble's gain `s_underwater_gain_hf` (0.25, held to 0.001 to 1),
the same under the software mixer and OpenAL. [R] Ours: no underwater filter on the sound was
found. [D ours] **Done (M18k4 round 34)**: `s_underwater` and `s_underwater_gain_hf`, one setting of both classic games (on in Quake II, off in Quake), a high shelf at 5 kHz on the synth's whole mix while the eye is in a liquid (`client/room_ear.cpp`, `engine/audio/synth.hpp`'s `muffle`).

**C29. Rank 2, music: the level's track on a joined server and in a recording.** The original plays
the track the level's configstring 1 names whenever a level is loaded and whenever that string
changes: empty or "0" stops the music; a whole number N plays `music/trackNN` (two digits); any other
text plays `music/<text>`; with `ogg_shuffle` 1 it plays the next of every file in `music/`, shuffled
once a cycle. The file is tried with its own extension, then as `.flac`, `.opus`, `.ogg`, `.mp3`,
`.wav`, from loose files only (never from a pack, which can be downloaded). A track that ends starts
again (or the next shuffled one). The music stops at a disconnect, a map change, a loading plaque and
a cinematic, and nothing plays while disconnected. `ogg_enable` 1 (turning it off stops at once),
`ogg_volume` 1 (0 to 1), `ogg_shuffle` 0; the command `ogg info` ("Playing <file>, <codec>, <rate> Hz,
<n> ch" or "Playback stopped."), `ogg play <file>` ("Can't play music in cinematic mode."), `ogg
stop`, `ogg next`. [R] Ours: the first game's music console (`bgmvolume`, `cd play|loop|stop|pause|
resume|info`), the track from the world's `sounds` key when the window's own game loads a level and
the track the program asks for at the intermission, played from the kept `music/trackNN.flac`. A
joined Quake II server's or a recording's configstring 1 was not found to reach it, nor the text
names, the shuffle, the `ogg` command or Quake II's spellings. [R ours]

**C30. Rank 2, the user info while joined.** The original sends its user info (`name`, `skin`,
`rate`, `msg`, `hand`, `fov`, `gender`, `spectator`, `password`, `version`, `uf`, and any setting
marked as user info) whenever one of them changes during a game, so a name, skin, handedness or field
of view changed mid-game reaches the server at once. `msg` (1, saved) is the lowest print level the
player wants (0 also pickups, 1 the usual, 2 only important, 3 only chat). [R; the levels' meaning D,
they are the game library's] A change is sent at the next packet on the stock protocol, and as only
the changed keys on the modern client's own protocols; never while a demo plays. [R] Ours: the user info is
built once, at the connect, and `msg` is always 1; no change was found to be sent while joined. [R
ours]

**C31. Rank 3, chat settings.** `cl_chat_notify` 1 (0 keeps chat out of the notify lines),
`cl_chat_sound` 1 (0 none, 1 `misc/talk.wav`, 2 `misc/talk1.wav`; a sound's name may be written
instead of the number), `cl_chat_filter` 0 (1 strips unprintable characters and ends the line),
`con_auto_chat` 0 (a console line typed without a slash: 0 a command, 1 said to all, 2 to the team),
`followip [n]` (connect to the n-th last address seen in chat), `trigger <command> <wildcard>` and
`untrigger` (run a command when a server line that is not chat, or a centre print, matches; never in
a demo or on a relay). [R] Ours: the chat sound (`misc/talk.wav`) and the ignore lists are landed; the
Quake II spellings, `followip`, `trigger` and `untrigger` are not found (our regex triggers are the
first game's). [R ours] **Done (M18k4 round 32)**: all of them; `cl_chat_notify`, `cl_chat_filter`,
`followip`, `trigger` and `untrigger` in the classic layer, `cl_chat_sound` and `con_auto_chat`
Quake II's own.

**C32. Rank 3, sound settings.** `s_enable` (1 software, 2 OpenAL, 0 none), `s_volume` 0.7,
`s_khz` 44 (saved), `s_mixahead` 0.1 s (0 to 1), `s_swapstereo` 0, `s_ambient` 1 (0 no entity loop
sounds at all, also none while paused; 1 all; 2 only the loops of entities that have a model; 3 only
the player's own), `s_auto_focus` 0 (0 sound always plays; 1 not while the window is minimized; 2
only while it has the focus), `al_device`, `al_hrtf` (OpenAL); commands `play <sound>` (a local
sound, the extension optional), `snd_restart`, `soundinfo`. [R] Ours: `s_volume` and `volume` are
had; the rest not found. [R ours]

**C33. Rank 3, the client's own commands.** Not found in ours: `writeconfig <file> [-a -b -c -m]`
(aliases, binds, saved settings, changed settings), `vid_restart`, `r_reload`, `in_restart`,
`snd_restart`, `fs_restart`, `viewpos` (prints the view's position and its yaw), `gun_next`,
`gun_prev`, `gun_model` (look at other view models), `dumpstatusbar`, `dumplayout` (write the status
bar program or the current layout to a file), `sky <name> [speed] [axis]` (exists in ours by name,
not compared), `timerefresh`, `setu` (set as user info), `resetall`, `cl_beginmapcmd`,
`cl_changemapcmd`, `cl_disconnectcmd` (a command run on entering each map, at each map change, at
each disconnect; empty by default), `cl_ignore_stufftext` (0; 1 refuses commands a server pushes
unless a whitelist file names them). [R] Rank 3 each; `cl_beginmapcmd` is how players auto-record. **Done (M18k4 round 35)**: `cl_beginmapcmd`, `cl_changemapcmd`, `cl_disconnectcmd` (`client/q2_session.cpp`).

**C34. Rank 3, sound: footsteps by surface and the landings.** On each footstep event the original
traces down from the entity (its box flattened, from 1 above its origin, 9 below plus its box's
bottom, or 66 more for an entity without a box) and picks the step set of the surface hit: a wall
texture's material is read from a `textures/<name>.mat` text file (a material name; `default` and
`ladder` are reserved), each material's steps are `sound/player/steps/<material>1.wav` upwards (at
most 9), the default set `sound/player/step1.wav` upwards. A step is picked at random from its set
(from the first four with `cl_footsteps 2`), never the same sound twice in a row. Another player's
steps (extended protocol) play at half volume with idle falloff; ladder steps use the ladder set.
A short fall plays one of `player/land1.wav` to `land8.wav` at random (`land1` only with
`cl_footsteps 2`); the stock packs hold only `land1`, so 7 times in 8 nothing is heard on them. [R; the last clause D] Ours: one of the four classic steps picked by the frame and the entity; materials
and the landing set not found. Rank 3: the stock packs carry no `.mat` files. [R ours]

**C35. Rank 3, input.** `in_grab` 1 (0 never take the mouse in a window, 1 in a game, 2 also let it go
while a demo plays or while chasing), `in_enable` 1, `m_side` 1 (the sideways mouse move's scale
with strafing), `+use` and `-use` (the use button, sent on the extended protocols). The original
has no joystick or gamepad support at all. [R] Ours: the mouse settings of `client-gaps.md` are
landed; `in_grab`, `m_side` and `+use` not found; our gamepads are more than the original has. [R
ours]

---

## Question C3: demos, the server browser, connecting, downloads, prediction, netgraph, chat, places

**C36. Rank 2, downloads: what is fetched before the game begins.** The original's order: the map
(when `allow_download_maps`), then every model the level names (`allow_download_models`), waiting for
the models before checking their skins: each model's own skin names and a sprite's frames; then the
sounds (`allow_download_sounds`; a name starting with `#` taken as a path), the pictures the level
names as `pics/<name>.pcx` (`allow_download_pics`), each player's figure (`allow_download_players`):
`tris.md2`, every carried weapon model the level names, `weapon.pcx`, the skin, the skin's `_i.pcx`
icon, and the figure's own sounds for the level's `*` sounds; then the sky's six `env/` pictures
(`allow_download_textures`); then, once the map is in, every wall texture it uses that is drawn
(`textures/<name>.wal`, and `.jpg` too with the texture overrides on). A file is fetched to
`<name>.tmp` and renamed when whole, and a `.tmp` already there is resumed from its length. A map on
disk whose checksum differs from the server's is fetched again once per connection; a second
mismatch ends the connection. All switches default 1 and are saved; `allow_download 0` (or a local
server) fetches nothing, -1 refuses even the `download` command ("Downloads are permanently
disabled."). [R] Ours (`net/q2_client.cpp`): the map, every model, every sound and each player's
`tris.md2` and skin; not the models' skins, sprite frames, pictures, weapon models, `weapon.pcx`, the
icons, the figures' sounds, the sky or the wall textures; no resume, no checksum re-fetch, and no
`allow_download` switches on the client. A server's custom map with its own textures draws with
them missing. [R ours]

**C37. Rank 3, downloads over HTTP.** `cl_http_downloads` 1: when the server names a download URL,
files are fetched over HTTP from it (or from `cl_http_default_url`), with `cl_http_filelists` 1 (a
`.filelist` for a map or a mod lists extra files to fetch), `cl_http_max_connections` 2,
`cl_http_proxy`, `cl_http_insecure` 0, `cl_http_blocking_timeout` 15 s; a failure falls back to the
UDP way. [R] Ours: UDP only. [R ours]

**C38. Rank 3, demos: recording's options and pauses.** `record [-z] [-e|-s] <name>`: `-z` gzip,
`-e` the extended 4086-byte packets, `-s` the standard 1390-byte ones (`cl_demomsglen` the default); `record` while recording prints its statistics; `suspend` and `resume` pause and resume the
recording (`suspend` toggles while `cl_demosuspendtoggle` is 1); `stop` prints statistics. `demo
<name>` plays `.dm2`, `.dm2.gz`, `.mvd2`, `.mvd2.gz` from `demos/` with the name's extension
optional and Tab completing it. `cl_demosnaps` 10: a snapshot every 10 s of play so `seek` backwards
is quick. [R] Ours: `record`, `stop`, `seek` and the timeline are landed (`client-gaps.md` rounds 22
to 27); `-z`, `-e`, `-s`, `suspend`, `cl_demosuspendtoggle` and `cl_demomsglen` are not (`client-gaps.md`
says "not taken"). [R ours]

**C39. Rank 3, demos: the demo bar's look.** `scr_demobar` 1: a bar along the bottom, filled by the
share of the file read, with the percentage centred under it; 2 also the time at the left as
minutes:seconds.tenths of game time; "[PAUSED]" at the right with `scr_showpause 2`. Not drawn for a
recording of the old compatible kind. [R] Ours: `demo_bar` (the first game's); Quake II's spelling
and look not compared. [D ours]

**C40. Rank 3, the demo browser's columns.** Lists `demos/` (with `ui_listalldemos` 1 every folder),
`.dm2`, `.dm2.gz`, `.mvd2` and `.mvd2.gz`; columns: the folder's path (name), Date, Size, Map, POV
(the player recorded); `ui_sortdemos` 1 (by name; the column number, negative descending). [R] Ours:
our own demo page (folders anywhere, newest first, a check line); Quake II's columns not shown. [R
ours]

**C41. Rank 3, the server browser.** The original's list comes from an HTTP list (q2servers.com, read
as raw lines), the address book (`favorites`), a `servers.lst` file and a LAN broadcast. Columns:
Hostname, Bots (Yes or No), Map, Players (players over slots), and the ping; the selection shows the
server's info (Key, Value) and its players (Frg, Ping, Name). `ui_sortservers` 0 (1 name, 2 the
second column, 3 map, 4 players, 5 ping; negative descending), `ui_colorservers` 1 (grey for a
password, anticheat or no-fake server; magenta with bots; green up to `ui_colorpingmax` 50 ms, yellow
past three times that, cyan for a lag-compensated server in between, white otherwise), `ui_pingrate`
0 (pings a second; 0 from `rate`). Commands: `pingservers` (the address book `adr0` to `adr15` and
the LAN), `serverstatus [address]` (a server's info and its players by frags). [R] Ours: our browser
(LAN, masters, our hub's masters, `sb_servers`; sorts, filters); Quake II's HTTP list, `servers.lst`,
the address book fields, the Bots column, the colours and the two commands are not found. [R ours]

**C42. Rank 3, connecting.** `passive` (wait for a server to connect to us, through a NAT),
`followip` (C31), `connect` completing addresses; `cl_protocol` 0 (the best the server offers; 34,
35, 36, 38 forced). [R] Ours: `connect`, `reconnect`, `disconnect` landed; `passive` and
`cl_protocol` not found. [R ours]

**C43. Rank 3, network settings the player sees.** `cl_maxpackets` 30 (packets a second on the
modern protocols, moves batched), `cl_packetdup` 1 (each move sent again in the next packet),
`cl_nodelta` 0, `cl_instantpacket` 1 (a button press sent at once), `cl_batchcmds` 1, `cl_showmiss` 0
(prints each prediction miss), `cl_shownet` 0, `qport` (the port's number in the connection).
[R] Ours: `cl_maxfps` and `r_maxfps` landed; the rest not found (`client-gaps.md`: waits for the
batched protocol). [R ours]

**C44. Rank 3, macros.** Typed and bound text expands `$name`: `$loc_here`, `$loc_there` (ours has
these two), `$cl_mapname`, `$cl_server`, `$cl_timer` (game time, minutes:seconds or
hours:minutes:seconds), `$cl_demopos` (minutes:seconds.tenths), `$cl_ups` (the speed, units a second,
the predicted one when predicting), `$cl_fps`, `$r_fps`, `$cl_mps`, `$cl_pps` (moves, drawn frames,
packets a second), `$cl_ping`, `$cl_lag` (packets lost, as a percentage with 2 decimals), `$cl_health`,
`$cl_ammo`, `$cl_armor`, `$cl_weaponmodel` (the held weapon's model name), `$cl_numentities`,
`$cl_surface` (the texture name and flags in the line of sight), `$net_uprate`, `$net_dnrate`,
`$com_date`, `$com_time` (by `com_date_format` and `com_time_format`), `$com_uptime`,
`$com_uptime_long`, `$random` (0 to 9), `$com_maplist` (every map's name). [R] Ours: `$loc_here` and
`$loc_there` only (and the first game's percent codes). [R ours]

**C45. Rank 3, places: drawing them.** `loc_draw 1` draws each place point of the map as a small
marker so a `.loc` file can be edited by sight. [R] Ours: the `.loc` files, `loc` editing,
`loc_dist` and `loc_trace` are landed; `loc_draw` not found. [R ours]

---

## Checked and the same (for coverage)

Each was compared and found landed in ours, as the notes say: the area list is read from the wire
(only its use is C1); the status bar and its layout language, the score blocks, the inventory, the
help computer, the scoreboard opened and closed by the rules; the crosshair pictures `ch1` to `ch3`
and the `ch_` colour, place and opacity settings; the held weapon's `cl_gun`, `hand`, `cl_gunfov`,
`cl_gun_x`, `cl_gun_y`, `cl_gun_z`, `cl_vwep`, `cl_noskins`; prediction (`cl_predict`,
`cl_predict_crouch`) and the other players run ahead; the colour shells, the item glow pulse and turn,
the gibs and their trails, the effects of every temporary entity, the entity loop sounds; the screen
tint from the player state; the view's bob, roll, kicks and fall; hor+ fields of view; `fov` as a
synonym; `m_autosens`, `m_filter`, `m_accel`; the chat sound and the ignore commands (`ignoretext`,
`ignorenick` and their opposites); `mul`, `div`; `record`, `stop`, `seek`, `cl_demowait`, `timedemo`; the console in the pack's `conchars` over `conback`; the `loading` and `pause` plaques; the `.cin`
cinematics and the end pictures; UDP downloads of the map, models and sounds; the server browser and
the demo page as pages of their own; place names (`.loc`, `loc`, `loc_dist`, `loc_trace`,
`$loc_here`, `$loc_there`); `cl_maxfps`, `r_maxfps`, `cl_timeout`, `rcon_address`, `showpackets`,
`showdrop`; `gl_modulate`, `intensity`, `vid_gamma`, `gl_brightness`, `gl_dynamic`. The original has
no joystick or gamepad support, so ours has nothing to match there. [R]

## Not settled by the source

- What `scr_lag_draw_scale` above 1 shows in its extra columns (the fork's lag graph reads its 48
  samples by a wider count; noted before in `q2-client-fork-delta.md`).
- What `gl_beamstyle 1` looks like in detail (not followed).
- Whether a stock client's recording carries an empty area list or a byte of 255 when the map has no
  areas depends on the server that recorded it, not on the client.

## Counts

45 gaps: rank 1: 0; rank 2: 9 (C3, C4, C5, C6, C7, C28, C29, C30, C36); rank 3: 36.

By area: world drawing 2 (C1, C2); view 2 (C3, C8); HUD 9 (C4, C9 to C15, C26); menus 3 (C5, C6, C7); effects and picture 7 (C16 to C21, C27); screenshots 1 (C22); console 1 (C23); loading and pause 1
(C24); cinematics 1 (C25); sound and music 4 (C28, C29, C32, C34); user info 1 (C30); chat 1 (C31); commands 1 (C33); input 1 (C35); downloads 2 (C36, C37); demos 3 (C38, C39, C40); server browser 1
(C41); connecting and network 2 (C42, C43); macros 1 (C44); places 1 (C45).
