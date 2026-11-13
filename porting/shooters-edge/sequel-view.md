# The sequel's first-person view: answers

Source read: Quake II, id Software's 3.21 source release (GPL-2.0), the game library, the shared
player movement, the server's player-state writer and the client. Every fact is marked:
**[read]** stated directly by the source, **[derived]** worked out from facts read (arithmetic or
combining two places), **[measured]** run (nothing here was run). Nothing in this document is
copied from the source.

Conventions used throughout (derived from the engine's angle-to-vector rule): angles are pitch,
yaw, roll in degrees. **Positive pitch looks down**; negative pitch looks up. **Positive roll puts
the right side of the view down** (the picture tilts clockwise as seen by the player). Speeds are
units per second. The server frame is 0.1 s (10 Hz) **[read]**. "This frame" below means one
server frame.

The game library (on the server) computes all of: the view offset, the kick angles, the gun
offset, the gun angles, the screen blend and the view angles of a dead player, once per server
frame at the end of the frame, for each player **[read]**. The client only interpolates and adds
these (question 9) **[read]**.

---

## 1. View height

- Standing: **22** units above the player's origin **[read]**. The player's box is 32 units across
  and runs from 24 below the origin to 32 above it when standing **[read]**.
- Crouched (ducked): **-2** units (2 units *below* the origin); the box top drops to 4 above the
  origin **[read]**. The drop from 22 to -2 is 24 units **[derived]**.
- Dead (body intact): the movement code forces the ducked state for a dead player, so the view
  height is **-2** **[read]**. (Death also sets the body's box top to 8 below the origin, but the
  next movement step resets the box from the ducked rule; the view height stays -2 **[derived]**.)
- Gibbed (only the head is left): **8** units, with a box from 0 to 16 above the origin
  **[read]**.
- Spectating / no-clip flight: **22** **[read]**.
- During the end-of-level intermission: **0** **[read]**. A spectator chasing another player also
  gets 0 (the chase camera places the origin itself) and has prediction turned off **[read]**.
- Where it comes from: the shared player movement step (the same one the client runs for
  prediction) sets the view height from the ducked state on every user command; the game library
  copies it to the player after the move, and at the end of the server frame it becomes the
  vertical part of the view offset **[read]**.
- Ducking: the player ducks when the move's up input is negative and the player is on the ground,
  and otherwise the player stands up only if the standing box fits where they are **[read]**. In the air
  a ducked player stands back up if there is room **[derived]**.
- Smoothing: **none on the server**; the 24-unit change happens in one step **[read]**. The only
  smoothing is the client's linear interpolation of the view offset between the last two server
  frames (over 0.1 s, question 9) **[read]**. The client's own prediction also computes a view
  height but it is not used for the view; the view uses the server's view offset **[derived]**.

## 2. The stride bob on the server

**The speed:** horizontal speed only, the length of the velocity's two horizontal components
**[read]**. Vertical speed is ignored **[read]**.

**The cycle:** a per-player bob time, advanced once per server frame (0.1 s) by a step that
depends on the speed **[read]**:

| horizontal speed | step per server frame | one half-sine ("one stride") lasts |
| --- | --- | --- |
| below 5 | bob time reset to 0, step 0 | (no bob) |
| 5 to 100 | 0.0625 | 1.6 s **[derived]** |
| over 100 to 210 | 0.125 | 0.8 s **[derived]** |
| over 210 | 0.25 | 0.4 s **[derived]** |

The steps are **[read]**, the durations **[derived]**.

- The step is chosen only while the player is on the ground **[read]**. In the air (at 5 or more)
  the step is **not** set to zero: the step keeps whatever value it last had, so the bob keeps
  cycling in the air **[read]**. Edge case: that last value is held in one variable shared by every
  player in the game library, so an airborne player continues with the step left by whichever
  player was processed before **[derived]**. A source comment claims bobbing cycles only on the
  ground; the behaviour does not match that **[read]**.
- Crouched: the accumulated bob time is multiplied by **4** before use (it is the stored value that
  is scaled, not the step), so the cycle runs 4 times as fast while ducked and the phase jumps
  when the player ducks or stands **[read]** / **[derived]** for the jump.
- **Stride number** = the whole-number part of the (possibly ×4) bob time; **bob fraction** = the
  absolute value of the sine of (bob time × pi), so it goes 0 to 1 to 0 once per whole unit of bob
  time and is never negative **[read]**.

**(a) Height:** bob height = bob fraction × horizontal speed × **bob_up**, then capped at a maximum
of **6 units** (upper cap only; there is no lower cap) **[read]**. It is added to the view
offset's vertical part (always upward with the default) **[read]**. Example: 300 u/s at the
default 0.005 peaks at 1.5 units **[derived]**; the 6-unit cap is reached at 1200 u/s **[derived]**.
Not multiplied when crouched **[read]**.

**(b) Pitch and roll** (added to the kick angles):
- Bob pitch = bob fraction × **bob_pitch** × horizontal speed, × **6** when crouched **[read]**.
  Always the same sign (positive: nose down, with a positive setting) — no flip per stride
  **[read]** / **[derived]**.
- Bob roll = bob fraction × **bob_roll** × horizontal speed, × **6** when crouched, and **negated on
  odd strides** (so it alternates side every stride) **[read]**.
- No caps on either **[read]**.

Footsteps (not a view effect, for completeness): while on the ground above 225 u/s, the server
emits a footstep event when the integer part of the bob time is about to change **[read]**.

## 3. run_pitch and run_roll

Using the player's view-direction vectors (from the view angles, pitch included) **[read]**:
- Pitch added = (velocity · forward) × **run_pitch** **[read]**. Moving forward gives positive pitch
  (nose dips down); moving backward tips the view up **[derived]**. Default 0.002: 300 u/s forward
  adds 0.6 degrees **[derived]**.
- Roll added = (velocity · right) × **run_roll** **[read]**. Moving to the right gives positive roll
  (right side down, leaning into the strafe) **[derived]**. Default 0.005: strafing at 300 u/s adds
  1.5 degrees **[derived]**.
- Full 3D velocity is used (falling while looking down contributes to the pitch term) **[read]**.
  No caps **[read]**. Not reduced when crouched **[read]**.

## 4. sv_rollangle / sv_rollspeed

- Formula: side speed = velocity · right (view right vector). If its magnitude is below
  **sv_rollspeed**, roll = magnitude × **sv_rollangle** / **sv_rollspeed**; otherwise roll =
  **sv_rollangle**; then the sign of the side speed is restored **[read]**. Moving right gives
  positive roll **[derived]**. Cap: sv_rollangle (2 degrees by default), reached at 200 u/s
  **[read]**.
- **Where it is applied: NOT to the view.** In the stock game this roll is used only for the
  player *model's* roll that other players see, multiplied by **4** (so up to 8 degrees with the
  defaults) **[read]**. It goes neither into the view angles' roll sent to the player nor into the
  kick angles **[read]**. The view's sideways roll from strafing comes only from run_roll (question
  3) **[read]**.
