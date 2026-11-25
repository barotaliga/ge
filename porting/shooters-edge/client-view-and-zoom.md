# Round 10 answers: the view (zoom, view height, the held weapon's place and size, bob)

Read: ezQuake release 3.6.9 (commit b2d448f, March 2026); Q2PRO "nightly" tag (commit 601a8df,
December 2025); the actionquake fork of Q2PRO (commit 6400f7e, November 2026). Each fact is marked
[read] (seen in the source or the project's own player docs) or [derived] (worked out from what was
read, not stated outright). Units: angles in degrees, distances in world units, time in seconds.

## A. The first game's modern client (ezQuake 3.6.9)

### 1. `+zoom` / `-zoom`

- The zoomed field of view is a fixed number, 35 degrees. There is no setting for it: no zoom fov
  setting, no zoom sensitivity setting, no toggle form. [read]
- Pressing (`+zoom`) remembers the current `fov` and the current `sensitivity`, sets `fov` to 35, and
  sets `sensitivity` to the remembered sensitivity times 35 divided by the remembered fov (a straight
  linear ratio of the two fov numbers, not of their tangents). Example: fov 110, sensitivity 4 gives
  4 × 35 / 110 ≈ 1.27 while zoomed. [read]
- Releasing (`-zoom`) puts back exactly the two remembered values. [read]
- A second press while already zoomed does nothing; a release while not zoomed does nothing (so a
  bound key cannot stack zooms). [read]
- The change is instant: the fov setting is simply written; there is no easing over time. [read]
- Both are written as ordinary settings, so anything that saves the config while zoomed saves the
  zoomed fov and sensitivity; a fov or sensitivity the player types while zoomed is overwritten by
  the remembered value on release. [derived]
- The fov setting itself is clamped to 10..140, default 90. Writing fov through the player's own
  console also updates a hidden "default fov"; a value of exactly 90 arriving from the server is
  replaced by that default (so a server's "fov 90" means "the player's own fov"). Fov changes are
  refused during a Team Fortress concussion and fov changes sent by the server are ignored during
  demo playback. Because zoom writes fov through the same path, the zoom's 35 also becomes that
  hidden default until release, and zoom has no effect while concussed. [read for the rules,
  derived for how they meet zoom]
- The factory key bindings (the "reset to defaults" bind set) put `+zoom` on ALT. [read]
- Nothing else in the client scales mouse speed by fov; the mouse is otherwise independent of fov.
  [read: no other fov term in the mouse path was found]

### 2. `v_viewheight`

- Default 0. Clamped by the client to −7..4 world units whenever it is used; the options menu offers
  −7 to 4 in steps of 0.5. The server never sees it and does not clamp it. [read]
- What it adds to: the eye is placed at the player's origin plus the server's view height (the
  server-sent view height stat when the server announces support for it, or in a converted older
  demo; otherwise the standard 22 units), plus a height adjustment, plus the stair-step smoothing
  offset. `v_viewheight`, when non-zero, IS that height adjustment. [read]
- When `v_viewheight` is 0 the height adjustment is the view bob instead (see question 6). So a
  non-zero `v_viewheight` both lifts or lowers the eye by a constant and removes the vertical camera
  bob. [read]
- It does not apply in the gib view (eye +8) or the dead view (eye −16 with 80 degrees of roll),
  those replace the normal view height. [read]
- It applies in every other case the normal first-person view is built: playing, demo playback and
  spectating (there is no demo or spectator test around it). [read]
- Two other places also read it, each clamped to −7..4: the start point of the player's own
  lightning beam is raised by it, and the point from which teamplay "what am I looking at" checks
  are made uses eye height 22 plus it. [read]
- The held weapon is placed at the eye, so it rises or drops with the eye; in addition the weapon is
  pushed forward by 0.4 times (bob plus the height adjustment), so a `v_viewheight` of −6 pulls the
  gun 2.4 units back toward the eye and +4 pushes it 1.6 forward. [read]

### 3. `v_gunkick`

- Default 0; the menu treats it as on/off ("Gun Kick"). Any non-zero value means on; there is no
  distinct meaning for 2. [read]
- It moves the VIEW, not the weapon model: when on, the camera is moved along its forward direction
  by the current kick amount and the camera pitch is changed by half that amount. [read]
- The kick comes from the server's two kick messages: a small kick sets a target of −2 and a big
  kick a target of −4. One number is used both ways: as a distance in units along the view's
  forward direction, and halved as degrees of pitch. With the negative values the eye moves BACK by
  up to 4 units and the view tilts UP by up to 2 degrees (pitch here is positive looking down). [read for the numbers, derived for the visual direction]
- Timing: from 0 the kick moves toward its target at 20 per second for the small kick and 40 per
  second for the big kick; on reaching the target the target resets to 0 and the kick returns toward
  0 at 20 per second. So the small kick takes 0.1 s out and 0.1 s back; the big kick 0.1 s out and
  0.2 s back. [read for the rates, derived for the durations]
- The kick messages are skipped when a demo is fast-forwarded or skipped through. With `v_gunkick`
  0 the kick is still tracked but never applied. [read]
- Separate settings `v_kickpitch`, `v_kickroll`, `v_kicktime` (all default 0) govern the damage
  kick, which is a different effect (pitch and roll by the direction of the hit, scaled by damage,
  fading over `v_kicktime` seconds). [read]

### 4. `r_viewmodelsize`

- The console name is `r_viewmodelSize` (case-insensitive lookup; the menu calls it "Weapon Size").
  Default 1. Used clamped to 0..1; the menu offers 0.1 to 1 in steps of 0.05. [read]
- The weapon is scaled along ONE axis only: its own forward (depth) axis, by a factor of
  0.5 + value / 2, so the factor runs from 0.5 (value 0) to 1 (value 1). The other two axes are not
  scaled. [read]
- The scale is about the model's own origin, which for a held weapon sits at the eye (plus the
  offsets of question 5), so the gun is squashed toward the eye: its far end (the muzzle) comes
  closer. Because perspective makes nearer points spread further from the screen's centre, the
  visible effect is a stubbier gun that does not move its base. [read for the axis and point, derived
  for the look]
