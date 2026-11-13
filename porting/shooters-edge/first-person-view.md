# The classic client's first-person view: clean-room answers (M26e)

Sources read: ezQuake, latest master on GitHub (QW-Group/ezquake-source, cloned November 2026,
with its protocol submodule), called "ezQuake" below; and the original QuakeWorld client in
id Software's 1996-97 Quake source release (GPL-2.0), called "the original" below.

Tags: [read] = stated directly by the source; [derived] = worked out from what the source does
(arithmetic, or the combined effect of several parts); [measured] = none in this document (nothing
was run).

Conventions used in the answers (both clients) [read/derived]: angles in degrees; pitch positive
means looking DOWN (the forward vector's vertical part is minus the sine of pitch); yaw is
counter-clockwise seen from above; a positive roll puts the view's RIGHT side DOWN (with roll r
and level pitch, the right vector's vertical part is minus sine r). "Frame time" means the
client's per-rendered-frame time step: in ezQuake the real frame duration capped at 0.2 s,
multiplied by the demo speed in demo playback and 0 while a demo is paused [read].

Headline correction to the notes [read]: in ezQuake the bob, the strafe roll, the damage kick and
the weapon punch are ALL OFF by default (cl_bob 0, cl_bobcycle 0, cl_bobup 0, cl_rollangle 0,
v_kicktime 0, v_kickroll 0, v_kickpitch 0, v_gunkick 0). The values in the notes (cl_rollangle
2.0, cl_rollspeed 200) are the ORIGINAL client's defaults; ezQuake keeps cl_rollspeed 200 but
sets cl_rollangle to 0.

---

## 1. View height

ezQuake:
- The eye starts at the player's (predicted) origin plus 1/16 unit on each of x, y and z (so the
  eye never sits exactly on a plane) [read].
- Base height [read]: if the server negotiated the ZQuake view-height protocol extension, or the
  FTE "replacement deltas" extension is active, or an NQ demo is playing, the base height is a
  stat the server sends, which an ezQuake server fills from the player entity's view offset
  (its vertical component). Otherwise it is a fixed 22 units. The stat itself is initialised to
  22 on connect.
- Client setting [read]: v_viewheight, default 0. When non-zero it is clamped to -7..+4 units
  and is added to the eye height IN PLACE OF the bob (see 3). The options menu offers it with
  range -7 to 4 in steps of 0.5. There is no cvar called cl_viewheight.
- Dead and gibbed [read]: only without the FTE replacement-deltas extension: if the player state
  carries the gib flag the eye is at origin plus 8 (no v_viewheight, no bob, no step smoothing
  added); else if it carries the dead flag AND health is 0 or less, the eye is at origin minus 16
  (likewise nothing else added). With replacement deltas the server's view-height stat is used
  even when dead, plus the normal adjustments.
- Intermission [read]: the eye is the bare origin, with no 1/16 offset, no view height, no bob.
- Order of the vertical parts in the normal case [read]: origin, plus 1/16, plus base height
  (22 or the stat), plus the height adjustment (bob or v_viewheight), plus the step-smoothing
  offset (question 7); after the angles are set, the weapon punch may move the eye along the
  forward vector (question 6).
- The source contains a routine that would hold the eye within 14 units horizontally and from
  22 below to 30 above the origin, but nothing calls it, so no such clamp applies [read].

Original: fixed 22 units, no stat, no setting [read]. Gib flag: +8; dead flag (any health): -16
[read]. Unlike ezQuake the bob is added to the eye even when dead or gibbed (the bob is added
before and independently of the height choice) [read]. Intermission as ezQuake [read].

## 2. The bob

CLAIM status: the shape is confirmed for both clients; the defaults are confirmed only for the
original; ezQuake has the effect off by default and differs in two details (the clock runs
double, see below; and cl_bobcycle 0 is a guard).

- Defaults [read]: original cl_bob 0.02, cl_bobcycle 0.6, cl_bobup 0.5. ezQuake cl_bob 0,
  cl_bobcycle 0, cl_bobup 0, plus cl_bobhead 0 (ezQuake only, see 3).
