# Quake II spectators and the chase camera (clean-room facts)

**A WORKING NOTE** (README.md's rules). Clean-room facts about Quake II's deathmatch game
library: switching to and from spectating mid-match, spectators at connect, and the chase
camera (controls, targets, placement, stats 16 and 17 and the status bar). Where the facts come
from: a reader session (2026-11-22, the `clean-room` skill) that read Q2PRO master (commit
601a8df) and Yamagi Quake II master (commit a1d7e2b), both GPL-2.0, and wrote only facts in its
own words, each marked read or derived; nothing was measured by running a game. Where Yamagi
is not named it behaves as Q2PRO. Units: game units, seconds, server frames of 0.1 s (10 Hz).

## Spectating mid-match

### 1. Which values of `spectator` count as "on"

- The wish to spectate is set whenever the player's user info is (re)read: deathmatch is
  non-zero, the `spectator` value is non-empty, and it is not exactly the one-character text
  "0". Otherwise the wish is off. (read)
- So "1", "yes", "00", " 0", "false" or any password all count as on; only an empty value or
  exactly "0" is off. The test is on the text, not on a number. (derived from the rule above)
- The value doubles as the spectator password: when the server's `spectator_password` is set
  (non-empty and not exactly "none"), the value must equal it, case-sensitively, character for
  character (see 3). So with a spectator password the player types `spectator <password>`, and
  `spectator 1` is refused unless the password is "1". (read)
- A password of "0" can never be given, since "0" means off. (derived)
- The wish is re-read whenever the user info changes and also every time the player is put into
  the world (each spawn re-reads the stored user info). (read)

### 2. When the change is applied, and what holds it back

- At the start of every server frame (every 0.1 s), before any other entity runs, for each
  client in deathmatch: if the wish differs from the player's current state, and at least 5.0 s
  (50 frames) have passed since the player's respawn mark, the switch is attempted. (read)
- The respawn mark (read in both, Q2PRO counts frames, Yamagi seconds, same durations) is kept as follows.
- The mark is set to "now" when a dead player respawns, and when a spectator switch succeeds.
- The mark is set to death time plus 1.0 s when the player dies.
- The mark is cleared to the level's zero by every spawn into the world (the whole client record is wiped
  except the persistent and per-life records); the respawn and switch paths then set it to now
  but entering the level at connect or at a level change leaves it at zero. (read)
- Hence: a player who has just connected or whose level just started can switch once the level
  is 5.0 s old; after a respawn or a switch he waits 5.0 s, and a dead player waits 6.0 s from the
  moment of death. (derived from the three settings above)
- Intermission holds it back: during intermission the per-client start-of-frame step is skipped
  entirely, so no switch happens until the next level. (read)
- Being dead does not hold it back: the switch test runs before the dead-player handling, so a
  dead player who still lies dead 6.0 s after death becomes a spectator; no corpse is copied to
  the body queue on this path, so his body simply disappears (the entity is moved to a spawn
  point as a spectator). (read; the disappearance derived)
- Not applied in co-op or single player (the test requires deathmatch). (read)
- Edge case, bypass by respawn: the wish is stored the moment the user info changes. A dead
  player who sets `spectator` and then presses attack to respawn before the 6.0 s is spawned
  straight into spectator form by the ordinary respawn: no password or limit check, no message,
  score kept, and a body is left on the floor as for any respawn. The same holds the other way
  at a level change: whoever's wish differs from his state is put in by the wish without checks
  or message (a spectator who cleared `spectator` enters as a player without the `password`
  check). (derived: the spawn routine decides spectator or player from the wish alone; only the
  switch path checks)

### 3. Going to spectator: checks, order, refusals

Order (read):

1. The spectator password: if the server's `spectator_password` is non-empty and not exactly
   "none" (case-sensitive), the player's `spectator` user info value must equal it exactly.
2. The limit: count every client slot whose entity is in use and whose wish to spectate is on.
   The player himself is counted, since his wish is already on. If the count is at least
   `maxspectators`, refuse.

- `maxspectators`: default 4, a server-info cvar, changeable at any time. (read)
- Because the player counts himself, and because players whose wish is on but not yet applied
  also count, a mid-match switch succeeds only while fewer than `maxspectators` minus one other
  slots have the wish on: with the default 4, at most 3 others. At connect the newcomer is
  normally not counted (see 6), so connecting allows one more. (derived from the count)
