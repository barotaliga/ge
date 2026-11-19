# Quake II game switches: aim from the muzzle, the machine gun climb, footsteps, single-player fixes, entity slots

WORKING AREA: deleted when porting ends. A clean-room answer document, in a reader's words, with no code. Read from Yamagi Quake II, release 8.71pre, its baseq2 game library. Marks: read, derived; nothing measured.

Read: Yamagi Quake II, release 8.71pre (the default branch as of November 2026), its baseq2 game library, plus the parts of its client and server that decide what the game's settings do.

Marks: **[read]** stated directly by the source; **[derived]** worked out from facts read, not run; **[measured]** none in this document (nothing was run).

Conventions used below: "units" are map units; angles in degrees; one server frame is 0.1 s (10 Hz). A muzzle offset is written as (forward, right, up): forward along the view direction, right along the view's right vector, up added straight onto world height, all measured from the player's origin. "Viewheight" is the player's eye height above the origin: 22 standing, -2 crouched, 8 when dead [read].

One correction to the notes' wording: the notes say the aim point is "found by a line of up to 8192 units from the eye". That is right, but when the line hits nothing the shot keeps its old direction (see 1); and the line runs along the shot's own direction, not always along the crosshair (see 4 and 5).

---

## 1. aimfix: the target point

- The line starts at the player's origin raised by the current viewheight, nothing else: no view bob, no kick offset, no gun offset, no client-side view smoothing [read]. So standing it starts 22 units above the origin, crouched 2 units below it [read].
- It runs 8192 units along the direction the weapon is about to fire in. For most weapons that is the view direction; for the machine gun it is the view plus that shot's kick angles; for the two super shotgun halves it is the view turned 5 degrees left or right (see 4) [read].
- The line uses the game's "shot" collision: world brushes that are solid, brushes marked as window (translucent glass), brush entities (doors, platforms, breakable glass and other movers), and every box entity: players, monsters, corpses, and any other box-shaped entity such as another player's rocket, grenade or blaster bolt [read; the box-entity list is derived from the rule that every box entity counts as a monster for collision].
- It skips the shooter, the shooter's own projectiles, and the shooter's owner [read]. It passes through water, slime and lava (those are not in the shot mask) and through triggers and pickup items (not solid) [read/derived].
- Sky brushes are solid in the map, so a line that reaches the sky stops there and the shot converges on that point [derived].
- If the line hits nothing within 8192 units, no aim point is used at all: the shot keeps its original direction, parallel to the eye line, starting from the muzzle as in the classic game [read].
- If the line starts inside something solid (eye in a wall) it stops at its own start, so the shot is aimed from the muzzle back toward the eye [derived].

## 2. aimfix: the start point

- The start point is exactly the classic one: the same per-weapon offset from the player's origin, turned by the same handedness rule (left-handed mirrors the right offset; centre-handed sets the right offset to 0) [read]. aimfix changes only the direction, then makes it the unit vector from that start point to the aim point [read].
- Per weapon (forward, right, up) [read]:
  - blaster: (24, 8, viewheight - 8)
  - hyperblaster: the blaster's (24, 8, viewheight - 8) plus a turning ring: forward -4·sin(r), up +4·cos(r), with r = (gun frame - 5) × 60 degrees
  - shotgun, super shotgun, machine gun: (0, 8, viewheight - 8)
  - chaingun: (0, 7 + 4·c1, viewheight - 8 + 4·c2) for each bullet, c1 and c2 fresh random numbers in -1..1
  - railgun: (0, 7, viewheight - 8)
  - grenade launcher, rocket launcher, hand grenade, BFG: (8, 8, viewheight - 8)
