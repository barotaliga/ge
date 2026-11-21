# Quake II client: the remaining temporary entities, effect bits, render bits and layout words (answers)

Clean-room reader's answers. Project and release read: Q2PRO, upstream master branch as cloned
(commit dated 2025-12-11). Facts only, in plain words, no code. Each fact is marked [read] (seen
directly), [derived] (worked out from what was read) or [measured]. Nothing here was measured by
running the program; every number is [read] or [derived].

## 0. Shared machinery this release uses (differences from the stock notes)

- Particle model as in our existing note: position = start + velocity × t + accel × t² (no
  one-half on the acceleration term), alpha = start alpha + alpha velocity × t, freed when alpha
  reaches 0 or below, drawn alpha clamped to at most 1. The gravity constant is 40. [read]
- "One-frame" particles: a special alpha velocity of −10000 marks a particle drawn exactly once
  at its start alpha and start position, then freed on the following rendered frame. [read]
- The renderer's particle list is capped at 8192 per frame; particles beyond that are not drawn
  that frame (they stay alive in the client pool). [read]
- Random helpers: frand is uniform 0..1, crand uniform −1..+1 (both from a 32-bit random); r(N) below means a uniform random integer from 0 to N inclusive. [read]
- Positions on the wire: three coordinates, each a signed 16-bit value in 1/8 unit steps; with
  the release's extended entity-state protocol flag, a variable form instead (a 16-bit word whose
  low bit says a third byte follows; the value is the rest, sign-extended to 15 or 23 bits, still
  in 1/8 units). A "dir" is one byte, an index into the 162 vertex normals; an index of 162 or
  more drops the connection with an error. [read]
- A per-particle size scale exists (1.0 by default). [read]
- Dynamic lights from temp entities are keyed: a light allocated with a non-zero key replaces any
  live light with the same key; otherwise the first dead slot is taken, else slot 0 is
  overwritten. A light lives until its "die" time; it is re-added at full radius every rendered
  frame until then (no fade). [read]
- "Sustains": a table of 32 lasting emitters. A slot is free when its id is 0. Every rendered
  frame each live slot runs its emitter if the clock is at or past its next-think time and not
  past its end time; once the clock passes the end time the slot is freed. If all 32 are in use a
  new sustain is silently dropped. [read]

## 1. Bubble trail (11) and bubble trail 2 (41)

Wire, both: two positions, start then end. [read]

Bubble trail (11): [read unless marked]
- One particle every 32 units along the line from start to end, the first at the start; count =
  ceiling(length / 32) (the loop runs while the travelled integer distance is below the length).
  [derived]
- Colour: palette index 4 + r(7) → 4..11 (the grey ramp). [read]
- Position: the line point plus crand × 2 on each axis. [read]
- Velocity: crand × 5 on each axis, plus 6 upward. No acceleration (no gravity). [read]
- Alpha 1, alpha velocity −1 / (1 + frand × 0.2) → life 1.0–1.2 s. [read; life derived]
- No light, no sound. [read]

Bubble trail 2 (41):
- The client draws it with a spacing of 8 units (fixed by the client, not sent). [read]
- Colour 4..11 as above. Position jitter crand × 2 per axis, then 4 units lower. [read]
- Velocity crand × 10 per axis, plus 20 upward. No acceleration. [read]
- Alpha 1, alpha velocity −1 / (1 + frand × 0.1) → life 1.0–1.1 s. [read; life derived]
- Also plays the sound "weapons/lashit.wav" at the start position, volume 1, normal
  attenuation. [read]
- So 41 differs by: 4× denser, faster rise (20 vs 6), wider sideways spread (10 vs 5), a 4-unit
  drop of the start, a slightly shorter maximum life, and the sound. [derived]

## 2. Force wall (37)

Wire: position (start), position (end), one byte colour (a palette index). [read]

