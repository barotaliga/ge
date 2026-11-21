# Quake II client-side entity effects: answers (clean-room reader)

Landed in the client effects round (flies, the power screen's plate, the shells, the thinning trails, missiles' effect bits on the wire).

Source read: the stock Quake II 3.21 tree (client, ref_gl, ref_soft, game, qcommon). Facts are in
plain words; no code. [read] = seen directly in the source; [derived] = worked out from what was
read. File names are base names only, for traceability.

Palette RGB values were read from the stock 8-bit palette (the last 768 bytes of
pics/colormap.pcx in the full game's pak0).

## 0. Shared machinery every answer below depends on

### 0.1 The particle system (cl_fx, client.h)
- A particle has: a birth time (the client clock in milliseconds), a start position, a velocity,
  an "acceleration", a palette colour index, a starting alpha and an alpha velocity (alpha change
  per second). [read]
- Every rendered frame the client recomputes each particle from its birth state, never steps it:
  t = (now − birth) in seconds; alpha = start alpha + t × alpha velocity; if that alpha is 0 or
  below the particle is freed and not drawn; alpha is clamped to at most 1. [read]
- Position = start + velocity × t + acceleration × t². NOTE: the acceleration term is multiplied
  by t squared WITHOUT the usual one-half, so an "acceleration" value of A behaves like a true
  physical acceleration of 2A. [read] (the consequence is [derived])
- The "gravity" constant used for particles is 40 (units per second squared as stored); so a
  particle given −40 falls as if under a physical 80 u/s². [read] constant; [derived] the
  physical figure.
- There is a colour velocity field, but the particle update never applies it: colour is fixed
  for the particle's life. [read]
- Particles come from a fixed pool of 4096 (MAX_PARTICLES); when the pool is empty an emitter
  stops emitting for that call. [read]
- Lifetime = start alpha / |alpha velocity|; e.g. alpha velocity −1/(1 + r×0.4) means a life
  of 1 to 1.4 s. [derived]
- Random helpers: "frand" is uniform 0..1 (a 15-bit random scaled), "crand" is uniform −1..+1.
  "rand & N" gives an integer 0..N. [read]

### 0.2 How a particle is drawn (ref_gl gl_rmain, gl_rmisc)
- GL path (default, no point-parameter extension): each particle is one triangle with a corner
  at the particle, one corner 1.5 units along the view's up vector and one 1.5 units along the
  view's right vector, all scaled by a distance factor: if the particle's depth along the view
  direction is under 20 units the factor is 1, otherwise 1 + 0.004 × depth. [read]
- The triangle is textured with an 8×8 white "dot" texture whose alpha is a small round blob
  (a 2-4-4-2 pixel diamond in the upper-left 4×4 of the 8×8 texel grid; the rest transparent);
  texture coordinates run 0.0625 → 1.0625, so the triangle shows one soft round dot. [read]
- Colour is the palette RGB of the index, alpha = particle alpha × 255; blend is source-alpha /
  one-minus-source-alpha; no depth writes (depth test on). [read]
- With the point-parameter extension on, particles are GL points of size gl_particle_size
  (cvar), untextured, same colour and alpha. [read]
- Software renderer: particles are small squares whose pixel size depends on depth, blended at
  three levels by alpha: above 0.66 opaque, above 0.33 the 66 % table, otherwise the 33 % table.
  [read]

### 0.3 Entity dynamic lights (cl_view, ref_gl gl_light)
- An entity light is added for one rendered frame only (re-added every frame while the bit is
  on): a position, an "intensity" (which is the radius in world units) and an RGB colour of
  floats (usually 0 or 1). [read]
- On world surfaces (GL lightmap path) the light reaches a surface only if the intensity minus
  the distance to the surface's plane is at least 64 (DLIGHT_CUTOFF); within the plane the added
  light falls off linearly as (intensity − approximate in-plane distance) × colour. [read]
- On models (GL) a dynamic light adds (intensity − distance)/256 × colour to the model's shade
  light when positive. [read]
- A negative colour (used by the Rogue tracker effects, not stock single player) subtracts
  light. [read]

### 0.4 Colour shells (cl_ents, ref_gl gl_mesh, ref_soft r_alias, ref.h)
- When an entity has the colour-shell effect bit, the client draws the model TWICE: first
  normally (with no render flags), then a second copy flagged translucent with alpha 0.30 and the
  shell render bits. [read]
- The shell copy (GL): texturing disabled, every vertex pushed outward along its stored vertex
  normal by 4.0 units (POWERSUIT_SCALE), drawn in one flat constant colour (no per-vertex
  shading) at alpha 0.30, blended source-alpha/one-minus-source-alpha after all opaque entities,
  with no depth writes. [read]
- Shell colour (GL), from the shell render bits: [read]
  - red bit alone: RGB (1, 0, 0)
  - green bit alone: (0, 1, 0)
  - blue bit alone: (0, 0, 1)
  - red + green + blue together (god mode): (1, 1, 1) white
  - red + blue (quad and pent together): (1, 0, 1) magenta
  - Rogue extras, not in stock single player: double damage (0.9, 0.7, 0), half damage
    (0.56, 0.59, 0.45), blue + double (0, 1, 1).
  - When green is combined with red or blue (but not all three), the red/blue branch wins and the
    green bit is ignored; e.g. quad (blue) plus a power shield (green) draws pure blue; pent (red)
    plus a power shield draws pure red. [derived from the branch order]
- Shell colour (software renderer): a palette index used as a constant fill at 33 % (alpha 0.30
  is not above 0.33). [read]
  - red: index 242 (0xF2) = RGB (255, 0, 0)
  - green: index 208 (0xD0) = RGB (0, 255, 0)
  - blue: index 243 (0xF3) = RGB (0, 0, 255)
  - red+green+blue (god): white index 215 (0xD7) = RGB (255, 255, 255)
  - red+blue: index 104 (0x68) = RGB (179, 91, 79) (this is what the table says; it is not
    magenta in the palette)
  - unused-in-stock: red+green 0xDC (255,255,39), blue+green 0x78 (7,55,75), cyan 0x72
    (71,119,139), double 0xDF (255,191,15), half damage 0x90 (151,159,123).
- Software shell vertices are also pushed out 4.0 units along the normals. [read]

### 0.5 The player's own entity
- When the client walks the packet entities and meets its own player entity, it marks it
  viewer-model and then SKIPS it entirely (it is not added to the scene; no shell, no power
  screen, no trails). The only thing done for the own entity is a light for the CTF flag bits
  (radius 225; flag 1 colour (1, 0.1, 0.1), flag 2 (0.1, 0.1, 1)). So in the stock first-person
  view a player never sees his own god shell, quad shell or power screen; other players and
  monsters do. [read]

### 0.6 Order of the per-entity effect branches (cl_ents)
- Only ONE trail/light branch runs per entity per frame, chosen in this priority order:
  rocket, blaster, hyperblaster, gib, grenade, flies, BFG, trap, flag 1, flag 2, tag trail,
  tracker trail, tracker, green gib, ion ripper, blue hyperblaster, plasma. The branch block is
  skipped entirely if the only effect bit set is EF_ROTATE. [read]
- After the branches, the entity's "last origin" (used as the trail's start next frame) is set to
  this frame's interpolated origin. When an entity first appears (or reappears after missing a
  frame, or teleports, or jumps more than 512 units on an axis) the trail start is reset to the
  entity's previous origin as sent by the server (or its current origin for a teleport), and its
  trail counter is reset to 1024. [read]

