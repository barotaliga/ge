# Quake II misc_actor / target_actor / "Think_Delay with no activator": facts

Clean-room answer document. Source read: Q2PRO upstream game library (src/game: the actor file,
the monster start file, the utility file, the combat file, the AI file, the trigger, target,
func, misc and items files). No code is copied here; everything is in plain words.
[read] = stated directly by the source. [derived] = follows from reading several places together.

Timing: the game runs at 10 frames per second (BASE_FRAMERATE 10); "frame" below is one 100 ms
game frame. [read]

## 1. misc_actor spawn

Order of the removal checks, each returning at once [read]:
1. If deathmatch is on: the entity is freed silently (no line).
2. If it has no targetname: prints `untargeted misc_actor at (x y z)` (developer print:
   "untargeted", the classname, "at", the origin as a vector string), then freed.
3. If it has no target: prints `misc_actor with no target at (x y z)`, then freed.
   (Classname first, then "with no target at", then the origin.)

Then [read]:
- Movetype STEP, solid BBOX.
- Model: `players/male/tris.md2`. No skin is set at spawn; the monster start routine resets the
  skin number to 0. Skin 1 is the "hurt" skin (see pain).
- Box: mins (-16, -16, -24), maxs (16, 16, 32).
- Health: the map's `health` key if non-zero, else 100. max_health is later set equal to the
  starting health by the monster start routine.
- Mass 200.
- No gib_health field is set; the gib threshold is a fixed number in its die routine (-80).
- Callbacks: pain = actor pain, die = actor die; stand, walk, run, attack (the ranged attack)
  are set; melee is explicitly none; sight is explicitly none. No search, dodge or idle.
  checkattack is the default monster one (set by the monster start routine).
- AI flag GOOD_GUY is set.
- Current animation set to the stand sequence, model scale 1.0.
- Starts through the generic walking-monster start (walkmonster_start).
- AFTER that start routine, its use function is replaced by the actor's own use (so the generic
  "monster use makes it angry at the activator" never applies). Comment in source: actors always
  start dormant and must be used to get going.

What the walking-monster start does for it [read in the monster file, applied to actor: derived]:
- Deathmatch check again (already removed above).
- Spawnflag 4 rewrite (ambush fix) is skipped because GOOD_GUY is set.
- Not counted in the level's total monsters (GOOD_GUY), and its death not counted as a kill.
- Think next frame; flags it as a monster (SVF_MONSTER), frame interpolation render flag,
  takedamage AIM, air supply 12 s, max_health = health, clip mask monster-solid, skin 0,
  deadflag NO; an `item` key is honoured (dropped on death).
- Random start frame within the current (stand) sequence.
- Next frame (start-go): unless spawnflag 2 (triggered spawn) and only during the first second,
  dropped to the floor and "in solid" check; yaw_speed defaults to 20; viewheight 25.
- Start-go then looks at its target: target_actor is not a path_corner and not a point_combat,
  so the actor gets goal and movetarget cleared, pause time set to "forever", and stands.
  Its `target` key is NOT cleared on that branch, so the later use can pick it. [derived]
  If no entity has that targetname at all, start-go prints `misc_actor can't find target <t> at
  (x y z)` (preceded by the picker's `G_PickTarget: target <t> not found`) and clears target;
  the later use then fails (see 6). [derived]
- If spawnflag 2 is set the generic "triggered spawn" path applies (invisible, not solid, use =
  triggered spawn) BUT the actor overwrites `use` afterwards with its own use, so a triggered
  actor would be used as an actor while still invisible/non-solid. [derived; edge case]

## 2. Frame sequences (frame indices in the model's frame list)

The model's frame list (the player model's) has 481 frames, indices 0 to 480. [read]

- stand: frames 128..167 (40 frames), every frame "stand" AI with distance 0. On entering stand
  during the first second of the level, the frame is randomized within the sequence. Loops
  (no end function). [read]
- walk: frames 251..258 (8 frames), "walk" AI. The per-frame table lists 11 distances but only
  the first 8 are used for this range: 0, 6, 10, 3, 2, 7, 10, 1. (The table's further entries,
  4, 0, 0, are never reached.) Loops. [read; the "only 8 used" is derived from the range]
