# The sequel's render effects and effect bits (M18v3d)

Clean-room answers, written by a reader and screened (`clean-room` skill). Each fact marked read, derived or measured. Built on in `games/shooters_edge/client/sequel_look.hpp`.

Source read: id Software's GPL v2 source release of the sequel game (the `quake2` repository on
GitHub): its base game module, its capture-the-flag game module, its client, and both of its
renderers (the OpenGL one and the 8-bit software one). Each fact is marked **[read]** (stated
directly by the release), **[derived]** (worked out from what it reads) or **[measured]**. No
fact here is measured: nothing was run.

Terms used below:

- "The GL renderer" and "the software renderer" are the release's two drawing back ends.
- "Light" on a model is three numbers, one per channel, where 1.0 is full brightness.
- "Light radius N" means a dynamic light whose reach is N world units: a surface at distance d
  from it gains N minus d (none past N). **[read]**
- Times are client time; one server frame is 100 ms (10 Hz). **[read]**
- Particle colours are indices into the game's 256-colour palette. The palette itself ships in
  the game's data, not in the source release, so the RGB of an index is not settled here.

## 1. Every render-effect bit

The field is a bit set. The release defines these bits **[read]**:

| Bit | Plain name | Meaning |
| --- | --- | --- |
| 1 | min light | the model's light never drops below a floor (Q4) |
| 2 | viewer's own model | do not draw it from the player's own eyes, only "in mirrors" (Q5) |
| 4 | weapon model | the first-person view weapon: only drawn through the player's eyes (Q5) |
| 8 | full bright | drawn at full light, ignoring the world's light (Q4) |
| 16 | depth hack | squeezed into the near part of the depth range so it never pokes into walls (Q5) |
| 32 | translucent | see-through (Q2) |
| 64 | frame lerp | the origin is not interpolated by the client; the frames carry the motion (Q6) |
| 128 | beam | drawn as a beam, not a model (already known) |
| 256 | custom skin | named "the skin is an index into the image list"; nothing in the release sets or reads it (Q6) |
| 512 | glow | the light pulses (Q7) |
| 1024 | red shell | |
| 2048 | green shell | |
| 4096 | blue shell | |
| 8192, 16384 | (none) | not defined |
| 32768 | IR visible | drawn solid red when the view wears infra-red goggles (Q6) |
| 65536 | double-damage shell | |
| 131072 | half-damage shell | |
| 262144 | use disguise | a player model is drawn with a "disguise" skin (Q6) |

The last four (32768 and up) were added for the mission packs; the base game never sets them
(Q3, Q6). **[read]**

Where the base game's server sets a render-effect bit **[read]**:

- **Players**: every server frame the player's bits are cleared to zero and rebuilt. While the
  player is alive and not at the intermission: the green shell while a power shield (not a power
  screen) is absorbing a hit; all three of red, green and blue when the player has god mode on.
  Quad and invulnerability go out as effects bits, not render bits (Q3). A dead player or one at
  the intermission has none.
- **Monsters**: every walking, flying and swimming monster gets frame lerp (64) when it starts.
  Each frame the shell bits are cleared, then: the red shell while a medic is bringing the
  monster back to life (set even while it is dead); and, alive only, the green shell while its
  power shield is absorbing a hit.
- **Items**: every item placed in a map, and every item dropped (by a player or a dying monster),
  gets glow (512) and nothing else. An item placed with the "no touch" spawn flag loses glow (and
  its spin).
- **Other entities**: the turret's driver and the dead commander's body get frame lerp; a
  developer "view thing" gets frame lerp alone; the black hole decoration gets translucent (32)
  alone; the target laser gets beam plus translucent.
- **Missiles, the view weapon**: no missile gets a render bit from the server. The view weapon is
  not an entity; the client gives it min light, depth hack and weapon model itself (Q5).
- **The capture-the-flag module** sets the same bits in the same places, and also gives glow to a
  flag dropped or returned to its stand.