- Formula [read]: amplitude = horizontal speed (x and y of the predicted velocity, vertical
  speed NOT counted) times cl_bob; result = 0.3 of the amplitude plus 0.7 of the amplitude times
  the sine of the phase; then clamped to between -7 and +4 units. Confirmed.
- Phase [read]: a private accumulator of time is taken modulo cl_bobcycle and divided by it,
  giving a fraction f of the cycle in [0, 1). If f is below cl_bobup, the phase is pi times
  f divided by cl_bobup (0 to pi, rising); otherwise pi plus pi times (f minus cl_bobup) divided
  by (1 minus cl_bobup) (pi to 2 pi, falling). Confirmed.
- Clock [read]: the accumulator advances by the client's frame time each time the bob is
  computed while on the ground. It is neither server time nor wall time, and it does not
  advance while in the air.
- ezQuake double advance [derived]: with v_viewheight at 0, ezQuake computes the bob twice per
  rendered frame (once for the weapon, once for the eye), so the accumulator advances by two
  frame times per frame; the effective cycle is cl_bobcycle/2 seconds, and the two values in
  one frame differ by one frame time of phase. The original computes it once per frame.
- In the air [read]: when not on the ground the bob is NOT zeroed; the last computed value is
  held (and in ezQuake both uses in that frame get that held value). The original tests the
  movement code's "no ground entity" state; ezQuake tests the predicted on-ground flag.
- cl_bobcycle of 0 or less [read]: ezQuake returns 0 (no bob) when on the ground; the original
  has no guard (division by zero).
- Spectator [read]: 0 in both.
- Dead [read]: no test for death in either; the bob keeps being computed. In ezQuake it is not
  added to the eye when the dead or gib height is used; in the original it is added to the eye
  even then. The weapon is hidden when dead (both), so its bob does not show.
- Intermission [read]: the intermission view does not compute or apply the bob (both).

## 3. Where the bob goes

Original [read]:
- Eye: the full bob is added to the eye's height.
- Weapon: the weapon model's origin is the player origin plus 22 up, plus 0.4 times the bob
  along the view's forward vector, plus the full bob straight up (world z). So it moves forward
  and back by 0.4 bob and up and down by 1.0 bob.
- No effect on the weapon's pitch; the weapon's angle-lag routine computes zero in all cases
  [derived: both of its offsets are a value minus itself], so the weapon's yaw equals the view's
  yaw and its pitch the negative of the view's pitch.

ezQuake [read]:
- Eye: with v_viewheight 0 and cl_bobhead 0, the eye gets the bob (the second computation) in
  full. With v_viewheight non-zero, the eye gets the clamped v_viewheight and NO bob.
  With cl_bobhead 1, the eye gets the adjustment above PLUS the first bob value.
- Weapon: the weapon's origin starts at the finished eye position (so it inherits every vertical
  eye offset, bob included) and is then moved along the view's forward vector by 0.4 times the
  sum of the weapon bob and the eye's height adjustment. With cl_bobhead 1 the weapon bob term is
  zero, so only 0.4 times the eye adjustment remains. [derived] At defaults with v_viewheight 0
  this forward push is 0.4 times (bob one plus bob two), about 0.8 bob; a non-zero v_viewheight
  also pushes the weapon forward by 0.4 times its value (the source notes this was kept for
  compatibility).
- No straight-up bob term on the weapon beyond what it inherits from the eye; no bob on the
  weapon's angles.
- Other weapon placement settings [read]: r_viewmodeloffset (empty by default; three numbers:
  right, down, forward in units), r_viewmodelsize (default 1); with scr_fovmode 0 the weapon is
  raised 1 unit at viewsize 110 or 2 at 100 (only with the classic status bar, cl_sbar 1, and
  the old HUD), 1 at viewsize 90, 0.5 at 80. The original applies the same viewsize raises
  (110: +1, 100: +2, 90: +1, 80: +0.5) unconditionally. ezQuake defaults: viewsize 100, cl_sbar 0.

## 4. The strafe roll

CLAIM status: the target formula is confirmed; ezQuake adds clamps and smoothing.

- Target [read]: side = predicted velocity dotted with the view's right vector (computed from the
  player's view angles with roll zero). Magnitude: if |side| is below cl_rollspeed, |side| times
  the roll angle divided by cl_rollspeed; otherwise the roll angle. Sign of side.
