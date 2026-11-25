# Quake II mission packs' temporary entities, as the client draws them (answers)

Read from: Q2PRO (github.com/q2pro/q2pro) at commit 601a8df8433b0c50dbbe37c0716c3793fff140a7 (2025-12-11), its client and its OpenGL refresh.

Every fact is marked [read] (stated directly by the source's behaviour), [measured] (run as a
small program of my own), [derived] (worked out from facts read, the working given) or [open]
(the source does not settle it). Wire types, the particle pool, the model-effect pool and its
classes, dynamic lights and cables are as in q2-client-te.md's "Shared conventions"; only
differences and additions are stated here.

## 0. Conventions added by these kinds

- Particle motion. A particle's drawn position at age t seconds is its start plus velocity × t
  plus its acceleration field × t² (the field is not halved). The "gravity" of the earlier notes
  is an acceleration field of 40 downward, so a gravity particle actually falls with 80 units/s².
  [read; the 80 derived] "Half gravity" below is a field of 20 downward (40 units/s² actual).
  [read; derived]
- One-frame particles. Some effects below make "instant" particles: drawn exactly once, at
  alpha 1, on the next frame drawn, then freed. They are re-made every frame by the effect that
  owns them. [read]
- The perpendicular pair. Effects that spread "across the direction" use the same construction
  as the rail spiral's axes in q2-client-te.md § 3: from the unit direction (dx, dy, dz) form
  (dz, -dx, dy), remove its component along the direction and normalise it to get the first
  axis; the second is the first crossed with the direction. [read; described as what it does]
- The sustain pool. Effects that last longer than one message (steam with a time, the widow's
  beam-out, the nuke) take a slot in a pool of 32; a new one with the pool full is dropped
  silently. A slot runs its effect once per client frame (or on its own schedule) while the
  current time has not passed its end time, then frees itself. [read]
- The player-beam pool. Kinds 38, 39 (and 57, 58 outside this list) use a second pool of 128
  beams, separate from the cable pool. A message whose source entity matches a beam already in
  this pool (expired or not) replaces it and gives it 200 ms to live; otherwise it takes a free
  or expired slot and lives 100 ms; with none free it is dropped. [read] So a beam the server
  sends every 100 ms frame stays up, and a single message shows for 100 ms. [derived]
- The client's own dynamic-light pool holds 64 lights keyed by an entity number; key 0 takes the
  first expired slot, or slot 0 when none is expired. [read]

## 1. Green blood (26)

- (a) position, direction byte: 7 bytes in the plain protocol. [read]
- (b) 30 particles, every one colour 0xdf (223) exactly, no random spread. [read] Start: the
  point, plus per axis an integer offset -4..+3, plus the impact normal times one whole distance
  0..7 shared by the three axes. [read] Velocity: per axis crand × 20 (-20..+20 units/s). [read]
  Gravity. Alpha 1, life 0.5..0.8 s. [read] (The same routine as laser sparks, kind 15, with a
  fixed colour and count. [read])
- (c), (d), (e) None. [read]
- (f) Not turned off by cl_disable_particles (its blood bit applies to kind 1 only). [read] Only
  the global switches (cl_particles). [read]

## 2. Plasma explosion (28)

- (a) position: 6 bytes. [read]
- (b) Exactly explosion1's (kind 5): 256 particles, 224..231, per-axis offset -16..+15, per-axis
  integer velocity -192..+191 units/s, gravity, life 0.625..1.0 s. [read]
- (c) Exactly explosion1's model effect: models/objects/r_explode/tris.md2, explosion class with
  15 frames from base 0 or 15 at even odds, random yaw 0..359, full bright, default scale; skins
  and alpha as q2-client-te.md § 25. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5). [read]
- (e) weapons/rocklx1a.wav, volume 1, attenuation normal, at the point. [read]
- (f) Drawn exactly as kind 5: cl_smooth_explosions only; cl_disable_explosions,
  cl_disable_particles and cl_dlight_hacks do not apply. [read]

## 3. Blaster2 (30)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles drawn as the blaster's (kind 2): start the point plus per axis -4..+3 plus the
  normal times a whole distance 0..15; velocity the normal × 30 plus per axis crand × 40; gravity,
  life 0.5..0.8 s; but colour 0xd0 plus rand&7, so 208..215. [read]
- (c) models/objects/explode/tris.md2 with skin 1 (the blaster uses skin 0), puff class with 4
  frames (frames 0..3 lerped over 300 ms, alpha 1 to 0 over 300 ms), full bright and translucent,
  turned to the impact normal exactly as the blaster's puff. [read]
