# Quake II single player: what a player would notice (audit A)

Source read: id Software's Quake II game library, the GPL-2.0 source release. Where Yamagi Quake II
differs in a way that matters, it is noted. Our side is games/shooters_edge/server/q2_*.cpp and
q2_*.hpp, net/q2_game.cpp and the notes porting/shooters-edge/q2-movers.md, q2-rules.md and
q2-campaign.md.

Marks: **[read]** means read in the original's source. **[measured]** means counted in the stock
campaign's maps: every map's entity text was taken from the full release's pak0, from base1 to
waste3, 39 maps. **[derived]** means worked out from the read facts. **[ours]** means read in our
code.

Ranks: **1** a player notices it at once on the stock campaign. **2** a player notices it
sometimes. **3** rare or cosmetic.

## Ranked table

| Rank | Area | Difference |
| --- | --- | --- |
| 1 | Level flow | The end of a unit and the end of the game: a cinematic, then the next unit. Ours cannot load the "name.cin+next" string, so the player is stuck. |
| 1 | Targets | target_explosion's `delay` holds back the blast. Ours blasts at once and delays its targets instead. |
| 1 | Classnames | func_clock is not spawned (biggun, fact3): no countdown on the wall, and the "boom" target never fires. |
| 1 | Classnames | trigger_elevator is not spawned (lab ×3, mine2 ×1): the lifts that buttons send to a floor do not move. |
| 1 | Targets | target_laser draws no beam, its sparks are in the wrong place and colour, and the beam stops at the first monster. |
| 1 | Movers | func_explosive and misc_explobox throw no debris. The barrel uses the wrong explosion. |
| 1 | Items | An item's `target` and `message` never fire when it is touched (64 items). |
| 1 | Text | A "\n" in a map's message shows as a backslash and an n, not a new line (119 messages, 37 maps). |
| 1 | Level flow | worldspawn `gravity` is ignored: space should play at gravity 100. |
| 1 | Movers | Monsters do not open doors and are not carried or pushed by movers. Items, gibs and bodies are not pushed, and a crushed non-living thing does not explode. |
| 1 | Triggers | trigger_push with angle 0 (city1 ×2) should stop the toucher. Ours throws it along +x at 1500 and 2000 units/s. |
| 1 | Targets | target_speaker ignores `attenuation` (-1 heard everywhere: 149 speakers) and `volume`. |
| 1 | Targets | target_spawner cannot spawn gibs, caps monsters at 4, throws items up, and kills nothing at the spot. |
| 1 | Movers | No animated textures on buttons, doors, walls, rotating brushes, objects or explosive brushes. |
| 2 | Classnames | func_door_secret is not spawned (city3): the wall is drawn but has no collision and never opens. |
| 2 | Movers | Door teams: the members do not finish together, and the "Team" key in mixed case is not read. Other mixed-case keys are lost too. |
| 2 | Text | The trigger_counter message is "N more to go...". Ours says "Only N more to go...". |
| 2 | Text | The trigger_key message has no full stop in the original. Ours adds one. |
| 2 | Targets | Secrets and goals: the wrong sound at the player, and two built-in map fixes are missing (mine3, fact1). |
| 2 | Sound | Mover start and stop sounds, and the help computer beep, fall off too slowly in ours. |
| 2 | Targets | target_splash ignores `count` (default 32) and its angle: always 8 particles, upward. |
| 2 | Targets | target_temp_entity style 22 (boss1, the boss teleport effect and its sound) is lost. |
| 2 | Lights | Light styles 3 to 11 and 63 are not set: 15 stock lights stay still (styles 4, 6, 10). |
| 2 | Sound | Music: worldspawn `sounds` (the CD track) is never set, and a finished last goal does not stop the music. |
| 2 | Classnames | Missing models: misc_banner, misc_satellite_dish, light_mine2, and a misc_gib_head placed on a map. |
| 2 | Items | Power cubes with spawnflags 2 or 3 (bunk1, ware2) should be hidden until used, then a decoration that cannot be taken. |
| 2 | View | No screen tints: water, slime and lava, quad, invulnerability, suit, rebreather and the pickup flash. |
| 2 | Sound | No run-out warning sounds 3 s before a power-up ends. |
| 2 | Triggers | trigger_hurt and trigger_push touch only players in ours. |
| 2 | Triggers | Monsters never fire a trigger_multiple or trigger_once (the MONSTER and NOT_PLAYER flags, 15 uses). |
| 2 | Triggers | Triggers with angle 0 and a facing check (boss1, train) should not check facing. Ours checks toward +x. |
| 2 | Movers | Trains: the wait -1 stop, picking a corner when several share a name, and the teleport corner's effect. |
| 2 | Level flow | Intermission: a random spot among the first four, and wait for a button with no time limit. Ours takes the first spot and leaves by itself after 20 s. |
| 2 | Sound | Teleporting plays no teleport sound in ours. |
| 3 | Movers | func_water is never blocked in the original. Ours reverses it. |
| 3 | Movers | A blocked door team reverses by the blocked door's own state. Ours uses each member's state. |
| 3 | Movers | Team pushers: when one part is blocked, all parts stop. Only the master plays sounds. |
| 3 | Movers | A rider on a rotating brush: in the original the view barely turns. Ours turns it fully. |
| 3 | Movers | func_rotating with "dmg" "0" deals 2 in the original. |
| 3 | Targets | A delayed use plays talk1 instead of the trigger's `sounds`. |
| 3 | Targets | A message used by a non-player still plays talk1 at that entity. |
| 3 | Lights | target_lightramp: an off-by-one step on falling ramps. |
| 3 | Targets | target_blaster: the bolt's look and its obituary. |
| 3 | Classnames | The viper bomb's trail and explosion. |
| 3 | Classnames | misc_blackhole is translucent. |
| 3 | Items | Power armour switches itself off at 0 cells, with a sound. |
| 3 | Sound | The breathing sound's phase under the rebreather and the suit. |
| 3 | Triggers | trigger_gravity affects players too (no stock map uses it). |
| 3 | Level flow | In deathmatch, a changelevel without the exit flag kills the toucher. |
| 3 | Movers | A func_wall or func_explosive that appears kills monsters in the way too. |
| 3 | Items | Coop power cubes are taken one per player. |