- No interaction with fov: the weapon is drawn with the same projection as the world (there is no
  separate gun fov in this client). [read]
- Related: `r_drawviewmodel` (default 1) hides the gun at 0, draws it at that opacity between 0 and
  1, and at 2 hides it only while fov is above 90. `r_drawviewmodel_invisible` (default 0) controls
  whether the gun shows while the player holds the invisibility ring. With the reduced view sizes
  and the classic status bar, the gun is nudged up by 0.5 to 2 units to keep about the same amount
  in sight, unless `scr_fovmode` is non-zero. [read]

### 5. `r_viewmodeloffset`

- Default empty (no offset). The value is up to three numbers separated by spaces; missing numbers
  count as 0, extra numbers are ignored, so a single number is a sideways shift only. [read]
- Units are world units along the VIEW's axes (the camera's right, up and forward after view roll
  and kicks), applied after the bob push. [read]
- First number: positive moves the gun to the RIGHT. Second number: positive moves the gun DOWN
  (the second number is applied against the up direction). Third number: positive moves it
  FORWARD. [read]
- No clamp in the code; the options menu ("Weapon Shift") edits it as one number from −10 to 10 in
  steps of 1. [read]

### 6. `cl_bobhead`

- Default 0. [read]
- Background: the bob settings `cl_bob`, `cl_bobcycle` and `cl_bobup` all default to 0 in this
  client, so there is no bob at all out of the box; bob also is 0 for a spectator and keeps its last
  value while airborne. When enabled, the bob is horizontal speed × `cl_bob`, shaped as
  0.3 × that + 0.7 × that × the sine of the cycle phase (the rising part of the cycle takes the
  `cl_bobup` fraction of `cl_bobcycle`), clamped to −7..4 units. [read]