- Start inside a wall: aimfix adds nothing; the weapon's classic handling applies. Bullets and pellets first check the path from the player's origin to the start point and fire nothing when it is blocked; blaster and hyperblaster bolts check the same path and, when blocked, are pulled back 10 units along the shot direction and burst at once; rockets, grenades, the rail and the BFG start where the offset puts them [read].
- Aim point closer than the muzzle (a wall right in front, nearer than the muzzle's forward offset): there is no guard. The direction is taken from the muzzle to the hit point whatever it is, so it can point sideways or back toward the shooter; if the two points coincide the direction becomes zero and the projectile does not move [derived]. This bites most on the blaster and hyperblaster (24 units forward) and the 8-unit weapons.

## 3. aimfix: which weapons

aimfix sits in the one routine every player weapon uses to find its muzzle, so it changes every player weapon's direction and none of their start points (the super shotgun is the one exception, see 4) [read].

| Weapon | Changed? | How |
| --- | --- | --- |
| blaster | yes | direction only |
| shotgun | yes | direction only; the 12 pellets spread around the new direction |
| super shotgun | yes | both halves aimed separately, and each half's start recomputed (see 4) |
| machine gun | yes | direction only; the aim line runs along view + kick (see 5) |
| chaingun | yes | each bullet (1 to 3 per frame) has its own random start and its own aim line; all lines run along the plain view direction, so the bullets converge on the crosshair point [read/derived] |
| hyperblaster | yes | direction only; the turning ring start is unchanged, but each bolt now flies from its point on the ring to the aim point, so the bolts converge instead of flying parallel [derived] |
| grenade launcher | yes | the launch direction only; the grenade still falls under gravity, so it does not land on the aim point [derived] |
| rocket launcher | yes | direction only |
| hand grenades | yes | the throw direction only, including grenades let go on a weapon change, on death, or when held too long [read] |
| railgun | yes | direction only; the rail passes through the aim point |
| BFG | yes | direction only, on the firing frame; the earlier wind-up frame also runs the aim line but only uses the start for the flash and noise [read] |

- Monsters: unchanged. Their weapons use their own muzzle routine, which aimfix does not touch [read].
- The backward view nudge each shot gives (the kick origin, -2 or -3 units along the shot) follows the aimed direction for the blaster, hyperblaster and grenade launcher (the nudge is set after aiming) and the plain view for the others; a cosmetic difference of a few degrees at most [read/derived].

## 4. aimfix and the super shotgun

Classic (aimfix 0) [read]:
- One start point, (0, 8, viewheight - 8) on the plain view axes.
- Two halves of 10 pellets each (20 in all), damage 6 and knockback 12 per pellet, spread 1000 across and 500 up and down (in units at 8192). The first half flies at view yaw - 5 degrees (to the right), the second at view yaw + 5 degrees (to the left), pitch and roll as the view.
- The kick: pitch -2 degrees, origin -2 units along the view direction.

With aimfix 1, for each half in turn [read]:
- Its direction starts as the view turned 5 degrees (same signs as above).
- The view nudge is set again: pitch -2 (unchanged), origin -2 units along that turned direction. The second half's setting is the one that lasts the frame, so the origin nudge points 5 degrees off to the left; the pitch kick is the same as classic [derived].
- Its start point is recomputed with the same (0, 8, viewheight - 8) offset but on the turned axes, so the two halves start a little apart: each moves about 0.7 units forward or back (8 × sin 5°) and 0.03 units inward compared with the classic point [derived].
- Its aim line runs from the eye along the turned direction (not along the crosshair), and the half is aimed from its start point to where that line hits. So the halves still part 5 degrees either side of the eye; each converges on its own point 5 degrees off the crosshair [derived].
- The muzzle flash and the noise monsters hear come from the second half's start [read].
- Spread, pellet count, damage and ammo (2 shells) are as classic [read].

## 5. aimfix, the kick angles and the spread

- Spread still applies, built around the new direction: each bullet or pellet picks a random point within the weapon's spread at 8192 units along the aimed direction, so the cone is centred on the aim point [read]. Spreads: machine gun and chaingun 300 across, 500 up and down; shotgun 500 by 500; super shotgun 1000 by 500 (units at 8192) [read].
- The kick angles still bend the machine gun's aim, and more than before: the machine gun's direction is view + kick, and the aim line itself runs along view + kick. So the converged point is where the kicked line hits, and the climb and the random yaw and roll jitter all move it [read/derived].
- No other player weapon puts its kick angles into its aim; their kicks only move the view [read].

## 6. Machine gun climb

- The counter goes back to 0 when [read]:
  - the machine gun's fire frame runs with the attack button not held (the gun checks the button on each fire frame, every server frame while in its firing cycle).
  - any weapon change completes, including the automatic switch when ammo runs out and the forced put-away when the player dies.
  - the player respawns (the per-life client state is cleared) [derived].
- Out of ammo itself does not reset it: the empty click ("weapons/noammo.wav", at most once a second) starts a switch to another weapon, and that switch resets it [read].
- Order on each bullet: the pitch kick is set to -1.5 × the counter first, then the counter rises (outside deathmatch, without the switch), capped at 9 [read]. So the first bullet of a burst has no pitch kick, the second -1.5, the tenth and later -13.5 degrees [derived].
- The counter drives nothing else: no sound, no animation [read].
- The climb is not lasting: kick angles are shown on the view for the frame they are set and cleared at the end of every server frame. While the gun fires (every frame) the view stays pitched up by the current kick; on release it drops back at once. The player's own view angle is never changed [read/derived]. The client shows view kicks only when its "cl_kickangles" is 1 (default 1) [read].
- g_machinegun_norecoil 1 stops only the counter from rising, so only the pitch climb goes. The random part stays: each bullet still gets yaw and roll kick of ±0.7 degrees and an origin shake of ±0.35 units on all three axes, and the yaw and roll jitter still bends the aim [read].
- The setting (and deathmatch) is read on every bullet, so it takes effect at once. If it is turned on mid-burst the counter stops where it is and the pitch kick stays at that value until the next reset [derived].
- In a game with more than one player slot, a connected client forces its own copy of g_machinegun_norecoil back to 0 every frame (and g_footsteps to 1, g_swap_speed to 1, cl_kickangles to 1); in single player it does not [read]. On a listen server the client and server share their settings, so there this holds the server at 0 [derived]; a dedicated server is not affected [derived].

## 7. g_monsterfootsteps

- Default 0, archived, read each time a step frame plays (takes effect at once) [read]. At 0 none of the monsters below make these steps [read]. Any non-zero value turns them on [read].
- Not covered by the switch, and stepping always (as in the classic game): the tank and tank commander ("tank/step.wav"), the mutant ("mutant/step1.wav" to "step3.wav"), and the two final bosses ("boss3/step1.wav", "boss3/step2.wav"; "makron/step1.wav", "makron/step2.wav") [read].
- The kinds, their sounds, and the animations that step. Frames are counted from 0 within each animation, at 10 frames per second [read]:

| Monster | Sounds (one picked at random each step) | Animations with steps (frame numbers) |
| --- | --- | --- |
| infantry (enforcer) | infantry/step1.wav, infantry/step2.wav | walk (12 frames: 0, 6), run (8: 1, 5), fidget (49: 5, 47), both pains (10: 4, 9), death 1 (20: 4, 8), death 2 (25: 5, 6), duck (5: 4), a firing attack (8: 3) |
| soldier (light, normal, SS) | player/step1.wav to player/step4.wav | walk 1 (33: 3, 8), walk 2 (10: 0, 4), run (6: 1, 4), pain 3 (18: 2, 17), attack 4 (6: 1, 5), attack 6, a running attack (14: 2, 6, 9, 12) |
| gladiator | gladiator/step1.wav, gladiator/step2.wav | walk (16: 4, 12), run (6: 2, 5) |
| iron maiden | bitch/step1.wav, bitch/step2.wav | walk (10: 1, 6), run (10: 1, 6), death 2 (23: 3, 10, 16), rocket attack start (13: 8), rocket attack (14: 2, 4, 9, 12), rocket attack end (5: 4), slash start (3: 2), slash end (4: 3) |
| berserker | berserk/step1.wav, berserk/step2.wav | walk (12: 3, 8), run (6: 1, 4), club attack (12: 2), strike attack (14: 2, 8, 13) |
| medic | medic/step1.wav, medic/step2.wav | walk (12: 1, 7), run (6: 0, 4), pain 2 (15: 3, 14) |
| gunner | gunner/step1.wav, gunner/step2.wav | walk (13: 0, 5, 12), run (8: 1, 5), run-and-shoot (6: 1, 4), pain 1 (18: 2, 12, 16), pain 2 (8: 2, 7), chaingun attack start (7: 1), chaingun end (7: 6) |
| brain | brain/step1.wav, brain/step2.wav | walk (11: 3, 9), run (11: 3, 9), pain 1 (21: 2, 19), duck (8: 3, 7), attack 1 (18: 3, 17) |
| parasite | parasite/paridle1.wav (its tapping sound; only one) | run (7: 2, 3, 5), stop running (6: 2), walk (7: 2, 4, 5), stop walking (6: 2) |
| insane marine | player/step1.wav to player/step4.wav | normal walk (13: 6, 12), crazed walk (26: 6, 11, 18, 24), standing pain (11: 4) |

- Rate: two steps per walk cycle for most; for example the infantry walk steps every 0.6 s, the soldier run every 0.3 s [derived from the frame numbers at 10 Hz].
- Volume 1 and normal attenuation on the monster's body channel for every kind, except the insane marine at volume 0.7 and the parasite at idle (shorter-range) attenuation [read].
- No speed or ground check: a step sounds whenever the animation reaches a step frame, even mid-air or in a pain or death animation [read].
- Each sound is registered with the server the first time that kind steps (the soldier's at spawn too), so loading an older save works [read].

## 8. g_footsteps

- 225 is compared with the horizontal speed itself (square root of the summed squares of the two horizontal velocity parts), in units per second, strictly greater [read].
- Exact matching: value 1 and 2 act only when exactly 1 or 2; 3 or more is "always"; 0, negative values and anything between (1.5) give no steps [derived].
- No step is sent when the player is dead (health 0 or less) or when another event already took the player's event slot that frame (a landing, a teleport, an item respawn) [read].
- The rate comes from the view bob cycle, advanced each server frame by 0.25 above 210 u/s, 0.125 above 100 u/s, 0.0625 below that (on the ground only); a step fires when the cycle crosses a whole number. So a step every 0.4 s above 210 u/s, every 0.8 s from 100 to 210, every 1.6 s below 100. Below 5 u/s the cycle stops and resets, so no steps even at 3 [read/derived].
- In the air the cycle keeps advancing at the last amount set (so value 3 steps in the air at the last running rate) [derived].
- Crouched quirk: while crouched, the number the step test compares against is the cycle scaled by 4 while the other side is not, so once the cycle passes about 0.25 the test fires nearly every server frame: with g_footsteps 2 or 3 a crouch-walking player steps about 10 times a second. At value 1 the speed limit of 225 normally keeps crouching players out [derived].
- What the game sends: only a "footstep" event on the player entity; it chooses no sound [read]. The client plays one of player/step1.wav to player/step4.wav at random, volume 1, normal attenuation, on the player's body channel, and only if the client's "cl_footsteps" setting (default 1) is non-zero [read]. No sound by surface or material in this release [read].
- Independently of g_footsteps, a short landing (a fall too small to hurt, landing speed between 1 and 15 in the game's fall measure, halved in shallow water) also sends the footstep event [read].
- In multiplayer the client forces its g_footsteps back to 1 (see 6) [read].

## 9. g_fix_triggered

- Default 0; neither archived nor latched; read each time a monster is placed (at map load, and for monsters spawned later) [read].
- The problem: a monster with the "trigger spawn" flag (spawnflag 2) waits hidden until something targets it by name. If the mapper gave it no name, nothing can ever wake it [read]. It is still counted in the level's monster total, so the "monsters killed" count can never reach the total [derived].
- At either value the game prints to the console: "triggered <classname> at <position> has no targetname" [read].
- With 1 the trigger-spawn flag is removed from such a monster, so it appears at map start like any other monster [read]. Monsters that do have a name are unaffected [read].
- Deathmatch places no monsters, so the switch matters only in single player and coop [read].

## 10. g_commanderbody_nogod

- The tank commander's decapitated body (a map decoration that slumps when its head is picked up). Default 0, archived; read when the body is placed at map load [read].
- 0, classic: the body is invulnerable; hits show sparks and do nothing [read].
- 1: the body is not invulnerable, is marked as already dead and as a monster corpse [read]. Its health starts at 0 and its gib threshold is 0, so the first hit that does any damage gibs it: the sound "tank/pain.wav", one small meat gib, four small metal gibs, a chest gib, and the body itself turns into a metal "gear" head gib [read/derived]. Hits draw blood rather than sparks [read].
- With 1 it does not change the monster count or kill count (it is already counted dead) [read]. Being a corpse, players can walk through it while shots still hit it [derived]. Picking up the head before it is gibbed still plays its slump ("tank/pain.wav", then "tank/thud.wav" as it lands) [read].

## 11. maxentities

- Default 1024; latched (a change takes effect on the next game start, when the game library is loaded) [read].
- Lower clamp: at least maxclients + 1; a smaller value is forced up and the setting rewritten [read]. maxclients itself is forced to at least 1 [read].
- Upper limit: the game library sets none. The network protocol's entity ceiling in this release is 1024 [read]; I found no check in the server that refuses a larger maxentities, so what happens above 1024 is open.
- Slots: slot 0 is the world, slots 1 to maxclients are the players [read]. In deathmatch and coop the next 8 slots are the body queue (corpses left by respawning players) and are never freed; in single player only the world and player slots are never freed [read].
- Finding a slot for a new entity [read]: a free slot after the player slots is reused first, but one freed in the last 0.5 s is passed over (except in the level's first 2 s); failing that a new slot is added at the end while under the limit; at the limit any free slot is taken, even one just freed. When none is free, a required entity ends the level with the error "Game Error: G_Spawn: no free edicts" (the server drops the map).
- Optional entities are simply not made when the slots run out: gibs and thrown debris, the markers that let monsters hear player noises, a monster's temporary search goal (it moves normally instead), the Makron's torso, and the extra player starts some maps get [read]. Nothing old is freed to make room [read].

## 12. Other gameplay switches the game library adds

Settings (beyond the ones in our notes) [read]:
- g_quick_weap, default 1, archived: "weapnext" and "weapprev" step on from the weapon already queued, not the one in hand, so pressing them quickly skips several weapons; each step shows the new weapon's icon and name for about 0.9 s.
- g_swap_speed, default 1, archived: weapon raise and lower animations advance this many frames per server frame (whole numbers above 1 speed them up; anything else counts as 1). Forced back to 1 by a connected multiplayer client.
- coop_pickup_weapons, default 1, archived: in coop, a weapon you already own can still be picked up once if no one has taken it yet; 0 leaves owned weapons for others, as classic.
- coop_elevator_delay, default 1.0 seconds, archived: in coop, platforms wait this long before moving once someone steps on them; 0 or less moves at once.
- cl_gun (read by the game for the HUD, default 2): when centre-handed or at a field of view above 91, the HUD shows the current weapon's icon unless cl_gun is 2.
- g_disruptor does not exist in this game library (no setting by that name) [read].

Commands added beyond the classic set [read]:
- cycleweap <weapon...>: switch to the next owned, loaded weapon in the given list.
- prefweap <weapon...>: switch to the first owned, loaded weapon in the given list.
- teleport x y z, spawnentity <classname> x y z [pitch yaw roll [spawnflags]], spawnonstart <classname> (at every start spot), listentities <all|ammo|items|keys|monsters|weapons>: debugging commands; in deathmatch or coop they need the "cheats" setting, with the message "You must run the server with '+set cheats 1' to enable this command."
