# Monster deaths, gibs and blood

**A WORKING NOTE** (README.md's rules). Where the facts come from: a reader session (2026-11-09,
the `clean-room` skill) that read the classic engine's GPL release and the single-player game
program's GPL release (1.06), answering the questions of the session that built our deaths, gibs
and blood (the `se-deaths` lab, M18s7). It wrote only facts in its own words, each marked read,
measured or derived. The building session read this note and the lab's measurements, not the
source.

What we built from it: our campaign variant's deaths (`games/shooters_edge/qc/campaign/ai.qc`:
the two death sequences, the frames a corpse stays solid, the backpacks, the gib speeds by
health, the per-kind pieces, the head; `common/weapons.qc`: a blast's pellets on one body landing
as one blow), the client's blood bursts (count and way, `client/world_fx.cpp`) and the blood
trails behind gibs and heads by their model files' flags (`app/effects.hpp`'s `blood_trail`).

## Facts that hold for every answer below

- A frame of a monster's (or the player's) death animation lasts 0.1 s: each frame schedules the
  next 0.1 s later. The final frame of every death sequence schedules itself again,
  so a corpse keeps re-entering its last frame every 0.1 s forever; nothing ever removes it
  (exceptions: the spawn, Chthon and Shub, below) **[read]**.
- When anything is killed, its health is first raised to -99 if it fell below -99. The kind's
  death handler then sees that clamped health **[read]**. Consequence: a gib throw from an ordinary
  kill never sees a "damage" figure below -99 **[derived]**.
- On death the victim stops taking damage at once (damage off) and loses any touch action, then
  fires its map targets, then its own death handler runs. Its fly and swim flags are cleared first,
  so a dead flier or swimmer falls under gravity like a walker **[read]**.
- Consequence: a corpse can never be gibbed after death, not even while it is still solid during
  its first frames; shots that hit a solid corpse make a wall puff, not blood **[derived]**.
- No monster's box changes when it dies; the body keeps its spawn box until it turns non-solid
  (and only a gib turns the body into a head with a new box, question 2) **[read]**.
- A falling corpse (monsters keep the stepping movetype when they die) that lands faster than
  80 units/s downward (0.1 × sv_gravity) plays "demon/dland2.wav" on landing **[read]**.
- The "damage" figure passed to every ordinary gib and head throw is the victim's health at
  death (a negative number), not the damage of the blow **[read]**.
- Gib sound for every monster that gibs, except the zombie: "player/udeath.wav", voice channel,
  normal attenuation **[read]**.

## 1. Each monster's death

### Gib thresholds and what is thrown

A death gibs when health is strictly below the threshold **[read]**. "Damage figure" is the
victim's health (see above). Every listed head replaces the body itself (question 2).

| Monster (spawn health) | Gibs below | Thrown | Head model |
| --- | --- | --- | --- |
| Grunt (30) | -35 | gib1, gib2, gib3, one each | progs/h_guard.mdl |
| Enforcer (80) | -35 | gib1, gib2, gib3 | progs/h_mega.mdl |
| Rottweiler (25) | -35 | gib3 three times | progs/h_dog.mdl |
| Knight (75) | -40 | gib1, gib2, gib3 | progs/h_knight.mdl |
| Hell knight (250) | -40 | gib1, gib2, gib3 | progs/h_hellkn.mdl |
| Ogre (200) | -80 | gib3 three times | progs/h_ogre.mdl |
| Fiend (300) | -80 | gib1 three times | progs/h_demon.mdl |
| Shambler (600) | -60 | gib1, gib2, gib3 | progs/h_shams.mdl |
| Zombie (60) | always (any death gibs) | gib1, gib2, gib3 | progs/h_zombie.mdl |
| Scrag (80) | -40 | gib2 three times | progs/h_wizard.mdl |
| Vore (400) | -90 | gib1, gib2, gib3 | progs/h_shal.mdl |
| Rotfish (25) | never gibs | nothing | none |
| Spawn (80) | never gibs (explodes) | nothing | none |
| Chthon | never gibs | nothing | none |
| Shub-Niggurath | special (50 gibs, below) | gib1/gib2/gib3 | none |

All gib models are "progs/gib1.mdl", "progs/gib2.mdl", "progs/gib3.mdl" **[read]**.