## 1. EF_FLIES (value 0x4000 = 16384)

When the game sets it [read, g_monster, m_infantry, m_mutant]:
- Only two stock monsters call the fly check on death: the infantry and the mutant (when their
  death animation ends). If the corpse is in water, nothing. Otherwise a 50 % chance; if it
  passes, flies switch on 5 + 10 × random seconds later (5 to 15 s). Switching on is again
  refused if the corpse is in water.
- On: the effect bit is set and the entity's looping sound is set to "infantry/inflies1.wav";
  60 seconds later both are cleared (bit off, sound none).
- If the corpse is gibbed while it has flies, the head-throw clears the flies bit and the sound.
- (A commented-out line would have put flies on the teleporter destination; inactive.)

The sound [read, snd_dma]: the client plays an entity's looping sound continuously at full master
volume (255) from the entity's origin, attenuated with the loop factor 0.003 per unit beyond a
full-volume radius of 80 units (the same as the "static" attenuation level). The client adds no
other sound of its own for flies.

How many particles, and the time ramp [read, cl_fx]:
- The client keeps, per entity, a "fly stop time". When the client draws a flies entity and the
  current client time is past that stop time, it starts a new cycle: start = now, stop = now +
  60 000 ms. Otherwise start = stop − 60 000.
- Let n = milliseconds since the cycle start. If n < 20 000, count = n × 162 / 20 000 (truncated
  to an integer). Otherwise let m = milliseconds left until the stop time; if m < 20 000,
  count = m × 162 / 20 000; else count = 162.