- run: frames 93..98 (6 frames; the model's run02..run07), "run" AI. Table holds 12 distances,
  the 6 used are: 4, 15, 15, 8, 20, 15. (Unused tail: 8, 17, 12, -2, -2, -1.) Loops. [read/derived]
- pain1: frames 74..76, "move" AI, distances -5, 4, 1; then run routine. [read]
- pain2: frames 77..79, distances -4, 4, 0; then run. [read]
- pain3: frames 80..82, distances -1, 1, 0; then run. [read]
- flipoff: frames 39..52 (14 frames), "turn" AI (turns toward ideal yaw), distance 0 each;
  then run. [read]
- taunt: frames 234..250 (17 frames), "turn" AI, distance 0; then run. [read]
- death1: frames 4..10 (7 frames), "move" AI, distances 0, 0, -13, 14, 3, -2, 1; ends with the
  dead routine. [read]
- death2: frames 11..23 (13 frames), distances 0, 7, -6, -5, 1, 0, -1, -2, -1, -9, -13, -13, 0;
  ends with the dead routine. [read]
- attack: frames 0..3 (4 frames), "charge" AI, distances -2, -2, 3, 2; the FIRST frame (0)
  calls the fire function; ends with the run routine. [read]
No other frame calls a function. [read]

The run routine [read]:
- If still within the pain debounce time and it has no enemy: walk if it has a movetarget, else
  stand.
- Else if the STAND_GROUND AI flag is set: stand.
- Else: the run sequence.

## 3. Sounds, pain rule, the taunt lines

Sounds [read]:
- The actor plays NO pain, death or gib sounds: those calls are commented out in the source.
- Its only own sound is the jump sound at a JUMP target_actor: `player/male/jump1.wav`, on the
  voice channel, volume 1, normal attenuation.
- Its gun fires through the generic monster bullet with the actor machine-gun muzzle flash,
  whose sound is played by the client from the flash kind (not by the game code). [derived]

Pain rule, in order [read]:
1. If health is below half of max_health (integer half), skin becomes 1. (Checked on every
   pain, before the debounce.)
2. If the current frame number is still before the pain debounce: nothing more.
3. Debounce set to now + 3 seconds (30 frames).
4. If the damage source (`other`) is a player client AND a random draw is below 0.4:
   - ideal yaw set to face that player;
   - 50/50 random: flipoff sequence or taunt sequence;
   - prints to THAT player only (not broadcast), at chat print level, the line
     `<name>: <word>!` followed by a newline. Format: the actor's name, a colon and a space,
     the word, then an exclamation mark.
   - returns (no pain animation).
5. Otherwise one of pain1/pain2/pain3 chosen uniformly (random modulo 3).

The words list has four entries in this order: "Watch it", "#$@*&", "Idiot",
"Check your targets". The pick is random modulo 3, so only the first three are ever used;
"Check your targets" is never said. [read; "never" derived]

Names: a list of 8 in this order: Hellrot, Tokay, Killme, Disruptor, Adrianator, Rambear,
Titus, Bitterman. The actor's name is chosen by its entity number (index in the entity array)
modulo 8; it is fixed per entity, not random. [read]

Since a good-guy monster never gets angry at a player (section 8), a player shooting an actor
gets only taunts/pain, never return fire. [derived]

## 4. Attack

- Attack routine: set the attack sequence and set a pause time = now + (random 0..15) + 3 + 7
  frames, i.e. 10 to 25 frames (1.0 to 2.5 s). [read]
- Fire function (frame 0 of attack): fires one bullet, then if now is at or past the pause time
  clears HOLD_FRAME, else sets HOLD_FRAME, so the animation stays on frame 0 and fires one bullet
  every frame (10 per second) until the pause time is reached. So 10..25 bullets per burst, then
  the remaining 3 attack frames, then run (which re-decides). [read/derived]
- Shot origin: the muzzle offset of the actor-machinegun flash kind, (18.4 forward, 7.4 right,
  9.6 up), projected from origin by the actor's angles. [read]
- Aim: if it has an enemy: if the enemy is alive, at the enemy's origin minus 0.2 times the
  enemy's velocity (it leads BACKWARD, aiming where the enemy was 0.2 s ago), raised by the
  enemy's viewheight; if the enemy is dead, at the enemy's absmin raised by half its height.
  No enemy: straight along its facing. [read]
- Bullet: damage 3, kick 4, default spread horizontal 300 and vertical 500, muzzle flash kind
  "actor machinegun 1". [read]

## 5. Die

Order [read]:
- Before die is called, the generic Killed sets enemy = attacker and (for a monster not yet
  dead) clears touch and runs the monster death-use: drops its `item` if any, swaps target to
  deathtarget if set, and fires targets with the enemy (the attacker) as activator. Note: this
  runs even for the actor, so a deathtarget works. The death-use also masks the AI flags down to
  only GOOD_GUY. [read in the combat and monster files]
- Gib check: health at or below -80 -> throws 2 bone gibs (`models/objects/gibs/bone/tris.md2`),
  4 small meat gibs (`models/objects/gibs/sm_meat/tris.md2`), and the head as
  `models/objects/gibs/head2/tris.md2`, all organic; deadflag DEAD; return. No sound.
- If already dead: nothing more.
- Regular death: deadflag DEAD, takedamage YES (so the corpse can still be gibbed later),
  death1 or death2 chosen 50/50. No sound.
- Dead routine at the end of either death sequence: box becomes mins (-16,-16,-24),
  maxs (16,16,-8), movetype TOSS, flagged as a dead monster (SVF_DEADMONSTER), stops thinking,
  relinked.
- Not counted as a kill (GOOD_GUY). [read]

## 6. Use

When the actor is used [read]:
1. goal and movetarget = a random pick among entities whose targetname equals the actor's
   target (the picker chooses uniformly among up to 8 matches).
2. If nothing was picked, or the pick's classname is not "target_actor": prints (developer
   print) `misc_actor has bad target <target> at (x y z)`; then clears target, pause forever,
   stands; return.
   - When no entity has that name, the picker's own line `G_PickTarget: target <t> not found`
     comes first. When the target is NULL (e.g. a second use, after the first cleared it, or
     start-go cleared it), the picker prints `G_PickTarget called with NULL targetname` first
     and the bad-target line shows the null string. [derived]
   - When the pick exists but is another class: only the bad-target line. [read]
