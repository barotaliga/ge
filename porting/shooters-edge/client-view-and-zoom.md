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

## B. The sequel's modern client (Q2PRO nightly) and its fork (actionquake/q2pro)

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

- The sequel's clients have NO client-side bob, roll or view height settings: no `cl_bob*`, no
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
