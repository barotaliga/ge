# The sequel's other monsters, specified

What the full sequel's other single-player monsters do, and how every monster steps, as rules and
numbers in our words (README.md, the rules), for M18v7d3; it continues `sequel-monsters.md` (life,
senses, the attack choice, the demo's roster), which it does not repeat. **Where these facts came
from**: a clean-room research session that read the sequel's GPL game library at its GPL release
and answered written questions; each fact is marked **[R]** read, **[D]** derived from read facts
(the working given) or **[M]** measured (none yet). As with the demo's roster, **exact parity is
not the goal**: the builder designs these monsters (on `engine/ai`, with free or the pack's own
models and sounds) and holds them to these numbers and rules in the `se-sequel` lab. Animation
frame names (`attak101`) are the pack's models' own. Units: map units, seconds, a 0.1 s frame (10
Hz), speeds in units per second, angles in degrees; melee range is under 80 units origin to origin,
near under 500, mid under 1000.

## Shared rules used below

- **A melee blow** (every claw, club, bite and slash below) [R]: it lands only when the enemy's
  origin is within the blow's reach (80 for every monster below) of the attacker's origin, and a
  line from the attacker's origin toward the enemy is not stopped by something that takes no
  damage (anything else that takes damage in the way is struck instead, and if that is a monster
  or a player the enemy is struck). The damage is dealt with no ordinary knockback; instead the
  enemy's velocity gets the blow's **kick** added directly (not divided by mass) along the line
  from the blow's point to the enemy's centre, and if that leaves it moving upward it leaves the
  ground. A **negative kick pulls** the enemy toward the attacker. A blow that misses does nothing.
- **Monster projectiles** [R]: a monster's rocket has splash damage equal to its direct damage,
  over a radius of the direct damage plus 20 (so a 50 rocket splashes 50 within 70). A monster's
  grenade has a 2.5 s fuse and a radius of its damage plus 40. A monster's rail and BFG are the
  player's (the BFG ball's own impact and its lasers are the player weapon's; the monster's numbers
  set its speed and its final burst's strength and reach, see the bosses). A monster's bullet is
  one hitscan shot with the given spread (horizontal × vertical at 8192 units).
- **Aim** [R]: unless said otherwise a shot aims at the enemy's eye (its origin plus its view
  height) at the moment of the frame that fires it.
- **Pain** [R]: every monster's pain rule starts by showing the damaged skin (skin 1) when health is
  below half its maximum, then ignores the hit while its 3 s pain wait is running, then sets the
  wait to 3 s. At skill 3 no pain animation plays (a few play the sound first, noted below), and
  after any hit at skill 3 the wait is 5 s. A monster that is ducked (see ducking) gets no pain
  call at all, so it neither flinches nor switches to its damaged skin while ducked; a hit fully
  absorbed by a power screen (zero health taken) gets no pain call either.
- **Ducking** [R]: a dodge (offered when a player's blaster, rocket or BFG is aimed at it; see the
  first note) starts a duck animation. Ducking lowers the box's top by 32, makes the monster no
  longer an aim-assist target, holds the low frame until a time (below) passes, then raises the box
  back.
- **Corpse box** [R]: unless said otherwise a finished death leaves a corpse box of (−16, −16, −24)
  to (16, 16, −8), tossed (falls under gravity), stops thinking, and is shot through except by
  shots that ask for corpses.
- **Standard gibs** [R]: a death at or below gib health plays `misc/udeath.wav` and throws 2
  `models/objects/gibs/bone/tris.md2`, 4 `models/objects/gibs/sm_meat/tris.md2` and a head
  `models/objects/gibs/head2/tris.md2` (all organic), and the monster is gone. Exceptions are noted.
- **Boss explosion** [R]: used by the super tank, the hornet and Jorg at the end of their death
  animation. On that frame and every 0.1 s after, eight explosion effects go off around the hulk
  (at ±24 then ±48 units on both ground axes, 24 to 39 units up); they are effects only and do no
  damage. On the ninth step (0.8 s after the first) its looping sound stops and it throws 4
  `sm_meat`, 8 `models/objects/gibs/sm_metal/tris.md2` (metallic), 1
  `models/objects/gibs/chest/tris.md2` and becomes a `models/objects/gibs/gear/tris.md2` head, all
  thrown as if by 500 damage. No corpse remains.
- Every class below except `monster_boss3_stand` and `monster_commander_body` is removed in
  deathmatch, has the spawn flags 1 ambush, 2 triggered spawn, 4 sight, and the life, senses and
  attack choice of `sequel-monsters.md` unless a different rule is given [R].

## The classes

### The numbers, one row per monster [R]

The box is the one the game uses; where the map editor's box for the class differs, it is given
after the slash. Speeds are the average distance per animation frame of the walk and run loops
(derived: the sum of the loop's per-frame distances over its frame count; all these classes move
their frame distances at scale 1). Turn rate: walkers 20 degrees a frame, fliers and swimmers 10.