The client also ORs bits in itself: shells from the quad, invulnerability, double-damage and
half-damage effects bits (Q3); translucency for some effects bits (Q2); and the viewer's own
model on the player's own entity (Q5). **[read]**

## 2. Translucency

- **The bit**: 32. The opacity comes from a per-drawn-model alpha that the client fills; the
  wire has no alpha field. **[read]**
- **Alpha by case** (client side) **[read]**:
  - the render bits are exactly translucent and nothing else: alpha 0.70 (only the black hole
   uses this in the base game),
  - beam (with or without translucent): 0.30,
  - the BFG effect bit (128): the client adds translucent with alpha 0.30 (the BFG ball in flight
   and its burst afterwards, both sprites),
  - the mission packs' plasma effect bit: translucent, 0.6; their "sphere translucent" bit:
   translucent, 0.3, or 0.6 when the tracker-trail bit is also on,
  - a colour shell's copy and the power-screen model: translucent, 0.30 (Q3, Q8),
  - a linked second model whose model number has its high bit (128) set: translucent, 0.32
   (mission-pack use: a shell around a sphere).
- **Translucent plus any other render bit, with none of the above**: the client sets no alpha of
  its own, so the model keeps whatever alpha the previous entity in the frame's list left behind,
  most often 0 (fully see-through in the GL renderer), sometimes 0.30. **[derived]** Nothing in
  the base game sends that combination except the beam, which sets its own. **[derived]**
- **How the renderers use alpha**:
  - GL, models: alpha is used only when the translucent bit is on; otherwise 1.0. Blending is
   source alpha over one minus source alpha. **[read]**
  - GL, brush models (doors, platforms): the translucent bit draws them at a fixed 0.25,
   whatever the alpha. **[read]**
  - GL draws every opaque model first, then every translucent one with depth writes off, in list
   order, unsorted. **[read]**
  - Software: alpha above 0.66 draws opaque, above 0.33 draws with a two-thirds blend table,
   else with a one-third table; shells use a two-thirds constant colour above 0.33, else
   one-third (so a 0.30 shell is one-third). **[read]**
  - A translucent model casts no shadow when the GL shadow option is on. **[read]**
- **Sprites (.sp2)**: drawn solid by default: alpha 1.0 with the image's transparent texels
  cut out (an alpha test, no blending). Only with the translucent bit do they blend, at the
  entity's alpha. **[read]** They are never lit: always white times the image. **[read]**
- **Client-made things** (temporary entities, never on the wire as render bits) **[read]**:
  - blaster impact puff: full bright and translucent, alpha fading from 1 to 0 over 300 ms,
  - smoke puff: translucent, alpha fading over 300 ms; muzzle-type flash: full bright, opaque,
   100 ms,
  - rocket, grenade and plain explosions: full bright; opaque for the first second (frames 0–9
   at 100 ms each), then translucent for frames 10 to 14 with alpha (16 − frame)/16, so 0.375
   down to 0.125; a light of radius 350, colour (1, 0.5, 0.5), is multiplied by (16 − frame)/16
   in every frame, opaque ones included, so it fades from 350 at once,
  - BFG burst sprite: full bright and translucent at 0.30, green light radius 350,
  - lasers drawn by the client: beam and translucent at 0.30.

## 3. The coloured shells

- **Bits**: 1024 red, 2048 green, 4096 blue, 65536 double damage, 131072 half damage. A shell is
  drawn only when the effects field also carries the colour-shell bit (256). **[read]**
- **How a shell is drawn**: the client adds the entity twice. The first copy is the plain model
  with ALL its render bits dropped (so it loses glow, full bright, etc. while it has a shell).
  The second is the same model, frame and position with its render bits plus translucent, at
  alpha 0.30. **[read]** The renderer draws that copy untextured, each vertex pushed out along
  its normal by **4.0 units**. **[read]**
