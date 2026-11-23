# Quake II's base-game temporary entities, as the client draws them (answers)

Read from: Q2PRO (github.com/q2pro/q2pro) at commit 601a8df, its client and its OpenGL refresh.
Every fact is marked [read] (stated directly by the source's behaviour), [derived] (worked out
from facts read, the working given) or [open] (the source does not settle it).

## Shared conventions used below

- Wire types. A message is the temp-entity command byte, then the kind byte, then the fields
  below. A "position" is three coordinates x, y, z. In the plain protocol each coordinate is a
  signed 16-bit little-endian integer in eighths of a unit (6 bytes a position). [read] When the
  client and server have negotiated Q2PRO's extended-coordinate entity-state extension, each
  coordinate is instead a little-endian 16-bit word whose lowest bit says a third byte follows:
  without it the upper 15 bits are a signed value in eighths of a unit (2 bytes); with it the
  third byte supplies bits 16..23 and the upper 23 bits of the 24 are a signed value in eighths
  (3 bytes). [read] A "direction byte" is one unsigned byte indexing the standard 162-entry
  table of unit normals (the same table the format uses for model vertex normals); a value of
  162 or more drops the connection with an "out of range" error. [read] A "short" is a signed
  16-bit little-endian integer. [read]
- Random helpers. "frand" below is uniform in [0, 1); "crand" is uniform in [-1, 1). [read]
  "rand&N" means a uniformly random integer 0..N for N one less than a power of two; "rand%N"
  means 0..N-1. [derived]
- Particles. The particle pool holds 8192; when it is empty an effect simply stops making
  particles part way, with no error. [read] Gravity, the alpha fade and the palette work as the
  known machinery says; gravity here is 40 units/s² downward for every particle described as
  "gravity" below. [read] A particle's life is 1 divided by the magnitude of its alpha velocity
  (alpha starts at 1). [derived]
- Sounds. All sounds below play at volume 1.0 (full). Attenuation values: normal is 1, static
  is 3 (dies off quickly with distance), none is 0 (full volume over the whole level). [read]
  Unless stated, a sound is played at a fixed world point (the effect's first position), on
  the world entity, on the "any" channel, so any number of them can overlap. [read]
- The model-effect pool. Animated model effects (explosions, puffs, flashes) come from a pool of
  256; when it is full the oldest one is overwritten. [read] Each has a start time set to the
  previous server frame's time, so its clock reads 0 at the moment the client starts lerping
  toward the frame that carried the event. [derived] Its animation clock always advances one
  model frame per 100 ms, whatever the server's tick rate. [read] Its drawn model frame is
  "base + f + 1" lerped from "base + f", with f the whole number of 100 ms elapsed and the lerp
  fraction the remainder, so the mesh morphs smoothly between frames at 10 frames per second.
  [read]
- Three effect classes in that pool, by how they fade and end. [read]
  - "Puff" class (N frames): alpha falls linearly from 1 to 0 over (N-1)×100 ms, and the effect
   is removed once (N-1)×100 ms have passed. Its dynamic light radius is its base radius times
   that alpha.
  - "Flash" class: alpha stays 1; removed after 100 ms whatever its frame count.
  - "Explosion" class (N frames): removed after (N-1)×100 ms; alpha and skin as Q25 describes, and
   light radius is base radius times alpha.
- Dynamic lights. A dynamic light is a point with a radius in units and an RGB colour 0..1.
  The renderer accepts at most 64 a frame; the rest are dropped. [read] How a light dims across
  the radius is the renderer's general dynamic-light model (with a renderer setting,
  gl_dlight_falloff, choosing a softer edge), not anything a temp entity sets. [read]
- Beams (cable models). A cable is drawn as copies of one segment model laid end to end from its
  start to its end. The pool holds 128 cables; a new message whose source and destination
  entities match an existing cable replaces it, otherwise a free or expired slot is taken, and
  when none is free the message is dropped. [read] A cable lives 200 ms from the message (drawn
  while its end time has not passed), so the server must resend it every frame to keep it up.
  [read]