- `needpass` (server info, read): re-computed whenever `password` or `spectator_password`
  changes: 1 when a password is set, plus 2 when a spectator password is set ("none" in any
  case counts as unset here, case-insensitively; the checks themselves treat only lower-case
  "none" as unset, so "NONE" is a real password that `needpass` does not advertise). (read; the
  mismatch derived)

Refusal (read, the same in both):

- Wrong spectator password: the player alone gets `Spectator password incorrect.` with a line
  end, at the high print level (the level ordinary game messages use, PRINT_HIGH, value 2).
- Limit full: the player alone gets `Server spectator limit is full.` with NO line end, at the
  high print level, so whatever prints next continues on the same console line. (read; the
  run-on derived)
- In both cases the wish is set back to off at once, and a command `spectator 0` followed by a
  line end is stuffed into the player's own console, sent reliably to him alone; his client runs
  it, which sets his `spectator` user info back to "0" and sends it.
- He stays in play exactly as he was: no respawn, no score change, no effect, and the respawn
  mark is not touched, so a new attempt is tested again on the very next frame after he sets the
  value again. (read; the next-frame retest derived)

### 4. Coming back to play

- Checked: the server's `password`. If it is non-empty and not exactly "none" (case-sensitive),
  the player's user info key `password` must equal it exactly. (read)
- No limit is checked on the way back (no count of players against anything). (read)
- On failure: the player alone gets `Password incorrect.` with a line end, at the high print
  level; the wish is set back to on, and `spectator 1` followed by a line end is stuffed into his
  console, reliably to him alone. He stays a spectator. (read)
- The stuffed value is "1", which would not match a spectator password; nothing re-checks it,
  since he is already a spectator. (derived)

### 5. On success, either way

Read in both unless marked.

- Score: both the per-life score and the persistent score are set to 0, in both directions.
- The player is put into the world afresh at a spawn point chosen by the ordinary deathmatch
  spawn selection (the same as any respawn), facing the spot's yaw with pitch and roll 0. No
  corpse is left at the old place (this path never copies to the body queue).
- Deathmatch wipes the persistent record on every spawn: health 100, the starting weapon and
  inventory, as for any deathmatch respawn. The per-life record (enter time and the rest) is
  kept except the score set to 0 above.
- Everything else in the client record is cleared by the spawn (showing scores, inventory,
  help, the chase target, damage and view kicks and the like), then the respawn mark is set to
  now.
- To spectator: free flight (no-clip movement; the movement type sent is "spectator", unless
  chasing), not solid, not sent to other clients (invisible to everyone), no gun model, chase
  target cleared. No effect is sent, no teleport hold. (read)
