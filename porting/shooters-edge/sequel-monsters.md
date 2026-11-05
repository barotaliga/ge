# The sequel's monsters, specified

What the sequel's single-player monsters do, as rules and numbers in our words (README.md, the
rules), for M18t4's single-player half on the demo maps. **Where these facts came from**: the
research session of `sequel.md`, which read the sequel's GPL game library. As with the first
game's campaign (`campaign-rules.md`), **exact parity is not the goal**: the builder designs the
monsters (on `engine/ai`, with free or the pack's own models and sounds) and holds them to these
numbers and rules in the `se-sequel` lab. The animation frames each attack fires on are the
pack's models' own (their frame names: `attak101`, `run3`); a monster's step lengths per frame are
left to the builder to tune to the models.

## Life

- **Not in deathmatch**: a monster entity is removed when the map spawns in deathmatch.
- **Spawning**: it counts in the level's monsters (unless on the good side), thinks first 0.1 s
  later, is a box (solid, the step move; fliers and swimmers the step move with their flags), takes
  aimed damage, has 12 s of air, starts on a random frame of its first animation, and remembers
  its health as its maximum. A walker is dropped to the floor at the map's start. Turning speed:
  20 degrees a frame for walkers, 10 for fliers and swimmers; the eye is 25 above the origin (10
  for swimmers). Its clip mask is monster solid (`sequel-formats.md` § Contents).
- **Spawn flags**: 1 ambush (wakes only by seeing a player itself, or being hurt), 2 triggered
  spawn (hidden and not solid until used; then it telefrags its spot, appears, and turns on
  whoever used it), 4 sight (treated as ambush).
- **Paths**: with a `target` naming a `path_corner`, it faces the first corner and walks the path;
  a `point_combat` target is kept as the spot it runs to when it first finds an enemy (then it
  holds there); with neither it stands.
- **An `item` key** names an item it drops at death.
- **Dying**: counted as killed; its targets fire with its enemy as the activator; health at or
  below its gib health throws gibs and removes it, else a death animation leaves a corpse (the
  corpse flag: shots pass unless they ask for corpses). A **damaged skin** (the skin number + 1)
  shows when health is below half.

## Senses

- **Ranges** (origin to origin): melee under 80, near under 500, mid under 1000, far beyond.
- **Visible**: a line (opaque mask: solid, slime, lava) from its eye to the other's eye is clear.
- **In front**: the direction to the other makes a cosine above 0.3 with its facing (about 73
  degrees each side).
- **Noticing**, once a frame each monster considers one candidate, in this order:
  1. a monster that saw a player this frame or the last (the **sight entity**), unless this one is
     an ambusher, or already has that one's enemy;
  2. else a **noise** made this frame or the last (a player's jump, pain, weapon, or a gesture),
     heard;
  3. else, when it has no enemy and is not an ambusher, a **bullet's impact** noise, heard;
  4. else the frame's **sight client** (one player, chosen in turn each frame: the next living
     player who is not untargetable).
- **Seeing** a candidate player: not far; standing in light above 5 (the light level its client
  reports; a monster that has just spotted a player marks it lit); visible; and at near range
  in front of it unless the candidate is already hostile-looking (a player who just woke another
  monster), at mid range always in front of it. A candidate that is a monster passes its own enemy
  on.
- **Hearing** a noise: an ambusher must see it; others need it in the hearable set and within
  1000, in a connected area; it turns toward it and goes to look (it treats the noise as a sound
  target until it sees the player, and stands when within 64 of the noise).
- **Finding a target** makes it the sight entity (waking others), records the last sighting, and
  runs to its combat point if it has one, else hunts (faces and runs at the enemy). Its sight
  sound plays when it saw (not heard).
- **Reacting to damage** from a player: it turns on the attacker (keeping a visible player enemy
  and remembering the attacker as its old enemy). From another monster: it turns on the attacker
  when both fly, swim or walk alike and are of different kinds (not when the attacker is a tank,
  super tank or the bosses), or when the attacker was after it; otherwise it takes up the
  attacker's enemy. A ducked monster does not react.

## Choosing to attack

Each running frame, with an enemy: when the enemy is alive, the line from eye to eye must reach
the enemy (solid, monsters, slime, lava, windows). Then:

- **Melee range**: at skill 0 three times in four it does nothing; otherwise melee when it has a
  melee attack, else its missile attack.
- Otherwise, with a missile attack, after its last attack's wait, and not far: a chance per frame
  of 0.4 when standing its ground, 0.2 at melee, 0.1 near, 0.02 mid; halved at skill 0 and
  doubled at skill 2 or more. On success a missile attack, the next allowed after `2 × random()`
  seconds. A flier that does not attack slides sideways (0.3) or flies straight.
- Without sight of the enemy it follows the last sighting and the player's trail (the spots the
  player passed out of the monster's sight), and gives up searching 20 s after it last saw the
  enemy (its search time is 5 s past each sighting).
- **Dodging**: a player's projectile (blaster, rocket, BFG) aimed at a living monster in front of
  the shooter lets it dodge (duck) with the time to impact; at skill 0 only one shot in four.

## The demo's roster

The numbers a monster carries; attacks named by what they fire (`sequel-rules.md` § Weapons for
the projectiles; monsters' shots use the same projectiles and damage rules, a monster's bullets
the same spread rule).

| Class | Health | Gib at | Mass | Box | Attacks |
| --- | --- | --- | --- | --- | --- |
| `monster_soldier_light` | 20 | −30 | 100 | (−16, −16, −24) to (16, 16, 32) | blaster bolt 5 damage at 600; skin 0 |
| `monster_soldier` | 30 | −30 | 100 | the same | shotgun, 12 pellets of 2 (kick 1), spread 1000 × 500; skin 2 |
| `monster_soldier_ss` | 40 | −30 | 100 | the same | machinegun, bursts of 2-damage bullets (kick 4) for 0.3 to 1 s; skin 4 |
| `monster_infantry` | 100 | −40 | 200 | the same | machinegun bursts of 3 (kick 4); melee club 5 to 9 (kick 50) |
| `monster_gunner` | 175 | −70 | 200 | the same | chaingun bursts of 3 (kick 4); grenades of 50 at 600 (beyond melee range, grenades or the chaingun one in two; a chaingun burst goes on one in two while the enemy is alive and visible) |
| `monster_berserk` | 240 | −60 | 250 | the same | melee only: a fast spike 15 to 20 (kick 400) and a slow club 5 to 10 (kick 400) |
| `monster_parasite` | 175 | −50 | 250 | (−16, −16, −24) to (16, 16, 24) | a drain tongue reaching its enemy by a clear shot line: 5 on its first frame, 2 on each after, no knockback |
| `monster_flyer` | 50 | explodes | 50 | (−16, −16, −24) to (16, 16, 32) | a flier: blaster bolts of 1 at 1000 in pairs; a slashing melee of 5 |
| `monster_tank` | 750 | −200 | 500 | (−32, −32, −16) to (32, 32, 72) | blaster bolts of 30 at 800; rockets of 50 at 550 in volleys of three (the volley may loop); a machinegun of 20 (kick 4) sweeping |
| `monster_tank_commander` | 1000 | −225 | 500 | the same | the tank's (it differs only in health and its skin) |

**The soldiers' aim**: each shot aims at the enemy's eye, then is spread by crandom() × 1000 right
and crandom() × 500 up at 8192 units (so they miss often at range); two of the soldier's firing
frames shoot straight ahead instead. The other monsters aim at the enemy's eye with their weapon's
own spread.

**Pain**: each monster has a pain animation and sound, at most once every 3 s (and none for 5 s
after a hit at skill 3: `sequel-rules.md` § Damage). Several duck or dodge instead of taking pain (the soldiers duck under
an incoming projectile).

## Not yet specified

The step move's edge rules for walking monsters (stepping, not walking off ledges, the partial
ground), the medic's resurrection, and the other monsters of the full game (the medic, the brain,
the iron maiden, the gladiator, the mutant, the technician, the icarus, the barracuda shark, the
supertank, the hornet, the bosses) are left for a later note: the demo maps need none of them.
