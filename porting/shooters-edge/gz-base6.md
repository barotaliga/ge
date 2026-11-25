Clean-room answers (no code), built on in games/shooters_edge/server/q2_gz_triggers.cpp.

# Ground Zero section 6 follow-ups: answers

Sources read:

- Base: Quake II 3.21 game library (id Software GPL release). Where an answer touches the
  player move, it also draws on the player-move code shipped in that same 3.21 release
  (the engine side, not the game library). It says so where it does.
- Ground Zero: id's GitHub repository could not be cloned from this sandbox. I read the
  Ground Zero ("rogue") game library as it was first imported, unchanged, into the Yamagi
  Ground Zero repository (its first commit, 2009-03-12). That import is id's GPL release.
  For every point below I also checked the current Yamagi bug-fixed version. It agrees
  except where a note says otherwise.

Tags: [read] means stated directly by the source. [derived] means it follows from what
the source does but is not written as one rule. [open] means the source does not settle it.

---

## 1. trigger_gravity and the player's gravity

### Base Quake II 3.21

- [read] Every entity has a per-entity gravity factor. It is set to 1 whenever an entity
  slot is initialised.
- [read] trigger_gravity's touch copies the trigger's gravity value into the factor of
  whatever touches it, and that includes players.
- [read] The factor has no effect on the player's movement. Each client frame, the game
  passes the player move the level gravity on its own (the `sv_gravity` cvar, default
  800 units/s²). The per-entity factor is not multiplied in.
- [derived] So in base Quake II a trigger_gravity does nothing to a player's movement.
- [read] The player's factor is set back to 1 only when the player's entity is
  initialised. That happens when the client begins on a level: in deathmatch each time
  the client begins, and in single player or coop when the entity was not already in use
  (a fresh level start, not a loaded save).
- [read] Respawning after death does not reset the factor.
- [derived] For a player the factor stays after leaving the trigger, but since nothing
  reads it, the result is the same either way.
- [read] For monsters and thrown objects the factor does count. Their physics multiplies
  the level gravity by it:
  - bounce, toss and gib objects: every frame,
  - walking monsters: when airborne and not in liquid.
  Fly and fly-missile movers get no gravity at all.
- [derived] In base Quake II nothing resets the factor on those entities. Once one touches
  a trigger_gravity, its factor stays at that value until the entity is freed.

### Ground Zero

- [read] Each client frame, the gravity given to the player move is the level gravity
  times the player's factor. So trigger_gravity does change a player's gravity.
- [read] That product goes into the player-move state's gravity field, a 16-bit signed
  integer.
- [derived] The product is therefore truncated to whole units/s². For example,
  0.5 × 800 gives 400.
- [read] Order of events each client frame:
  1. The move's gravity is taken from the factor as it stands.
  2. The move runs.
  3. The factor is reset to 1.
  4. The triggers the player now overlaps are touched, which sets it again inside a trigger_gravity.
- [derived] What this means:
  - The factor lasts only while the player stays inside the trigger. It does not persist after leaving.
  - It lags by one client frame: a frame's move uses the factor from the previous frame's touch.
  - On the first frame after leaving, the move still uses the trigger's factor. After that it is back to 1.
- [read] Touching is skipped for players in noclip and for dead players.
- [derived] For noclip and dead players the factor falls back to 1 after one frame.
- [read] Other entities are reset the same way:
  - Objects moved by the toss, bounce and fly physics: the factor is reset to 1 just before touching triggers at the end of each push.
  - Monsters under step physics: the factor is reset to 1 on every frame that they move, just before touching triggers.
- [derived] For those entities too, the factor holds only while they overlap the trigger.
- [read] As in base, the factor is set to 1 when the entity is initialised. A respawn
  needs no special reset because of the per-frame reset.
- [read, engine side] The 3.21 player move contains a change marked as Ground Zero's for
  negative trigger_gravity values. On the ground, the vertical speed is zeroed only when
  the move's gravity is positive. With zero or negative gravity it is not zeroed, and
  gravity is applied, so a negative value lifts the player off the floor.
  - [derived] The game library alone cannot produce a negative value in base, so this matters only when the game library passes a factor (Ground Zero).

