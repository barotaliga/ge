# The Reckoning's monsters, specified (WORKING note)

A WORKING note of facts, for the Quake II mission pack "The Reckoning" (game folder `xatrix`):
its new monsters and what it changes in the base ones. It continues `q2-monsters.md` and
`q2-monsters-full.md` (life, senses, the attack choice, the base roster, the step move), uses
their shape (frame names and counts, which frame acts, steps, sounds by path, numbers) and says
only what differs for a monster built on a base one. **Where these facts came from**: a
clean-room READER read the pack's GPL game library (the yquake2 project's release of it, with its
full history back to the import of the original source, and the yquake2 base game library for
comparison) and answered written questions in its own words, with no code. Each fact is marked
**[R]** read, **[D]** derived from read facts, or **[M]** measured (none yet). Where the GPL
fork changed the original 3.20-era behaviour, the original is given as "original release". As
with the base notes, **exact parity is not the goal**: the builder designs these monsters and
holds them to these numbers in the labs.

Units: map units, seconds, a 0.1 s frame (10 Hz), degrees. "Base" means the base monster as our
notes give it; anything not mentioned is unchanged.

Shared facts used below:

- **The red laser beam** (used by the laser guard and the brain's eyes) [R]: each firing frame
  spawns one short-lived beam entity at the shooter's muzzle point. At that moment it is aimed at
  the centre of the target's box. 0.1 s later the hit is resolved along that fixed direction,
  2048 units long. The beam passes through monsters, corpses and players, hurting each one it
  crosses (never its owner, never anything immune to lasers). It stops at the first thing that is
  neither a monster nor a player. Each one hurt takes **1 damage** (energy damage) with a
  **knockback equal to the skill number** (0 to 3). On a solid stop it sends temporary entity
  **15** (laser sparks), 10 sparks with colour byte 240. The beam is then removed 0.1 s later. So
  the laser "tracks" by being re-aimed on every firing frame, 0.1 s behind [D]. It is drawn as a
  beam (render flags 128 beam and 32 translucent), width 2, with palette colours 242, 242, 240,
  240 (red). The end point is written when the hit is resolved. Nothing in the library sets it
  earlier, so what a client draws in the beam's first 0.1 s is not settled by the library [R/D].
  The kill's cause is the one the map's laser uses. In coop it prints "<player> saw the light." [R].
- **The healing beam** (the repair bot) [R]: the same entity with damage −1 (so each one hit
  **gains 1 health**), drawn in palette colours 243, 243, 241, 241 (blue). Its aim point wobbles
  sideways along the world's first ground axis by 8 × sin(level time). Sparks use colour byte 241.
  A player who ends above 100 health has the point taken back again. A monster has no cap.
- **No pain while ignoring pain** [R]: the pack's damage rule skips the pain call for a monster
  that is ducked, as in the base game, and also for one marked "ignoring pain" (the fork's flag,
  used only by the gekk's leap; see 2).
- **Green blood** [R]: see 2 and 9.

## 1. The three new guards

**Common to all three** [R] (each fact differs from the base guards only as stated):

- Classes `monster_soldier_ripper`, `monster_soldier_hypergun`, `monster_soldier_lasergun`.
  Model `models/monsters/soldierh/tris.md2`. It has exactly the base guard model's frame names
  and frame numbers (475 frames, the same names), scale 1.2, the same box (−16, −16, −24) to
  (16, 16, 32), mass 100 and gib health −30.
- **Health**: ripper **50**, hypergun **60**, lasergun **70**.
- **Skins**: ripper 0 (damaged 1), hypergun 2 (damaged 3), lasergun 4 (damaged 5). As in the base,
  the weapon is chosen by the skin number on every shot: skin 0–1 ion ripper, 2–3 blue blaster,
  4–5 laser. The class only sets the skin at spawn, after the shared spawn, so a map's skin key
  is overwritten [R].
- **Sounds**: the base guards' by the same rule. The ripper uses the light guard's pain
  `soldier/solpain2.wav` and death `soldier/soldeth2.wav`. The hypergun uses the plain guard's
  `soldier/solpain1.wav` and `soldier/soldeth1.wav`. The lasergun uses the machine-gun guard's
  `soldier/solpain3.wav` and `soldier/soldeth3.wav`. All three share idle `soldier/solidle1.wav`,
  sight `soldier/solsght1.wav` or `soldier/solsrch1.wav` (even odds) and the cock
  `infantry/infatck3.wav`. Precached as well: the ripper's `models/objects/boomrang/tris.md2`,
  `misc/lasfly.wav` and `soldier/solatck2.wav`; the hypergun's
  `models/objects/blaser/tris.md2` and `soldier/solatck1.wav`; the lasergun's
  `soldier/solatck3.wav`. The fork's optional footstep sounds are given to the base guards only,
  not to these three.
- **Starting animation**: it always starts in the 39-frame second stand (`stand301` to
  `stand339`, with the cock sound on its frame 22). As for every monster, it starts on a random
  frame of it. The base guard picks between its two stands (8 in 10 the 30-frame one).
- **Walk, run, pain, duck, dodge, sight, death animations and their steps**: the base guard's,
  frame for frame [R]. The pain rule is the base guard's as well (sound by skin, the air flinch
  when rising faster than 100, no animation at skill 3).
- **Aim spread**: every aimed shot uses a spread **10 times tighter** than the base guard's:
  crandom × **100** sideways and crandom × **50** up or down at 8192 units (the base uses
  1000 × 500). As in the base, the dying shots (flash numbers 5 and 6 of the eight) go straight
  along the body's facing.
- **The flash numbers** [R]: the ripper and the hypergun look up the base light guard's eight
  blaster flash slots: 39, 40, 83, 86, 89, 92, 95, 98. Each slot's muzzle offset places the shot.
  The lasergun looks up the machine-gun guard's slots: 43, 44, 85, 88, 91, 94, 97, 100.
- **Spawn flags**: only 1 ambush, 2 triggered spawn and 4 sight. Removed in deathmatch.
- **Original release** [R]: in every guard class (the base three included) the health was set
  after the shared spawn had copied it to the maximum. So the maximum was 0 and the damaged skin
  never showed while the guard lived. The fork sets health first.

**The ion ripper guard** [R]:

- Shot: an ion ripper disc of **5 damage at 600 units per second** (see 9 for the projectile),
  sent with the guard's blaster flash slot of that frame. The client draws it as the base light
  guard's blaster flash.
- Attack 1 (`attak101` to `attak112`, 12 frames): fires on **frames 3, 4 and 5** (three discs, where
  the base light guard fires once, on frame 3). On frame 6, if the enemy is alive and either
  (skill 3 and one time in two) or it is in melee range, it goes back to frame 2. Otherwise it
  jumps to frame 10, skipping the cock on frame 8.
- Attack 2 (`attak201` to `attak218`, 18 frames): fires on **frames 5, 6 and 7**. On frame 8,
  the same test sends it back to frame 4 or on to frame 16.
- Even odds between the two, as in the base. Duck-and-shoot (`attak301` to `attak309`) fires one
  disc on frame 3. Run-and-shoot (`runs01` to `runs14`) fires one on frame 4. The dying shots of
  the first death fire discs.

**The hyperblaster guard** [R]:

- Shot: a blue blaster bolt of **1 damage at 600** (see 9). Every bolt sends the monster muzzle
  flash number **17** whatever the frame. In the base client's monster flash table 17 is a tank
  machine-gun slot. So the client draws a tank machine-gun flash at that slot's offset, with
  smoke and one of `tank/tnkatk2a.wav` to `tank/tnkatk2e.wav` [R from the base client; D that
  this is what a player sees].
- Attack 1: `weapons/hyprbl1a.wav` on **frame 2**, bolts on **frames 3, 4, 5**. On **frame 7**,
  7 times in 10 it loops (the next frames shown are 4, 5, 6, 7: two more bolts per 4-frame
  loop). Otherwise it plays `weapons/hyprbd1a.wav` (the spin-down) and goes on: the cock on
  frame 8. On frame 9, if the enemy is alive and either (skill 3 and one time in two) or it is
  in melee range, it restarts from frame 2 (a new spin-up and burst). The frame-6 refire test is
  the ripper's only.
- Attack 2: the spin-up sound on **frame 4**, bolts on **frames 5, 6, 7**. On **frame 9**, 7 in 10
  it loops through frames 6, 7, 8, 9 (two bolts per loop), else the spin-down. Then the cock on
  frame 13. On frame 15, the same restart test sends it back to frame 4.
- A burst is 3 bolts plus 2 per loop. The loop repeats on average 2.33 times, so a burst
  averages about 7.7 bolts (1 damage each) [D].
- Duck-and-shoot, run-and-shoot and the dying shots fire single blue bolts.

**The laser guard** [R]:

- It always uses attack 4 (`attak401` to `attak406`, 6 frames): the laser on frame 3, held there
  like the base machine-gun guard's burst. On the first firing frame it picks a hold of 3 to 10
  frames. It fires a beam on every held frame and goes on when the hold time is reached (a beam
  is also fired on that frame). So **4 to 11 beams of 1 damage each, one per 0.1 s**, if all
  hit [D] (frame-time rounding can move the count by one).
- On each beam frame, 2 times in 10, it plays `misc/lasfly.wav` at static attenuation.
- **Where the beam starts** [R]: at the guard's origin. It uses the machine-gun flash slot's
  offset, but laid out along the line from the guard to the enemy's origin, not along the
  guard's facing. The slot's forward and side numbers are used swapped, with 2 units more to the
  side (14 units less for slot 85, the duck-and-shoot), and 8 more up. No muzzle flash message
  is sent for the laser.
- **Sight**: where the base guard (at skill 1 or more, enemy at mid range or beyond, one time in
  two) starts run-and-shoot, the laser guard starts its laser attack instead.
- **Duck-and-shoot** fires the laser with the same 3-to-10-frame hold.
- **Attack 2's end test** (unused by it) excludes it from the melee-range repeat [R].
- **Dying**: it never uses the first death (the one with the dying shots). It picks among deaths
  2, 4, 5 and 6 with even odds (a head shot still gives death 3) [R]. Original release: it picked
  among all five like the others, and its dying-shot frames did nothing.

**The base three guards in the pack** [R]: their weapons, numbers, animations and choices are
the base game's. The pack adds no blind fire and no new duck. The only differences are the
original release's spawn-order bug above and the fork's changes shared with the base fork.

**Obituary**: see 10.

## 2. `monster_gekk`

**Numbers** [R]: health **125**, gib health **−30**, mass **300**. Box **(−24, −24, −24) to (24,
24, 24)** on land. In water the top drops to **16**. Model `models/monsters/gekk/tris.md2`, scale
1. **Skins**: 0 healthy, **1 below half health, 2 below a quarter**, and 2 when dead. A walker:
turn 20 a frame and eye 25 on land; 10 and 10 in water (it switches itself; see below).
Spawn flags: 1 ambush, 2 triggered spawn, 4 sight, **8 chant**. There is no "starts in water" flag:
it changes mode by whether it is in water. Removed in deathmatch.

**Sounds** [R]:

| Sound | When |
| --- | --- |
| `gek/gk_sght1.wav` | sight, and at every leap's take-off |
| `gek/gk_idle1.wav` | search and idle (see healing) when not chanting |
| `gek/gek_low.wav`, `gek/gek_mid.wav`, `gek/gek_high.wav` | instead, a third each, when it has the chant flag |
| `gek/gk_pain1.wav` | pain |
| `gek/gk_deth1.wav` | death |
| `misc/udeath.wav` | gibbed |
| `gek/gk_atck1.wav` | a claw that misses |
| `gek/gk_atck2.wav` | the left claw hitting |
| `gek/gk_atck3.wav` | the right claw hitting |
| `gek/gk_step1.wav` to `gek/gk_step3.wav` | one of three on frame 4 of each run or walk loop |
| `mutant/thud1.wav` | landing from a leap |

The bites and the spit have no sound.

**Animations** (frame names, count, per-frame steps, the frames that act) [R]:

- **Stand**: `stand_01` to `stand_39` (39, no movement). On frame 39 it enters water mode if it
  is in water.
- **Underwater stand**: `amb_01` to `amb_04` (4). On frame 4 it enters water mode if in water.
- **Idle** (on land): `idle_01` to `idle_32` (32). Frame 1 is the search: it makes the sound and
  heals (below). On frame 32, while hurt, one time in four it goes back to frame 1. Then it
  stands. A second use of the same frames (after a spit, below) runs instead of standing.
- **Walk and run**: both `run_01` to `run_06` (6), steps **3.849, 19.606, 25.583, 34.625, 27.365,
  28.480** (23.25 a frame on average [D]). Frame 1 enters water mode if in water. Frame 4 is the
  footstep. Run start: `stand_01` to `stand_02`, steps 0.212 and 19.753. On land the gekk's run
  entry always plays this start first, even when it was already walking.
- **Swim start**: `swim_01` to `swim_32` (32, moved by the run AI). Steps 14, 14, 14, 14, 16, 16,
  16, 18, 18, 18, 20, 20, 22, 22, 24, 24, 26, 26, 24, 24, then 22 eight times, then 18 four times.
  It **claws** left on frame 9 and right on frame 15, and **bites** on frame 21. Then the swim
  loop.
- **Swim loop**: `amb_01` to `amb_04` (4), **16 a frame**. It now counts as a swimmer. On frame 4
  the leave-water test runs (below).
- **Land claws A**: `clawatk3_01` to `clawatk3_09` (9, no movement, facing the enemy): left claw
  on frame 4, refire test on frame 9.
- **Land claws B**: `clawatk5_01` to `clawatk5_09` (9): left claw on frame 3, right claw on frame
  6, refire test on frame 9.
- **Water attack**: `attack_01` to `attack_21` (21, a charge of **16 units a frame**, 336 in
  all [D]): bites on frames 5 and 10, left claw on frame 14, right claw on frame 19.
- **Spit**: `spit_01` to `spit_07` (7, facing the enemy): spits on frame 6, re-spit test on
  frame 7.
- **Leap**: `leapatk_01` to `leapatk_19` (19). Steps 0, −0.387, −1.113, −0.237, 6.720, 6.414,
  0.163, 28.316, 24.198, 31.742, 35.977, 12.303, 20.122, −1.042, 2.556, 0.544, 1.862, 1.224,
  −0.457. The steps move it only while on the ground, by the base walker rule [D]. Take-off on
  frame 5, landing check on frame 11. Frames 12 to 18 stop any skid (if on the ground, its
  velocity is zeroed). Frame 19 enters water mode if in water. A second leap from the water
  uses the same frames with its own take-off.
- **Pain**: in water `pain_01` to `pain_06` (6). On land, at even odds, `pain3_01` to `pain3_11`
  (11) or `pain4_01` to `pain4_13` (13), each entering water mode on its last frame if in water.
  All without movement.
- **Deaths**: on land, a third each: `death1_01` to `death1_10` (10, steps −5.151, −12.223,
  −11.484, −17.952, −6.953, −7.393, −10.713, −17.464, −11.678, −11.678). Or `death3_01` to
  `death3_07` (7, steps 0, 0.022, 0.169, −0.710, −13.446, −7.654, −31.951; one time in ten it
  bursts into gibs on frame 6). Or `death4_01` to `death4_35` (35, small steps; one time in ten
  it bursts on each of frames 5, 10, 14, 18, 22, 26 and 30, and it **always** bursts on frame
  35). So the long death always ends in gibs [D]. In water: `wdeath_01` to `wdeath_45` (45, no
  movement).
- **Dodges**: `lduck_01` to `lduck_13` and `rduck_01` to `rduck_13` (13 each, no movement). They
  do **not** lower the box or hold. The duck rule is switched off in the source. They are only
  animations.

**Melee** [R]: each claw reaches 80 straight ahead, 24 units to the side (left or right) and 8 up.
It deals **15 to 19 damage, kick 100** (the base melee-blow rule). The hit sound is per side; the
miss sound is the swing. Each bite reaches 80 straight ahead and deals **5 damage, no kick**, with
no sound. On land, melee picks claws A about 1 time in 3 (34 in 100) and claws B otherwise. In
water it always uses the water attack. **Refire**: at the end of either land claw animation, with
a chance of **skill × 0.1** (0, 0.1, 0.2, 0.3) and the enemy alive in melee range, it plays the
other claw animation.

**Its own attack test** [R] (it replaces the base test; it runs only when the enemy is visible,
as for every monster): the enemy must be alive. **In melee range: melee** (no easy-skill skip).
Otherwise, if the mutant's jump test passes, "missile". The heights must overlap: its bottom not
above the enemy's bottom plus 3/4 of its height, and its top not below the enemy's bottom plus
1/4. The level distance must be at least 100: at exactly 100 it passes, beyond 100 one test in
ten. Otherwise, **out of water only**, "missile" whenever the level distance is 100 or more, or
when it is under 100 with the gekk lower than the enemy. It never reads its attack wait. So on
land, with the enemy visible beyond melee range, it attacks on nearly every frame it faces the
enemy [D].

**The missile choice** [R]: in water or swimming, nothing. On land: if a roll passes one time in
two and the enemy is at near range or beyond, it spits. Otherwise one time in five it spits, else
it leaps. Overall that is 6 spits in 10 and 4 leaps in 10 beyond melee range [D].

**The spit** [R]: on frame 6, if the enemy is alive, a glob leaves from 18 units behind the origin,
0.8 to the left and 26 up. It goes at the enemy's eye with no spread: **5 damage at 550 units per
second**, knockback 1, energy damage. Model `models/objects/loogy/tris.md2`. Its effect field
carries the value **8** (meant as "full bright", it is the blaster effect bit: a yellow glow and
trail). It has no sound and a 2 s life. On something that takes damage it deals the damage and
vanishes. On anything else it vanishes with no impact effect, and on sky it vanishes. On frame 7:
2 times in 10, if hurt, it plays the idle animation (healing) and then runs. Otherwise, if the
enemy's health is 0 or more, 3 times in 10 at near range it spits again.

**The leap** [R]:

- On land (frame 5): the sight sound, it rises 1 unit and leaves the ground. If the jump test
  passes again at that moment: **700 forward and 250 up**; otherwise **250 forward and 400 up**.
  It is marked "ignoring pain" (no pain call, the damaged skin still applies), its attack wait is
  set 3 s ahead, and contact damage is armed.
- **Contact**: touching something that takes damage while moving faster than **200** deals **10
  to 20 damage** (10 plus up to 10, rounded down), with knockback equal to the damage, in its
  direction of travel. Then, if its footing test fails: when it has ground, it goes to frame 11,
  stops ignoring pain and disarms contact; while airborne contact stays armed, so it can strike
  again. If the footing holds it stops ignoring pain and disarms contact.
- **Frame 11**: on the ground, the thud sound, the wait clears, pain returns and its velocity is
  zeroed. In the air, within 3 s of take-off it goes on to frame 12 (the skid frames, still
  airborne). After 3 s in the air it holds frame 11 until it lands.
- Original release: the leap marked the gekk as **ducked** instead (no pain, as for ducking).
  Only the frame-11 ground check cleared it; contact did not. So a gekk that landed while past
  frame 11 stayed "ducked" and never flinched again.

**Water and land** [R]:

- **Entering water mode**: on the frames named above (stand 39, underwater-stand 4, walk or run
  1, leap 19, land pain last frames), if any part of it is in water. It becomes a swimmer: turn
  10, eye 10, box top 16. It then plays the swim start (with its built-in claws and bite) and the
  swim loop. A walking gekk does not walk into water, by the base walker rule. It reaches water
  by leaping or falling [D].
- **In water** its attack test can only give melee (the water attack; the missile choice does
  nothing). Dodges become the water attack. Its idle and its run entry play the swim start [R].
  Without an enemy it settles into the underwater stand [D].
- **Leaving water**: at the end of each swim loop (every 0.4 s), when its enemy is not in water at
  all, 3 times in 10 it switches to land mode: walker, turn 20, eye 25, box top 24. Then it
  plays the leap with its water take-off on frame 5. That take-off **sets its own height to the
  enemy's origin height** at once. Then it goes **300 forward and 250 up** if the jump test
  passes, else **150 forward and 300 up**. The rest is the land leap's. It chases underwater by
  the ordinary run toward the enemy, within the base swimmer rules [D].

**Healing** [R]: every search call (the base walking search on its 15-to-30 s timer, and frame 1
of the idle) adds **10 to 20 health** (10 plus up to 10, rounded down), capped at its maximum. It
then re-picks the skin by the thresholds (below 1/4 skin 2, below 1/2 skin 1, else 0). So a hurt
gekk that idles heals and loses its damaged skin [D].

**Chant (spawn flag 8)** [R]: while standing, it uses the plain move instead of looking (it does
not notice players by sight or sound). Unless it is an ambusher it calls its idle every 15 to 30
s: the land idle with the chant sound and the heal. The **first hit** clears the flag and has no
other effect: no pain, and no skin change on that hit. From then on it is an ordinary gekk.
Without the chant flag, a standing gekk that already has an enemy uses the plain move too; one
without an enemy stands and looks as usual. Original release: a chanting gekk started in a
looping chant animation. It was the 32 idle frames, looping forever, with the chant sound and the
heal on frame 1 of every loop (every 3.2 s), also without looking. It left the chant only through
pain. A non-chanting standing gekk always looked, even with an enemy. The fork removed the loop
to fix gekks stuck walking.

**Pain** [R]: chant handling first (above). Then the skin by thresholds, then the 3 s pain wait,
then the pain sound. Then the water or land flinch (above). **There is no skill-3 suppression**:
it flinches at every skill.

**Dodge** [R]: offered as for the base monsters. One time in four it takes the dodge. With no
enemy, the attacker becomes its enemy (the fork also runs the found-target step, as it does for
every dodging monster). In water: the water attack. Skill 0: a left or right dodge animation at
even odds. Skill 1: 2 in 3 a dodge animation, else a land claw attack (A 1 in 3, B 2 in 3).
Skill 2: 1 in 3 a dodge animation, 2 in 3 a claw attack. Skill 3: always a claw attack. The dodge
animations do not duck.

**Death** [R]: at or below −30: `misc/udeath.wav` and the gekk's own gibs. They are
`models/objects/gekkgib/pelvis/tris.md2`, two `models/objects/gekkgib/arm/tris.md2`,
`models/objects/gekkgib/torso/tris.md2`, `models/objects/gekkgib/claw/tris.md2`, two
`models/objects/gekkgib/leg/tris.md2`, and the gekk becomes `models/objects/gekkgib/head/tris.md2`.
All are thrown as organic gibs. The death-animation bursts throw the same set with a fixed damage
of 20 for the throw. Each gib carries the **green gib effect** (effect bit 21, value 2097152)
and draws full bright. The head carries the green gib effect and the value 8 (the blaster glow).
Gibs and head vanish after 10 to 20 s. Otherwise: the death sound, skin 2, and the land or water
death. **On land** the standard corpse box follows. **In water** no corpse is made: it keeps its
living box, stays solid and shootable, keeps thinking, and its 45-frame water death repeats
forever [D from the source leaving the water case empty]. So a gekk dead in water can be gibbed
but never becomes a corpse a medic could raise [D].

**Green blood** [R]: every hit that takes health from a `monster_gekk` (alive or dead) sends
temporary entity **26** (green blood) at the hit point instead of the red blood (1). The pack's
trap (a player weapon) also uses the gekk torso gib for a trapped gekk's pieces.

**Obituary**: in coop, a player killed by the gekk's claws, bites, leap or spit gets "<player>
that's gotta hurt." [R] (see 10).

## 3. `monster_fixbot` (the repair bot)

**Numbers** [R]: health **150**, mass **150**. Box **(−32, −32, −24) to (32, 32, 24)**. Model
`models/monsters/fixbot/tris.md2`, scale 1, skin 0, with no damaged skin. A flier: turn 10. Eye
**16** (fork; original release: the flier default 25, see the note). No gib health is set: it
never gibs. Spawn flags: 1 ambush, 2 triggered spawn, **4 Fixit, 8 Takeoff, 16 Landing**.
Removed in deathmatch.

**Sounds** [R]: pain `flyer/flypain1.wav`, death `flyer/flydeth1.wav`. Welding `misc/welder1.wav`
to `misc/welder3.wav` at idle attenuation, a third each, played 2 times in 10 on a welding frame.
The healing beam plays `misc/lasfly.wav` at static attenuation on every beam frame. It has no
sight, search or idle sound.

**Animations** [R] (most are a single still frame `freeze_01` used as a mode):

- Stand: `ambient_01` to `ambient_19` (19, the plain move: it does **not** look for players).
  Frame 19 runs the roam choice (below).
- Second stand: the same 19 frames with the standing AI that looks for players. It loops with no
  end.
- Roam goal: one frame that picks a goal. Turn: one frame, turning toward the goal until it is in
  front, then forward.
- Forward: one frame, **5 units a frame** toward the goal, running the scanner (below) on every
  frame.
- Walk: one frame at 5. Run: one frame at **10**.
- Takeoff: `takeoff_01` to `takeoff_16` (16). Landing: `landing_01` to `landing_58` (58).
- Attack (charge): `charging_01` to `charging_31` (31). It backs off **10 units a frame on frames
  11 to 20** (100 in all) and fires the blaster on **frame 21**.
- Healing laser: `shoot_01` to `shoot_06` (6), a healing beam on every frame.
- Weld start: `weldstart_01` to `weldstart_10` (10). Weld: `weldmiddle_01` to `weldmiddle_07` (7,
  sparks on frames 1 to 6). Weld end: `weldend_01` to `weldend_07` (7, backing 2 units a frame).
  All three turn to face the goal.
- Pain: up to 10 damage, one still frame stepping 1 back. Up to 25, `painb_01` to `painb_08` (8).
  More, `paina_01` to `paina_06` (6). Then it runs.
- Defined and unused: a 6-frame `shoot_01` to `shoot_06` blaster attack, a 27-frame pickup, and
  still frames for land, backward and death.

**What it does, in order** [R]:

1. **Roam choice** (stand's frame 19): first it looks for a corpse to revive (below), but only
   when it has no goal. Otherwise:
   - with flag 16 (Landing) it makes a goal on the floor straight below (a box trace of
     (−32, −32, −24) to (32, 32, 24) down to 8096 units) and plays the landing,
   - with flag 8 (Takeoff) it makes a goal 128 units straight up and plays the takeoff,
   - with flag 4 (Fixit) it roams.

   After any of the three its spawn flags become 32 (the ambush bit is lost with them). **With
   spawn flags 0 it switches to the second stand for good** and becomes an ordinary standing
   monster that waits for a player. With any other flags (1, 2, or 32 after a first choice) it
   roams.
2. **Roaming**: it casts 12 shot lines 8192 units from its origin, at its facing plus 0, 30,
   60, 90, 120 and 150 degrees and minus the same. It makes a goal at the end of the longest
   (the farthest open point), turns to it, then flies forward at 5 a frame.
3. **The scanner** (every forward frame): unless it is already going for a repair object, or
   within 15 s of giving up on one, it looks for a **`func_object_repair`**. The object must be
   within **1024**, have health **100 or more**, and be visible. If one is found it becomes the
   goal, replacing a roam goal. Going for a repair object, it starts welding within **56 units**.
   Going for a roam goal, it gives up and stands when within **32** of it or **15 s** after the
   goal was made. If its origin has not moved for more than **10 frames** in a row: with a roam
   goal it drops the goal and stands; with a repair object it ignores repair objects for **15
   s** and stands. While roaming it never looks for players [D: none of its roaming moves
   looks].
4. **Takeoff and landing**: each frame turns toward the goal. The takeoff steps 0.01 forward a
   frame and ends on its 16th frame. Nothing in it moves the bot toward the goal 128 units up, so
   the rise is only the model's animation [D]. On every takeoff frame it **fires 10 thrust
   traces straight down**. They are bullets of **2 damage, kick 1**, spread 1000 + (frame −
   first) sideways and 500 + the same vertically. They hurt what is below, make the shotgun puff
   (temporary entity 4) on walls and splashes and bubble trails in water. The 58-frame landing
   loops until the bot's origin is within **32** of the floor goal. It does not move the bot
   either, so a bot more than 32 above the floor keeps looping it [D]. After either it stands.

**The weld** [R]: on the last start frame it begins the weld loop. Each weld frame 1 to 6 sends
temporary entity **25** (welding sparks): 10 sparks at 24 ahead, 0.8 left and 10 down from its
origin, colour 224 to 231. On frame 7: if the object's health is still 0 or more it takes **10**
off it and the loop repeats. Once it is below 0, the weld end plays and the bot stands with no
goal. So a full-health object takes 12 loops (8.4 s), plus the 1 s start and 0.7 s end [D].

**The repair object** (`func_object_repair`, a point entity, box (−8, −8, −8) to (8, 8, 8))
[R]: solid, health 100. Every `delay` seconds (default 1) it throws the welding sparks (10,
colour 224 to 231) while its health is 0 or more. When its health falls below 0 it fires its
targets once (with itself as activator). Then, every `delay`, it regains 1 health up to 101,
sparking only once above 100. It never fires its targets again [D]. Because the scanner takes
objects at 100 or more, a regrown object (after about 110 s at the default delay) can be welded
again with no further effect [D].

**Reviving the dead** [R]:

- **The corpse it looks for**: a monster within **1024**, not on the good side, not claimed (no
  owner), dead (health 0 or below), finished dying (no longer thinking), visible. It takes the one
  with the highest maximum health; on a tie the first found. These are the medic's rules, and a
  medic's claim and a bot's claim block each other.
- **Claiming**: it claims the corpse, remembers its current enemy as its old enemy, makes the
  corpse its enemy, enters "medic" mode (its beams turn blue) and hunts it. It runs at 10 a frame.
  Its attack fires through the ordinary chance rule. In melee range the "missile" slot is always
  taken, since it has no melee. The attack does nothing unless the corpse is visible and within
  **128**; then it plays the healing laser [D].
- **Each laser frame**: if the corpse has been gibbed (health at or below its gib health) it stops,
  leaves medic mode and stands. Otherwise it plays `misc/lasfly.wav` and fires a healing beam
  from 16 units toward the corpse. That gives **+1 health** to the corpse, and to every monster or
  player in the beam's 2048-unit line. Then, if the corpse's health is now above **its mass ÷
  10** (a guard needs more than 10, a tank more than 50), it tries to revive it.
- **The revive test**: a box trace of the corpse's box from its origin 48 units up. If that strikes
  something that takes damage, that thing is killed outright (10000 damage, cause unknown) and
  the revive waits for a later frame. Otherwise the corpse is cleared of its spawn flags, all AI
  flags, target, target name, combat target and death target. It is spawned again by its own
  class's spawn rule: full health, its living box, counted in the level's monster total again.
  The bot rises 1 unit, leaves medic mode and stands. Below the threshold the corpse is only
  marked as being revived.
- **How long** [D]: one health per beam, one beam a frame while it keeps choosing the laser. A
  guard killed at −20 needs 31 beams, at least 3.1 s of laser frames. A tank killed at −100 needs
  151 beams.
- **Which monsters**: any class [R]. There is no list and no count limit. A corpse in water that
  never stops thinking (the gekk) is never taken [D].
- **Not done** [R]: it does not reset the corpse's skin (a class whose spawn sets no skin comes
  back with its damaged skin [D]). It does not give the revived monster an enemy. It does not
  release a claimed corpse when the bot dies. The "being revived" mark shows nothing: the red
  shell is only drawn for a monster that thinks [D].
- Original release: the revive test traced from the corpse toward a point 48 units above the
  map's origin, not above the corpse. It "killed" by setting health to −1000.

**Fighting players** [R]: it fights only once it has a player as its enemy. That happens by the
second stand's looking, or by being hurt: being hurt turns it on the attacker, as for any monster.
Its attack is always the charge animation. On frame 21 it fires a **blaster bolt of 15 damage at
1000**, with the blaster effect (bit 3, value 8), at the enemy's eye with no spread. It uses
monster flash **62** (the icarus's blaster slot). If the enemy is not visible on that frame it
switches to running, but the bolt is still fired.

**Pain** [R]: the 3 s wait, the sound, then the flinch by the hit's damage (above). There is no
damaged skin and no skill-3 suppression.

**Death** [R]: whatever the damage: the death sound, and it becomes an explosion (temporary entity
**5**, explosion 1, no damage) and is gone. No corpse, no gibs.

**Triggered spawn** [R]: a bot with flag 4, 8 or 16 that is spawned by a trigger does not take
the activator as its enemy (every other triggered monster does).

**Fork changes** [R] (original release): the scanner rescanned every frame with no 15 s pause. It
started welding only within 32 units. It gave up on any goal (and stood) as soon as it had not
moved for a single frame. Roam goals never timed out. Roam goal entities leaked. The eye was 25.

**Obituary**: the blaster kills print nothing special (see 10). The thrust traces have their own
cause with no words, so a player killed by them "died".

## 4. `monster_gladb` (the gladiator beta)

Everything is the base gladiator's [R] except:

- **Model** `models/monsters/gladb/tris.md2` (skins 0, damaged 1). **Health 800** (base 400),
  **mass 350** (base 400). Gib health −175, box and animations unchanged.
- **Power shield, 400 points** [R]. Unlike the brain's screen it covers hits from **every
  direction**. A covered hit has **two thirds** of its damage (rounded down) taken by the shield.
  The shield absorbs **2 damage per point**, so 400 points absorb 800 damage. It sends the shield
  sparks (temporary entity **13**). It is drawn for 0.2 s as a green shell (effect value 256 with
  render flag 2048). Damage marked as ignoring armour passes. Points are never restored [R].
- **The gun**: the base gladiator's start rule (the 112-unit safe zone, then remember the enemy's
  eye). The sound is **`weapons/plasshot.wav`** instead of the railgun charge. The 9-frame
  `attack1` to `attack9` fires a **phalanx round on frames 3 and 6**, and **also on frame 9 at
  skill 3**: 2 rounds, or 3 at skill 3, all at the remembered point. Each round deals **100
  direct damage** (no knockback) plus **60 splash within 60** at **725 units per second**, leaving
  from the base gladiator's rail muzzle offset (slot 61). **No muzzle flash message is sent.** The
  projectile is described in 9.
- **The safe zone**: the fork skips it for a gladiator holding its ground (both classes). In the
  original release every gladiator refused to fire within 112 units.
- **Pain**: the base gladiator's except that the beta **plays its pain animations at skill 3**
  (the skill-3 wait still applies) [R]. The pack's base gladiator keeps the skill-3 suppression.
- Sounds otherwise the base gladiator's (`gladiator/pain.wav`, `gladiator/gldpain2.wav`,
  `gladiator/glddeth2.wav`, the cleaver's three, idle, search, sight). It has no footsteps.
- **Obituary**: see 10.

## 5. `monster_chick_heat` (the iron maiden beta)

Everything is the base iron maiden's [R] (same model `models/monsters/bitch/tris.md2`, health
175, sounds, animations, slash, dodge) except:

- **Skin 3** at spawn. Its pain rule is the base maiden's, which sets **skin 1** below half
  health. So a hurt beta maiden shows the base maiden's damaged skin, never a damaged skin of its
  own [D].
- **The rocket** is a heat-seeking rocket. It uses the same frame and flash as the base (flash
  **57**, the maiden's rocket slot) and is fired at the same point. It is **50 direct damage at
  500**, with **50 splash within 50** (a base monster rocket of 50 splashes within 70).
- Model `models/objects/rocket/tris.md2`, the rocket effect (bit 4, value 16), sound
  `weapons/rockfly.wav`. It explodes as a rocket: temporary entity 7, or 17 in water. The kill
  causes are the rocket's.
- **Homing**: 0.1 s after launch and every 0.1 s after, it looks for the **nearest living
  player** within **1024 units** of the rocket. The player must be in front of the rocket (a
  cosine above 0.3 with its current heading) and visible from it. It **turns instantly**
  straight at that player's origin (no turn limit) and flies at **500**. It homes on players
  only, never on monsters. With none found it keeps its course.
- **It has no life limit**: nothing removes it until it touches something [R].
- Monsters dodge it like a rocket when a player fires the same weapon. A monster's own heat
  rocket offers no dodge.
- **Which maidens fire it**: the fork decides by class name. **Original release**: by skin number
  above 1. So a beta maiden **fell back to plain rockets once hurt below half** (skin 1), and any
  `monster_chick` given a skin above 1 by its map fired heat rockets.
- **Obituary**: see 10.

## 6. `monster_boss5` (the super tank beta)

Everything is the base super tank's [R] (box, 1500 health, mass 800, sounds, walk, chaingun,
rockets, choices, boss explosion) except:

- **Model** `models/monsters/boss5/tris.md2`.
- **Power shield, 400 points**, always on: the same shield rule as in 4 (every direction, 2/3 of a
  hit, 2 damage per point, 800 damage absorbed in all, green shell, sparks 13).
- **Pain**: it **plays its flinches at skill 3** (no suppression; the 5 s skill-3 wait still
  applies). The pack's base super tank keeps the suppression.
- **Retaliation**: the rule that stops monsters turning on a super tank that hurt them names
  `monster_supertank` only. So other monsters **do** turn on a `monster_boss5` that hurts them
  [D].
- No weapon change. Its flash numbers are the super tank's (64 to 72). Its damaged skin is 1, as
  for the super tank.
- **Obituary**: see 10.

The pack's `monster_supertank` itself gains **spawn flag 8, "Powershield"**: with it the same
400-point shield [R].

## 7. The brain in the pack

- **Every `monster_brain`** in the pack has the new attacks [R]. There is no spawn flag, no
  separate class and no new skin: the class, model `models/monsters/brain/tris.md2`, numbers and
  power screen are the base brain's. It now has a **ranged attack slot**. So the base attack
  choice (the per-frame chances by range, no firing at far range) applies to it beyond melee
  range [D]. In melee range it still uses its base melee (claws or tentacles). The fork's
  history notes the slot was lost in a cleanup and restored. The original release had it.