- **Colour (GL)**, chosen in this priority **[read]**:
  - red, green and blue all set: white (1, 1, 1),
  - else, if red, blue or double is set (green ignored here): red alone (1, 0, 0); red with blue
   or with double, magenta (1, 0, 1); blue without red (0, 0, 1); blue with double, cyan
   (0, 1, 1); double alone, gold (0.9, 0.7, 0),
  - else (half damage and/or green): half damage (0.56, 0.59, 0.45); green sets the green channel
   to 1, so green alone is (0, 1, 0) and half damage with green is (0.56, 1, 0.45).
  - So quad with invulnerability is magenta; invulnerability with a power shield is red; quad
   with a power shield is blue. **[derived]**
- **Colour (software)**: a single palette index: white 215 (red, green and blue together); red
  242; red with blue or double 104; blue 243; blue with double (cyan) 114; double 223; half
  damage 144; green 208. **[read]** The priority is the same as GL's except at the end: GL mixes
  half damage with green, software shows half damage alone. **[derived]**
- **Shading (GL)**: with the default settings (vertex arrays off), a red, green or blue shell is
  one flat colour with no shading; a shell that is only double or half damage is untextured but
  shaded by the vertex normals like a lit model. With vertex arrays on, every shell is flat.
  **[read]** The shell's colour replaces the world's light entirely, so it looks the same in the
  dark. **[read]** If the glow bit (512) is also on, the glow pulse is applied to the shell's
  colour after it is chosen. **[read]**
- **See-through**: yes, alpha 0.30 (GL); one-third blend (software). **[read]**
- **Pulse**: the shell itself does not pulse. Quad and invulnerability blink near the end: the
  effect is on while more than 30 server frames (3 s) remain; within the last 30 frames it is on
  only when the remaining count has its bit of value 4 set, i.e. 4 frames on, 4 off: a 0.8 s
  blink. **[read]** for the rule, **[derived]** for the timing.
- **Which state sets each** **[read]**:
  - quad damage: the effects bit 32768, which the client turns into the blue shell,
  - invulnerability: the effects bit 65536, which the client turns into the red shell,
  - power shield absorbing a hit (players and monsters): green shell; the shield's "hit" state
   lasts 0.2 s after each absorbed hit,
  - power screen absorbing a hit: no shell; the power-screen effects bit instead (Q8),
  - a monster being resurrected by a medic: red shell,
  - god mode (a cheat): red, green and blue, so white,
  - double damage and half damage: effects bits the client turns into those shells (mission
   packs only; the base game never sets them).
- **The player's own shell**: the player never sees his own model (Q5), so never his own shell.
  **[read]**

## 4. Full bright and min light

- **Full bright (8)**: on a model, the light is set to (1, 1, 1) instead of being sampled from
  the world. **[read]** Also honoured for the placeholder shape drawn when a model is missing.
  **[read]** No base-game server entity sets it; the client uses it on explosions, flashes, the
  lightning beam and the menu's player preview. **[read]**
- **Min light (1)**: a floor of **0.1** on the model's light. **[read]** The two renderers differ:
  - GL: if no channel is above 0.1, all three become 0.1; if any one channel is above 0.1, the
   light is left alone. **[read]**
  - software: each channel is raised to 0.1 separately. **[read]**
  - It is applied after full bright and before glow. **[read]** Only the view weapon carries it
   in the release. **[read]**
- Sprites take no light at all; brush models take the world's lightmaps and ignore both bits.
  **[read]**
- With the weapon model bit, the renderer also reports the light at the player: 150 times the
  brightest channel (GL) or the first channel (software), before the min-light floor, into a
  variable the client sends to the server. **[read]**

## 5. The view weapon's bits

The client draws the view weapon with exactly min light (1), depth hack (16) and weapon model
(4). **[read]** It is not drawn at all when the player's gun option is 0 or the field of view is
over 90 degrees. **[read]** Its position and angles are the view's plus an offset and angles
interpolated between the last two server states; it is never moved by an old-to-new origin
blend. Its frames interpolate, except that frame 0 is drawn without blending from the old frame
(the weapon was just changed). **[read]**

