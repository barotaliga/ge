# Quake II's muzzle flashes (clean-room facts)

Read by a clean-room reader, 2026-11-21, from Yamagi Quake II (a1d7e2b) and upstream Q2PRO (601a8df). Built from by `games/shooters_edge/server/q2_flash.cpp`. No code.


Sources read: Yamagi Quake II at commit a1d7e2b (its client, its server and its base game library: this
release carries no mission-pack game code) and upstream Q2PRO at commit 601a8df (its client,
its documentation of client settings and its extended-protocol handling; its bundled game library is
the base game too). Every fact is marked **[read]** (stated by the source), **[derived]** (worked out
from what the source does) or **[measured]** (computed by a script of ours over the source's numbers,
no values copied). "Yamagi" and "Q2PRO" below mean those two releases.

Conventions used throughout:

- Distances are in map units. Times are in milliseconds (ms) unless said otherwise. The server runs at
  10 frames a second, one frame every 100 ms **[read]**.
- Sound channels are the wire numbers of the sound message: auto 0, weapon 1, voice 2, item 3, body 4.
  Attenuations: none 0 (heard at full volume everywhere), normal 1, idle 2, static 3 **[read]**.
- "rand31" means a uniformly random whole number from 0 to 31 (a random integer masked to its low five
  bits) **[derived]**.
- A light's colour is red, green and blue fractions. A negative colour (all three at -1) is a light
  that darkens what it touches **[read]**.

---

## 1. The player's flash: bytes and placement

**Bytes on the wire** **[read]** (both releases), 4 bytes in all:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 byte | message number 1 |
| 1 | 2 bytes, little-endian | entity number of the player who fired |
| 3 | 1 byte | flash value; bit 7 (value 128) is the silenced bit, bits 0 to 6 the flash value |

- The receiver strips bit 7 into a "silenced" flag and uses the remaining seven bits as the value
  **[read]**.
- Entity number: Yamagi reads it as a signed 16-bit number and drops the connection with an error when
  it is below 1 or above 32767 **[read]**. Q2PRO reads it as an unsigned 16-bit number and drops the
  connection when it is below 1 or not below the session's entity limit **[read]**. Q2PRO never packs
  extra bits into the player flash's entity field (contrast question 5) **[read]**.
- Yamagi does not range-check the flash value; a value it has no case for still makes a light (see
  question 2, "unknown values") **[read]**.

**Where the light goes** **[read]** (both releases, same numbers):

- Start at the firing entity's current origin as the client has it (the interpolation target of the
  latest frame, not the drawn position).
- Build the forward and right vectors from that entity's current angles (all three: pitch, yaw and
  roll).
- Move 18 units along forward and 16 units along right. There is no height offset: the light sits at
  the entity origin's height plus whatever the tilted forward and right vectors contribute.
- **[read, from the game]** a player entity's angles on the wire are: yaw equal to the view yaw, pitch
  equal to one third of the view pitch, roll equal to a lean computed from sideways speed (multiplied by
  4). So the light swings a little up or down with a third of the aim pitch and tips with strafe roll
  **[derived]**.
- Q2PRO additionally overwrites its record of the player's own entity, every frame, from the player
  state, so that an effect aimed at the own entity has an origin even when that entity is not sent
  (for example the entering flash) **[read]**.

---

## 2. The player's flash values: light, sound and what is drawn

### Rules shared by every value

- **Radius** **[read]**: 200 + rand31 (200 to 231) when not silenced, 100 + rand31 (100 to 131) when
  silenced. The chain gun values override this whatever the silenced bit says (see the table).
- **Volume** **[read]**: 1.0, or 0.2 when silenced. The entering, leaving and respawn values always use
  1.0.
- **Attenuation**: normal (1) for every sound below **[read]**.
- **One light per entity** **[read]**: the light is looked up by the firing entity's number; a new
  flash from the same entity takes over that entity's light. Otherwise the first expired slot is taken,
  and when none is free slot 0 is overwritten. Yamagi has 32 slots and reuses a slot without clearing
  it; Q2PRO has 64 and clears the slot first.
- **How long the light lasts, Yamagi** **[read, derived]**: the light's expiry is set to the client
  time of the message and the light is dropped once the client time passes expiry + 32 ms. So a
  default flash shows for about 32 ms (the release says it adds the 32 ms so a light survives at least
  one rendered frame at high frame rates). Some values push the expiry later (table). The light keeps
  its full radius until it is dropped: nothing in this release ever gives a muzzle light a fade
  **[read]**.
- **How long the light lasts, Q2PRO** **[read]**: expiry = client time + the setting
  `cl_muzzlelight_time` (default 16 ms, clamped to 0 to 1000), and the light shows while the client
  time has not passed expiry; the same values as in Yamagi push the expiry to a fixed time instead.
- **Light position**: question 1.

### Base game values 0 to 15

Colour is R, G, B. "Light time" is the Yamagi rule unless noted. Sounds are on the firing entity, weapon
channel (1), delay 0, unless the row says otherwise.

| Value | Weapon or event | Colour | Radius | Light time | Sounds |
| --- | --- | --- | --- | --- | --- |
| 0 | blaster | 1, 1, 0 | default | default | `weapons/blastf1a.wav` |
| 1 | machine gun | 1, 1, 0 | default | default | one of `weapons/machgf1b.wav` to `weapons/machgf5b.wav`, picked at random each shot |
| 2 | shotgun | 1, 1, 0 | default | default | `weapons/shotgf1b.wav`; then `weapons/shotgr1b.wav` on the auto channel (0) after 0.1 s |
| 3 | chain gun, 1 shot | 1, 0.25, 0 | 200 + rand31 always | default | one random `weapons/machgfNb.wav` (N from 1 to 5) |
| 4 | chain gun, 2 shots | 1, 0.5, 0 | 225 + rand31 always | expiry + 0.1 ms, i.e. no longer in practice (see note) | two random `machgfNb.wav`, the second 0.05 s later, both on the weapon channel |
| 5 | chain gun, 3 shots | 1, 1, 0 | 250 + rand31 always | as value 4 | three random `machgfNb.wav` at 0, 0.033 and 0.066 s, all on the weapon channel |
| 6 | railgun | 0.5, 0.5, 1 | default | default | `weapons/railgf1a.wav` (Q2PRO adds `weapons/railgr1b.wav` on the auto channel after 0.4 s) |
| 7 | rocket launcher | 1, 0.5, 0.2 | default | default | `weapons/rocklf1a.wav`; then `weapons/rocklr1b.wav` on the auto channel after 0.1 s |
| 8 | grenade launcher | 1, 0.5, 0 | default | default | `weapons/grenlf1a.wav`; then `weapons/grenlr1b.wav` on the auto channel after 0.1 s |
| 9 | entering the game | 0, 1, 0 | default | expiry + 1 ms (Yamagi); the setting (Q2PRO) | `weapons/grenlf1a.wav` at volume 1 always |
| 10 | leaving the game | 1, 0, 0 | default | as value 9 | `weapons/grenlf1a.wav` at volume 1 always |
| 11 | respawn | 1, 1, 0 | default | as value 9 | `weapons/grenlf1a.wav` at volume 1 always |
| 12 | BFG | 0, 1, 0 | default | default | `weapons/bfg__f1y.wav` |
| 13 | super shotgun | 1, 1, 0 | default | default | `weapons/sshotf1b.wav` |
| 14 | hyperblaster | 1, 1, 0 | default | default | `weapons/hyprbf1a.wav` |
| 15 | item respawn | no case in either client | default | default | none |

