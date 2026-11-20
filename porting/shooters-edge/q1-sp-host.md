# Quake single player, Reader A: the engine's physics and builtins under the single-player program

Read: id Software's Quake source release (GPL-2.0), the WinQuake tree, built the way that tree
builds by default (the optional "Quake 2 prototype" paths are off, so no rotation, no conveyors,
no extra builtins). Where the program's expectations mattered, the game program's QC source as
id released it with its Quake tools (GPL), which is the 1.01 program, not 1.06: none of the facts
below depend on a line that changed between the two as far as I can tell, but where a fact rests
on the program it is marked as read from 1.01. Ours: /home/user/ge at the time of reading.

Marks: [read] read in the source or in our code, [derived] worked out from what was read,
[measured] none here (nothing was run).

## The ranked table

| # | Rank | Area | Difference (as a player or mapper sees it) | Coop? |
| --- | --- | --- | --- | --- |
| 1 | 2 | Sounds the engine starts | The Fiend's (and the Rottweiler's) landing thud after a leap and the splash of a grenade, gib, backpack or monster crossing a water surface are never heard: the engine starts them but our host never sends them | also |
| 2 | 2 | Brush entities' size | Every door, button and secret door travels 2 units less, a plat with no height set stops 2 units higher, a train rides 1 unit off its path corners on each axis, and every trigger reaches 1 unit less: our brush models are not widened by a unit on each side | also |
| 3 | 2 | Client: level loops | Torches, hums, drips and wind loops (static sounds) share our 24 sound channels with gunfire; in a map with many torches, or after a fight, a loop that lost its channel never comes back, so torches go silent | also |
| 4 | 2 | Co-op pause | Derived, to verify: when the host opens the menu or console in co-op, the whole co-op game freezes; the original pauses only a one-player game | only |
| 5 | 3 | Pushers | A lift or door moves what it carries or shoves by placing it, not sweeping it: the carried thing fires no triggers and no touch on the way, an entity already stuck in something is never pushed, and a pusher that snaps to its end point at the end of a move carries its riders the last fraction of a unit | also |
| 6 | 3 | Toss | An item, corpse or grenade resting on the floor that is given upward speed by the program lifts off at once in ours; in the original it stays put until it loses the floor some other way | also |
| 7 | 3 | Speed limit | A thrown thing faster than sv_maxvelocity (2000) is slowed along its whole direction in ours, per axis in the original: gibs from an overkill (quad rocket) fly flatter and shorter in ours | also |
| 8 | 3 | Falling monsters | A dead walking monster (not solid) that falls passes through other monsters and players in ours; in the original it lands on them | also |
| 9 | 3 | Autoaim | Ours aims from 20 above the shooter at the target's box centre; the original corrects the pitch from origin to origin: on level ground ours aims about 12 units lower at man-sized monsters (20 at a Rotfish) | also |
| 10 | 3 | Traces | A sweep that starts inside a monster's or item's box keeps the world's hit in ours; in the original that box's result (often a full move) replaces it. A trace that starts and stays in solid ends at its start in ours, at its end in the original (a thing stuck in a wall slides on through it in the original) | also |
| 11 | 3 | findradius | The chain of entities in a blast comes in a different order (ours walks the area tree, the original walks the entity list): chained exploding boxes can damage a different set | also |
| 12 | 3 | Monster sight | checkclient tests the monster's eye against the player's view set as it is now; the original against a set taken when the player was last chosen (up to 0.1 s old) | also |
| 13 | 3 | Removed entities | A removed entity keeps its classname and health in the original until reused; ours clears both | also |
| 14 | 3 | Angles | vectoyaw and vectoangles return whole degrees (truncated) in the original, fractions in ours: monster facing and missile angles differ by under a degree | also |
| 15 | 3 | Monster chase | The chase's choice of which axis to try first compares whole-unit distances in the original, exact ones in ours | also |
| 16 | 3 | setmodel on models and sprites | The original gives a model a 32-unit cube and a sprite its picture's half sizes as the entity's box; ours leaves the box: on the last map the teleporter ball (misc_teleporttrain) flies 16 units off its path on every axis | also |
| 17 | 3 | Sound position | The original places every entity's sound at its box centre; ours only for brush and size-less trigger entities (monsters' sounds a few units lower) | also |
| 18 | 3 | Who hears | Ours sends a sound only to players in its hearing set or within 1024 units; the original to everyone: in co-op another player's death and gib cries (heard map-wide) are lost far away | only |
| 19 | 3 | Cheats | A noclipping player touches triggers and picks up items in the original, not in ours; fly slides along walls in the original, stops dead in ours; god and give do nothing in ours; notarget is missing | also |
| 20 | 3 | Frame | host_framerate (slow motion, fixed frame) is not offered | also |
| 21 | 3 | Touch order | Ours lists the triggers a mover overlaps before touching any; the original re-checks the mover's box before each, so a teleporter overlapping another trigger fires the other in ours | also |
| 22 | 3 | Around the player's move | The player's speed is bounded after gravity in ours, before the think in the original (only past 2000 units/s) | also |
| 23 | 3 | Co-op setting | Ours can switch co-op on mid-map and rewrite the program's coop and deathmatch globals; the original reads them once at map start | only |
| 24 | 3 | Contents | The map's current contents (-9 to -14) read as water in the original, as themselves in ours (no stock map has them) | also |

