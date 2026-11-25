# Walking through a teleporter: the turn, the high-lag teleport, the client's handling

WORKING NOTE (porting/): facts from a clean-room reader, 2026-11-23, for the se-teleport work (classic/protocol/lag_turn.hpp). Facts marked read or derived.

## Correction to the notes first

- [read] Current KTX does NOT set the engine's teleport time to now + 0.7 s at a teleport. That line is disabled in the mod, with a note that the engine field is now used by the water-jump code. KTX instead keeps its OWN mod-side field, also called `teleport_time`, and sets it to the moment of the teleport (not +0.7). The engine never sees that field. The mod uses it in one place: the "spawnicide" feature (`k_spawnicide`) skips players who teleported less than 1 s ago.
- [read] The rest of the notes holds for current KTX: it sets `fixangle` to 1, sets the player's `angles` to the destination's angles (the `mangle` that `info_teleport_destination` keeps from its own `angles` before zeroing them), and clears the on-ground flag. The destination point is 27 units above the marker. Velocity: the player's forward vector from the destination angles (pitch included) times 300 when `k_yawnmode` is off. When `k_yawnmode` is on, the vertical speed is zeroed first, the horizontal speed is cut by `k_teleport_cap` percent (default 24, clamped to 0–100), and the result is never below 300. That speed goes along the forward vector.
- [read] KTX also sets the field `teleported` to 1 on the player (an int field). This is what the server reads (question 1). Only the trigger_teleport touch sets it. Other `fixangle` users (the spawn, race mode, spectators, the match start) do not.

## Server (MVDSV)

### 1. When the setangle goes out, and which kind

- [read] The teleport happens inside the player's own move. The server runs the player's command when his packet arrives. After the move it links the player into the world and fires the touch functions of the triggers he overlaps. The mod's teleport runs at that moment, so `fixangle` is set partway through reading his packet.
- [read] The setangle goes into the next unreliable datagram the server builds for that client. The reliable channel is not used. The server's own note says a setangle can be lost with a dropped packet. In the datagram it comes after the choke count and any damage message, and before the entities.
- [read] A client's datagram is only built when that client has sent a packet ("reply at end of frame") and the client's rate allows a packet. So it normally goes out at the end of the same server frame in which his teleporting command ran. [derived] If the client is choked by rate, the setangle waits for the next frame he is allowed a packet.
- [read] `fixangle` is cleared in the same step that writes the setangle into the datagram. That is the moment the datagram is built, before it is sent. The MVD demo also gets a setangle for that player, with the player's number in place of the kind byte.
- [read] The kind byte is written only to clients that negotiated the extension:
  - 1 (teleport) when the mod's `teleported` field is nonzero at the moment the datagram is built,
  - 2 (respawn) when it is zero,
  - 0 (unknown) when the mod has no `teleported` field.
- [read] When the kind byte is written, the server then sets `teleported` back to 0.
- [read] The server only offers the extension on a map whose progs define a `teleported` field. It checks this each time a map loads. [derived] So kind 0 is reachable only in odd cases.
- [read] When the kind is written, the server also records three things:
  - the sequence number of the outgoing packet that carries the setangle (the number the packet header will bear),
  - the client's incoming sequence at that moment,
  - the yaw change: the player entity's new yaw minus the yaw of the last command received from the client.
- [read] It then applies the same adjustment as question 2 to its saved copy of the last command. That copy is replayed when packets were dropped.

### 2. Commands made before the client saw the setangle (with the extension)

- [read] Which commands are adjusted is decided by sequence numbers, not by time. While the client's acknowledged sequence (the newest server packet it says it has received) is below the sequence of the packet that carried the setangle, every command in each arriving packet is adjusted before it runs. A packet carries three commands: the newest, the one before and the oldest.
  - The newest is always adjusted.
  - The middle one is adjusted only if its packet's sequence (this packet's sequence minus 1) is above the incoming sequence recorded at the teleport.
  - The oldest is adjusted only if this packet's sequence minus 2 is above it.
  - [derived] So commands the server had already run before the teleport are never touched. Older commands only run at all after packet loss.
- [read] The adjustment stops when a packet arrives whose acknowledged sequence is at or past the setangle's packet. The record is then cleared. A later setangle starts a new record.
- [read] Teleport (kind 1): the command's view angles are NOT changed. Its sideways and forward move amounts are rotated together, as a 2D vector about the vertical axis, by the recorded yaw change. The up move and the pitch are untouched.
  - [derived] On the ground and in the air the game flattens the forward and right vectors to the horizontal before using them. So the rotated command pushes the player along the same horizontal direction and with the same strength as the unrotated command would under the new yaw.
  - [derived] Under water the wish direction includes pitch, so the match is exact only at level pitch.
  - [derived] The move fields are whole numbers, so the rotated values lose their fractions (a rounding of under 1 unit of move).