- The model's pitch is the view pitch divided by 3, its yaw the view yaw **[read]**.

## 5. The damage kick and the damage blend

All taken damage during a frame is summed per player (health lost, armour absorbed, power armour
absorbed, and the knockback), along with the impact point of the latest hit; the totals are turned
into the blend and the kick at the end of the frame and then cleared **[read]**.

**Count:** count = health lost + armour absorbed + power-armour absorbed this frame **[read]**. If
it is 0, nothing at all happens (no blend change, no kick) **[read]**. For the blend and the kick
minimum, a count under 10 is raised to **10** ("always a visible effect") **[read]**.

**The kick:**
- Size: the frame's summed knockback (absolute value) **[read]**. If it is 0, or the player is dead
  (health 0 or less), there is **no view kick** **[read]**. So damage with no knockback (falling,
  drowning, lava, slime) blends the screen but does not kick the view **[derived]**.
- kick = knockback × 100 / current health, then at least count × 0.5, then at most **50**
  **[read]**.
- Direction: the unit vector from the player's origin to the impact point (3D) **[read]**.
- Roll part = kick × 0.3 × (direction · right) **[read]**: a hit from the right tilts the right
  side down (toward the source) **[derived]**.
- Pitch part = kick × 0.3 × (minus direction · forward) **[read]**: a hit from in front tips the
  view up; a hit from behind tips it down **[derived]**.
- So each part is at most 15 degrees **[derived]**.
- A new kick replaces the previous one (not added) and restarts the timer **[read]**.
- Duration and decay: lasts **0.5 s**; each frame the pitch and roll parts are scaled by
  (remaining time / 0.5 s), a linear fall from 1 to 0 **[read]**. When the time has run out the
  stored parts are zeroed **[read]**. Because the kick is set and first used in the same end-of-frame
  pass, the frames carry factors 1, 0.8, 0.6, 0.4, 0.2, 0 **[derived]**.
- It goes into the **kick angles** (pitch and roll only, never yaw) **[read]**.
- The forward and right vectors are those of the player's view angles **[read]**.