Notes on the table:

- **Chain gun light time** **[derived]**: values 4 and 5 add one tenth of a unit to an expiry kept in
  milliseconds, so the intended "longer" light is 0.1 ms longer, which no one can see. Likewise values
  9 to 11 add 1 ms. In Q2PRO the chain gun and the entering, leaving and respawn values simply use the
  setting.
- **Entering, leaving, respawn particles** **[read]**: 500 particles in a box around the entity's
  origin, 32 units wide in x and y (origin minus 16 to plus 16) and from 24 below to 32 above the
  origin; each with a random speed of -20 to +20 units/s on every axis, falling under a gravity of 40
  units/s², fading from full in 1.0 to 1.3 s. Palette colours: entering 208 to 215 (green), leaving 64
  to 71 (red), respawn 224 to 231 (yellow) (the base index plus a random 0 to 7).
- **Value 15, item respawn** **[read]**: neither client has a case for it. It still makes a light with
  the default radius and lifetime but sets no colour, so in Yamagi the light keeps whatever colour its
  slot last held (unset, so black, on a fresh slot) and in Q2PRO it is black (cleared slot), which adds
  no visible light **[derived]**. The base game never sends it (question 9) **[read]**.

### Mission-pack values

| Value | Weapon | Colour | Radius | Light time | Sounds |
| --- | --- | --- | --- | --- | --- |
| 16 | ion ripper | 1, 0.5, 0.5 | default | default | `weapons/rippfire.wav` |
| 17 | blue hyperblaster | 0, 0, 1 | default | default | `weapons/hyprbf1a.wav` |
| 18 | phalanx | 1, 0.5, 0.5 | default | default | `weapons/plasshot.wav` |
| 19 | (Q2PRO only) the BFG's second flash | 0, 1, 0 | default | default | none |
| 20 | (Q2PRO only) the phalanx's second flash | 1, 0.5, 0.5 | default | default | none |
| 30 | ETF rifle | 0.9, 0.7, 0 | default | default | `weapons/nail1.wav` |
| 31 | unused in Yamagi (no case); in Q2PRO the prox launcher | Q2PRO: 1, 0.5, 0 | default | default | Q2PRO: `weapons/grenlf1a.wav`, then `weapons/proxlr1a.wav` on the auto channel after 0.1 s |
| 32 | shotgun 2 | 1, 1, 0 | default | default | `weapons/shotg2.wav` (but see the extended variant below) |
| 33 | heat beam | 1, 1, 0 | default | expiry + 100 ms (shows about 132 ms in Yamagi, 100 ms in Q2PRO) | none |
| 34 | blaster 2 | 0, 1, 0 | default | default | `weapons/blastf1a.wav` |
| 35 | tracker (disruptor) | -1, -1, -1 (a darkening light) | default | default | `weapons/disint2.wav` |
| 36 | nuke, stage 1 | 1, 0, 0 | default | expiry + 100 ms | none |
| 37 | nuke, stage 2 | 1, 1, 0 | default | expiry + 100 ms | none |
| 38 | nuke, stage 4 | 0, 0, 1 | default | expiry + 100 ms | none |
| 39 | nuke, stage 8 | 0, 1, 1 | default | expiry + 100 ms | none |

- Values 19 to 29 (Yamagi) and 21 to 29 (Q2PRO) have no case: same outcome as value 15 **[read,
  derived]**.
- **Extended-protocol variant** **[read]**: Q2PRO, when the server announces the extended (remastered
  game) configuration, reads value 32 as a second ETF rifle flash: colour 0.9, 0.7, 0 and
  `weapons/nail1.wav`, instead of the shotgun 2.

### Anything else drawn

- **Yamagi** **[read]**: nothing besides the light, the sounds and the particles of values 9 to 11. No
  flash model is drawn for a player's flash, on the own view weapon or on anyone else.
- **Q2PRO** **[read]**: when the setting `cl_muzzleflashes` is on (default 1) and the flash belongs to
  the local player, a flash model is drawn on the view weapon for 50 ms (question 4 and 12). The models
  are `models/weapons/<name>/flash/tris.md2` where name is one of `v_machn`, `v_shotg2`, `v_shotg`,
  `v_rocket`, `v_rail`, `v_launch`, `v_etf_rifle`, `v_dist`, `v_boomer`, `v_blast`, `v_bfg`, `v_beamer`.
  These files exist only in the remastered content, and with them missing nothing is drawn. Which
  weapon gets which model, and at what scale **[read]**: blaster `v_blast` 8, hyperblaster `v_blast` 9,
  machine gun `v_machn` 12, shotgun `v_shotg` 12, super shotgun `v_shotg2` 12, chain gun `v_machn` at
  12, 16 and 20 for one, two and three shots, railgun `v_rail` 12, rocket launcher `v_rocket` 10,
  grenade launcher and prox launcher `v_launch` 9, the BFG's second flash (19) `v_bfg` 16, the
  phalanx's second flash (20) `v_rocket` 9, ion ripper `v_boomer` 15, ETF rifle (and the extended 32)
  `v_etf_rifle` 4, heat beam `v_beamer` 16, tracker `v_dist` 10. The blue hyperblaster, the BFG's first
  flash, the phalanx's first flash, blaster 2 and the nukes draw no model. The machine gun's and the
  heat beam's models get a random roll of 0 to 359 degrees each shot; the others have none. Each model
  sits at a fixed small offset from the view weapon (about 18 to 29 ahead, 4 to 10 right, 5 to 11 down,
  **[derived]** rounded from the per-weapon numbers).