Effect: [read unless marked]
- Walk the line in 4-unit steps (about length / 4 steps, the first at the start); at each step a
  particle is made only when frand > 0.3, so about 70 % of steps carry one. [read; 70 % derived]
- Colour: exactly the wire byte (no random offset). [read]
- Position: the step point plus crand × 3 on each axis. [read]
- Velocity: 0 horizontally; vertical −40 − crand × 10, i.e. downward at 30–50 u/s. No
  acceleration. [read]
- Alpha 1, alpha velocity −1 / (3 + frand × 0.5) → life 3.0–3.5 s. [read; life derived]
- No light, no sound. The game resends it to keep the wall visible; the client keeps no state.
  [read; the resend is the game's business, not settled here]

## 3. Steam (40)

Wire, in order: [read]
- short: id (−1 for a one-off puff; otherwise the jet's id)
- byte: count (particles per emission)
- position: origin
- dir byte: direction
- byte: colour (palette index; the client keeps the low 8 bits)
- short: magnitude (speed)
- long (32-bit, milliseconds): duration — present ONLY when the id is not −1.

One emission (shared by the puff and each jet tick): [read unless marked]
- count particles; colour = wire colour + r(7).
- Position: origin plus magnitude × 0.1 × crand on each axis.
- Velocity: direction × magnitude, plus crand × magnitude / 3 along each of two axes
  perpendicular to the direction.
- Acceleration: 20 downward (half the usual gravity constant). [read]
- Alpha 1, alpha velocity −1 / (0.5 + frand × 0.3) → life 0.5–0.8 s. [read; life derived]
- No light, no sound.

The puff (id −1): one emission at once. [read]

The jet (id other than −1): [read unless marked]
- Takes a free sustain slot (of 32); if none is free, nothing happens.
- Stores id, count, origin, direction, colour, magnitude; ends at now + duration; next think =
  now (so the first emission happens on the next rendered frame's pass).
- Each time it runs it emits once and moves its next think 100 ms later → 10 emissions per
  second for the duration, independent of the frame rate. [read; rate derived]
- When the clock passes the end time the slot is freed. A second jet with the same id is not
  merged or replaced; it takes another slot. [read; no duplicate check exists]
- An id of 0 makes the slot look free at once, so the next sustain may overwrite it. [derived]

## 4. The other temporary entities

Welding sparks (25): [read]
- Wire: byte count, position, dir byte, byte colour.
- count particles of exactly the wire colour; position = point + (r(7) − 4) per axis +
  d × dir with d = r(7) (0..7 units out from the wall); velocity crand × 20 per axis; gravity 40 downward; alpha 1, alpha velocity −1/(0.5 + frand × 0.3) → life 0.5–0.8 s.
- Plus a light at the point: radius 100 + r(74) → 100..174, colour (1, 1, 0.3). It is
  carried by a short-lived "flash" record that starts one server frame in the past and is freed
  once one server frame (100 ms at the base 10 Hz rate) has elapsed from that start, so it shows
  for at most one server frame (typically less, as the client clock already sits between the
  previous and the current server frame). Its radius is not faded. [read; timing derived]
- The record carries the model "models/objects/flash/tris.md2" but is flagged as a beam from its
  own origin to the same point, so in practice no model is drawn, only the light. [derived]
- No sound.

Tunnel sparks (29): [read]
- Wire as welding sparks (byte count, position, dir byte, byte colour).
- count particles of exactly the wire colour; position and velocity as welding sparks; the
  acceleration is 40 UPWARD (the particles rise); life 0.5–0.8 s. No light, no sound.

Blue hyperblaster (27): [read]
- Wire: TWO positions (the second is used as the "direction" without normalising it). The
  release treats this number as the old, malformed form and adds a corrected form, number 56,
  whose wire is a position and a dir byte, drawing the same effect.
- Effect: 40 particles; colour 224 + r(7) → 224..231 (the yellow-orange blaster ramp, NOT
  blue); position = point + (r(7) − 4) per axis + d × direction, d = r(15); velocity
  direction × 30 + crand × 40 per axis; gravity 40 down; alpha 1, alpha velocity
  −1/(0.5 + frand × 0.3) → life 0.5–0.8 s. No light, no sound, no impact model.
- Consequence: with a real far-away second position as the "direction", the spawn offsets and
  velocities scale with those coordinates and the puff lands far from the hit. [derived]

Flashlight (36): [read]
- Wire: position, then short entity number (note the order: position first).
- A dynamic light keyed by that entity number (so each new message replaces the last one for that
  entity): at the position, radius 400, colour (1, 1, 1), dying 100 ms after it was made. No
  particles, no sound. The game must resend it each frame to keep the beam lit. [read; the resend
  derived]

Chainfist smoke (45): [read]
- Wire: position only.
- The direction is forced straight up (0, 0, 1). 20 particles; colour 0 + r(7) → 0..7
  (black to dark grey); position = point + 2 × crand per axis (magnitude 20 × 0.1); velocity 20
  upward plus crand × 20/3 along each of two horizontal axes; NO acceleration (no gravity); alpha
  1, alpha velocity −1/(0.5 + frand × 0.3) → life 0.5–0.8 s. No light, no sound.

Dodgeball goal (49): drawn exactly as the teleport effect (48): [read]
- Wire: position only.
- A grid: x offsets −16..16 step 4 (9 values), y offsets −16..16 step 4 (9 values), z offsets
  −16..32 step 4 (13 values) → 1053 particles. [read; count derived]
- Each: colour 7 + r(7) → 7..14 (grey to near white); position = point + offset +
  r(3) per axis; direction = normalised (y offset × 8, x offset × 8, z offset × 8) (note x
  and y swapped); speed 50 + r(63) → 50..113 u/s along it; gravity 40 down; alpha 1, alpha
  velocity −1/(0.3 + r(7) × 0.02) → life 0.30–0.44 s. [read; life derived]
- The particle at offset (0, 0, 0) gets a zero direction and so no velocity. [derived]
- No light, no sound from the client.

Widow beam-out (50): [read]
- Wire: short id (entity), position.
- A sustain: id = the wire id, origin = position, ends 2100 ms after it is received; its next
  think is never advanced, so it emits on EVERY rendered frame while alive (frame-rate
  dependent).
- Each emission: 300 one-frame particles on a sphere around the origin, radius 45 × ratio where
  ratio = 1 − (time left / 2100 ms), growing from 0 to 45 units over 2.1 s; directions uniform on
  the sphere; no velocity, no acceleration; drawn once at alpha 1. Colour picked at random from
  four indices: 16, 104, 168, 144. No light, no sound.
- If the sustain table is full the effect is dropped; an id of 0 makes it free at once.
  [read; the id-0 consequence derived]

Widow splash (52): [read]
- Wire: position only.
- 256 particles placed on a sphere of radius 45 round the point (uniform directions), moving
  outward at 40 u/s, no acceleration; colours from the same four: 16, 104, 168, 144; alpha 1,
  alpha velocity −0.8/(0.5 + frand × 0.3) → life 0.625–1.0 s. No light, no sound. [read; life
  derived]

Power splash (60): [read]
- Wire: short entity number, byte count. The count is read but not used by the effect.
- An entity number at or above the map's entity limit drops the connection with an error. If the
  entity is not in the current frame, nothing is drawn.
- Centre = the entity's current origin + the middle of its box (from the solid field); radius =
  half the box's diagonal (0 for an entity without an encoded box).
- 256 particles on a sphere of that radius round the centre, moving outward at 40 u/s, no
  acceleration; colour picked from 208, 209, 210, 211 (greens); alpha 1, alpha velocity
  −1/(0.5 + frand × 0.3) → life 0.5–0.8 s. No light; no sound from the client. The power screen
  and the power shield get the same picture (the release does not tell them apart).

## 5. Damage dealt (128)

Wire: one signed short, the damage (the "count"). [read]

What the client does: [read unless marked]
- Only when the count is above 0 AND the setting `cl_hit_markers` is above 0: it notes the
  moment (the real-time clock, not the game clock) and the count.
- When `cl_hit_markers` is above 1 it also plays "weapons/marker.wav" on the listener, full
  volume, no attenuation (heard at full level), on the listener entity's channel number 257 (a channel outside the usual 0..7 range; what that number does in the mixer was not traced).
- `cl_hit_markers` default 2 (marker and sound); 1 = marker only; 0 = neither. [read]
- The marker picture is "marker" from the pics folder (optional; when absent, no marker is
  drawn). It is drawn centred on the screen, offset by the crosshair offsets `ch_x` and `ch_y`
  (default 0), scaled by the crosshair scale `ch_scale` (default 1, clamped 0.1–9), as a full
  stretch of the picture. [read]
- Colour: red (255, 0, 0), alpha = 1 − f², where f = time since the hit / `scr_hit_marker_time`; so it fades slowly at first and fast at the end. [read]
- `scr_hit_marker_time`: default 500 (milliseconds). At 0 or below, no marker. Once the time
  since the hit exceeds it, the marker is cleared. [read]
- The marker is drawn as part of the crosshair: only when `crosshair` is non-zero (default 0)
  and the layouts stat does not ask to hide the HUD or the crosshair. So with the default
  crosshair off, the sound plays but no marker shows. [read; the consequence derived]
- A new hit restarts the fade from full. The damage number itself is not shown. [read]

## 6. Effect bits (the entity's effects)

Bit values, as the release defines them (they are wire facts): FLAG1 2^18, FLAG2 2^19,
IONRIPPER 2^20, GREENGIB 2^21, BLUEHYPERBLASTER 2^22, SPINNINGLIGHTS 2^23, PLASMA 2^24, TRAP 2^25,
TRACKER 2^26, DOUBLE 2^27, SPHERETRANS 2^28, TAGTRAIL 2^29, HALF_DAMAGE 2^30, TRACKERTRAIL 2^31,
BOB 2^2. [read]

### 6.0 How a trail's line is kept in this release (applies to every trail below)

- Each entity keeps a "trail point". A trail runs from the trail point to this rendered frame's
  interpolated origin. It places whole steps only: count = floor(distance / step); with count 0 it
  does nothing and leaves the trail point where it was, so short moves accumulate until a whole
  step is reached. After placing, the trail point is moved to just past the last step placed (the
  remainder carries to the next frame). [read]
- Any entity for which no trail-placing branch ran this frame (including branches that only add a
  light, and every early exit) has its trail point set to this frame's interpolated origin.
  [read]
- When an entity first appears, its trail point is its sent previous origin (or its current
  origin after a teleport event or for a beam), and its trail counter is set to 1024. [read]
- Per-step particle cap: if the pool runs out the trail stops for that frame. [read]

### 6.1 Branch order (confirmed)

One branch per entity per frame, first match wins: rocket, blaster, hyperblaster, gib, grenade,
flies, BFG, trap, flag 1, flag 2, tag trail, tracker trail, tracker, green gib, ion ripper, blue
hyperblaster, plasma. This matches our note. [read]

Before the branches (all [read]):
- The block is entered only when at least one of those 17 bits is set (ROTATE alone, SPINNING-
  LIGHTS alone, SPHERETRANS alone and so on never enter it).
- The player's own entity in first person (not third person) never reaches the branches: it gets
  only a light at its origin, radius 225: FLAG1 → (1, 0.1, 0.1); else FLAG2 → (0.1, 0.1, 1); else TAGTRAIL → (1, 1, 0); else TRACKERTRAIL → (−1, −1, −1) (a darkening light). Nothing is
  drawn for it.
- An entity with model index 0 (invisible) is skipped before the branches: no trail, no light.
- With the setting `cl_gibs` at 0 (default 1), entities with GIB or GREENGIB are not drawn at all
  and get no trail.
- Combined bits inside a branch: BLASTER + TRACKER = the green blaster trail; HYPERBLASTER +
  TRACKER = a green light; TRACKERTRAIL + TRACKER = the pulsing dark light (below).

### 6.2 Each bit

FLAG1 / FLAG2 (other entities, and the own entity in third person): [read unless marked]
- One particle every 5 units. Colour exactly 242 (flag 1, red) or 115 (flag 2, blue).
- Position: step point + crand × 16 per axis (a wide, fuzzy column). Velocity crand × 5 per axis.
  No acceleration.
- Alpha 1, alpha velocity −1/(0.8 + frand × 0.2) → life 0.8–1.0 s. [read; life derived]
- Light radius 225; flag 1 (1, 0.1, 0.1), flag 2 (0.1, 0.1, 1).

TAGTRAIL: [read]
- Exactly as the flag trail with colour 220; light radius 225, colour (1, 1, 0).

IONRIPPER: [read unless marked]
- One particle every 5 units, placed exactly on the line (no position jitter).
- Colour 228 + r(3) → 228..231.
- Start alpha 0.5 (not 1); alpha velocity −1/(0.3 + frand × 0.2) → life 0.15–0.25 s. [read; life derived]
- Velocity: 10 or −10 along the WORLD x axis, alternating particle by particle (not relative to
  the flight direction); y and z 0. No acceleration. The alternation's phase is kept in the
  entity's trail counter and carries across frames.
- Light radius 100, colour (1, 0.5, 0.5).

GREENGIB (the "diminishing" trail in its green-gib form): [read unless marked]
- One step every 0.5 units. At each step a particle is made only when r(1023) is below the
  entity's trail counter; the counter starts at 1024 (always), drops by 5 per step, never below
  100. So the trail thins from every step to about 1 step in 10 after 185 steps (92.5 units of
  travel). [read; distances derived]
- Spread, fixed for the whole call by the counter at the call's start: counter above 900 →
  position jitter crand × 4 and velocity crand × 15; above 800 → × 2 and × 10; otherwise × 1 and
  × 5.
- Colour 219 + r(7) → 219..226.
- Velocity as above, then 40 subtracted from the vertical (a downward start); no acceleration.
- Alpha 1, alpha velocity −1/(1 + frand × 0.4) → life 1.0–1.4 s.
- No light. The model is drawn normally.

BLUEHYPERBLASTER: no trail. Light radius 200, colour (0, 0, 1). [read]

SPINNINGLIGHTS (not a trail branch; handled with the angles, every frame): [read unless marked]
- The model's angles are replaced: pitch 0, roll 180 (drawn upside down), yaw = (client time in
  ms / 2) wrapped to 0..360 plus the entity's own sent yaw → 500 degrees a second, a turn in
  0.72 s. [read; rate derived]
- A red light, radius 100, colour (1, 0, 0), at the point 64 units ahead along that spinning yaw
  (horizontal). The light is added whatever other branch runs.

PLASMA: [read]
- The model is drawn translucent at alpha 0.6.
- Trail only when ANIM_ALLFAST is also set: the yellow blaster trail (one particle every 5 units,
  colour 224, jitter crand × 1, velocity crand × 5, no acceleration, life 0.3–0.5 s).
- Light radius 130, colour (1, 0.5, 0.5).

TRAP: [read unless marked]
- The effect's origin is the entity's origin raised 32 units (the light uses the raised point; the model is not moved).
- Particles are emitted at most once per 10 ms of client time (frame-rate limited above 100 fps).
- Each emission: (a) a rising column from 14 below to 50 above the raised point, a particle every
  5 units (13 particles); colour 224; jitter crand × 1; velocity crand × 15 per axis; acceleration 40 UPWARD; alpha 1, alpha velocity −1/(0.3 + frand × 0.2) → life 0.3–0.5 s.
  (b) 8 particles from a 2 × 2 × 2 grid of offsets ±2 on each axis; colour 224 + r(3); position = raised point + offset + r(23) × crand per axis; direction = normalised
  (y offset, x offset, z offset) (x and y swapped); speed 50 + r(63); gravity 40 down; alpha velocity −1/(0.3 + r(7) × 0.02) → life 0.30–0.44 s. [read; counts derived]
- Light: radius 100 + r(99) → 100..199, a fresh random every rendered frame (flicker); colour (1, 0.8, 0.1).
- The trail point is reset every frame (no carried line).

TRACKER (alone, i.e. not with BLASTER, HYPERBLASTER or TRACKERTRAIL): [read unless marked]
- One particle every 3 units. Colour 0 (black). Alpha 1, alpha velocity −2 → life 0.5 s.
- Position: the step point displaced along the "up" vector of the flight direction by
  ±8 × cos(d × π / 64), where d is the step point's position dotted with the flight direction
  (a world-anchored wave of period 128 units along the line); the sign alternates particle by
  particle, the phase carried across frames in the trail counter. So two black strands weave
  about the line. [read; the wave period derived]
- Velocity (0, 0, 5) (a slow rise), no acceleration.
- Light radius 200, colour (−1, −1, −1): it darkens.

TRACKERTRAIL: [read unless marked]
- With TRACKER also set: no particles; a darkening light (−1, −1, −1) whose radius pulses as
  50 + 500 × (sin(client ms / 500) + 1) → 50..1050, period about 3.14 s. [read; period derived]
- Without TRACKER: a "shell" of 300 one-frame black particles (colour 0) every rendered frame on
  a sphere of radius 40 round the origin, uniform directions (with the release's extended
  protocol: round the box's centre, radius = half the box diagonal, count 300 × clamp(radius/40,
  1, 2), particles scaled by the same factor). Light radius 155, colour (−1, −1, −1).
- Without TRACKER the model itself (and its linked models) gets a render bit that makes the GL
  renderer draw it in pure black (no light at all). [read]

SPHERETRANS: not a trail. The model is drawn translucent at alpha 0.3, or 0.6 when TRACKERTRAIL
is also set. [read]

BOB (a newer-protocol bit; the release applies it whenever it is set): the drawn origin (and the
old origin) is raised by 5 × sin(client ms / 400): amplitude 5 units, period about 2.51 s, one
phase for every entity. The setting `cl_nobob` (default 0) turns it off. No trail, no light.
[read; period derived]

Order of the non-trail changes, for completeness: frame override from the ANIM bits; then QUAD,
PENT, DOUBLE and HALF_DAMAGE are turned into the colour-shell bit plus their shell render bits; then the angles (ROTATE, else SPINNINGLIGHTS, else interpolated); then the BFG (alpha 0.3), PLASMA
(0.6) and SPHERETRANS (0.3/0.6) alphas in that order, later ones overriding; then an alpha sent
by the newer protocol overrides all of them. [read]

## 7. Render bits

Values: MINLIGHT 2^0, FULLBRIGHT 2^3, CUSTOMSKIN 2^8, IR_VISIBLE 2^15, USE_DISGUISE 2^18,
SHELL_LITE_GREEN 2^19, CUSTOM_LIGHT 2^20; the goggles view bit is bit 2^2 of the player state's
view flags. Against a server that does not speak the release's extended protocol, the client
clears every render bit from 2^19 upward on arrival (so SHELL_LITE_GREEN and CUSTOM_LIGHT exist
only on the extended protocol). [read]

How the GL renderer picks a model's light colour, in this priority (first match wins): [read]
1. any shell bit → the shell colour (below); 2. FULLBRIGHT → (1, 1, 1); 3. IR_VISIBLE AND the view has the goggles bit → (1, 0, 0) pure red; 4. the tracker black bit (section 6) → (0, 0, 0); 5. otherwise the light sampled at the origin from the lightmap (plus dynamic lights), then
   MINLIGHT, then the GLOW pulse, then each channel clamped to 0..1.
Alpha is the entity's alpha when it is translucent, else 1.

FULLBRIGHT: shade colour (1, 1, 1), ignoring the lightmap and dynamic lights. [read]

MINLIGHT: after sampling, if the light's length (the RGB vector's length) is 0 the colour
becomes (0.1, 0.1, 0.1); if it is below 0.1 it is scaled up to length 0.1 keeping its hue; otherwise unchanged. [read]