**The blend:**
- Strength (alpha): if negative it is made 0; then count × 0.01 is **added** to what remains of the
  previous damage alpha; the result is clamped to **0.2 minimum, 0.6 maximum** **[read]**.
- Colour: a weighted mix by the share of the (un-raised) count: power armour's share of **green**
  (0, 1, 0), armour's share of **white** (1, 1, 1), health's share of **red** (1, 0, 0) **[read]**.
  All health damage is pure red **[derived]**.
- Decay: after being composited into this frame's blend, the damage alpha drops by **0.06 per
  server frame** (0.6 per second) to a floor of 0 **[read]**. At the 0.6 maximum it is visible for
  10 frames (1 s) **[derived]**.
- Composite order of the full-screen blend each frame: the eye's contents first (lava or solid:
  orange 1, 0.3, 0 at 0.6; slime: 0, 0.1, 0.05 at 0.6; water: 0.5, 0.3, 0.2 at 0.4), then one
  powerup tint (quad: blue at 0.08; invulnerability: yellow at 0.08; environment suit: green at
  0.08; rebreather: pale green 0.4, 1, 0.4 at 0.04; flashing in the last 3 s), then damage, then the
  item-pickup flash (0.85, 0.7, 0.3) whose alpha drops 0.1 per frame **[read]**. Each layer is
  composited "over" the running total: new alpha = old + (1 - old) × layer alpha, and the colour is
  the mix weighted by how much of the alpha is new **[read]**.
- Side effects: the status bar's health and armour backgrounds flash for the frame (armour only if
  not in god mode or invulnerable), and a pain animation and pain sound start (sound at most every
  0.7 s, chosen by health band 25/50/75/100) **[read]**.

## 6. The fall

- Measured at the end of each server frame from the change in the player's vertical velocity
  since the last frame **[read]**:
  - if the player was falling, is still airborne and the vertical speed rose (hit something in the air), the fall value uses the old vertical speed itself.
  - Otherwise it is measured only when on the ground, using the change new minus old **[read]**.
- Severity = that speed squared × 0.0001 **[read]** (e.g. 387 u/s gives 15, 548 gives 30, 742
  gives 55, 894 gives 80 **[derived]**).
- Water: no fall at all when fully under water; × 0.25 when waist deep; × 0.5 when feet are in
  water **[read]**.
- Not applied in no-clip or when the player is not in the player model (gibbed) **[read]**.
- Severity under 1: nothing. Under 15: only a footstep sound, **no view effect** **[read]**.
- 15 or more: the fall value = severity × 0.5, capped at **40**, and the fall timer starts at
  **0.3 s** **[read]**. (So the smallest view-affecting fall value is 7.5 **[derived]**.)
- View effect while the timer runs, with factor = remaining time / 0.3 s (linear 1 to 0) **[read]**:
  - the pitch gains factor × fall value (the view **dips nose-down**, up to 40 degrees) **[read]**, sign **[derived]**.
  - The view height loses factor × fall value × **0.4** (a landing **dip of up to 16 units**) **[read]**, the 16 **[derived]**.
  - With 0.1 s frames the factors seen are 1, 2/3, 1/3, 0 **[derived]**.
- Sounds and damage: severity 15 to 30 gives the short-landing sound; over 30 the landing sound
  (55 or more: the far-fall sound), no ordinary pain sound, and damage of (severity − 30) / 2, at
  least 1, applied unless deathmatch with the "no falling damage" dmflag (value 8) **[read]**. The
  damage itself has no knockback, so it adds a blend but no damage kick **[derived]**.
- A new fall restarts the timer and replaces the value (not added) **[read]**.

## 7. Weapon kicks

Firing sets two per-player values: a kick-angle triple and a kick-origin triple (in world units,
world axes) **[read]**. Both are **cleared at the very end of every server frame**, after the view
has been computed, so a kick is present in exactly one server frame's player state (0.1 s) and
there is no decay curve on the server **[read]**. They are **set, not added**: a later shot in the
same frame overwrites the earlier one **[read]**. On the client the one-frame kick ramps in and
out over the interpolation between frames **[derived]**.

Per weapon (pitch negative means the view kicks **up**; "back 2" means the kick origin is the view's
forward vector, pitch included, times −2, i.e. 2 units backward along the aim) **[read]**:

- **Blaster** and **hyperblaster** (every bolt): pitch −1, back 2.
- **Grenade launcher**: pitch −1, back 2.
- **Rocket launcher**: pitch −1, back 2.
- **Shotgun**: pitch −2, back 2.
- **Super shotgun**: pitch −2, back 2.
- **Railgun**: pitch −3, back 3.
- **Machinegun** (each bullet): yaw and roll each a uniform random value in −0.7 to +0.7; kick
  origin random −0.35 to +0.35 on each world axis; pitch = −1.5 × the shot counter. The counter
  rises by 1 per bullet up to 9 (so down to −13.5 degrees) **only outside deathmatch**; in
  deathmatch it stays 0, so the pitch kick is 0. The counter resets when the trigger is released.
  The bullet's aim includes these kick angles (the climb moves the shots too).
- **Chaingun**: each of pitch, yaw, roll random −0.7 to +0.7; kick origin random −0.35 to +0.35 on
  each world axis (once per firing frame, whatever the 1 to 3 bullets).
- **BFG10K** (on release): kick origin back 2 and **no kick angles**; instead it sets the **damage
  kick** channel (question 5) to pitch −40 (a large upward tip, "an inverse fall") and roll random
  −8 to +8, lasting 0.5 s with the same linear decay **[read]**.
- **Hand grenades**: no kick **[read]**.
- Quad damage multiplies the damage and the bullets' push on targets, not the view kick **[read]**.

Edge case: −40 degrees of pitch at the first frame is outside what the kick-angle wire field can
carry (question 8) **[derived]**.

## 8. The whole per-frame build, in order, and the clamps

End of each server frame, per player, in this order **[read]**: update the falling check (Q6) →
damage feedback (Q5) → view offset and kick angles (below) → gun offset and angles → screen blend
(it reads the eye's contents at the final view position) → status, events, effects, sound,
animation → remember velocity and view angles → clear weapon kicks.

**Kick angles, alive** **[read]**, start from the weapon kick angles of this frame, then add:
1. the damage kick, pitch and roll × its remaining factor.
2. the fall pitch × its remaining factor.
3. run pitch (forward speed × run_pitch) and run roll (side speed × run_roll).
4. bob pitch (× 6 crouched).
5. bob roll (× 6 crouched, sign flipped on odd strides).
No clamp on the kick angles **[read]**.

**Dead** **[read]**: the kick angles are all 0, and the view angles themselves are forced to
**roll 40, pitch −15** (looking 15 degrees up, tilted 40 degrees right-side-down **[derived]**),
**yaw toward the killer** (the attacker, else the inflictor, else the player's own facing when
neither exists or it was the world or the player). The same forced angles are also set on every
user command while dead, and a spectator chasing a dead player sees them too.

**View offset** **[read]**: start at 0, then
1. + view height (vertical).
2. − fall dip (factor × fall value × 0.4, vertical).
3. + bob height (capped at 6, vertical).
4. + weapon kick origin (all three axes).
5. clamp: horizontal axes each to **−14 … +14**, vertical to **−22 … +30**, "so the view never
   leaves the player box".

**Wire quantisation** **[read]**: the view offset, kick angles, gun offset and gun angles each go
as a signed byte holding the value × 4 (quarter units, quarter degrees); a value is sent only when
changed from the previous frame. The usable range is −32 to +31.75 **[derived]**. The conversion
truncates toward zero **[derived]**; a value outside the byte's range (e.g. the BFG's −40) has no
defined result in the source and depends on the compiler **[derived]**. View angles go as 16-bit
angles; the blend as four bytes of 0 to 1 × 255 **[read]**.