- (d) Radius 150 × the puff's alpha (150 to 0 linearly over 300 ms), colour (0, 1, 0). [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]
- (f) None apart from the global switches. [read]

## 4. Lightning (33)

- (a) short (source entity), short (destination entity), position (start), position (end):
  16 bytes in the plain protocol. [read]
- (b) None. [read]
- (c) A beam in the cable pool (the one 16, 19 and 24 use), drawn every frame as copies of
  models/proj/lightning/tris.md2: [read]
  - Replacement: a message whose source AND destination entities both match a beam already in the
   pool replaces that beam (start, end, model, life), whether or not it had expired; otherwise a
   free or expired slot is taken; with none, the message is dropped. A message with the same
   source but another destination makes a second beam. [read]
  - Life: 200 ms from the message. [read]
  - The beam's extra start offset is forced to zero. [read] When the source entity is the local
   player, the start is replaced every frame by the player's drawn origin (in first person, the
   view origin); otherwise the start position from the message is used. [read] Third person is
   not settled here. [open]
  - Segment length 35 units, and the usable length is the start-to-end distance minus 20 units
   (so the chain stops short of the end point, where the tesla mine sits). The number of copies
   is that shortened length divided by 35, rounded up. [read]
  - When that count is 1 or less (a start-to-end distance of 55 units or less, or less than 20),
   one copy is drawn at the END position, turned along the start-to-end direction (its pitch
   and yaw), with a random roll. [read]
  - Otherwise the first copy sits at the start and the copies are spaced evenly by (shortened
   length - 35) divided by (count - 1), so the last copy's origin is 35 units short of the
   shortened length. Each copy is turned the opposite way: pitch negated and yaw plus 180
   degrees. [read] Where the mesh lies relative to its origin (and so exactly where the visible
   chain ends) depends on the model file. [open]
  - Every copy gets a fresh random roll 0..359 degrees each frame, so the bolt flickers. [read]
  - Full bright; not translucent; no "no shadow" flag set (full-bright models cast no shadow, as
   the earlier notes say). [read; derived]
- (d) None. [read]
- (e) weapons/tesla.wav, volume 1, attenuation normal, attached to the SOURCE entity (it moves
  with it) on that entity's weapon channel. [read] A sound on the same entity and the same
  non-zero channel cuts the one playing, so each repeated message restarts tesla.wav and only one
  plays per source at a time. [read for the software mixer; derived for the effect]
- (f) None apart from the global switches. [read]

## 5. Plain explosion (35)

- (a) position: 6 bytes. [read]
- (b) None. [read]
- (c) The explosion1 model effect alone: models/objects/r_explode/tris.md2, explosion class,
  15 frames from base 0 or 15 at even odds, random yaw 0..359, full bright, default scale; skins
  and alpha as q2-client-te.md § 25. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5). [read]
- (e) None: this kind plays no sound. [read]
- (f) cl_smooth_explosions only. [read]

## 6. Heat beam (38) and monster heat beam (39)

- (a) Both: short (source entity), position (start), position (end): 14 bytes. [read]
- Pool and life: the player-beam pool (§ 0): 100 ms for a first message, 200 ms when it replaces
  the same source's beam. [read]
- Model: models/proj/beam/tris.md2, segment length 32 units (no shortening), the count of copies
  the drawn length divided by 32 rounded up; with more than one copy, the first at the start and
  the rest spaced evenly by (length - 32) divided by (count - 1), so the last copy ends at the
  end; with one copy, it sits at the start. [read; derived] Each copy is turned with pitch negated
  and yaw plus 180 degrees, full bright, not lerped (shown exactly at one model frame), and all
  copies share one roll equal to the client clock in milliseconds modulo 360, in degrees, so the
  beam spins at 1000 degrees per second and wraps every 360 ms. [read; the rate derived]
- Offsets: kind 38 carries a fixed offset of 2 right, 7 forward, 3 down (units); kind 39 has no
  offset. [read]