- With `cl_bobhead` 0 and `v_viewheight` 0: the eye gets one bob value added to its height, and the
  gun is pushed forward by 0.4 × (bob + the eye's bob). [read]
- Note: in that same case the bob is evaluated twice per frame and its internal clock advances on
  each evaluation, so the eye's bob and the gun's bob are one frame-time apart in phase and the
  cycle runs at twice the configured rate. [derived from the call pattern; worth a measurement]
- With `cl_bobhead` 1: the gun's bob is added to the eye's height adjustment as well (so the eye
  carries the bob, twice over when `v_viewheight` is 0, or `v_viewheight` plus the bob otherwise),
  and the separate gun bob is set to 0. The gun still follows the eye, and its forward push of 0.4 ×
  the height adjustment remains. In short: the bob is moved onto the camera and the gun rides along
  with the camera instead of bobbing on its own. [read]

### 7. `r_viewmodellastfired`

- Default 0; only acts during demo playback. [read]
- With it on, the weapon drawn is decided by these rules: if the tracked player holds no weapon
  model, draw none and remember none. If the weapon is animating (a non-zero weapon frame, which is
  what firing produces), draw the current weapon and remember it as last fired, together with which
  player is being watched. If the weapon is idle and a last-fired weapon is remembered for the SAME
  watched player, draw the remembered one instead of the current one. If the watched player has
  changed, forget and use the current weapon. With nothing remembered, draw the current weapon.
  [read]
- The effect: in a demo the view keeps showing the gun that last fired while the player quickly
  cycles or previews other weapons, and switches only when a new weapon actually animates. [derived]
- Outside demos the companion setting `r_viewpreselgun` (default 0) instead shows the preselected
  best weapon while the current one is idle, when weapon preselection is in use. [read]

## B. Quake II's modern client (Q2PRO nightly) and its fork (actionquake/q2pro)

### 8. Zoom

- Q2PRO: no zoom command and no zoom setting in the client. Zoom in this game is the game module
  changing the player's fov in the player state, which the client follows. [read]
- Mouse handling by fov in Q2PRO: `m_autosens`, default 0 (off). When on, mouse motion is multiplied
  horizontally by (current horizontal fov ÷ reference fov) and vertically by (current vertical fov ÷
  the vertical fov the reference makes at 4:3). The reference is the setting's own value when it is
  above 90 and at most 179; any other non-zero value means a reference of 90. The fov used is the
  player-state fov at 4:3 (before widescreen adjustment). It is linear in degrees, not by tangents.
  [read; the player doc says the same]
- The fork adds three-step scope sensitivities: `cl_zoom_autosens` (default 0, archived),
  `cl_zoom_2x`, `cl_zoom_4x`, `cl_zoom_6x` (each default 0, archived; the value is a full
  sensitivity, not a multiplier). When `cl_zoom_autosens` is on, every rendered frame looks at the
  current horizontal fov: exactly 45 is the 2x scope, exactly 20 the 4x, exactly 10 the 6x; if the
  matching setting is non-zero, `sensitivity` is set to it. At fov 90 or more, or any other fov, or
  a matching setting of 0, `sensitivity` is set back to the remembered unzoomed value. [read]
- The remembered value is held in a visible, non-archived setting `oldsens`, created from
  `sensitivity` when the input system starts. Each frame its numeric value is refreshed from the
  current sensitivity, but the restore uses its TEXT, which is not refreshed. So what gets restored is
  the sensitivity as it was when input started (or whatever the player sets `oldsens` to), and while
  `cl_zoom_autosens` is on, a sensitivity the player changes unzoomed is put back to that each frame.
  [read for the mechanism, derived for the consequence; worth a black-box check]
- The fork's own player doc says a 0 step keeps the previous step's sensitivity (for example 4x at 0
  stays at the 2x value). The code instead restores the unzoomed value for a 0 step. [read both,
  they disagree]
- The mod's `lens` command (the server side of the scope) is not part of these answers.

### 9. The held weapon's place

- `cl_gun`, default 1: 0 draws no gun; 1 draws it by the player's `hand` (right for 0, mirrored to
  the left for 1, not drawn for 2, centre); 2 forces right-handed drawing; 3 forces left-handed
  drawing; −1 also draws nothing. Value 1 additionally hides the gun when playing an old-format demo
  whose fov is above 90. [read; the player doc lists 0..3]
