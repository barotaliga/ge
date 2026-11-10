# The sequel's rules, specified

What the sequel's game library does in a match and on its single-player maps, as rules and
numbers in our words (README.md, the rules). For M18t4: the sequel's rules as our C++ runner.
**Where these facts came from**: the research session of `sequel.md`, which read the sequel's
GPL game library. A summary of behaviour, not a transcription: the builder designs the code
(entities as data, the runner's table of calls) and holds it to these numbers in the `se-sequel`
lab. Exact parity is the goal for what a player feels in a match (damage, armour, timings, the
move); the visual extras (the view's bob, kicks, blends) need only match closely.

**Randomness**: the original draws from the C library's `rand()`, which differs by platform. Ours
draws from `core::Rng`, seeded by the match, so a run reproduces everywhere; the spreads and
chances below are what a draw must produce, not its bits. `random()` below is a draw in 0..1 and
`crandom()` one in −1..1.

## Time and the frame

- **A server frame is 100 ms**. The level's time is the frame number × 0.1 (the frame number
  counts from 1 after a map starts). Every "think at time t" is checked to 1 ms (`t ≤ level time +
  0.001` runs it).
- **The frame, in order**:
  1. the frame number and time advance;
  2. the monsters' **sight client** moves to the next living, targetable player after the last
     one (wrapping; none when no player qualifies) (`sequel-monsters.md`);
  3. when the intermission's exit is due, the level changes and the frame ends (§ Levels);
  4. **every entity in index order**: its old origin is kept; a ground entity whose link count
     changed (it moved) is dropped (a walking monster re-checks its ground); the players' slots
     (1 to max clients) run their **frame start** (§ A player's frame), every other entity **runs**
     (§ Running an entity);
  5. the deathmatch rules (§ Deathmatch);
  6. every player's **frame end** (§ A player's frame).
- **Commands arrive between frames** and each runs at once: the player's move (`sequel-movement.md`
  § The game's half) and, when the attack button is newly pressed, the weapon's step (§ Weapons).

## Entities

An entity has the engine's shared head (the network state, the bounds, the solid kind, the clip
mask, the owner, the server flags: not sent, corpse, monster) and the game's fields. What the
rules read and write:

- **Solid kinds**: not solid; trigger (touched when overlapped, after a move); box (touched at
  its edge, blocks); brush (its map model clips, blocks).
- **Move types**: none (only thinks), no-clip, push (a mover: no clip to the world, pushes boxes),
  stop (a mover that stops when blocked), walk (players: the move does it), step (walking
  monsters), fly, toss (gravity, stops), fly-missile (no gravity), bounce (gravity, bounces).
- **Game fields** used below: health, max health, gib health, mass (players 200), take-damage (no,
  yes, aim), dead flag, the think time and function, touch, use, pain, die, blocked, the
  targeting keys (`target`, `targetname`, `killtarget`, `message`, `delay`, `team`), the ground
  entity and its link count, the water level and type, gravity (a factor, default 1), the flags
  (fly, swim, god mode, no target, immune to slime, lava, laser; no knockback; power armour on;
  respawning item; team slave).
- **Using targets** (any trigger, button, item or death): after `delay` seconds (a helper entity
  carries it), print the `message` to the activator (a player, unless the entity is a monster),
  remove every entity named by `killtarget`, then call `use` on every entity named by `target` in
  index order.
- **Teams** of movers (the same `team` key): the first found is the master; the others are
  slaves that move with it.

## Running an entity

By move type, after its optional pre-think:

- **Push and stop** (a team, from its master): each member with a velocity or turning speed pushes
  by velocity × 0.1 and turning × 0.1, **the move rounded to eighths** (each axis × 8 rounded
  half away from zero, × 0.125). The boxes in its way (or riding it) move with it: a box is
  pushed by the move and the rotation about the pusher's origin, and a rider's view turns with the
  yaw. A box that would end in a solid is tried back at its old place; if still stuck, the whole
  push is undone, every member's think is delayed 0.1, and the member's `blocked` runs on the
  obstacle (a door's crushes or reverses: § The map's movers). A push that completes runs the
  team's thinks and touches the triggers of every moved entity.
- **None**: think. **No-clip**: think, then turn and move by velocity × 0.1, no clip.
- **Step** (walking monsters, and things at rest that fall when their floor goes): gravity when
  not on the ground (not for fliers, nor swimmers in deep water), a fly or swim damping of the
  vertical speed (friction 6 / 3 for fliers, the water friction for swimmers), ground friction
  (6, stop speed 100) on the horizontal speed when on the ground, then a slide over 0.1 s with the
  monster's mask (or the solid mask), triggers touched, a landing sound when it landed falling
  faster than 80 units/s, then think.