- To player: the ordinary spawn (solid box, walking movement, view height 22), anything already
  standing in the spawn spot is telefragged, the weapon is raised; the login muzzle flash (muzzle
  flash kind 9, a protocol number) is sent to everyone whose view can see the new position
  (potentially visible set of that point); he is held in place by the teleport-time flag for
  112 ms (Q2PRO: 14 units of 8 ms on the classic protocol, 112 ms on its extended protocol,
  Yamagi: 14 units of 8 ms). The logout flash (kind 10) is not used on either path. (read; ms
  derived from the protocol's 8 ms unit)
- Message, broadcast to every client at the high print level, each with a line end:
  `<name> has moved to the sidelines` when he became a spectator, `<name> joined the game` when
  he came back. `<name>` is his `name` user info as stored (up to 15 characters plus the end in
  the persistent record, read).
- Nothing about chasers is reset: other spectators who were chasing him and he becomes a
  spectator move on at his next command (see 9).

### 6. At connect with `spectator` set

- Same decision of "on" (deathmatch, non-empty, not exactly "0"), then the same checks in the
  same order: spectator password first, then the limit. (read)
- The count at connect covers every in-use slot whose wish is on; the connecting player's slot
  is treated by the same step as not yet in use on a fresh connect, so he is normally not
  counted, unlike the mid-match switch. (derived; the game library alone does not settle
  whether the server marks the slot in use before asking)
- A connecting spectator is not asked for `password` at all; only a connecting non-spectator is
  (and a ban check comes before both, refusing with `Banned.`). So with `password` set and no
  spectator password, anyone may enter as a spectator, and needs the password only to switch to
  play. (read; consequence derived)
- Refusals are connect refusals, not console prints in the game: the refusal text is
  `Spectator password required or incorrect.`, `Server spectator limit is full.` or
  `Password required or incorrect.`; the server sends the connecting client an out-of-band
  print of that text, a line end, then `Connection refused.` and a line end (read in both
  servers). No `spectator 0` is stuffed.
- On success the player begins as a spectator directly; the broadcast to everyone at the high
  print level is the ordinary `<name> entered the game` with a line end (never "moved to the
  sidelines"). (read in both)
- The entry effect differs. Q2PRO: a spectator entering gets no login flash and no hold; a
  player entering gets the login flash (kind 9, to the potentially visible set of his spawn
  point) and a 200 ms teleport hold. Yamagi: everyone entering a deathmatch level, spectator or
  not, gets the login flash at his spawn point, and no hold is set on this path. (read)
- The same entry happens at every level change in deathmatch (each client begins the level
  afresh, score cleared, the same message). (read)

### 7. Co-op and single player

- The key is ignored: the wish is forced off whenever the user info is read, the switch test
  needs deathmatch, and at connect the non-spectator branch runs, so `password` is checked as
  for anyone. Nothing resets the user info value (no stuffed command, no message). (read)

## The chase camera

### 8. Controls for a spectator

All read in both, unless marked. They act only while the player is a spectator in effect (not
merely wishing), and not during intermission (the command handling stops early then).

- Attack: acts on the press only. Each user command latches buttons newly down since the
  previous command; when attack is latched, the latch is cleared and: if chasing, the chase
  stops (free flight resumes and client prediction is turned back on); if not chasing, a chase
  starts on the first eligible player (see 9) or `No other players to chase.` is printed (12).
  Holding attack does nothing more.
- Jump (any upward move of at least 10, so jump or move-up): acts on the press only, tracked by
  the movement's jump-held flag, cleared when the upward move drops below 10. If chasing, step to
  the next player; if not chasing, start a chase as attack does (first eligible player, or the
  message).
- `invnext`, `invnextw`, `invnextp`: while chasing, step to the next player; `invprev`,
  `invprevw`, `invprevp`: step to the previous one. While not chasing they do their ordinary item
  selection.
- Nothing else steps the chase: `weapnext`, `weapprev`, `use`, crouch and the rest behave as for
  any client (or do nothing useful).
- Edge case, Q2PRO only: `invuse` and `invdrop`, and using or dropping an item that leaves its
  count lower, re-validate the selected item; when the selected item is no longer held, that
  re-validation goes through the same "next item" step and so, while chasing, steps the chase to
  the next player instead. Yamagi keeps the two apart, so only the six commands above step the
  chase. (read; consequence derived)
- `kill`: Yamagi refuses it for spectators (and within 5 s of the respawn mark). Q2PRO refuses
  it only within 5 s of the mark, so a spectator can kill himself; he then lies dead like a
  player and his next attack respawns him (still a spectator, by his wish) and leaves a body.
  (read; the aftermath derived)
- Stepping does not move the camera at once: the new place is computed when the new target next
  sends a command (see 10). Starting a chase computes it at once. (read)
- These run once per user command received (each client movement command), not once per
  server frame. (read)

### 9. Who can be chased, order, and losing the target

- Eligible: any client slot whose entity is in use and that is not a spectator in effect. Dead
  players are eligible. The chaser himself is a spectator, so never eligible. Baseq2 has no bots,
  anything occupying a client slot counts. (read)
- Start (attack or jump while not chasing): always the lowest-numbered eligible slot, scanning
  from slot 1 to `maxclients`; never "the one after the last". (read)
- Next: slot numbers upward from the current target, wrapping from `maxclients` to 1; previous:
  downward, wrapping from 1 to `maxclients`; skipping slots not in use and spectators. If no
  other eligible slot exists, the step lands back on the current target. (read)
- Target dies: the chase continues on him, in the dead view (see 10). When he respawns, it goes
  on following him. (read)
- Target becomes a spectator: at his next command (spectators still send commands) the camera
  steps to the next eligible slot; if the step lands back on him (no one else eligible), the
  chase ends silently: no message, free flight and prediction come back. (read)
- Target leaves: nothing updates the chaser, since the camera is moved only when its target
  sends a command. The chaser stays frozen at the last place, still nominally chasing; stat 16
  drops to 0, so the "Chasing" line disappears while "SPECTATOR MODE" stays. Attack ends the
  chase; jump or `invnext` steps on from the empty slot. If someone new connects into that slot,
  the camera follows the newcomer. (derived from reading which events move the camera and fill
  the stats)

### 10. The camera's place

All read in both; the arithmetic is in the engine's units.

- The eye: the target's origin raised by the target's view height (22 for a standing player).
- The direction: the target's own view angles, with pitch limited to at most 56 degrees looking
  down (positive pitch is down in this game); looking up is not limited.
- The goal: 30 units straight back from the eye along that direction. Its height is then raised,
  if below, to 20 units above the target's origin; if the target is off the ground (no ground
  entity), 16 units more are added.
- Walls: a point trace (zero size) from the eye to the goal against solid brushes and windows
  (not players, monsters or water), ignoring the target. The camera goes to the trace's end,
  then 2 units forward along the view direction (back toward the target).
- Floors and ceilings: a point trace 6 units straight up from there; if it hits, the camera is
  put at the hit and lowered 6. Then a point trace 6 units straight down; if it hits, the camera
  is put at the hit and raised 6.
- The chaser's own view height is set to 0, so the camera point is his eye.
- View angles: copied exactly from the target's (unlimited pitch; the 56-degree limit affects
  only the placement), and the chaser's angle offsets are set so his client shows those angles
  whatever his mouse does. If the target is dead: roll 40, pitch -15 (looking up), yaw toward
  the target's killer (the yaw the dead target turned to face).