| Class | Model | Moves | Health | Gib at | Mass | Box | Walk / run per frame |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `monster_medic` | `models/monsters/medic/tris.md2` | walks | 300 | −130 | 400 | (−24,−24,−24) to (24,24,32) / ±16 | 9.26 / 24.82 |
| `monster_brain` | `models/monsters/brain/tris.md2` | walks | 300 | −150 | 400 | (−16,−16,−24) to (16,16,32) | 2.0 / 2.27 |
| `monster_chick` | `models/monsters/bitch/tris.md2` | walks | 175 | −70 | 200 | (−16,−16,0) to (16,16,56) / (−16,−16,−24) to (16,16,32) | 7.5 / 7.5 |
| `monster_gladiator` | `models/monsters/gladiatr/tris.md2` | walks | 400 | −175 | 400 | (−32,−32,−24) to (32,32,64) | 5.5 / 16.17 |
| `monster_mutant` | `models/monsters/mutant/tris.md2` | walks | 300 | −120 | 300 | (−32,−32,−24) to (32,32,48) / top 32 | 7.5 / 22.67 |
| `monster_floater` | `models/monsters/float/tris.md2` | flies | 200 | none: explodes | 300 | (−24,−24,−24) to (24,24,32) / ±16 | 5 / 13 |
| `monster_hover` | `models/monsters/hover/tris.md2` | flies | 240 | −100 | 150 | (−24,−24,−24) to (24,24,32) / ±16 | 4 / 10 |
| `monster_flipper` | `models/monsters/flipper/tris.md2` | swims | 50 | −30 | 100 | (−16,−16,0) to (16,16,32) / (−16,−16,−24) to (16,16,32) | 4 / 24 |
| `monster_supertank` | `models/monsters/boss1/tris.md2` | walks | 1500 | none: boss explosion | 800 | (−64,−64,0) to (64,64,112) / top 72 | 4 / 12 |
| `monster_boss2` | `models/monsters/boss2/tris.md2` | flies | 2000 | none: boss explosion | 1000 | (−56,−56,0) to (56,56,80) | 8 / 8 |
| `monster_jorg` | `models/monsters/boss3/rider/tris.md2` plus `models/monsters/boss3/jorg/tris.md2` as a second model | walks | 3000 | none: boss explosion | 1000 | (−80,−80,0) to (80,80,140) / maxs (90,90,140) | 7.64 / 7.64 |
| `monster_makron` | `models/monsters/boss3/rider/tris.md2` | walks | 3000 | −2000 | 500 | (−30,−30,0) to (30,30,90) | 8.4 / 8.4 |
| `monster_boss3_stand` | `models/monsters/boss3/rider/tris.md2` | none | — | — | — | (−32,−32,0) to (32,32,90) | — |
| `monster_commander_body` | `models/monsters/commandr/tris.md2` | none | — | — | — | (−32,−32,0) to (32,32,48) | — |

Run-start and other partial animations are given in the prose. Who has which attack slot [R]: the
medic, chick, gladiator, floater have both a ranged and a melee attack; the brain and the flipper
have melee only (they never attack beyond melee range); the hover, super tank, hornet, Jorg and
Makron have ranged only (in melee range they use the ranged attack); the mutant has a melee and a
jump. Dodging: the medic, brain and chick dodge; no other class here does.

### `monster_medic` (the medic)

- **Sounds** [R]: idle `medic/idle.wav`, pain `medic/medpain1.wav` and `medic/medpain2.wav`, death
  `medic/meddeth1.wav`, sight `medic/medsght1.wav`, search `medic/medsrch1.wav`, the cable's launch,
  hit, heal and retract `medic/medatck2.wav` to `medic/medatck5.wav` in that
  order; `medic/medatck1.wav` is precached and never played.
- **The blaster** (its ranged attack; also used in melee range, since it has no melee) [R]: a
  14-frame animation (`attack1` to `attack14`) that steps toward the enemy 5, 5, 3, 2 units on its
  frames 2 to 5, fires a blaster bolt of **2 damage at 1000** on frames 9 and 12 (with the blaster's
  glowing trail), and on frame 14, when the enemy is visible, goes on 95 times in 100 into the
  rapid fire: 16 frames (`attack15` to `attack30`) firing one bolt of 2 at 1000 on each of the last
  12 (frames `attack19` to `attack30`; four of them, `attack19`, `22`, `25`, `28`, carry the
  hyperblaster's trail, the rest none). So one attack is 2 bolts, or 2 + 12 = 14 bolts (28 damage
  if all hit) [D]. It then runs.