- **Toss, bounce, fly, fly-missile**: think first; a team slave stops here; an upward speed drops
  the ground entity; on the ground (and the ground still in use), nothing more. Else the velocity
  is held to ±`sv_maxvelocity` (2000) on each axis, gravity (`gravity × sv_gravity × 0.1`) unless
  flying, turned by its turning speed × 0.1, and pushed by velocity × 0.1 (one trace with its clip
  mask, or the solid mask; touches both ways on a hit; triggers touched). On a hit the velocity
  is clipped with overbounce 1.5 (bounce) or 1 (the rest); on a floor (normal z above 0.7) it
  comes to rest (ground set, velocity and turning zero) unless it bounces faster than 60 up.
  Its water level becomes 1 or 0 by the point contents at its origin (a splash sound on entering
  and leaving), and its team slaves are moved to its origin.

Clipping a velocity here also reports "floor" for a normal z above 0 and "step" for a normal z of
exactly 0, which a monster's move reads.

## Items

Every pickable thing on a map is an item, spawned from its class name 0.2 s after the map
starts (so the movers are in place) and dropped to the floor: its box is (−15, −15, −15) to (15,
15, 15), it falls up to 128 units, and **an item that starts in a solid is removed**. Spawn flag 1
hides it until used (a trigger spawns it), spawn flag 2 makes it solid and untouchable (it is
then shown still, without its spin or glow). The spawn flags of an item other than a power cube
are otherwise cleared. Items spin (the rotate effect) and glow.

| Class name | Pickup name | Gives | Respawn (deathmatch) |
| --- | --- | --- | --- |
| `item_armor_body` | Body Armor | armour (below) | 20 s |
| `item_armor_combat` | Combat Armor | armour | 20 s |
| `item_armor_jacket` | Jacket Armor | armour | 20 s |
| `item_armor_shard` | Armor Shard | +2 to the worn armour, or 2 jacket | 20 s |
| `item_power_screen` | Power Screen | power armour (below), switched on at once in deathmatch when first owned | 60 s |
| `item_power_shield` | Power Shield | power armour, the same | 60 s |
| `weapon_shotgun` | Shotgun | the weapon and 10 shells | 30 s |
| `weapon_supershotgun` | Super Shotgun | the weapon and 10 shells | 30 s |
| `weapon_machinegun` | Machinegun | the weapon and 50 bullets | 30 s |
| `weapon_chaingun` | Chaingun | the weapon and 50 bullets | 30 s |
| `weapon_grenadelauncher` | Grenade Launcher | the weapon and 5 grenades | 30 s |
| `weapon_rocketlauncher` | Rocket Launcher | the weapon and 5 rockets | 30 s |
| `weapon_hyperblaster` | HyperBlaster | the weapon and 50 cells | 30 s |
| `weapon_railgun` | Railgun | the weapon and 10 slugs | 30 s |
| `weapon_bfg` | BFG10K | the weapon and 50 cells | 30 s |
| `ammo_shells`, `ammo_bullets`, `ammo_cells` | Shells, Bullets, Cells | 10, 50, 50 | 30 s |
| `ammo_rockets`, `ammo_slugs`, `ammo_grenades` | Rockets, Slugs, Grenades | 5, 10, 5 (grenades are also the hand grenade weapon) | 30 s |
| `item_health_small` | Health | +2, past the maximum | 30 s |
| `item_health` | Health | +10, up to the maximum | 30 s |
| `item_health_large` | Health | +25, up to the maximum | 30 s |
| `item_health_mega` | Health | +100 past the maximum; after 5 s it counts down 1 a second while above the maximum, then respawns 20 s later | (above) |
| `item_quad` | Quad Damage | 30 s of ×4 damage and knockback | 60 s |
| `item_invulnerability` | Invulnerability | 30 s of no damage | 300 s |
| `item_silencer` | Silencer | 30 silent shots | 60 s |
| `item_breather` | Rebreather | 30 s of air | 60 s |
| `item_enviro` | Environment Suit | 30 s of air and a third of lava, no slime | 60 s |
| `item_adrenaline` | Adrenaline | heals to the maximum (+1 maximum outside deathmatch) | 60 s |
| `item_ancient_head` | Ancient Head | +2 maximum health | 60 s |
| `item_bandolier` | Bandolier | carry 250 bullets, 150 shells, 250 cells, 75 slugs; +50 bullets, +10 shells | 60 s |
| `item_pack` | Ammo Pack | carry 300, 200, 100 rockets, 100 grenades, 300 cells, 100 slugs; one of each ammo's amount | 180 s |
| `key_blue_key`, `key_red_key`, `key_data_cd`, `key_power_cube`, `key_pyramid`, `key_data_spinner`, `key_pass`, `key_commander_head`, `key_airstrike_target` | Blue Key, Red Key, Data CD, Power Cube, Pyramid Key, Data Spinner, Security Pass, Commander's Head, Airstrike Marker | a key in the inventory | never |