- [read] Respawn (kind 2): the move amounts are left alone. The command's yaw is replaced by the player entity's current yaw. Pitch is left alone.
- [derived] Because kind 1 rotates the move and keeps the old angles, the server's view angle for that player stays on the OLD yaw for about one round trip. Two things follow from that:
  - The player's model yaw (what others see) follows that old yaw too.
  - Anything the mod aims along the view angle (shots) goes along the old direction during that window. The extension fixes only where he moves, not where he faces on the server.

### 3. Without the extension

- [read] Confirmed. The player move always takes its angles straight from the command it is running. While `fixangle` is set, the server does not copy the command's angles into the player's view angle or model angles, but the move itself still uses the command's angles.
- [read] Once `fixangle` has been cleared (when the setangle is written), each arriving command again sets the player's view angle and model yaw from its own angles.
- [derived] So every command the client made before the setangle reached it moves the player along the old yaw, with the 300 units/s of teleport velocity already set along the new direction. The server also shows and aims him along the old yaw until his commands carry the new angles. That is about one round trip plus the client's frame.
- [derived] The client's prediction replays the same commands with the same old angles. Client and server agree, so the player sees no correction. He simply steers the wrong way for that window.

### 4. Anything else the server does at a teleport

- [read] The mod moves the player with its "set origin" builtin. In MVDSV that builtin also clears that player's antilag position history. [derived] From then on, other players' lag compensation can rewind him only to positions from after the teleport.
- [read] The server does none of these at a teleport: change the move type, drop commands, change a command's msec, or pause. Long commands are still cut into halves at over 50 ms, and the move time is still capped at 0.1 s, as for any command.
- [read] Done by KTX, not the server: telefrags anyone whose box overlaps the arrival. If the player's grappling hook was out, KTX resets the hook and delays his next attack by 0.25 s. KTX plays the teleport sound and the teleport splash at the source and 32 units in front of the destination.

### 5. The teleport time as the water-jump time

- [read] The server hands the engine entity field `teleport_time` to the player move as the water-jump timer. After the move it writes the timer back. With old non-QW ("nq") progs it also keeps the field from being changed by the pre-think, and mirrors the timer into the water-jump flag.
- [read] The move treats the timer as a count-down in seconds. While it is nonzero:
  - ground friction is skipped,
  - ground and air acceleration are skipped,
  - jumping is refused,
  - after sliding along walls the velocity is put back to what it was before the slide.
- [read] It is cleared when:
  - the vertical speed is below 0 at the start of a move,
  - the player is found standing on ground,
  - the player is dead,
  - it counts down below zero, by the command's frame time each move.
- [read] The water-jump itself sets it to 2 s as a safety limit.
- [derived] A mod that wrote an absolute time such as now + 0.7 into it would hand the move a count of hundreds or thousands of seconds. Accel and friction would then be off until the player started falling or touched ground. The 0.7 would mean nothing.
- [read] Current KTX writes nothing to the engine field at a teleport (see the correction). So no water-jump state follows a KTX teleport.

## Client (ezQuake)

### 6. Receiving setangle kind 1 under the extension

- [read] ezQuake reads the kind byte only when the extension was agreed (or in MVD playback, where the byte is a player number). First it turns off position interpolation for the next two physics frames (also for kinds 0 and 2). Then it sets its view angles to the three angles received, pitch and roll included.
- [read] For kind 1 it walks the commands already sent but not yet acknowledged. It starts two past the last valid frame and stops before the newest outgoing sequence, at most one less than the size of its 64-frame buffer.
  - The yaw change is the new yaw minus the yaw of the command this packet acknowledges.
  - [derived] That is the same reference the server used, the command that touched the teleporter, when no packets were lost.
  - Each command's sideways and forward move are rotated by that change in the same way as on the server. Those commands' angles are left as they were.
- [read] After that walk, the view yaw is the yaw of the newest unacknowledged command plus the yaw change. [derived] So turning the player did after touching the teleporter is kept. The pitch stays at the destination's pitch and the roll at what was received.
  - [derived] With KTX the pitch is the destination marker's pitch, normally 0. So the player's pitch snaps to level at every teleport.
- [read] For kind 2 the commands' yaw is set to the new yaw instead, and their moves are left alone.
- [derived] Prediction replays those adjusted commands. With the server doing the same rotation, the predicted and the real paths agree.
- [read] If `cl_pext_lagteleport` is on but the server lacks the extension, the client prints, on connect: "Warning: high-lag teleport fix not available".

### 7. Predicting the teleport

