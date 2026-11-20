# Quake II monsters: round 3 facts (the trail, hearing, light, idle and search sounds, soldier moves, misc_insane)

Clean-room reader's answers, kept for the single player 1-1 work (q2-sp-gaps.md). No code.


Facts in plain words from the stock game library (and, for Q3, the stock client and renderers).
[read] = stated directly by the source; [derived] = follows from reading several places together.
Frames: "frame N" is the 0-based model frame index; "run starts at F" names the first frame of
the animation run it belongs to; "step k" is the 0-based position inside that run.
Attenuations: none = 0, normal = 1, idle = 2, static = 3 [read].

## 1. The player trail

- 8 markers in a ring, made at map load in single player and coop (not deathmatch). They start
  at the world origin with time stamp 0. A write index points at the oldest slot [read].
- Dropped once per server frame at the start of the player's frame processing, only while the
  player is alive (the dead/respawn branch returns first) and not in deathmatch: if the line from
  the player's eye to the most recent marker is blocked (opaque trace; the marker has eye height
  0), a marker is written at the player's position from the previous frame. No distance or time
  condition; only "cannot see the last marker" [read].
- Each marker stores its time and a yaw: the direction from the previous marker to this one [read].
- The monster keeps a "trail time": set to now when it finds its target and every frame its enemy
  is visible in the run state [read].
- Lost sight (run state, enemy not visible): on the first such frame it marks itself "lost sight"
  and "pursuing the last sighting" and heads for the last-sighting point (the enemy's position the
  last frame it was visible) [read]. A temporary goal entity is placed at that point for the move
  and freed after it [read].
- When the remaining distance to the current goal point is no more than this frame's move
  distance, the move is cut to exactly that distance and a "reached" flag is set [read]. On the
  next lost-sight frame: the search time becomes now + 5 [read]; then
  - if the goal was a temporary side step (below), the saved real goal is restored [read];
  - else if it was the last-sighting point: pick-first. Walk the ring from the oldest slot forward
    (at most 8 steps), stopping at the first marker newer than the monster's trail time. If the
    monster can see that marker it is chosen; else if it can see the marker just before it
    (one older) that one is chosen; else the newer one anyway [read]. If every marker is older,
    the walk wraps back to the oldest slot [derived].
  - else pick-next: the same walk, the first marker newer than the trail time, no sight test [read].
  The chosen marker becomes the new goal point, the trail time becomes that marker's time, and the
  monster's facing AND ideal yaw are snapped instantly to the marker's stored yaw (the player's
  heading there), not toward the marker [read].