The **world models** are `models/items/<...>/tris.md2` (armour under `armor/body`, `armor/combat`,
`armor/jacket`, `armor/shard`, `armor/screen`, `armor/shield`; health under `healing/stimpack`,
`healing/medium`, `healing/large`, and `mega_h`; ammo under `ammo/<kind>/medium`; the powerups
`quaddama`, `invulner`, `silencer`, `breather`, `enviro`, `adrenal`, `c_head`, `band`, `pack`;
keys under `keys/<name>`) and the weapons' `models/weapons/g_<name>/tris.md2` (`g_shotg`,
`g_shotg2`, `g_machn`, `g_chain`, `g_launch`, `g_rocket`, `g_hyperb`, `g_rail`, `g_bfg`). The
**icons** are `pics/<icon>.pcx` (`w_*` weapons, `a_*` ammo, `i_*` armour, health and others, `p_*`
powerups, `k_*` keys). The **pickup sounds**: armour `misc/ar1_pkup.wav` (`ar2` for a shard, `ar3`
for power armour), weapons `misc/w_pkup.wav`, ammo `misc/am_pkup.wav`, powerups and keys
`items/pkup.wav`, health by amount `items/s_health.wav` (2), `n_health` (10), `l_health` (25),
`m_health` (100).

**The player's carry limits** start at 200 bullets, 100 shells, 50 rockets, 50 grenades, 200
cells, 50 slugs; health 100 of 100.

**Picking up** (touching an item, alive): each kind's rule says whether it is taken:

- **Ammo** is refused when already at the limit, else added up to it. A weapon's pickup adds the
  weapon (count + 1) and its ammo's amount (unless the weapon was dropped by a player). A newly
  owned weapon (or ammo that is a weapon, the grenades) is switched to when the player holds the
  blaster (in deathmatch) or always (otherwise).
- **Health**: refused at the maximum, unless it goes past it (the small and mega).
- **Armour**: with none worn, the new armour's base amount. With armour worn, the better
  protection (by its normal share) wins: when the new is better, the old count × (old share / new
  share), truncated, is added to the new's base, held to the new's maximum, and the old is
  removed; otherwise the new base × (new share / old share), truncated, is added to the worn
  armour, held to its maximum, and the pickup is refused when that adds nothing.
- **Powerups**: refused at skill 1 when 2 are held, at skill 2 or more when 1 is held (skill
  counts in deathmatch too; its default is 1). In deathmatch with the instant-items flag, a
  powerup is used at once (a dropped quad keeps its time left).
- **Keys** always taken (in co-op one each, and power cubes by their spawn bit).
- A taken item flashes the screen, shows its icon and name for 3 s, selects itself when it can be
  used, plays its sound, fires its targets once (also when not taken), and is removed or set to
  respawn: in deathmatch it hides (not solid, not sent) and reappears after its respawn time, a
  team of items (the same `team` key) reappearing as one random member.
