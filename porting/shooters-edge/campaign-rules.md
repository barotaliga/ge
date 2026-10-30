# What a campaign program does, specified

The behaviour of the classic single-player game program, as rules and numbers in our words
(README.md, the rules), for writing **our own campaign mod**: the campaign variant of the
baseline mod (`games/shooters_edge/qc/`, M18s7). The player's own program in the pack needs none
of this (the VM runs it as it is, `campaign.md`); this note is for a free campaign that plays
the classic maps (and free maps) in the same spirit, under our own names and with free models
and sounds.

**Where each fact came from.** A research session (2026-10-30) read the single-player game
program's QC as released under the GPL. It is a summary of behaviour, not a transcription: the
builder designs the code (and may build it on `engine/ai`), and holds it to these numbers by
measurement in the `se-campaign` lab. Exact parity is not the goal here; the pack's program is
the exact game.

## The map vocabulary

A campaign mod that plays the classic maps answers their entities' classnames (the map's
entity text names them; an unanswered one is skipped with a message, `builtins.md`). Beyond the
deathmatch set the baseline mod already answers, the campaign's maps use:

- **Monsters**: `monster_army`, `monster_dog`, `monster_ogre` (and `monster_ogre_marksman`),
  `monster_knight`, `monster_hell_knight`, `monster_zombie`, `monster_wizard`, `monster_demon1`,
  `monster_shambler`, `monster_fish`, `monster_enforcer`, `monster_tarbaby`, `monster_shalrath`,
  `monster_boss`, `monster_oldone`.
- **Campaign flow**: `info_player_start` (and `info_player_start2`, the return to the start map
  from an episode), `info_player_coop`, `info_intermission`, `trigger_changelevel`,
  `trigger_setskill`, `trigger_secret`, `trigger_counter`, `trigger_relay`,
  `trigger_onlyregistered`, `trigger_monsterjump`, `func_episodegate`, `func_bossgate`,
  `item_sigil` (the rune), `item_key1`, `item_key2`, `path_corner`.
- **Set pieces**: `func_train`, `func_door_secret`, `func_illusionary`, `func_wall`,
  `misc_explobox`, `misc_explobox2`, `misc_fireball`, `misc_teleporttrain`, `trap_spikeshooter`,
  `trap_shooter`, `event_lightning`, `air_bubbles`, the `ambient_*` sounds, the `light_*`
  flames, `info_notnull`, `info_null`.

## A monster's life

- **Spawning**: counted in `total_monsters` at once; it starts thinking a random 0–0.5 s later
  (so a room does not move in lockstep). A walker is raised 1 unit and dropped to the floor; a
  flyer and a swimmer stay where placed. It takes aimed damage, turns at 20 degrees a step (10
  for flyers), sees from 25 above its origin. With a `target` naming a `path_corner` it walks
  the path (each corner names the next; the last one ends the walk with a stand); otherwise it
  stands.
- **Being used** (a trigger fires it): it becomes angry at the player who fired it, unless that
  player is invisible or not a target.
- **Dying**: counted in `killed_monsters` (and announced, `campaign.md` § statistics); its
  targets fire with its enemy as the activator; a body that took enough damage gibs.
- **Spawn flag 1, ambush**: it does not wake from another monster's sighting, only by seeing the
  player itself or by being hurt.

## Senses

- **Ranges** from eye to eye: melee under 120, near under 500, mid under 1000, far beyond.
- **Visible**: a line from eye to eye that ignores monsters reaches the target, and does not
  cross from open air into water or back.
- **In front**: the direction to the target makes a cosine above 0.3 with the monster's facing
  (about 73 degrees each side).