- So: a linear rise from 0 to 162 over the first 20 s, a hold at 162 for 20 s, a linear fall to 0
  over the last 20 s, then (if the bit is still on) a new cycle starts. Because the game keeps the
  bit for exactly 60 s, normally one cycle is seen. [derived]
- The per-entity stop time is not cleared when the bit goes off, so if flies come back on within
  the old window the old cycle continues. [derived]
- The count is capped at 162 (NUMVERTEXNORMALS). The emitter then makes one particle for every
  EVEN index from 0 up to count − 1 (index steps by 2): ceil(count / 2) particles, at most 81.
  [read] (the 81 is [derived])

Where each particle sits [read]:
- Each particle index i uses i-th entry of the standard table of 162 precomputed unit normals
  (the same table the model format's vertex normals use; anorms) as a fixed direction.
- A shared table of 162 × 3 angular rates is filled once (first time flies or BFG particles are
  drawn), each rate = (random integer 0..255) × 0.01, i.e. 0 to 2.55 radians per second.
- With T = client time in seconds: an angle yaw = T × rate[i][0], pitch = T × rate[i][1];
  a "forward" unit vector is built from them: x = cos(pitch) cos(yaw), y = cos(pitch) sin(yaw),
  z = −sin(pitch). (The third rate is computed but unused.)
- A distance d = 64 × sin(T + i) (the index i added as radians).
- Position = the entity's interpolated origin + normal[i] × d + forward × 16.
- So each fly oscillates through the corpse's origin along its own fixed direction with
  amplitude 64 units and a period of 2π seconds (phases offset by the index), while a 16-unit
  offset swings round on a slowly varying random rotation. They form a cloud up to about 80 units
  from the origin (origin is the entity origin, no vertical offset). [derived]

Colour, alpha, size, lifetime [read]:
- Palette index 0 = RGB (0, 0, 0), black.
- Start alpha 1, alpha velocity −100 per second; no velocity, no acceleration.
- The particles are re-created every rendered frame at their computed spot: a particle drawn
  in the frame it is made is gone 10 ms later (alpha would reach 0 at t = 0.01 s), so in effect
  each fly is drawn once per frame at full alpha. [derived]
- Size: the standard particle (section 0.2), about a 1.5-unit dot scaled by distance.

## 2. EF_POWERSCREEN (0x200 = 512) and the power shield

When the game sets the bits [read, g_combat, p_view, g_monster]:
- Neither bit is set "while on". The power armour code, each time it actually absorbs damage,
  stamps a "power armour time" = now + 0.2 s. Each server frame the effects are rebuilt from
  scratch, and the bit is set only while now < that time. So the screen or the shell flashes for
  0.2 s (two server frames at 10 Hz) after each absorbed hit. [read; frames [derived]]
- Players: power screen → EF_POWERSCREEN; power shield → EF_COLOR_SHELL plus the green shell
  render bit (RF_SHELL_GREEN, 2048). Monsters: the same from their power armour type. Dead
  monsters (health ≤ 0) get neither. [read]
- Absorption rules (which decide when the flash happens) [read]: no armour effect for damage
  flagged "no armour". Power screen only absorbs when the hit point is in front: the direction
  from the entity's origin to the hit point, dotted with the entity's facing, must exceed 0.3;
  it absorbs a third of the damage, 1 point per cell. Power shield absorbs two thirds of the
  damage, 2 points per cell. Absorbed amount is capped by cells × points-per-cell; zero cells or
  zero absorbed → no flash.
- The hit also spawns a temporary entity at the hit point along the surface normal:
  - Screen: 40 particles of palette base 208 (0xD0) plus random 0..7 → indices 208–215, i.e.
    greens from (0,255,0) through (95,123,51), and 215 is white (255,255,255).
  - Shield: 40 particles of base 176 (0xB0) plus 0..7 → indices 176–183, pale blue-violet
    from (119,123,207) down to (71,71,115).
  - Both: each particle starts at the hit point + an integer offset −4..+3 on each axis + the
    normal × (random integer 0..31); velocity crand × 20 per axis; acceleration −40 down; alpha 1,
    alpha velocity −1/(0.5 + frand × 0.3) → life 0.5 to 0.8 s.
  - Sound: "weapons/lashit.wav" at the hit point, volume 1, normal attenuation.
  [read, cl_tent, cl_fx]

What the client draws for the power screen [read, cl_ents, cl_tent]:
- An extra model "models/items/armor/effect/tris.md2", frame 0 (old frame 0, so no lerp), placed
  at the entity's interpolated origin with the entity's angles (it is added after the main model
  and linked models and shares their position/angles).
- It is flagged translucent AND green shell, alpha 0.30. Because the green shell bit is on, the
  renderer draws it as a shell: untextured, flat green (GL RGB (0,1,0); software index 208), every
  vertex pushed out 4 units along its normal, alpha 0.30. [read; the "it is drawn like a shell"
  consequence is [derived] from the shell path in 0.4]
- No scale other than the 4-unit normal push. No dynamic light for the power screen bit.

What the client draws for the power shield [read]: a green colour shell on the whole model as in
0.4 (green, alpha 0.30, pushed out 4 units). No light.

## 3. God mode

- The god command toggles the god flag on the player. [read, g_cmds]
- Each server frame, while the god flag is on (and the player is alive and not in
  intermission), the player's effects get EF_COLOR_SHELL (0x100) and the render bits red + green
  + blue (1024 + 2048 + 4096 = 7168). [read, p_view]
- Client: a second copy of the model, translucent alpha 0.30, untextured, pushed 4 units out
  along the vertex normals; GL colour white (1,1,1); software white palette index 215
  (255,255,255) at 33 % translucency. [read]
- No light is added for god mode. The player does not see his own shell in first person (0.5).
- The client's quad (EF_QUAD) and pent (EF_PENT) bits turn into shells the same way (see 5);
  since all three use the same shell bits, god + anything is white. [derived]
- (A monster with the god flag, e.g. the commander's body, gets no shell: only the player's
  effects function sets it.) [read]

## 4. EF_GIB (0x2) and the "diminishing" trail; grenade and rocket

EF_GIB, EF_GRENADE (0x20) and the smoke half of EF_ROCKET (0x10) share one routine. [read]

The trail routine [read, cl_fx]:
- It walks the straight segment from last frame's position to this frame's interpolated
  position in steps of 0.5 units (one attempt per half unit; the walk continues while the
  remaining length is above 0, so a segment of L units makes ceil(L / 0.5) attempts).
- Each attempt spawns a particle only if a random integer 0..1023 is below the entity's trail
  counter. The counter starts at 1024 when the entity appears (so the first attempts always
  spawn).
- After EVERY attempt (spawned or not) the counter drops by 5, with a floor of 100. So it falls
  by 10 per unit flown: from 1024 to 100 in about 92.4 units, after which the spawn chance stays
  at 100/1024 ≈ 9.8 % per attempt ≈ 0.195 particles per unit. Initially ≈ 2 particles per unit.
  [read; per-unit figures [derived]]
- Spread is chosen once per call from the counter at the start of the call: counter above 900 →
  position jitter 4 and velocity jitter 15; above 800 → 2 and 10; otherwise 1 and 5. Jitter means
  crand × value per axis. (So the spread is WIDEST at the start of the flight and narrows.)
  [read]
- There is no speed threshold; distance flown drives everything. [read]

Per-kind particles [read]:
- Gib (blood): colour index 232 (0xE8) + random 0..7 → 232–239: (155,31,0), (143,23,0),
  (127,15,0), (115,7,0), (95,0,0), (71,0,0), (47,0,0), (27,0,0) — dark reds to near black.
  Alpha 1, alpha velocity −1/(1 + frand × 0.4) → life 1.0–1.4 s. Position jitter and velocity
  jitter as above; then the vertical velocity is reduced by 40 (an initial 40 u/s downward
  push); acceleration 0 — no gravity, they drift down at constant speed.
- Green gib (mission pack bit, 0x200000; not set by the stock game): same as gib but colour 219
  (0xDB) + 0..7.
- Anything else (grenade and rocket smoke): colour index 4 + random 0..7 → 4–11, greys
  (63,63,63) up to (171,171,171) in steps of 16. Alpha 1, alpha velocity −1/(1 + frand × 0.2) →
  life 1.0–1.2 s. Position and velocity jitter as above; acceleration +20 upward (so it rises as
  a physical +40 u/s², per the rule in section 0.1).
- Lights: gib none; grenade none. [read]

Rocket (EF_ROCKET) [read]:
- First the diminishing smoke trail above (sharing the same counter).
- Then a fire trail along the same segment in 1-unit steps; each step spawns with a 1-in-8 chance
  (random 0..7 equals 0) ≈ 0.125 per unit, no counter. Colour 220 (0xDC) + 0..3 → (255,255,39),
  (255,235,31), (255,215,23), (255,191,15) yellow-orange. Alpha 1, alpha velocity
  −1/(1 + frand × 0.2) → life 1.0–1.2 s. Position jitter crand × 5, velocity crand × 20 per axis,
  acceleration −40 down.
- Light: radius 200, colour (1, 1, 0) yellow, at the rocket's position, every frame.
- Model "models/objects/rocket/tris.md2". The viper bomb (a map object) also uses the rocket bit.

Where the game sets EF_GIB [read, g_misc, g_items]: every thrown gib, a thrown monster head, a
player's thrown head (skull or head2 model), the target_spawner gib objects (arm, leg, head) and
the commander's head item. Grenade bit: hand and launcher grenades (models
"models/objects/grenade2/tris.md2" and "models/objects/grenade/tris.md2").

