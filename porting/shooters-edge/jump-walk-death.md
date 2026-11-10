# The jump heard, walking, and the view weapon at death

Clean-room answers (a reader session read the original multiplayer release, the modern client and the modern server; no code). Built from: the arena and campaign variants' jump and swim sounds (`games/shooters_edge/qc`), the window's controls with no walking (`client/controls.hpp`), the held weapon at death (`view/hand.hpp`: our own sinking, which no classic client draws).

Sources read: id Software's GPL source release of the classic multiplayer game (its client, its
server and its deathmatch QC mod), the modern client ezQuake (current main branch, shallow clone,
November 2026) and the modern server MVDSV (same date). Each fact is marked [read] (stated by the
source), [derived] (follows from facts read, not stated in one place) or [measured] (none here:
nothing was run).

## 1. The jump's sound in the multiplayer mod

- File: `player/plyrjmp8.wav`. Channel: body (channel number 4). Volume 1 (full). Attenuation:
  normal (1). [read]
- Where it is decided: in the mod's per-command pre-think hook (the one the server calls before
  it runs the move for that command), not in the move code. So the sound is started BEFORE the
  move of the same command runs; the move code then makes the actual 270 upward velocity change.
  [read]
- The mod only looks at the jump at all when: no intermission is running; the player's view
  offset is not zero (zero marks intermission or finale); the player is not dead and not dying
  (the dead/dying checks return before the jump is considered). [read]
- With the jump button down, the sound plays only when all of these hold: the player is less than
  waist deep (water level 0 or 1; at 2 or more the swim sound of question 2 plays instead and no
  jump sound); the entity is flagged on the ground; and the mod's own "jump released" flag is set
  (the "no pogo stick" rule). When it plays, the mod also clears its "jump released" flag and
  zeroes the entity's jump button field for the rest of that think; zeroing the button field has
  no effect on the move, which reads the button from the command itself. [read + derived]
- The on-ground flag the mod sees is the one the server wrote after the PREVIOUS command's move,
  not a fresh test. [read + derived]
- The mod also refuses when the entity carries the water-jump flag. [read] In this release the
  server's move never sets that flag (it keeps the water jump in its own timer, exposed to the mod
  only as the entity's `teleport_time`), and the mod's own water-jump check is disabled, so in
  practice this guard never fires. [derived]
- With the jump button up, the mod sets its "jump released" flag on every command. [read]
- Holding the jump: one sound per jump, not one per frame. The sound needs a release between
  jumps, mirroring the move's own rule. [read]
- The mod's "released" flag and the move's "jump held" memory are kept separately, set by the
  same button-up rule but cleared at different moments (the mod clears its flag only when it
  plays the sound; the move also marks the jump held during a water-jump out of the water and
  while dead). So in rare cases the two can disagree: e.g. the mod's on-ground flag is a command
  old, or a player who pressed jump fresh while swimming climbs out with a water jump and lands
  still holding it (the move refuses to jump, the mod may still play one jump sound). [derived, but
  not observed]
- A command longer than 50 ms is split by the server into two halves, each running the
  pre-think; the sound can still play at most once because the flag is cleared. [read + derived]

## 2. Jump held in water, lava or slime

- At water level 2 or more with jump held, the mod plays a swimming sound at most once per second:
  a random choice, half and half, between `misc/water1.wav` and `misc/water2.wav`, on the body
  channel, volume 1, normal attenuation. This applies to any liquid type (water, slime, lava),
  the mod does not distinguish. [read]
- The mod does not change velocity for swimming up in this release; the move code does it (it sets
  the upward speed to 100 in water, 80 in slime, 50 in lava while jump is held at level 2 or
  more). [read]
- Related sounds the mod plays from the same per-command hook (not tied to the jump button):
  entering a liquid: `player/inh2o.wav` (water), `player/slimbrn2.wav` (slime), `player/inlava.wav`
  (lava), body channel; leaving all liquid: `misc/outwater.wav`, body channel; surfacing after
  being under: `player/gasp1.wav` or `player/gasp2.wav` on the voice channel (gasp2 when the air
  had already run out). All volume 1, normal attenuation. [read]

## 3. The modern client and the jump sound

- The original client plays no jump sound itself; it only plays the server's sound message.
  [read]
- ezQuake can predict the own jump sound locally. It does so only when the server has negotiated
  MVDSV's weapon-prediction protocol extension and weapon prediction is not turned off (by the
  player's `cl_nopred_weapon`, by the server forcing it off, or by spectating). [read]
- Setting: `cl_predict_jump`, default 1. When on (and the extension active), the client's own move
  prediction queues `player/plyrjmp8.wav` on channel 4 (body), volume 1, attenuation 0 (no
  distance fall-off), at the moment the predicted move makes a jump (on the ground, jump released,
  not swimming, not water-jumping). Queued prediction sounds are delayed by up to
  `cl_predict_buffer` frames (default 1) so a misprediction can be corrected first. [read]
- When `cl_predict_jump` is on and `cl_nopred` is 0, the client drops the server's copy of the
  sound for the own player when it arrives on channel 4 with that exact file name, so the jump is
  heard once. [read]
