# Quake II's mission-pack temporary entities, as the client draws them (answers)

Read from: Q2PRO (github.com/q2pro/q2pro) at commit 601a8df, its client and its OpenGL refresh.
This note continues `q2-client-te.md` for the kinds the two mission packs add (25..55), and
lists the later kinds the same client also accepts (56..63 and 128). Every fact is marked
[read] (stated directly by the source's behaviour), [measured] (computed by running the
arithmetic the source does), [derived] (worked out from facts read, the working given) or
[open] (the source does not settle it).

## Conventions (beyond the base note's)

- Wire types, "frand", "crand", "rand&N", "rand%N", the 8192-particle pool, gravity of 40, the
  256-slot model-effect pool and its puff, flash and explosion classes, the 64 dynamic lights a
  frame, sound volumes and attenuations, and the explosions' frames, skins and alpha: exactly as
  `q2-client-te.md` states them. [read]
- A "long" is a signed 32-bit little-endian integer. [read]
- "Sphere direction" below means a direction drawn uniformly over the unit sphere (a rejection
  method on the unit disc, then lifted to the sphere). [read]
- "Two side axes" of a direction means two unit vectors at right angles to it and to each other,
  built from it alone (no randomness). [read]
- "One-frame particle": a particle drawn in exactly one rendered frame and freed in the next,
  whatever the frame rate. Effects that repeat such particles every rendered frame therefore
  spend particles in proportion to the frame rate, not to time. [read]
- The sustain pool. Kinds that keep emitting after their message (40 with a duration, 50, 51)
  take a slot in a pool of 32 shared by all three. A slot is taken only if one is empty; with all
  32 busy the message is dropped silently. Messages are never merged or replaced by id. A slot
  runs its emitter when the client time has reached its next-run time and has not passed its
  end time, and is freed once the end time has passed. The first run is in the same rendered
  frame the message is parsed in. [read]
- Particle motion. A particle's drawn position is its start plus velocity times t plus
  acceleration times t squared (t in seconds since it was made), without a factor of one half,
  so "gravity 40" moves a particle as a true 80 units/s² would. [read]
- Negative light. A dynamic light with negative colour subtracts from the lightmap's light and
  from the light on models in its radius; the result is clamped at 0 (black), never below. [read]

## Wire layout across the protocols

- Every kind below has one layout in all the protocols the client speaks (34, 35 and 36): no
  field is added, dropped or reordered by protocol. [read] The only difference is the
  coordinate size of every position, which follows the extended-coordinate rule in the base
  note. [read]
- A kind byte the client does not know (any value not listed here or in the base note,
  including 32) drops the connection with a "bad type" error. [read]
- The kinds 56..63 and 128 are parsed whatever protocol was negotiated; nothing checks that the
  server is a re-release one. [read]

## 25. Welding sparks

- (a) byte (count), position, direction byte, byte (palette colour): 9 bytes. [read]
- (b) count particles, all of exactly the given palette colour (no spread). Start: the point
  plus per axis an integer -4..+3, plus the normal times a whole distance 0..7. Velocity: each
  axis crand × 20. Gravity. Alpha 1, life 0.5..0.8 s. [read]
- (c) A flash-class effect of models/objects/flash/tris.md2 is made, but flagged so that the
  renderer treats it as a beam from its origin to the same point (zero length), so nothing
  visible is expected; it carries the light. [read and derived]
- (d) Radius a random whole number 100..174, colour (1, 1, 0.3), constant for 100 ms (flash
  class), then gone. [read]
- (e) None. [read]

## 26. Green blood

- (a) position, direction byte: 7 bytes. [read]
- (b) 30 particles, all palette index 223 (0xdf, no spread). Start: the point plus per axis an
  integer -4..+3, plus the normal times a whole distance 0..7 (shared by the three axes).
  Velocity: each axis crand × 20. Gravity. Alpha 1, life 0.5..0.8 s. [read]
- (c), (d), (e) None. [read]
- (f) Not removed by the blood bit (16) of cl_disable_particles, nor by any other setting but
  cl_particles 0. [read]

## 27. Blue hyperblaster (first form)