---

## 3. The silenced case

**What changes on the receiving client** **[read]** (both releases):

- The light's radius drops from 200 + rand31 to 100 + rand31. The chain gun values (3, 4, 5) ignore the
  silenced bit for the radius and keep 200, 225 and 250 + rand31.
- Every weapon sound is played at volume 0.2 instead of 1.0, including the delayed second sounds (the
  shotgun's, rocket's and grenade's tails, the chain gun's extra shots).
- Colour, lifetime and attenuation do not change. The entering, leaving and respawn values always play
  at volume 1, but the game never sets the bit on them (question 9).
- In Q2PRO the view-weapon flash model is drawn the same whether silenced or not **[read]**.

**How the server decides** **[read]** (base game):

- Picking up the silencer adds 30 to the player's silencer count (it stacks with what is left).
- Before each weapon think (once per server frame, or once in the player's command when attack was
  newly pressed, question 9) the game sets the silenced bit for that think when the count is above
  zero. Every player flash sent during that think carries bit 7 (value 128).
- The count goes down by one each time the weapon makes "noise" for the monsters' hearing, which every
  firing weapon does once per shot frame, after the flash is sent. When the count reaches zero the next
  shot is loud.
- Consequences **[derived]**: one silencer lasts 30 firing frames. A chain gun frame of two or three
  bullets costs one, a hyperblaster or machine gun shot costs one, and a BFG shot costs two (one at
  its flash frame, one at its launch frame). While silenced the game also skips the monsters'
  hearing of that shot (in single player it would otherwise wake monsters).

---

## 4. The own player's flash against another player's

- **Yamagi** **[read]**: the parser and the effect are the same for every entity: the same light, at
  the same place computed from that entity's state, the same sounds and no model. **[read]** The only
  difference comes from the sound system: a sound tied to the local player's own entity number is not
  placed in space; it plays at full left and right volume with no distance falloff. So the shooter
  hears their own shots centred and undimmed, everyone else hears them from the shooter's place.
- **Q2PRO** **[read]**: the light and the sounds are the same for everyone, with the same own-entity
  sound rule as above. Only the local player's own flash adds the view-weapon flash model (question 2,
  "Anything else drawn"); other players' flashes draw no model on their third-person figure.
- **Edge case for every flash** **[read, derived]**: in both sound systems a new sound on a non-zero
  channel of an entity cuts off the sound already playing on that same channel of that entity. So the
  chain gun's second and third shots (all on the weapon channel) each cut the previous one short when
  they start, and a player's next flash cuts the last shot's sound; the delayed tails on the auto
  channel (0) are never cut this way.
- Neither client predicts the flash: the own player's flash also arrives from the server, one round trip
  after the shot **[derived]**.

---

## 5. The monster's flash: bytes

**Base protocol** **[read]** (both releases), 4 bytes in all:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 byte | message number 2 |
| 1 | 2 bytes, little-endian | entity number of the monster (or other non-player shooter) |
| 3 | 1 byte | flash value, 0 to 255; there is no silenced bit |

- Yamagi: entity below 1 or above 32767 drops the connection with an error; a value above 210 is
  ignored with a developer-only message and nothing happens **[read]**.
- **Extended variant** **[read]**: Q2PRO, when the server announces the extended (remastered game)
  configuration, reads the same 4 bytes but splits the 16-bit entity field: the low 13 bits are the
  entity number (0 to 8191) and the top 3 bits are bits 8 to 10 of the flash value, so the value is
  (top three bits times 256) plus the value byte, up to 2047. Without that configuration the field is
  the plain entity number. A value at or above 289 (one past the last one Q2PRO knows), or an entity
  number below 1 or not below the entity limit, drops the connection **[read]**.
- **[derived]** a server writing values above 255 must use that packing, and only a client that knows
  the extended configuration can read them; a stock client reads such a message as a wrong entity.

---

## 6. The monster's flash values

The numbers are the protocol and are exact **[read]**. Values 1 to 82 and 101 to 137 are the base game's
own monsters; 83 to 100 are more of the soldier's postures. Values 138 to 210 belong to the mission packs'
monsters (they are defined in both releases' shared list; the senders are not in the code read).
Values 211 to 288 exist only in Q2PRO, for the remastered game, and need the extended variant
(question 5) beyond 255.

| Values | Monster and gun |
| --- | --- |
| 1 to 3 | tank, blaster |
| 4 to 22 | tank, machine gun (19 positions along its sweep) |
| 23 to 25 | tank, rockets |
| 26 to 38 | infantry, machine gun |
| 39, 40 | soldier, blaster (postures 1, 2) |
| 41, 42 | soldier, shotgun (postures 1, 2) |
| 43, 44 | soldier, machine gun (postures 1, 2) |
| 45 to 52 | gunner, machine gun |
| 53 to 56 | gunner, grenades |
| 57 | iron maiden (chick), rocket |
| 58, 59 | flyer, blasters (two guns) |
| 60 | medic, blaster |
| 61 | gladiator, railgun |
| 62 | icarus (hover), blaster |
| 63 | the actor's machine gun |
| 64 to 69 | supertank, machine gun |
| 70 to 72 | supertank, rockets |
| 73 to 77 | hornet (boss 2), left machine gun |
| 78 to 81 | hornet (boss 2), rockets |
| 82 | technician (floater), blaster |
| 83 to 100 | soldier, postures 3 to 8, in threes: blaster, shotgun, machine gun (83, 84, 85 for posture 3; 86, 87, 88 for 4; up to 98, 99, 100 for 8) |
| 101 | Makron, BFG |
| 102 to 118 | Makron, blaster (17 positions) |
| 119 | Makron, railgun |
| 120 to 125 | Jorg, left machine gun |
| 126 to 131 | Jorg, right machine gun |
| 132 | Jorg, BFG |
| 133 to 137 | hornet (boss 2), right machine gun |
| 138 | carrier, left machine gun |
| 139 | carrier, right machine gun |
| 140 | carrier, grenade |
| 141 | turret, machine gun |
| 142 | turret, rocket |
| 143 | turret, blaster |
| 144 | stalker, blaster |
| 145 | daedalus, blaster |
| 146 | medic commander's blaster (the second medic blaster) |
| 147 | carrier, railgun |
| 148 | widow, disruptor |
| 149 | widow, blaster |
| 150 | widow, railgun |
| 151 | widow, plasma beam |
| 152 | carrier, left machine gun (second position) |
| 153 | carrier, right machine gun (second position) |
| 154 | widow, railgun from the left |
| 155 | widow, railgun from the right |
| 156 to 164 | widow, blaster sweep (9 positions) |
| 165 to 175 | widow, blaster aimed at 100, 90, 80 down to 0 degrees (11 values, in that order) |
| 176 to 182 | widow, blaster aimed at 10 to 70 degrees to the left (7 values) |
| 183 to 190 | widow, blaster while running (8 positions) |
| 191 to 194 | carrier, rockets (4 positions) |
| 195 to 199 | widow's second form, beam (5 positions) |
| 200 to 210 | widow's second form, beam sweep (11 positions) |
| 211 to 218 | (Q2PRO) soldier, ion ripper, postures 1 to 8 |
| 219 to 226 | (Q2PRO) soldier, hyperblaster, postures 1 to 8 |
| 227 | (Q2PRO) guardian, blaster |
| 228, 229 | (Q2PRO) arachnid, railgun (two guns) |
| 230, 231 | (Q2PRO) arachnid, railgun aimed up (two guns) |
| 232 to 239 | (Q2PRO) infantry, machine gun while running |
| 240, 241 | (Q2PRO) gunner commander, chain gun (straight, dodging) |
| 242 to 244 | (Q2PRO) gunner commander, mortar grenades |
| 245 to 247 | (Q2PRO) gunner commander, grenades forward |
| 248 to 250 | (Q2PRO) gunner commander, grenades crouching |
| 251 to 255 | (Q2PRO) soldier lying prone: blaster, shotgun, machine gun, ion ripper, hyperblaster |
| 256 to 259 | (Q2PRO) gunner, grenades forward (alternate) |
| 260 | (Q2PRO) infantry, machine gun (one more position) |
| 261, 262 | (Q2PRO) supertank, grenades |
| 263 | (Q2PRO) icarus, blaster on its other side |
| 264 | (Q2PRO) daedalus, blaster on its other side |
| 265 to 276 | (Q2PRO) medic commander, hyperblaster sweep one way (12 positions) |
| 277 to 288 | (Q2PRO) medic commander, hyperblaster sweep the other way (12 positions) |

Value 0 is never used (its placement is all zero) **[read]**. The "actor" (63) has no sender in the
Yamagi base game read **[read]**.

---

## 7. The monster's flash groups: light, sound, particles, smoke and models

### Shared by every value

- **Radius** **[read]**: 200 + rand31 (200 to 231) for every value except the widow's beams.
- **Light time** **[read]**: as for the player's flash (Yamagi about 32 ms; Q2PRO the setting
  `cl_muzzlelight_time`, default 16 ms), except the widow's beams.