- **Weapon model (4)** **[read]**:
  - never culled against the view,
  - never casts the GL shadow,
  - the "hand" setting decides: 2 hides it; 1 (left-handed) mirrors it left to right, drawn with
   its own projection (near 4, far 4096 units, the same vertical field of view) and with the
   face culling swapped to match; 0 draws it as is,
  - its light is reported to the server (Q4).
- **Depth hack (16)**: GL keeps the model in the nearest 30 % of the depth range; software
  divides its depths by 3. Either way it draws in front of nearby walls. **[read]**
- **Viewer's own model (2)**: the client sets it on the player's own entity, then never sends
  that entity to the renderer; no renderer reads the bit. So a player never sees his own body or
  anything attached to it. **[read]** Before skipping it, the client still adds a light at the
  player's own position for the flag, tag-trail and tracker-trail effects bits (Q8). **[read]**
- "Not for the owner" as a separate bit does not exist. **[read]**

## 6. Frame lerp, scale, light, IR, custom skin, disguise

- **Frame lerp (64)** (also beam, 128): the client does not interpolate the entity's origin.
  The model is placed at its newest origin with the old origin taken from the state's
  "old origin" field, and the renderer blends the vertices from the old origin to the new by the
  same fraction it uses for frames. Entities without it have both origins set to the client's
  interpolated position. **[read]** Frame-to-frame vertex interpolation happens for every model,
  with or without the bit. **[read]** Setting the renderer's model-lerp option to 0 turns frame
  interpolation off for all models. **[read]**
- **Scale**: no render bit changes a model's scale. **[read]** Only shells inflate (4 units).
- **Lighting**: no bit turns off receiving light except full bright (and the shells and IR,
  which replace it). The GL shadow, when enabled, is skipped for translucent and weapon models
  only. **[read]**
- **IR visible (32768)**: when the view carries the infra-red goggles flag (render-definition
  flag 4), a model with this bit gets the light (1, 0, 0), overriding shells, full bright and
  glow, so it shows solid red. **[read]** The base game never sets either. **[read]**
- **Custom skin (256)**: defined but never set nor read in this release. **[read]** Player
  skins work another way: model number 255 means "use the player's own model and skin", picked
  by the low byte of the skin number. **[read]**
- **Use disguise (262144)**: on an entity drawn with a player's own model, if that player's skin
  is from the male, female or cyborg model, the client swaps in that model and its "disguise"
  skin. **[read]** The base game never sets it.

## 7. The glow pulse (512)

- **Formula**: the light gains 0.1 × sin(7 × t), with t the client's time in seconds (the
  client clock in milliseconds × 0.001), per channel, and each channel is held at no less than
  80 % of its value before the pulse. **[read]** Period 2π/7 ≈ 0.898 s. **[derived]** The gain is
  a fixed amount added, not a scale, so a dark item brightens relatively more; with full bright
  the light can reach 1.1. **[derived]**
- **Order**: applied after full bright or the world's light and after the min-light floor; the
  IR override comes after it. **[read]**
- **Same in both renderers**. **[read]** In software the three channels are then averaged into
  one level. **[read]**
- **Sprites**: no; sprites are never lit, so the bit does nothing to them. Brush models: no.
  **[read]**
- **Dynamic light**: the bit adds no dynamic light and scales none. **[read]**
- **Lost under a shell**: an entity with the colour-shell effects bit has its plain copy drawn
  with no render bits, so it does not glow (Q3). **[read]**

## 8. The effects field (what a viewer sees)

Each effects bit and what the client does **[read]** unless marked:

| Bit | Plain name | What is drawn |
| --- | --- | --- |
| 1 | rotate | yaw = 100 degrees per second of client time (the same angle for every item), pitch and roll 0; the entity's own angles are ignored |
| 2 | gib | blood trail (below) |
| 4 | (none) | not defined |
| 8 | blaster | blaster trail and a light radius 200, yellow (1, 1, 0) |
| 16 | rocket | smoke trail plus fire, light radius 200, yellow (1, 1, 0) |
| 32 | grenade | smoke trail only, no light |
| 64 | hyperblaster | no trail; light radius 200, yellow (1, 1, 0) |
| 128 | BFG | translucent 0.30; the light (below) |
| 256 | colour shell | draws the shell copy (Q3) |
| 512 | power screen | draws a second model, the power-screen panel (frame 0 of the armour "effect" model), translucent 0.30 with the green-shell look, so a flat green panel inflated 4 units |
| 1024 | animate 0/1 | frame alternates 0, 1 |
| 2048 | animate 2/3 | frame alternates 2, 3 |
| 4096 | animate all | frame = whole half-seconds of client time |
| 8192 | animate all fast | frame = whole tenths of a second of client time |
| 16384 | flies | a swarm of black particles (below) |
| 32768 | quad | becomes the blue shell (Q3) |
| 65536 | invulnerability | becomes the red shell |
| 131072 | teleporter | fountain particles (below) |
| 262144 | flag 1 (red team) | trail, palette 242; light 225, (1, 0.1, 0.1) |
| 524288 | flag 2 (blue team) | trail, palette 115; light 225, (0.1, 0.1, 1) |
| 1048576 | ion ripper | its trail; light 100, (1, 0.5, 0.5) |
| 2097152 | green gib | green blood trail, palette 219–226 |
| 4194304 | blue hyperblaster | light 200, blue (0, 0, 1) |
| 8388608 | spinning lights | yaw spins at 500 degrees per second added to the entity's own yaw, roll 180, pitch 0; a red light radius 100 placed 64 units ahead along the spun facing |
| 16777216 | plasma | translucent 0.6; light 130, (1, 0.5, 0.5); with "animate all fast" also a blaster trail |
| 33554432 | trap | particles 32 units above the origin; light of a random radius 100–199, (1, 0.8, 0.1) |
| 67108864 | tracker | the tracker's dark trail and a negative ("dark") light of 200; with blaster or hyperblaster it instead makes those green: green trail and green light (0, 1, 0) radius 200 |
| 134217728 | double damage | becomes the double-damage shell |
| 268435456 | sphere translucent | translucent 0.3 (0.6 with tracker trail) |
| 536870912 | tag trail | trail, palette 220; light 225, yellow (1, 1, 0) |
| 1073741824 | half damage | becomes the half-damage shell |
| 2147483648 | tracker trail | a dark light; with tracker it pulses: radius 50 + 500 × (sin(2t) + 1), t in seconds, dark |

Bits 1048576 and up were added for the mission packs; the base game sets none of them. Flag 1
and flag 2 are set only by the capture-the-flag module, on the carrier. **[read]**

**Order and exclusivity** **[read]**: only one trail-and-light effect is drawn per entity, the
first present in this order: rocket, blaster, hyperblaster, gib, grenade, flies, BFG, trap,
flag 1, flag 2, tag trail, tracker trail, tracker, green gib, ion ripper, blue hyperblaster,
plasma. Nothing is done when rotate is the only bit. Quad, invulnerability, double and half
damage are turned into shells before anything else. The teleporter's particles are added for
every entity in each received server frame, apart from this chain. **[read]** An entity with
model number 0 is sent when it has effects, but the client skips it before the trails, so it
shows no trail, light or shell; only the teleporter's fountain, added separately, still shows.
**[derived]**