## 0. Gunshot (bullet hitting a wall)

- (a) position, direction byte: 7 bytes in the plain protocol. [read]
- (b) 40 particles. [read] Colour: palette 0 plus rand&7, so indices 0..7 (the dark end of the
  grey ramp). [read] Start: the point, plus per axis an integer offset -4..+3, plus the impact
  normal times a distance 0..31 (one random whole distance per particle, shared by the three
  axes, so particles string out along the normal). [read] Velocity: each axis independently
  crand × 20, so -20..+20 units/s, no bias along the normal. [read] Gravity. [read] Alpha 1,
  life 0.5..0.8 s (alpha velocity -1/(0.5 + 0.3·frand)). [read]
- (c) A smoke puff and a muzzle-style flash at the point:
  - smoke: models/objects/smoke/tris.md2, puff class with 4 frames, so frames 0..3 lerped over
   300 ms, translucent, alpha 1 to 0 over 300 ms, casts no shadow, lit by the world (not full
   bright). [read]
  - flash: models/objects/flash/tris.md2, flash class, drawn for 100 ms lerping frame 0 to 1,
   full bright, opaque (alpha 1). [read]
  - Neither is rotated, neither is scaled. [read]
- (d) No dynamic light. [read]
- (e) A ricochet with odds 1 in 16 each: world/ric1.wav, world/ric2.wav, world/ric3.wav; 13 in
  16 no sound. Volume 1, attenuation normal. [read]
- (f) Nothing else (no decal in this client). [read]

## 1. Blood

- (a) position, direction byte: 7 bytes. [read]
- (b) 60 particles drawn exactly as the gunshot's puff (same offsets, velocity, gravity, life)
  but colour 0xe8 plus rand&7, so indices 232..239. [read]
- (c), (d), (e) none. [read]
- (f) Turned off entirely when the cl_disable_particles setting has bit value 16 set. [read]

## 2. Blaster (hitting a wall)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles. Colour 0xe0 plus rand&7 (224..231). Start: the point, per axis -4..+3, plus
  the normal times a distance 0..15. Velocity: the normal times 30 plus, per axis, crand × 40
  (so up to 40 units/s of scatter on a 30 units/s push off the wall). Gravity. Alpha 1, life
  0.5..0.8 s. [read]
- (c) models/objects/explode/tris.md2, skin 0, puff class with 4 frames (frames 0..3 lerped over
  300 ms, alpha 1 to 0 over 300 ms), full bright and translucent. [read] It is turned to the
  impact normal: pitch is the arccosine of the normal's z in degrees, yaw is the angle of the
  normal's x, y in degrees (90 for a pure +y normal, 270 for a pure -y normal, 0 for a vertical
  normal), roll 0. [read] Default scale. [read]