Counts: rank 1, 14. Rank 2, 20. Rank 3, 17.

---

## Rank 1 details

### 1. The end of a unit and the end of the game

**Original [read]:** A target_changelevel's `map` string may hold three parts. A name ending in
".cin" or ".pcx" is a cinematic or a picture shown first. A `+` joins it to the next part. A
leading `*` on a map name means a new unit: the help computer's data and the carried state reset.
A `$name` after the map picks the info_player_start whose `targetname` is that name. In single
player, the server plays the cinematic full screen. When it ends (or a key skips it), the part
after `+` runs as the next map. A picture (".pcx") stays shown until the player presses a key.
Every change to a new unit, and the end of the game, first shows the intermission screen (see 33)
before the cinematic.

**Stock usage [measured]:**

| Map | Its exit's `map` |
| --- | --- |
| base2 | eou1_.cin+*bunk1$start |
| ware2 | eou2_.cin+*jail1 |
| security | eou3_.cin+*mintro$start |
| mine3 | eou4_.cin+*fact1$start |
| fact1 | eou5_.cin+*power1$pstart |
| power2 | eou6_.cin+*biggun$bstart |
| biggun | eou7_.cin+*hangar1$unitstart |
| strike | eou8_.cin+*city1$unitstart |
| city3 | *boss1$bosstart (a new unit, no cinematic) |
| boss2 | end.cin+victory.pcx (the end of the game) |

**Ours [ours]:** The string is split at `$`, a leading `*` is dropped, and `+` is looked for only
after the spawn name. So "eou1_.cin+*bunk1" reaches the map loader whole, as
maps/eou1_.cin+*bunk1.bsp (the `*` inside it is not leading). No such map exists, so the change
fails and the player stays on base2 with no way forward. Eight of the nine unit ends are blocked.
Neither the cinematic nor the end picture is shown, and nothing handles the end of the game.
city3's exit works, apart from the unit reset. **[derived]**