- Course correction, on every new goal: a box trace (the monster's box, player-solid mask) to the
  goal. If it is blocked at fraction f: face the goal; probe two box traces to points at distance
  d1*(f+1)/2 forward and 16 units left / right; the scaled centre value is d1*f / that distance.
  The side whose fraction is >= that centre and greater than the other side wins (left checked
  first); if it is short of 1, the side point is pulled in to half of (distance * side fraction).
  The real goal is saved, the side point becomes a temporary goal, and the facing snaps to it [read].
- Search timeout: if the search time is set and now > search time + 20 (that is, 25 s after the
  last sighting or the last reached point), for one frame it moves toward its goal entity (the
  enemy himself, wherever he is) and clears the search time; next frame the trail logic resumes,
  with no timeout any more until another point is reached [derived].
- It never gives up the enemy: the enemy is only dropped when dead (health <= 0, or <= -80 for
  "brutal" monsters), freed, or (medic) healed [read]. In coop, while out of sight it re-runs the
  target search every frame and can switch to another visible player [read].

## 2. A heard noise

- Noise markers: each player owns two invisible 16-unit cube markers made on first use. The
  "personal" one is moved for jumps, every weapon shot, entering or leaving water, gasping for air,
  and breather/suit breaths; the "impact" one for bullet, shell, rail and explosion impacts and
  blaster bolts striking. Each move records the time and the frame number. Silenced shots use up
  one silencer count and make no noise. No noise in deathmatch or under notarget [read].
- Hearing (monster searching for a target), per frame, in this order: a monster that just found a
  player (within the last frame, monster not ambush) takes priority; else a personal noise from
  this or the last frame; else an impact noise from this or the last frame, only if the monster
  has no enemy and is not ambush; else the player itself by sight [read].
- A noise is heard if: ambush monsters must SEE the marker; others need it in their hearable set
  (the PHS); the distance must be <= 1000; the two areas must be portal-connected. No light test
  and no facing test [read].
- Then: one yaw step toward the noise, the "sound target" mode on, and the enemy set to the noise
  marker itself (not the player) [read]. Found-target follows: last sighting = noise position,
  trail time = now, goal = the marker, the run animation (or stand, if it holds ground), ideal yaw
  at it, first attack delayed 1 s. No sight sound for a noise [read].
- Moving to it: in the run state at the run animation's distances, by the ordinary goal-seeking
  move [read]. Each frame it re-checks for a target; if none is found it returns before any attack
  logic [read].
- Within 64 units of the marker it holds ground (a temporary hold) and switches to stand [read].
  Standing with an enemy, it turns to face the marker; if its yaw is not exactly the ideal it drops
  the temporary hold and runs again [read]. While standing this way it does not look for the
  player; it only runs the attack check [derived].
- The 5 s forgetting lives in the attack check: if more than 5 s have passed since that marker's
  noise time, the goal goes back to its path corner (or none), sound-target mode and the temporary
  hold are cleared [read]. The marker has 0 health, so the same check then treats the enemy as dead:
  the enemy is cleared; if it had an old enemy still alive it hunts that; else if it has a path it
  walks it; else it stands with its pause time pushed effectively forever [derived]. Within 5 s the
  check marks it hostile-looking (wakes others) and attacks nothing [read].
- Because the run state returns early when no target is found, the 5 s check is only reached while
  standing at the noise or on a frame a target is found; a monster that cannot get within 64 keeps
  going after the marker indefinitely [derived]. A new noise moves the same marker, so it then
  heads for the new spot and its time restarts [derived].
- The player never becomes the enemy through hearing [read]. When it then sees him (normal sight
  rules): enemy = the player, sound-target mode cleared, found-target runs (it becomes the
  "sight entity" others can react to, its own light value forced to 128, last sighting and trail
  time updated, goal = player, run, attack delayed 1 s), and the sight sound plays [read].

## 3. The player's light level

- Every client move command carries a light byte; the server copies it into the player each time
  it processes a command [read].
- The client fills it from a renderer value: the GL renderer samples light at the view origin
  (lightmaps plus dynamic lights, times the gl_modulate cvar) and takes the largest colour channel
  times 150; the software renderer stores 150 times its light sample. It is held in the r_lightlevel
  cvar and cast to a byte when the command is built [read].
- Sight test: a player (or a monster offered as "sight entity") whose light value is <= 5 cannot be
  spotted [read]. A monster that found a player gets 128, so it is always visible to others [read].
- No "fired recently" exception in the game logic; firing works through hearing (each shot moves
  the personal noise) and, client side, the muzzle flash's dynamic light raises the sampled
  value [derived]. The darkness test does not apply to hearing [read]. It is also not applied once
  a monster has an enemy (visibility then is a plain line-of-sight trace) [read].

## 4. Idle and search sounds

Timers [read]: "idle" plays from the stand state only when the monster is not ambush (spawnflag 1)
and has an idle action: the first time it only arms a timer at now + random*15 s; after that, when
the timer passes, it plays and re-arms at now + 15 + random*15 s. "Search" plays from the WALK
state (walking a path), never while hunting, on the same timer and the same rules (no ambush
condition). Both share one timer. Sound calls below are full volume.

- Soldiers (all three): no idle/search actions. Stand run A (frames 146-175) step 0 (frame 146):
  20% chance of soldier/solidle1.wav, voice channel, idle attenuation. Stand run B (frames
  176-214) step 21 (frame 197): infantry/infatck3.wav (gun cock), weapon channel, idle attenuation.
  On each stand loop end: after run B always run A; otherwise 80% A, 20% B [read].
- Infantry: idle action = the fidget run (frames 1-49, then back to stand frames 50-71) with
  infantry/infidle1.wav, voice, idle atten. No search (infsrch1 is precached, unused) [read].
- Gunner: no idle action. Stand run (frames 0-29) steps 9, 19, 29: 5% chance each (not when
  holding ground) to start the fidget run (frames 30-69); its step 7 (frame 37) plays
  gunner/gunidle1.wav, voice, idle atten. Search: gunner/gunsrch1.wav, voice, normal atten [read].
- Berserker: stand run is frames 0-4; step 0 each loop: 15% (not holding ground) -> fidget run
  frames 5-24 and berserk/beridle1.wav on the WEAPON channel, idle atten. Search:
  berserk/bersrch1.wav, voice, normal [read].
- Parasite: idle action = fidget: start run frames 100-103, loop run frames 104-109 (repeats with
  80% each pass, else end run 110-117). parasite/paridle2.wav (scratch) at frames 104, 107 and 110.
  The plain stand run (frames 83-99) plays parasite/paridle1.wav (tap) at steps 2, 4, 8, 10, 14, 16
  (frames 85, 87, 91, 93, 97, 99) every loop. Both on weapon channel, idle atten. No looping sound.
  A search function exists (parsrch1) but is never assigned, so never plays [read].
- Flyer: idle action plays flyer/flysrch1.wav, voice, idle atten. Looping sound flyer/flyidle1.wav,
  set at spawn, never cleared (the body explodes on death) [read]. No search.
- Floater: idle floater/fltidle1.wav, voice, idle atten. Looping floater/fltsrch1.wav. No search.
  Stand picks run A (144-195) or B (196-247) 50/50 [read].
- Hover: no idle. Search: 50/50 hover/hovsrch1.wav or hovsrch2.wav, voice, normal. Looping
  hover/hovidle1.wav, kept on the corpse until gibbed (gibbing clears it) [read]/[derived].
- Tank and tank commander (same code): idle tank/tnkidle1.wav, voice, idle atten. No search,
  no loop [read].
- Medic: idle action plays medic/idle.wav (voice, idle atten) AND looks for a dead monster to heal;
  it is also called at step 0 (frame 12) of every 90-frame stand loop (frames 12-101). Search:
  medic/medsrch1.wav, voice, IDLE atten, and if it has no old enemy it looks for a corpse [read].
- Brain: idle brain/brnlens1.wav on the auto channel, idle atten, and the idle run frames 192-221
  (then back to stand 162-191). Search brain/brnsrch1.wav, voice, normal. brnidle1/brnidle2 are
  precached but unused. No loop [read].
- Iron maiden (chick): no idle action. Stand run (121-150) last step (frame 150): 30% (not holding
  ground) -> fidget run 151-180; its step 8 (frame 159) plays chick/chkidle1.wav or chkidle2.wav
  50/50, voice, idle atten. No search (chksrch1 unused) [read].
- Gladiator: idle gladiator/gldidle1.wav, voice, idle atten (no animation change). Search
  gladiator/gldsrch1.wav, voice, normal [read].
- Mutant: idle -> idle run frames 113-125 and mutant/mutidle1.wav, voice, idle atten; at step 6
  (frame 119) 75% chance to jump back to frame 116 (a loop). Search mutant/mutsrch1.wav, voice,
  normal [read].
- Barracuda shark (flipper): none; flpidle1/flpsrch1 precached but never used [read].
- Supertank: no idle. Search 50/50 bosstank/btkunqv1.wav / btkunqv2.wav, voice, normal. No loop
  (commented out); instead bosstank/btkengn1.wav on the voice channel, normal atten, at step 0 of
  each forward/run (frame 128), left turn (146), right turn (176) and backward (80) run [read].
- Boss2 (hornet): no idle. Search: 50% chance bosshovr/bhvunqv1.wav, voice, attenuation NONE (heard
  level-wide), else silence. Looping bosshovr/bhvengn1.wav from spawn, never cleared, even on
  death [read].
- Jorg: no idle action; boss3/bs3idle1.wav, voice, NORMAL atten, at step 0 (frame 112) of every
  51-frame stand loop (112-162). Search 30% bs3srch1 / 30% bs3srch2 / 40% bs3srch3.wav, voice,
  normal. Looping boss3/w_loop.wav set when it starts its chain-gun attack (75% of its attack
  choices), cleared when that attack ends, on pain and on death [read].
- Makron: no idle/search. Taunts (voice4/voice3/voice.wav 30/30/40, auto channel, atten NONE) only
  at step 23 (frame 410) of its long pain run (starts at 387). Its thrown torso loops
  makron/spine.wav [read].
- Insane: see Q6.

## 5. Soldier special moves

- Run-and-shoot (run starts at frame 109, 14 frames 109-122): chosen ONLY in the sight reaction
  (first sighting by vision), after the sight sound: skill >= 1, distance >= 500 (mid or far
  range), then 50% [read]. Every step turns to face the enemy and advances; the shot is step 3
  (frame 112, which advances 11 units), aimed at the enemy's eye with the usual spread (up to 1000
  sideways, 500 up/down at 8192 range). Step 13 (frame 122, advance 17): if the enemy is alive, still
  at >= 500 and skill is 3, loop to frame 111 (step 2); else the run ends and the soldier runs [read].