## 5. Other bits the stock single-player game sets that the client draws specially

Values in parentheses. [read unless marked]

- EF_ROTATE (0x1): the entity's angles are replaced by pitch 0, yaw = (client ms / 10) wrapped
  to 0..360 (i.e. 100 degrees per second, a full turn in 3.6 s), roll 0. Shared by all rotating
  items (one common phase). Set on most item world models (armour, weapons, powerups, keys).
  Not a trail; EF_ROTATE alone skips the trail block.
- RF_GLOW (512), set with every placed or dropped item: GL adds 0.1 × sin(render time × 7) to
  each shade-light channel (period ≈ 0.9 s), not letting a channel drop below 80 % of its base.
- EF_BLASTER (0x8): trail in 5-unit steps (one particle per 5 units), colour 224 (0xE0) =
  (255,171,7) orange; position jitter crand × 1, velocity crand × 5, no acceleration; alpha 1,
  alpha velocity −1/(0.3 + frand × 0.2) → life 0.3–0.5 s. Light radius 200, colour (1, 1, 0).
  Model "models/objects/laser/tris.md2".
- EF_HYPERBLASTER (0x40): no trail; light radius 200, (1, 1, 0). The player's hyperblaster sets
  it only on the bolts fired at gun frames 6 and 9 (other bolts carry no effect); some monsters
  use it too.