**What the original does at the end [read]:** After the victory picture, a key press goes back to
the console and the main menu. The game is over.

### 2. target_explosion's delay

**Original [read]:** When used, a target_explosion with `delay` waits that many seconds, then
explodes, deals its radius damage (`dmg`, default 0) and fires its targets at the same moment. The
targets are not delayed again. **[measured]** 281 target_explosions on the campaign, 157 with a
`delay`.

**Ours [ours]:** The blast happens at once, and the delay is applied to its targets. Timed chains
of blasts (collapsing ceilings, sequences in the factory and the reactor) go off together.
**[derived]**

### 3. func_clock

**Original [read]:**
- A func_clock writes its reading as text into the target_string its `target` names, once a
  second.
- Its spawnflags pick the mode: TIMER_UP (1) counts up from 0 to `count` (default 3600 s), and
  TIMER_DOWN (2) counts down from `count` to 0. With neither, it shows the time of day.
- `style` picks the text: 0 is two characters (seconds), 1 is minutes and seconds, 2 is hours,
  minutes and seconds.
- With START_OFF (4) it waits for a use. Without it, it starts at spawn.
- When a timer reaches its end, it fires `pathtarget` in the same second the end shows. Then it
  stops. With MULTI_USE (8), a later use resets the count and runs it again. Without it, the clock
  is removed.
- A TIMER_DOWN clock with no `count` is removed at spawn with a console warning.

**Stock usage [measured]:** biggun and fact3, both with flags 14 (TIMER_DOWN, START_OFF,
MULTI_USE), style 0, count 10 and 30, and the pathtarget "boom". So each is a two-digit countdown
that starts when used. **[derived]**

**Ours [ours]:** No func_clock. The countdown never shows on the wall's characters, and its
pathtarget never fires, so whatever "boom" opens or destroys never happens. **[derived]**

### 4. trigger_elevator

**Original [read]:** A trigger_elevator's `target` names a func_train. When used, it reads the
using entity's `pathtarget`, finds the path_corner of that name, and sends the train there,
continuing from that corner. If the train is moving, the use is ignored. If the user has no
pathtarget, or the corner is missing, it prints a warning to the console and does nothing.

**Stock usage [measured]:** lab has 3, mine2 has 1. Twelve buttons on those maps carry a
`pathtarget`: one button per floor.

**Ours [ours]:** Not spawned. The buttons fire nothing that moves the train, so those lifts never
come or go. **[derived]**

### 5. target_laser

**Original [read]:**
- The beam is drawn every frame from the laser to what it hits. Its width is 4, or 16 with FAT
  (64).
- Its colour is set by the flags RED 2, GREEN 4, BLUE 8, YELLOW 16 and ORANGE 32. With no colour
  flag, it is a mixed colour.
- It may aim at its `target` (followed if that moves), or along its angles.
- Damage is `dmg`, default 1, every frame (10 times a second). Damage applies to each monster and
  player along the beam. It passes through them and stops at the first other solid.
- On first turning on, and every time the aim's direction changes, it throws a burst of laser
  sparks in its colour at the hit point. There are 8 sparks the first time, then 4.
- START_ON (1) turns it on 1 s after spawn. A use toggles it.

**Stock usage [measured]:** 150 target_lasers.

**Ours [ours]:**
- No beam is sent to the client: "te_laser" is not in our temp-entity names, and the laser is not
  an entity row that draws one. The beam is invisible.
- Sparks are thrown at the laser's own origin, in a fixed red-orange.
- The beam stops at the first monster it touches.

### 6. Debris from func_explosive and misc_explobox

**Original [read]:**
- **func_explosive:** when it dies, it throws chunks away from the inflictor's side at about 150
  units/s. Big chunks are mass/100, at most 8, debris1. Small chunks are mass/25, at most 16,
  debris2. Mass defaults to 75. The rocket-look explosion is shown only when `dmg` is more than 0.
- **misc_explobox:** 2 debris1 and 4 debris3 at the bottom corners, and 8 debris2. When the barrel
  is on the ground, the grenade-look explosion (and sound) is shown. In the air, the rocket look is
  used.