- (a) position, position: 12 bytes. [read]
- (b) The blaster's 40 particles of kind 2 (colour 224..231), but the second position is used
  as the "normal": each particle starts at the first position plus -4..+3 per axis plus the
  second position's coordinates times a whole 0..15, and moves at the second position times
  30 plus crand × 40 per axis. With a real world position as the second field the particles
  land far from the point and fly very fast. [read and derived]
- (c), (d), (e) None. [read]
- (f) 56 is the corrected form. [read]

## 28. Plasma explosion

- (a) position: 6 bytes. [read]
- (b) to (e): exactly as explosion1 (5): 256 explosion particles (224..231), the
  models/objects/r_explode/tris.md2 explosion with 15 frames from base 0 or 15, random yaw,
  light 350 × alpha of colour (1, 0.5, 0.5), sound weapons/rocklx1a.wav. [read]
- (f) Like 5, ignores cl_disable_explosions, cl_disable_particles and cl_dlight_hacks, and
  follows cl_smooth_explosions. [read]

## 29. Tunnel sparks

- (a) byte (count), position, direction byte, byte (palette colour): 9 bytes. [read]
- (b) count particles of exactly the given colour; start as 26 (per axis -4..+3, plus the
  normal times 0..7); velocity crand × 20 per axis; an UPWARD acceleration of 40 (gravity
  reversed); alpha 1, life 0.5..0.8 s. [read]
- (c), (d), (e) None. [read]

## 30. Blaster2 (green blaster hitting a wall)

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles, colour 0xd0 plus rand&7 (208..215). Start: the point, per axis -4..+3, plus
  the normal times a whole 0..15. Velocity: the normal times 30 plus crand × 40 per axis.
  Gravity. Alpha 1, life 0.5..0.8 s. Identical to the blaster's (2) but for colour. [read]
- (c) models/objects/explode/tris.md2, skin 1, puff class with 4 frames (300 ms, alpha 1 to 0),
  full bright and translucent, turned to the impact normal exactly as the blaster's (2). [read]
- (d) Radius 150 × the puff's alpha, colour (0, 1, 0). [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]

## 31. Railtrail2

- (a) position (start), position (end): 12 bytes. [read]
- (b), (c) Never the default particle trail of 3, even at cl_railtrail_type 0: it always takes
  the core-beam path of the base note's settings section. A core beam is drawn when
  cl_railcore_width is above 0 (default 2: so by default a red core beam that fades over
  cl_railtrail_time, 1 s), and the coloured spiral only when cl_railtrail_type is 2 or more.
  [read] With cl_railcore_width 0 and cl_railtrail_type below 2 nothing is drawn. [derived]
- (d) None. [read]
- (e) weapons/railgf1a.wav, volume 1, attenuation normal, at the END position. [read]

## 32. Flame

- No layout: the client does not accept this kind and drops the connection with a "bad type"
  error if it arrives. [read]

## 33. Lightning

- (a) short (source entity), short (destination entity), position (start), position (end):
  16 bytes. [read]
- (b) None. [read]
- (c) A cable (the base note's 128-slot cable pool) of models/proj/lightning/tris.md2. [read]
  - Segment length 35 units. The length used is the start-to-end distance minus 20 units, so
   the cable stops short of the end. The count is that length divided by 35, rounded up. [read]
  - When that count is 1 or less (a corrected length of 35 or less, including zero or negative),
   one segment is drawn AT THE END point, pitched and yawed along the start-to-end line, with a
   random roll 0..359 degrees. [read]
  - Otherwise the segments start at the start point, spaced evenly so the last starts 35 units
   short of the corrected length, each oriented with the line's pitch negated and its yaw
   turned 180 degrees (the model faces back toward the start), each with a random roll 0..359
   re-chosen every rendered frame. [read] The random roll comes from a generator reseeded
   every 16 ms of real time and restarted at each rendered frame, so the shimmer changes at
   most about 60 times a second whatever the frame rate. [read]
  - Full bright, model frame 0, default scale. [read]
  - The start is the message's start position, except that when the source entity is the local
   player the player's drawn origin (not the eye) is used. The end is always the message's end
   position. [read]
  - Keyed by the pair (source entity, destination entity): a new message with the same pair
   replaces the cable at once, even an expired one. Otherwise it takes an empty or expired slot,
   or is dropped when none is free. [read]
  - Lives 200 ms from the message. [read]
- (d) None. [read]
- (e) weapons/tesla.wav, volume 1, attenuation normal, played on the SOURCE entity (it follows
  that entity), on its weapon channel, so each new message restarts the sound and cuts off the
  previous one on that entity. [read]

## 34. Debug trail

- (a) position, position: 12 bytes. [read]
- (b) One particle every 3 units from the start (count: the length divided by 3, rounded up),
  each exactly on the line, colour 0x74 plus rand&7 (116..123), no velocity, no acceleration,
  alpha 1 falling 0.1 a second, so a life of 10 s. [read]
- (c), (d), (e) None. [read]

## 35. Plain explosion

- (a) position: 6 bytes. [read]
- (b) None. [read]
- (c) models/objects/r_explode/tris.md2, explosion class with 15 frames, base frame 0 or 15 at
  even odds, random whole yaw 0..359, full bright, default scale: the model of 5. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5). [read]
