# Quake II monsters: round 4 facts (pains and deaths by kind, the tank, the bosses, flies, weapon sounds, fliers, idle fidgets)

Clean-room reader's answers, kept for the single player 1-1 work (q2-sp-gaps.md). No code.

Source read: the id Software game library as released (version 3.21, the plain baseq2 game,
no mission packs, no forks). Frames are 0-based model frame indexes, resolved from each
kind's frame table. Attenuations: none 0, normal 1, idle 2, static 3. "Debounce" means the
time after a pain reaction during which further pain reactions are refused.

## 0. Rules shared by every kind (read once, they apply below)

- When a monster is hurt and survives, the game first lets it react to the attacker, then
  calls its pain reaction, but only if damage was actually taken after armour, and only if
  the monster is NOT currently in the "ducked" state. A ducked monster gets no pain reaction
  at all: no sound, no animation, no skin change, no debounce change. [read]
- On nightmare (skill 3), right after every pain reaction call, the debounce is pushed to
  5 seconds from now, overriding whatever the kind set. [read]
- Every kind below that has a debounce uses 3 seconds. [read]
- "Hurt skin": every kind that has one checks it at the very start of each pain call, before
  the debounce test: if current health is below half the starting health (integer half),
  the hurt skin is shown. So it changes even on hits that are otherwise ignored. [read]
- When standing (not spawned with the ambush flag), a monster with an idle reaction calls it
  on a timer: the first time the timer is merely armed for 0 to 15 s later; after that the
  idle reaction plays and the next one is 15 to 30 s later. While walking its beat, the same
  timer drives the search reaction. Idle and search share one timer. [read]
- Monsters that "duck" (and the frames during which they count as ducked, i.e. pain is
  skipped): soldier duck run 45-49, ducked from 45 until 48; soldier duck-and-shoot run
  30-38, ducked from 32 until 36; infantry duck 120-124, ducked 120 until 123; gunner duck
  201-208, ducked 201 until 207; brain duck 146-153, ducked 147 until 150; chick duck 83-89,
  ducked 83 until 87; medic duck 131-146, ducked 133 until 137; mutant leap: ducked from
  the take-off at frame 2 until it lands. [read; frame windows derived from the runs]
- The common gib sound is misc/udeath.wav, voice channel, attenuation normal (the insane
  marine uses idle). [read]

## 1. Pain per kind

Soldier (light, normal, SS) [read]
- Debounce 3 s.
- Hurt skin: adds 1 to the skin (light 0 to 1, normal 2 to 3, SS 4 to 5) below half health.
- Pain sound, played whenever the debounce allows (before the nightmare test, so nightmare
  still plays it): chosen by type, not random: light soldier/solpain2.wav, normal
  soldier/solpain1.wav, SS soldier/solpain3.wav; voice, normal.
- If knocked upward (vertical speed above 100) at that moment: pain run 4, frames 80-96,
  even on nightmare.
- Nightmare: sound but no animation (except the upward case above).
- Otherwise random: under 0.33 pain run 1 (50-54), under 0.66 pain run 2 (55-61), else pain
  run 3 (62-79). No damage thresholds, no chance to ignore.

Infantry [read]
- Debounce 3 s; hurt skin 1.
- Nightmare: nothing (no sound, no animation); the nightmare test comes before the sound.
- Otherwise 50/50: pain run 1 (100-109) with infantry/infpain1.wav, or pain run 2 (110-119)
  with infantry/infpain2.wav; voice, normal.

Gunner [read]
- Debounce 3 s; hurt skin 1.
- Sound 50/50 gunner/gunpain2.wav or gunner/gunpain1.wav, voice, normal, played before the
  nightmare test (nightmare: sound, no animation).
- Damage 10 or less: 185-189; 11 to 25: 177-184; above 25: 159-176.

Chick (Iron Maiden) [read]
- Debounce 3 s; hurt skin 1.
- Sound by random thirds: chick/chkpain1.wav, chkpain2.wav, chkpain3.wav; voice, normal;
  played before the nightmare test.