- Debris is tossed, lives a short time, and can be shot to gib it.

**Ours [ours]:** "te_debris" is not a name our wire knows, so no chunks appear. The barrel always
uses the rocket explosion.

### 7. Item targets

**Original [read]:** The first time anything that can pick items up touches an item, the item's
`target` (and its `message`, through the use of its targets) fires. This happens even when the item
is not taken (for example at full health). After that the target is cleared and never fires again.

**Stock usage [measured]:** 64 items carry a target, for example base1's weapon_shotgun (target
t55) and city3's item_ancient_head (t304).

**Ours [ours]:** Items never fire their targets. Doors, monsters and messages that a pickup sets off
never happen.

### 8. "\n" in messages

**Original [read]:** When a map's entity text is read, every backslash followed by n in a value
becomes a new line. All other text is kept as it is.

**Stock usage [measured]:** 119 values on 37 maps hold "\n". They include help computer messages
(target_help), trigger messages and door messages.

**Ours [ours]:** The two characters are shown as they are, so lines read "...\n..." on screen and in
the help computer.

### 9. worldspawn gravity

**Original [read]:** worldspawn's `gravity` key sets the server's gravity (the sv_gravity setting)
for that map. A map without it sets 800.

**Stock usage [measured]:** space has "gravity" "100". No other stock map has the key.

**Ours [ours]:** The key is ignored. The space map plays at normal gravity, so its jumps and its
feel are wrong.

### 10. Monsters and things with movers

**Original [read]:**
- **Doors:** a door's touch field (60 units out from the door, team-wide) opens the door for a
  monster too, unless the door has NOMONSTER (8). **[measured]** 144 doors have NOMONSTER, many
  others do not.
- **Riding and pushing:** every mover pushes and carries anything not part of the world. That means
  monsters, items, gibs, dead bodies, grenades and players.
- **Crushing:** when a door or a plat is blocked by something that is neither a monster nor a
  player, it deals 100000 damage. If the thing still exists, it is removed with the rocket-look
  explosion.

**Ours [ours]:** Monsters do not open doors and are not carried or pushed. Items, gibs and bodies
are not moved by movers, and nothing is cleared by crushing. Monsters that should follow a player
through a door stop at it. **[derived]**

### 11. trigger_push with angle 0

**Original [read]:** A trigger whose angles are all zero has no direction. Its move direction is
the zero vector, so the push sets the toucher's velocity to zero.

**Stock usage [measured]:** city1 has two trigger_push with angle 0, speeds 150 and 200.

**Ours [ours]:** Angle 0 is taken as +x, and the push is speed × 10. So it throws the toucher along
+x at 1500 or 2000 units/s.

### 12. target_speaker attenuation and volume

**Original [read]:**
- `attenuation` is 1 (normal), 2 (idle), 3 (static) or -1. -1 means no fall-off: heard everywhere
  on the map. A missing or 0 value is 1.
- `volume` is 0 to 1, default 1.
- A looped speaker (LOOPED_ON 1, LOOPED_OFF 2) always plays at static fall-off, full volume.
- RELIABLE (4) makes a one-shot sound reliable over the network.

**Stock usage [measured]:** 149 speakers on 26 maps use -1, 135 set `volume`, 9 set RELIABLE.

**Ours [ours]:** Every speaker plays at normal fall-off and full volume. Announcements meant to be
heard across the map are heard only near the speaker. Quiet ambient effects are too loud.

### 13. target_spawner

**Original [read]:**
- When used, it spawns an entity of the class named by its `target` key, at its own place and
  angles.
- Anything already in that spot is killed first.
- With `speed`, the spawned thing gets a velocity of speed along the spawner's angle.
- Every use spawns another one. There is no limit.

**Stock usage [measured]:** misc_gib_arm, misc_gib_leg and misc_gib_head are spawned on boss2,
fact2, lab and mine2. fact2's func_timers spawn misc_insane again and again.

**Gibs [read]:** A spawned gib lives 30 s, is tossed and spins.

**Ours [ours]:**
- Gibs cannot be spawned.
- Monsters are capped at 4 per spawner.
- Items are thrown upward at 300 units/s instead of along the angle.
- Nothing at the spot is killed.

