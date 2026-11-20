# Quake II single player, world round 4: answers

These come from the stock game library (the game folder) and the server folder. Every fact is
marked [read] if the source says it directly, or [derived] if I worked it out. No code is quoted.
Attenuations: none 0, normal 1, idle 2, static 3. One server frame is 0.1 s.

## 1. func_water

- It has no blocked handler. Its spawn function never sets one. [read]
- Spawn: it is a pusher with BSP solidity. Speed defaults to 25, and acceleration and deceleration
  are set equal to the speed, so it moves at a constant speed. Wait defaults to -1, and a wait of
  -1 sets the toggle flag. The move distance is the size along the move direction minus "lip",
  and the default lip is 0 because func_water does not apply the door's lip default of 8. It uses
  the door's use function. Its classname is then renamed to "func_door", so the door movement
  code (which picks a linear move by classname) works for it. [read]
- Sounds: "sounds" 1 (water) and 2 (lava) are identical: the start sound is world/mov_watr.wav and
  the end sound is world/stp_watr.wav. No middle (looping) sound is ever set. Any other value means
  no sound. Like a door, the sounds play only on a non-slave team member, on the voice channel with
  the no-PHS bit, volume 1, attenuation static 3. [read]
- A blocked frame works like this for every pusher. The whole team's moves made in that frame are
  undone. Every team member's next think time that is above 0 is pushed back by 0.1 s. Then the
  blocked part's blocked handler is called if it has one. func_water has none, so it simply stays
  where it was this frame. Its velocity is unchanged, so it tries the same push again next frame:
  it retries every frame until the obstacle moves, and its arrival think slides 0.1 s later for
  each blocked frame. [read for the mechanism; derived for func_water]
- It never deals damage. It has no blocked handler, no touch handler and no dmg is used. [read/derived]

## 2. A blocked door team

- The door that was blocked (the part named in the push) gets its blocked handler called. That
  door's own state decides what happens. [read]
- Order inside the handler [read]:
  1. If the obstacle is neither a monster nor a client (a gib, an item, a corpse that is not a
     monster and so on), it takes 100000 damage (damage flag 1 = radius/indirect, MOD_CRUSH). Then
     it is ALWAYS turned into an explosion (temp entity explosion1, then the entity is freed),
     because the "is it still there" test checks a pointer that can never be empty. That is a
     quirk: even an entity that freed itself while dying is "exploded" again. The handler then
     returns, so the door does not reverse for such objects.
  2. Otherwise (a monster or a player), the obstacle takes the door's dmg first (default 2; damage
     flag 1, MOD_CRUSH).
  3. If the door has the crusher flag, it stops there and does not reverse. It keeps pressing and
     dealing dmg each blocked frame.
  4. If the door's wait is negative (-1), it does not reverse either. The source comment says that
     such a door would otherwise never come back, so it just crushes.
  5. Otherwise, if the blocked door's state is "going down", every member of its team (starting at
     the team master) is sent up, each with its own activator. In every other state (going up,
     and also top or bottom) every member is sent down.
- Sending a member up does nothing if it is already going up. If it is at the top, it only resets
  its top wait. Otherwise it restarts the move toward the open position, plays the start sound
  (master only), fires its targets again and opens its area portals again. Sending down has no
  guard: the move to the closed position is restarted on every member. [read]
- Damage is dealt on every blocked frame (no debounce for doors), so it is dmg every 0.1 s. A
  reversing door usually stops being blocked after the first frame. [derived]

## 3. Team pushers in a blocked frame; who plays sounds

- Physics runs only for the team captain. A slave returns at once. The captain moves each moving
  part in chain order, accumulating every moved entity (the parts and the things they pushed) in one
  list. When a part's push fails, every entry in that list is restored, newest first. That includes
  the parts earlier in the chain that had already moved successfully this frame, and everything they
  carried. So the whole team is undone and stays put this frame; the parts after the blocked one are
  not tried. [read]
- Then every team member's next think, if it is above 0, is delayed by exactly 0.1 s (one frame).
  Only the blocked part's blocked handler is called, not the others'. When the frame succeeds, the
  captain runs the think of every member in chain order. On a blocked frame no member thinks. [read]