IR_VISIBLE and the goggles: an entity carrying IR_VISIBLE is drawn flat pure red (1, 0, 0)
whenever the local view has the goggles view bit; without the goggles the bit does nothing. A
shell or FULLBRIGHT on the same entity wins over the red. [read] (Any full-screen tint the goggles
add is not part of this answer.)

SHELL_LITE_GREEN: a shell colour (0.56, 0.93, 0.56). The shell colour is built in this order:
start black; LITE_GREEN sets (0.56, 0.93, 0.56); HALF_DAMAGE then sets (0.56, 0.59, 0.45); DOUBLE then sets red 0.9 and green 0.7 (blue kept); then RED forces red to 1, GREEN forces green
to 1, BLUE forces blue to 1. So shells MIX in this release: e.g. quad (blue) + power shield
(green) is cyan (0, 1, 1), pent + quad is magenta (1, 0, 1), red + green + blue is white. [read; the mixes derived] The client sets LITE_GREEN itself (with the shell) for an entity whose extra
effect bits carry the "dual fire" flag (extended protocol). In the mission pack's game folder
("rogue") only, the client first rewrites some shell combinations: half damage is dropped if red,
blue or double is present; double is dropped if red, blue or green is present, and red + double
adds blue (purple), blue + double adds green (cyan), or removes blue if green is already there.
[read]