- (e) None. [read]
- (f) Ignores the disable and light-hack settings; follows cl_smooth_explosions. [read]

## 36. Flashlight

- (a) position, short (entity): 8 bytes. Note the order: the position comes first. [read]
- (b), (c) None. [read]
- (d) Radius 400, colour (1, 1, 1), constant, lives 100 ms. It is keyed by the entity: a new
  message for the same entity re-uses that light slot (so a flashlight resent every frame is
  one light that moves). [read]
- (e) None. [read]

## 37. Force wall

- (a) position (start), position (end), byte (palette colour): 13 bytes. [read]
- (b) Every 4 units along the line (the length divided by 4, rounded up, steps), a particle with
  odds 7 in 10. Colour: exactly the given byte. Start: the step's point plus crand × 3 per
  axis. Velocity: straight down at 30..50 units/s, no sideways speed. No acceleration. Alpha 1,
  life 3.0..3.5 s. [read]
- (c), (d), (e) None. [read]

## 38. Heat beam (the player's)

- (a) short (source entity), position (start), position (end): 14 bytes. [read]
- (b), (c) A beam of models/proj/beam/tris.md2 in the player-beam pool, plus particle rings for
  the local player's own beam. [read]
- The player-beam pool. A second pool of 128 slots, apart from the cable pool. [read] A message
  replaces any slot with the same source entity (whatever kind made it, and even if expired) and
  gives it 200 ms. Otherwise it takes an empty or expired slot and gives it only 100 ms. With no
  free slot it is dropped. Kinds 38, 39, 59 and 61 share this pool and this key. [read] The model
  offset carried by 38 is (2, 7, -3). [read]