- Movement: the chaser's own movement step is not run while chasing (only his command's angles
  are kept). The movement type sent to his client is "freeze" while the target lives and
  "dead" while the target is dead, and client prediction is turned off.
- The camera is always this third-person point; the target's own first-person view is never
  used, and no gun model is shown (spectators have none).
- Update rate: once per command the target sends (and once when a chase starts), not once per
  server frame. (read; consequence derived)

### 11. Stats 16 and 17 and the bar

- Stat 17: 1 for every spectator in effect, chasing or not; 0 for every player. (read)
- Stat 16: while chasing a target whose slot is in use, the configstring index of the target's
  player-skin configstring, that is the player-skins base plus the target's slot number counted
  from 0 (entity number minus 1); otherwise 0. The base is 1312 on the classic protocol layout
  (derived: models start at 32, then 256 each for models, sounds, images, light styles and
  items); on Q2PRO's extended protocol it is 12862 (derived: models start at 62, then 8192
  models, 2048 sounds, 2048 images, 256 light styles, 256 items). (read; numbers derived)
- That configstring holds the player's name, a backslash, then his `skin` user info value (for
  example a name followed by a backslash and male/grunt). (read)
- While chasing, every server frame the target's whole stat array is copied into the chaser's
  after the target's own stats are filled, then 17, the layout bits and 16 are set over it: the
  chaser sees the target's health, armour, ammo, frags, timers and pickups. The layout bits are
  the chaser's own (his scoreboard and inventory toggles), not the target's. While not chasing,
  a spectator's stats are his own, filled as for any player. (read)
- The bar (deathmatch's status bar, read in both): when stat 17 is non-zero, `SPECTATOR MODE`
  in the alternate (highlighted) character set, at the left edge of the centred 320-wide area,
  58 pixels above the bottom. When stat 16 is non-zero, `Chasing` in the normal character set at
  the same left edge 68 pixels above the bottom (one line above "SPECTATOR MODE", 10 pixels
  higher), then the configstring that stat 16 names at 64 pixels from the left edge (after the
  7 characters of 8 pixels and one blank cell). (read)
- Both clients draw the whole configstring, so the line reads `Chasing` then the name, a
  backslash and the skin, for example a name followed by `\male/grunt`. (read in both clients,
  the resulting text derived)

### 12. `No other players to chase.`

- Printed when a spectator in effect who is not chasing presses attack or jump and no slot is
  eligible (none in use that is not a spectator). Only to that spectator, as a centre print, with
  no line end. Never printed when a step finds no one else or when a chase ends because the
  target became a spectator. (read)

### 13. Dead players

- Only spectators in effect have the chase camera; every control in 8 checks that. A dead player
  presses attack to respawn (in deathmatch only attack, or forced respawn), jump does nothing,
  the inventory commands select items. (read)
- The dead view in 10 (roll 40, pitch -15, toward the killer) is what a chaser sees when his
  target is dead. (read)