- (d) Radius 150 × the puff's alpha, so 150 falling linearly to 0 over 300 ms; colour
  (1, 1, 0). [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal. [read]
- (f) Nothing else. [read]

## 3. Railtrail

- (a) position (start), position (end): 12 bytes. [read]
- (b), the default (cl_railtrail_type 0), two particle strands, none with gravity or any
  acceleration: [read]
  - Spiral: one particle per unit of length (the count is the length rounded up). Particle i
   sits i units along the line from the start, pushed 3 units out from the line in the
   direction at angle 0.1·i radians in the plane across the beam, so one full turn every
   about 62.8 units. [read] Velocity: that same outward unit direction times 6 units/s. [read]
   Colour 0x74 plus rand&7 (116..123). Life 1.0..1.2 s (alpha velocity -1/(1 + 0.2·frand)).
   [read] The angle-zero axis: take the beam direction (dx, dy, dz), form the vector
   (dz, -dx, dy), remove its component along the beam and normalise it; the quarter-turn axis
   is that vector crossed with the beam direction. [derived from what the construction does]
  - Core: one particle every 0.75 units from the start (count is the length divided by 0.75,
   rounded up). Each sits within ±3 units of its point on every axis (crand × 3), drifts at
   crand × 3 units/s per axis, colour rand&15 (0..15, the grey ramp from black to near white),
   life 0.6..0.8 s (alpha velocity -1/(0.6 + 0.2·frand)). [read]
  - The default trail does not read cl_railtrail_time or any colour setting: its timings and
   colours are fixed. [read]
  - A long rail can use a large share of the 8192-particle pool (a 1000-unit rail makes about
   1000 spiral plus about 1334 core particles); if the pool runs out the trail stops short.
   [derived]
- (c) None in the default trail. [read]
- (d) None. [read]
- (e) weapons/railgf1a.wav, volume 1, attenuation normal, played at the END position, not the
  start. [read]
- (f) The other trail types are Q26. [read]

## 4. Shotgun (pellet hitting a wall)

- (a) position, direction byte: 7 bytes. [read]
- (b) 20 particles, otherwise identical to the gunshot's (indices 0..7). [read]
- (c) The same smoke puff and flash as the gunshot. [read]
- (d) None. (e) None: the shotgun never plays a ricochet. [read]
- (f) Nothing else. [read]

## 5. Explosion1

- (a) position: 6 bytes. [read]
- (b) 256 particles. Colour 0xe0 plus rand&7 (224..231). Start: per axis an integer offset
  -16..+15. Velocity: per axis an integer -192..+191 units/s, independent. Gravity. Alpha 1,
  alpha velocity -0.8/(0.5 + 0.3·frand), so life 0.625..1.0 s. [read]
- (c) models/objects/r_explode/tris.md2, explosion class with 15 frames; its base frame is 0 or
  15 with even odds, so it plays either frames 0..14 or 15..29; yaw a random whole number of
  degrees 0..359, pitch and roll 0; full bright; default scale. Animation, skins and alpha:
  Q25. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5). [read] Over life: Q25. [read]
- (e) weapons/rocklx1a.wav, volume 1, attenuation normal. [read]
- (f) Nothing else; not affected by any of the disable or light-hack settings. [read]

## 6. Explosion2

- (a) position: 6 bytes. [read]
- (b) The same 256 particles as 5. [read]
- (c) The same model, but explosion class with 19 frames and the base frame fixed at 30, so it
  plays frames 30..48. Random yaw as 5. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5). [read]
- (e) weapons/grenlx1a.wav, volume 1, attenuation normal. [read]
- (f) Not affected by the disable or light-hack settings. [read]

## 7. Rocket explosion

- (a) position: 6 bytes. [read]
- (b) The same 256 particles as 5. [read]
- (c) The same model and animation as 5 (15 frames, base 0 or 15, random yaw). [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5); 200 instead of 350 when cl_dlight_hacks has bit
  value 2 set. [read]
- (e) weapons/rocklx1a.wav, volume 1, attenuation normal. [read]
- (f) Settings in Q26. [read]

## 8. Grenade explosion

- (a) position: 6 bytes. [read]
- (b) The same 256 particles as 5. [read]
- (c) As 6: 19 frames from base 30, random yaw. [read]
- (d) Radius 350 × alpha (200 with cl_dlight_hacks bit value 2), colour (1, 0.5, 0.5). [read]
- (e) weapons/grenlx1a.wav, volume 1, attenuation normal. [read]
- (f) Settings in Q26. [read]

## 9. Sparks

- (a) position, direction byte: 7 bytes. [read]
- (b) 6 particles drawn as the gunshot's puff (same offsets, velocity, gravity, life) with colour
  0xe0 plus rand&7 (224..231). [read]
- (c), (d), (e) None: no smoke, no flash, no sound. [read]

## 12. Screen sparks (power screen hit)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles as the gunshot's puff, colour 0xd0 plus rand&7 (208..215). [read]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point, but on a special
  world-entity channel that holds only one sound at a time: while an earlier screen or shield
  spark sound is still playing anywhere, a new one is not started. [read]