- When the source is the local player, every rendered frame: [read]
  - Start: the view origin (the eye; in third person the camera's position) plus the gun offset
   interpolated between the last two player states, then moved along the view's right axis by
   the offset's 2 times the hand factor, along the view's forward by 7, and along the view's up
   by -3. The hand factor is -1 with cl_gun 3, +1 with cl_gun 2, otherwise 0 with hand 2
   (centre), -1 with hand 1 (left), +1 else. With the centre hand the start is lowered 1 unit
   more. [read]
  - With cl_gunfov above 0 (default 90; clamped to 30..160), the right and up parts are scaled
   by the ratio of the tangents of the half view angle to the half gun angle, horizontally and
   vertically (the gun's vertical angle taken for a 4:3 view). At the default fov 90 and
   cl_gunfov 90 the ratio is 1. The forward part is not scaled. [read and derived]
  - Direction: straight along the view's forward axis, NOT toward the server's end point. Its
   length is the distance from that start to the server's end point. [read]
  - Model segments: 32 units each, count the length divided by 32 rounded up, spaced so the
   last starts 32 short of the end (as cables do). Model frame 1. Full bright. Oriented with
   the line's pitch negated and yaw turned 180 degrees. Roll: the client time in milliseconds
   modulo 360, in degrees, the same for every segment, so the beam spins at 1000 degrees a
   second. Default scale. [read]
  - Rings: one ring every 32 units along the beam, the first at the client time in seconds times
   96, modulo 32, units from the start, so the rings slide forward at 96 units/s and the
   pattern repeats every third of a second. [read] Each ring is 20 particles [measured: the
   angle stepping a tenth of a half turn in single precision stops after 20], evenly spaced
   round a circle of radius 1.5 units in the plane of the view's right and up axes. A ring
   whose distance along the beam (rounded down to a whole unit) is under 10 is shrunk to that
   distance over 10 of its radius, so the beam appears to begin at a point. [read] Colour 223
   minus rand&7 (216..223). Alpha 0.5, fading 1000 a second, so each ring particle lives one
   rendered frame and the rings are rebuilt every frame. No velocity, no acceleration. [read
   and derived]
  - The rings are not drawn while the game is paused; the model is. [read]
- When the source is another player (the offset is not zero): the start is the message's start
  position, moved 1 unit to the right of the start-to-end line, 7 units forward along it and
  7 units down (relative to the line's own up axis); the beam then runs to the message's end
  point. Model frame 2, same segment length, orientation and spinning roll. No rings. It does
  not follow the shooter between messages. [read and derived]
- (d), (e) None from the client. [read]

## 39. Monster heat beam

- (a) short (source entity), position (start), position (end): 14 bytes. [read]
- (b), (c) As 38 in the player-beam pool, with a zero offset. [read]
  - A monster's beam (source not the local player): it starts exactly at the message's start
   position and runs to its end point, model frame 2, the same 32-unit spinning segments. No
   rings. [read]
  - Every rendered frame a shell of 40 one-frame particles is drawn at sphere directions 10 units
   from the start point, palette 224 exactly, no velocity. [read]
  - If the source entity is the local player, it is drawn as the player's own beam of 38 (frame
   1, rings, view-locked) but with a zero offset (so it starts at the eye plus gun offset, and
   1 unit lower with the centre hand). [read]
- (d), (e) None. [read]

## 40. Steam

- (a) short (id), byte (count), position, direction byte, byte (palette colour), short
  (magnitude), and then a long (duration in milliseconds) ONLY when the id is not -1:
  14 bytes with id -1, 18 bytes otherwise. [read]
- (b) A steam puff: count particles, colour the given byte plus rand&7. Start: the point plus,
  per axis, magnitude × 0.1 × crand. Velocity: the direction times magnitude, plus each of the
  two side axes times crand × magnitude / 3. Acceleration 20 downward (half gravity). Alpha 1,
  life 0.5..0.8 s. [read]
  - Id -1: one puff, at once. [read]
  - Any other id: a slot in the sustain pool that throws one puff at once and then one every
   100 ms of client time until the duration has passed, so about duration/100 + 1 puffs. [read
   and derived] The id is not used to find or replace an earlier steam. [read]
- (c), (d), (e) None. [read]

## 41. Bubble trail 2

- (a) position, position: 12 bytes. [read]
- (b) One particle every 8 units from the start (at distances 0, 8, 16, ... while below the
  length). Colour 4 plus rand&7 (4..11). Start: the step's point plus crand × 2 per axis, then
  4 units lower. Velocity: crand × 10 per axis plus 20 upward. No acceleration. Alpha 1, life
  1.0..1.1 s. [read]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the start position. [read]

## 42. More blood

- (a) position, direction byte: 7 bytes. [read]
- (b) 250 particles drawn as the gunshot's puff of 0 (start -4..+3 per axis plus the normal
  times 0..31, velocity crand × 20, gravity, life 0.5..0.8 s) with colour 0xe8 plus rand&7
  (232..239), the base blood's colour. [read]
- (c), (d), (e) None. [read]
- (f) NOT removed by the blood bit (16) of cl_disable_particles: that bit is checked only for
  kind 1. [read]

## 43. Heat beam sparks

- (a) position, direction byte: 7 bytes. [read]
- (b) A steam puff (40's) of 50 particles, magnitude 60, colour 8 plus rand&7 (8..15): start
  within ±6 per axis, velocity 60 along the normal plus up to ±20 on each side axis,
  acceleration 20 downward, life 0.5..0.8 s. [read and derived]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]

## 44. Heat beam steam

- (a) position, direction byte: 7 bytes. [read]
- (b) As 43 but 20 particles of colour 0xe0 plus rand&7 (224..231). [read]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point. [read]

## 45. Chainfist smoke

- (a) position: 6 bytes. [read]
- (b) A smoke puff: 20 particles, colour 0 plus rand&7 (0..7). Start: the point plus 2 × crand
  per axis. Velocity: 20 straight up, plus each of the two side axes of "up" times crand × 20/3.
  No acceleration (unlike the steam puff). Alpha 1, life 0.5..0.8 s. [read]
- (c), (d), (e) None. [read]

## 46. Electric sparks

- (a) position, direction byte: 7 bytes. [read]
- (b) 40 particles drawn as the gunshot's puff of 0, colour 0x75 plus rand&7 (117..124). [read]
- (c), (d) None. [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal, at the point, on the ordinary channel
  (any number can overlap, unlike 12 and 13). [read]

## 47. Tracker explosion

- (a) position: 6 bytes. [read]
- (b) 128 particles, all palette 0 (black). Start: per axis an integer -16..+15. Velocity: per
  axis an integer -128..+127 units/s. Gravity. Alpha 1, alpha velocity -0.4/(0.6 + 0.2·frand),
  so life 1.5..2.0 s. [read]
- (c) None. [read]
- (d) A NEGATIVE light: radius 150, colour (-1, -1, -1), constant, lives 100 ms. It darkens the
  world and models around the point. It has no key, so it takes any expired light slot (the
  first slot when all 64 are alive). [read]
- (e) weapons/disrupthit.wav, volume 1, attenuation normal, at the point. [read]

## 48. Teleport effect

- (a) position: 6 bytes. [read]
- (b) A lattice of 9 × 9 × 13 = 1053 particles: x and y offsets -16..+16 in steps of 4, z
  offsets -16..+32 in steps of 4, each plus a whole 0..3 per axis. Colour 7 plus rand&7
  (7..14). Velocity: the direction of (y offset, x offset, z offset) times 8 (note x and y
  swapped), normalised, times a whole 50..113 units/s, so the cloud bursts outward; the one
  particle at the zero offset does not move. Gravity. Alpha 1, life 0.3..0.44 s in steps of
  0.02 s. [read and derived]
- (c), (d), (e) None from the client. [read]

## 49. Death-ball goal

- Exactly as 48: same 6 bytes, same particles, nothing else. [read]

## 50. Widow beam-out

- (a) short (id), position: 8 bytes. [read]
- (b) A sustain lasting 2100 ms. On EVERY rendered frame of that time it draws 300 one-frame
  particles at sphere directions from the point, at a radius growing linearly from 0 at the
  message to 45 units at 2.1 s. Colours, with even odds: palette 16, 104, 168, 144. No
  velocity. [read]
- (c), (d), (e) None. [read]

## 51. Nuke blast

- (a) position: 6 bytes. [read]
- (b) A sustain lasting 1000 ms. On EVERY rendered frame of that time it draws 700 one-frame
  particles at sphere directions from the point, at a radius growing linearly from 0 at the
  message to 200 units at 1 s (so 200 units/s), making a hollow expanding shell. Colours, with
  even odds: palette 110, 112, 114, 116. No velocity, no fade within the frame. [read]
  - The particle cost is 700 per rendered frame (for example 42 000 a second at 60 frames a
   second), each living one frame; it competes with every other effect for the 8192-particle
   pool. [derived]
  - When the shell is complete (1 s) it vanishes at once; nothing lingers. [read]
- (c) None. [read]
- (d) None: the client adds no light for it. [read]
- (e) None: the client plays no sound for it. [read]
- Screen effect: none from the client; any white-out the viewer sees must come from the
  server's player-state screen blend, which this kind does not touch. [read and derived]
- If all 32 sustain slots are busy the blast is not drawn at all. [read]

## 52. Widow splash

- (a) position: 6 bytes. [read]
- (b) 256 particles at sphere directions 45 units from the point, moving outward along that
  direction at 40 units/s, no acceleration. Colours, with even odds: palette 16, 104, 168, 144.
  Alpha 1, alpha velocity -0.8/(0.5 + 0.3·frand), so life 0.625..1.0 s. [read]
- (c), (d), (e) None. [read]

## 53. Explosion1 big

- (a) position: 6 bytes. [read]
- (b) None: no particles. [read]
- (c) The explosion model of 35 (models/objects/r_explode/tris.md2, 15 frames from base 0 or 15,
  random yaw, full bright) drawn at scale 2. [read]
- (d) Radius 350 × alpha, colour (1, 0.5, 0.5): the light is not scaled with the model. [read]
- (e) weapons/rocklx1a.wav, volume 1, attenuation normal. [read]
- (f) Ignores the disable and light-hack settings; follows cl_smooth_explosions. [read]

## 54. Explosion1, no particles

- (a) position: 6 bytes. [read]
- (b) None. [read]
- (c), (d) The model and light of 35 (default scale). [read]
- (e) weapons/rocklx1a.wav, volume 1, attenuation normal. [read]
- (f) As 53. [read]

## 55. Flechette

- (a) position, direction byte: 7 bytes. [read]
- (b) As 30 but colour 0x6f plus rand&7 (111..118). [read]
- (c) As 30 but skin 2 of models/objects/explode/tris.md2. [read]
- (d) Radius 150 × the puff's alpha, colour (0.19, 0.41, 0.75). [read]
- (e) weapons/lashit.wav, volume 1, attenuation normal. [read]

## 56..63 and 128: the kinds after the mission packs

The client also accepts these, added after the mission packs for the re-release's protocol, and
they are listed here so the range 25..63 is complete. [read]

- 56, blue hyperblaster (corrected): position, direction byte (7 bytes). The blaster's 40
  particles of kind 2 only (colour 224..231, along the normal): no model, light or sound. [read]
- 57, BFG zap: position, position (12 bytes). The BFG laser of 23 from start to end, plus the
  BFG explosion of 20 (sprite and green light) at the END point. No sound. [read]
- 58, berserker slam: position, direction byte (7 bytes). [read]
  - 700 particles, all starting exactly at the point; velocity the normal times 0..192 (frand)
   plus each of its two side axes times crand × 192; no acceleration; colours palette 110, 112,
   114, 116 at even odds; life 0.5..0.8 s.
  - models/objects/explode/tris.md2, skin 2, scale 3, puff class with 4 frames (300 ms), full
   bright, translucent, turned to the normal as the blaster's puff is.
  - Light 550 × the puff's alpha, colour (0.19, 0.41, 0.75). No sound.
- 59, grapple cable (player-locked): short (source entity), position, position (14 bytes). In
  the player-beam pool of 38, models/ctf/segment/tris.md2, offset (9, 12, -3). [read] For the
  local player it starts at the eye-and-gun point as 38 does but its direction is toward the
  server's end point (not the view's forward), and with a non-centre hand the cable is moved
  forward by half a segment (15 units) and shortened by as much, since the model's origin is
  at its middle. Segments of 30 units, lit by the world, no shadow, a random roll per segment
  each frame. For another player it starts at the message's start moved by the offset rule of
  38 (here 8 units right, 12 forward along the line, 7 down). [read and derived]
- 60, power splash: short (entity), byte (count) (3 bytes). An entity number at or above the
  entity limit drops the connection; an entity not in the current frame draws nothing. The count
  is read but not used. 256 particles at sphere directions on a sphere around the centre of the
  entity's box, at the entity's radius, moving outward at 40 units/s; colours palette 208..211
  at even odds; no acceleration; life 0.5..0.8 s. No model, light or sound. [read]
- 61, lightning beam (player-locked): short (source entity), position, position (14 bytes).
  In the player-beam pool of 38, models/proj/lightning/tris.md2, offset (0, 12, -12), with the
  lightning's 35-unit segments, the 20-unit shortening and the single-segment-at-the-end case
  of 33. For the local player it runs along the view's forward axis as 38 does. For another
  player it starts at the message's start moved by 38's offset rule (here 1 unit left, 12
  forward along the line, 2 up). [read and derived] No sound from the client (unlike 33). [read]
- 62, explosion1 without light, and 63, explosion2 without light: position (6 bytes). As 5 and 6
  respectively (particles, model, sound) but with no dynamic light. [read]
- 128, damage dealt: short (amount), 3 bytes. With an amount above 0 and cl_hit_markers above 0
  (default 2) the client starts its hit-marker display with that amount; with cl_hit_markers 2
  or more it also plays weapons/marker.wav at full volume with no attenuation, on a single-sound
  channel of the listener (one at a time). How the marker is drawn is outside this note. [read,
  drawing open]

## Settings

- cl_disable_explosions, cl_disable_particles and cl_dlight_hacks change NONE of the kinds in
  this note. In particular the blood bit (16) of cl_disable_particles removes only kind 1: green
  blood (26) and more blood (42) are always drawn. [read]
- cl_smooth_explosions (default 1) sets the alpha, translucency, skin steps and light curve of
  every r_explode explosion here exactly as for the base explosions: 28, 35, 53, 54, 62, 63.
  [read] It does not touch the 4-frame puffs of 30, 55, 58 (their alpha is always the puff
  class's linear fade). [read]
- cl_railtrail_type, cl_railtrail_time, cl_railcore_color, cl_railcore_width,
  cl_railspiral_color, cl_railspiral_radius: draw 31 (which never uses the default trail).
  [read]
- hand (default 0), cl_gun (default 1) and cl_gunfov (default 90): place the start of the local
  player's beams of 38, 39 (from the local player), 59 and 61, as described under 38. [read]
- The game's pause: stops the local heat beam's rings (the model stays). [read]
- cl_hit_markers (default 2): 128. [read]
- The global switches of the base note apply as there: cl_particles 0 removes every particle
  here, cl_lights 0 every light (including the tracker's negative one and the flashlight),
  cl_entities 0 every model (explosions, puffs, cables, heat beams, lightning, the BFG zap's
  laser and sprite). [read]
- gl_dynamic 0 stops dynamic lights lighting the world, so the tracker's darkening is lost with
  it. [derived]

## Shared routines

- 26 and 25 and 29 throw the same small puff (per axis -4..+3, the normal times 0..7, crand ×
  20) of one exact colour (no spread); 25 and 26 fall with gravity, 29 rises. 26's count and
  colour are fixed; 25 and 29 take them from the wire. [read]
- 42 and 46 are drawn as 0's puff (the base gunshot's), 42 with 250 particles of the blood
  colour and no smoke, flash or sound, 46 with 40 of 117..124 and the lashit sound. [read]
- 30 and 55 are drawn the same as the base blaster (2) except their particle colours (208..215,
  111..118), the explode model's skin (1, 2) and the light's colour ((0, 1, 0), (0.19, 0.41,
  0.75)). Same particle pattern, puff, orientation, light radius and sound. [read]
- 58's puff and light use that same blaster-family routine with skin 2, scale 3 and radius 550.
  [read]
- 56 is the particle part of 2 alone. 27 is the same routine fed a position as its normal. [read]
- 28 is drawn the same as 5 with no exception. [read]
- 35, 53 and 54 are 5's model and light: 35 without particles or sound, 54 without particles,
  53 without particles and at scale 2. 62 and 63 are 5 and 6 without light. [read]
- 43 and 44 are the same steam puff (40's) at magnitude 60 with the lashit sound, differing in
  count (50, 20) and colour (8..15, 224..231). 45 is the same puff shape without its
  downward acceleration, along straight up. [read]
- 49 is drawn the same as 48 with no exception. [read]
- 50 and 51 are the same expanding one-frame shell, differing in duration (2.1 s, 1 s), final
  radius (45, 200), particles a frame (300, 700) and colours. 52 is 50's colours as a single
  burst that flies outward instead. [read]
- 38, 39, 59 and 61 are one player-locked beam routine differing in model, offset and segment
  length; only the heat beam model draws rings (local) or the shell (a monster's). [read]
- 33 is the base cable routine of 16 with the lightning model's 35-unit segments, the 20-unit
  shortening, the reversed orientation and full brightness. [read]
- 31 is the base rail routine with the default trail switched off. [read]
- 57 is 23 plus 20 at the end point. [read]