## 2. Base trigger_gravity: the key, who it affects, the missing-key message

- [read] The "gravity" key is parsed as a whole number: the text is read as an integer and
  anything after a decimal point is ignored.
  - [derived] So "0.5" gives 0 (no gravity on things that use the factor), "1.5" gives 1, and "2" gives 2.
- [read] The key-missing check tests whether the key is present, not its value. A key
  given as "0" is accepted with factor 0.
- [read] When the key is missing, the server console prints a line and the trigger is
  removed:
  "trigger_gravity without gravity set at (x y z)"
  - The position is the trigger's origin as three integers in parentheses, separated by spaces.
  - For a brush trigger with no origin brush, that is usually (0 0 0) [derived].
  - The line prints whatever the developer setting is.
- [read] The touch fires for anything that touches triggers. The touch itself has no
  filter. The shared trigger-touch pass skips players and monsters whose health is 0 or
  less. Things that touch triggers:
  - players (not in noclip),
  - walking monsters as they step,
  - monsters and objects under step physics,
  - tossed, bouncing and flying objects (grenades, gibs, items dropped as toss) as they are pushed,
  - entities carried by moving brushes.
- [derived] Monsters and tossed objects are affected (see 1). Players are not.
- [note] The level's own gravity is a different key: worldspawn's "gravity" key sets the
  `sv_gravity` cvar (800 when absent).

## 3. Ground Zero trigger_gravity: TOGGLE, START_OFF, use

- [read] The "gravity" key is parsed as a fraction (decimal number). 1.0 is the level's
  gravity.
- [read] The missing-key behaviour is the same as in base: the same console line, and the
  trigger is removed.
- [read] Spawnflags: 1 TOGGLE, 2 START_OFF. START_OFF implies TOGGLE.
- [read] With either flag, the trigger gets a use. Each use flips the trigger between
  "touchable trigger" and "not solid", then relinks it.
  - The brush stays linked into the world, and its touch handler is kept.
  - While it is not solid, nothing can touch it, so it has no effect.
  - The brush is not removed.
- [read] With START_OFF the trigger starts not solid. The first use turns it on.
- [read] START_OFF without a targetname prints nothing. The trigger just stays off for
  good unless something fires it by some other means.
- [read] Without TOGGLE or START_OFF the trigger has no use at all.
  - [derived] Firing it does nothing, and it stays on.
- [note] In both games the trigger sends nothing to clients: it is invisible and not
  solid to movement [read].

## 4. Ground Zero misc_explobox in liquid

- [read] In deathmatch the barrel is removed at spawn.
- [read] Spawn values:
  - box 32 × 32 × 40,
  - mass 400,
  - health 10,
  - "dmg" 150.
- [read] 0.2 s after spawn the barrel is dropped to the floor. After that it thinks every
  0.1 s (every server frame).
- [read] Each think, in this order:
  1. It works out its liquid level and type (the method below).
  2. It marks itself immune to slime.
  3. It sets its "air runs out" time 100 s into the future.
  4. It applies the same per-frame world effects monsters get.

### Liquid level

- [read] Up to three points straight up from its origin are checked for any liquid
  (water, slime or lava):
  - the box's bottom + 1 unit,
  - then 26 units higher (27 above the bottom),
  - then 22 units higher again (49 above the bottom).
- [read] Level 0 if the first point is dry; otherwise 1, 2 or 3 by how many of the points
  in turn are in liquid. The liquid type is the contents found at the lowest point.
- [derived] The barrel is 40 units tall, so level 3 means the liquid is at least 9 units
  above its top.

### Drowning

- [read] The barrel's "air runs out" time is pushed 100 s ahead every frame.
- [derived] So it never drowns.
- [read] The drowning test would only run while health is above 0.

### Lava