- Bandwidth: on the R1Q2 or Q2PRO protocols the client tells the server whether it wants gun data.
  `cl_gun` 0, or `cl_gun` 1 with `hand` 2, asks the server to stop sending the gun model and frames, and
  −1 asks it to stop the frames but keep the model index. Recording a demo on the server overrides
  this. [read]
- `hand` is a userinfo setting (archived), default 0. [read]
- Left-handed drawing is a MIRROR, not an offset: the gun is drawn with a projection whose horizontal
  axis is reflected (and triangle winding flipped to match), so the whole right-handed picture is
  flipped about the screen's vertical centre line. The gun's position in the world is not changed.
  This happens for `hand` 1 with `cl_gun` 1, or for `cl_gun` 3; `cl_gun` 2 never mirrors even with
  `hand` 1 or 2 (so `cl_gun` 2 with `hand` 2 draws a right-hand gun). [read]
- The beam weapons' start point follows the same handedness (right, mirrored left, or centre and
  one unit lower), in Q2PRO; the fork keeps the older beam placement. [read]
- `cl_gunfov`, default 90; 0 means "draw the gun with the world's fov"; any positive value is
  clamped to 30..160. It is a horizontal fov. With `cl_adjustfov` 1 (default) it is treated as the
  horizontal fov at 4:3 and widened for wider screens the same way as the world fov; with
  `cl_adjustfov` 0 it is the horizontal fov at the screen's own shape. The gun alone is drawn with
  this projection, so by default the gun looks the same at any player fov. Beam start points are
  rescaled by the ratio of the tangents of half the world fov to half the gun fov, horizontally and
  vertically, so beams still leave the gun's muzzle. [read]
- `cl_adjustfov` (default 1): the world fov is held as a horizontal fov at 4:3; with 1 the vertical
  fov is kept and the horizontal widens on wide screens ("Hor+"); with 0 the horizontal is kept.
  [read]
- `cl_gun_x`, `cl_gun_y`, `cl_gun_z`, each default 0, no clamp, world units along the view's axes:
  x positive = right, y positive = forward, z positive = up. Added to the server's gun offset. The
  player doc calls them developer settings that may misplace gun effects. [read]
- The game module has its own developer settings `gun_x`, `gun_y`, `gun_z` (default 0) that add to
  the gun offset it sends: x right, y forward, but z is applied DOWN (positive moves the gun down),
  the opposite of the client's `cl_gun_z`. [read]
- There is no gun scale setting in either client. [read]
- `cl_gunalpha`, default 1, clamped 0.1..1: the gun's opacity. In Q2PRO, when it is 1 the gun takes
  the player entity's own translucency instead; the shell effect layer is drawn over the gun at 0.3
  of the gun's alpha. The fork draws the shell at 0.3 × `cl_gunalpha` regardless. [read]
- The gun is always drawn with its depth squeezed into the first quarter of the depth range, so it
  never sinks into walls, and at minimum light. [read]

### 10. Bob, roll and kick a player sets

- Quake II's clients have NO client-side bob, roll or view height settings: no `cl_bob*`, no
  `cl_rollangle`, no `cl_viewheight` exists in Q2PRO or the fork. The view offset (eye height plus
  bob and fall dip), the kick angles (bob pitch and roll, run pitch and roll, damage and fire kicks)
  and the gun offset and angles all arrive from the game module in the player state, interpolated
  between frames by the client. [read]
- `cl_kickangles`, default 1, marked as a cheat-protected setting (it can be changed only where
  cheats are allowed): at 0 the client stops adding the server's kick angles to the view, which
  removes the view bob tilt, the strafe roll and the fire and damage kicks together. [read]
- `cl_nobob`, default 0: despite the name, it stops the bobbing of pickup items lying in the world,
  not the view. [read; the player doc says the same]
- `cl_rollhack`, default 1: concerns other players' models' roll, not the view. [read]
- The server-side game settings that shape the view, all in the game module, same defaults in Q2PRO's
  bundled game and the fork's Action game: `sv_rollangle` 2 and `sv_rollspeed` 200 (strafe roll),
  `run_pitch` 0.002 and `run_roll` 0.005 (tilt with speed), `bob_up` 0.005, `bob_pitch` 0.002,
  `bob_roll` 0.002 (the step bob). These are server settings, not the player's. [read]