CUSTOMSKIN (extended protocol only): the entity's skin number is an index into the image
precache list (a configstring-backed list of picture names); when the index is in range, that
image is used as the skin and the skin number is reset to 0. For a "flare" entity the FRAME field
is the image index instead (default flare picture when absent). [read]

CUSTOM_LIGHT (extended protocol only): the entity is not drawn at all; instead a dynamic light is
added at its interpolated origin, radius = 64 + the entity's frame field, colour = the skin number
read as four bytes, big-endian, the first three being red, green, blue in 0..255 (each divided by
255); a skin number of 0 means white. [read; the 64 is the release's dynamic-light cut-off
constant]

USE_DISGUISE: only on player models (model index 255). The skin is replaced with
"players/<the player's model name>/disguise.pcx" (registered on the fly; the model stays the
player's own). [read]

## 8. The layout language: `rstring`, `rstring2` and `color`

These three words are the release's own additions to the layout language (the status bar
program and the layout the server sends); they are accepted on every protocol. [read]

`rstring` <text>: [read]
- One argument, the next token (quote it to include spaces).
- Drawn right-aligned so that the text ENDS at the current cursor x (set by `xl`, `xr` or `xv`)
  at the current cursor y: it starts at x − (number of characters × 8). Characters are 8 × 8
  pixels in the HUD's own scaled coordinates. The width is counted in bytes, one line only (no
  splitting at newlines, unlike `cstring`). [read]