3. Otherwise: ideal yaw AND current yaw snapped to face the target_actor, walk routine (walk
   sequence), target cleared (so a later use hits the NULL case above). [read]
It walks with the generic walk AI toward the goal entity; arrival is by touching the
target_actor's trigger box (section 7). [derived]
The activator is ignored by the actor's use. [read]

## 7. target_actor

Spawn [read]:
- If it has no targetname: prints `target_actor with no targetname at (x y z)` but it is KEPT
  (not freed).
- Solid TRIGGER, touch = actor-point touch, box (-8,-8,-8)..(8,8,8), not sent to clients.
- JUMP (spawnflag 1): speed defaults to 200; `height` key defaults to 200; a yaw of 0 is turned
  into 360; the move direction is computed from the angles, then its vertical component is
  replaced by the height value (so movedir is a unit horizontal direction plus a raw vertical
  speed).
- Linked.

Spawnflags [read from the editor comment and the code]: 1 JUMP, 2 SHOOT, 4 ATTACK, 8 unused,
16 HOLD, 32 BRUTAL.
Keys: target (next target_actor), pathtarget (target of the action at this point), message
(said to players), speed and height (JUMP only). `wait` is documented in the comment but the
code never reads it. [read]

Touch (by any entity) [read]:
1. Ignored unless the toucher's movetarget is this point.
2. Ignored if the toucher has an enemy.
3. Toucher's goal and movetarget cleared.
4. If message: printed to EVERY in-use client slot 1..maxclients, at chat level, as
   `<actor name>: <message>` newline (no exclamation mark here). The name is the toucher's
   entity number modulo 8 into the same name list.
5. JUMP: horizontal velocity = movedir's x/y times speed. Only if the actor is on the ground:
   ground cleared, vertical velocity = the height value, and the jump sound plays. (Off the
   ground: horizontal velocity is still set, no vertical, no sound.)
6. SHOOT (2): nothing is done at all (empty branch) - and because of step 7's test, a SHOOT
   point also does NOT fire its pathtarget. [read/derived]
   Else ATTACK (4): enemy = a pick of pathtarget; if found: goal = enemy; BRUTAL (32) sets the
   BRUTAL AI flag (keep attacking past the enemy's death until gibbed); HOLD (16) sets
   STAND_GROUND and stands, otherwise run routine.
7. If neither SHOOT nor ATTACK and pathtarget is set: temporarily swaps target for pathtarget
   and fires targets with the actor as activator, then restores target. [read] The point's
   `message` is not centre-printed by that firing: target-firing skips the centre print when
   the activator is a monster. [derived]
   Note: this firing honours this point's `delay` and `killtarget` too. [derived]
8. movetarget = a pick of this point's target (if target is NULL the picker prints
   `G_PickTarget called with NULL targetname`). [read/derived]