- A **dropped** item (by a player's `drop`, or at death) is thrown 100 forward and 300 up from 24
  ahead and 16 below the eye, can be taken by its dropper only after 1 s, and vanishes after 29 s
  more in deathmatch.

| Armour | Base | Maximum | Normal share | Energy share |
| --- | --- | --- | --- | --- |
| Jacket | 25 | 50 | 0.30 | 0.00 |
| Combat | 50 | 100 | 0.60 | 0.30 |
| Body | 100 | 200 | 0.80 | 0.60 |

Only one armour is worn at a time; the worn one is the first of jacket, combat, body with a count.

## Damage

A hit carries a target, an inflictor (the rocket), an attacker (its owner), a direction, a point,
a surface normal, damage, knockback, flags (radius, no armour, energy, no knockback, bullet, no
protection) and a means of death (for the obituaries). In order:

1. A target that takes no damage is skipped.
2. **Teams**: in deathmatch with the skin-teams or model-teams flag (or in co-op), a hit on a
   teammate is 0 damage with the no-friendly-fire flag, else marked friendly fire.
3. **Easy**: at skill 0 outside deathmatch a player takes half damage, at least 1.
4. **Ambush bonus**: a direct (not radius) hit by a player on a living monster with no enemy yet
   does double damage.
5. **Knockback** (unless the no-knockback flag, or the target's no-knockback flag, or a target
   that does not move by physics: none, bounce, push, stop): velocity += direction normalised ×
   500 × knockback / mass (mass at least 50), **× 1600 instead of 500 when a player hits
   itself** (the rocket jump).
6. **Protection**: god mode, or invulnerability (also a pain sound at most every 2 s), saves it all
   unless the no-protection flag.
7. **Power armour** (unless no armour; a player with it on and cells, or a monster with its own):
   the screen guards only hits in front (the direction from the target to the point makes a
   cosine above 0.3 with its facing), absorbing up to a third of the damage at 1 cell a point; the
   shield guards all around, up to two thirds at 2 points a cell (cells = saved / points a cell,
   truncated).
8. **Armour** (players only, unless no armour): saved = the share (energy share for energy hits)
   × damage, **rounded up**, at most the armour's count, taken off the count.
9. **Health** loses the rest. At 0 or below the target is killed (a monster or player first gets
   the no-knockback flag): a monster not on the good side counts as a kill (a co-op attacker
   scores 1), and its targets fire; its `die` runs. Health is held at −999 at worst.
10. Otherwise a monster reacts (§ `sequel-monsters.md`) and feels pain (at skill 3 its pain then
    waits 5 s); a player feels pain unless in god mode; anything else with pain feels it.
11. A player's frame end shows the saved armour, power armour, blood and knockback as the view's
    flash and kick.

**Radius damage** (an explosion at the inflictor's origin with damage d and radius r, an entity to
ignore): every entity within r (by its box) that takes damage: points = d − 0.5 × the distance
from the explosion to the centre of its box; halved for the attacker itself; when above 0 and it
**can be damaged** (below), a radius hit of (int) points damage and (int) points knockback, the
direction from the explosion to its origin.

**Can be damaged** (an explosion or a melee blow reaching its target): a mover when a line from
the inflictor to the centre of its box is clear or hits it; anything else when a line (solid mask)
from the inflictor reaches its origin, or its origin offset by (±15, ±15, 0) in the four
combinations.

**Telefrag**: a player who spawns or teleports into another solid entity kills it (100 000 damage,
no protection), at most one entity a spawn.

## Weapons

Each weapon steps its view model's frame **once a server frame** (the step runs in the first
command that presses attack, else at the frame's start), through a state of activating, ready,
firing or dropping:

- **Activating**: frames from 0 to the last activate frame, then ready at the first idle frame.
- **A new weapon chosen** while not firing: dropping, through the deactivate frames, then the
  change (its view model, frame 0, activating).
- **Ready with attack** (pressed or held): with enough ammo (the weapon's per-shot amount), firing
  from the first fire frame **in the same step**; without, the "no ammo" click (at most once a
  second) and the best weapon with ammo is chosen: railgun, hyperblaster, chaingun, machinegun,
  super shotgun (2 shells or more), shotgun, blaster.
- **Ready without attack**: the idle frames, looping; at a pause frame it waits with a chance of
  15 in 16 each step.
- **Firing**: at a fire frame the weapon fires (and moves the frame itself), else the frame steps;
  at the first idle frame + 1 it is ready again. So a weapon's period is **(last fire frame − last
  activate frame + 1) frames of 100 ms**.
- The quad's sound plays on each shot under quad; quad multiplies damage and kick by 4 (and the
  rocket's radius damage, not radii).
- Shots leave from the eye: the origin + the view height − 8 up, and a per-weapon offset forward
  and right (right flipped for left-handed, 0 for centre-handed: the `hand` user setting).
- Every shot (unless silenced: a silencer shot is used up instead) makes a noise monsters hear.

| Weapon | Frames: activate, fire, idle, deactivate (last of each) | Fires at | Period | Shot | Damage (deathmatch / single) | Ammo |
| --- | --- | --- | --- | --- | --- | --- |
| Blaster | 4, 8, 52, 55 | 5 | 0.5 s | a bolt at 1000 units/s, offset (24, 8) | 15 / 10, energy, knockback 1 | none |
| Shotgun | 7, 18, 36, 39 | 8 (9 skips) | 1.2 s | 12 pellets, spread 500 × 500 | 4 each, kick 8 | 1 shell |
| Super Shotgun | 6, 17, 57, 61 | 7 | 1.2 s | 2 × 10 pellets, the halves aimed 5° left and right, spread 1000 × 500 | 6 each, kick 12 | 2 shells |
| Machinegun | 3, 5, 45, 49 | 4, 5 (alternating while held) | 0.1 s | 1 bullet, spread 300 × 500; kick climbs 1.5° a shot to 9 shots outside deathmatch | 8, kick 2 | 1 bullet |
| Chaingun | 4, 31, 61, 64 | 5 to 21 (loops 15 to 21 while held and stocked; 14 ends early on release) | 0.1 s | 1 bullet a step to frame 9, 2 to 14 (while held), 3 from 15; each from 7 ± 4 right, ± 4 up | 6 / 8, kick 2 | 1 a bullet |
| Grenade Launcher | 5, 16, 59, 64 | 6 | 1.2 s | a grenade at 600 forward, 200 ± 10 up, ± 10 right, 2.5 s fuse | 120 direct, radius 160 | 1 grenade |
| Rocket Launcher | 4, 12, 50, 54 | 5 | 0.9 s | a rocket at 650 | 100 + random 0..20 direct, 120 radius damage over 120 | 1 rocket |
| HyperBlaster | 5, 20, 49, 53 | 6 to 11 (loops 12 → 6 while held and stocked) | 0.1 s | a bolt at 1000 from a point circling 4 about the barrel | 15 / 20, energy | 1 cell |
| Railgun | 3, 18, 56, 61 | 4 | 1.6 s | a line through every player and monster to the first wall, offset (0, 7) | 100 / 150, kick 200 / 250 | 1 slug |
| BFG10K | 8, 32, 55, 58 | 9 (charging), 17 | 2.5 s | a ball at 400 | 200 / 500 (below) | 50 cells |
| Hand grenade | (its own steps, below) | | | thrown 400 to 800 | 125, radius 165 | 1 grenade |

The periods are derived from the frame numbers and want a measurement in the `se-sequel` lab.

**Bullets and pellets** (a line): first a line (shot mask) from the shooter's origin to the shot's
start; when that hits, the shot hits there. Otherwise the line goes from the start 8192 along the
aim, moved by crandom() × horizontal spread right and crandom() × vertical spread up (the aim's
right and up). A line that enters water, slime or lava continues from the surface with twice the
spread (a splash shows); a shot from inside water ignores the water. A hit on the sky does
nothing; a hit on something that takes damage deals the bullet's damage and kick (bullet flag);
a wall gets a puff and a ricochet.

**Bolts** (blaster, hyperblaster): a point projectile (no size, shot mask, fly-missile) that lives
2 s; on touching something other than its owner: nothing on the sky, damage (energy, knockback
1) to what takes it, sparks on a wall; then it is removed. When the shooter's own origin-to-start
line is blocked, it is moved back 10 and touches at once.

**Grenades**: a point (bounce) with a fuse; touching something that takes damage explodes it on
that (a direct hit of the damage less half the distance to its box's centre); otherwise it
bounces (a sound). Exploding: radius damage (the damage, its radius) ignoring the directly hit
one.

**Rockets**: a point (fly-missile) that lives 8000 / speed seconds; on touch (not the owner;
removed on the sky): the direct damage to what takes it (no knockback, no flags), debris off a
wall in single player, then radius damage (120 over 120) ignoring the one hit.

**The railgun's line**: from the start 8192 along the aim through the shot mask and slime and
lava; slime or lava is passed through once (the mask drops them); each player or monster it hits
takes the damage (not the shooter) and the line continues from there ignoring it; anything else
that takes damage takes it and stops the line; a wall stops it.

**The BFG**: the ball (fly-missile, 8000 / 400 = 20 s life) fires **lasers** every frame at every
player, monster or exploding box within 256 (not its owner): a line from the ball toward the
centre of each, through every body to the first non-body, 5 / 10 energy damage each (not to
laser-immune things). On touch (not the owner, not the sky): 200 to what it hits, 200 radius
damage over 100, and it stops and explodes over 5 frames; in the first, every entity within
1000 that takes damage, that it and the owner can both damage, takes the ball's damage × (1 −
√(distance / 1000)) as energy damage (the owner is skipped).

**The hand grenade**: pressing attack with grenades starts the throw: frames 1 to 15 at one a
step; at frame 11 the fuse is lit (3.2 s, a ticking sound) and the frame holds there while attack
is held; held to the end of the fuse it explodes in the hand (a held-grenade death); released, it
is thrown at frame 12 at 400 + (3 − time left) × 400 / 3 units/s (so 400 to 800 by how long it
was held), with the time left as its fuse, from (8, 8) at the eye; frame 15 waits for the fuse's
end before frame 16 makes it ready (idle 16 to 48, pauses 29, 34, 39, 48). Damage 125 (× 4
quad), radius 165. It explodes as a grenade.

## A player's life

- **Joining**: the player connects (a password or spectator password may refuse; spectators are
  capped by `maxspectators`), and on beginning is placed (§ Spawning a player). In deathmatch a
  player joining or respawning is a new body each time (inventory from scratch); in single player
  the inventory carries across levels (§ Levels).
- **Spawning a player**: a spawn point is chosen (§ Spawn points); the player stands 9 above it
  and is placed 1 higher, with health 100, the box (−16, −16, −24) to (16, 16, 32), mass 200,
  12 s of air, the walk move, the clip mask player solid, the blaster in hand (deathmatch, or a
  dead single player's restart), the view at the spawn's yaw (the delta angles set so the
  player's own angles give it); others in the spot are telefragged. In deathmatch a respawn holds
  the player still for 14 × 8 ms (the teleport hold) and shows a teleport flash.
- **The field of view**: the user setting `fov` held to 1..160 (90 when unset), or 90 with the
  fixed-fov flag in deathmatch.
- **Air**: under water (level 3) the air runs out after 12 s (a rebreather or suit refills it to
  10 s each frame); then each second the drowning damage grows by 2, starting at 2, up to 15, with
  no armour protection. Surfacing resets the damage to 2.
- **Lava** hurts 3 × the water level a frame (1 × with the suit), **slime** 1 × the water level a
  frame (none with the suit), each frame the player is in it.
- **Falling**: from the change of the vertical speed between frames (the last frame's speed when
  the player is still falling but slower, else the change on landing): delta = speed² × 0.0001,
  × 0.5 at water level 1, × 0.25 at level 2, nothing at level 3. Under 1 nothing; under 15 a
  footstep; up to 30 a short-fall sound; above 30 a fall (a far fall at 55 or more) and
  (delta − 30) / 2 damage (at least 1), unless the no-falling flag in deathmatch.
- **Dying** (health 0 or less): the body becomes a toss with its top lowered to −8 and the corpse
  flag, its weapon model dropped, a death frame (one of three in turn, or the crouched one), a
  death sound (`*death1` to `*death4` at random), the view turning to the killer; the obituary
  scores (§ Deathmatch); in deathmatch the weapon in hand (not the blaster, only with its ammo)
  is dropped, and the quad when the quad-drop flag is set and more than 1 s of quad is left (the
  two thrown 22.5° apart; the quad keeps its time left). The inventory is emptied and the
  powerups end. Under −40 health the body gibs (four gibs, the head thrown, a sound) and takes no
  more damage. A player may respawn 1 s after dying: in deathmatch by pressing attack, or at once
  with the forced-respawn flag; in single player any button reloads the last save. The corpse is
  copied into a queue of 8 bodies, so up to 8 stay in the world.
- **`kill`** (suicide) is refused within 5 s of spawning.
- **Spectators** (the `spectator` user setting, deathmatch only): no-clip, not solid, not sent;
  attack picks a player to follow, jump steps to the next.

## Spawn points

- **Deathmatch**: among `info_player_deathmatch`, either the one farthest from every living player
  (with the spawn-farthest flag), or a random one that is not one of the two nearest to a living
  player (all are allowed when there are 2 or fewer). The distance is to the nearest living
  player.
- **Co-op**: the n-th player's spot is the n-th `info_player_coop` whose `targetname` matches the
  level's spawn point name (the host is n = 0 and uses the start).
- **Single player**: the `info_player_start` whose `targetname` matches the spawn point name the
  last level change carried (the one without a name when none), or any start when none matches.
- `info_player_intermission` (with its `angles`) is where the intermission's camera sits; the
  first start when there is none.

## Deathmatch

- **The obituary**: a player killed by another player scores the killer +1 (−1 for a teammate's
  kill); a player killed by itself, the world, a fall, a liquid, a crusher or a trigger scores −1.
  The printed line names the means of death; our wording is our own.
- **Limits**: each frame, when not in an intermission, a `timelimit` (minutes) reached, or any
  player's score at the `fraglimit`, ends the level.
- **Ending a level**: with the same-level flag, the same map again; else the next map of
  `sv_maplist` (a space- or comma-separated list, wrapping), else the world's `nextmap`, else the
  map's first `target_changelevel`, else the same map. The intermission starts (§ Levels).
- **`dmflags`**, a bit set (default 0):

| Bit | Value | Meaning |
| --- | --- | --- |
| no health | 1 | health items are not spawned (nor adrenaline, the ancient head) |
| no items | 2 | powerups are not spawned |
| weapons stay | 4 | a weapon stays when taken and can be taken once by each player; weapons cannot be dropped |
| no falling | 8 | no falling damage |
| instant items | 16 | powerups are used when taken |
| same level | 32 | the level repeats |
| skin teams | 64 | teams by skin |
| model teams | 128 | teams by model |
| no friendly fire | 256 | teammates take no damage |
| spawn farthest | 512 | § Spawn points |
| force respawn | 1024 | the dead respawn without pressing |
| no armour | 2048 | armour and power armour are not spawned |
| allow exit | 4096 | the exit ends the level; without it, an exit in deathmatch kills whoever touches it (10 × max health) |
| infinite ammo | 8192 | ammo items and the BFG are not spawned, weapons give 1000, shots use none |
| quad drop | 16384 | the quad is dropped at death |
| fixed fov | 32768 | every field of view is 90 |

- **Spawning by mode**: an entity with spawn flag 0x800 is not spawned in deathmatch; outside
  deathmatch 0x100 is not spawned at skill 0, 0x200 at skill 1, 0x400 at skills 2 and 3. These
  bits are then cleared. (Monsters are not spawned in deathmatch at all: `sequel-monsters.md`.)

## Levels

- **The intermission** begins at a changelevel (or a deathmatch limit): dead players respawn,
  everyone is moved to the intermission spot frozen (no weapon, no model), the scores are shown in
  deathmatch and co-op, and after 5 s any button leaves. In single player a changelevel to a map
  without `*` (the next level of a unit) skips the intermission and goes at once; a `*` (a new
  unit) clears the unit's cross-level bits and the co-op keys.
- **Leaving**: the next map starts with the changelevel's `map` (`map$spawn` names the spawn
  point). Each player's health above the maximum is cut to it, and in single player the player's
  health, maximum health, inventory, weapon and god, no-target and power-armour flags are carried
  to the next level.
- **`target_changelevel`**: used by a trigger, it starts the intermission (single player: only
  while the player is alive); in deathmatch without allow-exit it kills its toucher instead.

## Settings

| Setting | Default | Flags | Meaning |
| --- | --- | --- | --- |
| `deathmatch` | 0 | latched | a match |
| `coop` | 0 | latched | co-op |
| `skill` | 1 | latched | 0 to 3, held to whole numbers |
| `maxclients` | 4 | server info, latched | slots |
| `maxspectators` | 4 | server info | |
| `maxentities` | 1024 | latched | |
| `dmflags` | 0 | server info | § Deathmatch |
| `fraglimit` | 0 | server info | |
| `timelimit` | 0 | server info | minutes |
| `password`, `spectator_password` | empty | user info | "none" means none |
| `sv_gravity` | 800 | | |
| `sv_maxvelocity` | 2000 | | per axis |
| `sv_airaccelerate` | 0 | latched (server) | § `sequel-movement.md` Air |
| `sv_maplist` | empty | | § Deathmatch |
| `sv_rollspeed`, `sv_rollangle` | 200, 2 | | the view's roll with sideways speed |
| `run_pitch`, `run_roll`, `bob_up`, `bob_pitch`, `bob_roll` | 0.002, 0.005, 0.005, 0.002, 0.002 | | the view's bob |
| `g_select_empty` | 0 | archived | allow choosing a weapon without ammo |
| `flood_msgs`, `flood_persecond`, `flood_waitdelay` | 4, 4, 10 | | chat flood control |
| `filterban` | 1 | | the ban list bans (1) or admits (0) |

The user settings the rules read: `name`, `skin` (`<model>/<skin>`), `hand` (0 right, 1 left, 2
centre), `fov`, `gender` (`male`, `female` or other: the obituary's words), `spectator`,
`password`.

**The player's commands**: `use <item>`, `drop <item>`, `inven` (the inventory screen; in
deathmatch the scores when it is open), `invuse`, `invdrop`, `invnext`, `invprev` (and `invnextw`,
`invprevw`, `invnextp`, `invprevp` by weapons and powerups), `weapnext`, `weapprev`, `weaplast`,
`kill`, `putaway` (close the scores, help or inventory), `wave <n>` (a gesture: 0 flip off, 1
salute, 2 taunt, 3 wave, 4 point), `say`, `say_team`, `players`, `score`, `help`, `playerlist`;
and the cheats (with `cheats 1` or outside deathmatch): `give`, `god`, `notarget`, `noclip`.

## The demo's map entities

Every class name the three demo maps use (`sequel.md`), with what it does and its defaults (keys
the map may set in brackets). Movers move with the sequel's **accelerated move** (speed, accel and
decel, each defaulting to the speed: a mover speeds up and slows down at its ends; verify the
curve in a lab).

| Class | Count (maps 1, 2, 3) | What |
| --- | --- | --- |
| `worldspawn` | 1 each | the world: `message` (the level's title), `sky`, `skyrotate`, `skyaxis`, `nextmap`, `sounds` (the CD track), `gravity` |
| `info_player_start`, `_deathmatch`, `_coop`, `_intermission` | | § Spawn points |
| `light` | 256, 255, 437 | the compiler's; at run time only a `targetname`d light with a `style` (32 and up) is kept, switching its light style on use |
| `func_door` | 4, 14, 6 | slides along its angle by its size less a lip [speed 100, doubled in deathmatch; wait 3 (−1 stays); lip 8; dmg 2 when blocking]; opened by touch, a trigger or damage (with `health`), owns area portals |
| `func_door_rotating` | 0, 4, 1 | swings 90 degrees [distance, speed 100, wait 3, dmg 2] |
| `func_button` | 2, 4, 3 | moves in by its size less a lip [speed 40, wait 3, lip 4], fires its targets; pressed by touch or damage |
| `func_plat` | 0, 1, 2 | a lift resting at the bottom [speed 20, accel and decel 5, each scaled × 0.1 for the accelerated move; lip 8; height]; a trigger above it raises it |
| `func_train` | 1, 0, 0 | follows its `path_corner`s [speed 100, dmg 100 when blocked] |
| `func_rotating` | 1, 2, 0 | spins [speed 100, dmg 2] |
| `func_wall` | 2, 10, 4 | a solid model, optionally toggled by a trigger |
| `func_water` | 0, 0, 1 | a mover of water [speed 25, wait −1] |
| `func_explosive` | 2, 1, 6 | breaks into debris when killed [health 100] or used |
| `func_areaportal` | 1 each | an area portal its door opens and closes |
| `func_timer` | 30, 7, 9 | fires its targets every `wait` [1] ± `random` seconds, starting after 1 s + `pausetime` + `delay` |
| `func_group` | | the compiler's grouping, no entity |
| `trigger_once`, `trigger_multiple` | | fire their targets when touched by a player [wait 0.2 for multiple] |
| `trigger_relay`, `trigger_always` | | pass a use on; fire at the map's start |
| `trigger_key` | 0, 0, 1 | fires only when the user holds its `item`, which is then used up |
| `trigger_push` | | sets a toucher's velocity to its direction × speed × 10 [speed 1000] |
| `trigger_hurt` | | damage [dmg 5] a frame (once a second with flag 16) to what touches it |
| `target_speaker` | 84, 92, 90 | a sound, looped or played on use |
| `target_explosion` | | an explosion on use (radius damage `dmg`) |
| `target_splash` | | a splash of particles on use |
| `target_secret`, `target_goal` | | counted in the level's secrets and goals; using them counts one found |
| `target_help` | | sets the help computer's message |
| `target_changelevel` | 1, 4, 1 | § Levels |
| `target_crosslevel_trigger`, `target_crosslevel_target` | 0, 1, 1 | set a unit-wide bit; fire when the bits match at the next level's start |
| `target_laser` | 0, 0, 2 | a beam toward its target or angle, energy damage a frame to what it touches [dmg 1] |
| `target_lightramp` | 0, 2, 0 | ramps a light style over time on use |
| `path_corner`, `point_combat` | | the paths monsters walk, the spots they run to in a fight |
| `misc_explobox` | 9, 3, 4 | an exploding barrel (a solid box that can be pushed: health 10, mass 400; killed, it explodes with radius damage `dmg` [150]) |
| `misc_teleporter`, `misc_teleporter_dest` | 0, 0, 1 | a pad: a toucher is moved to the destination 10 up, still, with a 160 ms hold, angles from the destination; telefrags |
| `misc_deadsoldier`, `misc_banner`, `misc_satellite_dish`, `misc_strogg_ship`, `misc_gib_head` | | scenery (models; the dead soldier can be gibbed) |
| `monster_*` | | `sequel-monsters.md` |
| items | | § Items |

The movers' finer behaviours (the accelerated curve, the door's crush and reverse, the train's
teleporting corners, the plat's trigger size) want their own measurements before M18t4 calls the
demo maps done; this table is the inventory the builder works through.