- A player's yaw delta (see 4) is restored too, as part of the undo. [read]
- Doors: the start sound, the looping middle sound (set as the entity's loop sound) and the end
  sound are played or set only by a member without the team-slave flag, which is the master. Slaves
  are silent, and their loop sound is never set. Channel: voice with the no-PHS bit; volume 1;
  attenuation static 3. The default door sounds (when "sounds" is not 1) are doors/dr1_strt.wav,
  doors/dr1_mid.wav (loop) and doors/dr1_end.wav. [read]
- Trains: the same rule. Only a non-slave plays the start sound and sets the loop sound when it
  leaves for the next corner, and plays the end sound and clears the loop when it stops at a corner
  with a wait. In the stock spawn function a train sets only the loop sound, from "noise", and never
  a start or end sound, so in practice a train has a loop sound and nothing else. [read]
- A train's blocked handler [read]: an obstacle that is neither a monster nor a client takes 100000
  damage and is then always turned into an explosion (the same quirk as the door). For a monster or
  a player: nothing happens if the 0.5 s debounce has not run out, or if dmg is 0; otherwise it
  takes dmg (flag 1, MOD_CRUSH) and the debounce is set to now + 0.5 s. The train never reverses.
  The default dmg is 100. The BLOCK_STOPS flag (4) forces dmg to 0, so the train just waits.

## 4. Riding a rotating brush: the yaw delta

- For every entity that is moved by a pusher (a rider, or one standing in its way), if it is a
  client, the pusher's yaw change for this frame is added straight onto the player's yaw
  delta-angle field. That field is a signed 16-bit integer in the protocol's angle units, where
  65536 units are 360 degrees (one unit is about 0.0055 degrees). [read]
- The value added is the brush's yaw change in DEGREES for that frame: its yaw angular velocity
  times 0.1. It is not converted into angle units. This is a bug: the player turns about 1/182 of
  what the brush turns. A brush spinning at 90 degrees a second adds 9 units a frame, about 0.05
  degrees. [read for the add; derived for the effect]
- Rounding: the sum (the old integer value plus the fractional degrees) is converted back to the
  16-bit integer by C's float-to-integer conversion, which truncates toward zero. A fractional step
  such as 0.5 is therefore lost when the old value is positive, and adds up to a whole unit toward
  zero when it is negative (for example -100 + 0.5 = -99.5, which becomes -99). The angle move
  itself is not quantized; only the linear move is snapped to 1/8 unit. [read for the
  types; derived for the arithmetic]
- On a blocked frame the saved delta is restored. [read]
- The rider's position IS swung round the pusher's origin by the pusher's angle change, for every
  moved entity. [read]
- A monster's angles are not changed at all. Only its position is swung round. The source notes
  this with a comment saying "doesn't rotate monsters?". [read]

## 5. func_rotating

- dmg defaults to 2. A dmg of 0 (or no dmg) is replaced by 2 at spawn, so a rotating brush can never
  have 0 damage. [read]
- The blocked handler is attached whenever dmg is nonzero, which after the default is always. No
  spawn flag is needed to hurt when blocked. [read/derived]
- Blocked damage: dmg per blocked frame (2 by default, so 20 per second), damage flag 1, MOD_CRUSH,
  with no debounce. [read/derived]
- Spawn flag 16 (TOUCH_PAIN) adds a touch handler while it spins. Each touch deals dmg (flag 1,
  MOD_CRUSH) when any angular velocity is nonzero. [read]
- Other flags: 1 starts it on; 2 reverses the direction; 4 and 8 pick the other two axes (the
  default is yaw); 32 makes it a "stop" pusher, which carries riders but is blocked by anything else
  in its way instead of pushing it; 64 and 128 animate its textures (all, or all fast). The speed
  defaults to 100 degrees per second. [read]
- It never reverses on a block and never stops by itself. On a blocked frame it stays still and
  retries next frame. Using it toggles it between full speed and stopped instantly (no
  acceleration); the touch handler is cleared when it stops. Its loop sound is the middle sound,
  which is never set (the line that would set it is commented out), so it is silent. [read]

## 6. A delayed use

- With "delay" set, the target-firing routine does nothing else at once. It spawns a temporary
  entity named "DelayedUse" that thinks after delay seconds. That entity gets copies of only the
  activator, the message, target and killtarget. [read]
- The noise sound (the field a trigger's "sounds" fills, for example misc/secret.wav,
  misc/talk.wav or misc/trigger1.wav) is NOT copied. So when the delayed copy fires and there is a
  message, it always plays misc/talk1.wav (at the activator, auto channel, volume 1, attenuation
  normal 1), never the trigger's chosen sound. The message is printed only at the delayed time,
  not at once. [read/derived]
- Area portals: when the firing entity's classname is func_door or func_door_rotating, the normal
  firing skips targets that are func_areaportal, because doors set their portals themselves. The
  delayed copy's classname is "DelayedUse", so that skip does NOT apply: it calls the area portal's
  use, which TOGGLES the portal's own open/closed bit. [read]
- Meanwhile the door still opens its portals at once when it starts opening, and closes them when it
  reaches the bottom. It does this directly, without touching the portal's toggle bit. So a delayed
  door that targets a portal opens it at once, and then the delayed toggle moves the portal's
  private bit (starting at closed, 0) and sets the portal to that new state. On the first opening
  the toggle goes to open (no visible change). On the next opening it goes to closed while the
  door is open, and so on, alternating. This is a quirk/bug. The door only fires targets when it
  starts going up, never going down. [read for the pieces; derived for the alternation]

## 7. A message when the activator is not a player

- The message is printed and its sound played only if the activator does not have the monster flag.
  If a monster is the activator, nothing is printed AND no sound plays: talk1 is not played
  anywhere. [read]
- When it does play (any activator that is not a monster), the sound is played at the activator,
  on the auto channel, volume 1, attenuation normal 1. It is the firing entity's noise sound if it
  has one, else misc/talk1.wav. [read]
- The text is a centre print to the activator. The server ignores a centre print to an entity that
  is not a client, so a non-monster non-client activator gets the sound but no text. [read]
- An empty activator would crash the check, because nothing tests for it. The delayed path only
  prints a developer warning, "Think_Delay with no activator". [read]
- The door's own touch message (a door with a message and no trigger) is separate: it prints to the
  toucher and plays misc/talk1.wav on the auto channel at attenuation normal 1, with a 5 s
  debounce. [read]

## 8. target_lightramp

- Spawn: the "message" must be exactly two lower-case letters a to z, and the two letters must
  differ. Otherwise it prints an error and is removed. It is removed in deathmatch, and removed if
  it has no target. [read]
- Stored values [read]:
  - start index S is the first letter minus 'a' (0 to 25);
  - end index E is the second letter minus 'a';
  - step D is (E - S) divided by (speed / 0.1), a float in letters per frame.
- There is no default for "speed". With speed 0 the step is a division by zero (infinite), and the
  first letter becomes undefined (0 times infinity is NaN). That is a bug for mappers who leave it
  unset. [read for the lack of a default; derived for the result]
- On use: the first time, it searches its targets and keeps the LAST entity named "light" (others
  get a warning); only that one light is ramped. It records the start time T0 and runs the step at
  once, in the same frame as the use. [read]
- Each step: the letter shown is 'a' + S + ((now - T0) / 0.1) * D. The fractional result is
  converted to a character by truncation toward zero, which is floor here because the value is
  positive. The step sets that light's style string to that one letter. [read; floor derived]
- Frame by frame: at elapsed 0 the start letter is shown; at elapsed k*0.1 the letter is floor of
  S + k*D. If the elapsed time is still below speed it thinks again 0.1 s later. Otherwise it stops.
  The last step is the one where elapsed is at least speed, which nominally shows S + (E-S), the end
  letter. [read/derived]
- Rounding quirks [derived]:
  - Because of floor, a rising ramp shows each letter only once the exact value reaches it. A
    falling ramp shows the lower letter as soon as the value drops below the integer, so it is one
    letter "ahead" through the ramp. For example z to a over 1 s (step -2.5): frames give 25, 22.5,
    20, 17.5 and so on, shown as z, w, u, r and so on. a to z over 1 s (step 2.5) gives a, c, f, h
    and so on.
  - The elapsed time is a float difference divided by 0.1, so it can come out a hair under k. At
    the last step that can (a) show one letter short of the end (floor of 24.9999 is y, not z), or
    (b) make the "elapsed is below speed" test pass once more. In that case one extra step runs and
    overshoots past the end letter by one step. There is no clamp.
- At the end: without the toggle flag nothing more happens. Another use replays the same ramp from
  the start letter. With the toggle flag (1), S and E are swapped and D is negated, so the next use
  ramps back the other way. [read]
- A use in the middle of a ramp restarts it from the current S (resets T0). The swap happens only
  when a ramp reaches its end. [read/derived]

## 9. target_blaster

- Defaults: dmg 15, speed 1000. [read]
- The bolt: model models/objects/laser/tris.md2. It carries the loop sound misc/lasfly.wav, has a
  zero-size box, uses fly-missile movement, and is removed after 2 s if it hits nothing. On hitting
  something that cannot take damage it sends the blaster temp entity. Sky surfaces just remove it.
  [read]
- BUG 1, effects: the use function works out an effect from the flags (NOEFFECTS 2 gives none,
  NOTRAIL 1 gives the hyperblaster effect, otherwise the blaster effect). It then ignores that
  result and always passes the BLASTER effect. So both flags do nothing: every bolt has the blaster
  glow. [read]
- BUG 2, means of death: the bolt-firing routine's last argument is a yes/no "hyperblaster" switch.
  The target passes its means-of-death number (MOD_TARGET_BLASTER, 33) there, which counts as yes.
  So every target_blaster bolt is marked as a hyperblaster bolt, and its damage uses MOD_HYPERBLASTER
  (10), with the energy damage flag. MOD_TARGET_BLASTER is never used, so the obituary "got blasted"
  never appears. Because the attacker is the target (not a client), a player killed in deathmatch
  or coop is reported as "<name> died."; single player prints nothing special. [read/derived]
- Sound: weapons/laser2.wav, played at the target on the voice channel, volume 1, attenuation
  normal 1, on every use. [read]
- The target is invisible to clients (no-client flag). It fires from its own origin along its
  "angle"/"angles" direction. [read]

## 10. misc_viper_bomb

- Model models/objects/bomb/tris.md2. Box -8,-8,-8 to 8,8,8. It is hidden and not solid until used.
  [read]
- dmg defaults to 1000. [read]
- On use: it becomes a bounding-box solid and visible, gets the ROCKET effect (a rocket trail and
  glow), and becomes a toss mover (gravity). It takes the velocity of the first misc_viper found:
  that viper's direction times its speed. It remembers the use time and the activator, and loses
  its use function. [read]
- Each frame (prethink): its ground entity is cleared. Its facing is set from a vector, the viper's
  direction scaled by (1 + d), with height d, where d is the time since release as a negative
  number, clamped at -1. So it noses down over its first second. Its roll grows by 10 degrees a
  frame. [read]
- On touching anything: it fires its targets (with the stored activator), lifts its origin to its
  box's bottom + 1, and deals radius damage. The damage is dmg (1000) and the radius is dmg + 40
  (1040). The bomb is both inflictor and attacker, nothing is ignored, and the means is MOD_BOMB.
  Then it becomes the explosion2 temp entity and is freed. Explosion2 is the GRENADE-style
  explosion (explosion1 is the rocket-style one). [read for the code; grenade/rocket look from the
  temp entity's use elsewhere, derived]
- Sound: the game plays none itself. The only sound is whatever the client plays for the explosion
  temp entity. There is no fly or loop sound. [read]

## 11. misc_blackhole

- Render flag TRANSLUCENT. Model models/objects/black/tris.md2. Not solid, does not move. Box
  -64,-64,0 to 64,64,8. [read]
- Frames: it starts at frame 0. Its first think is 0.2 s after spawn. Each think adds 1 to the
  frame; when the frame reaches 19 it is set back to 0. So it loops frames 0 to 18 (19 frames),
  one frame every 0.1 s (frame 18 is followed by 0). [read/derived]
- No effects flags. Using it removes it (the teleport temp entity that would have been sent is
  commented out). [read]

## 12. Power armour at 0 cells

- Players: each server frame, when the HUD stats are built at the end of the frame (the stats
  function, also run in intermission), the check runs. It applies if the player has power armour
  switched on and holds a power shield or power screen (the shield wins if both are held), AND
  their cell count is exactly 0. Then [read]:
  - the power-armour flag is cleared (switched off);
  - misc/power2.wav plays at the player on the item channel, volume 1, attenuation normal 1.
- It does NOT come back on by itself when cells are picked up again. The player must use the item
  again. Using it with 0 cells prints "No cells for power armor." and stays off; with cells it plays
  misc/power1.wav (auto channel, attenuation 1). A manual switch-off plays misc/power2.wav on the
  auto channel. [read]
- In the damage code, armour with 0 power simply absorbs nothing (no message). [read]
- Monsters: their power just reaches 0 and stops absorbing. No switch-off and no sound. [read]
- A small related fact: in deathmatch, picking up the first power armour item switches it on
  automatically. [read]

## 13. Rebreather and environment suit breathing

- Applies only while the player's water level is 3 (head under). Either item active refills the
  air timer to now + 10 s every frame. [read]
- Timing rule: a breath sound plays on a frame when (rebreather end frame - current level frame)
  is divisible by 25, using C's remainder, so 0 also counts for negative differences. The
  rebreather's end frame is used even when only the suit is worn. [read]
  - With the rebreather: a breath every 25 frames (2.5 s), counted from when it runs out (each
    pickup adds 300 frames, 30 s). [read/derived]
  - With only the suit (rebreather end frame 0 or long past): the difference is a negative number,
    and it is divisible by 25 when the absolute level frame number is a multiple of 25. Still every
    2.5 s, but locked to the level's frame counter. This is a quirk of using the wrong timer.
    [derived]
- Files alternate: player/u_breath1.wav when the player's alternate bit is 0, then
  player/u_breath2.wav, flipping the bit after each breath. The bit is per client and is not reset
  when a new item is picked up. Channel auto, volume 1, attenuation normal 1. Each breath also makes
  a "self" player noise that monsters can hear. [read]
- No bubble effect: there is only a "release a bubble?" to-do comment. [read]
- In noclip none of this runs. [read]

## 14. trigger_gravity

- The touch handler sets the touching entity's personal gravity field to the trigger's "gravity"
  value. It does not care what the toucher is. [read]
- The value is read from the spawn key with an INTEGER parse (atoi). So "0.5" becomes 0 (no
  gravity), "1.5" becomes 1, and so on. Only whole numbers work. This is a bug. The trigger is
  removed if the key is missing. [read]
- Who touches it: anything that runs the touch-trigger check. That is players (each move,
  unless noclip), monsters (step and walk moves), toss and bounce movers such as items in flight,
  gibs and grenades (each push), and anything carried by a pusher. Dead players and dead monsters
  do not touch triggers. [read]
- Who it affects: the field is used by the gravity routine for non-player physics (toss, bounce,
  fly-missile and the step physics of monsters in the air). Player movement takes its gravity from
  the server cvar alone, so a player's field is set but has NO effect on the player's own movement.
  [read/derived]
- Nothing restores it: no code resets the field when the toucher leaves. An entity keeps the new
  gravity for good. A player's field is only reset to 1.0 when their entity is re-initialised as
  they enter a level. [read/derived]

## 15. target_changelevel in deathmatch

- Order of tests [read]:
  1. If intermission has already started, do nothing.
  2. In single player (not deathmatch, not coop), do nothing if player 1 is dead.
  3. In deathmatch, if the "allow exit" dmflag (value 4096, bit 0x1000) is NOT set, and the "other"
     (the entity that called use, normally the trigger) is not the world, then "other" takes damage
     10 x its max_health, with knockback 1000, no damage flags, MEANS MOD_EXIT, and the use returns.
     The level does NOT change.
- Quirk: the damage goes to "other", the entity that fired the use (usually the trigger_multiple
  touched), not to the activator (the player). A trigger has max_health 0 and cannot take damage,
  so in the stock flow nobody is hurt. The level simply does not change. A player only dies if they
  call it directly as "other" (for example a trigger that passes the player). The obituary for
  MOD_EXIT is "found a way out". [read for the call; derived for the effect]
- With "allow exit" set: in deathmatch it announces "<name> exited the level." when the activator
  is a client, clears the cross-level trigger flags if the map name contains "*", and begins
  intermission. [read]

## 16. func_wall and func_explosive appearing (TRIGGER_SPAWN)

- Order: on the use that makes it appear, it first becomes BSP-solid and visible, then runs the
  kill-box, and only then is it relinked. For the wall the kill-box is between the visibility change
  and the link; for the explosive, its use is cleared first. [read]
- Kill-box: it traces the entity's bounding box (for a brush model this is the brush's whole
  bounding box, not its exact shape) as a zero-length move with the player-solid mask, ignoring
  nothing. Whatever is hit takes 100000 damage, with the "no protection" flag (god mode, armour and
  invulnerability do not help) and MEANS MOD_TELEFRAG. If the hit entity is still solid afterwards,
  the kill-box stops; otherwise it traces again. [read]
- What is hit: solid entities, which are living players and living monsters, and other brush
  entities. Dead monsters and dead players are flagged as dead and are skipped by this mask, so
  corpses are not hit. Items are triggers, not solid, so they are not hit. [read/derived]
- Quirk: the trace always reports the world as the hit entity when it hits nothing else, and the
  world counts as solid. So after killing at most the things that gib (and turn non-solid), it
  always ends on the world and reports failure (the result is ignored). [read: the server trace
  always sets the world as the hit entity] Worse, if the brush's bounding box overlaps any world
  brush, the world test returns at once (blocked at fraction 0) and NOTHING is killed. A boss that
  dies without gibbing stays solid and stops the loop, sparing anyone else in the box.
  [derived]
- func_wall: the TOGGLE flag (2) keeps its use, so it can be switched off (not solid, hidden) and on
  again, killing each time it appears. Without toggle it loses its use after one appearance. START_ON
  (4) without TOGGLE is fixed up to include TOGGLE, with a warning. [read]

## 17. Coop power cubes

- At spawn, in coop only, each key_power_cube gets one bit set in spawn-flag bits 8 to 15. The bit is
  8 + the count of cubes spawned so far on this level (the counter starts at 0 each level). [read]
- Pickup in coop: if the player's persistent cube mask already has any bit of this cube's
  (spawn flags bits 8 to 15, shifted down by 8), the pickup is refused. Otherwise the player's
  cube count goes up by 1 and that bit is added to their mask. [read]
- After one player takes it, the cube stays in the world for the others, because cubes are
  "stay in coop" items and are freed only when not coop or when dropped. It fires its targets only
  for the first taker (a "targets used" flag). [read]
- Quirks [derived]:
  - A ninth or later cube on one level gets a bit at position 16 or above. That bit is masked
    away, so its mask is 0: every player can take it again and again with no limit.
  - The mask is persistent across levels and the counter restarts each level. So a cube carried
    over from the last level blocks the cube with the same number on the new level.
- Using cubes at a trigger_key in coop: it finds the lowest bit in the activator's mask (0 to 7;
  if none, it uses 8). Then for every player holding that bit, it removes one cube from their
  inventory and clears the bit. Non-cube keys are instead zeroed in every player's inventory.
  [read]