**Gun offset and angles** (also the server's) **[read]**:
- Gun roll = horizontal speed × bob fraction × 0.005; gun yaw = horizontal speed × bob fraction ×
  0.01; both negated on odd strides. Gun pitch = horizontal speed × bob fraction × 0.005 (no flip).
  These constants are fixed, not settings, and have no crouch multiplier.
- Then a lag on turning: for each of pitch, yaw and roll, the change from last frame's view angle
  to this frame's (old minus new), wrapped into −180 … 180 and clamped to −45 … 45, adds 0.2 × that
  change to the same gun angle; the yaw change also adds 0.1 × itself to the gun roll. Turning right
  therefore swings the gun's yaw and roll to trail behind **[derived]**.
- Gun offset = 0 plus the developer settings gun_x (right), gun_y (forward) and gun_z (down: the
  value is subtracted along the up vector), all 0 by default. There is no bob height on the gun
  offset itself (it moves with the view origin, which has the bob) **[read]** / **[derived]**.

## 9. On the client

- **View origin** **[read]**: with prediction on (cl_predict, default 1) and the server not
  forbidding it: the predicted origin + the view offset interpolated linearly between the previous
  and the current server frame by the client's fraction into the frame, − (1 − fraction) × the
  prediction error. Without prediction (or in demos): the server origin + view offset, both
  interpolated together. If the origin jumped more than 256 units on any axis between frames, no
  interpolation (teleport). Finally 1/16 unit is added on each axis (the protocol's origin is in
  eighths).
- **Prediction error**: the gap between where the server put the player and what the client had
  predicted for that command, faded out over the interpolation; a gap over 80 units (summed over
  the axes) is treated as a teleport and dropped **[read]**.
- **View angles** **[read]**: while alive or spectating, the client's own predicted angles (its
  input plus the server's angle offset); when dead, gibbed or frozen, the server's view angles
  interpolated by the shortest way round. Then the **kick angles, interpolated** (shortest way
  round) between the two frames, are **added to the view angles**.
- The kick origin is not separate on the client: it already sits inside the view offset **[derived]**.
- **Field of view**: interpolated between frames **[read]**. The server takes it from the player's
  "fov" setting (default 90), 90 if below 1, capped at 160, or forced to 90 in deathmatch with the
  fixed-field-of-view dmflag (value 32768) **[read]**.
- **Blend**: not interpolated, taken from the latest frame **[read]**.
- **Gun model** **[read]**: origin = the final view origin + the gun offset interpolated between
  frames; angles = the final view angles (so including all kick angles) + the gun angles
  interpolated (shortest way round). The animation frame is interpolated except right after a
  weapon change. Not drawn when the setting **cl_gun** is 0 (default 1) or the field of view is
  over 90. With **hand** 2 (centre) the renderer does not draw it; with hand 1 (left) the GL renderer
  draws it mirrored; hand also moves where shots start on the server (left mirrors the side offset,
  centre zeroes it).
- **No client-side bob, sway or roll**: the client has no bob setting (no cl_bob), no idle sway and
  no roll of its own; every bob and sway term comes from the server through the view offset, kick
  angles and gun angles **[read]** (none found in the client).
- **Stair smoothing (prediction only)** **[read]**: after predicting, the client compares the
  predicted height with the one predicted one command earlier. If the player is on the ground and
  rose by more than 63/8 and less than 160/8 units (so from 8 units up to just under 20 **[derived]**),
  it stores that rise and a start time set half a frame in the past. On every rendered frame
  within **100 ms** (real time) of that start, the view is lowered by rise × (100 − elapsed ms) / 100,
  a linear catch-up over 100 ms. Upward steps only; no smoothing going down **[read]**. (The movement
  code's step-up height is 18 units **[read]**.)
- **Dead view**: as in question 8 (roll 40, pitch −15, yaw toward the killer), sent by the server and
  interpolated by the client; the view height is −2 (8 when gibbed) **[read]**.

## 10. Which settings change live, and their ranges

- **sv_rollspeed** (200), **sv_rollangle** (2), **run_pitch** (0.002), **run_roll** (0.005),
  **bob_up** (0.005), **bob_pitch** (0.002), **bob_roll** (0.002), **gun_x**, **gun_y**, **gun_z**
  (0 each) are server-side game-library settings registered with no flags: not saved to the config,
  not shown in the server info, not latched **[read]**. Their current values are read on every
  server frame, so a change takes effect on the next frame **[read]** / **[derived]**.
- None is range-checked: any number is accepted, negatives flip the effect's direction
  **[read]** / **[derived]**. The only bound in the formulas is bob height's 6-unit cap, the
  sv_rollangle cap, the damage kick's 50 cap and the view-offset box clamp **[read]**. An
  sv_rollspeed of 0 is safe (the roll is then always sv_rollangle) **[derived]**; a negative
  sv_rollspeed would divide by a negative and flip the roll **[derived]**. Remember sv_roll* affect
  only the model others see (question 4) **[read]**.
- None of these is sent to the client; the client sees only their results in the player state
  **[derived]**.
- Not settings (fixed in the stock game) **[read]**: the bob steps and speed thresholds, the 6-unit
  bob cap, the ×4 crouched cycle and ×6 crouched pitch/roll, the damage-kick 0.3 factor, 50 cap and
  0.5 s duration, the blend's 0.2/0.6 clamp and 0.06 decay, the fall's 0.3 s, 40 cap and 0.4 dip
  factor, the weapon kicks, the gun-bob constants, the dead angles, and the view-offset clamps.
- Related player and client settings: **fov** (default 90; server clamps to at most 160, under 1
  becomes 90), **hand** (0 right, 1 left, 2 centre; the menu resets anything outside 0 to 2 to 0),
  **cl_gun** (default 1), **cl_predict** (default 1) **[read]**. The dmflags values for no falling
  damage (8) and fixed field of view are server settings **[read]**.