## 13. Shield sparks (power shield hit)

- Drawn and heard exactly as 12 except the particle colour is 0xb0 plus rand&7 (176..183).
  [read] The two share the single-sound channel, so one blocks the other. [read]

## 14. Bullet sparks

- (a) position, direction byte: 7 bytes. [read]
- (b) 6 particles, colour 224..231, as 9. [read]
- (c) The gunshot's smoke puff and flash. [read]
- (d) None. [read]
- (e) The gunshot's ricochet: 1 in 16 each of ric1, ric2, ric3, otherwise silent. [read]

## 16. Parasite attack

- (a) short (the source entity), position (start), position (end): 14 bytes. [read]
- (b) No particles. [read]
- (c) A cable of models/monsters/parasite/segment/tris.md2. [read]
  - Each segment is 30 units long. The count is the start-to-end distance divided by 30,
   rounded up; a zero distance draws nothing. [read]
  - With two or more segments, the first sits at the start and the rest are spaced evenly so
   the last one starts 30 units short of the end (so segments overlap a little rather than
   leave a gap or overshoot). With one segment it sits at the start and may overshoot the end.
   [derived]
  - Every segment is pitched and yawed along the start-to-end line and given a random roll
   0..359 degrees, re-rolled every rendered frame, so the cable shimmers. [read]
  - Lit by the world (not full bright), casts no shadow, default scale, model frame 0. [read]
  - The start is the message's start position, except that when the source entity is the local
   player the player's drawn origin is used instead. [read] Its offset is always zero. [read]
  - A new message from the same source entity replaces the cable at once. [read]
  - Lives 200 ms (the server resends it each frame while the attack lasts). [read]
- (d), (e), (f) None. [read]

## 17. Rocket explosion in water

- Exactly as 7 (bytes, particles, model, frames, light, and the same rocket disable bits), except
  the sound is weapons/xpld_wat.wav (volume 1, attenuation normal). [read]

## 18. Grenade explosion in water

- Exactly as 8 (including the grenade disable bits), except the sound is weapons/xpld_wat.wav.
  [read]

## 19. Medic cable attack