- Damage 10 or less: 90-94; 11 to 25: 95-99; above 25: 100-120.

Gladiator [read]
- Debounce 3 s; hurt skin 1.
- Sound 50/50 gladiator/pain.wav or gladiator/gldpain2.wav; voice, normal; before the
  nightmare test.
- If moving upward faster than 100: air pain 83-89, else ground pain 55-60 (no damage
  thresholds). Nightmare: sound only.

Berserker [read]
- Debounce 3 s; hurt skin 1.
- Sound berserk/berpain2.wav always; voice, normal; before the nightmare test.
- Damage below 20, or a 50% chance otherwise: short pain 199-202; else long pain 203-222.

Medic [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing (test before the sound).
- 50/50: 108-115 with medic/medpain1.wav, or 116-130 with medic/medpain2.wav; voice, normal.

Brain [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- Random thirds: under 0.33 frames 88-108 with brain/brnpain1.wav; under 0.66 frames
  109-116 with brain/brnpain2.wav; else frames 117-122 with brain/brnpain1.wav. Voice,
  normal.

Parasite [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- One pain run 57-67; sound 50/50 parasite/parpain1.wav or parpain2.wav; voice, normal.

Mutant [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- Random thirds: 34-38 with mutant/mutpain1.wav; 39-44 with mutant/mutpain2.wav; 45-55 with
  mutant/mutpain1.wav. Voice, normal. No pain while in the air during its leap (ducked).

Flyer [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- Random thirds: 134-142 with flyer/flypain1.wav; 143-146 with flyer/flypain2.wav; 147-150
  with flyer/flypain1.wav. Voice, normal.

Hover (Icarus) [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- Damage 25 or less: 50/50 between frames 153-161 with hover/hovpain1.wav and frames 141-152
  with hover/hovpain2.wav. Above 25: frames 113-140 with hover/hovpain1.wav. Voice, normal.

Floater (Technician) [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- One chance in three: 117-123 with floater/fltpain1.wav; two in three: 124-131 with
  floater/fltpain2.wav. Voice, normal. The third pain run (132-143) exists but is never used.

Flipper (Barracuda) [read]
- Debounce 3 s; hurt skin 1. Nightmare: nothing.
- 50/50: 99-103 with flipper/flppain1.wav, or 94-98 with flipper/flppain2.wav. Voice, normal.

Tank and Tank Commander (same code) [read]
- Hurt skin: adds 1 (tank 0 to 1, commander 2 to 3).
- Damage 10 or less: ignored completely (no sound, debounce untouched).
- Then the 3 s debounce.
- Damage 11 to 30: ignored 80% of the time.
- Hard and nightmare: ignored while in the rocket run frames 115-144 or the blaster run
  frames 55-70.
- Sound tank/tnkpain2.wav, voice, normal, played before the nightmare test (nightmare:
  sound, no animation).
- Damage up to 30: 197-200; 31 to 60: 201-205; above 60: 206-221 (a footstep at 221).
- Also: starting a rocket attack (only chosen beyond 250 units) sets the debounce to 5 s
  ahead, so no pain reaction for 5 s. [read]

Supertank [read]
- Hurt skin 1. Debounce 3 s.
- Damage 25 or less: ignored 20% of the time.
- Hard and nightmare: ignored while in the rocket run frames 20-33.
- Nightmare: nothing (no sound, no animation), test comes before the sound.
- Damage 10 or less: bosstank/btkpain1.wav, 164-167; 11 to 25: bosstank/btkpain3.wav,
  168-171; above 25: bosstank/btkpain2.wav, 172-175. Voice, normal.

Boss2 (Hornet) [read]
- Hurt skin 1. Debounce 3 s. No chance to ignore, and no nightmare rule of its own (it still
  animates on nightmare, but the shared 5 s nightmare debounce applies).
- Damage below 10: bosshovr/bhvpain3.wav and light pain 128-131; 10 to 29:
  bosshovr/bhvpain1.wav and light pain 128-131; 30 and above: bosshovr/bhvpain2.wav and
  heavy pain 110-127. Voice, attenuation NONE.

Jorg [read]
- Hurt skin 1. Every pain call (even refused ones inside the debounce) silences the
  machine-gun loop sound.
- Debounce 3 s.
- Damage 40 or less: ignored 60% of the time.
- Frame-based ignore chances (any skill): frames 0-7 0.5%; frames 8-13 0.005%; frames
  18-25 0.5% (effectively almost never).
- Nightmare: nothing.
- Damage up to 50: boss3/bs3pain1.wav, 81-83; 51 to 100: boss3/bs3pain2.wav, 84-86; above
  100: 30% chance of boss3/bs3pain3.wav with 87-111, otherwise nothing at all (the debounce
  is still spent). Voice, normal.

Makron [read]
- Hurt skin 1. Debounce 3 s.
- Damage 25 or less: ignored 20% of the time.
- Nightmare: nothing.
- Damage up to 40: makron/pain3.wav, 379-382; 41 to 110: makron/pain2.wav, 383-386; 111 to
  150: 45% chance of makron/pain1.wav with 387-413; above 150: 35% chance of the same;
  otherwise nothing (debounce spent). Voice, attenuation NONE.

Insane marine (for completeness) [read]
- No hurt skin. Debounce 3 s.
- Sound player/male/painNN_M.wav where NN is 25 if health is under 25, 50 if under 50, 75 if
  under 75, else 100, and M is 1 or 2 at random; voice, idle; before the nightmare test.
- Crucified: struggle run 267-281. Lying or crawling (frames 98-159 or 227-235): crawl pain
  236-244. Otherwise standing pain 199-209.

## 2. Death per kind

All regular death sounds below are on the voice channel. Gib means health at or below the
listed value when killed. [read unless marked]

- Soldier: gib -30 (all three types). Death sound by type: light soldier/soldeth2.wav,
  normal soldier/soldeth1.wav, SS soldier/soldeth3.wav; normal. Skin set to the hurt skin.
  If the hit point is within 4 units (vertically) of eye height (25 above the origin):
  head-shot death 343-387. Otherwise one of five at random: 272-307, 308-342, 388-440,
  441-464, 465-474. The 272-307 run fires two shots straight ahead at frames 293 and 296
  (its weapon's sound comes from the muzzle flash).
- Infantry: gib -40. One third each: 125-144 with infantry/infdeth2.wav; 145-169 with
  infantry/infdeth1.wav (fires its machine gun every frame 155-166 while falling); 170-178
  with infantry/infdeth2.wav. Normal.
- Gunner: gib -70. One run 190-200, gunner/death1.wav, normal.
- Chick: gib -70. 50/50: 48-59 with chick/chkdeth1.wav, or 60-82 with chick/chkdeth2.wav.
  Normal.
- Gladiator: gib -175. One run 61-82, gladiator/glddeth2.wav, normal.
- Berserker: gib -60. berserk/berdeth2.wav, normal. Killing blow of 50 or more: 223-235;
  less: 236-243.
- Medic: gib -130. One run 147-176, medic/meddeth1.wav, normal. Releases its patient.
- Brain: gib -150. brain/brndeth1.wav, normal. 50/50: 123-140 or 141-145. Its power screen
  and effects are removed at death.
- Parasite: gib -50. One run 32-38, parasite/pardeth1.wav, normal.
- Mutant: gib -120. mutant/mutdeth1.wav, normal. 50/50: 15-23 or 24-33. Skin set to 1.
- Flyer: never leaves a corpse: flyer/flydeth1.wav (normal) and it instantly becomes a small
  explosion. No gib threshold applies.
- Hover: gib -100 (gibs: two bones, two meat, and meat as the head). 50/50
  hover/hovdeth1.wav or hover/hovdeth2.wav, normal. Run 162-172, then it falls under
  gravity and explodes when it touches ground, or after 15 s at the latest.
- Floater: never leaves a corpse: floater/fltdeth1.wav (normal) and instant explosion.
- Flipper: gib -30. One run 104-159, flipper/flpdeth1.wav, normal.
- Tank: gib -200 (commander -225), metal gibs. One run 222-253, tank/death.wav, normal. Thud
  tank/tnkdeth2.wav at frame 249, body, normal.
- Supertank: cannot be gibbed (stops taking damage at death). bosstank/btkdeth1.wav,
  normal. Run 98-121; at 121 a chain of explosions begins: 8 explosions 0.1 s apart around
  the body (24 then 48 units out on the diagonals, 24-39 units up), then a final burst of
  gibs (4 meat, 8 metal, a chest, and a gear as the head).
- Boss2 (Hornet): cannot be gibbed. bosshovr/bhvdeth1.wav, attenuation NONE. Run 132-180;
  at 180 the same explosion chain as the supertank. Its engine loop ends with the last step.
- Jorg: cannot be gibbed. boss3/bs3deth1.wav, normal; machine-gun loop silenced. Run 31-80;
  at 79 the Makron is launched (see 7 and 8), at 80 the explosion chain.
  boss3/d_hit.wav is precached but never played. [read]
- Makron: gib -2000 (metal). makron/death.wav, attenuation NONE. One run 251-345 (the
  20-frame run 346-365 is used only by the torso). Frame sounds: footsteps (makron/step1.wav
  left, makron/step2.wav right; body, normal) at 254 L, 268 R, 286 L, 306 R, 308 L, 315 R,
  317 L, 320 R, 323 L; makron/bhit.wav at 341 (auto channel, attenuation none);
  makron/brain1.wav at 343 (voice, normal).
- Insane marine: gib -50 (udeath at idle attenuation). player/male/deathN.wav with N 1-4 at
  random, voice, idle. Crucified: no run, stays on the cross. Lying or crawling: 245-251;
  else 210-226.

## 3. Velocity-based pain

- Soldier [read]: if its upward speed is above 100 when a pain reaction is allowed, it plays
  pain run 4, frames 80-96 (with the normal pain sound), on any skill including nightmare.
  Also, while still inside the debounce, if it is currently playing pain run 1, 2 or 3 and
  its upward speed is above 100, it switches to pain run 4 with no sound.
- Gladiator [read]: if upward speed is above 100 when pain is allowed, air pain 83-89
  instead of 55-60; not on nightmare (the nightmare test comes first). Inside the debounce,
  if it is playing the ground pain 55-60 and is moving up faster than 100, it switches to the
  air pain with no sound (this path works on nightmare too).
- No other kind looks at velocity in its pain reaction. [read]

## 4. Tank

- Stomp [read]: while its enemy is a player, the tank treats that enemy as alive until the
  enemy's health falls to -80 or lower. If it decides to attack and the enemy's health is
  below 0, it plays the "strike" run 77-114 instead of shooting: footsteps at 83 and 87,
  windup tank/tnkatck4.wav at 95 (weapon, normal), strike tank/tnkatck5.wav at 102 (weapon,
  normal), footstep at 114. It deals NO damage. At the end it forgets its enemy and returns
  to running (or standing). The "keep attacking the corpse" mode is dropped when the strike
  starts.
- Death thud: tank/tnkdeth2.wav at frame 249, body, normal. [read]
- The rocket run has no windup sound of its own: pre-rocket 115-135 (footstep at 130),
  rockets at 138, 141, 144 (the launch sound comes from the muzzle flash), post-rocket
  145-167 (footstep at 160). On hard and nightmare it fires another volley 40% of the time if
  the enemy is visible and alive. The windup sound tank/tnkatck4.wav belongs to the strike.
  [read]
- Footsteps tank/step.wav (body, normal) at 33 (start walk/run), 41 and 49 (walk/run cycle
  34-49), 54 (stop), 76 (after blaster), 83, 87, 114 (strike), 130, 160 (rocket), 221 (pain
  3). [read]
- Blaster shots at 64, 67, 70 (sound from the muzzle flash); on hard and nightmare it
  repeats frames 65-70 60% of the time if the enemy is visible and alive. Machine gun every
  frame 173-191. Sight sound tank/sight1.wav (voice, normal). Idle tank/tnkidle1.wav (voice,
  idle), no idle animation. No search sound. [read]

## 5. Supertank

- Tread sound bosstank/btkengn1.wav: a one-shot (not a loop), voice channel, attenuation
  normal, played on the first frame of each movement cycle: frame 128 (forward/run cycle
  128-145, so once every 18 frames), 146 (turn left), 176 (turn right), 80 (backward). The
  same file as an entity loop sound is commented out, so there is no engine loop. [read]
- Search: 50/50 bosstank/btkunqv1.wav or bosstank/btkunqv2.wav, voice, normal (on the
  shared idle/search timer while walking). [read]
- No sight sound (its sight reaction is empty). No idle sound. [read]
- Pain ignore rules exactly (in order): debounce 3 s; damage 25 or less ignored with chance
  0.2; on hard and nightmare ignored during frames 20-33; nightmare then stops before sound
  and animation (debounce already spent). See section 1 for the sound and run by damage.
  [read]

## 6. Boss2 (Hornet)

- Attenuation NONE: all three pain sounds (bhvpain1, bhvpain2, bhvpain3), the death sound
  bhvdeth1, and the search sound bhvunqv1 (which plays only half the times the search
  reaction fires). [read]
- Looping sound: bosshovr/bhvengn1.wav from spawn, as the entity's loop (loops are heard at
  static attenuation, see 9); it stops when the death explosion chain finishes. [read]
- BFG: it is flagged immune to lasers, so the BFG ball's in-flight laser beams do not hurt it
  (the beam passes through and goes on to others). It still takes the BFG's direct blast,
  splash and the post-impact effect damage. The same immunity protects it from laser
  trap beams. [read]

## 7. Makron

- Stand 414-473 (loops), walk and run both 477-486 (footsteps makron/step1.wav at 477,
  makron/step2.wav at 481; body, normal). [read]
- Spawn starts in its sight run 188-200 (silent), then runs. Its sight reaction plays that
  same run again, with no sound: the Makron has no sight sound. [read]
- Torso left on death [read]: a separate object using the Makron rider model, placed at the
  body's position moved 84 units along the world's negative Y axis (not relative to its
  facing), same angles. Starts on frame 346 and, from 0.2 s later, advances one frame every
  0.1 s through 346-364, then wraps to 346 forever (frame 365 is never shown). Loop sound
  makron/spine.wav (static attenuation, as all entity loops). It never expires. It has no
  box (zero size), is not solid, does not move, and cannot be shot.
- Death run sounds: see section 2 (steps; bhit at 341; brain1 at 343; death.wav at the
  start, attenuation none). [read]
- Pain sounds by damage (voice, attenuation none): up to 40 makron/pain3.wav (379-382);
  41-110 makron/pain2.wav (383-386); above 110 makron/pain1.wav with the long run 387-413
  (45% chance up to 150, 35% above). [read]
- The long pain run 387-413 plays makron/popup.wav at 402 (body, attenuation none) and a
  taunt at 410 (auto channel, attenuation none): random under 0.3 makron/voice4.wav, under
  0.6 makron/voice3.wav, else makron/voice.wav. [read]
- Attacks for reference: BFG run 201-208 fires at 204 with makron/bfg_fire.wav (voice,
  normal); hyperblaster run 209-234 fires every frame 213-229; railgun run 235-250,
  makron/rail_up.wav at 235 (weapon, normal), aim saved at 242, fires at 243. Chosen 30% /
  30% / 40%. [read]
- Arrival from Jorg: 0.8 s after Jorg's frame 79 the Makron appears at Jorg's position,
  facing the player who last woke monsters, thrown toward that player at 400 horizontally
  and 200 up. [read]

## 8. Jorg

- Attack choice: 75% machine guns: boss3/bs3atck1.wav (voice, normal) at the moment of
  choosing, and the loop boss3/w_loop.wav starts then; run 0-7 (wind-up), 8-13 firing both
  guns each frame; at 13, if the enemy is visible it repeats 8-13 90% of the time, else
  (10%, or not visible) the loop stops and it plays 14-17. 25% BFG: boss3/bs3atck2.wav at the
  moment of choosing, and again at frame 24 when the ball is fired (run 18-30). [read]
- The loop is stopped by: the end of the burst (above), any pain call (even ones ignored by
  the debounce), and death. [read]
- boss3/xfire.wav is precached but never played. [read]
- Death: boss3/bs3deth1.wav, voice, normal. [read]
- Pain sounds by damage: see section 1 (bs3pain1 up to 50, bs3pain2 up to 100, bs3pain3 30%
  above 100; voice, normal). [read]
- Search: under 0.3 boss3/bs3srch1.wav, under 0.6 boss3/bs3srch2.wav, else
  boss3/bs3srch3.wav; voice, normal. No sight sound. [read]
- Idle: boss3/bs3idle1.wav on the first stand frame 112, voice, attenuation NORMAL, every
  time the 51-frame stand loop (112-162) restarts. [read]
- Footsteps boss3/step1.wav (left) and step2.wav (right), body, normal: stand 146 L, 150 R,
  159 L, 162 R; walk/run cycle 168-181: 168 L, 175 R; long pain: 89 L, 91 R, 107 L, 111 R.
  [read]

## 9. Flies on corpses

- Kinds: infantry and mutant only. [read]
- When the death run finishes (the corpse settles), if the corpse is not in water, there is
  a 50% chance; if it passes, the flies start 5 to 15 s later (uniform). If the corpse is in
  water when they would start, they do not start. [read]
- They last 60 s, then both the effect and the sound are removed. [read]
- Sound: infantry/inflies1.wav as the corpse's own looping sound (not repeated one-shots);
  entity loops are mixed with a fixed distance falloff equal to static attenuation (3).
  [read for the loop; attenuation read from the client's loop mixing]
- Visual: the corpse carries the "flies" render effect flag for the same 60 s. [read]

## 10. Mutant skins

- Hurt skin: skin 1 below half health. At death the skin is set to 1 as well (so a corpse is
  always the hurt skin). No other skin changes. [read]

## 11. jail5 flyer fix

- When a flyer spawns, if the map's name equals "jail5" (case does not matter) and the
  flyer's origin height is exactly -104, its target is moved into its target name: it now is
  triggered by whatever named that target, and it no longer fires a target itself. [read]

## 12. The "count" key

- Ammo pickups: if the map gives a non-zero count, that is the amount given; zero or absent
  gives the item's standard quantity. In deathmatch with infinite ammo, an ammo item that is
  also a weapon (hand grenades) gives 1000. The total is capped at the player's maximum for
  that ammo, and the pickup is refused when already at the maximum. [read]
- Weapons: the count key is ignored; a placed weapon gives the standard quantity of its ammo
  (1000 with infinite ammo); a weapon dropped by a monster or player gives no ammo with
  it. [read]
- Health: each health item's spawn sets its own amount (stimpack 2, medium 10, large 25,
  mega 100) after the map's keys are read, so a map's count is overwritten and has no
  effect. [read; ordering derived from spawn order: keys are parsed before the item's spawn
  routine runs]
- Armour and power armour and powerups: count is not consulted. [read]

## 13. Player weapon detail sounds

Weapon-model frames are the view weapon's gun frames. A player's weapon loop sound is the
player's entity loop (static attenuation). [read]

- Hand grenade: when a throw starts (attack pressed with grenades in stock), at gun frame 5
  weapons/hgrena1b.wav (the pin), weapon channel, normal. At frame 11 the 3.2 s fuse starts
  and the loop weapons/hgrenc1b.wav (ticking) plays while the grenade is held; it is cleared
  when the grenade is thrown at frame 12 or blows up in the hand. Ready-idle frames 16-48
  pause at 29, 34, 39, 48 (fidget held 15 of 16 times).
- Chaingun: fire frames 5-21. At gun frame 5 weapons/chngnu1a.wav (spin up), auto channel,
  IDLE attenuation. While firing, the loop weapons/chngnl1a.wav is set on every fire frame.
  Holding fire at frame 21 jumps back to 15 (if ammo remains). If the trigger is released by
  frame 14, it skips to frame 32 with the loop cleared and NO spin-down sound. Otherwise when
  frame 22 is reached the loop is cleared and weapons/chngnd1a.wav (spin down) plays, auto
  channel, idle.
- Hyperblaster: fire frames 6-11; the loop weapons/hyprbl1a.wav is set on every fire frame;
  holding fire with ammo jumps from 12 back to 6. When frame 12 is reached the loop is
  cleared and weapons/hyprbd1a.wav plays (wind-down), auto channel, normal.
- Railgun: while the railgun is the current weapon, the player's loop is weapons/rg_hum.wav
  at all times (overridden only by the lava/slime sizzle).
- BFG: while it is the current weapon the loop is weapons/bfg_hum.wav (same override rule).
  Fire frames 9 and 17: at 9 the muzzle flash is sent, which plays weapons/bfg__f1y.wav (the
  charge) on the client, weapon channel, normal; the ball leaves at 17 if 50 cells remain.
- Out of ammo: weapons/noammo.wav, voice, normal, at most once per second.

## 14. Fliers sliding sideways

- Kinds: everything flagged as flying: flyer, hover, floater, and boss2 (also the crucified
  insane marine, which never attacks). Not the flipper (a swimmer), not Jorg or Makron (they
  have the same rule in their own attack check, but they walk so it never applies). [read]
- When decided: on each running think in which the enemy is visible, there is a clear line,
  the enemy is not in melee range and not far, the attack cooldown has expired, and the
  random missile-attack roll fails. Then: 30% chance it enters the sliding state, 70%
  straight. So it is re-rolled every such frame (10 per second), not once per run. If any of
  the earlier conditions fails, the previous state is kept, so a slide can continue even
  while the enemy is out of sight. [read; the persistence is derived]
- While sliding: it turns toward the enemy and moves the run frame's distance sideways (90
  degrees to its enemy direction), to one side; if that move is blocked it flips to the other
  side for good (until next blocked) and tries that way. It does not close distance while
  sliding. The side starts at the monster's initial "left" choice (zero = right side, i.e.
  minus 90 degrees). [read]
- An attack choice (missile or melee) ends it (state returns to straight after attacking).
  [read]

## 15. Fliers and knockback

- Same rule for every monster: knockback velocity = 500 times the knockback amount divided
  by the mass (mass taken as at least 50), added in the hit direction. Fliers are not
  exempt: masses flyer 50, hover 150, floater 300, boss2 1000. [read]
- What differs is how it decays: fliers have no gravity; their horizontal speed is damped by
  ground-style friction (6, stop speed 100) every frame even in the air, and any vertical
  speed by a third of that (2). Walkers get horizontal friction only while on the ground.
  [read]
- Exceptions: the crucified insane marine has the no-knockback flag; any monster that dies
  gets it at the killing blow (corpses are not pushed by that blow). No combat monster is
  exempt in life. [read]

## 16. Idle fidgets per kind

"Timer" means the shared standing idle timer from section 0 (first after 0-15 s, then every
15-30 s; not for ambush monsters). [read throughout]

- Soldier: stand run 1 is 146-175; on its first frame 146, a 20% chance of
  soldier/solidle1.wav (voice, idle). Whenever a stand run ends it picks again: after run 3
  always run 1, otherwise 80% run 1 and 20% run 3 (176-214). Run 3 plays the gun cock
  infantry/infatck3.wav at 197 (weapon channel, idle attenuation at that frame; normal
  elsewhere). No timer idle.
- Infantry: timer idle starts the fidget run 1-49 and plays infantry/infidle1.wav at its
  start (voice, idle). Stand is 50-71.
- Gunner: stand 0-29; at frames 9, 19 and 29 a 5% chance (not when holding ground) to start
  the fidget 30-69, which plays gunner/gunidle1.wav at frame 37 (voice, idle). No timer idle.
- Berserker: stand 0-4; at frame 0, a 15% chance (not when holding ground) to start the
  fidget 5-24, with berserk/beridle1.wav at its start (WEAPON channel, idle). No timer idle.
- Parasite: stand 83-99 taps parasite/paridle1.wav at 85, 87, 91, 93, 97, 99 (weapon
  channel, idle) on every loop. Timer idle starts 100-103, then the scratch loop 104-109
  (parasite/paridle2.wav at 104 and 107, weapon, idle), which repeats 80% of the time and
  otherwise ends with 110-117 (scratch sound at 110), then back to stand.
- Tank: timer idle plays tank/tnkidle1.wav (voice, idle) only, no animation. Stand 0-29.
- Medic: stand 90-frame loop 12-101; on its first frame 12 it plays medic/idle.wav (voice,
  idle) and looks for a dead monster to heal; the timer idle does the same thing.
- Brain: timer idle plays brain/brnlens1.wav (AUTO channel, idle) and the idle run 192-221,
  then returns to stand 162-191.
- Chick: stand 121-150; on its last frame 150, a 30% chance (not when holding ground) to
  start the fidget 151-180, which plays at frame 159 chick/chkidle1.wav or chkidle2.wav
  (50/50; voice, idle). No timer idle.
- Gladiator: timer idle plays gladiator/gldidle1.wav (voice, idle), no animation. Stand 0-6.
- Mutant: timer idle starts the idle run 113-125 with mutant/mutidle1.wav at its start
  (voice, idle); at frame 119 there is a 75% chance to jump back to 116, so 116-119 loops a
  random number of times before 120-125 and the return to stand 62-112.
- Floater: timer idle plays floater/fltidle1.wav (voice, idle), no animation. Its stand is
  either 144-195 or 196-247 (50/50 each time it is told to stand; each loops). Always
  carries the loop floater/fltsrch1.wav.
- Hover: no idle, no fidget; stand 0-29; always carries the loop hover/hovidle1.wav. Search
  on the walking timer: 50/50 hover/hovsrch1.wav or hovsrch2.wav (voice, normal).
- Flyer (for reference): timer idle plays flyer/flysrch1.wav (voice, idle); loop
  flyer/flyidle1.wav always.
- Flipper: no idle, no fidget; stand is the single frame 41. flipper/flpidle1.wav and
  flpsrch1.wav are precached but never played.
- Supertank: no idle and no fidget; stand 194-253.
- Insane marine: no timer idle; its stand is 50/50 the normal stand 59-64 or the shaking
  stand 64-93 (insane/insane5.wav at 64, voice, idle). At the end of either (frames 64 and
  93), unless spawned "always stand", a 30% chance to go down: half the time the slow sink
  0-39 (insane/insane7.wav moan at 7, insane/insane11.wav fist at 27 and 33), half the quick
  drop 95-99. Lying loop 99-159: fist at 115, moan at 132, a scream at 152, and at 159 a 50%
  chance to get up via 40-58. Screams are one of insane1, 2, 3, 4, 6, 8, 9, 10 .wav at
  random. Walk and run (160-172 or 173-198, 50/50) and crawl 227-235 scream on their first
  frame. Crucified: 252-266 with a moan at 252, 80%; 20% struggle 267-281 with a scream at
  267. All voice, idle.

## Could not answer / notes

- No fork-only behaviour: the source read is the stock release.
- Jorg's own machine-gun bullet sounds and all monster gun sounds come from the client's
  muzzle-flash table, not from the game logic read here, so their file names are not listed.