- In the normal colour (the font's plain glyphs).
- Reminder of the cursor words, for the point: `xl n` → x = n; `xr n` → x = HUD width + n; `xv n` → x = HUD width / 2 − 160 + n; `yt n` → y = n; `yb n` → y = HUD height + n; `yv n` → y = HUD height / 2 − 120 + n. [read]

`rstring2` <text>: as `rstring` but in the alternate colour: every character's code has its top
bit flipped (code XOR 128), so plain text uses the second half of the font (the green/alternate
glyphs) and text that already carried the top bit flips back to plain. [read]

Related forms (same placement rules): `stat_rstring` and `stat_rstring2` take a stat index; the
stat's value is a configstring index whose text is drawn the same way. An out-of-range stat index
or configstring index drops the connection with an error. [read]

`color` <colour>: [read]
- One argument. Accepted forms:
  - `#rgb` (3 hex digits, each doubled: `#f80` = 255, 136, 0), alpha 255; - `#rrggbb` (6 hex digits), alpha 255; - `#rrggbbaa` (8 hex digits) with its own alpha; - a name: `black`, `red`, `green`, `yellow`, `blue`, `cyan`, `magenta`, `white`; - an index 0–7 meaning the same eight in that order.
  Hex digits are case-insensitive. [read: names and indices; derived: case from a hex-digit
  helper — not checked letter by letter]
- The names `alt` and `none` and indices 8 or more are refused; any other text, a wrong number of
  hex digits, or a non-hex digit is refused. A refused colour changes nothing (the previous colour
  stays) and prints nothing. [read]
- Pure colours: red (255, 0, 0), green (0, 255, 0), yellow (255, 255, 0), blue (0, 0, 255), cyan
  (0, 255, 255), magenta (255, 0, 255), white (255, 255, 255), black (0, 0, 0). [read]
- The colour's alpha is multiplied by the HUD alpha setting `scr_alpha` (default 1). [read]
- It tints EVERYTHING drawn after it in that program (strings of every kind, pictures, numbers,
  the field picture), multiplying the glyph or picture colour. [read]
- It lasts until another valid `color` word or until the end of that layout program: at the end
  of every program the draw colour is reset to plain white and the alpha back to `scr_alpha`. The
  status bar program and the server's layout run separately, so a colour never carries from one
  into the other, nor from one frame to the next. [read]

## 9. Areas: how the client uses the frame's area bits when drawing the world

Wire: each frame carries one byte, the area-bits length (0..32), then that many bytes, one bit per
area (area n is bit n mod 8 of byte n / 8). A length above 32 drops the connection with an error.
[read]

Drawing: [read unless marked]
- When the current frame carried at least one area byte, the renderer is given those bits; while
  walking the world's tree, a leaf is skipped (none of its faces marked for drawing) when it is a
  solid leaf, or when its area's bit is NOT set. Areas beyond the bytes sent count as not set
  (the frame's bit array is zeroed before the bytes are copied in), so they are hidden too.
  [read; the zeroing consequence derived]
- The test applies only to the world's own leaves. Brush-model entities (doors, platforms) and
  all other entities are not area-tested by the client (the server already chose which entities
  to send). [read]
- The area test runs after, and independently of, the PVS: the PVS decides which nodes the walk
  can reach, the area bits then drop leaves inside them. Turning the PVS off (`gl_novis` 1,
  default 0) or freezing it (`gl_lockpvs` 1, a cheat) does NOT disable the area test. [read]
- When the frame carried zero area bytes, the renderer gets no area bits and draws every leaf the
  PVS allows (no area culling at all). [read]
- When the current frame is not valid (a delta against a frame the client no longer has), the
  view is not rebuilt: the previous frame's whole view description is reused, including its area
  bits. [read]
- Demos: recording writes each frame's area length and bytes exactly as received; playback reads
  them like a live frame, so a demo culls by area as the live game did. [read]
- No client setting turns area culling off. [read; searched the client and renderer for one, none
  found]
- On the sending side, for completeness: the release's server sends all bits set when the
  server setting `map_noareas` is 1 (default 0) or when the viewer stands in area 0 (outside the
  map or in solid); otherwise the bits of every area connected to the viewer's area through open
  area portals. If the map has no collision data loaded it sends 0 bytes to its own protocol and
  a single byte of all ones to the other protocols. [read]
