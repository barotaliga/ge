# Quake II monsters: round 2 facts (liquids, use, triggered spawn, grenade pickup, noise, soldier spread)

Clean-room reader's answers, kept for the single player 1-1 work (q2-sp-gaps.md). No code.


Source read: the stock game library (Quake-2/game). Facts in plain words; [read] = seen
directly, [derived] = worked out from what was read. Times in seconds; the game frame is 0.1 s.

## 1. Monsters in liquids

Water level (0..3), found every monster think (every 0.1 s), after the frame's move:
- Probe point: the monster's origin x, y; z = origin z + the box's bottom (mins z) + 1. If the
  contents there are not water, lava or slime: level 0 and liquid kind cleared; stop. [read]
- Otherwise the liquid kind is the contents at that first point, and level = 1. [read]
- Raise the point by 26 (so 27 above the box bottom): liquid there -> level 2, else stop. [read]
- Raise it by 22 more (49 above the box bottom): liquid there -> level 3. [read]
- The liquid kind is taken only from the lowest point. [read]
- The same test also runs once when the monster is dropped to the floor at spawn. [read]

Air and drowning (only while health > 0; a dead monster skips this block entirely): [read]
- Non-swimmers: while level < 3, the air deadline is reset to now + 12, so a monster has 12 s of
  air once its head goes under. [read]
- Swimmers (the swim flag) are exempt from drowning; instead they suffocate OUT of liquid: while
  level > 0 the deadline is now + 9, so 9 s out of the water before damage. [read]
- Fliers have no exemption: the fly flag is not checked, so a flier with level 3 drowns like a
  walker. [derived]
- Once past the deadline, damage is dealt when the monster's pain debounce time has passed, and
  the pain debounce is then set to now + 1, so at most once a second. This is the SAME field the
  monsters' pain reactions use (most set it to now + 3 on pain), so a recent pain can delay a
  drowning hit. [read] / sharing consequence [derived]
- Amount: 2 + 2 x (whole seconds since the deadline, floored), capped at 15. So 2, 4, 6, ...,
  14, then 15 from the 7th second on (with the 1 s spacing: 2, 4, 6, 8, 10, 12, 14, 15, 15...).
  [read] / sequence [derived]
- Drowning/suffocation damage ignores armor (the no-armor damage flag), zero direction, zero
  knockback, means of death "water"; inflictor and attacker are both the world. [read]
- Triggered-spawn monsters get the 12 s deadline set when they appear; a monster that has never
  been in liquid has its deadline refreshed every think anyway. [read]
- No sound is played for drowning or for its damage beyond whatever the monster's own pain
  reaction plays. [read] (pain reaction [derived])

Lava and slime (independent of health: a corpse still thinking still takes it): [read]
- Lava: if the liquid kind includes lava and the monster lacks a lava-immunity flag: every
  0.2 s, 10 x water level damage (10, 20 or 30). [read]
- Slime: same with slime, every 1 s, 4 x water level (4, 8 or 12). [read]
- Both use one shared damage debounce field (not the pain debounce). [read]
- Damage flags 0 (armor and power armor apply), zero direction, zero knockback; means of death
  "lava" or "slime"; inflictor and attacker are the world. [read]
- The lava and slime immunity flags exist but no stock monster sets them; swim and fly flags give
  no exemption. So every stock monster takes lava and slime damage. [derived from a search]
- Entering any liquid (in-water flag not yet set): the in-water flag is set and the damage
  debounce is reset to 0, so the first lava/slime hit lands on the very next check. [read]
- Entry sounds (only when not a dead monster): lava -> player/lava1.wav or player/lava2.wav,
  50/50; slime -> player/watr_in.wav; water -> player/watr_in.wav. Channel body, volume 1,
  normal attenuation. [read]
- Leaving (level 0 while the in-water flag is set): player/watr_out.wav, played even for a dead
  monster, and the flag cleared. Lava/slime checks only run while level > 0. [read]
- A dying monster's death-target use clears its fly and swim flags. [read]

## 2. Using a monster with no enemy (the standard monster use)

- Does nothing if the monster already has an enemy. [read]
- Does nothing if the monster's health is 0 or less (dead). [read]
- Does nothing if the activator has the no-target flag. [read]
- Does nothing unless the activator is a player (client) OR a monster marked as a good guy. [read]
- The activator's own health is NOT checked (a dead player activator would be accepted). [read]
- Otherwise: enemy = activator, then the found-target path runs directly (no sight test):
  it marks this monster as the level's sight entity for other monsters when the enemy is a
  player (with light level 128), sets show-hostile to now + 1, records the enemy's position as
  last sighting, then either hunts the enemy (no combat point) or runs to its combat point.
  [read]
- The sight sound is NOT played on this path (that is played only by the find-target routine).
  [read]

## 3. Triggered spawn

- Use while hidden: the monster appears after a one-frame delay (0.1 s). If the activator is a
  player (client), it is stored as the enemy; any other activator is not stored. The use is then
  replaced by the standard monster use of answer 2. [read]
- On appearing: raised by 1 unit, anything in its box is telefragged, it becomes solid with step
  movement and visible, air deadline now + 12, linked, then the normal start-go runs. [read]