- Exactly as 16: same 14 bytes, same model (the parasite's segment), same cable. [read]

## 20. BFG explosion

- (a) position: 6 bytes. [read]
- (b) No particles. [read]
- (c) The sprite sprites/s_bfg2.sp2, explosion class with 4 frames: frames 0..3 lerped over
  300 ms, then removed. Translucent from the start, full bright, faces the viewer as sprites
  do, default scale. [read] An initial alpha of 0.30 is set but is overwritten by the
  explosion class's alpha every frame, so the alpha actually drawn is 1 falling linearly to
  0 over 300 ms (smooth explosions on), or 1, 15/16, 14/16 for the three 100 ms steps (off).
  [derived]
- (d) Radius 350 × alpha, colour (0, 1, 0), so 350 to 0 over 300 ms with smooth explosions on.
  [read]
- (e) None. [read]

## 21. BFG big explosion

- (a) position: 6 bytes. [read]
- (b) 256 particles exactly as the explosion particles of 5 except colour 0xd0 plus rand&7
  (208..215). [read]
- (c), (d), (e) None: no model, no light, no sound. [read]

## 22. Boss teleport

- (a) position: 6 bytes. [read]
- (b) 4096 particles. [read]
  - Colour: one of palette 16, 104, 168, 144 with even odds, no spread. [read]
  - Horizontal: an angle chosen from 1024 even steps of a full turn (the first and last coincide)
   and a distance 0..31; the particle starts that far out at that angle, moves outward along
   the angle at 70..133 units/s and is pulled back inward at 100 units/s². [read]
  - Vertical: starts 8..97 units above the point, moves at -100..-69 units/s (downward), with an
   upward acceleration of 160 units/s² (four times gravity, reversed). [read]
  - Alpha 1, alpha velocity -0.3/(0.5 + 0.3·frand), so life 1.67..2.67 s. [read]
- (c), (d) None. [read]
- (e) misc/bigtele.wav, volume 1, attenuation none (heard at full volume everywhere). [read]

## 23. BFG laser

- (a) position (start), position (end): 12 bytes. [read]
- (b), (c) No particles, no model: a straight beam primitive from start to end. [read]
  - Colour: one of palette 0xd0, 0xd1, 0xd2, 0xd3 (208..211), chosen once with even odds. [read]
  - Alpha 0.30, constant; translucent, blended, does not write depth. [read]
  - Width setting 4. With the default renderer beam style (gl_beamstyle 0) it is a flat strip
   turned to face the camera, 9.6 units across (1.2 × 4 either side of the line), textured with
   the soft beam texture; with gl_beamstyle 1 it is a 12-sided tube of radius 2, untextured.
   [read and derived]
  - Lives 100 ms. The laser pool holds 256; if none has expired the new laser is dropped. [read]
- (d), (e) None. [read]

## 24. Grapple cable

- (a) short (source entity), position (start), position (end), position (offset): 20 bytes. [read]
- (b) None. [read]
- (c) A cable as in 16 but with models/ctf/segment/tris.md2, and its start is the start position
  plus the offset position (or, when the source is the local player, the player's drawn origin
  plus the offset). [read] Same 30-unit segments, random roll per frame, lit by the world, no
  shadow, 200 ms life, replaced by the next message from the same entity. [read]
- (d), (e) None. [read]

## 25. The explosions' model animation and light

- Frame rate: 10 model frames per second (one per 100 ms), lerped between frames. [read]
- Frames: rocket (7, 17) and explosion1 (5): 15-frame class from base 0 or 15 at even odds,
  drawn lerping through frames base..base+14 and removed after 1400 ms. Grenade (8, 18) and
  explosion2 (6): 19-frame class from base 30; frames 30..48; removed after 1800 ms. [read and
  derived] BFG explosion (20): 4-frame sprite, frames 0..3, 300 ms. BFG big explosion (21): no
  model. [read]
- Skin by whole 100 ms steps elapsed (f): f 0..9 use skin f/2 rounded down (0,0,1,1,2,2,3,3,4,4),
  f 10..12 use skin 5; f 13 on use skin 6. From f 10 on the model is forced translucent. [read]
  (This needs the explosion model to carry at least 7 skins and 49 frames. [derived])
- Alpha, with cl_smooth_explosions 1 (the default): 1 falling linearly with time to 0 at
  removal ((N-1)×100 ms, N the frame count), and the model is always drawn translucent. [read]
- Alpha, with cl_smooth_explosions 0: (16 - f)/16, a step every 100 ms, floored at 0; opaque
  until f reaches 10, translucent after. For the 15-frame explosions the last step drawn is
  f 13, alpha 3/16; for the 19-frame ones alpha reaches 0 at f 16 and the model is invisible for
  its last 200 ms. [read and derived]
- Light: radius = base radius × the alpha above, every frame, colour constant. So with smooth
  explosions a rocket's 350-unit light falls linearly to 0 over 1.4 s, a grenade's over 1.8 s,
  the BFG explosion's green 350 over 0.3 s. With smooth explosions off it steps down by 1/16 of
  the base each 100 ms (350, 328.1, 306.3, ...), ending at 65.6 for a rocket and reaching 0 for a
  grenade. [derived]
- Water versus air: a water explosion (17, 18) differs from its air one (7, 8) only in its kind
  number and in its sound (weapons/xpld_wat.wav for both water kinds). Bytes, particles, model,
  frames, skins, light, and which disable and light-hack bits apply are the same. [read]
- 5 and 6 also look the same as 7 and 8 respectively; they differ only in ignoring the disable
  and light-hack settings, and 6 sounds like a grenade. [read]

## 26. Client settings that change these effects

- cl_railtrail_type (default 0). 0: the default particle trail of 3. 1: no particles; a "core"
  beam only. 2 or more: the core beam plus a coloured spiral. [read]
  - Core beam: a straight blended beam from start to end, colour cl_railcore_color (default
   "red", which is RGBA 255, 0, 0, 255), width cl_railcore_width (default 2; 0 or less draws no
   core), alpha falling linearly from 1 to 0 over cl_railtrail_time seconds (default 1.0,
   clamped to 0..86400; 0 draws nothing). At the default beam style it is 4.8 units across,
   with gl_beamstyle 1 a tube of radius 1. It takes a slot in the 256-laser pool. [read and
   derived]
  - Spiral: built as the default trail's spiral (one per unit, 0.1 radians a unit, outward
   velocity 6 units/s, no gravity) but with radius cl_railspiral_radius (default 3), colour
   cl_railspiral_color (default "blue", RGBA 0, 0, 255, 255) as a true colour rather than a
   palette index, and life cl_railtrail_time + 0.2·frand seconds. No core particles are thrown.
   [read]
  - The colour settings take a colour name or #rgb, #rrggbb or #rrggbbaa; an invalid value prints
   a warning and resets the setting to its default. [read]
  - The sound is unchanged by any of these. [read]