- **Finding a patient** [R]: a dead monster is a candidate when it is a monster within 1024 units of
  the medic, not on the good side, not already claimed by a medic (nor killed by a medic: a monster
  a medic kills is marked as that medic's, so no medic heals it), dead (health 0 or below), has
  finished its death animation (it no longer thinks), and is visible to the medic (eye to eye).
  Among candidates it picks the one with the **highest maximum health**; on a tie the first found
  is kept. It looks: on the first frame of each standing loop (its idle), when its search sound
  plays while walking (only when it has no remembered old enemy), and every time it is set to
  running while not already on a patient. On finding one it remembers its current enemy as its old
  enemy (not from idle, which has none), claims the corpse, makes the corpse its enemy and goes
  after it like a found target.
- **Healing** [R]: while it has a patient its attack test always says "attack" and starts the cable
  at once whenever the corpse is visible, at any distance. The cable animation is 28 frames
  (`attack33` to `attack60`, 2.8 s): it steps 2, 3, 5, 4.4, then turns to the patient and steps
  4.7, 5, 6, 4, holds, launches the hook on frame 10 (`attack42`, sound), then on frames 11 to 19
  (`attack43` to `attack51`) tries the cable; frame 20 (`attack52`) retracts it (sound; the patient's
  red glow ends), then it steps back 15, 1.5, 1.2, 3, 2 and forward 0.3, 0.7, 1.2, 1.3 and runs.
  Each cable frame works only when: the patient still exists; the distance from the cable's start
  (a point 33 to 48 units ahead of the medic's origin, 9 to 20 units to its left, 10 to 16 up,
  moving with the frame) to the patient's origin is at most **256**; that line's pitch is within
  **45 degrees** of level; and a shot-line along it is clear or ends on the patient. On a working
  frame a cable beam is drawn from 8 units further along the medic's facing to the middle height
  of the patient's box. Frame `attack43`: the hit sound on the patient and the patient glows red
  (a red shell). Frame `attack44`: the heal sound. Frame **`attack50`, 1.7 s after the cable
  animation began**: the patient is **revived** (below). A check that fails on `attack50` means no
  revive in that cycle.
- **Approach when far** [D]: because the cable starts at any distance whenever the corpse is seen,
  a medic that sees a corpse beyond the cable's 256 reach repeats the cable animation instead of
  running, creeping about 15 units nearer per 2.8 s cycle (the sum of that animation's steps:
  34.1 forward, 22.7 back, 3.5 forward).
- **The revival** [R]: the corpse is spawned again as its own class: its spawn flags, its path
  target, its target name, its combat point and its death target are all cleared first, so it
  comes back with none of them (not ambushing, not following a path). It gets its class's full
  spawn health, skin 0, its living box, and is counted again in the level's monster total (its
  earlier kill stays counted, so total and killed both stay one apart from before) [D]. It starts
  thinking at once (not dropped to the floor: that happens only in the level's first second) and
  glows red until the medic's retract frame. If the medic's remembered old enemy is a player, the
  revived monster takes that player as its enemy at once. The item it carried was dropped at its
  death and is not given back.
- **After healing** [R]: once its patient is alive again the medic drops it; if its old enemy is
  alive it hunts it, else it returns to its path or stands.
- **Patients that can never be claimed** [D]: a corpse must have stopped thinking. The infantry and
  the mutant corpses keep thinking until flies have settled on them and left (each frame a coin
  decides whether flies come; once they do, they arrive 5 to 15 s later and leave 60 s after that),
  so they become revivable only about 65 to 75 s or more after death; an infantry or mutant corpse
  lying in water never gets flies and so is never revivable. Gibbed monsters are gone and cannot
  be revived.
- **Dodge** [R]: one time in four; it takes the attacker as its enemy if it had none, and plays a
  16-frame duck stepping back 1 unit a frame: down on frame 3 (held low for 1 s), up on frame 7.
- **Pain** [R]: one of two animations (8 or 15 frames) with its sound, at even odds.
- **Death** [R]: if it had a claimed patient, the patient is released for other medics. Standard
  gibs; otherwise a 30-frame death and the standard corpse box.

### `monster_brain` (the brains)

- **Sounds** [R]: chest open `brain/brnatck1.wav`, tentacles out `brain/brnatck2.wav` (precached,
  played by no rule), tentacles back `brain/brnatck3.wav`, death `brain/brndeth1.wav`, idle
  `brain/brnidle1.wav`, `brain/brnidle2.wav` and `brain/brnlens1.wav`, pain `brain/brnpain1.wav`
  and `brain/brnpain2.wav`, sight `brain/brnsght1.wav`, search `brain/brnsrch1.wav`, claw swings
  `brain/melee1.wav` and `brain/melee2.wav`, claw hit `brain/melee3.wav`.
- **Melee only** [R]: it has no ranged attack, so it attacks only in melee range; there it picks
  one of two at even odds.
- **The claws** [R]: an 18-frame animation (`attak101` to `attak118`) that turns to the enemy while
  stepping (8, 3, 5, 0, −3, 0, −5, −7, 0, 6, 1, 2, −3, 6, −1, −3, 2, −11): swing right sound on
  frame 5, the right claw on frame 8, swing left on 10, the left claw on 12. Each claw: reach 80,
  **15 to 19 damage, kick 40**; the hit sound plays on a hit.
- **The tentacles** [R]: a 17-frame animation (`attak201` to `attak217`): on frame 5 the chest opens
  (sound) and its **power screen goes off**; on frame 7 the tentacles strike: reach 80, **10 to 14
  damage, kick −600** (a pull of 600 units per second toward the brain), and the retract sound plays
  whether or not it hit; on frame 11 the chest closes and the screen comes back. If the tentacles
  hit at skill 1 or above, closing the chest goes straight into the claws (the 18-frame
  animation above), so a grab is followed by both claws.
- **The power screen** [R]: 100 points. It covers only hits whose point is in front of the brain
  (the direction from its origin to the hit point makes a cosine above 0.3 with its facing). A
  covered hit has a third of its damage (rounded down) taken by the screen instead of health, up
  to the points left, one point per damage absorbed; the screen's sparks show and the screen
  effect is drawn for 0.2 s. Points are never restored [R: nothing restores them]. Damage marked
  as ignoring armour passes it. The screen is turned back on every time the brain starts running,
  and off at death.
- **Dodge** [R]: one time in four; it takes the attacker as enemy if it had none, then an 8-frame
  duck (steps 0, −2, 17, −3, −1, −5, −6, −6): down on frame 2, held low until the projectile's
  time to impact plus 0.5 s has passed, up on frame 5.
- **Pain** [R]: one of three animations at about equal odds (21, 8 or 6 frames), with pain sound 1,
  2 or 1.
- **Walk and run** [R]: both use the same 11-frame walk (`walk101` to `walk111`); walk steps 7, 2,
  3, 3, 1, 0, 0, 9, −4, −1, 2; run 9, 2, 3, 3, 1, 0, 0, 10, −4, −1, 2. A second 40-frame walk exists
  in the model and is not used.
- **Death** [R]: standard gibs; otherwise an 18- or 5-frame death at even odds and the standard
  corpse box.

### `monster_chick` (the iron maiden)

- **Sounds** [R]: rocket prelaunch `chick/chkatck1.wav`, launch `chick/chkatck2.wav` (precached,
  not played by the library), slash swing `chick/chkatck3.wav`, slash hit `chick/chkatck4.wav`
  (precached, not played), reload `chick/chkatck5.wav`, death `chick/chkdeth1.wav` and
  `chick/chkdeth2.wav`, fall `chick/chkfall1.wav` (precached, not played), idle
  `chick/chkidle1.wav` and `chick/chkidle2.wav`, pain `chick/chkpain1.wav` to
  `chick/chkpain3.wav`, sight `chick/chksght1.wav`, search `chick/chksrch1.wav` (precached; it has
  no search rule).
- **Rockets** (ranged) [R]: a 13-frame wind-up (`attak101` to `attak113`, prelaunch sound on frame
  1, small steps), then the 14-frame firing loop (`attak114` to `attak127`): a **rocket of 50 at
  500 units per second** on its first frame, reload sound on frame 8, and at its end it loops again
  60 times in 100 while the enemy is alive, beyond melee range and visible; otherwise a 5-frame
  wind-down. So rockets come one every 1.4 s while it keeps firing [D].
- **The slash** (melee) [R]: a 3-frame start (`attak201` to `attak203`), then the 9-frame slash
  (`attak204` to `attak212`): swing sound and a blow on frame 2: reach 80, **10 to 15 damage, kick
  100**; at its end, if the enemy is alive and still in melee range, it slashes again 9 times in
  10; else a 4-frame end.
- **Dodge** [R]: one time in four; a 7-frame duck: down on frame 1 (held low for 1 s), up on frame
  5.
- **Pain** [R]: the sound always plays (one of three at about equal odds) before the skill-3
  check; then the animation by the damage of that hit: up to 10, a 5-frame flinch; up to 25, another
  5-frame flinch; more, a 21-frame stagger.
- **Running** [R]: from walking it runs at once; otherwise a 10-frame start (steps 1, 0, 0, −1,
  −1, 0, 1, 3, 6, 3) before the run loop. Standing, at the end of each 30-frame stand it fidgets
  three times in ten (a 30-frame idle with an idle sound), unless holding its ground.
- **Death** [R]: standard gibs; otherwise one of two deaths at even odds (12 frames with death
  sound 1, 23 frames with death sound 2) and a corpse box of (−16, −16, 0) to (16, 16, 16).

### `monster_gladiator` (the gladiator)

- **Sounds** [R]: pain `gladiator/pain.wav` and `gladiator/gldpain2.wav`, death
  `gladiator/glddeth2.wav`, railgun `gladiator/railgun.wav`, cleaver swing `gladiator/melee1.wav`,
  hit `gladiator/melee2.wav`, miss `gladiator/melee3.wav`, idle `gladiator/gldidle1.wav`, search
  `gladiator/gldsrch1.wav`, sight `gladiator/sight.wav`.
- **The railgun** (ranged) [R]: when chosen, if the enemy is within **112 units** (melee range plus
  32) it does nothing (a safe zone: no shot, keeps running); otherwise it plays the railgun sound
  and **remembers the enemy's eye position at that moment**, then a 9-frame animation (`attack1` to
  `attack9`) fires on frame 4, 0.3 s later, a rail **of 50 damage, kick 100** toward the remembered
  point (so a player who moves in those 0.3 s dodges it) [D for the dodge].