### 14. Animated textures on movers

**Original [read]:** Brushes can cycle through their texture's frames. A flag picks either frames
0 and 1 or frames 2 and 3, slow or fast.
- **func_button:** always cycles frames 0 to 1. Once pressed, it cycles 2 to 3, until it returns.
- **func_door:** ANIMATED 16, ANIMATED_FAST 64.
- **func_wall:** 8 and 16. **func_rotating:** 64 and 128.
- **func_object:** 2 and 4. **func_explosive:** 2 and 4.

**Ours [ours]:** None of these. Lit buttons and animated door panels stay on their first frame.

---

## Rank 2 details

### 15. func_door_secret

**Original [read]:**
- It opens when used, or when shot if it has no targetname or has ALWAYS_SHOOT (1). Any damage
  opens it.
- It first moves sideways by its width: to the right, or to the left with FIRST_LEFT (2), or down
  with FIRST_DOWN (4). It waits 1 s, then moves back along its angle by its length.
- It holds open for `wait` seconds (default 5, -1 holds forever), then returns the same way with a
  1 s pause.
- Speed 50, fixed. It deals `dmg` (default 2) to a blocker, at most every 0.5 s, and never
  reverses.
- It plays no sounds. It opens its area portals.

**Stock usage [measured]:** city3, flags 3, wait 5, angle 0.

**Ours [ours]:** Not spawned. The brush is drawn but not solid, so the player walks through the
wall, and the secret is never found the intended way.

### 16. Door teams and keys in mixed case

**Original [read]:** When door team members differ in move length, each member's speed is scaled so
all finish at the same moment. **Ours [ours]:** Each member moves at its own speed and finishes on
its own.

**Original [read]:** Entity keys match regardless of case. **Ours [ours]:** Keys are matched exactly.
**[measured]** city2 has a door with "Team" in mixed case, so its door pair does not link in ours.
city1 has a func_timer with "Wait", and city3 has a monster with "Targetname". In ours they get the
default wait and no name, so city3's monster can never be triggered.

### 17. trigger_counter message

**Original [read]:** While counting, it prints "N more to go..." (no "Only"). When it completes, it
prints "Sequence completed!". Both are suppressed by NOMESSAGE (1). **Ours [ours]:** "Only N more to
go...".

### 18. trigger_key message

**Original [read]:** "You need the <item's pickup name>", with no full stop. It shows at most once
every 5 s, with misc/keytry.wav. On success it plays misc/keyuse.wav. **Ours [ours]:** Adds a full
stop.

### 19. Secrets and goals

**Original [read]:** When a target_secret or target_goal is used, its sound (`noise`, default
misc/secret.wav for both) is played at the activator. **Ours [ours]:** talk1 is
played at the activator, and secret.wav at the entity.

**Two built-in fixes in the original [read]:**
- On mine3, the target_secret at 280 -2048 -624 is given the message "You have found a secret
  area.".
- On fact1, a changelevel to "fact3" becomes "fact3$secret1".

**Ours [ours]:** Neither fix. On fact1 the secret exit leads to fact3's default start.

### 20. Mover and help computer sounds

**Original [read]:** Doors, plats, buttons and trains play their start and stop sounds at static
fall-off. The help computer's beep is static too. **Ours [ours]:** Normal fall-off, so these are
heard much farther away.

### 21. target_splash

**Original [read]:** It sends `count` particles (default 32) in the direction of its angle. The
`sounds` key picks the colour: 1 sparks, 2 blue water, 3 brown water, 4 slime, 5 lava, 6 blood.
`dmg` deals radius damage. **[measured]** 185 splashes, 123 with an angle.

**Ours [ours]:** Always 8 particles, upward.

### 22. target_temp_entity style 22

**Original [read]:** Style 22 is the boss teleport effect: a big sparkle and misc/bigtele.wav, heard
everywhere. **[measured]** Used on boss1.

**Ours [ours]:** The name "te_style_22" is sent, but it is not in our temp-entity table, so nothing
appears or sounds.

### 23. Light styles

