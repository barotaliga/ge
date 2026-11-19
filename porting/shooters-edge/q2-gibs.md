# Quake II's gibs (M18v3e)

Clean-room answers, written by a reader and screened (`clean-room` skill). Built on in
`games/shooters_edge/server/q2_gibs.hpp`.

Source read: id Software's Quake 2 source release (GPL v2), the id-Software/Quake-2 repository on
GitHub, master branch; the base game's server module, the shared player move and the client.
Every fact is marked [read] (stated directly by the source), [derived] (follows from two or more
read facts taken together) or [open] (the source does not settle it). Nothing was measured by
running the game.

Common units: distances in map units, speeds in units per second, spins in degrees per second,
the server's frame is 0.1 s (10 Hz) [read]. "Uniform in [a, b]" means the game's own random
number in [0, 1] (15 bits of the C library's generator divided by 32767) scaled to that range,
"uniform in [-a, a]" is twice that random minus one, times a [read].

## 1. A player gibbed in deathmatch

- Threshold: a player is gibbed when, at death or at any later hit on the still-damageable
  corpse, health is strictly below -40 [read]. Health is clamped to no lower than -999 before the
  death handler runs [read]. Monsters use "at or below their own gib health" instead (see 9).
- Order on the killing hit [read]: the spin is zeroed, the move type becomes toss, the linked
  weapon model (the second model slot) is cleared, pitch and roll of the body are zeroed, looping
  and weapon sounds are cleared, the box top is set to -8 and the player is flagged as a dead
  body for traces. If the player was not already dead: the respawn timer is set to now + 1.0 s,
  the view turns toward the killer, the obituary is printed, the weapon is dropped, the
  scoreboard is shown in deathmatch, and the inventory is emptied (keys kept for coop respawn).
  Powerups are removed. Then, because health is below -40: the sound `misc/udeath.wav` plays on
  the body channel at full volume with normal attenuation, four gibs of
  `models/objects/gibs/sm_meat/tris.md2` are thrown (organic kind), the player's own entity is
  turned into the head (below), and the entity is made undamageable. No death animation and no
  `*death` sound play on a gib death [read].
- Gib models for a player: four `sm_meat` and the head only. No bone, no chest, no arm or leg
  [read].
- The player's entity becomes the head [read]: with even chance it is
  `models/objects/gibs/head2/tris.md2` with skin 1 (the player's skin of that model) or
  `models/objects/gibs/skull/tris.md2` with skin 0. Its origin is raised 32 units, frame 0, box
  (-16, -16, 0) to (16, 16, 16), not solid, effects set to exactly the gib-trail bit, looping
  sound off, immune to knockback, nominal move type bounce. Its velocity is the body's velocity
  plus the damage velocity of question 3 at full scale (not halved), and it is NOT clamped
  [read]. It gets no spin [read].
- While the player is still connected the head is the client entity, so it is moved by the
  shared player move, not by the server's toss physics [read]: the player-move type becomes the
  gib type whenever the entity's model is no longer the player model; that type uses the box
  (-16, -16, 0) to (16, 16, 16) and a view height of 8 [read]; all movement input is zeroed
  [read]. So it slides and stops like a player body with no input under normal gravity; it does
  not bounce [derived]. The extra "dead body" ground friction (speed reduced by 20 units/s each
  move) applies only to the unbroken-corpse type, not to the gib type [read].
- The view while dead: roll 40, pitch -15, yaw toward the killer [read]. The head entity's own
  angles keep following the frozen view angle: pitch is a third of the view pitch, yaw the view
  yaw, roll from the sideways speed [read, derived that the view angle is frozen at death].
- Respawn: no sooner than 1.0 s after death; in deathmatch only the attack button respawns (or
  forced respawn by the server flag), otherwise any button [read]. On respawn in deathmatch or
  coop the entity's state (head model, box, move type) is copied into the next slot of an
  8-slot body queue and the player is rebuilt at a spawn point [read]. The copy has no thinking
  and so lasts until its queue slot is reused by the eighth later respawn [derived]; there it is
  run by the server's toss physics with the bounce move type (see 5) [derived].
- Gibbing a corpse that is already in the body queue: same 4 `sm_meat` gibs and
  `misc/udeath.wav`, the origin is first lowered 48 units and then the head is made as above
  (so it ends 16 units below the corpse's origin), its think is cleared, and it becomes
  undamageable [read].

## 2. A gib's starting place

- Each thrown piece is placed independently, uniformly at random inside the body's linked
  bounding box: per axis, the box centre plus a uniform offset in [-half size, +half size]
  [read]. The centre is taken from the server's absolute box, which is padded 1 unit on every
  side, while the half size is from the unpadded box, so the centre sits 1 unit below the true
  centre on each axis (x, y and z all shifted by -1) [derived].
- The box used is the one last linked. For a player killed outright that is still the living
  box (standing -24 to 32 in z, crouched lower), because the lowered dead box is linked only at
  the end of the death handler [derived].
- The head is not a new piece: a monster's head stays at the monster's origin [read]; a
  player's head is raised 32 units (see 1).
- A gib's own box is a point (zero size): the model is not a brush model, so setting it does not
  give it a box [derived]. A monster head's box is also cleared to a point [read].

## 3. Starting velocity

- The attacker's direction is NOT used [read]. Only the damage amount (the last hit's damage)
  and randomness are.
- The damage velocity [read]: x and y each uniform in [-100, 100]; z uniform in [200, 300]. The
  whole vector is then scaled by 0.7 if the damage is below 50, otherwise by 1.2.
- A gib [read]: velocity = the body's current velocity + kind scale times the damage velocity,
  kind scale 0.5 for organic, 1.0 for metallic. Then clamped: x and y to [-300, 300]; z to
  [200, 500] (always at least 200 upward).
- So an organic gib from a hit under 50 has a random part of x, y in [-35, 35] and z in [70,
  105], and the clamp lifts z to 200 unless the body itself was rising [derived].
- A monster head [read]: the same formula and the same clamp as a gib, applied to the monster's
  own velocity, scale 0.5 organic or 1.0 metallic.
- A player head [read]: body velocity + the damage velocity at scale 1.0, with no clamp.
- Afterwards, each frame of the server's toss physics bounds every velocity component to
  plus or minus the server's maximum velocity setting, 2000 by default [read].

## 4. Spin

- A gib: each of pitch, yaw and roll spin uniform in [0, 600] degrees per second, independently
  and only positive [read].
- A monster head: yaw spin uniform in [-600, 600] degrees per second; pitch and roll spin are
  left as the body had them [read].
- A player head: zero spin (cleared at death, none added) [read].
- Spin is applied as angle += spin x 0.1 s each server frame [read].
- The map-placed scenery gibs (arm, leg, head; see 9): each axis uniform in [0, 200] [read].

## 5. Movement

- Organic gib and organic monster head: the toss move type [read]. Metallic: the bounce move
  type [read].
- Each server frame, when not on ground [read]: gravity is applied first (velocity z decreases
  by gravity x 800 x 0.1 = 80 units/s per frame with the default world gravity of 800 and the
  entity's own gravity factor of 1.0), then the angles turn by the spin, then the piece moves by
  velocity x 0.1 s.
- On hitting something [read]: the velocity is clipped against the surface normal with an
  overbounce of 1 for toss (the into-surface part removed: it slides) or 1.5 for bounce (half the
  into-surface part reflected back). Any component that ends within 0.1 of zero becomes zero. If
  the surface is floor-like (normal z above 0.7): a toss piece stops at once; a bounce piece
  stops if its upward speed after the bounce is below 60. Stopping zeroes both velocity and spin
  and records what it lies on; the angles stay as they were at that moment [read].
- On the ground it does nothing until upward speed appears, the thing under it is removed, or
  that thing moves (then it is released and falls again) [read]. A mover carries pieces resting
  on it [read].
- What it collides with: only solid world and window brushes and solid brush models (doors,
  lifts). It passes through players, monsters and corpses, whose boxes count as monster
  contents [derived].
- Water: entering or leaving water plays `misc/h2ohit1.wav` (at full volume, attenuation 1) at
  the old or new position; there is no water drag on these pieces [read].
- The player head while connected moves by the player move (see 1), not by this.

## 6. Life, solidity, damage

- A gib is removed after a uniform 10 to 20 s [read]. A monster head is removed (the monster
  entity freed) after a uniform 10 to 20 s [read]. Removal is instant: no fade [read].
- A player's head lasts until respawn, then its body-queue copy lasts until the slot is reused
  (see 1) [derived].
- Solidity: every gib and head is not solid [read]. Players and monsters pass through them
  [derived].
- Can a gib be shot or gibbed again? It is marked as damageable and its death removes it at
  once [read], but because it is not solid no trace hits it and the radius-damage search skips
  non-solid entities, so in practice nothing damages it [derived]. The player head is marked
  undamageable [read]. Gibs are immune to knockback [read].
- A dead player's unbroken corpse stays damageable and can be gibbed later (see 1) [read]. A
  corpse in the body queue takes its damage against the queue slot's own health, which is not
  copied from the player: it is 0 on the slot's first use and whatever a previous corpse left
  after that [derived].

## 7. Frames

- Every gib and every head is shown at frame 0 [read for heads; derived for gibs, a new entity
  starts at frame 0].
- The source contains a landing behaviour for organic pieces: on landing on a surface, play
  `misc/fhit3.wav`, turn the piece to lie along the surface, and, for `sm_meat` only, step its
  frame by one every 0.1 s until frame 10, then remove it after a further uniform 8 to 18 s
  [read]. That behaviour never runs in this release [derived]: it is a touch reaction, and the
  server calls touch reactions only for solid entities, while gibs are not solid; moreover it
  requires the piece to be on ground already, which is only set after the touch reaction of the
  landing move. So `sm_meat` is shown at frame 0 throughout and the server never cycles it
  [derived].
- How many frames `sm_meat` or other gib models contain is a property of the model files, not
  in the source [open].

## 8. Effect and render bits

- Every gib carries the gib-trail effect bit (value 0x00000002) [read]. A monster head adds that
  bit to its effects and removes the flies bit (value 0x00004000) [read]. The scenery gibs carry
  it too [read]. No render-effect bits are set on gibs [read].
- A player head is given exactly the gib-trail bit, but a connected player's effects and render
  bits are reset to zero every server frame (and stay zero while health is 0 or below) at the
  end of the frame, before the frame is sent, so a connected player's head shows no trail
  [derived]. Its body-queue copy inherits the cleared effects, so it shows none either
  [derived]. Only a head made by gibbing a body-queue corpse keeps the bit [derived].
- The client's trail for that bit [read]: it is drawn only along the distance the entity moved
  between frames, a candidate every 0.5 units. A per-entity counter starts at 1024 when the
  entity comes into view; each candidate is emitted with probability counter/1024 and the
  counter then drops by 5, never below 100 (so after about 92 units of travel about 1 in 10
  candidates is emitted) [read; derived the distance]. Position jitter per axis uniform in plus or
  minus 4, 2 or 1 units, velocity jitter plus or minus 15, 10 or 5 units/s, by counter above 900,
  above 800, or lower [read]. Each particle starts with an extra 40 units/s downward and has no
  acceleration [read]; it starts opaque and fades linearly to nothing in 1.0 to 1.4 s [read]; its
  colour is one of the 8 palette entries 232 to 239, chosen at random [read]. The client draws
  this trail only when no higher-priority trail bit (rocket, blaster, hyperblaster) is also set
  [read]. A green variant exists in the client but the base game's server never sets it [read].

## 9. Monster versus player; the dead soldier

- Same damage velocity for both [read]. Differences [read]: a monster head is halved (organic)
  and clamped, a player head is full scale and unclamped; a monster head spins in yaw, a player
  head does not; players throw 4 `sm_meat` and no bones; most organic monsters throw 2
  `models/objects/gibs/bone/tris.md2`, 4 `sm_meat` and the `head2` head.
- Monster exceptions in the base game [read]: the soldier throws 3 `sm_meat`, 1
  `models/objects/gibs/chest/tris.md2` and the `head2` head (gib health -30); the flipper and
  the hover throw 2 bone, 2 `sm_meat` and use a further `sm_meat` as the head; the tank and the
  second stage of the final boss throw 1 `sm_meat`, 4 `models/objects/gibs/sm_metal/tris.md2`
  (metallic), and the tank a chest, with `models/objects/gibs/gear/tris.md2` as a metallic head,
  the supertank's explosion throws 4 `sm_meat`, 8 `sm_metal`, a chest and the gear head, all at
  a fixed damage of 500 (so the 1.2 scale). Gib health per monster ranges from -30 (soldier,
  flipper) through -40 (infantry), -50, -60, -70, -100, -120, -130, -150, -175, -200 to -500
  and -2000 [read].
- The dead soldier scenery (the dead player model `models/deadbods/dude/tris.md2`, box (-16,
  -16, 0) to (16, 16, 16), six poses by spawn flag, solid, damageable) [read]: removed at spawn
  in deathmatch. In single player and coop, every damage that leaves it at 0 or below runs its
  death reaction; nothing happens until health is at or below -80; then `misc/udeath.wav`, 4
  `sm_meat` (organic) and the entity itself becomes a `head2` head through the monster head path
  (skin 0, halved and clamped velocity, yaw spin, 10 to 20 s life) [read]. Its health starts at
  0 unless the map gives it one, so 80 cumulative damage gibs it [derived].
- The scenery gibs `misc_gib_arm`, `misc_gib_leg` and `misc_gib_head` (models
  `models/objects/gibs/arm/tris.md2`, `leg/tris.md2`, `head/tris.md2`; meant to be spawned by a
  spawner): not solid, toss, gib-trail bit, spin [0, 200] per axis, removed after a fixed 30 s,
  no initial velocity of their own [read].

## 10. Skin, colour, landing sound

- A gib is a new entity and shows skin 0 of its model; it does not take the body's skin or
  colour [derived]. A monster head is set to skin 0 [read]. A player head is `head2` skin 1 or
  `skull` skin 0 (see 1) [read]. No colour tint is applied [read].
- Sounds [read]: `misc/udeath.wav` when the body is gibbed; `misc/h2ohit1.wav` on entering or
  leaving water. A landing sound `misc/fhit3.wav` exists in the source but never plays (see 7)
  [derived]. Bounce-type (metallic) pieces have no impact sound [read].

## 11. How many may exist

- There is no cap on gibs and no removal of the oldest gib [read]. They share the general
  entity pool: 1024 entities by default (the latched server setting for the maximum entity
  count) [read]. When the pool is full, spawning any entity is a fatal server error ("no free
  edicts") [read]. A freed slot is reused only after 0.5 s, except in the first 2 s of a level
  [read].
- Player corpses and heads have their own ring of 8 body-queue slots; the oldest is overwritten
  [read].