- **The cleaver** (melee) [R]: a 17-frame animation (`melee1` to `melee17`), facing the enemy: swing
  sound on frames 5 and 11, a blow on frames 7 and 14: reach 80, **20 to 24 damage, kick 300**; hit
  or miss sound accordingly.
- **No dodge, no duck** [R].
- **Pain** [R]: during the pain wait, if it is rising faster than 100 units per second while in its
  ground flinch, it switches to the air flinch; otherwise ignored. A new pain: a sound at even odds
  (played before the skill-3 check), then the 7-frame air flinch when rising faster than 100, else
  the 6-frame ground flinch.
- **Death** [R]: standard gibs; otherwise a 22-frame death and the standard corpse box.

### `monster_mutant` (the mutant)

- **Sounds** [R]: swing `mutant/mutatck1.wav`, hits `mutant/mutatck2.wav` and `mutant/mutatck3.wav`,
  death `mutant/mutdeth1.wav`, idle `mutant/mutidle1.wav`, pain `mutant/mutpain1.wav` and
  `mutant/mutpain2.wav`, sight `mutant/mutsght1.wav` (also its jump cry), search
  `mutant/mutsrch1.wav`, footsteps `mutant/step1.wav` to `mutant/step3.wav` (frames 2 and 4 of the
  run), landing `mutant/thud1.wav`.
- **Its own attack test** [R]: with a living enemy in sight (after the usual sight check, but with no
  clear-shot line and no attack wait), in melee range it always claws (no easy-skill skip); else it
  jumps when the jump test passes; else nothing.
- **The claws** [R]: a 7-frame animation (`attack09` to `attack15`), facing the enemy: a blow on
  frame 3 (left) and frame 6 (right): reach 80, **10 to 14 damage, kick 100**; hit sound or swing
  sound on a miss. On frame 7 it claws again (from `attack09`) when the enemy is alive and in melee
  range, or, at skill 3, one time in two regardless.
- **The jump test** [R]: the heights must overlap: the mutant's bottom not above the enemy's bottom
  plus three quarters of its height, and its top not below the enemy's bottom plus a quarter of its
  height. The level distance between origins must be at least 100; at exactly 100 it jumps, beyond
  100 only one test in ten passes.
- **The jump** [R]: an 8-frame animation (`attack01` to `attack08`) that charges forward 17, 15,
  15, 15 on frames 2 to 5. On frame 3 it cries (sight sound), rises 1 unit and leaves the ground
  with **600 units per second forward and 250 up**, counts as ducked (so no pain while airborne),
  and sets its attack wait 3 s ahead. On touching something that takes damage while moving faster
  than 400 units per second, it deals **40 to 50 damage with knockback equal to the damage** in its
  direction of travel. A touch on ground that is not a full footing (its floor test fails) restarts
  the jump from frame 2 (so it jumps again); any other touch ends the touch check. On frame 5: if
  on the ground, the thud sound, the attack wait clears and it is no longer ducked; if still in the
  air and more than 3 s have passed since take-off, it restarts the jump from frame 2, else it
  holds on frame 5 until it lands.
- **No dodge** [R].
- **Pain** [R]: one of three flinches at about equal odds (5, 6, 11 frames; the 6- and 11-frame ones
  first throw it back 24 and 22), with pain sound 1, 2 or 1.
- **Walking** [R]: a 4-frame start (5, 5, −2, 1), then the 12-frame walk. Standing idle plays a
  13-frame idle that loops part of itself three times in four.
- **Death** [R]: standard gibs; otherwise skin 1 (the damaged skin) at once, a 9- or 10-frame death
  at even odds, the standard corpse box, then flies may settle on the corpse (see the medic).

### `monster_floater` (the technician)

- **Sounds** [R]: attack `floater/fltatck2.wav` (the zap) and `floater/fltatck3.wav` (the whack),
  `floater/fltatck1.wav` precached and not played, death `floater/fltdeth1.wav`, idle
  `floater/fltidle1.wav`, pain `floater/fltpain1.wav` and `floater/fltpain2.wav`, sight
  `floater/fltsght1.wav`, and a constant looping sound `floater/fltsrch1.wav` it carries always.
- **Flies** [R]: it starts at either of two 52-frame hovering loops at even odds; walking and
  running reuse the first loop at 5 and 13 units a frame.
- **The blaster** (ranged) [R]: a 14-frame animation (`attak101` to `attak114`), facing the enemy:
  one bolt of **1 damage at 1000** on each of frames 4 to 10 (7 bolts; frames `attak104` and
  `attak107` carry the hyperblaster trail).