- Sign [derived]: moving to the player's right gives a positive side speed and a positive roll,
  and a positive roll puts the right side of the view down. So strafing right rolls the view
  right side down (leaning into the strafe).
- Defaults [read]: original cl_rollangle 2.0, cl_rollspeed 200. ezQuake cl_rollangle 0,
  cl_rollspeed 200, cl_rollalpha 20.
- ezQuake's roll angle [read]: in demo playback, as a spectator, or under a ruleset that does not
  restrict it, the absolute value of cl_rollangle is used (a negative setting acts as positive), while
  under a restricting ruleset (see 12) cl_rollangle is clamped to 0..5. The result is capped at
  45 degrees.
- ezQuake smoothing [read]: the applied roll moves toward the target at a constant rate of
  cl_rollalpha times the roll angle (that angle clamped to 2..45) degrees per second, stopping
  at the target. [derived] Default: 20 times at least 2, so at least 40 degrees per second, and
  cl_rollalpha 0 freezes the applied roll at its current value (0 from the start), which turns
  the player's own roll off. The original applies the target immediately, no smoothing.
- Order [read]: roll is added after the view angles are copied, before the damage kick (both).
- Dead [read]: the roll is still computed, but when dead the final roll is overwritten with 80
  (see 10), so it does not show. Intermission: not applied.

## 5. The damage kick and the flash

From the server's damage message (armour byte, blood byte, source position) [read]:

- Count: half the blood plus half the armour, at least 10. [derived] Range 10 to 255 (bytes).
- Direction: from the player's predicted origin (not the eye) to the source, normalised
  (ezQuake uses an approximate normalisation; the original an exact one).