- Attack 1 (frames 0-11) and attack 2 (frames 12-29) are for blaster and shotgun soldiers, 50/50
  per attack; the machine-gun soldier always uses attack 4 (frames 39-44, shot at frame 41, which
  holds the frame for 3-10 frames of fire) [read].
  - Attack 1: shot at step 2 (frame 2); step 5 (frame 5): blaster only: if enemy alive and (skill 3
    with 50%, or enemy within 80 units) go back to frame 1, else skip to frame 9; step 7 (frame 7)
    cock sound (normal atten); step 8 (frame 8): shotgun/machine-gun only, same test, back to
    frame 1 [read].
  - Attack 2: shot at step 4 (frame 16); step 7 (frame 19): blaster: same test, back to frame 15,
    else skip to frame 27; step 12 (frame 24) cock; step 14 (frame 26): shotgun: same test, back to
    frame 15 [read].
  - Attack 3 (frames 30-38, shot at 32) is only reached by dodging: a dodge happens 25% of the
    time; skill 0 always plain duck, skill 1 duck 67% / attack 3 33%, skill >= 2 duck 34% /
    attack 3 66% [read].
- Head shot: if the vertical gap between the damage point and the soldier's eye (origin height +
  25) is at most 4 units, death run 3 (frames 343-387, 45 frames) [read].