- cl_disable_explosions (default 0), a bit set: bit value 1 for grenade explosions (8, 18), bit
  value 2 for rocket explosions (7, 17). A disabled explosion draws no model but keeps its light,
  with the same radius curve and lifetime as if the model were there; particles and sound are
  unchanged. [read]
- cl_disable_particles (default 0), a bit set: value 1 removes the particles of 8 and 18; value 4
  removes those of 7 and 17; value 16 removes blood (1) entirely. (Values 2, 8 and 32 affect
  trails, not these entities.) [read]
- cl_dlight_hacks (default 0), a bit set: value 2 makes the light of 7, 8, 17, 18 start at 200
  instead of 350. [read]
- cl_smooth_explosions (default 1): the explosions' alpha and light curve, Q25. [read]
- cl_particles (default 1): 0 draws no particles at all. cl_lights (default 1): 0 draws no
  dynamic lights at all. cl_entities (default 1): 0 draws no entities, which includes every
  explosion model, puff, flash and cable segment, and also the BFG laser and the rail core beam, which are drawn as entities. [read]
- Renderer: gl_beamstyle (default 0) chooses flat camera-facing textured strips or 12-sided
  untextured tubes for the BFG laser and the rail core; gl_dynamic (default 1) turns dynamic
  lighting of the world on or off; gl_partscale and gl_partstyle change how every particle is
  drawn (the known particle machinery). [read]
- Not affected by any setting: 0, 2, 4, 5, 6, 9, 12, 13, 14, 16, 19, 20, 21, 22, 23, 24 apart from
  the global switches above. [read]

## 27. Shared routines

- 1 is drawn the same as 0's particles except colour 232..239 and 60 particles, with no smoke,
  flash or sound, and it can be turned off. [read]
- 4 is drawn the same as 0 except 20 particles and never a ricochet sound. [read]
- 14 is drawn the same as 0 except 6 particles of colour 224..231. [read]
- 9 is drawn the same as 14 except no smoke, no flash and no sound. [read]
- 12 and 13 are drawn the same as each other except the particle colour. [read]
- 19 is drawn the same as 16, with no exception. [read]
- 24 is drawn the same as 16 except its model and its extra offset added to the start. [read]
- 17 is drawn the same as 7 except its sound; 18 the same as 8 except its sound. [read]
- 5 is drawn the same as 7 except that it ignores the disable and light-hack settings; 6 the
  same as 8 with the same exception. [read]
- 21's particles are drawn the same as the explosion particles of 5 to 8 except colour 208..215.
  [read]
- 2's puff model and light are the blaster-family routine (other kinds outside this list use it
  with other skins and colours); its particles are its own. [read]
- 20, 22, 23 and 3 have no partner among the listed kinds. [read]