- Roll kick = count times (direction dot the view's right vector) times v_kickroll.
  Pitch kick = count times (direction dot the view's forward vector) times v_kickpitch.
  [derived] A hit from the right rolls the view right side down; a hit from in front pitches the
  view down (positive pitch).
- Defaults [read]: original v_kicktime 0.5 s, v_kickroll 0.6, v_kickpitch 0.6. ezQuake all three
  0, so no kick by default. ezQuake's menu offers each 0 to 10 in steps of 0.5.
- Decay [read]: a timer is set to v_kicktime. Each frame while the timer is above zero, roll and
  pitch get the kick times (timer divided by v_kicktime), and then the timer drops by the frame
  time. [derived] Linear fade from the full kick (the first frame shows it in full) to zero over
  v_kicktime seconds. A new damage message REPLACES the kick and restarts the timer (no
  accumulation). Added after the strafe roll (both).
- Flash [read]: the damage tint's strength (in "percent" units out of 255) gains 3 times the
  count, clamped to 0..150; colour (200,100,100) if armour exceeds blood, (220,50,50) if there was
  any armour otherwise, else (255,0,0). It fades at 150 units per second. Its alpha is the
  strength times gl_cshiftpercent (default 100) divided by 100, divided by 255. [derived] Peak
  150 gives alpha about 0.59 at defaults in the original.
- ezQuake extras [read]: after the gain and clamp, the whole strength is multiplied by
  v_damagecshift (default 0.2, clamped 0..1); the tint is drawn only when gl_polyblend (default 1)
  and gl_cshiftpercent are non-zero; the status bar face shows pain for 0.2 s (also in the
  original); a "hurt blur" lasts count/24 seconds.

## 6. The punch (weapon recoil)

- What the server sends [read]: no angle value; two messages, "small kick" and "big kick".
- Original [read]: small sets the punch to -2, big to -4, applied as pitch (pitch plus punch,
  i.e. tipping the view UP). Each frame, before the view is built, the punch is reduced by 10
  times the frame time and anything below zero is set to zero. [derived] Because the punch is
  negative, this zeroes it on the first frame after the message, before it is drawn: the
  original client shows no punch at all.
- ezQuake [read]: small sets a TARGET of -2, big a target of -4. Each frame (before the view is
  built) the punch moves toward the target at 20 degrees per second (when the target is -2) or
  40 (otherwise); on reaching it the target becomes 0 and the punch then returns toward 0 at
  20 degrees per second. [derived] Small: 0 to -2 in 0.1 s, back in 0.1 s. Big: 0 to -4 in
  0.1 s, back in 0.2 s. Linear ramps, no other smoothing; the client decays it, the server sends
  only the trigger.
- ezQuake application [read]: only when v_gunkick is non-zero (default 0, a menu on/off switch, and
  its value is not used as a scale). The eye is moved along the view's forward vector by the
  punch (a negative punch moves it back: up to 2 or 4 units) and the pitch gets half the punch
  (up to 1 or 2 degrees up). Applied after the idle sway and before the dead roll. The weapon
  model, placed from the finished eye, moves and pitches with it.

## 7. Step smoothing

Original [read]:
- Only when on the ground AND the origin is higher than the smoothed height: the smoothed height
  rises at 80 units per second toward the true one, never more than 12 units below it; the eye
  and the weapon are both lowered by the difference. Otherwise (in the air, level or going down)
  the smoothed height snaps to the true height immediately.

ezQuake [read]:
- On the ground and rising: the smoothed height rises at 100 units per second. If the rise since
  the smoothed height exceeds 20 units in one frame (steep stairs), the rate becomes 160 units per
  second and an extra lag allowance is granted: the rise minus 200 times the frame time minus 15,
  at most 5 units (only set when the rate was not already 160). The maximum lag is 15 units plus
  that allowance; the allowance decays at 200 units per second to 0.
- In the air, level or going down: the smoothed height snaps to the true height, but the eye
  offset does NOT snap: it returns toward zero at 150 units per second; the rate resets to 100
  and the allowance to 0. [derived] So a lag left at the top of a stair eases out over up to
  0.1 s rather than jumping.
- Reset to zero at once: on a teleport or respawn (the prediction's no-interpolation flag) or
  when the origin moved 48 units or more since the previous frame; also cleared in intermission.
- Going down: no smoothing of descents (both).
- Weapon: moved too, since it is placed from the eye (both). In ezQuake the lightning beam's
  start from the player follows the same offset.
- It runs every client frame, after movement prediction.

## 8. Idle sway

- Defaults (both) [read]: v_idlescale 0 (off), v_iyaw_cycle 2, v_iroll_cycle 0.5,
  v_ipitch_cycle 1, v_iyaw_level 0.3, v_iroll_level 0.1, v_ipitch_level 0.3.
- Formula [read]: roll gains v_idlescale times sine(client time times v_iroll_cycle) times
  v_iroll_level; pitch and yaw likewise with their own cycle and level. Client time is the
  client's game clock in seconds and the sine takes radians, so the "cycle" values are angular
  rates in radians per second, not periods [derived: period 2 pi / cycle, e.g. yaw about 3.14 s,
  roll about 12.6 s, pitch about 6.28 s]. Added after the strafe roll and damage kick.
- Intermission [read]: the sway is always applied with scale 1, whatever v_idlescale is (both).
- Weapon [read/derived]: ezQuake's weapon copies the view angles, so it sways with the view. The
  original subtracts the sway from the weapon's angles but then overwrites the weapon's angles,
  so in effect the weapon's yaw and pitch follow the view including the sway and its roll is 0.
- ezQuake guard [read]: when the game directory is "fortress", the client resets all seven to the
  defaults on connect, and a player cannot change them while connected (only the server can,
  which Team Fortress uses for its concussion effect).

## 9. Landing

No view dip, kick or bob on landing in either client [read: nothing in the view code reacts to
landing]. The only vertical eye effects are the bob, step smoothing (rising only) and, in
ezQuake, the 150 units per second ease-out described in 7. Any landing sound or damage is the
server's.

## 10. Other view effects at default settings

- Dead roll [read]: dead (ezQuake: dead flag and health 0 or less, or under replacement deltas a
  locked movement type with health 0 or less; original: the dead flag alone, which a gib also
  sets) sets the final roll to exactly 80 degrees, overriding strafe roll, damage kick and sway.
  The eye lowering when dead is in 1.
- Weapon hidden [read]: when dead or gibbed, or health 0 or less (ezQuake), the weapon is not
  drawn. ezQuake also hides it with r_drawviewmodel 0 (default 1), with r_drawviewmodel 2 when
  fov is above 90, and while invisible unless r_drawviewmodel_invisible is 1 (default 0).
- Pitch drift [read]: both clients can drift the pitch back to level after the centre-view
  trigger or lookspring; the drift starts after the forward move has been at least 200 for
  v_centermove (0.15 s) and accelerates at v_centerspeed (500 degrees per second per second).
  Only on the ground, never in demo playback. It is not active by default unless a player uses
  lookspring.
- Underwater, slime, lava [read]: a full-screen colour tint by contents. ezQuake: water
  (130,80,50) strength 128, slime and inside-solid (0,25,5) 150, lava (255,80,0) 150, scaled by
  v_contentblend (default 0.2) and gl_cshiftpercent. The original uses the same colours with
  v_contentblend default 1 acting as an on/off switch. No underwater view warp or field-of-view wobble was found in
  ezQuake, while the original's software renderer has r_waterwarp (default 1), a picture warp only, not a
  change of the eye or angles.
- Powerup tints (ezQuake) [read]: quad blue strength 30, suit green 20, ring grey 100, pentagram
  yellow 30, each times its own setting (default 0.5 each).
- Item pickup flash (both) [read]: colour (215,186,69) strength 50, fading at 100 per second, and
  ezQuake shows it only with cl_bonusflash (default 0), the original always.
- No forward or back weapon movement by speed exists beyond the bob's forward term (3).
- No view effect for swimming beyond the tint (both) [read].

## 11. Field of view

- Default [read]: fov 90 in both. ezQuake clamps fov to 10..140 (also has default_fov, 90, which
  a server's fov change of exactly 90 restores; in demo playback server fov changes are ignored).
  The original clamps to 10..170.
- Original [read]: the fov is the horizontal angle of the 3D view rectangle at any aspect; the
  vertical is derived from the rectangle's width and height. [derived] Wider windows lose vertical
  view.
- ezQuake [read]: for a window at least as wide as 4:3 (width/4 not less than height/3), the fov
  is taken as the horizontal angle a 4:3 screen would have; the matching 4:3 vertical angle is
  derived, kept fixed, and the horizontal is widened to the real aspect from it (horizontal plus
  scaling). For a window narrower than 4:3 the fov is the horizontal and the vertical follows
  from the window. The window size used is the whole video mode, not the reduced view rectangle,
  except that when the view rectangle is shortened by the status bar (viewsize below 120 with
  the bar showing) the vertical is recomputed from the rectangle, cropping it; scr_fovmode 1
  instead letterboxes. Limits after the conversion: horizontal 10..165, vertical 10..140, each
  recomputing the other when hit.
- [derived] fov 90: the 4:3 vertical is about 73.74 degrees; at 16:9 the horizontal becomes about
  106.26 degrees; at 16:10 about 100.39 degrees.

## 12. Server forcing, cheat protection and clamps

ezQuake [read]:
- Rulesets (the ruleset setting: default, smackdown, qcon, thunderdome, mtfl, smackdrive).
  Under smackdown, qcon, thunderdome and smackdrive, cl_rollangle is clamped to 0..5 while
  playing (not in demo playback or as a spectator). Under smackdown, qcon and smackdrive,
  cl_rollalpha is forced to 20 and made read-only. The default and mtfl rulesets do not restrict
  the roll.
- Team Fortress (game directory "fortress"): the idle-sway settings are reset and locked against
  player changes while connected; the v_cshift command from the player is ignored; fov is under
  the server's control for the concussion effect.
- No server info key forbids or forces cl_bob, cl_bobcycle, cl_bobup, cl_rollangle,
  cl_rollspeed, the kick settings, v_gunkick or v_viewheight; none of these is flagged as a cheat
  setting. Not found in the source read.
- Client clamps: v_viewheight -7..4 (applied where it is used); fov 10..140 (and the 165/140
  limits in 11); the bob's result -7..+4 (cl_bob, cl_bobcycle, cl_bobup themselves are not
  clamped; cl_bobcycle 0 or less disables); the roll result 45 degrees; v_damagecshift and the
  powerup tint settings 0..1. v_kick settings and cl_rollspeed are not clamped (the menu offers
  0..10 for the kicks and 0..10 for cl_rollangle, but the console accepts any value).

Original [read]: no rulesets and no clamps on any of these except fov 10..170 and the bob's
-7..+4 result.