- EF_BFG (0x80) with EF_ANIM_ALLFAST on the flying ball ("sprites/s_bfg1.sp2"): the entity is
  drawn translucent with alpha 0.30; particles: one for each of the 162 normals every frame,
  positioned exactly as the flies (section 1) but for all 162 indices; each colour =
  floor(208 + 7 × r) where r = (particle's distance from the ball) / 90 → indices 208–215 (green
  to white); alpha 1 − r; alpha velocity −100 (one-frame). Light radius 200, colour (0, 1, 0).
  On impact the game swaps the model to "sprites/s_bfg3.sp2", frame 0, removes ANIM_ALLFAST and
  steps the frame each server frame; the light radius then follows the frame: 300, 400, 600, 300,
  150, 75 for frames 0–5, colour (0, 1, 0). (The impact itself sends a separate temp entity not
  covered here.)
- EF_ANIM01 (0x400), EF_ANIM23 (0x800), EF_ANIM_ALL (0x1000), EF_ANIM_ALLFAST (0x2000): the client
  overrides the frame sent: let a = integer of 2 × client ms / 1000 (2 per second). ANIM01 →
  frame a mod 2 (0,1 at 2 Hz); ANIM23 → 2 + (a mod 2); ANIM_ALL → a (all frames at 2 Hz);
  ANIM_ALLFAST → client ms / 100 (10 Hz). Checked in that order; the first that matches wins.
  For brush models the renderer uses this frame to pick the texture animation: frame modulo the
  texture's sequence length, counted from the base texture. For a sprite or alias model the frame
  is used as is (the renderer resets an out-of-range alias frame to 0 with a console warning).
  Set by: buttons (ANIM01 at rest, ANIM23 while pressed/at top), func_rotating (spawnflags 64 →
  ALL, 128 → ALLFAST), func_door (16 → ALL, 64 → ALLFAST), func_door_rotating (16 → ALL),
  func_wall (8 → ALL, 16 → ALLFAST), func_object (2 → ALL, 4 → ALLFAST), func_explosive (2 → ALL,
  4 → ALLFAST), and the BFG ball.