9. If no goal yet: goal = movetarget.
10. If no movetarget and no enemy: pause forever and stand. Else if movetarget equals goal:
    ideal yaw set toward it (keeps walking via the current move).
A last point with no target therefore logs the NULL-targetname line and the actor stands. [derived]

## 8. Other interactions

- Finding enemies: for a GOOD_GUY monster the target-finding routine always returns false
  (there is a "look for monsters?" note but no code). An actor never acquires enemies by sight or
  sound. [read]
- Reacting to damage: a good-guy monster ignores damage from players and from other good guys.
  Damage from a bad monster goes through the normal rules: the actor gets mad at the attacker if
  it is the same base type (walking) and a different classname and not a tank/supertank/makron/
  jorg; or if the attacker meant to shoot it; or else adopts the attacker's enemy. [read]
  So a soldier hitting an actor makes the actor fight back; an actor only fights via ATTACK
  points or such retaliation. [derived]
- Ordinary monsters can target the actor only through infighting (it is not a client, so normal
  sight finds players only). [derived]
- Medics skip GOOD_GUY corpses when looking for monsters to heal. [read in the medic file]
- Using an actor never makes it angry at the activator (its use is the actor-specific one). [read]
- Player-visible: chat-level lines from a named actor (taunts on being shot by a player, 40%,
  to that player only; target_actor messages to all players), the flipoff/taunt animations,
  skin change under half health, silent pain and death, machinegun bursts with the actor flash.
  [derived]

## 9. "Think_Delay with no activator"

When it prints [read]: inside the generic target-firing routine (G_UseTargets), when the firing
entity has a non-zero `delay`: a temporary entity of class "DelayedUse" is spawned to fire
later (nextthink = now + delay times 10 frames), it stores the activator, message, target and
killtarget; and IF the activator passed in is null, the developer line
`Think_Delay with no activator` is printed right then (at scheduling time, not when it fires).
The routine then returns. Condition = the activator pointer is null; an activator that is the
world entity or the firing entity itself does not count. When the delayed entity later fires it
calls the same routine with the stored activator and no delay, then frees itself. [read]

What activator each stock source passes [read]:
- trigger_always: passes ITSELF as activator, with delay forced to at least 0.2 s. Never null.
- func_timer: fires with its stored activator. START_ON sets the stored activator to itself;
  when used, the stored activator becomes the user's activator. Never null in stock use. [derived]
- Monster death (deathtarget or target): the monster's enemy, which the kill routine has just
  set to the attacker (could be the world entity, never null in the normal kill path). [read/derived]
- Item pickup targets: the player (or toucher) who picked it up, once only. [read]
- path_corner pathtarget: the monster arriving. point_combat pathtarget: the monster's client
  enemy, old enemy or activator if a client, else the monster itself. [read]
- target_actor pathtarget: the actor. [read]
- target_crosslevel_target: itself. target_secret/goal: their user's activator. [read]
- trigger_multiple/once: the toucher or user. trigger_counter: the user's activator. [read]
- func_explosive / misc_explobox: the attacker. buttons: the stored activator (presser, toucher
  or damager). func_train: its stored activator; START_ON (or no targetname) stores itself. [read]

Stock ways a delayed fire can have NO activator (so the line prints) [derived]:
- A func_door with targets and a delay that is BLOCKED while closing: it reopens using the door
  entity's stored activator, which doors never set, so null.
- A func_clock that starts on its own (not spawnflag 4, "triggered"): its stored activator is
  reset to null, and when its countdown/up completes it fires its pathtarget with that null
  activator; with a delay on the clock, the line prints. A triggered clock stores its user.
- misc_viper_bomb touching down, and target_explosion / similar, fire with their stored
  activator; it is null only if they were started some way that never set it.
- Any entity whose stored activator is null and which fires targets with a delay, or any map
  that chains a use with a null activator into a delayed entity (e.g. a train started on its
  own stores itself, so not that).
trigger_always and func_timer START_ON do NOT produce the line in stock code, because both pass
themselves. [derived]

Note: with no delay, the same routine centre-prints `message` to the activator and would
dereference a null activator; stock doors avoid that only because using a door clears its
message. [derived]
