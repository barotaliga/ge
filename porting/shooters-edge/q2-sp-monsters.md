# Quake II single player: what a player would notice (audit B, answers)

Source read: id Software's Quake II source release (GPL-2.0), the game library. A later
community port was consulted only where the original was unclear and no difference from it
matters here. Ours: games/shooters_edge/server/ (q2_monsters.cpp, q2_monster_kinds.hpp,
q2_monster_poses.hpp, q2_poses.cpp, q2_fight.cpp, q2_items.cpp, q2_items.hpp, q2_server.cpp).

Marks: [read] means stated by the original source as read; [derived] means worked out from what
was read (an effect or a consequence); [measured] means measured. Nothing here was measured by
running the original, so every fact is [read] or [derived]. What "ours does" is read from our
own code.

Frame numbers are model frames counted from 0. Times are in seconds. Monsters think ten times a
second, so "one frame" is 0.1 s.

## Ranked table

Rank 1 means noticed at once on the stock campaign. Rank 2 means noticed sometimes. Rank 3 means
rare or cosmetic.

| # | Rank | Area | Difference |
| --- | --- | --- | --- |
| 1 | 1 | Weapons, AI | Corpses cannot be shot, gibbed or blown apart (shots and splash pass through them) |
| 2 | 1 | Weapons | A rocket or grenade that hits a monster directly hurts it twice (direct plus full splash) |
| 3 | 1 | Weapons | The BFG's lasers and its final burst never hurt monsters |
| 4 | 1 | Weapons | The railgun stops at the first monster instead of passing through every body in line |
| 5 | 1 | AI | No double damage on a monster that has not yet noticed the player |
| 6 | 1 | AI | No infighting: a monster hurt by another never turns on it |
| 7 | 1 | AI, maps | The `deathtarget` key is ignored (a death fires `target` only) |
| 8 | 1 | AI, maps | The `combattarget` key is ignored |
| 9 | 1 | AI, maps | A monster at any point_combat holds there for ever, Hold flag or not |
| 10 | 1 | Parasite | The drain has no reach limit (the original's reaches 256 and needs the pitch within 30 degrees) |
| 11 | 1 | Flyer | It dies leaving a corpse instead of exploding at once |
| 12 | 2 | Weapons | Splash damage reaches through walls (the original needs a clear line to each victim) |
| 13 | 2 | Brain | Bullets, pellets and splash ignore the power screen and push nothing |
| 14 | 2 | AI | Hearing: no PHS or area test, no turning to the sound, no hunting the noise point, no 5 s forgetting |
| 15 | 2 | AI | Dark corners do not hide the player (the original ignores a player in light 5 or less) |
| 16 | 2 | AI | At melee range a monster still needs the player in front to see him |
| 17 | 2 | AI | No 1 s pause between first sighting and first attack |
| 18 | 2 | AI | A monster that loses the player stops at the last sighting and gives up after 20 s, instead of following the player's trail for ever |
| 19 | 2 | AI | A monster may attack without first facing within 45 degrees of the player |
| 20 | 2 | AI | Monsters attack while running to their point_combat |
| 21 | 2 | AI, maps | path_corner `wait` and `pathtarget` ignored, and the teleport flag on the next corner ignored |
| 22 | 2 | AI, maps | point_combat `pathtarget` and chained `target` ignored |
| 23 | 2 | AI, maps | Using a monster that has no enemy does not anger it at the user |
| 24 | 2 | AI, maps | An ambushing triggered monster still targets whoever triggered it |
| 25 | 2 | AI | Pain hold is 0.3 s for every monster (the original's pain animations run their own length, soldiers stunned 0.5 to 1.8 s) |
| 26 | 2 | Sounds | No idle, search or looping sounds on any monster |
| 27 | 2 | Soldiers | No aim spread on the light soldier, and the shotgun and SS soldiers miss the extra spread every soldier adds |
| 28 | 2 | Soldiers | Death sounds of the light and shotgun soldiers swapped |
| 29 | 2 | Soldiers | No run-and-shoot attack, no dying shots, no head-shot death |
| 30 | 2 | Gunner | Grenade attack throws one grenade instead of four, aimed rather than straight ahead |
| 31 | 2 | Gunner | Chaingun is not its melee-range choice, and its burst is not the original's 8-shot one with 50% refire |
| 32 | 2 | Tank | Volleys wrong: blaster 1 bolt (should be 3, plus 2 at skill 2 and up), rockets no refire, machine gun not a 19-shot yaw sweep |
| 33 | 2 | Tank | Attack choice by range not as the original's (125 and 250 thresholds) |
| 34 | 2 | Flyer | Two bolts per volley instead of eight, and its melee is one blow instead of two repeating slashes |
| 35 | 2 | Berserker | Only one melee blow, the club with the spike's numbers (the original has two of each kind, 50/50) |
| 36 | 2 | Infantry | Melee and gun timings and the dying spray differ |
| 37 | 2 | Monsters | Monsters take no damage from lava, slime or drowning |
| 38 | 2 | Weapons | The silencer makes the shot silent; in the original the shot is still heard, only quieter |
| 39 | 2 | Weapons, items | No quad fire sound, no quad or invulnerability use sound, no invulnerability hit sound |
| 40 | 2 | Items | No warning sounds 3 s before quad, invulnerability, enviro suit or rebreather run out |
| 41 | 2 | Items | Item spawnflag 1 read as "no rotate" instead of "hidden until triggered", and spawnflag 2 (no touch) not honoured |
| 42 | 2 | Items | Touching an item does not fire its targets |
| 43 | 2 | AI | Monsters hear only weapon fire: no jump or pain noise, no impact noise |
| 44 | 2 | Weapons | The first pickup of grenades does not switch to them |
| 45 | 2 | Weapons | BFG timing: muzzle sound and noise at the wrong frame, and the ball dies after 20 s instead of flying until it hits |
| 46 | 2 | Items | Power armor on and off sounds and the "No cells for power armor." message missing |
| 47 | 2 | misc_insane | One pain and one death sound; no crawling, lying down, crucified or screaming behaviour |
| 48 | 3 | Weapons | No pin sound on the hand grenade, no chaingun spin-up or spin-down, no hyperblaster wind-down |
| 49 | 3 | AI | Fliers never slide sideways while closing in |
| 50 | 3 | AI | Pain sound plays at skill 3 and the 5 s pain debounce at skill 3 is 3 s here |
| 51 | 3 | AI | A ducking monster can still be put into pain |
| 52 | 3 | AI | Monsters start on the first stand frame, not a random one |
| 53 | 3 | AI | Fliers get no knockback |
| 54 | 3 | AI | Flier eye height is 10 here, 25 in the original |
| 55 | 3 | Monsters | Pain animations not chosen by damage (gunner, chick, gladiator, hornet, supertank, Makron and others) |
| 56 | 3 | Monsters | Several monsters have one pain or death sound where the original has two or three |
| 57 | 3 | Monsters | Sight sounds: soldier never uses solsrch1; infantry sight on another channel; supertank has none (correct) |
| 58 | 3 | Monsters | Corpse boxes differ; the chick's corpse box is inverted |
| 59 | 3 | Monsters | No flies on infantry and mutant corpses |
| 60 | 3 | Tank | No stomp on a dead enemy, no thud at the end of death, no windup sound |
| 61 | 3 | Supertank | Pain sound fixed to one file; pain ignore rules differ; no tread sound |
| 62 | 3 | Hornet | Pain, death and search sounds not heard map-wide; not immune to BFG lasers (moot while lasers miss monsters) |
| 63 | 3 | Makron | Run frames 474 to 490 instead of 477 to 486; no torso left behind; no taunts or pop-up sound |
| 64 | 3 | Jorg | Attack choice weights and sounds differ slightly |
| 65 | 3 | Mutant | No skin change at death |
| 66 | 3 | Soldiers | No pain on being knocked upward (frames 80 to 96) |
| 67 | 3 | Flyer | The jail5 map's flyer target fix not applied |
| 68 | 3 | Items | A map's `count` on ammo not checked (see detail) |

## Details

### 1. Corpses (rank 1)

Original [read]: shots, pellets, rails and blasts all hit dead monsters. A corpse keeps taking
damage, and once its health falls to its gib value or below it bursts into gibs with
misc/udeath.wav. Players routinely gib corpses with the shotgun or a rocket.

Ours: death clears the body's box, so every shot passes through. The blast loop skips anything at
health 0 or less. A monster gibs only if the killing blow already took it to its gib value.

### 2. Rocket and grenade direct hits (rank 1)

Original [read]: a rocket that hits something that takes damage deals its direct damage to it.
Its splash then hurts everything else in range, leaving out the body it hit. A grenade that
touches a damageable body explodes. It deals that body its damage less half the distance from
the blast centre, and its splash leaves that body out.

Ours: the direct hit deals full damage. The blast then runs with no body left out, so the same
monster takes the splash too. A rocket kills things that should survive. A tank dies in about
two-thirds the rockets [derived].

### 3. BFG against monsters (rank 1)

Original [read]: while the ball flies, each frame it lasers every monster or player within 256.
A laser does 10 in single player (5 in deathmatch), and its beam passes through monsters to hit
more. The hornet is immune. When the ball hits, every damageable thing within 1000 that the ball
sees takes 500×(1 − √(d/1000)), provided the shooter can see it as well.

Ours: lasers and the final burst hit players only. The BFG kills monsters only through the ball's
own direct and splash damage.

### 4. Railgun (rank 1)

Original [read]: the slug passes through monsters and players and stops only at the world. Each
body it crosses takes 150 in single player, with kick 250.

Ours: it stops at the first monster or mover.

### 5. Surprise bonus (rank 1)

Original [read]: when a player hits a monster that has no enemy yet, the hit does double damage
(not radius damage). Sneaking up and opening with the shotgun or railgun kills most foot
soldiers in one hit.

Ours: no bonus.

### 6. Infighting (rank 1)

Original [read]: when one monster hurts another, the victim turns on the attacker if they move
the same way (both walk, or both fly, or both swim) and are of different classes, and the
attacker is not a tank, tank commander, supertank, Makron or Jorg. It also turns if the attacker
was aiming at it, or if it is helping the attacker's own target. The stock campaign often shows
gunners and soldiers or flyers fighting one another.

Ours: only players become enemies.

### 7. deathtarget (rank 1)

Original [read]: a dying monster fires the entities named by its `deathtarget` key if it has
one, else its `target`. The killer is the activator. Stock maps hang doors, lifts and
triggers on monster deaths this way.

Ours: only `target` is fired. No code reads `deathtarget`.

### 8. combattarget (rank 1)

Original [read]: a monster with a `combattarget` key runs to that point_combat when it first
gets angry.

Ours: the key is not read. Only a point_combat reached through `target` counts.

### 9. point_combat hold (rank 1)

Original [read]: a monster that reaches a point_combat holds its ground there only if the point
has spawnflag 1 (Hold), and not if it flies or swims. Otherwise it goes on hunting.

Ours: reaching any point_combat makes it hold for ever.

### 10. Parasite drain reach (rank 1)

Original [read]: the drain reaches 256 from the mouth, and the target's pitch must be within 30
degrees. Out of reach, the animation still plays and does nothing. The damage is 5 at frame 41,
then 2 at each of frames 42 to 51, 25 in all.

Ours: the same damage sequence with no reach limit.

### 11. Flyer death (rank 1)

Original [read]: the flyer explodes at once on death (flydeth1) and leaves no corpse.

Ours: it leaves a corpse.

### 12. Splash and walls (rank 2)

Original [read]: radius damage hurts a body only if a clear line runs from the blast to it.

Ours: no line test.

### 13. Power screen (rank 2)

Original [read]: a power screen saves a hit only if the hit point is in front (cosine greater than
0.3). It saves a third of the damage, one cell per point, sparks, and lights the screen effect for
0.2 s. A power shield saves two-thirds at two cells per point. The brain's screen is off only
between frames 75 and 81 of its second attack.

Ours: the screen works only for damage that comes with a direction. Bullets, pellets and blasts
come with none, so they pass the screen and push nothing.

### 14. Hearing (rank 2)

Original [read]:
- A noise counts only if it is in the monster's PHS. An ambusher also needs to see the noise's
  source.
- The noise must be within 1000, in a connected area.
- The monster turns towards it and goes to the noise point, standing once within 64. It forgets
  the sound after 5 s.
- A player's noise lasts one frame. Impact noises are heard only by a monster with no enemy, and
  never by ambushers.
- No sight sound plays on hearing.

Ours: any player's noise within 1000 makes the player the enemy directly, as if seen. There is no
PHS or area test and no noise point. The monster goes straight to hunting the player.

### 15. Light (rank 2)

Original [read]: a player standing in light level 5 or less is invisible to monsters unless he
fired.

Ours: no light test.

### 16. Melee-range sight (rank 2)

Original [read]: at melee range the facing test is skipped. At near range the target must be in
front, unless he showed hostility in the last second. At mid range and beyond it must be in
front.

Ours: melee range still needs the target in front.

### 17. First attack delay (rank 2)

Original [read]: on first sighting a monster waits 1 s before it may attack, unless it is
standing its ground.

Ours: no delay.

### 18. Losing the player (rank 2)

Original [read]: while the player is in view, the search time is kept at now+5. Once it has run
out by 20 s, the monster moves once and then follows the player's trail of recent positions. It
never forgets its enemy.

Ours: it goes to the last place it saw the player, stops within 40, and drops its enemy after 20 s
unseen.

### 19. Facing before attack (rank 2)

Original [read]: a monster turns to face the player and attacks only within 45 degrees of him.

Ours: no facing gate [derived from our code reading].

### 20. Attacking on the way to point_combat (rank 2)

Original [read]: a monster running to a combat point does not attack until it arrives.

Ours: it attacks on the way.

### 21. path_corner (rank 2)

Original [read]: `wait` on a corner holds the monster there for that many seconds (−1 waits for
ever). `pathtarget` is fired on arrival. If the next corner has spawnflag 1, the monster
teleports to it.

Ours: none of the three.

### 22. point_combat chaining (rank 2)

Original [read]: a point_combat's `target` names the next point to run to, and its `pathtarget`
fires on arrival.

Ours: neither.

### 23. Using a monster (rank 2)

Original [read]: using an awake monster that has no enemy makes it angry at the activator (if the
activator is a player, not a no-target one).

Ours: a use only wakes a triggered spawn.

### 24. Triggered spawn with ambush (rank 2)

Original [read]: a triggered monster appears 0.1 s after the use, raised 1 unit, and kills anything
in its box. If it also has the ambush flag, it does not target the activator. Spawnflag 4 on a
triggered spawn is turned into ambush.

Ours: it finds the activator even with the ambush flag. We treat flags 1 and 4 both as ambush,
which matches [derived].

### 25. Pain hold (rank 2)

Original [read]: pain plays the kind's pain animation for its full length. For soldiers it adds
a stun of 0.5 to 1.8 s. The pain debounce is 3 s, or 5 s at skill 3, where no pain animation
plays (the hornet's pain animations still play there).

Ours: every kind is held 0.3 s on pain, never at skill 3.

### 26. Idle, search and loop sounds (rank 2)

Original [read]: most monsters play an idle sound now and then while standing. Examples are
soldier/solidle1 (20%) and infantry/infidle1 with a fidget animation. Some play a search sound
while hunting (infantry/infsrch1, tank, supertank btkunqv1 and 2, and others). Several carry a
looping sound: flyer flyidle1, floater fltsrch1, hover hovidle1, hornet bhvengn1, and the Jorg's
attack loop w_loop.wav. The brain has an idle brnlens1. The parasite's tap sounds play while
idle.

Ours: none of these, apart from footsteps.

### 27. Soldier aim spread (rank 2)

Original [read]: every soldier spreads its aim by 1000 sideways and 500 up and down at 8192
before firing. This makes the light soldier's blaster bolts miss sometimes. The shotgun soldier's
12 pellets and the SS soldier's bullets (300×500) add their own spread on top.

Ours: the light soldier's bolt has no spread. The others use only one spread.

### 28. Soldier death sounds (rank 2)

Original [read]: the light soldier dies with soldier/soldeth2, the shotgun soldier with soldeth1
and the SS soldier with soldeth3.

Ours: soldeth1 for the light soldier and soldeth2 for the shotgun soldier, which is swapped. The
SS soldier is right.

### 29. Soldier special moves (rank 2)

Original [read]:
- Run-and-shoot: on sighting, at skill above 0, at mid range or more, 50% of the time, it fires
  while running (frames 109 to 122, the shot at 112).
- Dying shots: death 1 fires at frames 293 and 296.
- Head shots: a hit within 4 units of eye height plays death 3 (frames 343 to 387). Otherwise
  the death is picked at random from 1, 2, 4, 5 and 6.
- Attack 1 (frames 0 to 11) fires at frame 2. Attack 2 (frames 12 to 29) fires at frame 16.
  Either refires at skill 3 (50%) or at melee range.
- The SS soldier fires from frame 41 for 0.3 to 1.0 s. The cock sound is infantry/infatck3.
- The sight sound is solsght1 or solsrch1 at random.
- Gibs: 3 small meat, a chest and a head.

Ours: no run-and-shoot, no dying shots, no head-shot death. Deaths are picked at random among
all the runs.

### 30. Gunner grenades (rank 2)

Original [read]: the grenade attack throws four grenades, at frames 112, 115, 118 and 121. They go
straight along the gunner's facing, 50 damage at 600.

Ours: one grenade (50 at 600) per attack, aimed.

### 31. Gunner chaingun (rank 2)

Original [read]: the gunner opens at frame 137 (gunatck1) with a 0.7 s windup, then fires 8
bullets over frames 144 to 151, refiring 50% of the time. It aims 0.2 s behind the player. At
melee range it always uses the chaingun. Otherwise it picks chaingun or grenades 50/50. Its pain
animation is chosen by damage (10 or less, 25 or less, more), with sounds gunpain1 and 2.

Ours: weights 1.5 and 1 between burst and grenade at all ranges.

### 32. Tank volleys (rank 2)

Original [read]:
- Blaster: 3 bolts at frames 64, 67 and 70. At skill 2 and up, 60% of the time 2 more follow.
- Rockets: the windup is frames 115 to 135, with rockets at 138, 141 and 144. At skill 2 and up
  it refires 40% of the time, and firing sets a 5 s pain debounce.
- Machine gun: 19 bullets of 20 over frames 173 to 191, sweeping 8 degrees of yaw per frame,
  with the pitch aimed.

Ours: one bolt of 30, three rockets of 50 with no refire, and a plain burst of 20.

### 33. Tank attack choice (rank 2)

Original [read]: within 125, 40% machine gun and 60% blaster. Within 250, 50/50. Farther, a
third each for blaster, rockets and machine gun.

Ours: equal weights at every range.

### 34. Flyer attacks (rank 2)

Original [read]: 8 blaster bolts of 1 at 1000, over frames 82 to 89 from alternating sides, after
0.3 s. In melee it slashes for 5 at frames 66 and 71 and repeats 80% of the time while in reach.

Ours: 2 bolts in one volley, and a single club of 5.

### 35. Berserker (rank 2)

Original [read]: the spike does 15 to 20 with kick 400 at frame 79. The club does 5 to 10 with
kick 400 at frame 92. It picks 50/50. Pain 1 plays when damage is under 20, or 50% of the time.
Death 1 plays when damage is 50 or more.

Ours: one blow of 15 to 20 with kick 400.

### 36. Infantry (rank 2)

Original [read]:
- It has no melee slot. At melee range it uses attack 2 (frames 199 to 206): the swing sound at
  201, the hit at 204 for 5 to 9 with kick 50.
- Gun: it cocks at frame 187 (infatck3) and fires from frame 194. The burst lasts until 1.0 to
  2.5 s after the cock. It aims 0.2 s behind the player.
- Death 2 sprays 12 bullets over frames 155 to 166 at fixed angles.
- Pain sounds infpain1 and 2, death sounds infdeth1 and 2.

Ours: a melee club of 5 to 9, a burst lasting 0.5 to 1 s, one pain and one death sound, no dying
spray.

### 37. Monsters in liquids (rank 2)

Original [read]: monsters take lava damage (10 × water level every 0.2 s), slime damage (4 × water
level every 1 s), and drown.

Ours: no liquid damage for monsters.

### 38. Silencer (rank 2)

Original [read]: the silencer lasts 30 shots. It marks the muzzle flash as silenced, so the shot
is still heard at a lower volume, and the shot makes no noise that wakes monsters.

Ours: no sound at all and no monster noise.

### 39. Powerup and quad sounds (rank 2)

Original [read]: items/damage3.wav plays on each quad-boosted fire frame. Using quad plays
items/damage.wav, and using invulnerability plays items/protect.wav. A hit while invulnerable
plays items/protect4.wav, at most every 2 s.

Ours: none of these.

### 40. Powerup warnings (rank 2)

Original [read]: quad, invulnerability, enviro suit and rebreather last 30 s. At 3 s left the
warnings play: items/damage2.wav for quad, items/protect2.wav for invulnerability, and
items/airout.wav for the breather and the suit.

Ours: no warnings. The durations match.

### 41. Item spawnflags (rank 2)

Original [read]: item spawnflag 1 hides the item until something targets it (it then appears).
Spawnflag 2 makes it untouchable.

Ours: spawnflag 1 is treated as untouchable-looking (no rotate or glow), and the item is still
touchable and visible from the start.

### 42. Item targets (rank 2)

Original [read]: the first touch of an item fires its `target` entities, with the player as
activator.

Ours: not fired.

### 43. Noise sources (rank 2)

Original [read]: the player makes noise when he jumps, takes pain, and fires. Impacts of bullets,
blaster bolts, rockets, grenades, rails and the BFG make impact noise.

Ours: firing only.

### 44. First grenade pickup (rank 2)

Original [read]: in single player, picking up grenades for the first time switches to them, under
the usual rule for a new weapon.

Ours: grenades never switch.

### 45. BFG timing (rank 2)

Original [read]: the firing sound and the noise come at weapon frame 9 and the ball leaves at frame
17. The ball has no time limit.

Ours: the sound plays at frame 17, and the ball is removed after 20 s.

### 46. Power armor messages (rank 2)

Original [read]: switching power armor on plays misc/power1.wav and off plays misc/power2.wav.
With no cells the player sees "No cells for power armor.".

Ours: none.

### 47. misc_insane (rank 2)

Original [read]:
- Spawnflags: 4 crawl, 8 crucified (hung, box 16 wide and 8 deep, no knockback), 16 hold
  ground (lies down), 32 always stand.
- It stands, sits down and gets up at random, crawls, and walks in two styles.
- Crucified prisoners loop frames 252 to 266, with a struggle on frames 267 to 281.
- Screams pick from eight insane/insane*.wav files. It also has a fist sound (insane11), a shake
  (insane5) and a moan (insane7).
- Pain sounds are player/male/pain25, 50, 75 and 100 by health, _1 or _2.
- Death sounds are player/male/death1 to 4.
- The death animation depends on its pose: crawl death 245 to 251, stand death 210 to 226.
- Skin 0 to 2 at random.

Ours: crucified hangs in place with the right box, but plays the standing frames. It has one
pain sound (pain50_1) and one death sound (death1). It does not crawl, lie down or scream, and its
skin is fixed.

### 48. Weapon detail sounds (rank 3)

Original [read]: the hand grenade's pin sound weapons/hgrena1b plays at frame 5. The chaingun
spins up with weapons/chngnu1a at frame 5 and down with chngnd1a. The hyperblaster ends with
weapons/hyprbd1a.

Ours: none.

### 49 to 54. Small AI points (rank 3)

- 49 [read]: fliers slide sideways 30% of the time while closing in, when they do not attack.
  Ours does not.
- 50 [read]: at skill 3 the pain debounce is 5 s and no pain sound or animation plays. Ours
  plays the sound and uses a 3 s debounce.
- 51 [read]: a monster that is ducking gets no pain reaction. Ours gives it.
- 52 [read]: monsters start on a random frame of their stand animation. Ours starts on the first.
- 53 [read]: fliers take knockback like everything else. Ours skips them.
- 54 [read]: the default eye height is 25 for walkers and fliers and 10 for swimmers. Ours uses 10
  for fliers too.

### 55. Pain animation choice (rank 3)

Original [read]: many kinds choose their pain animation by damage:
- gunner: 10, 25,
- chick: 10, 25,
- hornet: under 10, under 30,
- supertank and Makron: 25 or less ignored 20% of the time; Makron 40, 110, 150, with the
  biggest playing only 45% or 35% of the time,
- tank: 10 or less none, 30 or less ignored 80%, none during attacks at skill 2 and up.

The gladiator plays an air pain when knocked up.

Ours: picks at random among the kind's pain runs, never over an attack.

### 56. Sound variety (rank 3)

Original [read]: two or three pain sounds and often two death sounds on the chick, gladiator,
gunner, infantry, hover, hornet and supertank. Supertank pain is btkpain1, 3 or 2 by damage.

Ours: one pain and one death file per kind.

### 57. Sight sounds (rank 3)

Original [read]: soldiers pick solsght1 or solsrch1, and the infantry's plays on the body channel.

Ours: always solsght1. The supertank has no sight sound in either.

### 58. Corpse boxes (rank 3)

Original [read]: the tank's corpse box is −16 −16 −16 to 16 16 0. The chick's is −16 −16 0 to 16
16 16. The Makron's is −60 −60 0 to 60 60 72.

Ours: a corpse keeps its kind's box with the top cut to −8. For the chick, whose box starts at 0,
that makes the top lie below the bottom.

### 59. Flies (rank 3)

Original [read]: 50% of infantry and mutant corpses get flies after 5 to 15 s, for 60 s, with
infantry/inflies1.wav.

Ours: none.

### 60. Tank extras (rank 3)

Original [read]: the tank stomps a dead enemy until its health reaches −80. It thuds with
tnkdeth2 at death frame 249, and its rocket windup plays tnkatck4.

Ours: none.

### 61. Supertank extras (rank 3)

Original [read]: pain sounds by damage. Pain is ignored at 25 or less 20% of the time, and none
plays during rockets at skill 2 and up. The tread sound btkengn1 plays each run loop. Search
sounds btkunqv1 and 2.

Ours: btkpain2 only, and none of the rest.

### 62. Hornet (monster_boss2) extras (rank 3)

Original [read]: its pain, death and search sounds are heard across the whole map, and it loops
bhvengn1. It is immune to BFG lasers.

Ours: normal attenuation and no loop.

### 63. Makron extras (rank 3)

Original [read]:
- Stand frames 414 to 473, run 477 to 486.
- Attacks: BFG 30% (frame 204, makron/bfg_fire.wav), the 17-bolt hyperblaster sweep 30%
  (frames 213 to 229, sweeping in yaw by 10 degrees per frame), rail 40% (rail_up at 235, the aim
  saved at 242, the shot at 243).
- Pain sounds pain3, 2, 1 by damage, heard map-wide. Pain 6 plays popup and a taunt (voice4,
  voice3 or voice).
- Death leaves a torso on the floor 84 units to one side, looping makron/spine.wav. The corpse
  plays brain1 and bhit near the end.
- At its arrival from the Jorg it is thrown towards the sight client at 400 forward and 200 up.

Ours matches the attack weights, numbers and the throw. It runs frames 474 to 490 and has no
torso, taunts, popup, brain or hit sounds.

### 64. Jorg (rank 3)

Original [read]: 75% machine guns (bs3atck1, with w_loop.wav looping), else the BFG at frame 24
with bs3atck2. The guns start on frames 0 to 7, fire on 8 to 13, and repeat 90% of the time. The
attack chance is 0.8 at melee, 0.4 near, 0.2 mid. The death runs 50 frames, the Makron toss at
79, then the explosion.

Ours: weights 0.75 and 0.25, the same chances, and the Makron brought out at the last blast,
about 5.6 s after death [derived], close to the original's. Only the sounds differ.

### 65. Mutant skin (rank 3)

Original [read]: the mutant switches to skin 1 at death. Its melee runs at melee range with no
visibility test, and its leap hurts anything damageable it lands on.

Ours: no skin change. Melee and leap are as the original [derived].

### 66. Soldier knocked up (rank 3)

Original [read]: pain 4 (frames 80 to 96) plays when a soldier's vertical speed is over 100.

Ours: no.

### 67. jail5 flyer (rank 3)

Original [read]: on the map jail5, a flyer placed at height −104 has its target replaced by its
own targetname. This works around a map fault.

Ours: not applied.

### 68. Ammo count (rank 3)

Original [read]: a map may set `count` on an ammo item to change the amount given. This was not
checked in ours, and is listed as a point to confirm.

## Checked and found the same

- [read] Every stock monster's health and gib health, mass and box. This covers the light,
  shotgun and SS soldiers, infantry, gunner, berserker, parasite, flyer, tank, tank commander,
  medic, brain, chick (iron maiden), gladiator, mutant, floater (technician), hover (icarus),
  flipper (barracuda), supertank, hornet, Jorg, Makron and misc_insane.
- [read] Monster attack damage and speed where not listed above. This covers the medic's
  hyperblaster and cable (reach 256, pitch 45 degrees, revive at frame 226, heal sound at 220),
  the brain's tentacles and screen, the chick's slash and rockets, the gladiator's cleaver and
  rail, the mutant's claws and leap, the floater's zap and bolts, the hover's bolts, the
  flipper's bite, the supertank's chaingun and rockets, and the hornet's guns and rockets.
- [read] Attack ranges (melee 80, near 500, mid 1000), the standard attack chances and their skill
  scaling, the 0.4 chance when standing ground, the 2 × random attack delay, the turn rates, the
  model scale on frame distances (soldier 1.2, gunner 1.15), and the step height.
- [read] Melee reach 80. The melee kick is applied as a push, not as knockback. The knockback
  formula and the minimum mass.
- [read] Monster rocket splash (damage + 20) and monster grenade fuse and splash (2.5 s,
  damage + 40).
- [read] Easy skill halves damage to the player.
- [read] Sight test through the opaque mask, the 20-degree and 10-degree turns, the medic's
  search and revive, and the triggered-spawn telefrag.
- [read] Player weapons: damage, pellet counts and spread, rate of fire by frame, ammo use, kick,
  quad ×4 damage, and the switch-on-pickup rule for a newly owned weapon.
- [read] Items: every amount and maximum, the bandolier and ammo pack raises, armor kinds and
  their absorption, power screen and shield cell costs for the player, adrenaline (+1 max health
  and a full heal), mega health and its drain (the first point 5 s after pickup, then 1 a second),
  the stacking of powerup time, enviro suit and rebreather protection, and keys.
- [read] Monster-dropped items, monster footsteps (the heavy kinds always; the light ones under a
  setting), and ducking.