- EF_QUAD (0x8000): turned by the client into a colour shell with the blue bit (GL (0,0,1),
  software 243). The game sets it while quad time remains, except in the last 30 server frames
  (3 s) where it is set only when bit 2 (value 4) of the remaining frame count is set — on for 4
  frames, off for 4 (0.4 s / 0.4 s blink). No light. [read; durations [derived]]
- EF_PENT (0x10000): the same with the red bit (GL (1,0,0), software 242), same 3-second blink
  rule on invulnerability time. Quad + pent → red + blue shell ((1,0,1) in GL).
- EF_TELEPORTER (0x20000), on misc_teleporter (model "models/objects/dmspot/tris.md2", skin 1,
  looping sound "world/amb10.wav"): handled like an event — each time a server frame is parsed
  (10 per second), 8 particles: colour 219 (0xDB) = (255,255,83) pale yellow; x and y = origin −
  16 + random integer 0..31, z = origin − 8 + random 0..7; x/y velocity crand × 14; z velocity
  80 + random 0..7; acceleration −40 down; alpha 1, alpha velocity −0.5 → life 2 s. No light.
  (So ≈ 80 particles per second, ≈ 160 alive.) [read; rates [derived]]
- EF_COLOR_SHELL (0x100) with shell render bits: section 0.4. Set by god mode (white), the
  power shield (green), and on a monster being resurrected by a medic (red; AI_RESURRECTING).