- **Choosing** [R]: when the attack choice gives "missile", 8 times in 10 it attacks: at near
  range (80 to 500) the tongue or the eye lasers at even odds; at mid range (and beyond) the eye
  lasers. 2 times in 10 it does nothing that time.
- **The tongue attack** (`attak201` to `attak217`, 17 frames, steps 5, −4, −4, −3, 0, 0, 13, 0, 2,
  0, −9, 0, 4, 3, 2, −3, −6) [R]:
- Frame 5: the chest opens (`brain/brnatck1.wav`, the power screen off).
- Frames 6 and 10: a tongue strike.
- Frame 8: the base tentacle melee (reach 80, 10 to 14, kick −600; at skill 1 or more a hit
  leads into the claws after the chest closes). The base has it on frame 7. At near range it
  usually misses [D].
- Frame 11: the chest closes (the screen back).
- **A tongue strike** [R]: from a point 24 ahead and 16 up. The target point is the enemy's origin.
  If that is more than **512** away or more than **30 degrees** above or below level, it tries
  the top of the enemy's box less 8, then the bottom plus 8. If none passes, nothing happens.
  Then a shot line is traced to the enemy's **origin**, and it must strike the enemy. On a hit:
  `brain/brnatck3.wav`, and temporary entity **16** (the parasite's drain beam) from the
  brain's point to the enemy's origin. It deals **5 damage with no knockback**. The enemy's
  velocity is **replaced** by 1200 units per second along the brain's facing, toward the brain
  (a hard pull). The brain itself rises 1 unit. Kill cause: brain tentacle.
- **The eye lasers** (`walk101` to `walk111`, 11 frames, the run steps 9, 2, 3, 3, 1, 0, 0, 10, −4,
  −1, 2, facing the enemy) [R]:
- **Frames 1 to 10** each fire **two red laser beams**, one from each eye. The eye points move
  with the frame, about 30 to 34 units above the origin and a few units apart. Each beam is
  **1 damage**, re-aimed every frame, 2048 long. So up to 20 damage per pass [D].
- 2 times in 10 per frame, `misc/lasfly.wav` at static attenuation.
- **Frame 11**: one time in two, if the enemy is visible and alive, the animation continues
  from frame 2. Each repeat is 9 more frames of beams, up to 18 more damage [D].
- The power screen stays up throughout [D].
- The brain otherwise keeps every base behaviour [R].

## 8. Other base monsters and shared rules

Monsters [R]:

- **Infantry** (`monster_infantry`): the burst attack (`attak101` to `attak115`) is reworked.
  Steps 10, 6, 0, 0, 1, −7, −6, −1, 0, 0, 0, 0, 0, −1, −1. On frame 1 it picks the burst's end,
  5 to 20 frames ahead. It fires on **frame 3** (held, a bullet every frame until that end) and
  plays the cock sound on frame 9, after the burst. Base: the cock and the pick on frame 4 (10 to
  25 frames), the fire on frame 11. The bullet count is the same in both, 4 to 19 [D]. The first
  aimed shot's flash belongs to frame `attak103`. Damage, kick, aim and the club are unchanged.
- **Super tank** (`monster_supertank`): spawn flag 8 gives a 400-point power shield (see 6).
- **Brain**: see 7.
- **Gladiator, iron maiden, soldier guards**: the base classes are unchanged in the pack (their
  betas and new guards are separate classes).
- **Medic, tank, tank commander, flyer, parasite, berserker, gunner, mutant, technician, icarus,
  barracuda shark, hornet, Jorg, Makron**: no change to numbers, attacks, animations or choices
  [R].
- The parasite's sight, tap, scratch and search sounds, and the infantry's, flipper's and iron
  maiden's search sounds, play on the weapon channel instead of the voice or body channel.
  Only which sounds cut each other off changes.
- The pack's library lacks the base game's jail-map flyer fix (a base map).
- The Makron's sight entry is an empty rule (no sound), the same effect as none.

Shared rules:

- **The attack choice, sight, hearing, the trail, the step move, the medic's raising rule, the
  power screen and shield rule, and the retaliation exceptions (tank, super tank, Makron, Jorg)**
  are unchanged [R].
- The medic still revives any class, and its claim and the repair bot's block each other (3).
- The new betas are not in the retaliation list, so monsters turn on the super tank beta (6).
- **Dodging** is offered by the same rule. The pack's new player weapons also offer it, being
  projectiles: the ion ripper, the phalanx, the heat rocket and the trap launcher [R].
- **Blood** is green (26) for the gekk (2).
- **Pain is skipped** for a monster "ignoring pain" as well as a ducked one (the fork's gekk fix).
- **New monster weapon calls**: the blue blaster, the ion ripper, the heat rocket and the laser
  beam (9).
- **Triggered spawn**: the repair bot's exception (3).
- **Friendly fire**: the trap's damage passes the friendly-fire guard (player weapon; outside this
  question's monsters).

## 9. Projectiles and effects new in the pack

| What | Effect field (bit, value) | Model, sound | Speed, life | On hit | Temporary entities |
| --- | --- | --- | --- | --- | --- |
| **Ion ripper disc** (ripper guard: 5 at 600) | bit 20, 1048576; drawn full bright | `models/objects/boomrang/tris.md2`, loop `misc/lasfly.wav` | 600; **bounces off walls** keeping its speed; **3 s** | something that takes damage: the damage, knockback 1, energy; removed. Walls: bounces with no effect. Sky: removed | at 3 s it ends with **25** (welding sparks), count 0, colour 228 to 231, and is removed; no impact effect otherwise |
| **Blue blaster bolt** (hyper guard: 1 at 600) | bit 22, 4194304 | `models/objects/blaser/tris.md2`, loop `misc/lasfly.wav` | 600; 2 s | as the blaster bolt: the damage, knockback 1, energy, kill cause blaster | **2** (blaster puff) on a wall; nothing on sky. Temporary entity 27 exists but is sent by no rule in the library |
| **Heat rocket** (beta maiden: 50 at 500, 50 within 50) | bit 4, 16 (the rocket's) | `models/objects/rocket/tris.md2`, loop `weapons/rockfly.wav` | 500; re-aims every 0.1 s at the nearest visible player in front within 1024; **no life limit** | as a rocket: direct damage, then splash; kill causes rocket and rocket splash | **7**, or **17** in water |
| **Phalanx round** (beta gladiator: 100, 60 within 60, at 725) | bit 24, 16777216, plus bit 13, 8192 (fast frame animation) | sprite `sprites/s_photon.sp2`, loop `weapons/rockfly.wav` | 725; life 8000 ÷ speed s, 11.0 s at 725 [D] | direct damage with no knockback, then splash on all but the one hit; kill cause phalanx | **28** (plasma explosion), or **17** in water, placed 0.02 s back along its path |
| **Laser beam** (laser guard, brain) | none; render flags 128 + 32; width 2; colours 242, 242, 240, 240 | model index 1 (a beam) | instant; one per firing frame, resolved 0.1 s after it appears, removed 0.1 s later | 1 damage per beam, knockback = skill, energy, through monsters and players, 2048 long | **15** (laser sparks) on the solid end, 10 sparks, colour 240 |
| **Healing beam** (repair bot) | as the laser; colours 243, 243, 241, 241 | the same | the same | +1 health to each one crossed | **15**, colour 241 |
| **Gekk spit** | value 8 (the blaster bit 3) | `models/objects/loogy/tris.md2`, no sound | 550; 2 s | 5, knockback 1, energy | none |
| **Gekk gibs** | bit 21, 2097152 (green gib trail) | `models/objects/gekkgib/...` | 10–20 s | — | — |

Muzzle flashes [R]:

- The ripper guard uses the soldier blaster slots (39, 40, 83, 86, 89, 92, 95, 98).
- The hyper guard always sends **17** (see 1). The player weapons' flash numbers 16 (ion ripper),
  17 (blue hyperblaster) and 18 (phalanx) are player flashes. Only the hyper guard sends 17, and
  it sends it as a monster flash.
- The beta maiden sends 57. The beta gladiator sends none. The laser and the brain's beams send
  none. The repair bot sends 62.

Other numbers [R]:

- Green blood is temporary entity **26** (2).
- The shields' sparks are **13**; the screen's are 12.
- The repair bot's welding uses **25** and its thrust uses the shotgun puff **4**.
- Its death uses **5**.

## 10. Obituaries

[R] The pack's library has **no monster names** in its death messages. It prints obituaries only
in deathmatch or coop. In single player nothing is printed for a death by a monster. In coop a
player killed by any monster gets **"<player> died."**, except where the kind of damage has its
own words, whoever caused it:

- The laser guard's and the brain's beams (the map laser's cause): **"<player> saw the light."**
- The gekk's claws, bites, leap and spit, and the brain's tongue: **"<player> that's gotta
  hurt."**
- The heat rocket, the phalanx round, the ion ripper and the blue bolt: just "died." Their
  weapon-specific lines ("ripped to shreds by …'s ripper gun", "was evaporated by …") are printed
  only when the killer is a player.
- The repair bot's thrust traces: "died."

So for every new monster the line is "died." unless one of the two cases above applies.