## 1. Sounds the engine starts are never sent (rank 2)

**Original.**
- Two sounds are played by the engine itself, not the program; the program only precaches both
  (1.01 worldspawn) [read].
- The splash, "misc/h2ohit1.wav", on channel 0 at volume 1 and attenuation 1, when a tossed,
  bouncing, flying or missile entity (after a frame in which its move hit something) or a
  walking monster (every frame) has its contents change between empty and water, slime or lava,
  tested at its origin [read].
- The landing thud, "demon/dland2.wav", same channel, volume and attenuation, when a walking
  monster (MOVETYPE_STEP, neither flying nor swimming) that was falling faster than 80 units/s
  downward at the start of the frame (a tenth of sv_gravity) has the floor flag after that
  frame's fall [read]. The Fiend's and the Rottweiler's leaps end this way, and so does any
  walking monster or walking corpse dropping off a ledge [derived].

**Ours.** plugins/shooter/physics.cpp `check_water_transition` and `physics_step` play
`kSoundSplash` and `kSoundLand` with `start_sound`, which only appends to the `Server` sounds list
[read]. Nothing in games/shooters_edge/server, client or net reads the `Server` sounds list for a QC
program (the only readers are plugins/shooter/senses.cpp, the bots and the multiplayer mirror)
[read]. The program's own sounds go through `bi_sound` as `NetEvent`s, so they are heard; these
two never are [derived].

**Test.** A campaign test that drops a step monster 64 units onto a floor and a grenade into a
water brush and checks that the frame's `out.events` holds a sound event named
"demon/dland2.wav" and one named "misc/h2ohit1.wav". In play: a Fiend's leap on e2m2 or e3m1.

## 2. Brush entities' bounds (rank 2)

**Original.**
- When a map is loaded every inline brush model's bounds are widened by 1 unit on every side
  (the stored mins less 1, maxs plus 1) [read]. setmodel on "*n" gives the entity those widened
  bounds as mins and maxs, and size as their difference, so size is 2 units more than the
  stored extent on each axis [read].
- The program sizes its movers from them (1.01 [read]): a door's and a button's travel is the
  size along its move direction less its lip; a plat with no height lowers by its height less
  8; a train is placed at each path_corner's origin less its mins; a secret door's travel uses
  its size [read].
- So, against ours: doors and buttons travel 2 units further, a plat without height stops 2
  units lower, a train sits 1 unit higher, further along x and further along y at every corner,
  and a trigger's box (and so its reach) is 1 unit larger on every side, before the 1-unit
  linking margin both engines add [derived].
- The world model is widened too; the area tree's root splits at the same midpoints, deeper
  splits move by half a unit or less [derived].
- The same widening is in the multiplayer engine of the same release [read].

**Ours.** plugins/shooter/bsp_world.cpp copies each model's stored bounds unchanged into
the `BrushModel` mins and maxs; games/shooters_edge/server/bsp_map.cpp hands them to the map spec, and
`set_brush_model` (plugins/shooter/server.cpp) sets them as the entity's size [read]. Only the
`.bsp` item path in `bi_setmodel` widens by 1 [read].

**Test.** On e1m1, the first door's `pos2` minus `pos1` after spawn: 2 units longer in the
original's terms than ours today; a func_plat without height: its bottom origin. A unit test
of `bsp_world` against a fixture model whose stored bounds are known.

## 3. The level's looping sounds share 24 channels (rank 2, client)

**Original.**
- The client has 8 channels for sounds started during play and, separately, one channel per
  static sound (ambientsound), up to 128 channels in all; a static sound never loses its
  channel [read].
- Each frame a static loop is heard by distance; identical loops in range are combined [read].
- A new dynamic sound takes, in order: the same entity's same numbered channel (channel 0
  never replaces), else the dynamic channel with the least time left, but never a channel the
  player's own entity is using for a monster's sound [read].
- Static sounds: volume byte over 255, attenuation byte over 64, silent beyond 1000 units over
  the attenuation (ATTN_STATIC 3: 333 units) [read]. Every flame light
  (light_torch_small_walltorch, light_flame_*) adds one fire loop at volume 0.5 and attenuation
  3; light_fluoro, light_fluorospark and the ambient_* entities add theirs (1.01) [read].