**Animation bits** **[read]**: the client counts in whole half-seconds of its clock for the
first three, so animate 0/1 shows each frame for 0.5 s (a 1 s cycle), animate 2/3 likewise
with frames 2 and 3, and animate all steps to the next frame every 0.5 s; animate all fast
steps every 0.1 s. The base game puts them on brush entities (walls, objects, explosive
pieces, rotating brushes, doors) by spawn flags (spawn flag 64 for "all", 128 for "all fast"
on several of them); a button switches from 0/1 to 2/3 when pressed and back when it resets,
the BFG ball in flight has "all fast", cleared when it bursts. The count is not wrapped by the
client: brush textures and sprites wrap by their own frame count **[read]**; a jointed model
given a frame past its last is reported on the console and drawn at frame 0 (GL). **[read]**

**Trails** **[read]** (all particles start at alpha 1 and fade linearly at the rate given; the
gravity constant is 40 units/s²):

- **Smoke (rocket and grenade)**: particles every 0.5 units along the move, each kept with
  probability trailcount/1024; the count starts at 1024 when the entity appears and falls by 5
  per step to a floor of 100, so the trail thins as it flies. Spread: ±4 units and ±15 units/s
  while the count is above 900, ±2 and ±10 above 800, else ±1 and ±5. Colour palette 4–11
  (random), rising at 20 units/s², life 1 to 1.2 s.
- **Rocket fire**: in addition, about one particle per 8 units, palette 220–223, ±5 units,
  ±20 units/s, falling under gravity, life 1 to 1.2 s.
- **Gib blood**: the same thinning trail, palette 232–239, starting with an extra 40 units/s
  downward and no acceleration, life 1 to 1.4 s. Green gibs use palette 219–226.
- **Blaster**: a particle every 5 units, palette 224, ±1 unit, ±5 units/s, life 0.3 to 0.5 s.
- **Flags and tag trail**: a particle every 5 units in the colour named above, ±16 units,
  ±5 units/s, life 0.8 to 1.0 s.
- **BFG in flight** (with "animate all fast"): 162 particles in a shell around the ball, each
  drawn for one frame, placed by a time-driven spin up to 64 units out plus 16; colour palette
  208 plus up to 7 by distance (distance/90 × 7), alpha 1 − distance/90; light radius 200 green
  (0, 1, 0). After the burst (no "animate all fast") the light radius follows its frame: 300,
  400, 600, 300, 150, 75 for frames 0–5, green.
- **Flies**: black particles (palette 0) in a cloud up to 64 units around the body, built the
  same way as the BFG's; the number grows from 0 to 162 over the first 20 s of a 60 s cycle,
  stays, then falls to 0 over the last 20 s, and the cycle restarts; only every second one is
  drawn, so at most 81 **[derived]**. The base game sets flies on some dead monsters' corpses
  (the infantry and the mutant): half the time, 5 to 15 s after death, unless in water; they
  stay 60 s. A gibbed body loses them. **[read]**
- **Teleporter**: each received server frame, 8 particles of palette 219 within ±16 units across
  and 1 to 8 units below the entity's origin, rising at 80–87 units/s under gravity, fading at 0.5 per second.
  The base game sets it on the deathmatch teleporter pad. **[read]**