- Start-go (same as any monster's): if dead, stop. Resolves a target to a combat point (target
  becomes the combat target); if a remaining target is a path corner it faces it and walks;
  any other target or a missing one -> stand with a very long pause; no target -> stand with a
  very long pause. Then normal thinking starts next frame. [read]
- After start-go: if there is a stored enemy AND the ambush flag (spawnflag 1) is NOT set AND
  the enemy lacks no-target -> found-target path (as in 2). Otherwise the enemy is cleared. So
  with ambush it only goes to its path or stands; no sound, no extra action. [read]
- No sight or idle sound is played on appearing in either case. [read]

## 4. Grenades picked up in single player

- Grenades are one item that is both ammo and weapon, picked up by the ammo pickup. [read]
- Switch rule: only when the player held 0 grenades before this pickup (and the ammo was added,
  i.e. not already at the maximum), and the current weapon is not already grenades. In single
  player (deathmatch off) that is enough: it switches from ANY weapon, not only the blaster. In
  deathmatch it additionally requires the current weapon to be the blaster. [read]
- Ammo added by a grenade launcher pickup goes through the plain ammo add, which never
  switches; afterwards the grenade count is not 0, so a later grenade box does not switch.
  [derived]
- Amount: the item's count field if set, else 5. [read]
- For comparison, weapon pickups switch when the weapon count becomes exactly 1, same
  single-player/deathmatch rule. [read]

## 5. Noise

General (one routine): nothing at all in deathmatch; nothing if the noise maker has the
no-target flag; a weapon noise is swallowed while silencer shots remain (one shot consumed).
Each player owns two noise markers (16-unit boxes). "Self" and "weapon" noises move the first
marker and make it the level's current sound entity; "impact" noises move the second marker and
make it the level's second sound entity. Each is stamped with the frame number. [read]

- Jump: when the player was on the ground last frame, is not after the move, the jump input
  (up move) is at least 10, and the move's water level is 0. Plays the *jump1.wav voice and
  makes a self noise at the player's origin. Never while in liquid. [read]
- Pain: the pain voice (*painNN_N.wav, at most every 0.7 s, not in god mode or invulnerable)
  makes NO noise for monsters. [read]
- Other self noises: entering or fully leaving liquid, a gasp on surfacing after the air ran out,
  and every 25 frames of breather/enviro-suit breathing under water. [read]
- Player weapon firing makes a weapon noise at the muzzle start point for the blaster,
  hyperblaster, shotgun, super shotgun, machinegun, chaingun, grenade launcher, rocket launcher,
  railgun and BFG (the BFG also makes one at the player's origin when its charge-up flash
  starts). Throwing a hand grenade makes NO weapon noise; only its explosion's impact noise.
  [read]
- Impact noises, all owned by the shooter, only when the shooter is a player (monsters' shots
  make none): [read]
  - Bullets and shotgun pellets: at the impact point, only when hitting a surface that takes
    no damage, and not sky. Hitting a damageable entity makes none. [read]
  - Blaster and hyperblaster bolts: at the bolt's position on touch, whatever it hits (not sky,
    not its owner). [read]
  - Rockets: at the rocket's position on touch (not sky). [read]
  - Grenades (launcher and hand): at the grenade's position when it explodes. [read]
  - Railgun: at the rail trace's end point, always. [read]
  - BFG ball: at the ball's position on touch (not sky). [read]
- How a monster takes them (each think while looking for a target), checked in this order, the
  first that applies wins: [read]
  1. A monster that got angry this frame or last (the sight entity), unless this monster has
     the ambush flag;
  2. the player's self/weapon noise of this frame or last;
  3. the impact noise of this frame or last, only if this monster has no enemy and lacks the
     ambush flag;
  4. otherwise the sight client (actual sight).
- So ambush monsters wake on a player's own noises but never on impact noises or other
  monsters' alerts. [read]
- A heard noise: ambush monsters must also see the marker; others need it in the potentially
  hearable set. It must be within 1000 units and in a connected area. The monster turns toward
  it and hunts it as a sound target; the sight sound is not played for a heard noise. [read]
- A noise whose owner has no-target is ignored. [read]

## 6. Soldier aim spread

- Every soldier kind (light/blaster, shotgun, machinegun "SS"), on every aimed shot: aim at the
  enemy's origin raised by its view height, then pick a point 8192 units along that line and
  offset it by a random amount right of 1000 x crandom and up of 500 x crandom, where crandom is
  uniform in about [-1, 1] (2 x (random - 0.5), random being a 15-bit integer / 32767). The aim
  is the normalized direction to that point. [read]
- Angular size: up to about 6.96 degrees sideways and 3.49 degrees vertically. [derived]
- Shots fired from the death animation (two frames of the first death) skip the aim and fire
  straight along the facing. [read]
- Light soldier (skins 0-1): one blaster bolt, damage 5, speed 600; no extra spread. [read]
- Shotgun soldier (skins 2-3): 12 pellets, damage 2 each, kick 1; each pellet adds its own
  spread around the already-spread aim: right 1000 x crandom, up 500 x crandom at 8192. [read]
- SS / machinegun soldier (skins 4-5): one bullet per fire frame, damage 2, kick 4; extra spread
  right 300 x crandom, up 500 x crandom at 8192. It holds the firing frame for a burst of
  3 to 10 frames (3 + random 0..7, x 0.1 s). [read]
- The two spreads add as offsets at 8192 in each stage's own right/up frame. [derived]