Order and start point **[read]**, **[derived]**: every kind except the rottweiler turns the body
into the head first and throws the gibs after. The head throw lowers the body's origin by 24, so
for those kinds every gib starts 24 units below the body's origin. The rottweiler throws its three
gibs first (from the body's origin) and becomes the head last.

Gib velocity class **[derived]** (question 2 gives the formula): a gibbed grunt, enforcer or
rottweiler at health -36 to -49, or a knight, hell knight or scrag at -41 to -49, throws slow gibs
(the 0.7 scale); at -50 or lower, fast gibs (the 2 scale). Ogre, fiend, shambler and vore gibs are
always fast, since their thresholds are -50 or lower. The 10 scale is never reached by an ordinary
kill (health is clamped to -99).

A gibbed monster drops no backpack: the backpack is dropped only from a death animation frame
**[derived]**.

### Death animations, sounds, non-solid frame, backpack

Sounds play on the voice channel at normal attenuation unless stated **[read]**. "Chance" is the
probability of each sequence. "Non-solid" is the frame (1-based within the sequence) on which the
body stops being solid; it stays non-solid **[read]**.

| Monster | Death sound | Sequences (frame names, indices) and chance | Non-solid | Backpack |
| --- | --- | --- | --- | --- |
| Grunt | "soldier/death1.wav", at the moment of death | A: death1 to death10 (8 to 17), 50%. B: deathc1 to deathc11 (18 to 28), 50% | frame 3 of either | frame 3 of either: 5 shells |
| Enforcer | "enforcer/death1.wav", at death | A: death1 to death14 (41 to 54), 50%. B: fdeath1 to fdeath11 (55 to 65), 50% | frame 3 | frame 3: 5 cells |
| Rottweiler | "dog/ddeath.wav", at death | A: death1 to death9 (8 to 16), 50%. B: deathb1 to deathb9 (17 to 25), 50% | at the moment of death, before frame 1 | none |
| Knight | "knight/kdeath.wav", at death | A: death1 to death10 (76 to 85), 50%. B: deathb1 to deathb11 (86 to 96), 50% | frame 3 | none |
| Hell knight | "hknight/death1.wav", at death | A: death1 to death12 (42 to 53), 50%. B: deathb1 to deathb9 (54 to 62), 50% | frame 3 | none |
| Ogre | "ogre/ogdth.wav", at death | A: death1 to death14 (112 to 125), 50%. B: bdeath1 to bdeath10 (126 to 135), 50% | frame 3 | frame 3: 2 rockets |
| Fiend | "demon/ddeath.wav", on frame 1 | one: death1 to death9 (45 to 53) | frame 6 | none |
| Shambler | "shambler/sdeath.wav", at death | one: death1 to death11 (83 to 93) | frame 3 | none |
| Zombie | none (it always gibs: "zombie/z_gib.wav") | none | not applicable | none |
| Scrag | "wizard/wdeath.wav", on frame 1 | one: death1 to death8 (46 to 53) | frame 3 | none |
| Rotfish | "fish/death.wav", on frame 1 | one: death1 to death21 (18 to 38) | frame 21 (the last) | none |
| Vore | "shalrath/death.wav", at death | one: death1 to death7 (16 to 22) | at the moment of death | none |

Movement during the animation (the corpse walks a fixed distance, as a dead stepping body)
**[read]**:

- Grunt sequence B: backwards 5, 4, 13, 3, 4 on frames 2 to 6 (29 in all).
- Enforcer sequence A: forwards 14, 2 on frames 4 and 5, then 3, 5, 5, 5 on frames 9 to 12 (34).
- Hell knight sequence A: forwards 10, 8, 7 on frames 1 to 3, then 10, 11 on frames 8 and 9 (46).
- Ogre sequence B: forwards 5 on frame 2, then 1, 3, 7, 25 on frames 4 to 7 (41).
- Scrag: on frame 1 the body is launched with horizontal velocity uniform in -200 to 200 on each
  axis and upward velocity 100 to 200, and leaves the ground; with its fly flag cleared it falls.
- All others: no movement.

Durations at 0.1 s a frame **[derived]**: the last frame is reached (n - 1) × 0.1 s after death
for an n-frame sequence, e.g. 0.9 s for the grunt's 10 frames, 2.0 s for the rotfish's 21.

Backpack details **[read]**: dropped only if the body holds some ammunition (the monsters above set
theirs on the drop frame); it appears 24 units below the body's origin, with velocity 300 up and
uniform -100 to 100 on each horizontal axis; it tosses (MOVETYPE_TOSS), is a trigger with box
(-16 -16 0) to (16 16 56), model "progs/backpack.mdl", and is removed after 120 s. It carries the
body's ammunition (5 shells, 5 cells or 2 rockets) and no weapon.

Final state **[read]**: the body stays forever on the last frame of its sequence, non-solid,
undamageable, keeping its spawn box and the stepping movetype.

### The spawn (tarbaby)

No death animation and no gibs **[read]**. At the moment of death it shows its single frame "exp"
(index 60) and stops taking damage. 0.1 s later, still on "exp": it does radius damage 120 (to
everything solid and damageable within 160 units; a target at distance d from its centre takes
120 - d/2, halved for the shambler; the spawn counts as its own attacker), plays
"blob/death1.wav", steps back 8 units against its direction of motion, sends the tar-explosion
temporary entity at its origin, then turns into the explosion sprite "progs/s_explod.spr": not
solid, not moving, frames 0 to 5 at 0.1 s each, then removed **[read]**. A spawn killed by
another's blast explodes in turn, 0.1 s later **[derived]**.

### Chthon (monster_boss)

It never takes damage: it is killed only by the map's lightning event **[read]**. Its health is 1
on skill 0 and 3 on other skills; each lightning hit while the first lightning door is at its top
plays "boss1/pain.wav" and removes 1. At 0 it plays its third shock animation, shockc1 to shockc10,
then death1 to death9 (indices 48 to 56), playing "boss1/death.wav" on death1. On death9 it plays
"boss1/out1.wav" on the body channel and sends a lava-splash temporary entity at its origin. 0.1 s
later (still showing death9) it adds itself to the killed-monster count, fires its targets and is
removed **[read]**. From the last hit to removal: 10 + 9 + 1 frames, about 2.0 s **[derived]**. No
gibs, no head.

### Shub-Niggurath (monster_oldone)

It has 40000 health, its pain resets health to 40000, and it dies only to a telefrag (50000
damage) **[read]**. Its death starts the ending, in this order **[read]**:

1. At once: intermission starts (with no exit), the finale message is sent with empty text, every
   player is hidden, made non-solid, frozen, undamageable and moved to the intermission spot.
2. 1 s later: a teleport-splash temporary entity at its origin minus 100 on the y axis, and "misc/r_tele1.wav".
3. 2 s after that: "boss2/death.wav", light style 0 starts pulsing, and the body starts thrashing:
   shake1 to shake15 three times, then shake16 to shake20, at 0.1 s a frame, with light style 0
   stepped through bright and dark letters each frame.
4. On reaching shake20 (50 frames, about 5 s after step 3) **[derived]**: "boss2/pop2.wav", then
   50 gibs thrown from a grid of points around its origin: x and y each at -64, -32, 0, 32, 64,
   and z at 16 and 112 above it **[read]**. Each gib is gib1 with chance 30%, gib2 30%, gib3 40%,
   with damage figure -999 (the fast 10 scale) **[read]**. Then the finale text is shown, a
   stand-in player model ("progs/player.mdl", frame 1, yaw 290) is placed at its origin offset by
   (-32, -264, 0), the CD track switches to 3, light style 0 returns to "m", and Shub is removed.

The declared frame list repeats the name "shake12", so the indices of the frames after it cannot
be settled from the source **[read]**.

## 2. The gib throw and the head

### Gib velocity **[read]**

Take three random terms: x and y each uniform in -100 to 100, z uniform in 200 to 300. Then scale
the whole vector by the damage figure dm (the victim's health):

- dm above -50: scale 0.7 (x, y in -70 to 70; z in 140 to 210).
- dm above -200 (and at most -50): scale 2 (x, y in -200 to 200; z in 400 to 600).
- dm at most -200: scale 10 (x, y in -1000 to 1000; z in 2000 to 3000). Only Shub's -999 reaches
  this. The server clamps every velocity component to sv_maxvelocity (default 2000) each frame
  before moving, so z is in effect 2000 **[derived]**.

The random source returns 0 to 1 inclusive in steps of 1/32767; the symmetric term is twice that
minus 1 **[read]**.

### The gib entity **[read]**

- A new entity at the body's (current) origin, with the gib model.
- Box: a point (zero mins and maxs).
- Movetype MOVETYPE_BOUNCE, solid SOLID_NOT; flags cleared.
- Angular velocity: each of the three axes uniform in 0 to 600 degrees/s (never negative).
  Starting angles 0.
- Frame 0.
- Lifetime: removed after 10 s plus a uniform 0 to 10 s (10 to 20 s), whether or not it has come to
  rest.
- Being non-solid, it clips only against the world and brush models, never against monsters or
  players, and touches nothing **[read]**.

### The head **[read]**

The body itself becomes the head (no new entity):

- Model swapped to the head model, frame 0; it never thinks again.
- Movetype MOVETYPE_BOUNCE; damage off; solid SOLID_NOT.
- View offset 8 up (this is what the player's camera uses, question 8).
- Box set to (-16 -16 0) to (16 16 56). Being non-solid, that box is used only to clip against the
  world and brush models.
- Velocity from the same formula as a gib, with the same damage figure.
- Origin lowered by 24.
- Leaves the ground.
- Angular velocity on yaw only: uniform -600 to 600 degrees/s; pitch and roll 0. Its angles are
  whatever the body had.
- It stays forever.

## 3. Blood at hits

The blood request is the `particle` builtin with colour 73 and count twice the damage figure given
to it **[read]**; the direction given to the builtin is one tenth of the velocity handed to the
blood call **[read]**. Combined, every blood burst below asks for direction = 0.02 × some
velocity **[derived]**.

### Shotgun and super shotgun pellets (and the grunt's rifle) **[read]**

- Shotgun: 6 pellets, spread 0.04 by 0.04; super shotgun: 14 pellets, spread 0.14 by 0.08; grunt:
  4 pellets, spread 0.1 by 0.1. Each pellet is 4 damage.
- Each pellet that hits something damageable makes its own burst at once: colour 73, count 8,
  start point 4 units back along the pellet's direction from the hit point.
- Direction: take the pellet's direction, add the view's up and right vectors each scaled by an
  independent uniform -1 to 1, normalise, add twice the hit surface's normal, multiply by 200; the
  builtin's direction is 0.02 times that, i.e. 4 × (jittered unit direction + 2 × normal). Each
  component lies within -12 to 12 before the wire clamp (question 4) **[derived]**.
- A pellet that hits something not damageable sends a gunshot temporary entity instead.
- Batching: only damage is batched. Consecutive pellets that hit the same entity add their damage
  into one damage call; a pellet hitting a different entity flushes the running sum first; the last
  sum is applied after all pellets. Blood is never batched: one burst of 8 per pellet **[read]**.

### Nails **[read]**

- Nailgun nail: 9 damage, a burst of count 18. Super nailgun nail: 18 damage, count 36. Each nail
  is its own damage call (no batching).
- Direction: 0.2 times a velocity built as: the nail's normalised flight direction plus up and
  right vectors each scaled by uniform -0.5 to 0.5, normalised, plus twice a surface normal, times
  200. The burst starts at the nail's position nudged by 0.01 of that 0.2-scaled velocity. The
  builtin's direction is therefore 0.02 × that velocity.
- The normal and the up and right vectors used here are whatever the game last computed for some
  other purpose: the touch event does not refresh them **[derived]** (the engine's touch call sets
  only self, other and time). So the "away from the surface" part is effectively arbitrary.

### The axe **[read]**

20 damage; a burst of count 40 with direction zero, 4 units back from the hit point along the
view. On a wall: "player/axhit2.wav" and a gunshot temporary entity.

### Enforcer laser **[read]**

15 damage; a burst of count 30 with direction 0.02 × the laser's velocity, 8 units back along its
flight. It bleeds anything with nonzero health (not the damage test the others use).

### Monster melee **[read]**

- Knight, hell knight and rottweiler melee, the rotfish bite, and the zombie's thrown flesh when it
  hits (10 damage): damage only, no blood burst and no meat.
- "Meat spray" is not particles: it is a thrown model, "progs/zom_gib.mdl" (which leaves the
  small blood trail, question 5). One spray: started 16 units in front of the attacker, velocity
  the given sideways vector plus 250 to 300 upward, angular velocity fixed at (3000, 1000, 2000)
  degrees/s, MOVETYPE_BOUNCE, SOLID_NOT, a point box, removed after 1 s.
- Who sprays, how many, sideways velocity:
  - Ogre chainsaw: one spray on the hits of swing frames 6 (200 to the right) and 10 (200 to the
  left), and on smash frame 10 (uniform -100 to 100 sideways); none on its other chainsaw frames.
  A spray comes only when the hit lands (within 100 units and visible).
  - Fiend claws: one spray per landed hit, 200 right on one, 200 left on the other.
  - Shambler claws: one spray per landed hit, 250 right or 250 left; the shambler's smash: two
  sprays, each uniform -100 to 100 sideways.
- Melee damage for scale: knight and hell knight 0 to 9 (three uniform 0 to 3 terms), rottweiler 0
  to 24, ogre 0 to 12, fiend 10 to 15, shambler claw 0 to 60, smash 0 to 120.

### Quad damage **[read]**

Quad multiplies the damage by 4 inside the damage call only. The blood count is computed from the
unmultiplied figure, so a quad pellet still makes 8 particles. Visible or audible effects of quad:
the owner glows with a dynamic light (radius 200 to 231); firing plays "items/damage3.wav" at most
once a second; for its last 3 s the owner hears "items/damage2.wav", reads "Quad Damage is wearing
off" and gets a screen flash once a second. Derived consequences **[derived]**: the pushback a hit
gives a player (8 units/s per point of damage, along the line from the attacker) is quadrupled,
victims are left with lower health, so gibbing and the faster gib scale are more likely.

## 4. The client drawing a `particle` burst

**[read]**, WinQuake client:

- The wire carries the direction per axis as a signed byte: the server multiplies by 16 and clamps
  to -128 to 127; the client divides by 16. So each direction component arrives in -8 to 7.94 in
  steps of 1/16. The count travels as one byte (the low 8 bits of the integer count); 255 means the
  explosion.
- Burst of N (not 255): exactly N particles, as long as free particles remain (the pool is 2048 by
  default; the "-particles" command line option sets it, never below 512).
  - Start: the origin plus an integer offset per axis, uniform -8 to 7.
  - Velocity: 15 × the received direction on every axis, no random part (so at most about 120
  units/s per axis).
  - Life: 0, 0.1, 0.2, 0.3 or 0.4 s, equally likely. A particle is removed once the client time
  passes its end, so even a 0-life particle is drawn at least once **[derived]**.
  - Colour: the colour with its low 3 bits cleared, plus uniform 0 to 7, fixed for life. Colour 73
  gives palette indices 72 to 79; 225 gives 224 to 231.
  - Gravity: downward acceleration 0.05 × sv_gravity, i.e. 40 units/s² at the default 800 (the
  client reads its own sv_gravity setting). Each rendered frame the particle moves by its
  current velocity first, then gravity is applied.
- Burst of 255 (count 1024, the same as the rocket explosion): 1024 particles, start offset per
  axis integer -16 to 15, velocity per axis integer -256 to 255, all starting at palette 111, a
  5 s cap on life. Odd-numbered ones: a ramp starting at 0 to 3, rising 10 per second, dying at 8
  (so 0.5 to 0.8 s), colour stepping down from 111 by 2 per ramp step to 97, velocity growing by
  4 × frame time of itself each frame, and 40 units/s² gravity. Even-numbered ones: ramp rising 15
  per second (0.33 to 0.53 s), colour from 111 down by 1 per step to 106 then 104 and 102,
  velocity shrinking by frame time × itself each frame, and the same gravity.
- QW client differences: the start offset is multiplied by 2 for counts above 20 and by 3 above
  130, and count 255 is not special in the burst itself; life, colour, velocity and gravity are the
  same.

## 5. Model-flag trails

The flag bits are part of the .mdl format **[read]**: 1 rocket trail, 2 grenade smoke, 4 gib blood
trail, 8 rotate, 16 green tracer, 32 zombie-gib (small) blood trail, 64 orange tracer (rotates),
128 purple trail. The client checks them in the order gib, zombie gib, green tracer, orange tracer,
rocket, grenade, purple, and draws only the first that is set **[read]**.

How a trail is laid each rendered frame **[read]**: from the entity's previous drawn position to
its new one, distance L. The loop takes 3 units off L per particle (6 for the zombie-gib trail)
while it is above 0, and each particle is placed 1 unit further along than the previous one. So a
frame's move of L lays ceil(L/3) particles (ceil(L/6) for the zombie gib), spaced 1 unit apart,
covering only the first third (sixth) of the path **[derived]**.

| Trail | Colour | Life | Spread | Motion |
| --- | --- | --- | --- | --- |
| Gib blood (bit 4) | palette 67 to 70, random | 2 s | integer -3 to 2 per axis | starts at rest, falls at 40 units/s² |
| Zombie gib (bit 32) | palette 67 to 70 | 2 s | -3 to 2 | same, half as many particles |
| Rocket (bit 1) | fire ramp starting at 0 to 3 | ramp rises 5/s, dies at 6 (0.6 to 1.2 s) | -3 to 2 | rises at 40 units/s²; also a dynamic light of radius 200 |
| Grenade (bit 2) | fire ramp starting at 2 to 5 | same rule (0.2 to 0.8 s) | -3 to 2 | rises at 40 units/s² |
| Green tracer (bit 16) | 52 or 60, alternating every 4 particles | 0.5 s | none | 30 units/s sideways, alternate particles to alternate sides |
| Orange tracer (bit 64) | 230 or 238 | 0.5 s | none | as the green |
| Purple (bit 128) | 152 to 155 | 0.3 s | integer -8 to 7 | none |

All **[read]**; the life ranges in brackets are **[derived]** from the ramp rule. In the QW
client the trail types are the same; both blood trails use the same gravity there.

Models carrying the flags, as the game source's model build script declares them **[read]** (the
.mdl files in the pack were not opened):

- Bit 4 (gib blood trail): progs/gib1.mdl, progs/gib2.mdl, progs/gib3.mdl, and every head:
  h_player, h_dog, h_mega, h_guard, h_wizard, h_knight, h_hellkn, h_zombie, h_shams, h_shal,
  h_ogre, h_demon (all "progs/<name>.mdl").
- Bit 32 (zombie gib): progs/zom_gib.mdl (the zombie's thrown flesh and every meat spray).
- Others for reference: bit 1 on missile and lavaball; bit 2 on grenade; bit 16 on w_spike (the
  scrag's spit); bit 64 on k_spike (the hell knight's); bit 128 on v_spike (the vore's); bit 8 on
  the pickups, keys, powerups, armour and backpack.

## 6. The server moving a bouncing or tossed entity

**[read]**, per server frame:

- If the entity is on the ground it is not moved at all (no gravity, no rotation).
- Otherwise: its think runs first, then each velocity component is clamped to ±sv_maxvelocity
  (2000), gravity is applied (velocity z minus sv_gravity × frame time, times the entity's own
  gravity field if a mod sets one), then angles advance by angular velocity × frame time, then it
  is moved once along velocity × frame time.
- On a hit, the velocity has its component into the surface removed and reflected: BOUNCE uses
  an overbounce of 1.5 (so the normal component comes back at half speed), TOSS uses 1 (the
  normal component is removed: it slides). Components that end up within ±0.1 are zeroed. There is
  no second move that frame.
- Coming to rest: when the hit surface's normal has z above 0.7 (a floor no steeper than about 45
  degrees), a TOSS entity stops at once; a BOUNCE entity stops if its velocity z after the bounce
  is below 60 units/s. Stopping sets on-ground, records the ground entity, and zeroes both velocity
  and angular velocity.
- So a resting gib does not spin, and it keeps the angles it had when it stopped. It is not
  re-checked for falling afterwards in this physics path; whether a moving brush under a resting
  gib frees it is not settled by what was read.
- Non-solid and trigger entities clip only against the world and brush models.

## 7. The zombie

**[read]**:

- Health is 60, and every pain resets it to 60, so a zombie dies only to a single damage call of 60
  or more. Every death gibs (see question 1), with "zombie/z_gib.wav".
- Consequences **[derived]**: a super shotgun blast whose pellets on that zombie add to 60 (15 of
  14 pellets) gibs it, as does a rocket or grenade landing close enough; a quad shotgun blast of 4
  pellets or more gibs it; a quad super nailgun nail (72) gibs it; a quad nailgun nail (36) does not,
  since each nail is its own damage call.
- A hit below 9 is ignored.
- While knocked down, any further pain is ignored (the health still resets to 60).
- A hit of 25 or more knocks it down at once.
- A hit of 9 to 24: if the zombie is in one of its short pain animations, it only notes that it
  was hit (a later hit within 3 s will drop it). If it is not, and it was hit within the last 3 s
  (as noted), it is knocked down. Otherwise it plays one of four short pain animations, 25% each
  (paina 12 frames, painb 28, painc 18, paind 13). On nightmare skill every pain is followed by a
  5 s window, so a second hit within 5 s knocks it down.
- The knock-down animation, paine1 to paine30 at 0.1 s a frame: "zombie/z_pain.wav" on frame 1; it
  staggers back 8, 5, 3, 1, 2, 1, 1, 2 on frames 2 to 9; on frame 10 "zombie/z_fall.wav" (body
  channel) and it becomes non-solid; frame 11 is held 5 s longer (5.1 s); on frame 12 it plays
  "zombie/z_idle.wav", becomes solid again and tests whether it can stand where it lies; if not, it
  goes back to non-solid frame 11 for another 5.1 s and tries again. Then frames 13 to 30 stand it
  up (moving forward 5, 3, 1 on frames 25 to 27), and it resumes running, which clears its pain
  state. At least about 7.9 s on the ground and getting up **[derived]**.
- A lying zombie (frames 10 to 11, non-solid) cannot be hit by anything: bullets, the axe and the
  laser trace through non-solid entities, missiles do not touch them, and radius damage skips
  non-solid entities **[derived]** (the engine's radius search skips non-solid entities). During
  the fall (frames 1 to 9) and the rise (12 to 30) it is solid: a single hit of 60 or more gibs it,
  anything less is ignored.

## 8. The player

**[read]**:

- On death: invisibility, invulnerability, quad and suit end; the model returns to the player's
  own (no floating eyes); in deathmatch or co-op a backpack drops; the weapon model is hidden; the
  view offset becomes 8 below the origin; the body becomes non-solid, leaves the ground and tosses
  (MOVETYPE_TOSS); if its upward velocity is below 10, a uniform 0 to 300 is added to it.
- Gibs below -40 health (after the clamp to -99). Thrown: the head "progs/h_player.mdl" (the
  player entity itself becomes the head, first) and gib1, gib2, gib3, one each, all with the
  player's health as the damage figure. Sound (voice channel, no attenuation): "player/teledth1.wav"
  if killed by a telefrag, otherwise "player/gib.wav" or "player/udeath.wav" at 50% each. A gibbed
  player may respawn as soon as all buttons have been released.
- Not gibbed: a death sound on the voice channel with no attenuation: under water (fully
  submerged) "player/h2odeath.wav" plus 20 bubbles; otherwise one of "player/death1.wav" to
  "player/death5.wav", where 1 and 5 have chance 1/8 each and 2, 3, 4 have 1/4 each (a rounded
  uniform 1 to 5) **[derived]** from the rounding. Pitch and roll of the body are zeroed.
- Death sequences: with the axe in hand, always axdeth1 to axdeth9 (indices 41 to 49). Otherwise
  one of five: deatha1 to deatha11 (50 to 60), deathb1 to deathb9 (61 to 69), deathc1 to deathc15
  (70 to 84), deathd1 to deathd9 (85 to 93), deathe1 to deathe9 (94 to 102). The pick is 1 plus the
  floor of 6 × random: a, b, c, d each 1/6, e 2/6 (plus the rare draw of exactly 1, which also
  gives e) **[derived]**. A setting named "temp1", when nonzero, forces the choice (1 a, 2 b, 3 c,
  4 d, anything else e); it defaults to 0 **[read]**. 0.1 s a frame; on the last frame the player
  is marked dead and stops thinking; the body stays on that frame.
- Respawn: once dead, all buttons must be released, then any of attack, jump or use pressed. In
  single player that restarts the level; in co-op and deathmatch a copy of the body is left and the
  player respawns. While dead on the ground, the body's speed is cut by 20 units/s each frame.
- The camera **[read]**: the view is always the player entity's origin plus the view offset (and
  the view bob), kept within 14 units horizontally and from 22 below to 30 above the entity's
  origin. A dead player has no movement input, but the client still turns the view with the mouse.
  While health is 0 or below, the view roll is forced to 80 degrees. When gibbed, the player entity
  is the head, so the camera rides the bouncing head, 8 units above its origin, rolled 80 degrees,
  the head's own spin does not turn the view (the view's angles come from the client's input, not
  from the entity) **[derived]**. Not gibbed: the camera sits 8 below the tossed body's origin,
  rolled 80 degrees.