- Otherwise a uniform pick of five: run 1 (272-307), run 2 (308-342), run 4 (388-440), run 5
  (441-464), run 6 (465-474) [read]. Gib at health <= -30 first (3 small meat, chest, head) [read].
- Dying shots: only death run 1: step 21 (frame 293) and step 24 (frame 296) each fire one shot
  straight along the body's facing (no aim at the enemy, no spread added), from muzzle offsets 6 and
  7 of the soldier's weapon type [read]. For the machine-gun soldier each such shot may hold the
  frame for 3-10 frames of fire [derived].

## 6. misc_insane

- Health 100, gib at <= -50, mass 300, box -16,-16,-24 to 16,16,32 (crucified: -16,0,0 to 16,8,32,
  flying, no knockback). Skin random 0-2 (not when crucified). Not counted as a monster kill,
  marked as a friendly ("good guy"): never looks for targets and never gets angry at players or
  other friendlies; no attack, melee, dodge or sight action, so it never attacks [read].
- Spawnflags: 1 ambush, 2 trigger spawn, 4 crawl, 8 crucified, 16 stand ground, 32 always stand [read].
- Sounds (all voice channel, IDLE attenuation): fist insane/insane11.wav, shake insane5.wav, moan
  insane7.wav, screams picked uniformly from insane1,2,3,4,6,8,9,10.wav [read].
- Standing: runs A (frames 59-64) or B (frames 64-93) 50/50; B plays shake at its step 0 (frame 64).
  At the end of either: unless always-stand, 30% chance to go down: half "up to down" (frames 0-39:
  moan at frame 7, fist at frames 27 and 33) and half "jump down" (95-99), both ending in the
  "down" loop (frames 99-159: fist at 115, moan at 132, scream at 152 with a tiny move back,
  and at frame 159 a 50% chance to get up via frames 40-58, unless crawl+stand-ground) [read].
- Walking/running (path corners; or running if a hostile monster hurt it): plain walk (160-172) or "insane" walk (173-198)
  50/50, a scream at the first frame of each loop; with crawl set, the crawl run (227-235) with a
  scream each loop [read].
- Crawl + stand ground: lies in the down loop for good. Stand ground alone: holds position (and
  from crawl pain's last frame, 244, goes to the down loop) [read].
- Crucified: hangs in the cross run (252-266, moan at 252), at its end 80% again / 20% the
  struggle run (267-281, scream at 267); holds ground [read].
- Pain (3 s debounce): player/male/pain<L>_<1|2>.wav with L = 25, 50, 75 or 100 by health under
  that number (100 otherwise), idle atten. No pain animation on skill 3. Crucified -> struggle run;
  if crawling or in the down frames (98-159) the crawl pain run (236-244), else the stand pain run
  (199-209) [read].
- Death: player/male/death1-4.wav at random, idle atten. Crucified: stays hanging (flying, no fall)
  on its current frame; crawling/down -> crawl death 245-251; else stand death 210-226; the corpse
  box becomes -16,-16,-24 to 16,16,-8 and tosses [read].
- Gibs at <= -50: misc/udeath.wav (idle atten), 2 bones, 4 small meat, head2 head [read].