- [read] In lava (unless immune to lava, which the barrel is not), if the barrel's damage
  timer has passed:
  - it takes 10 × liquid level damage (10, 20 or 30),
  - the timer is reset to 0.2 s later,
  - the damage has no special flags (armour does not matter; a barrel has none) and the cause of death is lava.
- [read] The damage does not depend on skill. The source has no scaling by level or skill.
- [read] The damage step runs before the "just entered" step. On the frame it enters
  liquid, that step clears the damage timer to 0.
- [derived] So the first lava damage lands on the first think in lava, or at most one
  think later.
- [derived] With the default health of 10, the first lava hit (10 or more) kills it.

### Slime

- [read] Slime would do 4 × level every 1 s.
- [read] The barrel sets itself immune to slime every think.
- [derived] So slime never damages it.

### Sounds

- [read] On entering any liquid (the first frame its level is above 0 after being dry), it
  plays one sound on the body channel at normal attenuation:
  - lava: player/lava1.wav or player/lava2.wav, chosen 50/50 at random,
  - slime: player/watr_in.wav,
  - water: player/watr_in.wav.
- [read] On leaving (level back to 0 after being in liquid) it plays player/watr_out.wav.
  Nothing else plays while it stays in.

### Movement in liquid

- [read] The barrel uses the walking-monster physics. In that physics an airborne entity
  gets gravity only when its liquid level is 0.
- [read] Nothing in that physics adds liquid friction or buoyancy for a barrel. The
  vertical-friction rules there apply only to flying and swimming monsters.
- [derived] An airborne barrel whose bottom is in liquid stops gaining downward speed. It
  keeps whatever vertical speed it had: it neither floats up nor slows.
- [derived] A barrel resting on the floor under water is unaffected (its velocity stays 0).
- [derived] This follows from the barrel now having a liquid level at all. Base barrels
  never think, so their level stays 0.

### Explosion

- [read] Death by lava goes through the same death handling as being shot:
  - the barrel stops taking damage,
  - it explodes 2 frames later (0.2 s), with the attacker recorded as the world.
- [read] That death handling runs inside the barrel's think and replaces the next think,
  so the 0.2 s fuse is not overwritten.
- [derived] The explosion is the normal barrel explosion, the same as a shot barrel.

## 5. Ground Zero trigger_push START_OFF without a targetname

### The pusher wall

- [read] This case prints this line to the server console exactly, with no position and
  with the full stop:
  "trigger_push is START_OFF but not targeted."
  It prints whatever the developer setting is.
- [read] The entity then becomes a solid brush model of the pusher-mover kind. Its touch
  is removed, and its "do not send to clients" flag is cleared.
- [derived] Its solidity:
  - it blocks players, monsters and projectiles like any solid brush mover,
  - it never moves (no speed or think is given to it),
  - it never pushes anything, since it has no touch,
  - PUSH_ONCE therefore never fires.
- [read] Because the "do not send" flag is cleared, the entity is sent to clients with
  its brush model.
- [open] Whether it actually shows depends on how the map compiler marked that brush's
  faces (trigger brushes usually carry a no-draw texture). The game library does not
  settle it.

### With a targetname

- [read] The trigger gets a use. Each use flips it between "touchable trigger" and "not
  solid", then relinks it. It is not unlinked or removed.
- [read] With START_OFF it starts not solid. Without START_OFF it starts on.
- [read] In base, a targetname on a trigger_push does nothing.

### Wind sound

- [read] The timing is the same as in base:
  - only for a client being pushed (alive),
  - misc/windfly.wav on the auto channel, full volume, normal attenuation,
  - at most once every 1.5 s per player (a per-entity timer).
- [read] The push is unchanged:
  - velocity set to the push direction × speed × 10 (the "speed" key defaults to 1000),
  - grenades always pushed, other things only when their health is above 0,
  - a pushed client's previous velocity is overwritten so no fall damage comes immediately.

## 6. Ground Zero trigger_push SILENT (4) and PUSH_ONCE

- [read] SILENT only suppresses the wind sound. Everything else is unchanged.
- [read] While SILENT, the per-player sound timer is not advanced either.
- [read] PUSH_ONCE (1) is unchanged from base: after any touch, the trigger is removed.
  - This includes a touch that pushed nothing (for example, a dead body).