- **Melee** [R]: at even odds the zap or the whack.
- **The zap** [R]: a 34-frame animation (`attak301` to `attak334`). On frame 9 it sounds, throws
  sparks from a point 18.5 ahead and 10 up, and deals **5 to 10 energy damage** with a knockback of
  −10 straight to the enemy, wherever the enemy is at that moment (no reach or line check).
- **The whack** [R]: a 25-frame animation (`attak201` to `attak225`). On frame 12 a blow straight
  ahead: reach 80, **5 to 10 damage, kick −50** (a small pull).
- **No dodge** [R].
- **Pain** [R]: one time in three the 7-frame flinch with pain sound 1, else the 8-frame flinch with
  pain sound 2. A third 12-frame flinch and a 31-frame activation animation exist and are unused.
- **Death** [R]: always: the death sound and it becomes an explosion effect at once (no damage, no
  corpse, no gibs; gib health plays no part).

### `monster_hover` (the icarus)

- **Sounds** [R]: pain `hover/hovpain1.wav` and `hover/hovpain2.wav`, death `hover/hovdeth1.wav`
  and `hover/hovdeth2.wav`, sight `hover/hovsght1.wav`, search `hover/hovsrch1.wav` and
  `hover/hovsrch2.wav` (even odds), `hover/hovatck1.wav` precached not played, and a constant
  looping sound `hover/hovidle1.wav`.
- **Flies** [R]: walk and run reuse a 35-frame forward loop at 4 and 10 units a frame.
- **The blaster** (ranged, also used in melee range) [R]: a 3-frame start (`attak101` to
  `attak103`, 1 unit forward each), then a 3-frame loop (`attak104` to `attak106`) that backs off
  10, 10, 0 while facing the enemy and fires one bolt of **1 damage at 1000** on each of its first
  two frames (the first with the hyperblaster trail); at the loop's end it fires again 60 times in
  100 while the enemy is alive and visible, else a 2-frame end. So two bolts every 0.3 s while it
  keeps firing, backing away 20 units each loop [D].
- **No melee, no dodge** [R].
- **Pain** [R]: hits up to 25: at even odds a 9-frame flinch (pain sound 1) or a 12-frame flinch
  (pain sound 2); harder hits: a 28-frame stagger (pain sound 1).
- **Death** [R]: at or below −100: `misc/udeath.wav`, 2 `bone`, 2 `sm_meat` and an `sm_meat` as the
  head (organic). Otherwise a death sound at even odds, an 11-frame death, then the standard corpse
  box, tossed (it falls), and it **explodes** (an effect, no damage, no corpse left) as soon as it
  is on the ground or 15 s after the death animation ended, whichever comes first.

### `monster_flipper` (the barracuda shark)

- **Sounds** [R]: pain `flipper/flppain1.wav` and `flipper/flppain2.wav`, death
  `flipper/flpdeth1.wav`, the bite's chomp `flipper/flpatck1.wav`, `flipper/flpatck2.wav` precached
  not played, idle `flipper/flpidle1.wav` (precached, no idle rule), search `flipper/flpsrch1.wav`
  (precached, no search rule), sight `flipper/flpsght1.wav`.
- **Swims** [R]: eye 10 above the origin, turn rate 10. Standing is a single still frame; walking a
  24-frame loop at 4 units a frame; running: a 6-frame start at 8 units a frame, then a 24-frame
  loop at **24 units a frame**.
- **The bite** (melee only; it never attacks beyond melee range) [R]: a 20-frame animation
  (`flpbit01` to `flpbit20`), facing the enemy: the chomp sound on frame 1 and a bite straight
  ahead on frames 14 and 19: reach 80, **5 damage, no kick**.
- **No dodge** [R].
- **Pain** [R]: one of two 5-frame flinches at even odds, each with its sound.
- **Death** [R]: at or below −30: `misc/udeath.wav`, 2 `bone`, 2 `sm_meat`, an `sm_meat` head.
  Otherwise a 56-frame death and the standard corpse box (tossed: it sinks only as tossing in water
  allows; the library gives nothing more).

### `monster_supertank` (the super tank)

- **Sounds** [R]: pain `bosstank/btkpain1.wav` (hits up to 10), `bosstank/btkpain3.wav` (up to 25),
  `bosstank/btkpain2.wav` (more); death `bosstank/btkdeth1.wav`; search `bosstank/btkunqv1.wav` or
  `bosstank/btkunqv2.wav` (even odds); the treads `bosstank/btkengn1.wav` played on the first frame
  of each walk, run, turn and reverse loop. No sight sound.
- **Walks** [R]: an 18-frame loop at 4 units a frame walking, 12 running; turning and reversing
  loops exist (no distance).
- **Choosing an attack** [R]: enemy within 160 units: the chaingun; else the chaingun 3 times in 10,
  the rockets 7 in 10.
- **The chaingun** [R]: a 6-frame loop (`attak1_1` to `attak1_6`), facing the enemy, one bullet a
  frame: **6 damage, kick 4, spread 300 × 500**, aimed at the enemy's eye (no lead), from six
  muzzle points in turn. At the loop's end it goes on 9 times in 10 while the enemy is
  visible; otherwise a 14-frame wind-down.
- **The rockets** [R]: a 27-frame animation (`attak2_1` to `attak2_27`), facing the enemy for its
  first 8 frames: a **rocket of 50 at 500** on frames 8, 11 and 14, each from its own launcher.
- **No melee, no dodge** [R].
- **Pain** [R]: hits of 25 or less are ignored one time in five; at skill 2 or more it takes no pain
  during the first 14 frames of the rocket animation; then the pain wait (3 s), the skill-3 check,
  and a 4-frame flinch chosen by the hit's damage as for the sounds above.
- **Death** [R]: whatever the damage: the death sound, it can no longer be damaged, a 24-frame
  death, then the boss explosion on its last frame (no corpse). Gib health plays no part.
- Another monster hurt by the super tank does not turn on it [R] (`sequel-monsters.md` § Reacting).

### `monster_boss2` (the hornet)