## 18. The Makron's torso

- Spawned only on a normal death (not a gib). Its origin is the body's origin with 84 subtracted
  from the WORLD Y coordinate, not relative to his facing (a quirk: the offset is always along world
  -Y). It copies his angles. [read]
- Model models/monsters/boss3/rider/tris.md2. It starts on frame 346. Its first think is 0.2 s
  later. Each think adds 1 to the frame, and when it reaches 365 it sets it back to 346. So it loops
  frames 346 to 364 (19 frames) at one frame every 0.1 s, forever. [read/derived]
- Sound: makron/spine.wav as the entity's loop sound: a continuous loop, not repeated plays. No
  channel is involved. Clients play entity loop sounds at static attenuation (3) in the stock
  client; the game sets no attenuation itself. [read for the loop; static attenuation derived from
  the engine's loop-sound handling]
- Box -8,-8,0 to 8,8,8. Not solid. Takes no damage (the damage flag is never set), so it cannot be
  shot or gibbed. Movetype none: it does not move and is not thrown. It is never removed, so it
  lasts for the rest of the level. [read]
- For completeness, the body: it plays its death sound on the voice channel at attenuation none (0)
  and stays damageable (so it can still be gibbed). [read]

## 19. The floater (technician): stand runs

- stand1: frames 144 to 195 (52 frames). stand2: frames 196 to 247 (52 frames). Both loop
  themselves (no end function), every frame doing the stand AI with no movement and no per-frame
  callbacks, so there is no sound inside them. [read]
- The choice: whenever its stand function is called, it picks stand1 if a random number is at most
  0.5, else stand2. The random number is (rand AND 0x7fff) / 32767. So stand1 has a 16385/32768
  chance (just over half). It picks again each time the monster is told to stand, not each loop.
  [read/derived]
- The spawn function also picks one the same way, but the start routine calls stand again at start,
  so the effective pick is the one made at start. When its run is called while it holds its ground,
  it always uses stand1. Its walk and run also use frames 144 to 195 (stand1's range). [read]
- Sounds: its loop sound is floater/fltsrch1.wav, all the time. The idle sound
  floater/fltidle1.wav (voice channel, attenuation idle 2) comes from the shared stand AI's idle
  timer, not from the frames, and not for monsters with the ambush flag. The first idle is set up at
  a random 0 to 15 s; after that, each idle plays and sets the next one 15 to 30 s later. [read]