- **Noticing**: once a frame a monster checks ONE candidate: the client the server's rotating
  check offers (`checkclient`, `builtins.md`: one client every 0.1 s, only when the monster's eye
  is in that client's visibility set), or, within 0.1 s of another monster spotting a player,
  that monster (so a sighting wakes the room). Never a player not a target or invisible. Far:
  never. Mid: only in front and visible. Near: visible, and in front unless the player made
  noise in the last second (fired, which sets the player "hostile"). Melee: visible is enough.
  A non-player candidate (another monster) passes its own enemy on, if that is a player.
- **On noticing**: its sight sound, it records itself as the room's latest sighting, it heads for
  the enemy, and waits 1 s before its first ranged attack.

## The states

Each state is a chain of animation frames at 0.1 s a frame, and each frame moves the monster a
set distance (the walk and run distances in the roster) and runs one decision:

- **Stand**: look for a target; when `pausetime` passes, walk.
- **Walk**: look for a target; step toward the path's next corner.
- **Run** (has an enemy): when the enemy dies, return to an earlier enemy or to the path, or
  stand. While it can see the enemy it keeps a 5 s memory; in coop it also looks for a closer
  target when that memory runs out. In order: finish turning toward the enemy for a pending
  melee or missile attack (attack when facing within 45 degrees); else decide whether to attack
  (below); else strafe sideways at the same range when in the strafing state (switching side when
  blocked); else step toward the enemy with the chase step (`movetogoal`, `builtins.md`), which
  follows walls when the straight way is blocked.
- **Pain**: a pain reaction with a recovery time; at the hardest skill (3) pain reactions come at
  most every 5 s.

**Deciding to attack** (the default; some monsters have their own, the roster): a clear line from
eye to eye that hits the enemy (not through water), then: in melee range a monster with a melee
attack uses it. A ranged attack needs its cooldown past and the enemy not far: chance a decision
0.9 in melee range, 0.4 near (0.2 if it also has melee), 0.1 mid (0.05 with melee); after a ranged
attack the cooldown is a random 0–2 s.

**Getting angry**: hurt by anything but itself or its current enemy, a monster turns on the
attacker, unless the attacker is of its own kind (soldiers excepted: they turn on each other);
when the old enemy was a player it is remembered and returned to. This is the infighting players
use.

**A melee blow** lands only within 60 units, for a random amount (three random numbers summed,
times the monster's factor).

## The roster

Health; box; walk/run step distances a 0.1 s frame (typical, the cycles vary); attacks. `verify`
each against the pack's program in the `se-campaign` lab (it can spawn one of each with the pack's
own program and measure).

| Classname | Role | Health | Box | Moves | Attacks |
| --- | --- | --- | --- | --- | --- |
| monster_army | rifleman | 30 | player-sized, 64 tall | walk 1, run 8–15 | a 4-pellet shotgun blast with a short wind-up |
| monster_dog | hound | 25 | 64 × 64 × 64 | walk 8, run 16–20 | a bite (3 randoms × 8), a leap (300 forward, 200 up) that bites 10–20 on contact |
| monster_ogre | brute | 200 | the large hull | walk ~2, run up to ~20 | a chainsaw (3 randoms × 4 per stroke), a lobbed grenade (600 at the enemy, 200 up, 40 radius damage); the marksman variant is the same |
| monster_knight | swordsman | 75 | player-sized | walk 3, run 7–20 | a sword, swung while charging when within 80 |
| monster_hell_knight | heavy swordsman | 250 | player-sized | walk 2, run 14–25 | a sword, a charge, a fan of slow magic bolts (300) |
| monster_zombie | zombie | 60 | player-sized | walk 0–2, run 1–8 | thrown flesh (600, 200 up, 10 on hit); knocked down by any hit under its threshold, it gets back up with full health; only a big enough single blow (an explosion) gibs it (`verify` the threshold) |
| monster_wizard | flyer | 80 | player-sized | fly 16 | spit (600), fires while strafing |
| monster_demon1 | leaper | 300 | the large hull | walk 8, run 20 | claws (10–15), a leap (600 forward, 250 up) for 40–50 on contact |
| monster_shambler | giant | 600 | the large hull | run 20–24 | claws (3 randoms × 20 and × 40 for the two-handed smash), a lightning bolt; takes half damage from explosions |
| monster_fish | swimmer | 25 | 32 × 32 × 48 | swim 12 | a bite (2 randoms × 3) |
| monster_enforcer | trooper | 80 | player-sized | walk 2, run 7–18 | laser bolts (600, 15 on hit) |
| monster_tarbaby | spawn | 80 | player-sized | run 2 | a bouncing leap (600 forward, 200 up, 10–20 on contact); explodes on death (120 radius) |
| monster_shalrath | caster | 400 | the large hull | walk 6 | a homing ball (400, 110 on hit, 40 radius) |
| monster_boss | lava boss | 3 hits | 256 × 256 × 280 | none | lobbed fire (300); hurt only by the map's lightning set piece, three times |
| monster_oldone | final boss | 40000 | 320 × 256 × 280 | none | killed only by teleporting into it |

The large hull is 64 × 64 × 88 (mins −32 −32 −24, maxs 32 32 64), the clip hull the map format
reserves for big things.

## The game's flow

- **Skill** filters the map (`campaign.md`); `trigger_setskill` sets the `skill` setting (the
  start map's four halls). The program reads skill for little else: at 3, pain is rarer.
- **New game, carried game**: a new game's player starts with the axe, the shotgun, 100 health
  and 25 shells. Leaving a map keeps weapons, ammunition and armour (keys and powerups are taken,
  health clamped to 50–100, shells at least 25), carried in `parm1..parm9` (items, health,
  armour, shells, nails, rockets, cells, weapon, armour type × 100). Arriving on the start map
  with any rune held starts the inventory afresh.
- **Leaving a map**: a `trigger_changelevel` touched by a player notes its `map`, fires its
  targets, then (unless its spawn flag 1 says no intermission, outside deathmatch) starts the
  intermission a moment later: every player frozen at an `info_intermission` (a random one of
  those present, else the start), invisible, unhurt. The intermission ends on a button after 2 s
  (5 s in deathmatch); an episode's last map shows its finale text first (a second button press);
  the shareware episode's end shows the sell screen. `samelevel` keeps the same map; `noexit`
  kills whoever touches an exit (deathmatch servers).
- **Runes**: `item_sigil` sets its episode's bit in `serverflags` and the player returns to the
  start map. `func_episodegate` appears (blocking the hall) once its episode is done;
  `func_bossgate` disappears once all four bits are set (opening the way to the last map).
- **Secrets**: each `trigger_secret` counts toward `total_secrets` at spawn; touching it counts
  a find, announces it, plays its sound and prints its message.
- **Death in single player**: after a short delay, a button press restarts the map with the parms
  it was entered with (`restart`); in coop the player respawns at an `info_player_coop` with a
  fresh inventory; in deathmatch as the baseline mod does.
- **Coop**: monsters choose among players as above; players do not hurt each other with
  `teamplay 1`; keys are shared (a key picked up is not removed).

## What our campaign mod is, then

The same baseline sources (`common/`) with a campaign variant (`campaign/progs.src`): monsters on
these senses, states and numbers (each a free model and sound set), the campaign flow above, and
the arena variant (`arena/progs.src`, today's deathmatch) unchanged. Our names and our code; the
classnames are the maps' and stay.