- **Sounds** [R]: pain `bosshovr/bhvpain3.wav` (hits under 10), `bosshovr/bhvpain1.wav` (under 30),
  `bosshovr/bhvpain2.wav` (30 or more); death `bosshovr/bhvdeth1.wav`; search
  `bosshovr/bhvunqv1.wav` one time in two; constant engine loop `bosshovr/bhvengn1.wav`. Its pain,
  death and search sounds are heard **at any distance** (no attenuation).
- **Flies** [R], at 8 units a frame walking or running; immune to laser damage (the map's lasers
  and the BFG's lasers do not hurt it).
- **Its own attack test** [R]: it needs a clear shot-line eye to eye that does not stop at
  windows; it turns to face the enemy; in melee range it always attacks (ranged); otherwise, after its
  attack wait and not at far range, with a chance per frame of **0.8 at melee, near or mid range**
  (0.4 when holding its ground), **not** changed by skill; a success sets the next wait to 0 to 2 s.
  A failed roll slides it sideways 3 times in 10, else it flies straight.
- **Choosing an attack** [R]: enemy within 125 units: the machine guns; else the machine guns 6
  times in 10, the rockets 4 in 10.
- **The machine guns** [R]: a 9-frame spin-up, then a 6-frame loop that on each of its first 5
  frames fires **two bullets** (left and right guns), each **6 damage, kick 4, spread 300 × 500**,
  aimed at the enemy's eye **0.2 s behind where it is moving** (its position minus 0.2 s of its
  velocity), so 10 bullets a loop [D]. At the loop's end it goes on 7 times in 10 while the enemy
  is in front of it, else a 4-frame wind-down.
- **The rockets** [R]: a 21-frame animation (`attack20` to `attack40`), facing the enemy; on frame
  13 it **fires four rockets at once, each 50 at 500**, each from its own launcher at the enemy's
  eye, and on that frame it steps 20 units back.
- **No melee, no dodge** [R].
- **Pain** [R]: no skill-3 suppression of the animation (the 5 s skill-3 wait still applies); under 30
  damage a 4-frame flinch, 30 or more an 18-frame stagger.
- **Death** [R]: whatever the damage: the death sound, no longer damageable, a 49-frame death, then
  the boss explosion on its last frame (no corpse).

### `monster_jorg` (Jorg, the first phase of the final boss)

- **Sounds** [R]: pain `boss3/bs3pain1.wav` to `boss3/bs3pain3.wav`, death `boss3/bs3deth1.wav`,
  attack `boss3/bs3atck1.wav` (machine guns) and `boss3/bs3atck2.wav` (BFG), search
  `boss3/bs3srch1.wav` to `boss3/bs3srch3.wav` (3, 3 and 4 in 10), idle `boss3/bs3idle1.wav`,
  footsteps `boss3/step1.wav` and `boss3/step2.wav`, `boss3/xfire.wav` and `boss3/d_hit.wav`
  precached and not played by any frame, and the guns' loop `boss3/w_loop.wav`.
- **Walks** [R]: a 14-frame loop (steps 17, 0, 0, 0, 12, 8, 10, 33, 0, 0, 0, 9, 9, 9) for walking and
  running alike; its 51-frame stand also shuffles it (a few frames move it up to 19 units).
- **Its own attack test** [R]: like the hornet's (clear eye-to-eye line not stopped by windows,
  faces the enemy, in melee range always ranged), but with chances per frame of **0.8 at melee,
  0.4 near, 0.2 mid** (0.4 holding its ground), not changed by skill.
- **Choosing an attack** [R]: 3 times in 4 the machine guns (attack sound 1, the loop sound
  starts); else the BFG (attack sound 2).
- **The machine guns** [R]: an 8-frame spin-up (`attak101` to `attak108`), then a 6-frame loop that
  fires two bullets (left and right) on **every** frame, each **6 damage, kick 4, spread 300 × 500**,
  aimed at the enemy's eye 0.2 s behind its motion: 12 bullets a loop [D]. At the loop's end it
  goes on 9 times in 10 while the enemy is visible; otherwise the loop sound stops and a 4-frame
  end.
- **The BFG** [R]: a 13-frame animation (`attak201` to `attak213`); on frame 7 the BFG sound and a
  BFG ball **at 300 units per second** toward the enemy's eye, its final burst **50 strong with a
  200-unit reach**.
- **Pain** [R]: any hit stops the guns' loop sound. During the pain wait, nothing more. Hits of 40
  or less are ignored 6 times in 10. (There are also tiny chances, 1 in 200 or 1 in 20 000, of
  ignoring a hit during its attack wind-ups and loop.) Then the wait, the skill-3 check, and: up to
  50 a 3-frame flinch, up to 100 another 3-frame flinch, more than 100 a 25-frame stagger 3 times in
  10 (otherwise nothing, the wait still set).
- **No melee, no dodge, no sight sound** [R].
- **Death** [R]: whatever the damage: the death sound, no longer damageable, its loop sound stops, a
  50-frame death. On frame 49 it **launches the Makron**; on frame 50 the boss explosion starts
  (Jorg ends as gibs, no corpse).
- **The Makron's launch** [R]: a new entity at Jorg's origin, carrying Jorg's `target`, which 0.8 s
  later becomes a `monster_makron` (full spawn, counted in the level's monster total) facing the
  frame's sight client (the player chosen in turn for monsters to look at), and leaps at it: 400
  units per second level toward that player and 200 up. With no sight client it just stands up
  where Jorg was. Since Jorg's own targets fire at Jorg's death, the same targets fire again at the
  Makron's death [D]. Timing [D]: frame 49 at time T, the boss explosion's effects at T + 0.1 to
  T + 0.8, the Makron appears at T + 0.8, Jorg's gibs fly at T + 0.9.

### `monster_makron` (the Makron)

- **Sounds** [R]: pain `makron/pain3.wav` (hits up to 40), `makron/pain2.wav` (up to 110),
  `makron/pain1.wav` (the long stagger); death `makron/death.wav`; footsteps `makron/step1.wav` and
  `makron/step2.wav`; BFG `makron/bfg_fire.wav`; the brain's splat `makron/brain1.wav`; railgun
  charge `makron/rail_up.wav`; `makron/popup.wav`; taunts `makron/voice4.wav`,
  `makron/voice3.wav`, `makron/voice.wav` (3, 3 and 4 in 10); the hit `makron/bhit.wav`. Pain,
  death, taunts, popup and hit are heard at any distance.
- **Starts** [R] in its 13-frame activation (rising) animation, which is also what it plays on
  sighting a player (no sight sound). Walks and runs on a 10-frame loop (3, 12, 8, 8, 8, 6, 12, 9,
  6, 12) with footsteps on frames 1 and 5.
- **Its own attack test** [R]: the same as Jorg's (0.8 melee, 0.4 near, 0.2 mid, 0.4 holding
  ground; no skill scaling; ranged in melee range).
