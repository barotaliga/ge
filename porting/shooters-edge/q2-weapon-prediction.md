# Quake II's held weapon: what the protocols carry, when a stock server fires

Clean-room facts for the client's weapon prediction (`games/shooters_edge/net/q2_weapon.hpp`), written by a reader from the questions in this thread; the implementer never read the source. Built from: § 1 (no protocol carries weapon state: the prediction derives it from the gun model, gun frame and STAT_AMMO), § 3 (a new press steps the weapon at once, once per frame; the press is dropped at each frame's start), § 4 (the shot reaches the client with the next frame packet), § 7 (bit 128 alone says silenced).


Read: Q2PRO at its current master (commit 601a8df, December 2025): its client, its server and the
baseq2 game library it bundles. Also read the original Quake II 3.21 game source (id Software's
GPL release) wherever the bundled baseq2 might differ; each answer says whether they differ.

Tags: [read] means stated directly by the source. [derived] means worked out from facts I read,
with the reasoning given. [open] means the source does not settle it. Nothing here was measured
by running a binary.

---

## 1. What the protocols carry about the held weapon

**Player state fields (all protocols)** [read]
- The weapon fields are gun index, gun frame, gun offset (3 values) and gun angles (3 values).
  Nothing else in the player state describes the held weapon.
- Gun index: one byte without the extended-limits extension, and an unsigned 16-bit short with
  it. With the extension the low 13 bits are the model index and the top 3 bits are the gun
  model's skin number (0 to 7). The client draws that skin.
- Gun frame: always one byte.
- Gun offset and gun angles: three signed bytes each. Each byte is the value times 4, clamped to
  -128..127. So the resolution is 0.25 units (or 0.25 degrees) and the range is -32 to +31.75.