- When the source entity is the LOCAL player (either kind), every frame: [read]
  - The start is recomputed: the view origin plus the player-state gun offset (lerped between the
   two key frames), then the offset applied along the view's right, forward and up axes.
  - The right component is multiplied by a hand factor: -1 when cl_gun is 3; +1 when cl_gun is
   2; otherwise from the userinfo setting hand: 0 (right) gives +1, 1 (left) gives -1,
   2 (centre) gives 0 and also moves the start 1 more unit down.
  - When cl_gunfov is above 0 (default 90), the right component is scaled by tan(fov_x/2)
   divided by tan(g/2), with g the gun fov clamped to 30..160, and the up component by
   tan(fov_y/2) divided by tan(g_y/2), with g_y the vertical fov a 4:3 view of g would have and
   fov_y the current view's vertical fov at 4:3. So at the default 90 and a 90 view fov the
   offset is unchanged. [read; the last sentence derived]
  - The direction is NOT the start-to-end direction: the beam is laid along the view's forward
   axis, with length equal to the distance from the recomputed start to the end position sent.
   So the beam follows the player's view each frame. [read]
  - The model is drawn at model frame 1. [read]
  - The particle spiral is drawn (below), unless the local server is paused. [read]
- When the source is any other entity: [read]
  - With a non-zero offset (kind 38, another player's beam), the start from the message is
   moved by a fixed amount in the frame of the reversed beam: in the beam's own terms 1 unit
   to the right, 7 units toward the end, 7 units down. [read the construction; derived the
   resulting directions, since the reversed frame's right is the beam's left, its forward is
   toward the start and its up is the beam's up]
  - With a zero offset (kind 39, a monster), the start is used as sent, and every frame a shell
   of 40 one-frame particles is drawn: colour 0xe0 (224) exactly, each 10 units from the start in
   a uniformly random direction (a rejection method that gives a uniform point on the sphere),
   no velocity or acceleration. [read; uniformity derived from the method]
  - The beam then runs from that start to the end, model frame 2. [read]
  - No spiral for other entities. [read]
- The spiral (local player only), remade every frame: [read]
  - Rings of particles across the beam every 32 units along it, starting at a phase that is the
   client clock in seconds × 96 modulo 32, so the rings appear to stream away from the gun at
   96 units/s. The first ring is at that phase distance from the start (its
   whole part decides the shrink below), then one every 32 units up to but not including the
   beam's length. [read; the motion derived]
  - Each ring is 20 particles at angles 0, π/10, ... 19π/10 radians [measured: the float loop
   runs 20 times], placed 1.5 units from the axis along the VIEW's right and up axes (not the
   beam's). Rings less than 10 units from the start shrink their radius in proportion
   (distance/10 of 1.5). [read; 1.5 derived from 0.5 times 3]
  - Colour 223 minus rand&7, so 216..223. Alpha 0.5, no velocity, no acceleration, and an alpha
   velocity of -1000 per second, so each particle is shown for the frame it is made in and gone
   after (about 0.5 ms of life). [read; derived]
  - It draws from the shared 8192 pool and stops short when the pool is empty. [read] A 1000-unit
   beam makes about 32 rings of 20, about 640 particles a frame. [derived]
- (d), (e) None, on either kind. [read]
- (f) cl_gun, hand and cl_gunfov change the local start (above); cl_particles hides the spiral
  and shell; cl_entities hides the model. [read]

## 7. More blood (42)

- (a) position, direction byte: 7 bytes. [read]
- (b) 250 particles drawn exactly as blood's (kind 1: the gunshot puff's offsets, distance 0..31
  along the normal, velocity crand × 20 per axis, gravity, life 0.5..0.8 s), colour 0xe8 plus
  rand&7, so 232..239. [read]
- (c), (d), (e) None. [read]
- (f) NOT turned off by cl_disable_particles' blood bit (only kind 1 checks it). [read]

## 8. Heat beam sparks (43) and heat beam steam (44)

- (a) Both: position, direction byte: 7 bytes. [read]
- (b) The steam puff routine (the one kind 40 uses), with magnitude 60: [read]
  - 43: 50 particles, colour 0x08 plus rand&7, so 8..15 (the light end of the grey ramp).
  - 44: 20 particles, colour 0xe0 plus rand&7, so 224..231.
  - Start: per axis the point plus crand × 6 (magnitude × 0.1), each axis independent. [read]
  - Velocity: the normal × 60, plus crand × 20 along the first perpendicular axis and another
   crand × 20 along the second (magnitude / 3 each). [read]
  - Acceleration: half gravity (field 20 downward). Alpha 1, life 0.5..0.8 s. [read]
- (c), (d) None. [read]
- (e) Both: weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]
- (f) None apart from the global switches. [read]

## 9. Electric sparks (46)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles drawn exactly as the gunshot puff (kind 0's particles: offsets, distance 0..31
  along the normal, velocity crand × 20, gravity, life 0.5..0.8 s), colour 0x75 plus rand&7, so
  117..124. [read]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]