- No setting found that silences only the own jump sound. Setting `cl_predict_jump` to 0 does not
  silence it: it just falls back to the server's sound message. [read + derived]
- MVDSV itself adds no jump sound; the sound comes from whatever mod the server runs. [read, by
  search for the sound name and for jump-sound handling; nothing found]

## 4. Landing (multiplayer mod)

- Checked in the mod's post-think hook, which the server runs once per received move packet
  (after all commands in it). It compares the vertical speed remembered at the previous post-think
  with the current on-ground flag; it is skipped while dead, dying, or in intermission. [read]
- A landing counts when the remembered vertical speed was below -300 units/s and the player is now on the ground. Three outcomes:
- Landing in water (the current liquid type is water): `player/h2ojump.wav`, body channel, no damage,
- Any other landing faster than 650 units/s downward: 5 points of damage (from the world, death type "falling", so the
  obituary names a fall), plus `player/land2.wav` on the voice channel; the damage goes through
  the normal damage path, so armour absorbs its usual share and the pain sound may play too,
- Any other landing between 300 and 650 units/s downward: `player/land.wav` on the voice channel, no damage.
  All volume 1, normal attenuation. [read; armour and pain derived from the damage path]
- Then the current vertical speed is remembered for the next check. [read]

## 5. Walking and running

- No `cl_run` setting exists in the original release or in ezQuake. [read, by search]
- Original release defaults: `cl_forwardspeed` 200, `cl_backspeed` 200 (both saved to the config),
  `cl_sidespeed` 350, `cl_upspeed` 200, `cl_movespeedkey` 2.0, `cl_anglespeedkey` 1.5. [read]
- ezQuake defaults: `cl_forwardspeed` 400, `cl_backspeed` 400, `cl_sidespeed` 400, `cl_upspeed`
  400, `cl_movespeedkey` 2.0, `cl_anglespeedkey` 1.5. [read]
- `+speed` (held): multiplies the forward, side and up move amounts the client sends by
  `cl_movespeedkey`, and multiplies the keyboard turning speed by `cl_anglespeedkey`; it also
  scales joystick moves by `cl_movespeedkey`. It is a "run" key: it never slows the player.
  [read]
- Server maximum: `sv_maxspeed` default 320 units/s in the original server and in MVDSV; the move
  clamps any wished speed above it down to it. [read]
- In practice: at the original release's defaults a player holding only forward or back asks for
  200 and moves at 200 units/s, slower than the 320 maximum (a "walk"), unless holding `+speed`
  (400, clamped to 320) or with "always run" on. Strafing alone (350) is already clamped to 320.
  At ezQuake's defaults every direction asks for 400 and is clamped to 320, so a player never moves
  slower than the maximum; `+speed` then has no visible effect on move speed. [derived]
- Menu: the original client's options menu has "Always Run", shown checked when
  `cl_forwardspeed` is above 200; toggling sets `cl_forwardspeed` and `cl_backspeed` to 400 (on)
  or 200 (off), leaving side speed alone. ezQuake's options menu has "Always Run" with the same
  test; toggling sets forward, back AND side speed to 400 (on) or 200 (off). [read]

## 6. The view weapon at death

- Mod, on death (same moment health drops to 0 or below and the death routine runs): the view
  weapon model field is cleared to empty at once, the view offset is set to 8 units below the
  origin, the player becomes non-solid and tossed, gets up to 300 extra upward speed (random) if
  rising slower than 10 units/s, and enters the dying state. A gibbed player's head gets a view
  offset of 8 above its origin and a box with a floor of 0 instead of -24. On respawn the view
  offset returns to 22 and the weapon model is set again from the current weapon. [read]
- The original client does NOT use the mod's view offset for the eye height. It uses a fixed eye
  height of 22 above the origin while alive, 16 BELOW the origin when the player state carries the
  dead flag, and 8 above when it carries the gib flag; when dead it also forces the view roll to
  80 degrees. These change in one frame, with no smoothing. [read]
- The server sets the dead flag in the player state whenever the entity's health is 0 or below,
  and the gib flag whenever the entity's box floor is not -24. [read]
- The original client hides the view weapon on its own whenever the dead or gib flag is set, with
  no setting; independently the empty weapon model the mod sets reaches the client as the weapon
  stat. No lowering, sinking or other animation: the weapon simply stops being drawn that frame.
  [read]
- ezQuake: likewise hides the view weapon with no animation whenever the dead or gib flag is set,
  or the health stat is 0 or below, or `r_drawviewmodel` is 0 (default 1; value 2 hides it only
  when the field of view is above 90), or the player is invisible and
  `r_drawviewmodel_invisible` is 0, or the camera mode says not to. Eye height when dead is
  16 below the origin, when gibbed 8 above, and roll 80 degrees, as in the original; the alive eye
  height comes from a server-sent view-height stat when the server supports it, else a fixed
  default (its value is defined in a shared header not found in this clone; the original's is 22).
  [read]
- No death-specific lowering of the weapon exists in any of the three projects. [read, by
  search of the view code]