**Dynamic lights of the effects** are listed in the table; the colour numbers are the light's
RGB weights, the radius is in units. A negative colour (the tracker's) darkens in the GL
renderer; in the software renderer the client instead sends a negative radius with white.
**[read]**

## Arrival and respawn particles

Client-side bursts when something appears or leaves (read from the sequel's GPL source; facts
only). "Random 0..n" is a whole number, "random ±x" a real number, both uniform.

**How every particle moves** **[read]**: nothing is stepped. Each frame a particle's age t (s)
gives its position as start + velocity × t + acceleration × t², with no factor of one half, so
the drawn fall is twice the stated constant: gravity "40" falls like 80 units/s². Alpha is
1 − t / life, clamped to at most 1; at 0 or below the particle is freed. The client's pool holds
4096 particles; an effect that finds it empty stops spawning. GL draws each one, by default, as
a round blended point of 40 px divided by the square root of 0.01 + 0.01 × distance², clamped to
2..40 px (`gl_particle_size`, `gl_particle_att_a/b/c`, `gl_particle_min_size`,
`gl_particle_max_size`; without point parameters a 1.5-unit textured triangle grown by
1 + 0.004 × depth beyond 20 units). **[read; the formula derived from the attenuation factors]**

**Event 6, player teleport** **[read]**: 1053 particles at once on a lattice round the entity's
origin, 9 × 9 across (offsets −16..+16 by 4) and 13 up (−16..+32 by 4), each nudged by random
0..3 units per axis. Palette 7–14 (grey). Speed 50–113 units/s along the unit vector of the
lattice offset with x and y SWAPPED (the burst is mirrored across the x = y diagonal; the centre
particle does not move); gravity 40; life 0.30–0.44 s (0.30 + 0.02 × random 0..7). No light.
Sound misc/tele1.wav. The base game sets it on a player who respawns in deathmatch or co-op (at
the spawn point, held 112 ms), and on a teleporter pad's use on BOTH the arriving player and the
source pad, so two bursts play in one frame; the player arrives 10 units above the destination's
origin, velocity zeroed, held 160 ms. Temp entity 48 ("teleport effect", at a sent position)
draws the same burst; the base game never sends it.

**Event 1, item respawn** **[read]**: 64 particles, random ±8 units per axis round the item's
origin, palette 212–215 (green), velocity random ±8 units/s per axis, gravity 8 (falls like 16),
life 1.0–1.3 s. No light. Sound items/respawn1.wav. A teamed item spot picks one member at
random and the event goes on that one. Events last one server frame.

**Login, logout, respawn flashes** **[read]**: muzzle-flash numbers 9 (login, green), 10 (logout,
red) and 11 (respawn, yellow; handled by the client, never sent by the base game), sent to the
PVS of the player's origin. Login is sent on first entering a deathmatch game ("<name> entered
the game"), on a spectator joining play, and when a client begins a level in any game with more
than one slot; logout on disconnect. Each: 500 particles uniform in a box ±16 across and 24 below
to 32 above the origin, palette 208–215 / 64–71 / 224–231, velocity random ±20 units/s per axis,
gravity 40, life 1.0–1.3 s; a light of radius 200 + random 0..31 (100 + random 0..31 with the
silenced bit 128), minimum light 32, colour (0, 1, 0) / (1, 0, 0) / (1, 1, 0), 18 units ahead and
16 to the right of the origin, lasting 1 ms on the client's millisecond clock, so one rendered
frame **[derived]**; sound weapons/grenlf1a.wav at full volume. No temp entity marks a spawn.

## Entity loop sounds

Facts read from the sequel's source release (the client's mixer, the server's frame builder
and the base game's rules) by a clean-room reader. **[read]** unless marked.

**How the client plays an entity's `sound` field.** Rebuilt on every client sound update
(once per rendered frame) from the latest server frame: last update's loop channels are
wiped, then one channel is allocated per distinct sound index. Master volume is always 255
(full); the overall `s_volume` (default 0.7) scales it in the mixer. Distance from the
listener (the predicted view origin) minus 80 units, floored at 0, times 0.003 gives the
fade: full within 80 units, linear, silent beyond 413.3 units **[derived]**. That factor
equals a one-shot at static attenuation (3 × 0.001; normal and idle one-shots use
attenuation × 0.0005). Pan: right share half of (1 + d), left half of (1 − d), d = the
listener's right vector dotted with the unit direction to the source; mono output does not
pan. A lone close loop dead ahead is 127 per ear out of 255 **[derived]**. The play
position is the global output-sample clock modulo the sample's length, so loops stay
continuous through the per-frame rebuild and identical loops share one phase. At the end of
a sample the loop wraps to sample 0 (a WAV loop-start point is ignored for entity loops).
A change of the field or the entity leaving the frame takes effect next update, with no
fade. Loops stop while paused, while not fully in-game, and before sounds are registered.

**Position.** The entity's origin as the server frame gives it, not the lerped origin
drawn, so a moving loop steps at 10 Hz. Brush models get no bounding-box centre: a mover
built without an origin brush has origin 0,0,0 at its start, so its loop is heard near the
world origin (a known flaw; later clients offer the box centre or the box's nearest point,
see the settings below) **[read; consequence derived]**.

**What the server sends.** An entity with no model, effects, sound or event is never sent.
An entity with a sound is culled by the PVS (not the PHS) and area connectivity like any
other. A sound-only entity (no model, such as a speaker) is also dropped beyond 400 units
of the viewer. The client's own entity is always sent, so its own loop plays (at the body
origin, within 80 units, so at full gain and nearly centred **[derived]**).