- Third person: `cl_thirdperson` (default 0, cheat-protected), `cl_thirdperson_angle` (default 0),
  `cl_thirdperson_range` (default 60). [read]

## View tints and scr_fovmode (round 17 reader)

Read from ezQuake 3.6.9. [read] is stated by the release, [derived] is worked out from facts read.

### The tint layers

- Four layers, mixed in this order every frame: contents (water, slime, lava, or the custom tint
  of the `v_cshift R G B strength` command), damage, bonus (the pickup flash), powerup. Dynamic
  light tints go on top, during the world draw of the same frame. [read]
- Mixing: start black with alpha 0. For each layer with alpha a_l above 0 the total alpha becomes
  a + a_l × (1 − a), and the colour moves toward the layer's colour by a_l / (new total alpha), a
  linear mix. Total alpha clamped to 0..1. Lights use the same rule after the layers. [read]
- Layer alpha: damage, bonus and powerup are strength × gl_cshiftpercent / 100 / 255, and all
  three are dropped when gl_cshiftpercent or gl_polyblend is 0. The contents layer is
  strength / 100 / 255 with gl_cshiftpercent and v_contentblend already multiplied in. The
  `v_cshift` tint in open air ignores gl_cshiftpercent (alpha = strength / 255). gl_polyblend 0
  zeroes the contents layer, except in Team Fortress, where the custom open-air tint survives.
  [read]
- Damage strength goes up by 3 per point of damage, capped at 150, then is multiplied by
  v_damagecshift. It falls by 150 per second. [read]
- Shown as one full-screen polygon over the 3D view (gl_hwblend 0, the default). With gl_hwblend 1
  and hardware gamma, outside Team Fortress, it goes into the gamma ramp instead, updated at most
  vid_hwgamma_fps times per second (default 60, at least 10). [read]

### The pickup flash

- Started only by the console command `bf`, which a QW server stuffs on a pickup. Each `bf` first
  fires the `f_bonusflash` message trigger, at most once per frame, even when the flash is off.
  [read]
- `cl_bonusflash` (default 0, menu "Pickup Flash") gates only a `bf` stuffed by the server. A `bf`
  the player types or binds always flashes. [read]
- A new `bf` resets the strength to 50. It does not add, and it does not keep the larger value.
  The colour is (215,186,69). The strength fades by 100 per second of frame time, also while not
  connected. That gives alpha 50/255 = 0.196 at gl_cshiftpercent 100, gone after 0.5 s.
  [read, numbers derived]
- There is no per-layer percent setting for the bonus tint. [read]
- Demos take the same path, so the flash shows with `cl_bonusflash` 1. In MVD or QTV playback,
  text stuffed for players other than the tracked one is dropped, and so is everything stuffed
  while the demo seeks. The client has no spectator rule of its own. Whether a live server sends a
  spectator the tracked player's `bf` is not settled by the client. [read]

### Dynamic-light tints

- A light tints the view only when it is drawn as a flash-blend glow and the eye is within
  0.35 × its radius. Ordinary lights are drawn as glows only with gl_flashblend non-zero (default
  0). Rocket and missile trail lights need gl_flashblend 2 or bit 1 of gl_rl_globe (default 0).
  At the defaults, then, no light tints the view. [read; the last sentence derived]
- The light keyed to the viewed player (your own muzzle flash and powerup glow) is never drawn and
  never tints. [read]
- Alpha is 0.0015 × radius (world units), clamped 0..1, times v_dlightcshiftpercent (clamped 0..1,
  0 is off). The alpha is the same anywhere inside the glow: it does not fall off with distance.
  Every light the eye is inside adds its own tint by the mixing rule, so tints compound. Nothing
  fades: the tint is worked out again each frame from the lights alive then. [read]
- Default radii: explosion 350 (150 + 200 × r_explosionLight), shrinking 600 units/s
  (300 × r_lightdecayrate, default 2) and dead after 0.5 s. Muzzle flash 200 to 231, lives 0.1 s.
  Effect glows 200 plus a flicker of 0..31. Bright-light effects 400 plus flicker. Rocket light
  200 × r_rocketLight. Example: inside a new explosion (within 122.5 units) the tint alpha is
  0.525 × 0.5 = 0.26. [read; example derived]