**Ours.**
- games/shooters_edge/client/demo_sound.cpp plays the level's static sounds once, when they
  arrive, on the same `Channels` pool of `kDemoChannels` (24) as every other sound, and never
  starts a static again [read].
- engine/audio/channels.hpp: when every slot is taken a new sound steals the quietest slot
  (oldest among equals) or is dropped when quieter still [read].
- So a map with more than 24 loops loses some at once, and later gunfire steals loops that are
  quiet because they are far away; walking up to them, they stay silent [derived].
- Dynamic sounds get more room than the original's 8 [derived].

**Test.** A listen lab on e1m1 (many torches): count `DemoSoundStats` loops started against the
signon's static sounds, then fire 30 shots and walk to the far torches; the loops sounding near
the listener should equal the original rule (every loop within 333 units audible).

## 4. Co-op pauses with the host's menu (rank 2, co-op only, derived)

**Original.** The server's client think and physics are skipped while the console or a menu is
up only when the server has one player slot; with more than one (co-op) the game runs on
[read].

**Ours.** games/shooters_edge/client/main.cpp holds the listen game (`listen_advance` gets 0
frames) whenever `listen.campaign_host` is set and the console or game menu is open [read],
`campaign_host` is set for any campaign, co-op included (games/shooters_edge/client/listen.hpp)
[read]. Whether remote co-op players ride this same listen loop was not followed to the end
[open].

**Test.** A co-op listen test: host and a joined client, the host's menu open for 1 s; the
server's time must advance by 1 s.

## 5. Pushers (rank 3)

**Original.**
- Order and time.
  - A pusher's move time is the frame, or the time left until its nextthink on its own clock
   (ltime) when that comes sooner (never below 0) [read].
  - A still pusher only adds the move time to ltime [read].
  - Its think runs when nextthink is above ltime before the move and at or below it after; the
   program then sees the server's time, not the think time [read].
  - No rotation: avelocity on a pusher moves nothing [read].
- Who is moved.
  - Every entity in use except pushers, MOVETYPE_NONE and MOVETYPE_NOCLIP is considered, in
   entity order [read].
  - A rider (on-ground flag with the pusher as its ground entity) always; anything else only
   when its box overlaps the pusher's moved box and it is stuck there [read]. "Stuck" is a test
   against everything solid, by the normal rule, whatever the entity's own solidity [read].
  - Non-walking entities lose the on-ground flag [read].
- How it is moved.
  - It is swept (the pusher made not solid for the sweep) by the same move: the sweep stops at
   walls, monsters and players by the entity's own trace kind [read].
  - After the sweep it is relinked with trigger touches, and the touch functions of it and of
   what the sweep hit are run [read].
- When it is still stuck afterwards:
  - a point-sized entity (mins x equal to maxs x) is left where the sweep put it [read],
  - a SOLID_NOT or SOLID_TRIGGER one (a corpse, an item) is flattened: mins x and y set to 0
   and maxs set equal to mins, size untouched, left where the sweep put it [read],
  - anything else blocks: it goes back (relinked with touches), the pusher goes back, ltime
   loses the move time, the pusher's blocked function runs with self the pusher and other the
   blocker, and every entity moved so far goes back, relinked without touches [read].