- **Choosing an attack** [R]: 3 in 10 the BFG, 3 in 10 the blaster sweep, 4 in 10 the railgun.
- **The BFG** [R]: an 8-frame animation; on frame 4 the BFG sound and a ball at **300 units per
  second**, final burst **50 strong with a 300-unit reach**.
- **The blaster sweep** [R]: a 26-frame animation (`attak401` to `attak426`), facing the enemy for
  the first 4; on each of frames 5 to 21 (`attak405` to `attak421`) a bolt of **15 damage at 1000**
  (blaster trail), 17 bolts. Each bolt's pitch is toward the enemy's eye; its yaw is the Makron's
  facing plus an offset that steps 10 degrees a frame: from 80 degrees to one side down to 0 over
  the first 9 bolts, then jumping to 70 degrees to the other side and back to 0 over the last 8
  [D from the offsets: frame 405 gives +80, 413 gives 0, 414 gives −70, 421 gives 0; positive is a
  turn to the left under the game's angle convention].
- **The railgun** [R]: a 16-frame animation (`attak501` to `attak516`), facing the enemy for 8: the
  charge sound on frame 1, the enemy's eye position **remembered on frame 8**, the rail fired on
  frame 9 (0.1 s later) at the remembered point: **50 damage, kick 100**.
- **Pain** [R]: hits of 25 or less are ignored one time in five; then the wait, the skill-3 check,
  and: up to 40 a 4-frame flinch; up to 110 another 4-frame flinch; up to 150 the 27-frame stagger
  45 times in 100; more than 150 the stagger 35 times in 100 (otherwise nothing). The stagger plays
  the popup sound on frame 16 and a taunt on frame 24.
- **No melee, no dodge** [R]. Another monster hurt by the Makron or Jorg does not turn on it [R].
- **Death** [R]: at or below −2000: `misc/udeath.wav`, 1 `sm_meat`, 4 `sm_metal` and a `gear` head.
  Otherwise the death sound, and a separate torso is spawned 84 units along the world's negative
  second ground axis from its origin (not relative to its facing): not solid, not moving, the rider
  model looping frames 346 to 364 at 10 Hz forever. The Makron plays a 95-frame death that walks it
  about (steps, footsteps), with the hit sound on frame 91 and the brain splat on frame 93, then a
  corpse box of (−60, −60, 0) to (60, 60, 72). The corpse can still be damaged.

### `monster_boss3_stand` (the Makron's appearance)

- [R] Not a monster: not counted, no thinking beyond its animation, cannot be damaged, has no
  spawn flags; removed in deathmatch. A solid box (−32, −32, 0) to (32, 32, 90),
  moving as a stepping entity, showing the rider model and looping frames `stand201` to
  `stand260` at 10 Hz. When used (by a target), it sends the boss-teleport effect at its origin and
  is removed. `misc/bigtele.wav` is precached for that effect; the library itself plays no sound.

### `monster_commander_body` (the dead tank commander)

- [R] Not a monster (not counted, no senses); kept in deathmatch too (no deathmatch check). A solid
  box (−32, −32, 0) to (32, 32, 48), the model `models/monsters/commandr/tris.md2`. It takes damage
  but is in god mode, so it never loses health (it still gets knocked about: see below). For its
  first 0.5 s it stays exactly where placed; then it is raised 2 units and becomes a tossed object
  (falls to the floor; knockback from damage moves it, treating its mass as 50 [D: no mass is set,
  and knockback treats mass under 50 as 50]). When used: it plays `tank/pain.wav` and, from 0.1 s
  later, advances one frame every 0.1 s from frame 1 to frame 24 and stops there; on frame 22 it
  plays `tank/thud.wav`.

## The step move

All [R] unless marked.

- **Who moves**: a walker moves only while on the ground; fliers and swimmers always. Each animation
  frame's distance is multiplied by the class's model scale (1 for every class in question 1; 1.2
  for the soldiers and 1.15 for the gunner, see question 3).
- **The step height is 18.** A walker's move of a frame is tried as: the wished spot is the current
  origin plus the move; a box trace runs from 18 above the wished spot to 18 below it. If the trace
  starts and stays in solid, the move is refused. If it only starts in solid, it is tried again from
  the wished height (not raised) down to 18 below, and refused if that starts in solid. So it climbs
  steps up to 18 and walks down steps up to 18 in one frame.
- **No walking into water**: a walker not in water refuses a move whose end puts the point 1 unit
  above its box's bottom in water (any of water, slime, lava).
- **No floor within 18 below**: the move is refused (it does not walk off a ledge), unless it has the
  partial-ground mark, in which case it moves the plain horizontal distance, loses its ground and
  falls.
- **The footing test** (after a floor was found and the monster placed on it): the quick pass looks
  at the four bottom corners of its box, each 1 unit below the bottom; if all four are in solid
  world, it stands. Otherwise the full pass traces a line from the centre of its box's bottom down
  36 units (twice the step): if nothing is hit the footing fails. Then a line from each bottom corner
  down to the same depth: if any corner hits nothing, or hits more than 18 below where the centre's
  line hit, the footing fails. So a box may hang over an edge as long as its centre is over floor
  and no corner is over a drop deeper than 18 below the centre's floor [D].
- **A failed footing**: the move is undone and refused, unless it has the partial-ground mark, in
  which case the move is kept (it is trying to get back onto floor). A successful footing clears
  the mark and records what it stands on.
- **The partial-ground mark** is set only when a monster has tried every direction and can move in
  none, and the footing test fails where it stands (a floor pulled out from under it).
- **A step toward a direction**: the monster sets the direction as its wished facing and turns
  toward it by up to its turn rate (20 degrees a frame for walkers, 10 for fliers and
  swimmers; the shorter way round), then tries the move along the wished direction (not its facing). If the
  move worked but its facing is still more than 45 degrees off the wished direction, the move is
  taken back (it turns in place), though the attempt counts as a success.
- **Moving to a goal** each frame: if it has an enemy and the enemy's box comes within the frame's
  distance of its box on all three axes (the boxes would touch within one step), it does not move
  at all. Otherwise it first tries a step along its current wished facing; one frame in four at
  random, or when that step is refused, it picks a new direction toward its goal (not toward the
  enemy, though usually the goal is the enemy).
- **Picking a new direction** (the eight compass directions, 45 degrees apart; no sliding along
  walls): take the goal's offset on each ground axis, counting an offset within 10 units as none.
  The reverse of its current heading (the heading rounded down to a multiple of 45, plus 180) is
  avoided until last.
  1. If both axes have an offset, the diagonal toward the goal, unless it is the reverse. (The
     diagonal used when the goal is toward negative on both axes is 215 degrees, not 225: a quirk
     of the source, so that "diagonal" is 10 degrees off [R].)
  2. Then the two single-axis directions toward the goal, one after the other: the first ground
     axis's direction first, except that the second axis's goes first whenever its offset is the
     larger, and otherwise at random one time in two.
  3. Then the old heading.
  4. Then all eight directions in turn, starting at 0 and going up by 45, or starting at 315 and
     going down, at even odds, skipping the reverse.
  5. Then the reverse.
  6. If none worked it keeps its old heading as its wished facing, and if its footing fails where it
     stands it gets the partial-ground mark.
- **Fliers and swimmers** (no stepping up or down): when the monster has an enemy, the first try
  adjusts the height of the move: toward a player goal, down 8 units when its origin is more than 40
  above the player's origin, up 8 when less than 30 above (a swimmer that is less than waist-deep,
  water level under 2, never rises this way); toward any other goal (a path point, the spot where
  it last saw the player), it closes the height difference by up to 8 units a frame (exactly, when
  within 8). So a flier holds its origin between 30 and 40 units above a player's origin [D]. The
  box is traced along the move; if blocked and it has an enemy, a second try is made without the
  height change; without an enemy there is only the one try, level.