**Original [read]:** The game sets styles 0 to 11 (normal, flicker, slow pulse, candle, fast strobe,
gentle pulse, flicker 2, candle 2, candle 3, slow strobe, fluorescent flicker, slow pulse not
fading to black) and 63 (testing).

**Ours [ours]:** Only styles 0 to 2 are set. **[measured]** 15 stock lights use styles 4, 6 or 10
(for example on base1). They stay still.

### 24. Music

**Original [read]:** worldspawn's `sounds` sets the CD track for the map. When the last target_goal
is found, the track is set to 0 (stops). **Ours [ours]:** No track is ever set.

### 25. Missing models

**Original [read]:**
- **misc_banner:** models/objects/banner/tris.md2. 16 frames looped, from a random start frame.
  **[measured]** base1 ×2, base3.
- **misc_satellite_dish:** 38 frames, played once when used. **[measured]** base3.
- **light_mine2:** a mine light model. **[measured]** 34 on mine3, mine4, mintro and train.
- **misc_gib_head:** placed on maps as a decoration. **[measured]** base1, base2.

**Ours [ours]:** Not drawn.

### 26. Power cubes with spawnflags

**Original [read]:** Only an item_power_cube keeps its spawnflags. Any other item with spawnflags
has them cleared at spawn, with a console warning. On a power cube, TRIGGER_SPAWN (1) hides it
until its targetname is used, and NO_TOUCH (2) makes it a solid thing that cannot be taken.
**[measured]** bunk1 ×1 and ware2 ×3 power cubes carry flags 3 or 2051 (2048 is the "not in
deathmatch" flag), so each is hidden until used, then appears and cannot be taken.

**Ours [ours]:** Visible from the start and can be taken.

### 27. Screen tints

**Original [read]:**
- Under water: a blue-green tint. Slime: green. Lava: red.
- The power-ups: quad blue, invulnerability yellow, the environment suit green, the rebreather pale
  green. Each blinks during its last 3 s.
- A pickup flashes the screen for 0.25 s.
- Being under water also bends the view (an underwater flag sent to the client).

**Ours [ours]:** Our runner sends no tints and no underwater flag.

### 28. Power-up run-out sounds

**Original [read]:** At 3 s left, a sound plays. Quad: items/damage2.wav. Invulnerability:
items/protect2.wav. Suit and rebreather: items/airout.wav. **Ours [ours]:** None.

### 29. trigger_hurt and trigger_push touch only players in ours

**Original [read]:** trigger_hurt harms anything that can take damage, including monsters and
barrels. trigger_push moves monsters, players and grenades.

### 30. Monsters firing triggers

**Original [read]:** trigger_multiple and trigger_once with MONSTER (1) are fired by monsters.
NOT_PLAYER (2) stops players from firing them. **[measured]** 2 MONSTER and 13 NOT_PLAYER uses.

**Ours [ours]:** Monsters never fire triggers. The 13 NOT_PLAYER triggers can never fire.

### 31. Facing check with angle 0

**Original [read]:** A trigger with all-zero angles has no facing check. **[measured]** boss1 has a
trigger_once and train has a trigger_multiple with angle 0.

**Ours [ours]:** Requires the player to face +x.

### 32. Trains

**Original [read]:**
- A train that stops at a wait -1 corner without TOGGLE (2) cannot be started again. Later uses are
  ignored. **Ours [ours]:** It restarts.
- When several path_corners share a targetname, the train picks one at random. **[measured]**
  Duplicate names exist mostly on monster paths.
- A corner with TELEPORT (1) makes the train jump to the next corner with a teleport effect.

### 33. Intermission

**Original [read]:** It picks one of the first four info_player_intermission spots at random (the
first if fewer exist). The scoreboard (single player: the help computer's unit score) shows, and
after 5 s any button leaves. There is no automatic exit in single player.

**Ours [ours]:** Always the first spot, and leaves by itself after 20 s.

### 34. Teleport sound

**Original [read]:** A player who teleports plays misc/tele1.wav at the pad and at the player, with
particles at both. **Ours [ours]:** Particles only.

---

## Rank 3 details

35. **func_water blocking [read]:** never blocked. **Ours [ours]:** a blocked func_water with
   wait ≥ 0 reverses.
36. **Door teams reversing [read]:** the blocked door's own state decides the whole team's
   reversal. **Ours [ours]:** each member's own state.
37. **Team pushers [read]:** when one part is blocked, all parts stop and all their thinks are
   delayed. Only the master plays sounds. **Ours [ours]:** team trains each play their loop.
38. **Rotating riders [read]:** a rider's view turns by the brush's yaw in degrees applied to a
   unit of 1/65536 of a turn, so it barely turns **[derived]**. **Ours [ours]:** turns fully.
39. **func_rotating dmg [read]:** "dmg" "0" is forced to 2.
40. **Delayed use [read]:** the delayed copy plays talk1, not the trigger's `sounds`. A delayed use
   of a door does toggle its area portals.
41. **Messages from non-players [read]:** talk1 still plays at the entity.
42. **target_lightramp [read]:** falling ramps step one letter apart from ours at their end, by
   rounding.
43. **target_blaster [read]:** its bolt always has the blaster glow, and its kills read as the
   hyperblaster's obituary.
44. **The viper bomb [read]:** a rocket trail, and the grenade-look explosion.
45. **misc_blackhole [read]:** drawn translucent.
46. **Power armour [read]:** at 0 cells it switches off with misc/power2.wav. **Ours [ours]:** stays
   on and resumes when cells return.
47. **Breathing sounds [read]:** the rebreather and the suit play player/u_breath1.wav and
   u_breath2.wav, alternating on a fixed phase of the level clock. Ours runs on its own phase.
48. **trigger_gravity [read]:** affects players too. **[measured]** No stock map uses it.
49. **Changelevel in deathmatch [read]:** without the exit flag (1), a toucher is killed.
50. **Things appearing [read]:** a func_wall or func_explosive that appears kills anything in the
   way, monsters included. **Ours [ours]:** kills players only, and only for func_wall.
51. **Coop power cubes [read]:** each cube is taken once per player.

---

## Checked and found the same

- **Plats [read]:** speed 200, accel and decel 50, lip 8, 3 s at the top, a hold of 1 s, the field.
- **Doors [read]:** speed 100 (doubled in deathmatch), wait 3, lip 8, dmg 2, the 60-unit field, the
  touch message (once per 5 s, talk1), toggles, start open, area portals.
- **Buttons and rotating brushes [read]:** buttons speed 40, wait 3, lip 4. The rotating door axes.
  func_rotating.
- **Timers and triggers [read]:** func_timer. trigger_once and trigger_multiple (wait 0.2,
  TRIGGERED 4, the old flag-1 fix). trigger_relay. trigger_always (0.2 s).
- **trigger_hurt and trigger_push [read]:** hurt dmg 5, flags 1, 2, 4, 8 and 16, its sound every 10
  frames. push speed 1000, ONCE, a 1.5 s wind sound. trigger_monsterjump.
- **Targets [read]:** target_crosslevel (default delay 1, fires at 2 × delay). target_help.
  target_earthquake (count 5, speed 200, world/quake.wav every 0.5 s everywhere, players on the
  ground). target_lightramp (apart from 42). target_string and target_character. func_object.
  func_killbox.
- **Decorations [read]:** misc_deadsoldier, the easter figures, the vipers. Teleporters (+10 up,
  160 ms hold, killing what is there).
- **Level changes within a unit [read]:** no intermission. Health cut to its maximum. The inventory
  carried.
- **Death in single player [read]:** a button after 1 s opens the load menu.
- **Falling [read]:** the fall's measure is the speed change squared × 0.0001. Under 15 is a
  footstep. 15 to 30 is player/land1.wav. Over 30 is fall2. Over 55 is fall1. Damage is
  (measure − 30) / 2.
- **Drowning [read]:** air lasts 12 s. Damage starts at 2 and grows by 2 up to 15. It ignores
  armour.
- **Lava and slime [read]:** lava deals 3 × the depth level (1 with the suit). Slime deals 1 × the
  level. The water sounds.
- **The help computer [read]:** its layout and beeps.