- How the bits are laid out differs by protocol:
  - **Protocol 34:** one "weapon index" bit and one "weapon frame" bit in the 16-bit player-state
   flag word. The weapon frame bit is set when any of gun frame, gun offset or gun angles
   changed, and it sends all three together: the frame byte, then 3 offset bytes, then 3 angle
   bytes.
  - **Protocols 35 and 36:** the frame byte has its own bit. Gun offset and gun angles each have
   their own "extra" bit (protocol 35's extra flags), or their own bits in protocol 36's packed
   player-state word. Each is sent only when it changed.
- In the 64-stat layout (extended limits 2: protocol 36 minor 1025 or later, extended minor 3435
  or later), the newer game API's player state has four reserved integer fields. They are never
  packed or sent. No weapon meaning is attached to them.

**Stats** [read]
- With protocol extensions the player state has 64 stats; otherwise 32.
- The stats that baseq2 fills and that relate to the weapon:
  - STAT_AMMO_ICON (index 2) and STAT_AMMO (index 3): the CURRENT weapon's ammo icon and count.
   Both are 0 for a weapon that uses no ammo.
  - STAT_SELECTED_ICON (6) and STAT_SELECTED_ITEM (12): the inventory item currently selected.
  - STAT_PICKUP_ICON (7) and STAT_PICKUP_STRING (8): an item just picked up, shown for 3
   seconds (30 frames at 10 Hz).
  - STAT_HELPICON (11): carries the current weapon's icon, but only when the player is
   center-handed (hand 2) or the field of view is above 91, and the help icon is not flashing.
- No stat carries any of these: the weapon state (ready, activating, dropping, firing), the next
  think time, the pending weapon, the ammo of weapons not held, or the inventory.
- The bundled baseq2 also adds STAT_TIMER2_ICON (18) and STAT_TIMER2 (19). They show
  invulnerability as a second timer. The 3.21 game has one timer only.

**Other channels that touch the weapon** [read]
- The player's own entity state:
  - Bits 8 to 15 of its skin number carry the visible-weapon model index. This is the index into
   the list of "#"-prefixed player-weapon model configstrings. It changes when the weapon
   changes.
  - Its looping sound carries the weapon hum or loop: the railgun hum, the BFG hum, the
   hyperblaster loop, the chaingun spin and the primed-grenade tick.
- The extended entity "more effects" bits include a dual-fire shell bit. It only colours the
  shell.
- The client also has a damage-dealt temp entity for hit markers (client settings
  cl_hit_markers, default 2). The bundled baseq2 never sends it.

**Client settings (clc_setting: a 16-bit index, then a 16-bit value)** [read]
- Index 0 is the "no gun" setting. It is the only weapon-related setting.
  - Value 0: send everything.
  - Value 1: leave out gun index, gun frame, gun offset and gun angles.
  - Value 2: leave out the frame, offset and angles, but keep sending the gun index.
- The client sends it on protocols 35 and 36:
  - Value 1 when cl_gun is 0, or when cl_gun is 1 and the player is center-handed.
  - Value 2 when cl_gun is -1.
  - Value 0 otherwise.
- While the "recording" setting (index 2) is on, the server ignores "no gun" and sends
  everything.
- The other settings: no-blend (1), player updates (3), update rate (4), no-gibs (10),
  no-footsteps (11), no-predict (12), no-flares (13). None is about weapons. "No-predict" only
  drops the movement-prediction fields.
- For multi-view demos, the server setting sv_mvd_nogun (default 0, latched) filters the gun
  fields out.

**Server settings, configstrings, stufftext, game features** [read]
- Server-to-client settings (svc_setting) are only "player updates" (0) and "fps" (1).
- No configstring is about weapon prediction. The weapon models are ordinary model configstrings,
  and the item names are ordinary item configstrings.
- Neither the server nor the bundled game stuffs any weapon-related text.
- The game feature bits (g_features) defined are:
  - bit 0: client number
  - bit 1: proper in-use
  - bit 2: MVD spectator
  - bit 3: all disconnects
  - bit 10: enhanced savegames
  - bit 11: variable FPS
  - bit 12: extra userinfo
  - bit 13: IPv6 aware
  - bit 14: allow index overflow
  - bit 15: protocol extensions

  None is about weapons.
- The frame header of protocol 36 has 4 flag bits:
  - suppressed (bit 0)
  - client drop (bit 1)
  - client predict (bit 2): set when the server repeated the client's last command to cover lost
   packets
  - reserved (bit 3): the source gives it no meaning.

**Reserved bits for weapons** [read]
- No extension bit, setting index or feature bit is reserved for weapon state or weapon
  prediction.

## 2. What the client does with the gun

**No gun prediction** [read]
- The Q2PRO client runs nothing about the gun ahead of the server. It does not advance the gun
  frame, start the flash or play the fire sound.
- Its prediction code covers movement only. Its input code sends commands only.
- The gun is drawn from the gun index, frame, offset and angles that the received frames carry.

**Where the gun is drawn** [read]
- The gun's origin is the predicted view origin plus the server's gun offset.
- Its angles are the predicted view angles plus the server's gun angles.
- Developer-only cvars move it further: cl_gun_x, cl_gun_y and cl_gun_z (default 0).
- Developer commands override the model or frame for inspection: gun_next, gun_prev and
  gun_model.

**The muzzle flash on receipt** [read]
- The client acts on a received muzzle flash for its own entity. Besides the light and sound
  (question 7), it draws a muzzle-flash model on the view weapon, and only for its own entity.
  This is the cvar cl_muzzleflashes, default 1.
- The flash model is drawn while the client time is within 50 ms of the start of the current
  frame interval (the server time minus one frame time), then dropped.
- The muzzle light's lifetime is cl_muzzlelight_time, default 16 ms, clamped to 0..1000.

**How the gun is interpolated** [read]
- The client interpolates between the previous key frame's player state and the current key
  frame's player state, by the key-frame fraction:
  - Gun offset is a linear blend.
  - Gun angles use a shortest-way angle blend.
  - The model is told to draw the new frame with the old frame as its "old frame", with backlerp
   equal to 1 minus the fraction.
- At 10 Hz every frame is a key frame. The fraction is the client time minus (server time minus
  100 ms), divided by 100 ms, clamped to 0..1.
- With variable server FPS the client still interpolates the gun, and the kick angles, only
  between KEY frames: every Nth frame, where N is the server rate divided by 10. The key-frame
  fraction runs over 100 ms. So a gun-state change that the server sends on a non-key frame is
  not drawn until the next key frame [read; derived for the consequence].

**Special cases** [read]
- When the new gun frame is 0, the client does not interpolate: the old frame is set to 0. This
  marks "just changed weapon".
- Nothing special is done when the gun INDEX changes and the frame is not 0. The new model is
  drawn, interpolating from the previous frame NUMBER to the new one. That previous number
  belonged to the old model [derived].
- The previous player state is replaced by the current one (no interpolation) when any of these
  holds:
  - the previous frame was invalid or not the immediately preceding (key) frame
  - the origin jumped more than 256 units on any axis
  - a teleport event or teleport bit is seen
  - the point-of-view player changed
  - cl_nolerp is 1

## 3. When the game steps the weapon

The bundled baseq2 and the 3.21 game behave the same here. The only difference: the 3.21
debounce for the "no ammo" click counts in seconds, and baseq2 counts in frames. The effect is
the same 1 second [read].

**Where the weapon is stepped** [read]
- On a move command: after that command's movement and touches, so a shot leaves from the
  command's final position.
- At the start of each server frame: in the per-client step, which runs in entity-number order.
  The world entity comes first, then the clients, then all other entities, so projectiles and
  monsters think after it.

**(a) On a move command** [read]
- The game keeps three button sets per player:
  - "held": this command's buttons
  - "previous": the command processed just before this one
  - "latched": the bits that went from up to down
- Each command adds to "latched" any button that is down now and was up in the previous command.
- The weapon is stepped on a command only when two things are true:
  - the latched attack bit is set
  - the weapon has not already been stepped since the last frame start
- So a step comes from a NEW press. Holding attack through later commands does not step it again.
- Because "latched" persists until it is cleared (see c), the step happens on the first command
  at or after the press.
- A spectator's latched attack is used to switch the chase camera instead. That clears all
  latched buttons.
- During intermission the command handler returns early. Nothing is stepped and the buttons are
  not updated.

**(b) The "already stepped this frame" flag** [read]
- A per-player flag is set when a command steps the weapon.
- At the next frame start:
  - If the flag is set, that frame's weapon step is skipped and the flag is cleared.
  - If the flag is clear and the player is not a spectator, the weapon is stepped there.
- So each frame interval gets exactly one weapon step. A press-step at time t, between frame
  starts F and F+1, replaces the step that F+1 would have made. The next step is at F+2.
  Example: at 10 Hz with a press 30 ms after F, the steps fall at F, F+30 ms, then F+200 ms
  [derived].

**(c) What a firing weapon reads**

When the generic weapon is ready [read]:
- It tests "latched OR held" attack.
- When that is true, it clears ONLY the latched attack bit. It clears it whether or not there is
  enough ammo.
- With enough ammo it sets the first firing frame and the firing state. In the SAME step it goes
  on into the firing logic, so if that frame is a fire frame the shot happens at once.

Which weapons fire on the press step [derived]:
- From each stock weapon's activate-last and fire-frame numbers: for the blaster, shotgun, super
  shotgun, machinegun, chaingun, hyperblaster, grenade launcher, rocket launcher, railgun and
  BFG, the first firing frame is a fire frame. So the shot is made in the same step as the press.
- The fire routine itself advances the frame. So the client never sees the first firing frame:
  it sees the one after.
- The hand grenade only starts priming on the press.

While firing [read]:
- The automatic weapons read the HELD attack bit only:
  - The machinegun stops and resets its shot count when attack is up.
  - The chaingun drops to its wind-down at one frame if attack is up, and loops back from a later
  frame while attack is held and it has ammo.
  - The hyperblaster keeps cycling while attack is held.
  - The held grenade waits while attack is held.
- The single-shot weapons do not read attack while firing.
- The hand grenade's ready state reads latched OR held and clears latched, like the generic
  weapon.

When the latched bits are cleared [read]:
- All of them are cleared at the end of each frame-start step for a living player.
- A dead player's latched bits are not cleared, except when respawning (then all are cleared).
- The attack bit is cleared by a ready weapon when it reads it.
- All of them are cleared by a spectator's chase toggle.

A quick second tap is lost [derived]:
- Say a player presses, releases and presses again within one frame interval. The second press
  sets the latched bit, but the weapon was already stepped, so no step happens.
- At the next frame start the step is skipped and the latched bits are cleared.
- So the second tap is lost unless attack is still held at a later step.

**(d) Is the frame-start step skipped?** [read]
- Yes. It is skipped when a command already stepped the weapon since the previous frame start.

## 4. When the shot reaches the client

**The muzzle flash message** [read]
- The bundled baseq2 sends it as svc_muzzleflash:
  - the entity number as a short
  - one byte: the flash kind, with bit 128 set when silenced
- It is multicast to the PVS of the shooter's origin, and it is UNRELIABLE.

**Does the shooter get it?**
- The shooter is included when his own position's cluster is visible from his origin, and his
  area is connected [read].
- In practice that is always, unless his position is outside the map (no cluster) [derived].

**Timing on the Q2PRO server** [read]
- Packets are read and move commands run as soon as they arrive, between server frames.
- The flash is queued in the client's unreliable list. Unreliable messages are written only when
  that client's next frame packet is built, which happens after the next game frame. The flash
  is written AFTER the frame data in the same packet.
- Between frames, nothing is sent to an active client: neither unreliables nor reliables.

**Timing for a shot made on a move command** [derived]
- The shot arrives with the next frame packet, not sooner.
- That packet's player state already holds the gun frame the command set, because the
  frame-start step is skipped that frame. It also holds the new ammo count.
- At 10 Hz the wait is between 0 and 100 ms after the command was processed.

**Timing for a shot made by the frame-start step** [derived]
- It goes out in the packet of that same frame.

**Cases where the flash is late or lost** [read]
- With variable FPS and a client whose update rate is below the server's, unreliables wait for
  that client's next synced frame.
- If the client is held back by rate limiting that frame, or a fragmented message is still
  going out, the queued unreliables for that frame are thrown away.

**Ordering on the client** [read]
- The client handles a muzzle flash as soon as it parses it: light, sound and view-model flash.
- Since the frame comes first in the packet, the new frame is already in place when the flash
  sound starts.

## 5. Variable server frame rates

**What the server does** [read]
- A game library that advertises variable FPS (feature bit 11) runs at sv_fps. The rate is a
  multiple of 10, up to 60 (a divider of 1 to 6).
- The server calls the game's frame function once per server frame and does not throttle it.
- Whether the weapon steps every server frame or once per 100 ms is entirely up to that game
  library.
- Q2PRO's tree contains no game library that advertises variable FPS, so the source does not
  settle how such a library times its weapon [open].

**The bundled baseq2** [read]
- It advertises only proper in-use, all disconnects and enhanced savegames, plus protocol
  extensions when the server offers them and its cvar g_protocol_extensions is set.
- It does NOT advertise variable FPS. Its frame time is fixed at 0.1 s.
- Without that bit the server forces the rate to 10 and writes 10 back into sv_fps. So with the
  bundled baseq2 the weapon steps once per 100 ms frame, plus the press-steps of question 3.
- The 3.21 game sets no feature bits at all, so the same applies.

**The client** [read]
- The client expects the gun to change on 100 ms key frames (see question 2).
- Under variable FPS, a lone server event (sound, flash) is still spread over the shorter frame
  time.

## 6. A weapon change by `use <item>`

**When the command runs** [read]
- The string command runs as soon as its packet is read.
- Reliable data comes before the unreliable move commands in a packet, so a `use` runs before
  any moves in the same packet [derived].

**What the command checks**, in order [read]
- An unknown name prints "unknown item: <name>".
- An item that cannot be used prints "Item is not usable.".
- An item not owned prints "Out of item: <name>".
- For a weapon:
  - Choosing the weapon already held does nothing.
  - Unless g_select_empty (default 0) is set, a weapon with no ammo prints
   "No <ammo> for <weapon>.".
  - A weapon with less ammo than one shot uses prints "Not enough <ammo> for <weapon>.".
  - Otherwise the weapon is only stored as the PENDING weapon.
- Nothing else happens on the command. The gun frame does not change and no weapon step runs.

**Order of events at the following weapon steps** [read]

Each step is the next frame start, or a command step if attack is newly pressed first.
1. While the old weapon is ACTIVATING, the pending weapon waits. Activation finishes first and
   the weapon reaches ready at its first idle frame.
2. While it is FIRING, the pending weapon also waits until the firing sequence returns to ready.
3. In any other state, the step that sees a pending weapon starts DROPPING:
   - The gun frame is set to the first deactivate frame.
   - The third-person "lower weapon" animation starts, either here (for short drop sequences) or
     when 4 frames remain.
   - There is no other advance in that step.
4. Each later step adds one to the gun frame until the last deactivate frame.
5. The step after the last deactivate frame makes the switch:
   - The old weapon is remembered as the last weapon.
   - The pending weapon becomes current and the pending slot is cleared.
   - The machinegun's shot count is reset.
   - The visible-weapon bits of the player's skin are updated.
   - The ammo index is updated.
   - The state becomes ACTIVATING, the gun frame becomes 0 and the gun index becomes the new
     view model.
   - The third-person pain animation is used as the "raise" animation.
6. Each later step adds one, from 0 up to the last activate frame.
7. The step after that sets the state to ready at the first idle frame. It does not fire.
8. A press can fire on the step after that.

**Step counts** [derived]
- The switch happens on step (last deactivate frame minus first deactivate frame plus 2) after
  the change is seen.
- Ready comes (last activate frame plus 1) steps after the switch.
- At 10 Hz each step is 100 ms. A press-step can make one of them come earlier (question 3).
- The client sees gun frame 0 on the first frame after the switch, and does not interpolate it
  (question 2).

**The hand grenade** [read]
- With a pending weapon and ready, it switches AT ONCE, with no drop frames.
- Its activation is a single step to its first idle frame.
- Any switch while a grenade is primed throws that grenade first.

**Other routes to a pending weapon** [read]
- weapnext, weapprev and weaplast go through the same use action.
- Trying to fire with too little ammo selects the best owned weapon that has ammo. The order is
  railgun, hyperblaster, chaingun, machinegun, super shotgun (needs at least 2 shells), shotgun,
  then the blaster. This happens in the step that tried to fire, and also plays
  "weapons/noammo.wav" on the voice channel at most once per second.
- Death clears the pending weapon and switches at once to no weapon (gun index 0).

## 7. The silencer and quad damage's sound

**The silencer**
- Bit 128 of the muzzle flash value is the ONLY thing that tells any client a shot is
  silenced [read].
- No stat, effect bit, configstring or message carries the silencer's state or its remaining
  shots, in the bundled baseq2 or in 3.21. Neither game's powerup timer shows the silencer
  [read].
- The silencer item's count is in the inventory. It is visible only through the inventory
  message (question 8) or as the selected item [read].

**What a silenced flash changes on the client** [read]
- The light radius is 100 plus 0..31, instead of 200 plus 0..31. The chainguns use their own
  radii either way.
- The fire sounds play at volume 0.2 instead of 1.

**The silencer's shot count** [read]
- Using a silencer adds 30 shots.
- Each "weapon noise" the game makes uses up one shot. That happens once per fire call, so per
  bullet frame for the machinegun.
- Whether a shot is silenced is decided at the start of the weapon step, while shots remain. So
  the shot that uses the last one is still silenced [derived].

**Quad damage's own sound** [read]
- When quad damage is active, each fire frame plays "items/damage3.wav" from the game. It is
  sent before the shot itself.
- It is played on the shooter entity: ITEM channel (3), volume 1, normal attenuation (1), no time
  offset.
- On the wire it is an UNRELIABLE svc_sound, culled to the PHS of the shooter's origin. The
  entity and channel short is sent.
- Q2PRO's server sends the position only when the entity is not in that client's frame (or the
  client uses protocol 34 and the entity is a brush model).
- It is queued and goes out with the next frame packet, like the muzzle flash.

## 8. Does the client ever get the whole inventory?

- No. The only time the whole inventory is sent is when the player opens the inventory with the
  `inven` command [read].
- That message is svc_inventory: the op byte (5), then 256 signed 16-bit little-endian counts,
  one per item index. That is 513 bytes. It is sent reliably to that player only [read].
- `inven` toggles: when the inventory is already open, it only closes it and sends nothing
  [read].
- The open inventory is not refreshed by later changes. A new snapshot is sent only on the next
  open [derived].
- The inventory is marked closed in these cases, and no message is sent:
  - when the scoreboard or help is opened
  - by the `putaway` command
  - on respawn [read].
- No inventory message is sent on a pickup [read].
- A pickup only does three things [read]:
  - sets the pickup icon and name stats for 3 seconds
  - makes the picked-up item the selected item when it can be used
  - changes STAT_AMMO if it is the current weapon's ammo
- These are the same in the bundled baseq2 and in 3.21 [read].