- **Sound** **[read]**: always on the monster's entity, weapon channel (1), volume 1, delay 0; the
  attenuation is normal (1) unless the table says "none" (heard at full volume on the whole level).
- **"Puff"** **[read]**: 40 particles at the flash point, each placed -4 to +3 units off it on every
  axis, palette colours 0 to 7 (black to dark grey), a random speed of -20 to +20 units/s on every
  axis, falling at 39.8 units/s² (the particle gravity of 40 less 0.2), fading from full in 0.5 to
  0.8 s.
- **"Smoke and flash"** **[read, derived]**: two short-lived models at the flash point.
- Smoke: `models/objects/smoke/tris.md2`, translucent. Its clock starts 100 ms before the latest
  server frame's time; it shows model frames 1 to 3 in turn (one per 100 ms) and its opacity falls
  from 1 to 0 over 300 ms of that clock, so it is gone 200 ms after the latest server frame's time.
- Flash: `models/objects/flash/tris.md2`, full-bright, on the same clock; it shows only during the
  first 100 ms of the clock, that is until the client's time reaches the latest server frame's time
  (at most 100 ms, often less, since the client's time runs behind the latest frame).
- **Q2PRO model** **[read]**: Q2PRO adds, when `cl_muzzleflashes` is on and the remastered model file
  exists, one of the flash models named in question 2 at a point 4 units (times the monster's scale)
  ahead of the light point, turned by the monster's angles, translucent, full-bright and casting no
  shadow, with a random roll of 0 to 359 degrees (except the `v_boomer` model), shown for 50 ms. Its
  size is the number in the table times the monster's scale. Skin 0 unless the table says otherwise
  (`v_blast` skin 1 is blue, skin 2 green).

### The groups (base and mission packs, both releases unless marked)

| Values | Colour | Sound | Puff | Smoke and flash | Q2PRO model, size |
| --- | --- | --- | --- | --- | --- |
| 26 to 38 infantry machine gun | 1, 1, 0 | `infantry/infatck1.wav` | yes | yes | `v_machn` 18 |
| 43, 44, 85, 88, 91, 94, 97, 100 soldier machine gun | 1, 1, 0 | `soldier/solatck3.wav` | yes | yes | `v_machn` 13 |
| 45 to 52 gunner machine gun | 1, 1, 0 | `gunner/gunatck2.wav` | yes | yes | `v_machn` 24 |
| 63 actor, 64 to 69 supertank machine gun, 141 turret machine gun | 1, 1, 0 | `infantry/infatck1.wav` | yes | yes | `v_machn` 32 |
| 73 to 77 hornet left gun, 138 and 152 carrier left gun | 1, 1, 0 | `infantry/infatck1.wav`, attenuation none | yes | yes | `v_machn` 32 |
| 133 to 137 hornet right gun, 139 and 153 carrier right gun | 1, 1, 0 | none | yes | yes | `v_machn` 32 |
| 39, 40, 83, 86, 89, 92, 95, 98 soldier blaster; 143 turret blaster | 1, 1, 0 | `soldier/solatck2.wav` | no | no | `v_blast` 8 |
| 58, 59 flyer | 1, 1, 0 | `flyer/flyatck3.wav` | no | no | `v_blast` 8 |
| 60 medic | 1, 1, 0 | `medic/medatck1.wav` | no | no | `v_blast` 8 |
| 62 icarus | 1, 1, 0 | `hover/hovatck1.wav` | no | no | `v_blast` 8 |
| 82 technician | 1, 1, 0 | `floater/fltatck1.wav` | no | no | `v_blast` 8 |
| 41, 42, 84, 87, 90, 93, 96, 99 soldier shotgun | 1, 1, 0 | `soldier/solatck1.wav` | no | yes | `v_shotg` 17 |
| 1 to 3 tank blaster | 1, 1, 0 | `tank/tnkatck3.wav` | no | no | `v_blast` 24 |
| 4 to 22 tank machine gun | 1, 1, 0 | one of `tank/tnkatk2a.wav` to `tank/tnkatk2e.wav`, at random | yes | yes | `v_machn` 20 |
| 57 iron maiden rocket, 142 turret rocket | 1, 0.5, 0.2 | `chick/chkatck2.wav` | no | no | `v_rocket` 16 |
| 23 to 25 tank rockets | 1, 0.5, 0.2 | `tank/tnkatck1.wav` | no | no | `v_rocket` 28 |
| 70 to 72 supertank rockets, 78 to 81 hornet rockets, 191 carrier rocket (first only) | 1, 0.5, 0.2 | `tank/rocket.wav` | no | no | `v_rocket` 28 |
| 53 to 56 gunner grenades | 1, 0.5, 0 | `gunner/gunatck3.wav` | no | no | `v_launch` 18 |
| 61 gladiator, 147 carrier, 150 widow railgun | 0.5, 0.5, 1 | none | no | no | `v_rail` 32 |
| 119 Makron railgun | Yamagi: no case (see below); Q2PRO: 0.5, 0.5, 1 | none | no | no | Q2PRO `v_rail` 32 |
| 101 Makron BFG, 132 Jorg BFG | 0.5, 1, 0.5 | none | no | no | `v_bfg` 64 |
| 102 to 118 Makron blaster | 1, 1, 0 | `makron/blaster.wav` | no | no | `v_blast` 22 |
| 120 to 125 Jorg left gun | 1, 1, 0 | `boss3/xfire.wav` | yes | yes | `v_machn` 32 |
| 126 to 131 Jorg right gun | 1, 1, 0 | none | yes | yes | `v_machn` 32 |
| 144 stalker, 145 daedalus, 146 medic commander, 149 widow, 156 to 190 widow blasters | 0, 1, 0 | `tank/tnkatck3.wav` | no | no | `v_blast` skin 2 (green), 22 |
| 148 widow disruptor | -1, -1, -1 (darkening) | `weapons/disint2.wav` | no | no | `v_dist` 32 |
| 151 widow plasma beam, 195 to 210 widow's second form beams | 1, 1, 0 | none | no | no | `v_beamer` 32 |

Special rules:

- **Widow beams (151, 195 to 210)** **[read, derived]**: radius 300 plus a random number masked by 100
  (so one of 300, 304, 332, 336, 364, 368, 396 or 400); expiry 200 ms after the message, so the light
  shows about 232 ms in Yamagi and 200 ms in Q2PRO.
- **No case at all** **[read]**: Yamagi has none for 119 (Makron railgun), 140 (carrier grenade), 154,
  155 (widow side railguns) and 192 to 194 (carrier rockets 2 to 4); Q2PRO has none for 140, 154, 155,
  192 to 194 and 248 to 250. Such a value still places a light with radius 200 + rand31 and the
  default time but no colour: in Yamagi the slot keeps the colour it last had (none on a fresh slot), in
  Q2PRO the slot is cleared to black, so no visible light; no sound, no particles, no model
  **[derived]**.
- **Hornet's second guns, extended variant** **[read]**: in Q2PRO with the extended configuration,
  value 74 (hornet left gun, second position) and value 134 (hornet right gun, second position) become
  blasters: `flyer/flyatck3.wav` at attenuation none, model `v_blast` 12, no puff, no smoke; 74 keeps
  the colour 1, 1, 0 and 134 sets no colour.

### Values only Q2PRO knows (211 to 288)

| Values | Colour | Sound | Puff | Smoke and flash | Model, size |
| --- | --- | --- | --- | --- | --- |
| 211 to 218 and 254 soldier ion ripper | 1, 0.5, 0.5 | `weapons/rippfire.wav` | no | no | `v_boomer` 32 |
| 219 to 226 and 255 soldier hyperblaster | 0, 0, 1 | `weapons/hyprbf1a.wav` | no | no | `v_blast` skin 1 (blue), 8 |
| 227 guardian blaster | 1, 1, 0 | `weapons/hyprbf1a.wav` | no | no | `v_blast` 16 |
| 228 to 231 arachnid railguns | 0.5, 0.5, 1 | none | no | no | `v_rail` 32 |
| 232 to 239 and 260 infantry machine gun | as 26 to 38 | as 26 to 38 | yes | yes | `v_machn` 18 |
| 240, 241 gunner commander chain gun | 0, 0, 1 | `guncmdr/gcdratck2.wav` | no | no | `v_etf_rifle` 16 |
| 242 to 247 gunner commander grenades | 1, 0.5, 0 | `guncmdr/gcdratck3.wav` | no | no | `v_launch` 18 |
| 248 to 250 gunner commander crouching grenades | no case | none | no | no | none |
| 251 soldier prone blaster | as soldier blaster | | | | |
| 252 soldier prone shotgun | as soldier shotgun | | | | |
| 253 soldier prone machine gun | as soldier machine gun | | | | |
| 256 to 259 gunner grenades (alternate), 261, 262 supertank grenades | 1, 0.5, 0 | `gunner/gunatck3.wav` | no | no | `v_launch` 18 |
| 263 icarus other side | as 62 | | | | |
| 264 daedalus other side | as 145 | | | | |
| 265 to 276 medic commander sweep one way | as the medic blaster (60) | | | | |
| 277 to 288 medic commander sweep the other way | as the widow blaster group (0, 1, 0, `tank/tnkatck3.wav`, `v_blast` skin 2, 22) | | | | |

---

## 8. Where the monster's light goes

**The rule** **[read]** (both releases):

- Each flash value has its own offset of three numbers in a table both the game and the client carry:
  a distance ahead, a distance to the right, and a height, in the monster's own frame. Value 0's offset
  is all zero. Several values of one monster exist because its gun is in a different place on
  different animation frames.
- The client takes the monster's current origin and current angles (pitch, yaw and roll), builds the
  forward and right vectors from the angles, and places the light at origin + ahead times forward +
  right-distance times right, then adds the height straight up the world's vertical axis. The height is
  NOT turned by the monster's pitch or roll; the ahead and right distances are.
- **Scale**: Yamagi never scales the offset. Q2PRO multiplies all three numbers by the monster's
  drawn scale (from the extended entity state; a scale of 0 counts as 1), and also scales the Q2PRO
  flash model's size and its 4-unit lead by the same factor.
- **[read]** The game uses the same table to start the monster's projectile or bullet: it places the
  shot's start point from the monster's origin and angles by the same rule (ahead and right turned,
  height straight up), which is also the point
  the flash message is sent from (question 10). So a client that places the light by the table puts it
  at the shot's origin.
- **[measured]** 26 of the values 1 to 210 have different offsets in the two releases (Q2PRO updated
  them): the tank blaster (1 to 3, by up to 8 units), three soldier postures (40, 42, 44, by up to 5),
  the flyer (58, 59, up to 8), the medic (60, up to 32), the icarus (62, up to 31), the supertank's
  machine gun (64 to 69, up to 9) and rockets (70 to 72, up to 21), the Makron railgun (119, up to 35),
  the turret (141 to 143, up to 4), the daedalus (145, up to 31), the medic commander (146, up to 32)
  and the widow disruptor (148, up to 7). The summary below uses Yamagi's numbers for 1 to 210 and
  Q2PRO's for 211 to 288.

**Derived summary** **[derived]**: typical place of the gun, rounded to the nearest 8 units; "ahead",
"right" (negative is left) and "up" from the monster's origin; a range in brackets where the values of
one gun spread. Computed by a script of ours that averaged and bounded each group's offsets; no
offset was copied.

| Monster, gun (values) | Ahead | Right | Up |
| --- | --- | --- | --- |
| tank, blaster (1 to 3) | 16 [8 to 24] | -24 [-24 to -16] | 32 |
| tank, machine gun (4 to 22) | 16 [-8 to 32] | 24 [-8 to 40] | 24 [16 to 32] |
| tank, rockets (23 to 25) | 8 | 24 [16 to 32] | 48 |
| infantry, machine gun (26 to 38) | 8 [-16 to 24] | 16 [8 to 24] | 16 [16 to 32] |
| soldier, all guns and postures (39 to 44, 83 to 100) | 24 [8 to 40] | 8 [8 to 16] | 0 [-24 to 24] |
| gunner, machine gun (45 to 52) | 32 | 0 [0 to 8] | 24 |
| gunner, grenades (53 to 56) | 8 | -16 | 8 |
| iron maiden, rocket (57) | 24 | -8 | 40 |
| flyer, blasters (58, 59) | 16 | 0 [-8 to 16] | -16 |
| medic, blaster (60) | 16 | 8 | 16 |
| gladiator, railgun (61) | 32 | 16 | 32 |
| icarus, blaster (62) | 32 | 0 | 8 |
| actor, machine gun (63) | 16 | 8 | 8 |
| supertank, machine gun (64 to 69) | 32 | 32 | 88 |
| supertank, rockets (70 to 72) | 16 | -32 [-40 to -24] | 88 [80 to 88] |
| hornet, left gun (73 to 77) | 32 | -40 | 72 |
| hornet, right gun (133 to 137) | 32 | 40 | 72 |
| hornet, rockets (78 to 81) | 24 | 0 [-16 to 16] | 8 |
| technician, blaster (82) | 32 | 0 | 8 |
| Makron, BFG (101) | 16 | -16 | 64 |
| Makron, blaster (102 to 118) | 0 [-8 to 8] | 0 [-24 to 40] | 56 |
| Makron, railgun (119) | -16 | 8 | 72 |
| Jorg, left gun (120 to 125) | 80 | -48 | 96 |
| Jorg, right gun (126 to 131) | 80 | 48 | 96 |
| Jorg, BFG (132) | 8 | -8 | 112 |
| carrier, left gun (138, 152) | 56 [56 to 64] | -32 | 24 [16 to 32] |
| carrier, right gun (139, 153) | 56 [56 to 64] | 32 | 24 [16 to 32] |
| carrier, grenade (140) | 40 | 24 | 48 |
| carrier, railgun (147) | 32 | 0 | 8 |
| carrier, rockets (191 to 194) | 0 | 0 | -8 |
| turret, all guns (141 to 143) | 16 | 0 | 0 |
| stalker, blaster (144) | 24 | 0 | 8 |
| daedalus, blaster (145) | 32 | 0 | 8 |
| medic commander, blaster (146) | 16 | 8 | 16 |
| widow, disruptor (148) | 56 | 16 | 88 |
| widow, railgun (150) | 64 | -16 | 80 |
| widow, side railguns (154; 155) | 16; 64 | -64; 16 | 88 |
| widow, plasma beam (151) | 32 | 0 | 8 |
| widow, blaster (149, 165 to 175) | 32 [-16 to 56] | 56 [24 to 80] | 80 [32 to 88] |
| widow, blaster sweep (156 to 164) | 64 [48 to 72] | 16 [-16 to 56] | 88 |
| widow, blaster to the left (176 to 182) | 56 [32 to 64] | -8 [-40 to 16] | 80 |
| widow, blaster running (183 to 190) | 72 | 8 | 88 [80 to 88] |
| widow's second form, beam (195 to 199) | 72 | -16 | 96 [88 to 96] |
| widow's second form, beam sweep (200 to 210) | 64 [48 to 72] | -16 [-56 to 24] | 88 [88 to 96] |
| (Q2PRO) soldier, ion ripper and hyperblaster (211 to 226, 254, 255) | 32 [8 to 40] | 8 [8 to 16] | 0 [-24 to 24] |
| (Q2PRO) guardian, blaster (227) | 88 | 48 | 64 |
| (Q2PRO) arachnid, railguns (228, 229) | 64 [56 to 64] | 0 [-24 to 16] | 24 [16 to 24] |
| (Q2PRO) arachnid, railguns up (230, 231) | 48 [40 to 56] | -8 [-24 to 16] | 72 |
| (Q2PRO) infantry running and extra (232 to 239, 260) | about 16 [-16 to 32] | 16 [0 to 24] | 16 [8 to 32] |
| (Q2PRO) gunner commander, chain gun (240, 241) | 24 | 8 | 24 |
| (Q2PRO) gunner commander, grenades (242 to 250) | 24 [8 to 32] | 8 [-32 to 40] | 0 to 24 by kind |
| (Q2PRO) gunner, grenades alternate (256 to 259) | 32 | -8 | 16 |
| (Q2PRO) supertank, grenades (261, 262) | 32 | -40 and 40 | 56 |
| (Q2PRO) icarus and daedalus, other side (263, 264) | about 0 | about 0 [-8 to 8] | 8 |
| (Q2PRO) medic commander, hyperblaster sweeps (265 to 288) | 32 [32 to 40] | 8 [0 to 16] | 16 |

---

## 9. Who sends the player's flash, and when

**How it is sent** **[read]** (base game, every case below): the 4-byte message of question 1 is written
and multicast to every client whose potentially visible set, seen from the firing player's origin,
includes that point, and whose area is connected to it (closed doors cut it off); it goes in the
unreliable part of the next packet to each such client (it may be dropped on overflow or loss). It is
not sent to everyone, and not reliably. A client that has not finished entering the game does not get
it. The multicast point is the player's origin, not the muzzle.

**When in the animation** **[read]**: a weapon fires when its view-weapon frame (the gun frame) is one
of its fire frames, at most once per server frame (100 ms). The weapon think normally runs once per
server frame; when the attack button is newly pressed it runs at once inside the player's command
instead, and then is not run again in that server frame. The flash is written in the same think that
spawns the shot, right after the projectile, bullets or rail are created.

| Weapon | Fire frame(s) and flash | Separate sound messages from the same shot |
| --- | --- | --- |
| blaster | frame 5: flash value 0 | none |
| shotgun | frame 8: flash 2 (frame 9 is a fire frame that only advances) | none |
| super shotgun | frame 7: flash 13 | none |
| machine gun | frames 4 and 5, alternating every server frame while the button is held: flash 1 each shot | none |
| chain gun | frames 5 to 21: flash 3, 4 or 5 every server frame (question 11) | spin-up `weapons/chngnu1a.wav` (auto channel, idle attenuation) on the first fire frame; a looping entity sound `weapons/chngnl1a.wav` while firing; spin-down `weapons/chngnd1a.wav` (auto channel, idle attenuation) at the end |
| hyperblaster | frames 6 to 11, looping back to 6 while held and with ammo: flash 14 each shot | a looping entity sound `weapons/hyprbl1a.wav` while firing; `weapons/hyprbd1a.wav` (auto channel) when it stops |
| rocket launcher | frame 5: flash 7 | none (the rocket carries its own flying sound) |
| grenade launcher | frame 6: flash 8 | none |
| railgun | frame 4: flash 6 | none |
| BFG | frame 9: flash 12 (the charge-up: light and `bfg__f1y.wav`); frame 17: the ball is launched with no flash | none |
| hand grenade | no flash at all | pin pull `weapons/hgrena1b.wav` (weapon channel), the held-timer loop `weapons/hgrenc1b.wav`, the throw `weapons/hgrent1a.wav` (weapon channel), all as ordinary sound messages |

- Beside these, any fire frame under quad damage also sends `items/damage3.wav` on the item channel
  (3), as an ordinary sound message **[read]**; that is a second sound but not a second gunshot.
- **[derived]** apart from the hand grenade and the loops and spin sounds above, the gunshot sound
  exists only inside the flash: a server that sends the flash must not also send the shot as a sound
  message, and a server that skips the flash leaves the shot silent on stock clients.

**Entering, leaving and respawn** **[read]** (base game):

| Event | Flash value | When | To whom |
| --- | --- | --- | --- |
| entering a deathmatch game | 9 | when the client begins (after it is put at its spawn point), unless an intermission is running | the PVS of the player's origin, as above |
| entering a cooperative or single-player game | 9 | the same moment, only when the server allows more than one client; with the console message "<name> entered the game" printed to all | as above |
| a spectator joining play | 9 | when the spectator becomes a player and is put in at a spawn point (not when a spectator enters) | as above |
| leaving | 10 | when the client disconnects, after "<name> disconnected" is printed to all, before the entity is removed | as above |
| respawn (11), item respawn (15) | never sent by the base game; a player's respawn and an item's return use entity events instead | | |

---

## 10. Who sends the monster's flash, and when

- **[read]** Every base-game monster weapon (bullet, shotgun, blaster, grenade, rocket, railgun, BFG)
  goes through one shared fire helper per kind, which first creates the shot and then writes the
  4-byte message of question 5 with the monster's entity number and the flash value the monster
  passed, multicast to the potentially visible set of the shot's start point (the muzzle, not the
  monster's origin), unreliable, exactly as for the player (question 9). So it is sent on the very
  animation frame on which the monster's fire function runs, once per shot; a monster that fires
  several times in a frame sends several flashes.
- **[read]** The monster passes the flash value that matches the gun and the animation frame (the
  multi-value monsters pick the value by their current frame), and the same value gives the start
  point of the shot.
- **Is the gun sound only in the flash?** **[read]** Yes for almost every monster: the base game only
  preloads the flash sounds so clients have them and plays no gunshot itself. The exceptions, all
  extra sounds sent as ordinary sound messages BEFORE the shot, not instead of it:
- gladiator: `gladiator/railgun.wav` on the weapon channel when it starts its railgun attack; its
  flash (61) has no sound, so this is the only rail sound **[derived]**.
- Makron: `makron/bfg_fire.wav` on the voice channel for its BFG (its flash 101 has no sound) and
  `makron/rail_up.wav` on the weapon channel before its railgun (its flash 119 has no sound, and in
  Yamagi no light either, question 7).
- Jorg: the voice `boss3/bs3atck1.wav` and the looping entity sound `boss3/w_loop.wav` when it starts
  its machine-gun attack; the left guns' flash carries `boss3/xfire.wav`, the right guns' is silent.
- iron maiden: `chick/chkatck1.wav` (voice) before the rocket and `chick/chkatck5.wav` (voice) to
  reload; the launch sound itself is in the flash.
- infantry and soldier: a weapon-cocking sound `infantry/infatck3.wav` before some attacks; gunner: a
  voice `gunner/gunatck1.wav` as it opens its gun.
- hornet: the right guns' flash is silent and the left guns' sound has attenuation none, so the pair
  is heard once, everywhere on the level **[derived]**.
- Mission-pack monsters (values 138 to 210) and the Q2PRO-only values: their senders are not in the
  code read, so when and how they are sent is **open**.

---

## 11. Hyperblaster, chain gun and machine gun

- **Machine gun** **[read]**: one flash (value 1) for every bullet, one bullet per server frame (10 a
  second) while the button is held and there is ammo.
- **Hyperblaster** **[read]**: one flash (value 14) for every bolt, one bolt per server frame on
  frames 6 to 11, looping while held and with ammo (10 a second).
- **Chain gun** **[read]**: one flash per server frame, whatever the number of bullets that frame; the
  value is 3 + (bullets − 1), so 3, 4 or 5 for one, two or three bullets, with the silenced bit added.
  The number of bullets depends on the gun frame the weapon reaches on that server frame (it advances
  the frame first, then counts): up to frame 9, one; frames 10 to 14, two while the button is held and
  one once it is released; frames 15 to 21 (the full-speed loop, which jumps back from 21 to 15 while
  held with ammo), three. The count is cut to the ammo left; with none left no flash is sent. Released
  on frame 14 the gun stops (jumps to its wind-down) without firing. **[derived]** A full burst
  therefore fires 1 bullet on its first four frames, 2 on the next five, then 3 on every frame after:
  flash values 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5 and so on.

---

## 12. Client settings that change flashes

**Yamagi** **[read]**:

| Setting | Default | Effect |
| --- | --- | --- |
| `cl_lights` | 1 | 0 drops every dynamic light from the frame, muzzle lights included; sounds, particles and models stay |
| `cl_particles` | 1 | 0 drops every particle, including the monster puff and the entering and leaving bursts |
| `cl_entities` | 1 | 0 drops the drawn entity list, which includes the smoke and flash models |
| `gl1_dynamic` | 1 | the classic OpenGL 1 renderer's switch for dynamic lights (0 turns them off there) |
| `gl1_flashblend` | 0 | the OpenGL 1 renderer's alternative drawing of dynamic lights as glowing blobs |

- There is no setting for the muzzle light's lifetime: the 32 ms floor is fixed **[read]**. There is no
  flash model to switch.

**Q2PRO** **[read]**:

| Setting | Default | Effect |
| --- | --- | --- |
| `cl_muzzlelight_time` | 16 | lifetime of a muzzle light in milliseconds, clamped to 0 to 1000; the heat beam, the nukes and the widow's beams ignore it (fixed 100 or 200 ms) |
| `cl_muzzleflashes` | 1 | 0 turns off the remastered flash models (on the view weapon and on monsters); it does not touch the lights |
| `cl_dlight_hacks` | 0 | a bit mask; the bit worth 4 removes the light of the machine gun and the chain gun flashes (the bits worth 1 and 2 change rocket and explosion lights) |

---

## 13. The view weapon's flash model

Q2PRO only (Yamagi draws no flash model). Units are world units, degrees and milliseconds.

**The table** **[read]**: model `models/weapons/<name>/flash/tris.md2`, offset ahead, right and up
along the view weapon's own axes, and a uniform scale.

| Value | Weapon | Model | Ahead | Right | Up | Scale |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | blaster | v_blast | 27.0 | 7.4 | -6.6 | 8 |
| 1 | machine gun | v_machn | 29.0 | 9.7 | -8.0 | 12 |
| 2 | shotgun | v_shotg | 26.5 | 8.6 | -9.5 | 12 |
| 3, 4, 5 | chain gun, 1, 2, 3 bullets | v_machn | 29.0 | 9.7 | -10.0 | 12, 16, 20 |
| 6 | railgun | v_rail | 20.0 | 5.2 | -7.0 | 12 |
| 7 | rocket launcher | v_rocket | 20.8 | 5.0 | -11.0 | 10 |
| 8 | grenade launcher | v_launch | 18.0 | 6.0 | -6.5 | 9 |
| 13 | super shotgun | v_shotg2 | 25.0 | 7.0 | -5.5 | 12 |
| 14 | hyperblaster | v_blast | 23.5 | 6.0 | -6.0 | 9 |
| 16 | ion ripper | v_boomer | 24.0 | 3.8 | -5.5 | 15 |
| 19 | BFG, second flash | v_bfg | 18.0 | 8.0 | -7.5 | 16 |
| 20 | phalanx, second flash | v_rocket | 18.0 | 10.0 | -6.0 | 9 |
| 30 | ETF rifle | v_etf_rifle | 24.0 | 5.25 | -5.5 | 4 |
| 31 | prox launcher | v_launch | 18.0 | 6.0 | -6.5 | 9 |
| 32 | ETF rifle second barrel (remastered protocol only) | v_etf_rifle | 24.0 | 4.0 | -5.5 | 4 |
| 33 | heat beam | v_beamer | 18.0 | 6.0 | -8.5 | 16 |
| 35 | disruptor | v_dist | 18.0 | 6.0 | -6.5 | 10 |

No model **[read]**: 9 to 12 (login, logout, respawn, BFG first flash), 15 (item respawn), 17 (blue
hyperblaster), 18 (phalanx first flash), 32 on the original protocol, 34 (blaster 2), 36 to 39 (nukes),
and any value with no entry or whose model file is missing. The silenced bit changes nothing here.

**Placement** **[read]**:

- Start from the view weapon as drawn: view origin + the server's interpolated gun offset +
  `cl_gun_y`/`cl_gun_x`/`cl_gun_z` (ahead, right, up along the un-kicked view axes, default 0).
- Lay the three offset numbers along forward, right and up of the weapon's drawn angles: the view
  angles (kick included when `cl_kickangles` is on) plus the server's interpolated gun angles. So the
  flash follows bob, sway, kick and the `cl_gun_*` settings in position.
- Orientation: the view angles (kick included), NOT the gun angles, plus an extra roll: a random whole
  0 to 359 for v_machn (machine gun and all chain gun values) and v_beamer (heat beam), 0 otherwise,
  picked once per flash.
- The offset is always right handed; left hand (`hand` 1 with `cl_gun` 1, or `cl_gun` 3) comes from the
  renderer mirroring the projection of every view-weapon draw, the flash included **[derived]**.

**Size** **[read]**: the scale multiplies the model's own coordinates; nothing normalises the model.
The remastered models' own extent is not in the release **[open]**.

**Look** **[read]**: frame 0, skin 0, full bright, translucent at opacity 1 with ordinary alpha
blending (transparency from the skin's alpha **[derived]**), no depth write, the view weapon's depth
range (0 to 0.25), never culled, the weapon's field of view (`cl_gunfov`), no shadow, glow map and bloom
when present, no fade. `cl_gunalpha`, the player's transparency and powerup shells do not reach it.

**Timing** **[read]**: start time = latest server frame time minus one server frame time (100 ms at
10 Hz); shown while client time minus start is at most 50 ms. One slot: a new own flash replaces it and
restarts the clock; the later of two in one packet wins. One model per message, so the chain gun draws
one per server frame. **[derived]** That is the first 50 ms of the interpolation interval: half of it
at 10 Hz.

**Hidden by** **[read]**: `cl_muzzleflashes` 0; `cl_gun` 0 or less; `hand` 2 with `cl_gun` 1; no gun
model; third person; a compatible-format demo with field of view over 90. Only the frame's own client
(client number + 1) draws it: in a demo that is the recorded player; a spectator chasing someone sees
the followed player's flashes only when the server sends the followed player's number with each frame
**[read, derived]**. Seeking a demo skips flashes **[read]**.