## 7. Ground Zero func_plat

### A monster's use

- [read] When a monster is the user:
  - at the top it starts down at once (this replaces any pending timed descent),
  - at the bottom it starts up,
  - while moving up or down the use does nothing.
  The use then returns, so the base rule "ignore a use while a think is pending" does not
  apply to monsters.
- [read] Non-monster users still go through base's rule: start down only if nothing is
  pending.
- [read] Monsters call this from their "blocked" handling. Most ground monsters do (for
  example berserker, gunner, infantry, soldier, tank, medic, mutant, parasite, gladiator,
  iron maiden, supertank, stalker, widow), and only when the monster has an enemy whose
  box is entirely above or entirely below the monster's.
- [read] The plat is either:
  - the one the monster stands on; or
  - the first thing hit straight down within 384 units, measured from a point the attempted step distance ahead of the monster.
- [read] The plat is matched by class-name prefix "func_pla", so it covers func_plat and
  func_plat2.
- [read] The monster uses the plat only when that would carry it toward the enemy:
  - Enemy above: it uses the plat if it stands on the plat and the plat is at the bottom, or if it stands off the plat and the plat is at the top (to bring it down).
  - Enemy below: the mirror case.

### Blocked by a dead thing

- [read] When a player or monster with health below 1 blocks the plat:
  1. It first takes 100 crush damage (knockback 1).
  2. Then, as in base, the plat's own crush damage: "dmg", default 2 for func_plat.
  3. The plat reverses: rising turns to lowering, lowering turns to rising.
- [derived] The 100 usually gibs the corpse, which clears the way.
- [read] Living blockers take only the normal crush damage and the reversal. Other
  blockers (neither player nor monster) take 100000 and are exploded if still present.
  This is unchanged except for a guard for an entity already freed.

### The bad area

- [read] Each time the plat starts up, it spawns an invisible, non-solid trigger box with
  the class name "bad_area", owned by the plat:
  - X and Y: the plat's footprint.
  - Z: from the plat's bottom to 64 units above it, in its spawn (raised) position. The plat's spawn-time bounds are used as world coordinates, not moved with the plat.
- [derived] With no "height" key, the plat lowers by its own height minus "lip" (default
  8). So that box runs from 8 units below the lowered plat's top face to 56 units above
  it: the space just over the plat while it waits at the bottom.
- [open] With a "height" key the placement relative to the lowered plat differs. I did not
  check it against a map.
- [read] The box has no timeout. Every bad area the plat owns is removed when the plat
  reaches the bottom. It is not removed at the top or when it starts down.
- [derived] It stays through the climb, the wait at the top and the descent. If a block
  reverses the plat back up mid-descent, a second box is spawned, and all of them go at
  the next bottom.
- [read] What living monsters do with a bad area:
  - A ground step that would put a monster into a bad area it was not already inside is refused: the monster stays where it was and the step fails. The air-movement path does the same.
  - A monster already inside one moves freely, and the game remembers it is in one. Some monsters change attack while that is remembered: the gunner switches to its chaingun; the carrier and the widow also check it.
  - When the monster leaves, it forgets the area. If it had a previous enemy, it takes that enemy back up.
  - The extra "back away" and "attack the tesla" reactions apply only when the area's owner is a tesla, never a plat.
  - Dead monsters ignore bad areas.
- [read] The box's own touch does nothing. Players are unaffected.

### Other func_plat changes

- [read] The plat's trigger box is unchanged: same size, still touched only by living
  clients.
- [read] Riding a plat as a player is unchanged.
- [read] The shared accelerated-move routine was changed for every accelerating mover,
  including func_plat. It recomputes its acceleration-and-deceleration plan every frame
  instead of only when the mover's current speed is 0.
- [open] The exact effect on the plat's speed curve was not measured.
- [read] Nothing else in func_plat's spawn, timing (3 s at the top) or sounds changed.