- Colour: `v_dlightcolor` (default 1, 0 or 1). At 0 every light tints orange (1, 0.5, 0). At 1 the
  tint takes the light's glow colour, scaled so its strongest channel is 1. Normalised, the colours
  come to [derived]:
  - default, explosion and rocket lights: about (1, 0.5, 0.25)
  - muzzle flash: (1, 0.4, 0.2)
  - red: about (1, 0.1, 0.1)
  - blue: about (0.16, 0.16, 1)
  - red and blue: about (1, 0.1, 0.8)
  - green: about (0.11, 1, 0.11)
  - red and green: about (1, 0.9, 0.1)
  - blue and green: about (0.11, 1, 0.89)
  - all three: white
  - a light with a colour the player chose: that colour, normalised the same way
- v_dlightcshift 2 leaves out every trail light and every light keyed 0 to 32. That covers
  explosions, all muzzle flashes and the glows on players' entities. Glows on other entities (flags,
  keys, glowing items) still tint. [derived from the keys each light gets]

### scr_fovmode

- `fov` (default 90, 10..140) is the horizontal field at 4:3. Wider screens keep the 4:3 vertical
  field and widen the horizontal. Narrower screens keep the horizontal. Results are clamped to
  10..165 degrees horizontal and 10..140 vertical. `viewsize` (default 100) is clamped 30..120 and
  steps by 10. `cl_sbar` (default 0) draws the bar over the view at 0 and below it at 1. The bar is
  48 pixels below viewsize 110, 24 pixels from 110 to 119, and none at 120. [read]
- The mode matters only when the bar below the view cuts the view's height. That happens with
  cl_sbar 1 at viewsize 100..119, or below viewsize 100 on very short screens (at 90, under 480
  pixels). [read; 480 derived]
- Mode 0 (default) crops: full width, the full screen's horizontal field, a vertical field cut to
  the shorter view. [read]
- Mode 1 narrows the view to its height × the screen's aspect, centred with black side bars, and
  keeps the full-screen field. [read]
- Example, 1920×1080, fov 90, cl_sbar 1, viewsize 100: mode 0 gives 1920×1032 at
  106.26° × 71.26°, mode 1 gives about 1834×1032 at 106.26° × 73.74°. [derived]
- Weapon nudge, mode 0 only, raised along world up:
  - 1 unit at viewsize 110 and 2 units at 100, both only with cl_sbar 1 and scr_newhud 0
  - 1 unit at 90 and 0.5 at 80, whatever the bar and HUD
  - nothing at any other viewsize, and nothing in mode 1 [read]
- With cl_sbar 0 at viewsize 100 or more, or at 120, the mode does not change the field. The weapon
  nudge still differs at viewsize 80 and 90. [derived]

### Other settings

- No other v_ or scr_ setting tints or reshapes the 3D view. `v_centermove` (0.15) and
  `v_centerspeed` (500) set the pitch auto-centring. `cl_demoplay_flash` (0.33, clamped 0..1)
  scales a grey or white `v_cshift` tint, the Team Fortress flash, during demo playback.
  `v_cshift`, typed by a player on a Team Fortress server, is ignored. [read]

## Crosshair placement and pictures (round 18 reader)

Read: ezQuake 3.6.9; actionquake/q2pro at its latest commit (6400f7e), with upstream Q2PRO nightly
for what the fork changed. "Console pixels" are ezQuake's virtual 2D screen (`vid_conwidth`),
and "HUD units" are Q2PRO's window pixels divided by the HUD scale.

### ezQuake

- `cl_crossx`, `cl_crossy`: default 0, no clamp, fractions kept, positive right and down. In
  console pixels under `crosshairscalemethod 0` (default), in window pixels under 1. Not scaled by
  `crosshairsize`. They move every textured style and the `crosshairimage` picture, and the main
  view's character "+" (always in console pixels). Split multiview's character crosshairs ignore
  them. There is no hit marker. [read]