- **A flier never enters water voluntarily**: when it is out of water, a move whose end puts the
  point 1 unit above its box's bottom in water is refused.
- **A swimmer never leaves water voluntarily**: when it is less than waist-deep (water level under
  2), a move whose end puts that point out of water is refused. When deeper, no such check is made
  [R; the source does not settle what keeps a deep swimmer from rising out through the surface
  other than its height rule].
- **Physics around it**: a walker not on the ground falls under gravity (and makes the landing
  sound when it lands from a fall faster than a tenth of gravity per second); a flier gets no
  gravity and its vertical speed is damped by a third of the ground friction; a swimmer in deep
  water (level 3) gets no gravity, and its vertical speed is damped by water friction times its
  water level. Horizontal friction applies on the ground or to fliers and swimmers, except that a
  dead monster whose footing fails slides. A monster is on the ground when its box, a quarter unit
  lower, rests on a surface whose normal's up part is at least 0.7 and it is not rising faster than
  100.

## Speeds for the demo's roster

Each animation frame moves its distance times the class's model scale [R]: **1.2 for the soldiers**
(all three: light guard, guard, heavy guard), **1.15 for the gunner**, 1 for the rest. One frame is
0.1 s, so units per second are ten times the figures [D]. The figures are fixed per frame of each
animation [R], not constant across frames; the table gives the average per frame over the loop
(sum over frame count, then times the scale) [D].

| Class | Walk per frame | Run per frame | Notes |
| --- | --- | --- | --- |
| soldiers | 3.48 (first walk) or 6.48 (second walk) | 14.6 | walking picks one of two walks at even odds; the first walk's 10 moving frames total 29 (× 1.2) and loop back to their start 9 times in 10 at frame 10; one time in 10 it plays 23 more still frames; the second walk's 10 frames total 54. Run: straight into the run loop when it was walking; otherwise (standing, after an attack or pain) a 2-frame start (7, 5, times 1.2) first; the loop is 6 frames totalling 73 |
| `monster_infantry` | 4.5 | 14.4 | walk 12 frames totalling 54; run 8 frames totalling 115 (uneven: 10, 20, 5, 7, 30, 35, 2, 6) |
| `monster_gunner` | 4.95 | 13.9 | walk 13 frames totalling 56; run 8 frames totalling 97; a faster run-and-shoot animation exists and is never used |
| `monster_berserk` | 6.13 | 19.2 | walk 12 frames totalling 73.6; run 6 frames totalling 115 |
| `monster_parasite` | 25.4 | 25.4 | walk and run are the same 7 frames totalling 178; a 2-frame start (0, 30) and a 6-frame stop exist |
| `monster_flyer` | 5 | 10 | every frame the same |
| `monster_tank` (and commander) | 4.69 | 4.69 | walk and run the same 16 frames totalling 75 (the tank runs no faster than it walks); a 4-frame start (0, 6, 6, 11) unless it was walking; 5-frame stops |

**The distance kept from the enemy** [R, D]: there is none for any monster. While the enemy is
visible a running monster moves toward it; it stops only when the next step would bring its box
within that step's distance of the enemy's box (they would touch). A monster without a melee attack
does not stand off: in melee range (under 80) its attack test always chooses its ranged attack (no
chance roll and no attack wait; at skill 0 three times in four it does nothing that frame) [R]. A
flier that does not attack slides sideways around the enemy three frames in ten of the frames its
roll fails, otherwise closes in. Exceptions that keep a short zone: the gladiator refuses to fire its
railgun inside 112 units (it keeps closing until in melee range, then cleaves), the super tank uses
its chaingun inside 160, the hornet its machine guns inside 125.