- RF_TRANSLUCENT (32) as the ONLY render bit: the client sets alpha 0.70 (comment: "only used
  for the black hole"). The stock misc_blackhole ("models/objects/black/tris.md2") is the user.
  Translucent entities are drawn after opaque ones with no depth writes, textured and lit,
  blended at their alpha (software: above 0.66 opaque, so the 0.7 black hole is OPAQUE in
  software; above 0.33 the 66 % table; else 33 %). [read; the software consequence [derived]]
- RF_BEAM (128) with translucent (target_laser): drawn as a beam, alpha 0.30, its colour picked
  each frame at random from the four palette bytes packed in the skin number. (Beam geometry not
  covered here.)
- RF_FRAMELERP (64) (the viewthing, the commander body, turrets, and others): the origin is not
  interpolated between server frames; it snaps to the current origin (the frames do the motion).
- RF_FULLBRIGHT (8): the model's shade light is set to 1,1,1 (no stock single-player entity
  found setting it in game code).

Not set by the stock single-player game (mission-pack or CTF only, listed for completeness, drawn
by this client): EF_FLAG1/EF_FLAG2 (flag trails colour 242 red / 115 blue, light 225 (1,0.1,0.1)
/ (0.1,0.1,1)), EF_IONRIPPER (light 100, (1,0.5,0.5)), EF_GREENGIB, EF_BLUEHYPERBLASTER (light
200, (0,0,1)), EF_SPINNINGLIGHTS (red light 100 placed 64 ahead on a spinning yaw), EF_PLASMA
(alpha 0.6, light 130 (1,0.5,0.5)), EF_TRAP (light 100–199 random, (1,0.8,0.1)), EF_TRACKER,
EF_DOUBLE, EF_SPHERETRANS (alpha 0.3, or 0.6 with the tracker-trail bit), EF_TAGTRAIL,
EF_HALF_DAMAGE, EF_TRACKERTRAIL. Also linked models (second model index) with the top bit set
are drawn translucent at alpha 0.32 (Rogue). [read]

## 6. Entity light per effect bit (radius = intensity in world units; colour as RGB floats)

All [read], added every rendered frame at the entity's interpolated origin while the bit is on.

| Bit | Radius | Colour |
| --- | --- | --- |
| EF_ROCKET (0x10) | 200 | (1, 1, 0) yellow |
| EF_BLASTER (0x8) | 200 | (1, 1, 0) yellow |
| EF_HYPERBLASTER (0x40) | 200 | (1, 1, 0) yellow |
| EF_BFG flying ball (with ANIM_ALLFAST) | 200 | (0, 1, 0) green |
| EF_BFG explosion, frames 0–5 | 300, 400, 600, 300, 150, 75 | (0, 1, 0) green |
| EF_GIB, EF_GRENADE, EF_FLIES | none | — |
| EF_POWERSCREEN, power shield, god, quad, pent shells | none | — |
| EF_TELEPORTER, EF_ROTATE, EF_ANIM bits | none | — |
| EF_FLAG1 / EF_FLAG2 (CTF; also on own entity) | 225 | (1, 0.1, 0.1) / (0.1, 0.1, 1) |

Muzzle flashes and other temporary lights come from events and temp entities, not from effect
bits, and are not covered here.

## Not answered / caveats
- Nothing asked was left unanswered.
- The beam geometry for RF_BEAM and the BFG impact temp entity were out of scope.