- A think that moves the pusher (the program's snap to the end point) just places it [read].

**Ours.** plugins/shooter/push.cpp `push_move`: same candidates, rider rule, overlap test,
on-ground clearing, corpse flattening (and size zeroed), point rule and blocked call [read].
Differences [read]:
- the entity is placed by adding the move, not swept, and relinked without trigger touches and
  without impacts,
- an entity already stuck before the push (with the pusher not solid) is skipped,
- a stuck entity whose old place is free is left there and not counted as moved,
- the stuck test treats SOLID_NOT and SOLID_TRIGGER entities as meeting only brush models
  (`test_entity_position`),
- after a think that moved the pusher by more than 1/64 unit, `physics_pusher` moves it back and
  pushes by that amount, carrying riders.

[derived]: a grenade lying on a lift is never touched against what it is shoved into; an item
or monster carried through a trigger does not fire it until it moves by itself; riders follow
the end snap.

**Test.** shooter_push_test cases: a rider carried through a trigger (count touches), a
bouncing missile on a plat shoved into a monster (impact count), an entity overlapping a wall
when the door arrives.

## 6. Toss entities on the floor (rank 3)

**Original.** A toss, bounce, fly or missile entity thinks first; then, holding the on-ground
flag, it does nothing at all that frame, whatever its velocity [read]. The flag is set when its
move hits a plane whose normal z is above 0.7 and either it is not bouncing or its clipped
upward speed is under 60; velocity and avelocity are then zeroed and the hit entity becomes its
ground entity [read]. The flag goes only when a pusher moves it or the program clears it [read].

**Ours.** `physics_toss` clears the flag first whenever velocity z is above 0 [read], so a
resting entity given upward speed by the program lifts off [derived]. In 1.01 the program clears
the flag itself where it throws things (the player's head) [read], so the stock game rarely
reaches this [derived].

**Test.** shooter_physics_test: a toss entity on the floor with velocity z 100 set must not move
for 10 frames.

## 7. sv_maxvelocity (rank 3)

**Original.** Every axis of the velocity is clamped to plus or minus sv_maxvelocity (default
2000) on its own; a non-number axis (of velocity or origin) becomes 0 with a console line
[read]. Toss and step entities are bounded before gravity is added; players before their think
[read].

**Ours.** `bound_velocity` scales the whole vector down to the limit [read]. The program's gib
speed for damage worse than 200 can reach about 1000, 1000, 3000 (1.01 velocity for damage, times
10) [derived]: per axis that becomes 1000, 1000, 2000; scaled it is about 600, 600, 1800 [derived].

**Test.** shooter_physics_test: velocity 1000, 1000, 3000, one toss frame with gravity 0.

## 8. Step movers that are not solid (rank 3)

**Original.** A falling MOVETYPE_STEP entity slides with the normal trace (it meets monsters and
players) whatever its solidity [read].

**Ours.** `fly_move` traces with the no-monsters kind when the mover is SOLID_NOT [read], so a
falling monster corpse passes through monsters and players [derived].

**Test.** A corpse (SOLID_NOT, MOVETYPE_STEP) dropped onto a standing monster.

## 9. Autoaim's answer (rank 3)

**Original.**
- aim(entity, speed); the speed is ignored [read].
- A line from the shooter's origin plus 20 up along v_forward, 2048 long, normal kind: when it
  hits an entity whose takedamage is 2 (DAMAGE_AIM) and not a team-mate (teamplay set, the
  shooter's team above 0 and equal to the other's), the answer is v_forward [read].
- Otherwise every entity with takedamage 2 except the shooter and team-mates is tried in entity
  order: the direction from the start to its box centre, normalised; its dot with v_forward
  must be at least the best so far (starting at sv_aim, 0.93); a line to its centre must hit
  it; then it becomes the best [read].
- With a best entity the answer keeps v_forward's direction scaled by the distance from the
  shooter's ORIGIN to the target's ORIGIN along v_forward, takes the z difference of the two
  ORIGINS as its z, and is normalised; with none, v_forward [read].

**Ours.** `autoaim` in bi_world.cpp: the same search [read], but the final vector is built from
the target's box centre minus the start (origin plus 20) [read]. With the 1.01 boxes (soldier,
zombie, knight, scrag: centre 8 above the origin; Rotfish: at the origin; Fiend, Shambler: 20
above) ours points 12 units lower over the horizontal distance at a man-sized target at the
same height, 20 lower at a Rotfish, the same at a Fiend or Shambler [derived].

**Test.** A builtins test: shooter at the origin facing +x, a soldier-sized target 500 units
ahead at the same height and 2 units off the line: the original's answer has z exactly 0.

## 10. Trace merging and stuck sweeps (rank 3)

**Original.**
- A sweep first meets the world, then each linked solid it could reach. An entity's result
  replaces the best so far when it is all-solid, starts solid, or ends sooner [read]. A
  start-solid result therefore replaces even a nearer world hit (so a mover starting inside a
  monster's box can be given a full move through a wall), and start-solid is kept on [read].
- A sweep that never leaves solid reports fraction 1 and ends at its END point [read]: a toss
  entity stuck in a wall is carried on to its end each frame (and its touch called with the
  world), and traceline's trace_endpos is the end [read].

**Ours.** plugins/shooter/trace.cpp `clip_links` replaces only on all-solid or a nearer
fraction and just sets start-solid otherwise; `clip_to_entity` ends an all-solid trace at its
START [read].

**Test.** shooter_trace_test: a box sweep starting inside a monster's box toward a wall 10
units away; a point sweep from inside the world's solid.

## 11. findradius's order (rank 3)

**Original.** Every entity in use, in entity order from 1, that is not SOLID_NOT and whose box
centre is within the radius (a true distance) is put at the head of the chain, so the chain
runs from the highest entity number down [read]. Unlinked entities count too [read].

**Ours.** `radius_walk` walks the area tree (each node's solids then triggers) and prepends what
it finds [read]: another order, and only linked entities [derived]. The order matters when the
program's radius damage sets off another radius damage inside its loop (exploding boxes), since
both walks write the same chain field [derived].

**Test.** A builtins test of findradius's chain order over five entities in two area nodes.

## 12. checkclient (rank 3)

**Original.**
- When at least 0.1 s of server time has passed since the last choice, the next client is
  chosen: from the one after the last (wrapping over the player slots) the first that is in use,
  alive and without FL_NOTARGET; failing all, the last one stays even if it no longer
  qualifies; with one player slot the only player is always kept, notarget or not [read].
- At the choice the visibility set (not the widened one) of the leaf at the chosen player's eye
  is copied [read].
- Each call: world when the chosen player is free or dead; world when the caller's eye is in no
  leaf or a leaf outside the copied set; else the player [read].
- notarget is honoured by the program (1.01's target search) [read].

**Ours.** `bi_checkclient`: the same 0.1 s cycle over 32 slots [read], but the visibility is
tested at call time between the two eyes' clusters [read]. With one player, notarget makes ours
return world; the program refuses him anyway [derived]. The difference is the up-to-0.1 s
stale set [derived].

## 13. Freed entities (rank 3)

**Original.** remove clears model, takedamage, modelindex, colormap, skin, frame, origin,
angles and solid, sets nextthink to -1 and notes the time; every other field (classname, health,
owner, enemy, ...) stays until the slot is reused [read]. Reuse: a free slot freed before 2 s of
server time, or more than 0.5 s ago [read].

**Ours.** games/shooters_edge/sim/qc_program.cpp `free_rule` also clears classname and health
[read]. Reuse rule the same [read].

## 14. vectoyaw and vectoangles (rank 3)

**Original.** Both truncate yaw and pitch toward zero to whole degrees before adding 360 to a
negative [read]. The multiplayer engine of the same release does the same [read].

**Ours.** plugins/progs/stdlib.cpp returns the fraction [read].

## 15. The chase's axis choice (rank 3)

**Original.**
- movetogoal: nothing when the mover has none of on-ground, fly, swim; nothing when it has an
  enemy and its box widened by the distance meets the goal's [read].
- Else, one time in four, or when stepping along ideal_yaw fails, a new chase direction [read]:
  - the diagonal toward the goal first (45, 135, 215 or 315; 215, not 225, for the minus-x,
   minus-y quarter), never the turnaround [read],
  - then the two axis directions, swapped when a coin (one in two) says so or when the y
   distance is larger than the x distance, both truncated to whole units first [read],
  - then the old direction, then all eight from 0 up or from 315 down (a coin), then the
   turnaround [read],
  - failing all, ideal_yaw goes back to the old direction, and a mover without a full floor gets
   the partial-ground flag [read].
- An axis is skipped when the goal is within 10 units on it [read].

**Ours.** `new_chase_dir` matches all of it except that it compares exact distances [read].

## 16. setmodel on models and sprites (rank 3)

**Original.** setmodel sets the box from the model whatever its kind: an alias model always
gives a cube from -16 to 16; a sprite gives plus and minus half its widest frame across and half
its tallest frame up and down; a brush model its widened bounds; and it links the entity
(without touches) [read]. Only misc_teleporttrain in 1.01 keeps such a box (it calls setsize
with its own mins and maxs after setmodel), so its train path is offset by 16 on each axis
[read, derived].

**Ours.** `bi_setmodel` leaves the box alone (and does not link) for anything but "*n" and
`.bsp` files [read].

## 17. Where a sound starts (rank 3)

**Original.** Every sound is placed at the entity's origin plus the centre of its box [read].

**Ours.** `bi_sound` adds the box centre only for SOLID_BSP, and SOLID_TRIGGER with no model
[read].

## 18. Who hears a sound (rank 3, co-op only)

**Original.**
- Sounds go to every client, unattenuated by any set; only the listener's distance silences
  them [read].
- Attenuation 1 (ATTN_NORM) is silent at 1000 units, 2 (ATTN_IDLE) at 500, 3 (ATTN_STATIC) at
  333; 0 (ATTN_NONE) is heard everywhere [read].
- The level's datagram drops a sound when it is within 16 bytes of 1024 [read].

**Ours.** `bi_sound` sends to the clients whose hearing set holds the sound or who are within
1024 units, unless sv_phs is 0 [read]. Only ATTN_NONE sounds are lost by this [derived]; in 1.01
those are the player's own death, gib and telefrag cries [read], so another co-op player's
are not heard from far away [derived].

## 19. The player's cheats (rank 3)

**Original.**
- After every player's move the engine links him with trigger touches, whatever his movetype
  [read], so a noclipping player picks up items and fires triggers [derived].
- MOVETYPE_FLY on a player thinks, then slides along walls (the same multi-plane slide as the
  walk), without gravity [read].
- The original's god, notarget, noclip, fly and give work in single player and co-op without a
  cheats setting (refused only in deathmatch); god and notarget set the flags the program reads
  [read].

**Ours.**
- `frame_move` links a noclipping player without touches [read].
- A flying player goes through `physics_toss`, one straight push that stops at the first wall
  [read].
- games/shooters_edge/server/commands.cpp accepts god and give but changes nothing, has no
  notarget, and needs cheats on [read].

## 20. host_framerate (rank 3)

**Original.**
- The host frame runs only when at least 1/72 s of real time has passed (except in a timedemo)
  [read].
- frametime is the real time since the last frame, clamped to 0.001 to 0.1 s; host_framerate
  above 0 replaces it [read].
- sys_ticrate (0.05) only paces a dedicated server [read].

**Ours.** A fixed 1/72 s step (campaign.hpp `kCampaignStep`) [read]; no host_framerate [read].

## 21. Trigger touch order (rank 3)

**Original.** Touching walks the area tree's trigger lists and tests the mover's box as it is at
each trigger, and descends by the box as it is at each node [read]. A trigger that teleports
the mover stops later triggers at the old place from firing [derived].

**Ours.** `touch_links` lists all overlapping triggers first, then calls each [read].

## 22. Around the held player move (rank 3)

**Original.** In a player's physics the velocity is bounded right after PlayerPreThink, before
his think, gravity and the stuck check [read].

**Ours.** `frame_move` bounds it after gravity [read]. Only above 2000 units/s on an axis.

## 23. Co-op switched on mid-map (rank 3, co-op only)

**Original.**
- At map start: when coop is set, deathmatch is set to 0; the program's coop global is
  written when coop is set, else its deathmatch global [read].
- The teamplay global is never written by the engine; the 1.01 program reads teamplay and skill
  with cvar every StartFrame [read].
- Changing coop takes effect at the next map [read].

**Ours.** `campaign_set_coop` writes coop, deathmatch and the program's globals during a map
[read].

## 24. Current contents (rank 3)

**Original.** pointcontents and every contents test the physics makes fold the six current
contents (-9 to -14) into water (-3) [read].

**Ours.** `world_contents` returns the hull's leaf contents as they are [read].

## Answers by question

**Q1, the frame.**
- Original order each host frame [read]:
  - every client's messages are read and his command thought (angles, wish speed),
  - then physics: StartFrame with self and other the world and time the server's time,
  - then every entity from 0 up, skipping free ones. With force_retouch above 0, each is first
   relinked with touches. A player slot runs: pre-think; the bound; by movetype (none: think,
   walk: think, water and gravity, unstuck, walk; toss and bounce: the toss; fly: think then
   the slide; noclip: think then move); relink with touches; post-think. Everything else runs
   by its movetype,
  - force_retouch then drops by one, and the server's time advances by the frame after the
   whole frame, so everything in it sees the time it started at.
- Think rule [read]:
  - a think runs when nextthink is above 0 and at most the server's time plus the frame,
  - the program sees the later of nextthink and the server's time,
  - nextthink is cleared before the call; at most one think per entity per frame,
  - an entity spawned during the frame with a higher number runs in the same frame.
- The 0.1 s monster cadence is the program's nextthink; a think due inside a frame runs at the
  frame's start, up to one frame early, and sees time equal to its nextthink exactly [derived].
- The map starts at time 1.0 and runs two 0.1 s frames before anyone joins [read].
- Ours matches all of this (frame.cpp, physics.cpp, client.cpp `run_player_frame`) [read],
  except rows 19, 20 and 22.

**Q2, movetypes.**
- Gravity [read]:
  - velocity z loses the entity's gravity field (looked up by name; 0 or absent means 1) times
   sv_gravity (800) times the frame,
  - for toss and bounce only (not fly, not flymissile) and for falling step entities.
- Toss, bounce, fly and flymissile [read]:
  - think; nothing more while on the floor; bound; gravity as above,
  - angles advance by avelocity; one sweep (missile kind for flymissile, no-monsters for
   trigger and not-solid, else normal) with a relink with touches and an impact,
  - on a hit, velocity is clipped with a rebound of 1.5 for bounce, 1 otherwise, and the floor
   rule of row 6 applies; components under 0.1 become 0,
  - the water transition is tested only after a hit.
- Water transition [read]:
  - on the first test (watertype 0) the contents are taken silently with waterlevel 1,
  - after that, a change between empty and water, slime or lava plays the splash,
  - leaving water leaves waterlevel equal to the contents number (-1).
- Step [read]: only without on-ground, fly or swim it falls (the 80 units/s thud rule, gravity,
  bound, slide, relink with touches); then it thinks; then the water transition every frame.
  The slide sets on-ground only on a floor (normal z above 0.7) of a SOLID_BSP entity.
- None: think only. Noclip (not a player): think, angles and origin advance, relink without
  touches [read].
- Ours matches except rows 1, 6, 7, 8 [read].

**Q3, pushers.** Row 5, every fact there. Also [read]: pushers ignore each other, MOVETYPE_NONE
and noclip; no rotation in this engine; a SOLID_NOT or SOLID_TRIGGER entity is pushed like any
other and is flattened rather than blocking.

**Q4, monster movement.**
- walkmove(yaw, dist) [read]:
  - 0 without on-ground, fly or swim; else the step move by dist along yaw, relinked with
   touches; returns whether it moved; the program's self is kept across it.
- The step move [read]:
  - flyers and swimmers: first with an 8-unit height correction toward the enemy (8 down when
   more than 40 above it, 8 up when less than 30 above), then without, only when there is an
   enemy; a swimmer whose end is in empty contents fails,
  - walkers: the box is swept from 18 above the target spot down 36 (a second try from the
   spot itself when the first starts in solid); a miss fails unless the mover has the
   partial-ground flag (then it moves and loses on-ground); the bottom check (below) failing
   puts it back unless partial-ground; success clears partial-ground and sets the ground
   entity.
- checkbottom [read]:
  - true at once when the four points 1 below the box's bottom corners are all in solid,
  - else a line from the bottom centre down 36 (no-monsters kind) must hit, and from each
   corner down to the same depth must hit no more than 18 below the centre's hit.
- droptofloor [read]: sweeps the box 256 down; fails (0) on a miss or all-solid; else moves
  there, links without touches, sets on-ground and the ground entity, returns 1.
- changeyaw [read]: turns the yaw (wrapped to the 65536-step circle) toward ideal_yaw by at
  most yaw_speed per call, the short way; nothing when already equal.
- The step direction used by movetogoal [read]: sets ideal_yaw, turns, moves without relinking,
  undoes the move when the yaw is then still more than 45 degrees off, relinks with touches
  either way.
- movetogoal: row 15 [read]. It returns nothing [read].
- Ours matches all of it except row 15 [read].

**Q5, sight and targeting.**
- checkclient: row 12.
- traceline(v1, v2, nomonsters, ent) [read]:
  - 0 meets everything solid,
  - 1 only brush entities,
  - 2 meets monsters (FL_MONSTER) as a 30-unit cube around the line (the missile kind),
  - any other value acts as 0,
  - ent is passed over, and so are its owner and what it owns,
  - a sized mover passes size-less entities,
  - trace_ent is the world when nothing was hit.
- Ours: the same kinds [read] (a value with 64 set is read as its kind without 64, ours only).
- aim: row 9. visible and infront are the program's (traceline and makevectors) [read].

**Q6, touch and triggers.**
- Every relink with touches calls the touch of every SOLID_TRIGGER entity with a touch whose
  box meets the mover's, with self the trigger, other the mover and time the server's time; a
  SOLID_NOT mover is not linked and touches nothing [read].
- Relinks with touches happen [read]:
  - after a player's move,
  - on every toss sweep,
  - on a step entity's fall,
  - on walkmove and movetogoal steps,
  - on force_retouch,
  - on a pushed entity (row 5),
  - never on setorigin, setsize, setmodel or droptofloor.
- Box grow [read]: an item (FL_ITEM) has its box widened 15 sideways when linked; everything
  else 1 on every side.
- Impacts (a sweep hitting something) call both touch functions, the mover's first, skipping a
  SOLID_NOT side [read].
- Items do not touch on spawn: they are placed by droptofloor, which links without touches
  [read].
- Ours matches except rows 2 (trigger size), 5 and 21 [read].

**Q7, damage-related engine pieces.**
- Damage is the program's [read].
- pointcontents: the world's point contents (row 24) [read].
- Hulls [read]: against a brush entity the hull is chosen by the mover's x width (under 3:
  point; up to 32: player-sized; more: large); against anything else a box made of the two
  boxes.
- setsize: no rotation, no checks beyond refusing mins above maxs (an error that stops the
  program) [read].
- setmodel: rows 2 and 16. particle sends the burst to every client [read].
- Ours: hull choice the same [read]; particle only to clients that can see the point, and only
  the stock program's kinds (count 255, colour 73, colour 225) [read], which covers every 1.01
  call that runs [read].

**Q8, sounds.**
- sound [read]:
  - channels 0 to 7 (0 never replaces, 1 to 7 replace that entity's channel), volume 0..1 as a
   byte, attenuation 0..4; out of range is a fatal error,
  - default byte volume 255 and attenuation 1 are left out of the message.
- Distance [read]: falloff is linear, silent at 1000 units over the attenuation. Stereo: each
  ear gets (1 minus distance factor) times (1 plus or minus the dot of the listener's right with
  the direction). The player's own entity is always full and centred.
- ambientsound [read]: goes into the level's signon (every client, joining later too) with
  volume times 255 and attenuation times 64 as bytes; the sound must loop.
- Leaf ambience [read]:
  - two automatic loops, "ambience/water1.wav" and "ambience/wind2.wav", at ambient_level
   (0.3) times the listener leaf's water and sky levels,
  - a level under 8 of 255 is silence,
  - each moves toward its target by ambient_fade (100 per second, out of 255).
- Ours: the leaf ambience matches (games/shooters_edge/sound/ambience.hpp: 0.3, a floor of 0.03,
  0.4 of full a second) [read]; volume 0.7 matches the original's volume default [read]. Rows
  1, 3, 17, 18 differ. The exact stereo law of engine/audio/channels.cpp was not compared [open].

**Q9, the rest.**
- lightstyle [read]: stores the style; sends it to every client only once the map is running,
  the signon carries the ones set while loading.
- makestatic [read]: writes model index, frame, colormap, skin, origin and angles to the signon
  and removes the entity.
- setspawnparms copies that client's spawn parms into parm1..16; a non-client is an error
  [read].
- changelevel [read]: queues one console changelevel line per map; later calls in the same map,
  including during the intermission, are ignored until the next map starts. The console
  command saves every client's parms through SetChangeParms and the program's serverflags,
  then starts the map.
- localcmd appends to the console buffer, run at the start of the next host frame [read].
- cvar returns the value; cvar_set sets it (unknown names print a line) [read].
- At map start [read]: skill becomes the whole number nearest (adding a half and truncating),
  clamped 0..3 and written back, and filters the map's entities. coop set forces deathmatch 0.
  The program reads skill every frame itself (1.01 StartFrame), so the start map's skill
  trigger changes the program's skill at once and the entity filter at the next map.
- Ours matches [read], except row 23; localcmd's lines run at the end of the same frame, the
  same moment in effect [derived].

**Q10, single player against multiplayer.**
- The engine pauses physics and client thinking while the console or menu is up, only with one
  player slot; ours does (and also in co-op, row 4) [read].
- Autoaim on by default (sv_aim 0.93), the ideal pitch, the punch [read]; ours has them [read].
- Walking monsters are sent without interpolation, so they step on screen at their think rate,
  our listen client draws the server's state each frame without interpolating, which gives the
  same look [read, derived].
- Rows 19 and 20 are the remaining player-facing gaps.

## What matches (checked, not to redo)

- Frame and clock [read]:
  - frame order; one think per frame; think time rule,
  - StartFrame's self, other and time,
  - force_retouch's per-entity relink and decrement,
  - the server's time advancing after the frame; start at 1.0 and two 0.1 s settle frames,
  - frametime given to the program.
- Pushers [read]: move time and ltime, think window and time, blocked call's self and other,
  rider rule, overlap rule, corpse flattening, point rule, no rotation.
- Toss [read]: think first, frozen on the floor, gravity only for toss and bounce, 1.5 rebound,
  0.7 floor normal, 60 units/s rest, splash logic, first-test quirk, waterlevel set to the
  contents on leaving water.
- Step [read]: fall only without on-ground, fly or swim; 80 units/s thud threshold; floor flag
  only on brush entities; think then water every frame.
- Noclip non-players: angles and origin, no touches [read].
- Monster builtins [read]: walkmove, movetogoal's early outs and its one-in-four, the 215 degree
  diagonal, the coins and their order, the eight-direction search both ways, partial-ground,
  checkbottom's 36 and 18, the flyer's 40/30 and 8, the swimmer's water rule, droptofloor's 256,
  changeyaw's wrap and clamp.
- Traces [read]: the kinds 0, 1, 2; the 15-unit missile cube; pass entity and owner rules; the
  size-less rule; hull by x width (3, 32); the trace globals.
- Area and linking [read]: the area tree's depth and split; list order (appended at the tail),
  the 15-unit item grow and 1-unit grow; SOLID_NOT not linked; touch's self, other and time,
  impact order.
- Entities [read]: slot reuse after 0.5 s, or at once before 2 s; nextthink -1 on remove.
  Ours numbers dynamic entities from 33 and allows 2048 where the original starts at the player
  count plus one and allows 600; nothing a player sees [derived].
- Builtins [read]:
  - aim's straight line, candidate rule, teamplay rule and sv_aim default,
  - checkclient's 0.1 s cycle and its skips,
  - lightstyle, makestatic, setspawnparms,
  - changelevel's once-per-map latch,
  - localcmd and the restart the 1.01 program sends on death,
  - cvar and cvar_set,
  - skill rounding, clamp and write-back; the entity filter by skill and deathmatch,
  - coop forcing deathmatch 0,
  - ambientsound's bytes; sound's byte volume and range checks,
  - particle for every 1.01 call,
  - droptofloor, pointcontents (stock maps), nextent, find.
- Sound and client [read]: the 1000-unit attenuation law, the static sounds' attenuation over 64,
  the leaf ambience's level, floor and fade, master volume 0.7, the player's own sounds
  centred.
- Pause with the console or menu in single player [read].