- (f) None apart from the global switches. [read]

## 10. Tracker explosion (47)

- (a) position: 6 bytes. [read]
- (b) 128 particles, every one colour 0 (black: the run of 1 gives no spread). [read] Start: the
  point plus per axis an integer offset -16..+15. Velocity: per axis an integer -128..+127
  units/s, independent. Gravity. Alpha 1, alpha velocity -0.4/(0.6 + 0.2 × frand), so life
  1.5..2.0 s. [read; life derived]
- (c) None. [read]
- (d) A dark light: radius 150, colour (-1, -1, -1), at the point, for 100 ms at a constant
  radius (no decay), from the client's own light pool with key 0. [read] Its effect: in the
  lightmap path a light's contribution is added per luxel in proportion to its colour, so a
  negative colour subtracts from the world's light, clamped at 0 per channel; on models it
  subtracts from the point light, clamped at 0. So it darkens the area for 100 ms. [read the
  clamps; derived the darkening] With gl_dlight_falloff (default 1) the edge is the soft one.
  [read]
- (e) weapons/disrupthit.wav, volume 1, attenuation normal, at the point. [read]
- (f) cl_lights 0 drops the dark light; cl_particles 0 the particles. [read]

## 11. Nuke blast (51)

- (a) position: 6 bytes. [read]
- (b) A sustained effect (§ 0's pool of 32), lasting 1000 ms from the message. [read] Every
  client frame while it lasts it makes 700 one-frame particles on the surface of a sphere
  centred on the point, of radius 200 × the elapsed fraction (elapsed ms / 1000), so the shell
  grows linearly from 0 to 200 units in 1 s; each particle in a uniformly random direction
  (as the shell in § 6), no velocity or acceleration. [read; uniformity derived] Colours: one of
  110, 112, 114, 116 at even odds [read] Because it is per frame, the particles per second scale with the frame rate,
  and at 700 a frame it holds about 8.5% of the 8192 pool each frame. [derived] If the pool is
  empty part way the frame's shell is short. [read]
- Several nukes each take their own slot; a 33rd concurrent sustained effect is dropped. [read]
- (c), (d), (e) None from this kind in the client (no model, no light, no sound). [read]
- (f) cl_particles only. [read]

## 12. Explosion1 big (53) and explosion1 no-particles (54)

- (a) Both: position: 6 bytes. [read]
- 53: [read]
  - (b) None.
  - (c) models/objects/r_explode/tris.md2 (the same model as every explosion), explosion class,
   15 frames from base 0 or 15 at even odds, random yaw 0..359, full bright, drawn at scale 2
   (the model's three axes doubled about its origin). Skins and alpha as q2-client-te.md § 25.
  - (d) Radius 350 × alpha, colour (1, 0.5, 0.5): the light is not scaled with the model.
  - (e) weapons/rocklx1a.wav, volume 1, attenuation normal.
- 54: [read]
  - (b) None.
  - (c) As kind 5's model, default scale.
  - (d) Radius 350 × alpha, colour (1, 0.5, 0.5).
  - (e) weapons/rocklx1a.wav, volume 1, attenuation normal.
- (f) Both: cl_smooth_explosions only; the disable and light-hack settings do not apply. [read]

## 13. Flechette (55)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles as Blaster2's (§ 3), colour 0x6f plus rand&7, so 111..118. [read]
- (c) models/objects/explode/tris.md2 with skin 2, puff class with 4 frames over 300 ms, full
  bright and translucent, turned to the impact normal as the blaster's. [read]
- (d) Radius 150 × the puff's alpha, colour (0.19, 0.41, 0.75). [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]
- (f) None apart from the global switches. [read]

## 14. The colour indices used (palette 0..255)

- 26 green blood: 223 exactly. [read]
- 28 plasma explosion: 224..231. [read]
- 30 blaster2: 208..215. [read]
- 38 heat beam spiral: 216..223. 39 monster heat beam shell: 224 exactly. [read]
- 42 more blood: 232..239. [read]
- 43 heat beam sparks: 8..15. 44 heat beam steam: 224..231. [read]
- 46 electric sparks: 117..124. [read]
- 47 tracker explosion: 0 exactly. [read]
- 51 nuke blast: 110, 112, 114, 116. [read]
- 55 flechette: 111..118. [read]
- 33, 35, 53, 54: no particles. [read]