**Merging and the cap.** Per sound index, every carrying entity's left and right volume is
computed at its own position, the volumes are summed and each ear clamped to 255: one
channel per distinct sound. 32 mixing channels in all, shared with one-shots. A loop takes
the channel with the least time left (wiped loop channels first), never one playing the
local player's own sound; if none is free, the remaining loops are dropped that update.

**Base game loops** (team slaves never set a mover's loop):
- Blaster bolts (player and monsters) `misc/lasfly.wav`, rockets `weapons/rockfly.wav`,
  thrown hand grenades `weapons/hgrenc1b.wav` (launcher grenades none), BFG ball
  `weapons/bfg__l1a.wav` (off on impact). Each from spawn until removed.
- Player, set every server frame, first match wins: in lava or slime `player/fry.wav`,
  railgun held `weapons/rg_hum.wav`; BFG held `weapons/bfg_hum.wav`; hyperblaster firing
  `weapons/hyprbl1a.wav`; chaingun firing `weapons/chngnl1a.wav`; a cooked hand grenade
  `weapons/hgrenc1b.wav` (from the pin until thrown). Cleared on death and at intermission
  (a dead player still holding railgun or BFG gets its hum back next frame **[derived]**).
- `func_plat` `plats/pt1_mid.wav`, `func_door` and `func_door_rotating`
  `doors/dr1_mid.wav` (`sounds` 1 silences doors) while moving. `func_train` loops the
  file in its `noise` key (none by default), off when it waits at a corner (a train stopped
  by a toggle keeps sounding **[derived]**). `func_water`, `func_door_secret`,
  `func_rotating` and `func_button` have no loop.
- `misc_teleporter` `world/amb10.wav` always. `target_speaker` with spawnflag 1
  (looped-on) or 2 (looped-off): the `noise` file toggled by each use, always at full
  volume and static attenuation (its `volume` and `attenuation` keys apply to one-shots
  only).
- Monsters: `monster_flyer` `flyer/flyidle1.wav`, `monster_floater`
  `floater/fltsrch1.wav`, `monster_hover` `hover/hovidle1.wav` (kept by a corpse that is
  not gibbed), `monster_boss2` `bosshovr/bhvengn1.wav` (until its explosion sequence ends),
  `monster_jorg` `boss3/w_loop.wav` during its gun attack, the Makron's torso
  `makron/spine.wav` forever, infantry and mutant corpses' flies `infantry/inflies1.wav`
  (50% chance, after 5–15 s, for 60 s). Thrown heads clear the loop.

**The extended protocol's loop volume and attenuation bytes** are not in the stock
protocol; absent, a loop is volume 255 and factor 0.003 per unit beyond 80. Their units
are not settled by the source read.

**Settings.** The stock client has none that turns loops off (`s_volume` scales them,
`s_show` lists playing channels). A later client adds `s_ambient` (0 off, 1 all, 2 only
entities with a model, 3 only the player's own), `s_bsp_soundpos` (0 the original origin,
1 the brush box centre at the lerped origin, the default, 2 the box's point nearest the
listener), `cl_audiopaused` (loops pause with the game only when 1), and with its OpenAL
backend plays each entity's loop on its own channel, unmerged.