- [read] No. Nothing in ezQuake's prediction touches triggers or moves the player to a teleporter's destination. The only client code that knows about teleporters is a demo spawn warning feature that lists teleporter exits from the map's entities. No setting exists for it.
- [derived] For about one round trip the predicted player keeps going into the teleporter's volume, often into the wall behind it. Then the confirmed position jumps to the destination.

### 8. How the jump in origin is drawn

- [read] A setangle of any kind turns off interpolation of the player's own position for the next two physics frames.
- [read] With independent physics on (`cl_independentPhysics`, default 1; skipped when `cl_nolerp` is 1 or `cl_nolerp_on_entity` applies), the client smooths its predicted position between physics frames. If the predicted position moves more than 100 units on any one axis between two physics frames, that step is treated as a teleport and not interpolated. With `cl_earlypackets` on (default 1) the step before it is not interpolated either, to avoid a flash of the old direction at the entrance.
- [read] Step smoothing (the crouch that smooths stair climbs) resets whenever the player moved 48 units or more since the last drawn frame, measured as straight-line distance, or interpolation was turned off.
- [read] Other entities and other players are not interpolated across an origin change of 200 units or more (straight-line).
- [read] Prediction error smoothing (`cl_predict_smoothview`, default 1) works only with the weapon-prediction extension. It eases an error of more than 4 and less than 64 units over a few frames. A bigger error, such as a teleport, snaps. The nudge it carries is capped at 64 units.
- [read] View angles: the player's own angles are not interpolated when he is playing. Angle interpolation is used only when tracking someone else, in a demo, or in the locked move type.
- [read] Bob (default 0 in ezQuake) and view roll from sideways speed (`cl_rollangle`, default 0) are not reset at a teleport. They follow the velocity like any other frame.

### 9. Settings around teleports, and any flash

- [read] `cl_pext` (default 1) is the master switch for all protocol extensions. `cl_pext_lagteleport` (default 1; only its first bit matters; servers cannot set it) asks for this one.
- [read] `cl_earlypackets` (default 1, "Early Packets" in the menu): reads server packets as soon as they arrive and sets up prediction on that frame. As said in 8, it also skips interpolating the frame before a jump.
- [read] Also related: `cl_nolerp` (default 0), `cl_independentPhysics` (default 1), `cl_predict_smoothview` (default 1).
- [read] Teleport effect settings: `r_telesplash` (default 1) draws the particle splash of the teleport temporary entity. `gl_particle_telesplash` (default 0) is a denser splash, and `gl_coronas_tele` (default 0) a blue corona at the splash. A particle script can replace the splash. Hud radar can show teleport effects.
- [read] Confirmed: no full-screen tint or flash for the player's own teleport. The setangle handling sets no colour shift. The teleport temporary entity draws particles (and the optional corona) at the splash point, and spawns no dynamic light, so the dynamic-light colour shift (`v_dlightcshift`) does not fire. KTX's teleport sends no bonus-flash command. [derived] What the player sees is the splash 32 units in front of him at the destination.

### 10. Other causes of lost control after a teleport

- [derived] One round trip of steering the old way (question 3) when the extension is off or not agreed. With KTX on MVDSV and ezQuake defaults it is agreed, and both sides rotate the move.
- [derived] The prediction gap (question 7). Until the server's word arrives, the player's view keeps walking into the teleporter. Then it jumps to the destination. Prediction then replays the in-flight commands from there.
- [derived] The setangle travels in an unreliable datagram. If that one packet is lost, the client never turns. The server stops rotating once the client acknowledges a later packet.
  - With the extension the player then moves along the destination's direction while facing the old one. Without it he stays facing and moving the old way.
  - Neither program resends it.
- [read] The arrival velocity is replaced by 300 along the destination's facing, and the player is put in the air (on-ground cleared, 27 units above the marker). In the air, acceleration toward the wish direction is capped at 30 units/s per command.
  - [derived] Until he lands, he can barely change that 300 units/s, however he presses.
  - How long the fall lasts depends on how high the mapper placed the marker. If the marker sits at a standing player's origin, the player drops 27 units, about 0.26 s at gravity 800.
  - The source does not fix the marker height.
- [derived] The pitch snaps to the destination's pitch (normally level) at every teleport (question 6).
- [derived] With the extension the server keeps the player's facing on the old yaw for about one round trip (question 2). Opponents see him facing the old way, and shots fired in that window leave along the old yaw.
- [read] A grappling hook that was out is pulled in, and the next attack is delayed 0.25 s (KTX).
- [read] How the programs avoid these:
  - the extension's rotation of in-flight moves on both sides,
  - no interpolation across the jump (thresholds in 8),
  - the step-smoothing reset at 48 units,
  - prediction error smoothing kept to errors under 64 units, so a teleport snaps instead of sliding.
  - No program re-sends the setangle reliably or predicts the teleport.