- The aim point: the screen's horizontal middle, and the vertical middle of the 3D view's
  rectangle, so a status bar below the view raises the crosshair by half the height it takes. Zoom
  does not move it. Not drawn in menus or the intermission. Over the held scoreboard it stays
  while `scr_showcrosshair` is 1 (default). [read]
- `crosshairimage`: a name under `crosshairs/` on the game's search path (packs included; the help
  names `ezquake/crosshairs/` and `qw/crosshairs/`). Any extension typed is ignored. Types tried
  TGA, PNG, JPEG, PCX, first found wins. Drawn at its own size in window pixels times
  `crosshairsize` (clamped 0 to 20); no 320-wide scale, no `crosshairscale`. Multiplied by
  `crosshaircolor` (so white shows a coloured picture as it is) and by `crosshairalpha`
  (alpha 0 draws nothing). While loaded it replaces every style and shows even at `crosshair 0`.
  A missing file prints "Couldn't load image <name>" and the selected style is drawn. [read]
- `crosshairs/crosshair.txt`: 64 X or O cells (white space ignored) form the 8 by 8 cross of
  `crosshair 1`. Bad files print "Invalid format in crosshair.txt (Need 64 X's and O's)" or "(Only
  X's and O's and whitespace permitted)", and the character "+" is used. [read]
- Styles: 1 "+" glyph (8 console pixels, plain text colour, deaf to the crosshair settings), 2
  stippled cross, 3 small cross (arms one eighth of the grid), 4 dot (default), 5 diagonals with an
  open middle, 6 nothing visible, 7 square outline with a dot; above 7 the "+" glyph. [read]
- `crosshairscale` (0 to 5) sets the built-in grid to 8 to 256 texels. Texels keep their on-screen
  size, so it changes the size as well as the sharpness: on-screen square = grid × `crosshairsize`
  × (window width ÷ 320 under method 0, or 1 under method 1). The dot stays one texel.
  [read, the dot derived]
- `r_smoothcrosshair` (default 1): smooth or nearest filtering of the textured crosshairs. [read]

### Q2PRO

- `crosshair N` (default 0) draws `pics/chN` (PCX, or a PNG, JPEG or TGA of the same name tried
  first under `r_override_textures 1`; a replacement beside an existing PCX is drawn at the PCX's
  size). A missing picture warns "Couldn't load pics/chN.pcx: <reason>" and draws a small
  placeholder. [read; placeholder derived]
- Size: picture size × `ch_scale` (default 1, clamped 0.1 to 9; the fork uses half the value),
  rounded, in HUD units, so the HUD scale (`scr_scale`, 0 automatic: 1, 2 from 1080 tall, 4 from
  2160) multiplies it. Placed at the middle of the whole screen, not the 3D view. [read]
- `ch_x`, `ch_y` (default 0): whole HUD units, positive right and down; they move the crosshair,
  the hit marker and (fork) the sniper scope picture. [read]
- Colour: `ch_red`, `ch_green`, `ch_blue`, `ch_alpha` (default 1, clamped 0 to 1) multiply the
  picture; `scr_alpha` does not apply. `ch_health 1` replaces the RGB by health: black at 0 or
  below; red full; green 0 below 33, ramping to full at 66; blue 0 below 66, ramping to full at
  99 (100 white, 66 yellow, 33 red). [read]
- Hit marker: `cl_hit_markers` (fork 0, upstream 2; 2 adds a sound), the picture `pics/marker` in
  red, sized like the crosshair, fading over `scr_hit_marker_time` (500 ms) as 1 minus the square
  of the elapsed fraction; shown only while the crosshair is. [read]
- Fork only: `ch_hide_while_zoomed` (1) hides the crosshair under a sniper scope. `xhair_enabled`
  (0) swaps in a line crosshair drawn in window pixels with its own `xhair_x`, `xhair_y`,
  `xhair_dot`, `xhair_thickness` (1), `xhair_length` (4), `xhair_gap` (10) and spread settings
  (`xhair_deployed_weapon_gap`, `xhair_movement_error`, `xhair_firing_error`, `xhair_elasticity`,
  `xhair_fast_transition`), coloured by the ch_ colour settings. [read]
