# Ground Zero: monsters and AI changes (facts page)

This is a WORKING-AREA facts page for Ground Zero's (the Quake II mission pack, game folder
`rogue`) monsters and its changes to the shared monster AI. It was written by a clean-room
reader from the pack's GPL v2 game library (id Software's release as kept by the Yamagi
Quake II project, "rogue" repository, current head, read 2026-11-23), compared against the
base game's library from the same project only to tell what the pack changes. It holds
facts only, in the reader's own words: no code, no source names. Each fact is marked
**[read]** (stated directly by the source), **[derived]** (worked out from several read
facts) or **[measured]** (not used: nothing here was run). Frames are model frame indexes
(0-based). The server runs at 10 frames a second (one frame = 0.1 s). Distances are in map
units, speeds in units per second. Skill: 0 easy, 1 medium, 2 hard, 3 nightmare.

---

## 1. Changes to the shared monster AI (every monster)

### 1.1 Dodging, ducking and side-stepping (the new dodge)

- **What starts it [read].** Only these projectiles, and only when fired by a *player*,
  warn the monster they will hit: blaster bolt, rocket, BFG ball, flechette, heat-seeking
  rocket (the heatbeam's projectile form) and the disruptor's tracker. Hitscan weapons
  (machinegun, chaingun, shotgun, railgun, the plasma beam) never warn. The warning traces
  8192 units along the shot; the first thing hit must be a living monster that has a dodge
  handler and has the shooter in front of it. Time to impact (eta) = (distance to the hit
  point − the monster's box half-width) ÷ projectile speed. [read]
- **Easy pre-filter [read]:** on easy, 75 % of warnings are dropped before anything else
  (kept with probability 0.25). This is unchanged from the base game.
- **The new shared dodge [read]:** a monster can be a *ducker* (has a duck and an unduck
  routine) and/or a *dodger* (has a side-step routine and is not standing its ground).
  Neither: nothing happens.
  1. If the monster has no enemy, the shooter becomes its enemy and it reacts as on sight
     (this happens before any of the checks below). [read]
  2. Ignored if eta < 0.1 s or eta > 5 s. [read]
  3. Skill roll: it reacts with probability 0.25 × (skill + 1): 25 % easy, 50 % medium,
     75 % hard, 100 % nightmare. On easy the warning pre-filter also applies, so easy is
     0.25 × 0.25 = 6.25 % of warned shots. [read; the product derived]
  4. "Low shot" height: for a ducker, the cut is (top of the box − 33); a shot whose impact
     point is at or below that height cannot be ducked. A ducker that cannot side-step
     gives up on a low shot, and also gives up when it is already ducked. For a non-ducker
     the cut is the top of its box (every shot counts as low). [read]
  5. A dodger already in a side-step finishes it and ignores the new shot. A dodger facing a
     low shot (or already ducked) side-steps: it picks the side AWAY from the side of its
     body the shot will strike (shot to its left → step right, and the reverse), stands up
     first if ducked, marks itself dodging, enters the strafing ("sliding") attack state and
     calls its own side-step routine. [read]
  6. Otherwise (a ducker, high shot): if its next allowed duck time has not come, nothing,
     else it clears any dodge and ducks (its own duck routine, given eta). [read]
- **The shared duck mechanics [read]:**
  - Duck down: the box top is lowered by 32 units below the monster's spawn-time box top
     (the "base height", recorded for every monster at start); the monster takes damage but
     is no longer an aim target for auto-aim (damage "yes" rather than "aim"); the hold time
     is at least now + 1 s.
  - Duck hold: the animation holds on that frame until the hold time.
  - Duck up: box top restored, aim target again, next duck allowed 0.5 s later.
  - Each monster sets the hold time from eta in its own duck routine (see 2.x); the common
     pattern is: easy → now + eta + 1 s ("stupid dodge"), other skills → now + eta +
     0.1 × (3 − skill) s, i.e. +0.2 s medium, +0.1 s hard, +0 s nightmare. [read, per
     monster]
  - Any run frame stands a ducked monster back up (the run AI clears the ducked state and
     restores the box if it differs from the base height). [read]
- **Strafing while dodging [read]:** a dodging monster's run AI forces the strafing state
  even when the enemy is out of sight ("dodging out of view"). A side-step whose walk move
  fails in both directions ends the dodge and goes back to straight running.
- **Who dodges [read]:** with the shared dodge: soldier (all three), infantry, gunner, iron
  maiden (chick), berserker (side-step only), medic and medic commander, brain (duck only).
  The stalker has its own dodge (section 7.1). Explicitly no dodge: gladiator, tank,
  supertank, mutant, insane, makron, jorg, turret. Monsters not listed keep no dodge
  handler. [read]

### 1.2 Strafing ("sliding") changes

- **Chance to strafe after deciding not to fire [read]:** in the base game only fliers
  strafed (30 %). In the pack: the daedalus 80 %, every other flier 60 %, and **ground
  monsters now strafe too, 40 %**. Against a tesla mine the chance is 0 (never strafe a
  tesla). [read]
- **Ground strafe speed cap [read]:** a non-flier's sideways step is capped at 8 units per
  frame (80 units/s) "to look less jerky"; fliers are not capped.
- If the side move fails, the monster flips side and tries the other; if that fails too it
  switches to straight running for that frame. [read]
- No strafing when the enemy is not visible (unless dodging). [read]
- **Strafing while firing [read]:** an attack frame that moves (a charge frame with a
  non-zero distance) while the monster is in the sliding state moves at 90° to the facing
  (left or right by the same side flag) instead of forward; if blocked it flips side.
  Against a tesla it never circle-strafes.
- **"Charging" monsters [read]:** a monster flagged as charging (used by the stalker and
  others while lunging) moves toward its goal with the normal goal-seeking move on attack
  frames instead of a plain forward step, and the random 25 % "bump around" in the
  goal-seeking move is suppressed for it.
- **"Manual steering" [read]:** a monster may switch off its own turning toward the enemy
  (used while blind-firing, while turning to call reinforcements and by the widow); then
  the stand, run, charge and face AIs do not change its yaw.
- After an attack the state goes back to straight only if it was missile or blind; a
  melee- or slide-chosen state is kept. [read]

### 1.3 Blind fire (shooting at the last place the player was seen)

- **Record of the last place [read]:** every frame the enemy is visible (in the stand AI
  with an enemy, the run AI, the charge AI and when a target is found), the monster stores
  the enemy's current origin as its blind-fire target, notes the time ("trail time") and
  resets its blind-fire delay to 0.
- **When blind fire is chosen [read]** (the shared attack check, when the line to the
  enemy's eye is blocked):
  1. the blocking thing is not a monster, and the enemy is not visible,
  2. the monster can blind-fire (flag set at spawn) and its blind-fire delay is ≤ 20 s
     (the turret: ≤ 10 s, its own check),
  3. its attack-finished time has passed,
  4. now ≥ trail time + blind-fire delay (it waits that long after losing sight),
  5. a trace from its eye to the blind-fire target hits no other monster (a hit on the
     enemy itself is allowed).
  Then the attack state is "blind" and the monster runs its missile attack. [read]
- **Each blind-fire attack adds to the delay [read]**, so the shots get rarer until the
  delay passes the limit and they stop. Per monster (each attack, before firing):
  | Monster | Added per blind attack | Chance to actually fire (by current delay) |
  |---|---|---|
  | Light soldier (blaster soldier) only | 2.1 + 2.0 + random 0–3 s (4.1–7.1 s) | delay < 1 s: always; < 7.5 s: 40 %; else 10 % |
  | Gunner | 2.1 + 2.0 + random 0–3 s (4.1–7.1 s) | same as soldier |
  | Iron maiden (chick) | 4.0 + 1.5 + random 0–1 s (5.5–6.5 s) | same as soldier |
  | Tank (and tank commander) | 3.2 + 2.0 + random 0–3 s (5.2–8.2 s) | same as soldier |
  | Turret | 0.4 + 3.0 + random 0–4 s (3.4–7.4 s) | same as soldier |
  (The chance table is read for each; "same as soldier" means the same three bands were
  read in each file.) [read]
  The chance roll is made before the delay grows; a failed roll skips the attack. [read]
- **Only these blind-fire [read]:** the light soldier (not the normal or SS soldier),
  gunner, iron maiden, tank, and the rocket and blaster turrets (only with the turret's
  blind-fire spawnflag, section 4.2). Medic commander uses the
  "blind" state for a different purpose (calling reinforcements, section 7.4).
- **Info_notnull targets [read]:** if the enemy is a non-solid entity (a target_anger
  pointing a monster at an info_notnull), a monster with a missile attack fires every
  time the check runs (no chance roll), and the clear-shot test passes as long as the
  trace is not stopped short.

### 1.4 Jumping down and up ledges

- **Shared test [read]**, run when a monster is blocked while chasing (it has an enemy):
  - Enemy "above" = enemy's box bottom more than 16 units above the monster's; "below" =
     more than 16 units under it; otherwise no jump.
  - **Down:** the monster must be able to move its full box 48 units straight ahead; from
     that point a line straight down (through water too) to (its box bottom − max drop − 1)
     must hit solid ground; the drop from its box bottom must be ≥ 24 units; the landing
     must be no more than 32 units below the enemy's box bottom; the floor must be nearly
     flat (normal z ≥ 0.9). Water at the landing fails (the hit must be solid).
  - **Up:** a line from (48 ahead, at its box top + max climb) straight down to 48 ahead at
     its origin height must hit solid; the step height above its box bottom must be ≤ max
     climb; it then turns to face the wall it hit (64-unit probe ahead) and jumps.
  - A jump in progress waits in the air on its "wait for land" frame until it is on the
     ground again, or 3 s after take-off at most, then continues. [read]
- **Per monster [read]:**
  | Monster | Max drop | Max climb | Jump launch (forward / up), down | up |
  |---|---|---|---|---|
  | Infantry | 192 | 40 | 100 / 300 | 150 / 400 |
  | Gunner | 192 | 40 | see 2.3 | see 2.3 |
  | Berserker | 256 | 40 | see 2.5 | see 2.5 |
  | Parasite | 256 | 68 | see 2.6 | see 2.6 |
  | Mutant | 256 | 68 | (its own leap, 2.9) | |
  | Stalker | 256 | 68 | see 7.1 | |
  It jumps up when the enemy's box bottom is above its own, otherwise down (infantry and
  the others with a two-way jump). [read]
- **Plats [read]:** when blocked, soldier, infantry, gunner, gladiator, iron maiden,
  berserker, medic, supertank, tank, parasite, stalker and mutant first try to use a
  func_plat: if the enemy is above (enemy box bottom ≥ its box top) or below (enemy box top
  ≤ its box bottom), and it is standing on a plat or will step onto one within this move (a
  384-unit line down from the step point), it triggers the plat when the plat is at the
  wrong end (standing on it at the bottom with the enemy above, or not on it and the plat
  is at the top so it comes down, and the mirror cases for an enemy below).
- **Blocked handler [read]:** when the goal-seeking move fails to step toward the goal,
  a monster with a blocked handler gets to act first (plat, jump, or a special attack),
  if it acted, the normal "pick another direction" search is skipped. Monsters with one:
  soldier, infantry, gunner, gladiator, iron maiden, berserker, medic, supertank, tank,
  parasite, stalker, mutant, floater, flyer, hover/daedalus, widow.

### 1.5 Hint paths (hint_path)

- **Entity [read]:** classname `hint_path`, box ±8, a trigger, invisible, removed in
  deathmatch; spawnflag 1 = END (set on both endpoints of a chain); key `wait` = seconds a
  monster freezes on touching it (for riding a plat). A hint_path with neither target nor
  targetname is removed with a warning.
- **Chains [read]:** at map start each END node that has a target and no targetname starts
  a chain (at most 100 chains); the chain follows targets. A node whose target names two
  entities (a fork) or a node already on a chain (a loop) ends the chain and is reported,
  such a chain is cut off right after its start node.
- **When a monster uses them [read]:** in the run AI, when the enemy has not been seen for
  ≥ 5 s and the monster has not checked for hint paths in the last 10 s (the check time
  resets each attempt). Not used by monsters standing their ground or by the turret, nor
  when there is no enemy.
- **How it chooses [read]:**
  1. Candidate start nodes: every node within 512 units of the monster and visible to it.
  2. Their chains; on those chains, every node within 512 units of the enemy and visible
     to the enemy is a candidate destination.
  3. Keep start nodes whose chain has a destination candidate; take the start node
     nearest the monster as the first goal; on that chain take the destination candidate
     nearest the **monster** (not the enemy) as the final goal. [read]
  (The "nearest" bookkeeping never lowers its running best distance, so in effect the
  *last* qualifying node in chain order is chosen, not the nearest; noted as read
  behaviour.) [read]
- **Following [read]:** the monster runs to each node in turn; on touching the node it is
  heading for it moves one node along the chain toward the goal (up or down the chain, by
  where the goal lies); touching the goal ends hint following. A node with `wait` stops the
  monster thinking for that many seconds. While following, the monster ignores sound in
  co-op, and the moment it sees its real enemy (the owner if the enemy is a player noise)
  it leaves the path and attacks (or hunts if not visible). With no valid enemy left it
  stands and stops acting (pause time set ~10^8 s ahead).
- Finding a target while on a hint path also leaves the path. [read]

### 1.6 Medic changes (both medics)

- **Search [read]:** radius 1024 units, but 400 when the medic stands its ground. A body
  is skipped if it is: a good-guy monster; a medic that already gave up on it (two
  "bad medic" slots per body: the last two medics that failed); being healed by another
  living medic; alive; still thinking (except for the flies-on/flies-off think); not
  visible; a player body; or within 32 units of the medic. Among the rest, the highest
  max health wins (ties keep the first found). The medic then has 10 s to finish
  (abandons the attempt after that and marks the body as bad for itself). [read]
- **Searching happens [read]** on the idle and search sounds (only when it has no saved
  old enemy) and every time its run starts (unless already healing).
- **Healing range [read]:** it heals when within 410 units of the body; farther, it runs
  to it.
- **Cable [read]:** frames 209–236 (see 2.17); the cable is cut and the attempt abandoned
  if the body is gibbed, gone, has come alive or the enemy became a client (switched by
  damage). Too close (< 32 units from the cable start): abandoned and the body gibbed. A
  wall in the way: retried once more on the next frames (two tries), then abandoned and
  the body marked bad for this medic; anything else in the way: abandoned.
- **Resurrection [read]:** at frame 219 the body becomes "being resurrected" (cannot be
  damaged, drawn with a shell effect via the monster effects); at frame 226 it is
  respawned: before respawning, it checks the spawn volume with the box 48 units taller
  than the corpse's; if it is blocked, in solid, or touches anything but the world, the
  attempt is abandoned and the body gibbed (damage = −gib health, or 500). A revived
  monster: does not count toward the level's kill count, ignores and is ignored by
  monster infighting ("ignore shots"), its flies are removed, it targets the medic's old
  enemy if alive, else looks for one and stands still if none. [read]
- **Gib when abandoned [read]:** gibbing uses damage = −(the body's gib health), or 500 if
  none.
- **Pain while healing [read]:** a healing medic plays no pain animation; damage only
  breaks the healing when its health is ≤ 25 % of max (then it releases the body).
- **No longer counts its revived** monsters as owned; the revived monster's kill does not
  add to the level total. [read]
- **Duck and side-step:** see 2.17.

### 1.7 target_anger

- **Entity [read]:** `target_anger`, keys `target` (the monster(s) to anger) and
  `killtarget` (the entity to be angry at). Both required (removed with a warning if
  missing). When used: the killtarget entity is turned into a monster-flagged "good guy"
  with health 300 (so monsters will shoot it); each targeted entity that has a use
  function and health ≥ 0 gets it as enemy, is marked "target anger" and reacts at once
  as on sight. A targeted entity with negative health stops the whole loop. [read]
- **While angry [read]:** damage from others is ignored as long as the angry target exists
  and the monster's health is above 33 % of max; at or below 33 %, or if the target is gone,
  the flag clears and normal damage reactions resume.
- If the killtarget is non-solid (an info_notnull) the monster fires every check (1.3).
  [read]

### 1.8 Tesla mines (the shared "bad area")

- A tesla that hurts a monster marks a "bad area" around itself (the tesla's trigger
  box, or ±128 horizontally from its floor to 128 up, lasting 30 s, if it has no trigger)
  and makes the monster target the tesla (if the area is new or the monster had no
  enemy). A medic that is healing gives up the body first. [read]
- Monsters never walk into a bad area; one that is in it and is fighting a tesla backs out
  (reverses a move that heads toward it); on leaving the area it returns to its old enemy.
  A monster that walks into a tesla's bad area while not already fighting a tesla, and
  whose enemy is not a visible client, retargets the tesla. [read]
- The medic's blaster does 3 instead of 2 against a tesla. [read]
- If the old enemy was a player, it is remembered and returned to when the tesla dies.
  [read]

### 1.9 Decoys, disguise, sight and hearing

- **Disguise [read]:** a player can be disguised by `trigger_disguise` (brush trigger,
  spawnflags 1 TOGGLE (use switches it on/off), 2 START_ON, 4 REMOVE (takes the disguise
  off instead of giving it)), or the console command `disguise`. A disguised player is
  never picked as the sight client, is not reacted to on monster use or on a triggered
  spawn, and his non-weapon noises are not heard. **Firing a weapon while disguised**
  makes him the "disguise violator" for 5 frames (0.5 s): during that window monsters'
  target search hears/notices him like a sight-entity; once a monster actually reacts to
  him, the disguise is removed. [read]
- **Doppelganger decoy:** it is a deathmatch item; the decoy is a damageable box (30
  health, ±16 × −24…32, lasts 30 s) but it is not flagged as a monster or client, so
  single-player monsters do not target it. [read; derived that monsters ignore it]
- **Visibility [read]:** "visible" now also passes when the sight line is stopped by the
  target itself.
- **Infighting [read]:** in the base game, shots from the tank, supertank, makron and jorg
  never started infighting. In the pack the rule is a flag ("ignore shots") on either the
  attacker or the victim: set on tank, supertank, both medics, turret, carrier, widow,
  widow2, every medic-revived monster and every monster spawned by the medic commander,
  carrier or widow. Makron and jorg are no longer exempt (they carry no such flag).
  [read; the makron/jorg consequence derived]
- After killing or losing its current non-player enemy, a monster returns to its saved
  last *player* enemy (stored when it switched to a tesla) if that player is alive. [read]

### 1.10 Flying monsters

- Strafe chances raised (1.2). [read]
- The carrier keeps 104 units off the ground instead of the flier's usual 40 (and rises
  when lower than 94 instead of 30) "to avoid shooting its own fliers". [read]
- Per-flier changes in 2.x (flyer's kamikaze variant, hover/daedalus, floater).

### 1.11 Other shared changes

- **Monsters walking on ceilings [read]:** the step and ground checks work for a monster
  whose gravity points up (used by the stalker): it stands on surfaces whose normal z ≤
  −0.7 and steps up/down "toward" the ceiling. (7.1)
- **Power-up shells on monsters [read]:** a monster can carry quad, double damage and
  invulnerability timers (frame counts); each shows its shell while running and blinks in
  the last 30 frames (on when frame count bit 4 is set). An invulnerable monster takes no
  damage (plays the protect sound once per 2 s).
- **Stationary monsters [read]** (the turret): start without moving; spawnflag 2 = trigger
  spawn (appears when used, telefragging what is in its box; one-frame delay); yaw speed
  defaults to 20; on appearing it attacks the activator unless spawnflag 1 (ambush) is
  set or the player is notarget/disguised.
- **Spawned monsters [read]:** monsters created at run time by the medic commander,
  carrier or widow are made with gravity down, drawn visible to infrared goggles, and
  never count toward the kill total; when one dies, its creator gets its slot back
  (medic commander, carrier) or its "used" count drops by one (widow).
- **The railgun from a monster [read]** does not fire if its muzzle point is inside a
  solid (prevents shooting through walls).
- **Co-op [read]:** the per-skill inhibit flags are applied in co-op too; entities with
  all three not-easy / not-medium / not-hard flags appear in co-op only (at every skill),
  and spawnflag 4096 removes an entity in co-op (not when the "co-op as base game" cvar is
  set).

---

## 3. Skill (what is new)

- Health is **not** scaled by skill for any monster, old or new, except where noted per
  monster (none found). [read: no health scaling in the spawn code or the new monsters]
- New per-skill numbers (details in the sections):
  - dodge reaction 25/50/75/100 % (1.1),
  - duck hold time eta + 1 s on easy, eta + 0.2 / 0.1 / 0 s on medium/hard/nightmare
     (per monster, 2.x),
  - medic commander reinforcement slots 3/4/6/6 and summon strength (7.4),
  - many attacks: see each monster (soldier SS and gunner aim, stalker, turret, carrier,
     widow).
- No reaction-time change in the shared AI. [read]

---

## 2. The base monsters: what the pack changes

Every monster below uses the shared changes of section 1 (strafing chance, blind-fire
bookkeeping, hint paths, tesla handling, infighting flag). Only differences from the base
game are listed. Health, box, mass, sounds and models are unchanged unless stated. [read]

### 2.1 Soldiers (monster_soldier_light, monster_soldier, monster_soldier_ss)

- **New spawnflag 8 "Blind" [read]:** the soldier's stand is replaced by a 30-frame loop
  of frames 146–175 (no movement, an idle sound chance on the first frame) that repeats
  until it is triggered or wakes; i.e. "will just stand there until triggered".
- **Blind fire [read]: only the light soldier (blaster)** can blind-fire (the normal and
  SS soldiers cannot). Blind attack: the chance bands of 1.3, delay grows 4.1–7.1 s per
  attempt; no shot at the world origin (blind target never set); it fires attack 1
  (frames 0–11) with its yaw locked (manual steering) and attack-finished = now + 1.5–2.5
  s. The refire at frame 5 is skipped while blind (and the steering lock is released
  there).
- **Aim spread [read]:** the base game deviates each shot by up to ±1000 sideways and ±500
  up at 8192 units. The pack keeps that below hard and halves it on hard and nightmare
  (±500 / ±250).
- **Clear-shot test [read]:** before firing at a live enemy, a trace from the muzzle to
  the enemy's eye must hit the enemy or the world; anything else (another monster) → the
  shot is skipped (and a held machinegun burst ends when its time is up).
- **Running fire (attack 6, frames 109–122) [read]:** in the base game used only from the
  sight reaction (50 %, at mid range or farther, skill > easy). In the pack:
  - from sight: skill > easy, enemy at least "near" range (not melee), 25 % chance, and
     never for the machinegun (SS) soldier,
  - as a normal attack choice: if not blocked and not standing ground, enemy at near range
     or farther, and random < skill × 0.25 (0 / 25 / 50 / 75 %), blaster and shotgun
     soldiers only,
  - its frames use the run AI (moving) instead of the charge AI; distances unchanged (10,
     4, 12, 11, 13, 18, 15, 14, 11, 8, 11, 12, 12, 17); fires at frame 111 (the third
     frame) using flash 8 but only when the enemy is within about 25° of its facing
     (dot ≥ 0.9); "charging" set on frame 109 and cleared on the refire frame (122),
  - refire at frame 122: at "near" range or farther, nightmare always, otherwise with
     probability 0.25 × skill → back to frame 111.
- **Duck [read]:** easy: duck animation (frames 45–49: 5, −1 hold, 1, 0 up, 5), hold until
  now + eta + 1 s. Other skills: with probability 1 − 0.3 × skill (70 / 40 / 10 % on
  medium / hard / nightmare) the duck animation with hold eta + 0.1 × (3 − skill) s,
  otherwise the crouched-fire attack 3 (frames 30–38, ducks on its fire frame 32 and
  stands at 36), hold eta + 1 s. Attack 3 repeats its shot while more than 0.4 s of hold
  remains. (Base: 25 % chance, eta + 0.3 s, and duck-vs-shoot split 67/33 % medium, 34/66 %
  hard.)
- **Side-step [read]:** blaster and shotgun soldiers side-step by playing the running fire
  (attack 6); the SS soldier plays its start-run animation.
- **Pain [read]:** the steering lock and charging are cleared; otherwise the same.
- **Death [read]:** the long death (frames death401–death453) ends with a wider corpse box
  (±32 × −24…−8) when that fits, else the usual ±16.
- **Blocked [read]:** uses plats (not while dodging or ducked).
- Machinegun burst length is timed by a separate timer so it does not clash with dodging,
  the burst is still 3–10 frames, as base. [read]

### 2.2 Infantry (monster_infantry)

- **New frames 207–216 (jump01–jump10) [read]** in the pack's model, for the ledge jump:
  - jump down: all frames move 0; take-off on frame 210 (velocity + 100 forward, + 300 up),
     frame 214 waits for landing (max 3 s),
  - jump up: frames 207–209 move −8, −4, −4 (a run-up back-step), take-off on 210 (+150
     forward, +400 up), 214 waits for landing.
  Ledge limits: drop 192, climb 40 (1.4). Plats too.
- **Machinegun attack rebuilt (frames 184–198) [read]:** moves per frame −3, −2, −1, 5, 1,
  −3, −2, 2, 1, 1, −1, 0, −1, −1, 4. Frame 186 sets the burst length to 4–19 frames
  (0.4–1.9 s); frame 187 fires and holds there firing every frame until the burst time is
  up; frame 191 plays the cock sound. (Base: cock and a 10–25 frame burst set on frame
  187, fire and hold on frame 194.) Damage unchanged.
- **Dodge [read]:** shared dodge (duck and side-step). Duck: not while jumping; while
  firing or punching on medium and above it does not duck; hold eta + 1 s on easy, else
  eta + 0.1 × (3 − skill). Duck frames 120–124 as base (−2 down, −5 hold, 3, 4 up, 0).
  Side-step: switch to the run (frames 92–99), not while jumping or (above easy) while
  attacking. (Base: 25 % duck.)
- **Pain [read]:** no pain reaction at all while airborne (skin still changes at half
  health); stands up if ducked.
- Search sound moved to the weapon channel. [read]

### 2.3 Gunner (monster_gunner)

- **New frames 209–218 (jump01–jump10) [read]:** jump down: all 0, take-off on 212 (+100
  forward, +300 up), wait for landing on 215; jump up: 209–211 move −8, −4, −4, take-off on
  212 (+150 forward, +400 up), wait on 215. Limits drop 192, climb 40. It tries plats
  first, then the jump.
- **Grenade check [read]:** a grenade volley (frames 108–128, grenades on 112, 115, 118,
  121 with sideways spread 0.02, 0.05, 0.08, 0.11, damage 50, speed 600, flashes grenade
  1–4) is now chosen only when: the enemy is not above the gunner's head (enemy box bottom
  below the gunner's box top), the target is at least 100 units away, and a line from the
  grenade muzzle reaches the target or hits the enemy. Otherwise the chaingun. Choice
  otherwise 50/50 as base. When being hurt by a tesla (standing in a bad area) it always
  uses the chaingun.
- **Grenade aim [read]:** a normal volley is thrown level along the facing plus spread
  (as base). A blind volley would aim at the blind-fire target, raising the aim when the
  target is more than 512 units away and within ±64 in height (by distance − 512), with
  the up component clamped to −0.5…0.4; [derived] in practice the steering lock that marks
  "blind" is released right after the volley is chosen, so blind volleys are thrown like
  normal ones, toward the facing (the gunner turns toward the enemy's real position while
  charging).
- **Blind fire [read]:** yes (blindfire flag set): chance bands of 1.3, delay grows
  4.1–7.1 s; only grenades, only if the grenade check passes; attack-finished = now + 0–2 s.
- **Duck [read]:** frames 201–208 (moves 1, 1, 0, −1, −1, 0, −1 with hold on 203 and stand
  up on 207). On hard and nightmare 50 % chance to throw a grenade as it goes down (as
  base). Not while jumping; not while firing above easy. Hold: easy eta + 1 s, else eta +
  0.1 × (3 − skill). (Base: 25 % chance.)
- **Side-step [read]:** switch to the run animation (not while jumping; not while firing
  above easy).
- **Pain [read]:** none while airborne.

### 2.4 Gladiator (monster_gladiator)

- Only change: when blocked it uses plats (1.4). Still no dodge. [read]

### 2.5 Berserker (monster_berserk)

- **Dodge [read]:** now has the shared dodge as a side-stepper only (no duck), so every
  warned shot it reacts to is side-stepped: it switches to its run (frames 36–41), not
  while jumping. (Base: no dodge.)
- **New frames 244–252 (jump1–jump9) [read]:** jump down: all 0, take-off on 247 (+100
  forward, +300 up), wait for landing on 250 (max 3 s); jump up: 244–246 move −8, −4, −4,
  take-off on 247 (+150 forward, +400 up), wait on 250. Limits drop 256, climb 40. Then
  plats.
- Melee damage unchanged (15–20 with 400 kick for the fast hit, 5–10 for the slow). [read]

### 2.6 Parasite (monster_parasite)

- **New frames 118–125 (jump01–jump08) [read]:** jump up: 118–121 move −8 each, take-off
  on 121 (+200 forward, +450 up), wait on 124 (max 3 s); jump down: 0s, take-off on 121
  (+100 forward, +300 up), wait on 124. Limits drop 256, climb 68. Then plats.
- **Drain reach test [read]:** the drain may start only if the enemy is within 256 units of
  the tongue's start (24 forward, 6 up) and the pitch to it is within ±30°, tried at the
  enemy's origin, then 8 below its box top, then 8 above its box bottom. The shared check
  decides to attack; then this reach test must pass. If the straight line is blocked it
  still starts the drain (and then no damage lands until the line clears).
- **Blocked attack [read]:** when blocked and its enemy is a player, with probability
  1 − (0.25 + 0.05 × skill) (75 / 70 / 65 / 60 %), if within drain reach and visible it
  starts the drain instead of trying another way.
- Drain damage unchanged (5 on the first contact frame, 2 per later frame, no knockback).
  [read]
- Sight, tap, scratch and search sounds moved to the weapon channel. [read]

### 2.7 Mutant (monster_mutant)

- **New frames 149–153 (jump01–jump05) [read]:** a ledge jump separate from its leap
  attack. Up: 149 −8, 150 −8 and take-off (+200 forward, +450 up), 151 waits for landing
  (no 3 s limit for the mutant); down: take-off on 150 (+100 forward, +300 up), wait on
  151. Limits drop 256, climb 68. Then plats.
- Its leap attack and melee are unchanged. [read]
- Its walk frames play no footstep sounds (the base library's walk does). [read]

### 2.8 Brain (monster_brain)

- **Dodge [read]:** shared dodge as a ducker only (no side-step). Duck animation unchanged
  (its frames: −2 down, 17 hold, …, −1 up); hold easy eta + 1 s, else eta + 0.1 × (3 −
  skill). (Base: 25 % chance, hold eta + 0.5 s.)
- Pain stands it up if ducked. Everything else unchanged. [read]

### 2.9 Iron maiden (monster_chick)

- **Model [read]:** `models/monsters/bitch2/tris.md2` (base: `models/monsters/bitch/…`).
  Health 175, gib −70, mass 200, box ±16 × 0…56 unchanged.
- **Rocket [read]:**
  - speed 500 + 100 × skill (500 / 600 / 700 / 800); damage 50; flash chick rocket 1
     (base: always 500),
  - aim point: if the enemy's feet are above the muzzle, or with 33 % chance, the enemy's
     eye; otherwise the enemy's feet (box bottom) — splash at the feet,
  - lead: with probability 0.2 + 0.15 × (3 − skill) (65 / 50 / 35 / 20 % on easy /
     medium / hard / nightmare) the aim point is moved along the enemy's velocity by the
     rocket's flight time (not when blind-firing),
  - clear-shot test: skipped if the line to the aim point first hits something other than
     the enemy or the world, or hits anything that is not a client in the first half of the
     distance,
  - re-fire after the shot (frame 26): enemy alive, beyond melee range and visible, with
     probability 0.6 + 0.05 × skill (base 0.6).
  Frames unchanged: wind-up 0–12, fire on 13 (moves 19, −6, −5, −2, −7, 0, 1, 10 reload, 4,
  5, 6, 6, 4, 3 for frames 13–26), wind-down 27–31.
- **Blind fire [read]:** yes; chance bands of 1.3, delay grows 5.5–6.5 s; aims straight at
  the blind-fire target; the shot is taken only if a line to the target, or to a point 10
  units to its left or its right, runs at least half way clear and not fully in solid
  (and the start is not in solid); attack-finished now + 0–2 s; no re-fire while blind.
- **Dodge [read]:** duck frames 83–89 (0 down, 1, 4 hold, −4, −5 up, 3, 1). Not while
  winding up or firing rockets above easy. Hold easy eta + 1 s, else eta + 0.1 × (3 −
  skill). Side-step: switch to the run (not while firing above easy).
- Plats when blocked. [read]

### 2.10 Tank and tank commander (monster_tank, monster_tank_commander)

- **Rocket [read]:** speed 500 + 100 × skill (base 550); damage 50; aim at the eye with
  66 % chance or whenever the enemy's feet are above the muzzle, else at the feet; lead by
  flight time with probability 0.2 + 0.15 × (3 − skill) (not when blind); the same
  clear-shot test as the iron maiden (2.9). Rocket loop frames 136–144, rockets on 138,
  141 and 144 (moves −3, 0, 0, 0, 0, 0, 0, 0, −1).
- **Blind fire [read]:** yes (new). Chance bands of 1.3; delay grows 3.2 + 2.0 + 0–3 s
  (5.2–8.2 s); goes straight into the rocket loop (no wind-up) with yaw locked, one loop of
  3 rockets, no re-fire; the shot test is the line to the blind target or 20 units to its
  left or right (half way clear, 2.9); attack-finished now + 3–5 s; no pain animation
  for 5 s.
- **Pain [read]:** clears the steering lock; otherwise as base.
- "Ignore shots" (its splash does not start infighting, and others' splash does not anger
  it), as in 1.9. Plats when blocked. Health 750 (gib −200) / commander 1000 (gib −225),
  mass 500 unchanged.

### 2.11 Supertank (monster_supertank)

- Only: plats when blocked; "ignore shots" flag (1.9). [read]

### 2.12 Boss2 (monster_boss2, the hornet)

- **Rocket volley [read]:** when its enemy is a player, 90 % of volleys are *predictive*:
  four rockets at speed 750 (base 500), damage 50, flashes boss2 rocket 1–4, each aimed
  at where the enemy will be after the rocket's flight time plus an offset: rocket 1
  −0.3 s, rocket 2 −0.15 s, rocket 3 0 s, rocket 4 +0.15 s (so the four bracket the
  target's path). Aimed at the enemy's origin (not the eye).
- **Otherwise [read]** (the other 10 %, or a non-player enemy): four rockets at speed 500,
  damage 50, aimed at the enemy's origin; rockets 1 and 4 aimed 15 units lower and fanned
  sideways by 0.4 (1 to the right, 4 to the left, in the boss's frame), rockets 2 and 3
  fanned by ±0.025. (Base: all four at the eye, no fan.)
- Frames, health and the rest unchanged. [read]

### 2.13 Jorg and Makron (monster_jorg, monster_makron)

- Jorg: no change other than the shared AI. [read]
- Makron: its thrown-off torso shows a red shell while a medic is resurrecting the
  Makron; a gibbed Makron throws 1 meat gib (base 4), 4 metal gibs and the gear head.
  [read]
- Neither carries the "ignore shots" flag any more (1.9). [read]

### 2.14 Flyer (monster_flyer) and the new kamikaze flyer (monster_kamikaze)

- **Flyer box [read]:** ±16 × −24…16 (base top 32). Health 50, mass 50 unchanged.
- **Circle-strafe attack (new, "attack 3") [read]:** same frames as the blaster attack
  (79–95) but every frame moves 10 (sideways, since the attack state is sliding) instead
  of the stationary attack's 0/−10; fires left/right alternately on frames 82–89 (8 bolts,
  damage 1, speed 1000; bolts on 82, 85 and 88 carry the hyperblaster trail effect, as in
  the base attack). Chosen with probability 0 on easy, 1 − 0.5 ÷ skill otherwise (50 %
  medium, 75 % hard, ~83 % nightmare); when chosen, 50 % chance to switch strafe side.
  Otherwise the base stationary attack (frames 79–95, moves −10 on the 8 firing frames
  82–89).
- **Blocked [read]:** handler present (kamikaze behaviour below; a normal flyer does
  nothing special).
- **monster_kamikaze [read]:** model `models/monsters/flyer/tris.md2`, box ±16 × −24…16,
  health 50, mass 100 (the mass marks it as a kamikaze), rocket trail effect on its body,
  looping sound `flyer/flyidle1.wav`, the flyer's sounds (sight `flyer/flysght1.wav`, idle
  `flyer/flysrch1.wav`, pain `flyer/flypain1.wav` / `flypain2.wav`, death
  `flyer/flydeth1.wav`). Removed in deathmatch.
  - It never stands, walks or attacks normally: every stand, walk, attack or melee turns
     into the kamikaze run: frames 120–124 (rollr02–rollr06), charging 40 units per frame
     (400 u/s) at its enemy, looping.
  - Each frame: if it has no enemy, or is within 90 units of it, it explodes: 50 damage
     straight to the enemy (no falloff, no range test beyond the 90-unit trigger), then dies
     as a flyer (death sound, explosion effect, removed). If it was a carrier's, the
     carrier gets its slot back.
  - Blocked: checks the 90-unit trigger first; if still alive it bursts (rocket explosion
     effect, slightly behind its motion), returns a carrier slot, and is removed without
     doing damage.
  - It feels no pain (no pain animation, no skin change).
  - When killed by damage it just explodes visually (no damage). [read]

### 2.15 Hover (monster_hover; the daedalus shares this monster, section 7.3)

- **Circle-strafe attack (new) [read]:** after the wind-up (frames 197–199, moves 1) the
  hover chooses: the base stationary burst (frames 200–202: moves −10, −10, 0; fires on
  200 and 201, the 200 bolt with the hyperblaster trail; re-fire check on 202) or the
  strafing burst (same frames 200–202 but moving 10 per frame sideways while sliding).
  Strafe chance: 0 on easy, else 1 − 0.5 ÷ skill (50 / 75 / 83 %); the daedalus adds 0.1.
  When strafing it switches side with 50 % chance. The re-fire (60 %, enemy alive and
  visible) repeats the same kind of burst; otherwise wind-down 203–204 (moves 1). Bolt:
  damage 1, speed 1000, flash hover blaster 1. (Frames for a separate strafing wind-up and
  wind-down exist but are not used. [derived])
- **Pain [read]:** damage ≤ 25: 50 % pain3 (frames 153–161) / 50 % pain2 (141–152), as
  base; damage > 25: pain1 (113–140) with probability 0.45 − 0.1 × skill (45 / 35 / 25 /
  15 %), else pain2 (base: always pain1). Half-health skin is "current skin + 1" (so the
  daedalus goes 2 → 3).
- **Death [read]:** clears its effects and power armour first; otherwise as base (falls,
  explodes on landing or after 15 s; gibs: 2 bone, 2 meat, a meat "head").
- Blocked handler does nothing. [read]

### 2.16 Floater (monster_floater)

- **Circle-strafe blaster (new) [read]:** the blaster attack's frames 31–44 played with
  every frame moving 10 (sideways while sliding) and firing on 34–40 (7 frames) instead of
  the stationary version (moves 0, fires on the same loop frames as base). Chance 0 on
  easy, else 1 − 0.5 ÷ skill; switches side 50 % of the time when chosen.
- Blocked handler does nothing. Everything else unchanged. [read]

### 2.17 Medic (monster_medic)

Health 300, gib −130, mass 400, box ±24 × −24…32, skin 0, sounds unchanged.

- **Healing:** see 1.6 (search radius rules, 10 s limit, bad-medic marks, abandon/gib
  rules, revived monster flags). [read]
- **Cable attack frames 209–236 [read]:** moves per frame 2, 3, 5, −4.4, −4.7, −5, −6, −4,
  0 (frames 209–217; the base game moved *forward* 4.4, 4.7, 5, 6, 4 on 212–216 — the pack
  backs off instead), hook launch sound 218, cable on 219–227 (219: hit sound and the body
  marked as being resurrected; 220: heal sound; 226: the body respawns), 228 retract (base
  had a −15 move there; the pack 0), then −1.5, −1.2, −3, −2, 0.3, 0.7, 1.2, 1.3 (229–236).
  The first 3 frames now use the charge AI (face the body) instead of a plain move.
- **Attack check [read]:** while healing, it starts the cable only within 410 units of the
  body; on medium and above, a medic standing its ground always chooses its attack,
  otherwise the shared check.
- **Blaster [read]:** unchanged: frames 177–190, bolts on 185 and 188 (damage 2, speed
  1000, flash medic blaster 1; 185 and 188 with the blaster trail), continue at 190 into
  the hyperblaster burst (frames 191–206; frames 191–194 do not fire, a bolt on each of frames 195–206, twelve in all, damage 2,
  bolts on 195, 198, 201, 204 carry the hyperblaster trail) with 95 % chance if the enemy
  is visible. Against a tesla each bolt does 3.
- **Dodge [read]:** duck frames 131–146 (−1 each; down 133, hold 134, up 144); not while
  healing; not during any of its attacks (ignores skill); hold easy eta + 1 s, else eta +
  0.1 × (3 − skill). Side-step: switch to the run (frames 102–107), not while attacking
  above easy. (Base: 25 % duck.)
- **Pain [read]:** while healing: no pain animation. Otherwise as base (50/50 pain1
  108–115 / pain2 116–130, 3 s debounce). Half-health skin 1.
- Plats when blocked; "ignore shots" flag (1.9). [read]

### 2.18 Flipper (monster_flipper)

- Only the search sound moved to the weapon channel. [read]

### 2.19 Insane (misc_insane)

- No change (no dodge handler, explicitly none). [read]

### 2.20 Actor (misc_actor)

- Neither library read registers a `misc_actor` spawn, so there is nothing to compare.
  [read; open whether the original pack shipped one]

---

## 4–8. The new monsters

Each monster answers questions 4 (model, box, health, sounds, spawnflags), 5 (animations),
6 (attacks), 7 (pain, death, special abilities) and 8 (boss deaths, where it applies).
Spawnflags 1 Ambush, 2 Trigger_Spawn, 4 Sight are the usual monster flags unless stated.
Every one of them is removed in deathmatch. [read]

### 7.1 Stalker (monster_stalker)

**Q4 [read]**
- Model `models/monsters/stalker/tris.md2` (projectile model `models/proj/laser2/tris.md2`
  precached). Skin 0, skin 1 below half health.
- Box ±28 × −18…18; view height 15. Health 250, gib health −50, mass 250.
- Sounds: sight `stalker/sight.wav`, idle `stalker/idle.wav` (volume 0.5), pain
  `stalker/pain.wav`, death `stalker/death.wav`, melee hits `stalker/melee1.wav` and
  `stalker/melee2.wav`; sight, idle and pain on the weapon channel.
- Spawnflag 8 "OnRoof": starts stuck to the ceiling (upside down, gravity reversed).

**Q5 animations [read]** (frames 0–92)
| Name | Frames | Moves per frame | Events |
|---|---|---|---|
| stand | 0–20 | 0 | idle noise on 6 |
| idle2 | 21–33 | 0 | — |
| walk | 34–41 | 4, 6, 8, 5, 4, 6, 8, 4 | — |
| jump (up) | 42–48 | −8, −8, −8, −8, 0, 0, 0 | take-off on 46 (+200 forward, +450 up); wait for landing on 47 |
| jump (down) | 42–48 | all 0 | take-off on 46 (+100 forward, +300 up); wait on 47 |
| jump straight up (dodge) | 45–48 | 1, 1, −1, −1 | jump on 45; wait for landing on 46 |
| run | 49–52 | 13, 17, 21, 18 | — |
| shoot (run and fire) | 49–52 | 13, 17, 21, 18 (charging; sideways when strafing) | fire on 50; second shot chance on 52 |
| dodge run | 49–52 | 13, 17, 21, 18 | dodge ends on 52 |
| swing left | 53–60 | 2, 4, 6, 10, 5, 5, 5, 5 | hit on 57 |
| swing right | 61–65 | 4, 6, 6, 10, 5 | hit on 63 |
| pain | 66–69 | 0 | — |
| death / false-death start | 70–78 | death: 0, −5, −10, −20, −10, −10, −5, −5, 0; false death: all 0 | — |
| twitch (false death) | 79–88 | 0 | heals every frame, loops |
| reactivate | 89–92 | 0 | then run |
- Stand picks stand (25 %) or idle2 (75 %); the idle hook picks stand (35 %) or idle2
  (65 %).

**Q6 attacks [read]**
- **Blaster (ranged):** green blaster bolt ("blaster2" type), damage 15, speed 800, flash
  "stalker blaster", from 24 forward / 6 up; aimed at the enemy's origin (not the eye).
  With probability 0.2 + 0.1 × skill it leads the target by (distance ÷ 1000) s of the
  target's velocity. Fired only if a line to the aim point first hits the enemy or the
  world. The second shot (frame 52) fires with probability 0.4 + 0.1 × skill.
- **Strafing while shooting:** straight with probability 0.5 ÷ skill (always on easy; 50 %
  medium, 25 % hard, ~17 % nightmare), else circle-strafe, switching side 50 % of the time.
- **Jumps from the shot frame:** on the first fire frame, if on the ground, 33 % chance to
  jump first: if the enemy is more than 256 away (or 50 % otherwise) it pounces at the
  enemy, else it jumps straight up (and may stick to the ceiling); it still fires.
- **Pounce:** not from the ceiling; the enemy must be standing on ground, not in water, the
  target point not in water, and four probe points under the enemy's box must be solid
  (the probes use the enemy's box corners as absolute map positions, i.e. points near the
  map origin, not under the enemy — read as written); the stalker must face within 45° of
  the target (it then turns to face exactly) and be within 450 units. It aims 32 above the
  target if the target is ≥ 32 higher, and prefers the high arc then or when the straight
  line is blocked by something other than the enemy. Launch speed: tries 400.1, then
  600.1 u/s, solving the ballistic angle with an assumed gravity of 800; the low arc unless
  a high jump is preferred; vertical speed gets +0.5 × gravity × 0.1 s extra. Fails (no
  jump) if no angle exists.
- **Melee:** 50/50 swing left or right; damage 5–9, kick 50, reach the standard melee
  distance; left swing's hit sound is `melee2`, right's `melee1`.
- **While airborne (wait-for-land frames):** each frame, if its attack-finished time has
  passed, with probability 0.3 + 0.1 × skill it fires a blaster shot (and sets the next
  allowed shot 0.3 s later). Airborne it falls with 1.3 × gravity; the wait ends on
  landing or after 3 s.

**Q7 pain, death, abilities [read]**
- **Pain:** none while dead, on nightmare (returns before anything else), while airborne,
  or while falling into / rising from false death. Hit while twitching in false death:
  it gets up at once (reactivate). Below 25 % health, with probability 0.2 × skill, it
  feigns death (only if on the floor, or if it can drop from the ceiling). Otherwise, with
  a 3 s debounce, only for damage > 10: 50 % (if on the ground) a dodge jump straight up,
  else the pain animation; the pain sound either way.
  [derived] False death therefore happens 0 % easy, 20 % medium, 40 % hard and never on
  nightmare.
- **False death:** gravity set back to normal (it drops off a ceiling, roll reset), stands
  its ground, plays frames 70–78 then loops the twitch 79–88 healing 1 hp per frame on
  easy/medium, 2 on hard, 3 on nightmare; skin back to 0 above half health; at full health
  it plays reactivate (89–92) and runs again.
- **Death:** it always falls (gravity normal, roll reset, toss). Gibbed (health ≤ −50):
  `misc/udeath.wav`, 2 bone gibs, 4 meat gibs, head gib `models/objects/gibs/head2`.
  Else the death sound and frames 70–78, corpse box ±28 × −18…−4.
- **Dodge (its own):** only when on the ground and alive; acquires the shooter if it had
  no enemy (and does nothing else that time); otherwise for eta 0.1–5 s it always (no skill
  roll beyond the shared easy pre-filter) does the jump straight up (frames 45–48).
- **Jump straight up:** on the floor: velocity +random ±5 sideways, +400 up; if a ceiling
  is in reach it switches to the ceiling (gravity reversed, roll 180°). On the ceiling: if
  a floor is in reach it lets go (gravity normal, roll +180°).
- **Ceiling reach test:** from the floor, a box trace straight up 180 units (256 for the
  widow's stalkers) must hit world solid whose surface faces down (normal z ≤ −0.9); from
  the ceiling, 384 units down to an up-facing surface (normal z ≥ 0.9). Then the four
  corners of its box, traced up/down to that height plus a margin (8 beyond the box),
  must each hit world solid within 8 units of the expected height (flat enough).
- **Blocked:** on the floor: if the enemy is visible it pounces (and counts as handled
  even if the pounce fails); else the ledge jump (drop 256, climb 68; up when the enemy's
  box bottom is at or above its own); else plats. On the ceiling: drops to the floor if it
  can.
- It carries a "walks on walls" flag that nothing else reads. [read]
- Hint paths, blind fire: no blind fire. [read]

### 7.2 Turret (monster_turret) and turret_invisible_brain

**Q4 [read]**
- Model `models/monsters/turret/tris.md2`; skin 0 blaster, 1 machinegun, 2 rocket. A wall
  unit also has a base piece, `models/monsters/turretbase/tris.md2`. Debris model
  `models/objects/debris1/tris.md2`.
- Box ±12 (all axes). Health 240, gib health −100 (never used: it has no gib death), mass
  250. Mechanical (hits spark rather than bleed). No gravity, never moves (except a wall
  unit's slide out). Turn rate 45° per aim step, both yaw and pitch.
- Sounds: none of its own for sight, idle, pain or death (empty sight/search). Weapon
  sounds are the borrowed ones: blaster `soldier/solatck2.wav` (+ `misc/lasfly.wav`),
  machinegun `infantry/infatck1.wav`, rocket `chick/chkatck2.wav` (+
  `weapons/rockfly.wav`); a wall unit opening plays `world/dr_short.wav`.
- **Spawnflags:** 1 Ambush, 2 Trigger_Spawn (appears when used, as 1.11), 4 Sight,
  **8 Blaster, 16 MachineGun, 32 Rocket, 64 Heatbeam, 128 WallUnit**. No weapon flag →
  blaster. Heatbeam is turned into blaster (no heatbeam turret exists in this library).
  If more than one is set: blaster is checked before machinegun before rocket when firing
  (so blaster wins), but the skin picks machinegun over rocket. [read]
- **Mounting (the angle key):** −1 = on the floor facing up (pitch 270), −2 = on the
  ceiling facing down (pitch 90), 0 / 90 / 180 / 270 = on a wall facing +X / +Y / −X /
  −Y. The origin is nudged 2 units out along its facing.
- Counts as a monster for the level total; a wall unit is added to the total at spawn.

**Q5 animations [read]** (frames 0–15)
| Name | Frames | Events |
|---|---|---|
| stand | 0–1 | — |
| ready gun (opening) | 2–8 | then run |
| run / seek | 8–9 | aims every frame |
| fire | 10–13 | fires on 10 (after aiming), aims on 11–13 |
| blind fire | 10–13 | aims on 10–12, fires on 13 |
| death | 14–15 | (not played: it explodes) |
No frame moves (all distances 0). Until it has finished opening (frame 8) it does not aim
or fire.

**Q6 attacks [read]**
- **Aim:** each aim step turns toward the enemy (eye for a player, origin for others) by
  up to 45° in yaw and in pitch. It fires only when its facing is within ~11.5° of the
  enemy's origin direction (dot ≥ 0.98).
- **Arc (sight/fire limits):** wall turrets: yaw within ±85° of the wall's facing, pitch
  within ±85°. Floor turret (facing up): any yaw, pitch at least 5° above horizontal.
  Ceiling turret (facing down): any yaw, at least 5° below horizontal. Aim beyond the arc
  is clamped to its edge.
- **Blaster:** damage 20, speed 600 easy / 800 medium / 1000 hard and nightmare, flash
  turret blaster, blaster trail.
- **Machinegun:** damage 4, no kick, default bullet spread, flash turret machinegun. One
  bullet per fire animation.
- **Rocket:** damage 50, speed 550 (hard 550–750, nightmare 650–850), flash turret
  rocket; not fired if the first thing on the line is within 72 units.
- **Lead:** within 512 units, blaster and rocket only, it leads the target by
  (distance ÷ 1000) s of its velocity with probability 50 % easy, 60 % medium, 70 % hard,
  80 % nightmare. A shot is taken only if the line to the aim point first hits the enemy
  or the world.
- **When it fires (its own attack check):** ignores range. Chance per check: rocket 10 %,
  blaster 35 %, machinegun 50 %; halved on easy, doubled on hard and nightmare; the enemy
  must be visible (an info_notnull enemy is fired at every time). Next allowed shot after
  rocket 1.8 − 0.2 × skill s, blaster 1.2 − 0.2 × skill s, machinegun 0.8 − 0.1 × skill s.
- **Blind fire:** only rocket and blaster turrets. Allowed while its delay ≤ 10 s; chance
  bands of 1.3; delay grows 3.4–7.4 s; attack-finished 0.5–2.5 s. Aims at the blind-fire
  target raised to (enemy eye + 10) if the enemy is now lower than that spot, else lowered
  to (enemy box bottom − 10); blaster at speed 1000, rockets at the rocket speed. No
  clear-shot test for blind shots.
- It never uses hint paths. Its yaw is always under its own aiming (manual steering).
  "Ignore shots" set: it does not anger monsters and is not angered by them.

**Q7 [read]**
- **Pain:** none (no reaction at all).
- **Death:** a plain explosion effect, 4 small debris pieces (debris1; speeds 1, 2, 1, 2)
  from 1 unit in front; the wall base becomes solid and undamageable and stays; the turret
  fires its targets (activator = its enemy if alive, else itself); the turret is removed.
- **Wall unit (spawnflag 128):** needs a targetname (removed otherwise); starts hidden in
  the wall, not damageable; when used it slides 32 units along its facing at speed 15
  (key `speed` overrides; same value for acceleration and deceleration), together with its
  base piece, playing `world/dr_short.wav`; when it stops it wakes as a normal turret (can
  be damaged, aims, fires).
- **turret_invisible_brain (drives a turret_breach, not a monster_turret):** keys
  `target` (the breach) and `killtarget` (a starting enemy, picked when it links); both
  required. With a targetname it waits to be used (the user becomes its enemy if it has
  none; first shot not before 3 s; using it again switches it off); without, it links on
  the next frame. Every frame it keeps an enemy (finds one like a monster; drops a dead or
  removed one), aims the breach at the centre of the enemy's box when the breach has a
  clear line, and fires the breach when it has had the enemy in sight for the reaction
  time: key `delay`, default (3 − skill) s (3 / 2 / 1 / 0 s). After each shot it waits
  reaction time + 1 s. The breach's rocket: damage 100–150, speed 550 + 50 × skill,
  radius 150 (unchanged from the base breach).

### 7.3 Daedalus (monster_daedalus)

The daedalus is the hover (icarus) monster with a different classname; everything in
2.15 applies, with these differences. [read]

**Q4 [read]**
- Model `models/monsters/hover/tris.md2`, **skin 2** (skin 3 below half health).
- Box ±24 × −24…32. Health 450 (hover 240), gib health −100, mass 225 (hover 150), turn
  rate 25°/frame (the hover keeps the flier default).
- Sounds: looping idle `daedalus/daedidle1.wav`; sight `daedalus/daedsght1.wav`; search
  `daedalus/daedsrch1.wav` / `daedalus/daedsrch2.wav` (50/50); pain
  `daedalus/daedpain1.wav` / `daedalus/daedpain2.wav`; death `daedalus/daeddeth1.wav` /
  `daedalus/daeddeth2.wav` (50/50). (`tank/tnkatck3.wav` is precached.)
- Spawnflags: the usual three only.

**Q5 [read]:** the hover's animations unchanged (2.15): wind-up 197–199, burst 200–202,
wind-down 203–204, pain1 113–140, pain2 141–152, pain3 153–161, death 162–172.

**Q6 [read]**
- Bolts are the green blaster ("blaster2" type): damage 1, speed 1000, flash "daedalus
  blaster", blaster trail, two per burst (frames 200 and 201), aimed at the enemy's eye.
- Strafing: in the shared attack check, after deciding not to fire, it strafes 80 % of the
  time (other fliers 60 %); in its own attack choice the circle-strafe chance is the
  hover's plus 0.1: 10 % easy, 60 % medium, 85 % hard, ~93 % nightmare.

**Q7 [read]**
- **Shield:** a power screen with 100 cells. It works only for hits whose point is in
  front of it (more than about 72° off its facing gets nothing); it absorbs up to a third
  of each hit, one cell per point absorbed, with screen sparks; flechette (ETF) damage
  gets only half the screen's capacity and costs double. When the cells run out the
  screen is gone. On death the screen and all effects are switched off.
- Pain and death as the hover (2.15), with its own sounds.

### 7.4 Medic commander (monster_medic_commander)

The commander is the medic (2.17, 1.6) with a different classname; differences: [read]

**Q4 [read]**
- Model `models/monsters/medic/tris.md2`, **skin 2** (skin 3 below half health).
- Box ±24 × −24…32. Health 600, gib health −130, mass 600, turn rate 40°/frame.
- Sounds (`medic_commander/…`): idle `medidle.wav`, pain `medpain1.wav` (light) /
  `medpain2.wav` (heavy), death `meddeth.wav`, sight `medsght.wav`, search `medsrch.wav`,
  hook launch `medatck2c.wav`, hook hit `medatck3a.wav`, heal `medatck4a.wav`, retract
  `medatck5a.wav`, reinforcement call `monsterspawn1.wav`. (`tank/tnkatck3.wav` is
  precached.) Footsteps `medic/step1.wav`, `medic/step2.wav` when monster footsteps are on.
- Spawnflags: the usual three only.
- It precaches every reinforcement class and the grow-effect models
  `models/items/spawngro/tris.md2` and `models/items/spawngro2/tris.md2`.

**Q5 [read]:** the medic's animations; the reinforcement call reuses the cable frames
209–236:
| Frames | Moves | Events |
|---|---|---|
| 209–216 | 2, 3, 5, 4.4, 4.7, 5, 6, 4 (charging, facing the enemy) | — |
| 217 | 0 | — |
| 218 | 0 | call sound; jumps straight to 224 (219–223 skipped) |
| 224 | 0 | decides how many and where (below) |
| 225 | 0 (charging) | if it decided to turn round, holds this frame until it faces the new way (within 0.1°); then shows the grow effects |
| 226–227 | 0 | — |
| 228 | −15 | the reinforcements appear |
| 229–236 | −1.5, −1.2, −3, −2, 0.3, 0.7, 1.2, 1.3 | — |
If no spot is found at 224 or 225 it jumps to 229 (nothing appears).

**Q6 attacks [read]**
- Blaster bolts are the green "blaster2" type (damage 2, 3 against a tesla; speed 1000,
  flash medic blaster 2); same frames as the medic.
- **Reinforcement slots:** 3 easy, 4 medium, 6 hard, 6 nightmare. Each reinforcement that
  appears uses one; one comes back whenever a reinforcement dies.
- **When it calls (its attack check, in order):**
  1. healing: as the medic, but 20 % of cable attacks become a call when it has more than
     2 slots,
  2. enemy is a player it cannot see, and more than 2 slots → "blind" state → always a
     call,
  3. 80 % chance, more than 5 slots and the enemy farther than 150 → a call is requested
     (then see 5),
  4. on medium and above, standing its ground → attack,
  5. otherwise the shared check; the attack then picks: a call with 80 % if the enemy is
     not in melee range and it has more than 2 slots, else the blaster. [derived: the
     request in 3 is overridden by this 80/20 choice, so it does not always call.]
- **How many and what (strength):** strength starts at the skill (0–3) and is changed by a
  roll: 5 % −3, 10 % −2, 15 % −1, 40 % 0, 15 % +1, 10 % +2, 5 % +3; floored at 0 (max 6).
  Count: strength 0, 1 or 2 → 1 monster; 3 or 4 → 3; 5 or 6 → 5. The *i*-th monster
  (i = 0…4) is class number (strength − 0, −2, −2, −4, −4) from the list: 0
  monster_soldier_light, 1 monster_soldier, 2 monster_soldier_ss, 3 monster_infantry, 4
  monster_gunner, 5 monster_medic, 6 monster_gladiator. So:
  | Strength | Group |
  |---|---|
  | 0 | 1 light soldier |
  | 1 | 1 soldier |
  | 2 | 1 SS soldier |
  | 3 | infantry + 2 soldiers |
  | 4 | gunner + 2 SS soldiers |
  | 5 | medic + 2 infantry + 2 soldiers |
  | 6 | gladiator + 2 gunners + 2 SS soldiers |
  [derived from the read rules; for 5 the five are medic, infantry, infantry, soldier,
  soldier.]
  Boxes used for placement: ±16 × −24…32 for all, gladiator ±32 × −24…64.
  [derived] Slots can go negative: a group of up to 5 is made with no check against the
  slots left, and each monster made takes one slot.
- **Where:** five positions relative to its facing (forward, right): (80, 0), (40, 60),
  (40, −60), (0, 80), (0, −80), each 10 units up. A position is usable if the box fits
  there (or, when blocked, if it fits when dropped from 32 units higher), with solid,
  dry, nearly flat ground within 256 units below (the four corners within 18 units of the
  middle). At frame 224 it only checks that at least one position works; if none in front,
  it tries the same positions mirrored behind it, and if one works it turns round 180°.
  Each position that works shows a grow effect (frame 225, lasting 0.3 s, model
  `models/items/spawngro2/tris.md2`, random orientation, translucent sphere effect) and at
  frame 228 the monster is created there (if it still fits).
- **Reinforcements:** do not count toward the level's kills, are "ignore shots", belong to
  the commander (slot returned on death), and attack the commander's enemy (its saved
  enemy while it is healing); in co-op each picks a random visible player, re-rolling once
  to avoid the commander's own enemy. No valid enemy → they stand.

**Q7 [read]**
- **Pain:** debounce 3 s; nightmare and healing: no animation. Damage < 35: only the light
  pain sound, no animation. Else the heavy pain sound and the long pain (frames 116–130)
  with probability min(damage × 0.005, 0.5), otherwise the short pain (108–115). Pain
  cancels the turn-round of a reinforcement call.
- Death: as the medic (gib: 2 bone, 4 meat, head2; else death frames 147–176), with its
  own death sound.

### 7.5 Carrier (monster_carrier)

**Q4. Model, box, health, sounds, spawnflags.**

| Item | Value | Mark |
| --- | --- | --- |
| Model | `models/monsters/carrier/tris.md2`, skin 0; skin 1 once health is below half of max | read |
| Box | −56 −56 −44 to 56 56 44 | read |
| Dead box | −56 −56 0 to 56 56 80, a tossed dead body (only if the death end step runs; see Q8) | read |
| Health | 2000 at skill 0 and 1, 3000 at skill 2, 4000 at skill 3; plus 500 × skill in co-op | read |
| Gib health | −200 | read |
| Mass | 1000 | read |
| Turn rate | 15 (dropped to 10 while spawning) | read |
| Movement | a flier (starts as a flying monster); keeps 104 units off the ground (§1.10) | read |
| Deathmatch | removed at spawn | read |
| Immunities | immune to laser damage; ignores shots from other monsters (no infighting from it) | read |
| Melee | none | read |
| Engine loop sound | `bosshovr/bhvengn1.wav` | read |
| Pain sounds | `carrier/pain_sm.wav` (damage < 10), `carrier/pain_md.wav` (10–29), `carrier/pain_lg.wav` (30 and up), all at no attenuation | read |
| Death | `carrier/death.wav`, no attenuation | read |
| Sight | `carrier/sight.wav`, normal attenuation | read |
| Rail | `gladiator/railgun.wav` on the weapon channel when the rail attack is chosen | read |
| Spawn | `medic_commander/monsterspawn1.wav`, no attenuation, per flier | read |
| Other content it loads | the flier's sounds and model, rocket, grenade and debris models, `weapons/rockfly.wav`, `infantry/infatck1.wav`, `gunner/gunatck3.wav`, `weapons/grenlb1b.wav`, `tank/rocket.wav`, both grow models, small metal gib and gear gib | read |
| Spawnflags | the editor comment lists Ambush (1), Trigger_Spawn (2), Sight (4), the usual monster flags; no carrier-only flag | read |
| Spawn slots | 3 at skill 0, 6 at skill 1 and 2, 9 at skill 3 | read |

A defined constant names 6 as the most fliers it may spawn, but nothing uses it: the real cap is the slots above (derived).

**Q5. Animations.** Frame numbers are 0-based model frames; the "index" is the frame's place inside its move. At 10 Hz.

| Move | Frames | Per-frame movement and events | Mark |
| --- | --- | --- | --- |
| Stand | search01–13 (0–12) | stand, 0 | read |
| Walk | search01–13 (0–12) | walk 4 each | read |
| Run | search01–13 (0–12) | run 6 each, the co-op check (Q6) every frame | read |
| Machinegun in | firea01–08 (13–20) | charge 4 each; the last frame starts the loop | read |
| Machinegun loop | firea09–11 (21–23) | charge −2, −2, then the re-attack choice | read |
| Machinegun out | firea12–15 (24–27) | charge 4 each, then run | read |
| Grenade in | fireb01–06 (28–33) | charge 4 each; the last starts the loop and its timer | read |
| Grenade loop | fireb07–10 (34–37) | −15 with one grenade, then 4, 4, then the re-attack choice | read |
| Grenade out | fireb11–16 (38–43) | charge 4 each, then run | read |
| Rocket | fireb01 (28), one frame | charge 15 and the whole rocket volley, then run | read |
| Rail | search01–09 (0–8) | 2, 2 (target saved), 2, −20 (fire), then 2 for five frames; then run | read |
| Spawn | spawn01–18 (44–61) | all −2 except index 11 (−10); see the spawn sequence below | read |
| Pain, light | spawn01–04 (44–47) | 0 each, then run | read |
| Pain, heavy | death01–10 (62–71) | 0 each, then run | read |
| Death | death01–16 (62–77) | 0 each; the explosion sequence starts on the LAST frame | read |

Spawn sequence by index (read):

| Index | What happens |
| --- | --- |
| 0–5 | both machineguns fire |
| 6 | prepare: manual steering on, timer starts, turn rate 10, machineguns |
| 7 | aim: the ideal yaw is set to the enemy's yaw −30°, 0° or +30° by which 0.5 s step of the timer this is |
| 8 | ready: the frame is held until the carrier faces the ideal yaw within 0.1°, pushing the timer on while held; then a grow effect at 105 forward and 58 down |
| 9–10 | machineguns |
| 11 | machineguns, one flier spawned, and back to index 7 until 1.1 s have passed since the timer started (three fliers); then the turn rate and steering are restored |
| 12–17 | machineguns; the last frame makes the machinegun re-attack choice |

**Q6. Attacks.**

Machineguns (read): two guns, left and right, firing together on each machinegun frame.

- Damage 6, kick 4, sideways spread three times the default bullet spread, default vertical spread.
- The left gun aims at the enemy's eyes 0.2 s behind its motion. The right gun aims 0.2 s ahead of it.
- With manual steering on (during the spawn), the lower pair of muzzle flashes is used.

Grenades (read):

- One grenade per loop, damage 50, speed 600, with the gunner's grenade flash.
- The loop repeats while the enemy is in front and less than 1.3 s have passed since it began. That gives four grenades (derived).
- Each grenade bends by which 0.4 s step of the volley it is, with a random direction (±1) picked per grenade:

| Step | Right offset | Up offset |
| --- | --- | --- |
| 0 | 0.15 × dir | 0.1 − 0.1 × dir |
| 1 | 0 | 0.1 |
| 2 | −0.15 × dir | 0.1 + 0.1 × dir |
| 3 | 0 | 0.1 |
| later | 0 | 0 |

- The aim is at the enemy's origin.
- The vertical part of the aim direction is clamped to −0.5…0.15.

Rockets (read): four rockets in one frame, with flashes rocket 1 to 4.

- At a player, half the volleys are predictive:
  - speed 750,
  - each rocket leads the player by its own time: −0.3, −0.15, 0 and +0.15 s,
  - aimed at the origin, with no eye height.
- Otherwise:
  - speed 500, damage 50,
  - rockets 1 and 4 aim 15 units low and fan sideways by ±0.4,
  - rockets 2 and 3 fan by ±0.025.
- The volley has its own 2 s cooldown, kept apart from the other attacks' timer.

Railgun (read):

- The target is the enemy's eyes, saved on the second frame of the rail move.
- Fires on the fourth frame: damage 50, kick 100.
- It then blocks the next rail for 3 s.
- When the rail is picked from far range (600 or more) with slots left, the target is saved at once as well.

Co-op check (read):

- Runs on run frames, machinegun frames and the grenade and spawn re-attack choices.
- Only in co-op, and at most once per 2 s.
- Picks at random, among living players behind the carrier (facing dot below −0.3) or under it (within 18° of straight down) with a clear line, one to fire a full rocket volley at.
- Then turns back to its real enemy.

Muzzle flash numbers (read; the monster flash table of the pack's protocol):

| Weapon | Flash |
| --- | --- |
| Machinegun, left, upper and lower | 138, 152 |
| Machinegun, right, upper and lower | 139, 153 |
| Grenade | fired from the carrier grenade muzzle (140), but sent with the gunner's grenade 1 flash (53) |
| Rockets 1–4 | 191–194 (both volley kinds; the predictive rockets also do 50 damage) |
| Railgun | 147 |

Choosing an attack (read). "Rail ready" means 3 s have passed since the last rail. "In front" means facing dot above 0.3.

| Situation | Choice |
| --- | --- |
| In a tesla bad area, enemy behind or below | rockets |
| In a tesla bad area, otherwise | machinegun if 10 % or rail not ready; else rail |
| Blind state (set when a player cannot be seen and more than 2 slots are left) | spawn |
| Enemy to the side (not in front, behind or below) | machinegun if 10 % or rail not ready; else rail |
| In front, distance ≤ 125 | machinegun if 80 % or rail not ready; else rail |
| In front, 125–600, more than 2 slots | roll ≤ 0.2 machinegun; ≤ 0.4 grenades; ≤ 0.7 and rail ready rail; else spawn |
| In front, 125–600, 2 slots or fewer | roll ≤ 0.3 machinegun; ≤ 0.65 grenades; else rail if ready, otherwise machinegun |
| In front, 600 or more, more than 2 slots | roll < 0.3 machinegun; < 0.65 and rail ready rail; else spawn |
| In front, 600 or more, 2 slots or fewer | machinegun if roll < 0.45 or rail not ready; else rail |
| Not in front but below | rockets |

Machinegun re-attack (read): if the enemy is in front, a coin flip decides.

- On one side, it keeps firing (70 %, or always with 2 slots or fewer) or else spawns.
- Otherwise, or with the enemy not in front, it plays the machinegun out frames.

When it attacks (read):

- **No clear shot at a living enemy:** if the enemy is a player and more than 2 slots are left, it goes blind and spawns. Otherwise it does not attack, except at a non-solid target with no blocking hit.
- **Enemy behind, or below and not in front:** if the 2 s rocket cooldown is over, it fires the attack choice at once. It then strafes (60 %) or flies straight.
- **Melee range:** it always attacks.
- **Otherwise:** the chance is 0.4 when holding ground, 0.8 at near and mid range, 0.5 at far. It always attacks a non-solid target. When it does not attack, it strafes (60 %) or flies straight.

Spawning (read):

- Each flier comes from a point 105 forward and 58 down, searched for a free spot 32 up. The flier's box is ±16 × −24…16.
- The flier at the 0.5 s step numbered 2 is a kamikaze. The others are ordinary fliers.
- Each costs one slot. A spawned flier is marked as the carrier's, is not counted in the level's kill total, ignores other monsters' shots, and links back to the carrier.
- When its enemy is alive:
  - step 1's flier circle-strafes one way,
  - step 2's kamikaze charges straight at mass 100,
  - step 3's flier circle-strafes the other way.
- Killing a carrier's flier gives the slot back (§1.6, read).

**Q7. Pain, death, abilities.**

Pain (read):

- The skin turns damaged below half health.
- At skill 3 there are no pain animations (the skin still changes).
- There is a 5 s debounce. Damage under 10 only plays the small pain sound.
- Damage 10–29 plays the medium sound and, 50 % of the time, the light pain move.
- 30 or more plays the large sound and the heavy pain move.
- An interrupting pain clears the frame hold and manual steering, and restores the turn rate.

Death (read):

- The death sound plays, the carrier stops taking damage, and the explosion counter resets.
- It plays the 16-frame death move.

Abilities (read): flight, spawning fliers, laser immunity, shot ignoring, and the co-op rear defence.

**Q8, carrier.** Its boss death is the same shared explosion sequence the supertank uses (read).

- From the last death frame, one explosion every 0.1 s for eight steps. Each is raised 24–39 units:
  - four at ±24 on the diagonals,
  - then four at ±48 on the diagonals.
- On the ninth step:
  - the engine sound stops,
  - it throws 4 small meat gibs, 8 small metal gibs and a chest gib, all at damage 500,
  - it becomes a gear head gib,
  - it marks itself dead.
- It fires no targets of its own beyond the usual monster death targets (read).
- The explosion takes over the entity's think before the death move can end, so the dead-box step at the end of the move does not run (derived).

### 7.6 Black widow, first stage (monster_widow)

**Q4. Model, box, health, sounds, spawnflags.**

| Item | Value | Mark |
| --- | --- | --- |
| Model | `models/monsters/blackwidow/tris.md2`; skin 1 below half health | read |
| Box | −40 −40 0 to 40 40 144 | read |
| Health | 2000 + 1000 × skill (2000, 3000, 4000, 5000); plus 500 × skill in co-op | read |
| Gib health | −5000 (in practice never gibbed: the death removes her, see Q8) | read; the reason is derived |
| Mass | 1500 | read |
| Turn rate | 30 | read |
| Power armour | skill 3 only: a power shield with 500 cells | read |
| Movement | walks (a ground monster) | read |
| Deathmatch | removed at spawn | read |
| Immunities | laser damage; ignores other monsters' shots | read |
| Pain sounds | `widow/bw1pain1.wav` (damage < 15), `widow/bw1pain2.wav` (15–74), `widow/bw1pain3.wav` (75 and up), no attenuation | read |
| Death sound | none at death; `misc/bwidowbeamout.wav` when she beams out | read |
| Search sound | `bosshovr/bhvunqv1.wav` is loaded, but her search does nothing, so it never plays | read; "never plays" derived |
| Sight | no sound; it clears her blaster timer | read |
| Stand | `widow/laugh.wav` each time she enters the stand move | read |
| Steps | `widow/bwstep3.wav` on walk and run frames 1 and 8; `widow/bwstep2.wav` on run-attack frames 1 and 5 | read |
| Rail | `gladiator/railgun.wav` when a rail attack is chosen | read |
| Content it loads | the stalker's sounds and model, `tank/tnkatck3.wav`, the laser bolt model, the small grow model, small metal and gear gibs, the four first-stage and four second-stage widow gibs, the legs model, `misc/bigtele.wav` | read |
| Spawnflags | the editor comment lists Ambush (1), Trigger_Spawn (2), Sight (4); nothing her own | read |
| Spawn slots | 3 at skill 0 and 1, 4 at skill 2, 6 at skill 3; in co-op, slots + skill × (players − 1), at most 6; recomputed before every attack choice | read |

A defined constant names 6 as the most stalkers she may spawn, but nothing uses it (derived).

**Q5. Animations.** Model frame numbers in brackets. At 10 Hz.

| Move | Frames | Movement and events | Mark |
| --- | --- | --- | --- |
| Stand | idle01–11 (0–10) | 0 | read |
| Walk and run | walk01–13 (11–23) | 2.79, 2.77, 3.53, 3.97, 4.13, 4.09, 3.84, 3.62, 3.29, 6.08, 6.94, 5.73, 2.85; a step sound on the 1st and 8th | read |
| Run attack | run01–08 (24–31) | charge 13, 11.72, 18.04, 14.58, 13, 12.12, 19.63, 11.37; a blaster bolt every frame, a step sound on the 1st and 5th; then run | read |
| Blaster in | fired01–02a (59–61) | 0; the last frame starts the blaster | read |
| Blaster | fired02a–20 (61–79) | 0; each frame fires, then picks the next frame by where the enemy is (see Q6) | read |
| Blaster out | fired21–22 (80–81) | 0; then run | read |
| Blaster out, turned right | transa01–05 (116–120) | −2, −10, −2, 0, 0; then it rejoins the run at walk12 | read |
| Blaster out, turned left | transb01–05 (121–125) | 0, 14, −2, 10, 10; then it rejoins the run at walk12 | read |
| Rail in | transc01–04 (126–129) | 0; manual steering on at the first, the rail move chosen at the last | read |
| Rail ahead | firea01–09 (32–40) | 0, 0, target saved, −10 with the shot, 0 × 4, manual steering off; then run | read |
| Rail to the right | fireb01–09 (41–49) | the same pattern | read |
| Rail to the left | firec01–09 (50–58) | the same pattern | read |
| Spawn | spawn01–18 (82–99) | 0; manual steering on at the 4th; bolts from the 6th to the 13th; grow effects at the 7th; stalkers at the 10th; manual steering off at the 18th; then run | read |
| Kick | kick01–08 (161–168) | 0; the hit on the 4th; then run | read |
| Pain, light | pain201–203 (113–115) | 0; then run | read |
| Pain, heavy | pain01–13 (100–112) | 0; then run | read |
| Death | death01–31 (130–160) | 0; the beam-out begins on the 10th; she leaves on the 31st | read |

The run move ends without a follow-up, so it loops. Only certain run frames allow an attack (read):

- An attack check is refused on walk04–08 and walk12.
- An attack started on walk13 or walk01–03 counts as "rail frames".
- An attack started on walk09–12 counts as "blaster frames".

**Q6. Attacks.**

Her bolt and rail damage is multiplied by a damage multiplier: ×1 normally, ×2 or ×4 when she answers a player's power-up (see "Power-ups" below). Every fourth bolt she fires carries the blaster trail effect. That count is shared by every widow in the level (read).

Blaster bolts (read):

- Damage 10 × multiplier, speed 1000.
- Flash numbers (read): straight ahead 175; the turret frames 100° right to 10° right are 165–174, and 10° left to 70° left are 176–182; the spawn sweep's frames are 156–164 (by spawn frame 5 to 13, so 157–164 are the ones that fire); the run attack's eight frames are 183–190.

Blaster turret mode (read):

- Each frame of the blaster move matches an aim direction relative to her facing:
  - the first frame fires straight ahead,
  - then from 100° right down to 70° left, in 10° steps.
- Each frame, the frame for the enemy's current bearing is chosen next. Each 10° band picks one frame.
- If the enemy is 105° or more to the right, she leaves through the right-turn exit. At 75° or more to the left, she leaves through the left-turn exit.
- Each bolt is aimed at the enemy's eyes with lead for a speed of 1000, plus a random −0.05…+0.05 s extra lead.
- The bolt's yaw is then clamped to within 15° of the frame's own direction.
- On entry, the blaster is given 1 to 3 s (random). The move keeps firing until that time is up, then plays the plain exit.
- The blaster may be chosen again 2 s after that time ends.
- Manual steering is on while she aims.

Run attack (read):

- Eight frames charging forward.
- Each fires a bolt straight at the enemy's eye position (no lead) from a muzzle that moves with the frame.

Spawn sweep (read): during the spawn move, bolts are fired on eight frames, the 6th to the 13th.

- Each is pitched at the enemy and turned by a set yaw: 26° right, 20° right, 10° right, 0°, 6.5° left, 13° left, 27° left, 41° left.
- So the sweep runs from her right to her left.

Rail (read):

- The rail-in move chooses the left, right or straight rail by the enemy's bearing: more than 15° to the left, more than 15° to the right, or between.
- The target (enemy eyes) is saved one frame before the shot.
- The shot: damage 50 × multiplier, kick 100, then 3 s before the next rail.
- Flashes (read): straight 150, left 154, right 155.

Kick (read):

- Reach 100 forward, 4 up. Damage 50–55.
- Kick 500 if the enemy is on the ground, 250 if it is in the air.

Spawning stalkers (read):

- Needs at least 2 slots left (slots minus stalkers alive from her).
- Two points, 30 forward and 100 to each side, 16 up. For each, a stalker-sized spot (±28 × ±18) is searched up to 64 above the point.
- The spot must have ground within 256 below.
- A small grow effect shows at each spot three frames before.
- Each stalker made adds one to her used count. It is marked hers, not counted in the kill total, and set to ignore shots.
- If her enemy is alive, the stalker takes it and attacks at once.
- A dying stalker of hers lowers her used count again (§1.6).

When she attacks (read):

1. Her answer to power-ups is checked.
2. In a run move, the refused frames above stop the check.
3. With at least 2 slots left and the enemy more than 150 away, 80 % of the time she "attacks" with the spawn bias set.
4. Without a clear line to a living enemy, she goes blind and attacks if the enemy is a player and 2 slots are left. Otherwise she does not attack, except at a non-solid target.
5. Within 100 (melee distance 80 + 20), she kicks. At skill 0 she skips 3 checks in 4.
6. Otherwise she attacks with chance:

| Situation | Chance |
| --- | --- |
| Holding ground | 0.4 |
| Melee band | 0.8 |
| Near | 0.7 |
| Mid | 0.6 |
| Far | 0.5 |

Choosing an attack (read). "Rail ready" means 3 s have passed since the last rail. "Blaster ready" means 2 s have passed since the last blaster ended. "Slots left" means slots minus used.

| Situation | Choice |
| --- | --- |
| In a tesla bad area | blaster if 10 % or rail not ready; else rail |
| Blind and 2 slots left | spawn |
| Spawn bias set and 2 slots left | spawn |
| Enemy more than 300 away, not angered, no bias, 50 % | run attack |
| Blaster frames | spawn if 2 slots left; else blaster if ready; else nothing new |
| Rail frames | rail if ready; else nothing new |
| Other frames, 2 slots left | roll ≤ 0.4 and blaster ready: blaster; ≤ 0.7 and rail ready: rail; else spawn |
| Other frames, fewer slots | rail not ready: blaster; else rail if roll ≤ 0.5 or the blaster time is within 2 s of now, else blaster |

The second test in the last row holds nearly always, so with few slots she rails whenever the rail is ready (derived).

Blocked (read):

- If she is blocked during the run attack, she is "angered" and makes an attack check at once.
- She attacks if it passes; otherwise she runs.
- Angered means the next choice skips the run attack.

Power-ups (read; shared with the second stage):

- Single player: she looks at her enemy's power-ups. In co-op, she looks for a player with invulnerability first, then quad, then double damage, and answers the first found.

| Player has | Skill 0 | Skill 1 | Skill 2 | Skill 3 |
| --- | --- | --- | --- | --- |
| Quad | nothing | ×2 for the quad's remaining time | ×4 | ×4 and power shield |
| Double damage (no quad) | nothing | nothing | ×2 | ×2 and power shield |
| Neither | multiplier back to ×1 | same | same | same |
| Invulnerability (also) | nothing | power shield | invulnerable for the player's remaining time | invulnerable and power shield |

- "Power shield" sets her armour to a power shield. If it is empty, she gains 250 × skill cells.
- With a quad at skill 0, the multiplier keeps its last value (derived).
- In co-op with no power-up found, nothing is reset (derived).
- The timers also give her the matching shell glow, and invulnerability blocks damage (read, from the shared monster and damage code).

**Q7. Pain, death, abilities.**

Pain (read):

- The skin changes below half health.
- At skill 3 there are no pain moves.
- There is a 5 s debounce.
- Damage < 15 only makes a sound.
- Damage 15–74: light pain with chance 0.6 − 0.2 × skill (0.6, 0.4, 0.2).
- Damage 75 and up: heavy pain with chance 0.75 − 0.1 × skill (0.75, 0.65, 0.55).
- A pain move clears manual steering.
- Pain also clears a held "very long" blaster timer back to zero.

Death (read):

- She stops taking damage, and all power-up timers are cleared.
- The 31-frame death move plays; see Q8 for what follows.

Abilities: stalker spawning, the power-up answer, laser immunity, shot ignoring, the turret blaster.

**Q8, first stage (read).**

- On the 10th death frame, two widow beam-out effects appear at two points on her body (about 13 forward, 44 left, 69 up, and 3 forward, 59 right, 68 up; the side offsets are along her right vector, negative meaning left), with the beam-out sound.
- On the 31st frame:
  - two widow splash effects at those points,
  - a boss-teleport effect 36 above her origin,
  - a legs entity at her place and facing,
  - she is removed.
- The legs:
  - not solid and visible to infrared goggles,
  - play their model frames one per 0.1 s up to frame 23,
  - at frame 17, an explosion at 11.77 forward, 7.24 left, 23.31 up, with small debris (2 small meat gibs and 2 small metal gibs),
  - after frame 23 they wait 1 s; 0.5 s before the end, two explosions (31 forward, 88.7 left, 10.96 up and 12.67 back, 4.39 left, 15.68 up),
  - at the end, two explosions (65.6 back, 8.44 left, 28.59 up and 1.04 back, 51.18 left, 7.04 up) with debris and large widow gibs (first-stage gib 1 and 2 at the first point, gib 1, 2 and 3 at the second, at damage 80–99), then they are removed.
- This library has no code that starts the second stage. No path creates a second-stage widow or links the two (read).
  - The hand-over must come from the map, through the first stage's ordinary death targets (fired by the shared death code when she dies) and a second-stage widow placed to spawn on trigger (derived; not settled by the source).

### 7.7 Black widow, second stage (monster_widow2)

**Q4. Model, box, health, sounds, spawnflags.**

| Item | Value | Mark |
| --- | --- | --- |
| Model | `models/monsters/blackwidow2/tris.md2`; skin 1 below half health | read |
| Box | −70 −70 0 to 70 70 144 | read |
| Health | 2800 + 1000 × skill (2800, 3800, 4800, 5800); plus 500 × skill in co-op | read |
| Gib health | −900 | read |
| Mass | 2500 | read |
| Turn rate | 30 | read |
| Power armour | skill 3 only: a power shield with 750 cells | read |
| Deathmatch | removed at spawn | read |
| Immunities | laser damage; ignores other monsters' shots | read |
| Pain sounds | `widow/bw2pain1.wav`, `bw2pain2.wav`, `bw2pain3.wav` by the same damage bands as stage one, no attenuation | read |
| Death sound | `widow/death.wav`, no attenuation | read |
| Search | `bosshovr/bhvunqv1.wav` at 50 % on each search, no attenuation | read |
| Tongue | `brain/brnatck3.wav` when the tongue hits | read |
| Content it loads | parasite pain, death, attack and sight sounds, `infantry/melee2.wav`, `misc/fhit3.wav`, `tank/tnkatck3.wav`, `weapons/disrupt.wav`, `weapons/disint2.wav`, the stalker, the small grow model, laser and disruptor projectile models, every widow gib | read |
| Spawnflags | Ambush (1), Trigger_Spawn (2), Sight (4) per the editor comment | read |
| Spawn slots | the same rule as stage one | read |

**Q5. Animations.** Model frame numbers in brackets.

| Move | Frames | Movement and events | Mark |
| --- | --- | --- | --- |
| Stand | blackwidow3 (0), one frame | 0 | read |
| Walk and run | walk01–09 (1–9) | 9.01, 7.55, 7.01, 6.66, 6.20, 5.78, 7.25, 8.37, 10.41 | read |
| Beam in | fireb01–04 (35–38) | charge 4 each; the last starts the beam | read |
| Beam loop | fireb05–09 (39–43) | 0; a beam on the first four frames, the re-attack choice on the fifth | read |
| Beam out | fireb06–07 (40–41) | charge 4; then run. The table holds three entries but the range plays two | read; "plays two" derived |
| Disruptor | firea01–07 (28–34) | 2, 2, 2 (target saved), −20 (shot), 2, 2, 2 with the repeat choice; then run | read |
| Spawn | spawn01–18 (10–27) | 0; manual steering on at the 3rd, beams on the 4th–14th, grow effects on the 10th, stalkers on the 14th, the beam re-attack choice on the 18th | read |
| Tongue | tongs01–08 (47–54) | 0; tongue on 1–3, pull on 4–6, crunch on 7, cooldown on 8; then run | read |
| Pain | pain01–05 (55–59) | 0; then run | read |
| Death | death01–44 (60–103) | 0; explosions on the way (Q8) | read |
| Dead, searching | dthsrh01–15 (104–118) | 0; repeats (Q8) | read |
| Dead, final | dthsrh16–22 (119–125) | 0; then the body settles | read |

**Q6. Attacks.**

Her weapons do not use the widow damage multiplier; their damage is fixed (derived from the fixed damage values below).

Heat beam (read):

- Each beam frame fires a heat beam: damage 10, kick 50.
- In the beam loop, each frame aims at where the enemy's origin was on the previous beam frame, 0.1 s ago, at eye height minus 10.
- Flashes (read): the beam loop's flashes 195–199 by frame fireb05 to fireb09, of which 195–198 are used since the fifth frame does not fire (derived), the sweep's eleven frames 200–210, the disruptor 148.

Spawn sweep (read):

- Eleven beams on spawn frames 4–14.
- Each is pitched at the enemy and turned by a set yaw: 40° left, then 8° steps to 40° right. So the sweep runs from her left to her right.

Disruptor (read):

- The target (enemy eyes) is saved one frame before the shot.
- At the shot, if the enemy is still within 30 of that saved point, she fires a homing tracker at it: damage 20, speed 500.
- Otherwise she fires a non-homing tracker: damage 20, speed 1200, led for that speed at eye height.
- A standing enemy that has not moved is about 22 from the saved eye point, so it gets the homing shot (derived).
- On the last frame she repeats the disruptor with chance 0.25 + 0.15 × skill (0.25, 0.40, 0.55, 0.70).
- A 3 s disruptor constant is defined, but the attack timer it would feed is never set, so the disruptor has no cooldown (derived).

Tongue (read):

- **When it can start:** the tongue cooldown is over, the enemy is closer than 300, and the tongue's start point near her mouth (about 17 forward, 69 up) reaches the enemy within 256 at a pitch of 30° or less.
  - At skill 0, 3 checks in 4 are refused.
- **Tongue frames (1–3):**
  - Reach and pitch are tried at the enemy's origin, then 8 below its top, then 8 above its bottom.
  - A shot trace to the origin must hit the enemy.
  - On a hit: the tongue sound, a parasite-style tongue beam effect, and 2 damage with no knockback.
- **Pull frames (4–6):** while in reach (no trace needed), a grounded enemy is lifted 1 unit off the ground.
  - A player gets 1000 units/s added toward her.
  - A monster is turned toward her and given a speed of 1000 along her own forward direction.
- **Crunch (7):** one more pull, then a hit with reach 150 forward, 4 up, damage 20–25, kick 500 on the ground or 250 in the air.
  - The code also has a zero-kick case for other frames, but the crunch only runs on this frame, so that case is never used (derived).
- **Last frame (8):** the tongue waits 3 s before it can be used again.

Spawning stalkers (read):

- The same rules as stage one, except the points are 30 forward and 135 to each side at 0 up.
- In co-op, each stalker is given a random target player, preferring one other than her own enemy: a second pick is made if the first is her enemy. Her enemy is the fallback.

When she attacks (read):

1. Her power-up answer is checked.
2. The same 80 % spawn bias (2 slots left, enemy more than 150 away).
3. The same clear-line and blind rule.
4. The tongue, if its conditions hold.
5. Otherwise she attacks with chance:

| Situation | Chance |
| --- | --- |
| Holding ground | 0.4 |
| Near | 0.8 |
| Mid | 0.8 |
| Far | 0.5 |
| Melee band | 0 (no value is set for it, so it stays 0; derived) |

Choosing an attack (read):

| Situation | Choice |
| --- | --- |
| In a tesla bad area | beam 75 %, else disruptor |
| Blind and 2 slots left | spawn |
| Spawn bias and 2 slots left | spawn |
| Closer than 600, 2 slots left | roll ≤ 0.4 beam; ≤ 0.7 disruptor; else spawn |
| Closer than 600, fewer slots | roll ≤ 0.5 beam; else disruptor |
| 600 or farther, 2 slots left | roll < 0.3 beam; < 0.65 spawn; else disruptor |
| 600 or farther, fewer slots | roll < 0.45 beam; else disruptor |

Beam re-attack (read):

- Manual steering is cleared.
- If the enemy is in front, a coin flip decides:
  - On one side, she beams again (70 %, or always with fewer than 2 slots) or spawns.
  - On the other side, or with the enemy not in front, she plays the beam out frames.
- The spawn move ends on this choice too, so a spawn can lead straight into a beam or another spawn.

**Q7. Pain, death, abilities.**

Pain (read): the same rules as stage one (skin, skill 3, 5 s debounce, the bands and chances), with one pain move.

Death (read):

- The death sound plays and she stops taking damage.
- **Every living monster in the level with the stalker's class name is killed**, hers or not.
- Her power-up timers are cleared.
- The 44-frame death move plays.

Gibbing (read):

- Once dead, she can still be gibbed: after the final settle she takes damage again, and health at or below −900 gibs her.
- On a gib:
  - the gib sound plays (`misc/udeath.wav`),
  - she throws 2 bone gibs and 3 small meat gibs,
  - 3 each of second-stage gib 1 and gib 2 (gib 2 with the `misc/fhit3.wav` hit sound),
  - 2 each of second-stage gib 3 and first-stage gib 3,
  - a chest gib, and she becomes a head gib.
  - All use the damage clipped to 100. None of these fade early.

Abilities: stalker spawning, the power-up answer (it changes only her shell, shield and invulnerability, since her damage is fixed), laser immunity, shot ignoring, the tongue pull.

**Q8, second stage (read).**

Death frames, counted from 1:

| Frame | Event |
| --- | --- |
| 3 | explosion at 23.74 forward, 37.67 left, 76.96 up |
| 6 | explosion at 20.49 back, 36.92 right, 73.52 up |
| 18 | explosion at 2.11 forward, 0.05 left, 92.20 up |
| 25 | explosion at 28.04 back, 35.57 left, 77.56 below the origin |
| 29 | explosion at 20.11 back, 1.11 left, 40.76 up |
| 30 | the leg: a big explosion at 31.89 back, 47.86 left, 67.02 up throwing second-stage gib 2 (damage 200, hit sound); an explosion at 44.9 back, 82.14 left, 54.72 up throwing second-stage gib 1 (damage 300, hit sound); each with a small meat gib (300) and a small metal gib (100) |
| 34 | explosion at the frame-29 point |
| 38 | explosion at the frame-29 point |
| 44 | the final explosion chain starts |

- Each plain explosion throws one small meat gib (damage 300), one small metal gib (100) and two small metal gibs (300).
- Final chain: one step every 0.1 s, thirteen steps. Each is placed 24–39 above the origin, plus another 24–55 for the first eight.

| Step | Offset in world x and y (not turned with her) | Extra |
| --- | --- | --- |
| 1 | −24, −24 | |
| 2 | +24, +24 | small debris |
| 3 | +24, −24 | |
| 4 | −24, +24 | more debris |
| 5 | −48, −48 | |
| 6 | +48, +48 | arm 1: a big explosion at 65.76 forward, 17.52 right, 7.56 up, 2 small metal gibs |
| 7 | −48, +48 | arm 2: second-stage gib 4 (damage 200, hit sound) and a small meat gib at the arm point |
| 8 | +48, −48 | small debris |
| 9–12 | ±18 on the diagonals, at 48 above the origin | step 9 adds more debris; these four are big explosions |
| 13 | at the origin + 24–39 | engine sound off; 1 small meat gib (400), 2 small metal (100), 2 small metal (400), fading in 5–15 s; she switches to the dead move |

- Steps 1–8 alternate between the normal explosion and the no-particle explosion. Step 13 still writes a normal explosion after the change to the dead move (derived).
- "Small debris" is 2 small meat gibs (300), 1 small metal gib (300) and 1 small metal gib (100).
- "More debris" is 1 small meat gib (300), 2 small metal (300) and 3 small metal (100), plus a set of small debris in co-op.
- The dead move: dthsrh01–15 once, then dthsrh02–15 three more times (the loop jumps to the first frame, whose counter reset is then skipped; derived), then dthsrh16–22.
- At the end: the box becomes −70 −70 0 to 70 70 80, the body is tossed, takes damage again and stops thinking.
- No targets are fired beyond the ordinary monster death targets at the moment she dies (read). The level end belongs to the map (not settled by the source).

Widow gib physics (read; used by both stages and the legs):

- **Speed:** the gib takes the parent's velocity plus a random push. The horizontal part is random within ± the damage; the vertical part is random within ± the damage, plus 200. Meat gibs get half of that push; metal gibs get all of it.
- **Clipping:** horizontal speed is clipped to ±300 and vertical speed is raised to at least 200.
- **Large widow gibs ("sized"):**
  - solid boxes: second-stage gib 2 is ±10 × 0–10; the others are ±5 × 0–5,
  - spin up to 400, 400 and 200,
  - horizontal speed doubled, then clipped again,
  - upward speed at least 350–450,
  - gravity 0.25,
  - on touching anything they stop spinning, turn non-solid and play their hit sound.
- **Small gibs:** horizontal speed doubled after the clip, spin up to 600.
- **Lifetimes:**

| Gib | Fading | Not fading |
| --- | --- | --- |
| Large | 20–35 s | 60–75 s |
| Small | 5–15 s | 25–35 s |
