# Spectator tracking (ptrack): answers

Read: mvdsv, master as of 2026-11-14 (its protocol definitions come from the QW-Group protocol
submodule at its master of the same date); ezQuake (ezquake-source), master as of 2026-11-14.
Every fact is marked [read] (stated by the source's behaviour), [derived] (follows from several
read facts put together) or [measured] (none in this document: nothing was run).

Wire numbers named below are the protocol's own definitions (the message and flag numbers both
programs share): print level "high" is 2; server message numbers: stat update 3 (byte index, byte
value), long stat update 38 (byte index, 4-byte value), print 8 (byte level, string), stuff text 9,
set angle 10, damage 19, centre print 26, player info 42; client message numbers: move 3, string
command 4, teleport request ("tmove") 6. Stat indices: health 0, weapon model 2, ammo 3, armour 4,
shells 6, nails 7, rockets 8, cells 9, active weapon 10, items 15, view height 16. Slots run 0..31
(32 clients). [read]

## Server side (mvdsv)

### 1. `ptrack <slot>` and bare `ptrack`

- The command is a client string command named `ptrack`. It is in the server's fixed table and is
  NOT overridable by the mod: the server handles it itself and the mod's client-command hook never
  sees it. [read]
- A sender that is not a spectator is ignored silently: nothing stored, nothing sent. [read]
- Argument count: anything other than exactly one argument (no argument, or two or more) turns
  tracking off. Stored state becomes "tracking nobody", and the spectator's `goalentity` field
  (visible to the mod) is set to the world entity (entity 0). Nothing is sent back. [read]
- With one argument, it is parsed as an integer: leading whitespace skipped, an optional sign,
  decimal or `0x` hex. A non-numeric argument parses as 0, i.e. slot 0. [read for the parser,
  derived that "abc" means slot 0]
- The argument is a SLOT number (0-based, 0..31), not a user id and not an entity number. [read]
- Refusal: slot below 0, slot 32 or above, the slot's client not fully in the game (anything short
  of "spawned": free, connecting, a zombie), or the slot's client a spectator. On refusal the
  server prints `Invalid client to track` followed by a newline, at print level high (2), to the
  sender only (a print message, number 8), and ALSO turns tracking off exactly as a bare `ptrack`
  does (state cleared, `goalentity` set to world). [read] The print is dropped when the
  spectator's `msg` userinfo level is above 2, like any print. [read] Tracking yourself is
  impossible because you are a spectator. [derived]
- Success: the server stores "tracking slot N" (internally as N plus one, zero meaning nobody) and
  sets the spectator's `goalentity` to the tracked player's entity (entity number slot plus one).
  Nothing is sent back: no print, no message, no stat burst at that moment. [read] The effect is
  seen in the next frames' stats and player info (question 2). [derived]
- The server re-asserts `goalentity` every physics frame for every tracking spectator, and again
  whenever the spectator's entity is rebuilt (spawn, a map change's re-spawn), so a mod that
  writes `goalentity` on a tracking spectator has it overwritten each frame. When not tracking,
  the per-frame pass leaves `goalentity` alone. [read]
- The mod is not called back when `ptrack` arrives; it learns of tracking only through
  `goalentity` (it still gets its usual spectator think each frame). [read]

### 2. What changes in what the tracking spectator is sent

Visible set (PVS for entities, PHS/PVS for multicast):
- Entities (packet entities and nails) are chosen from the PVS at the SPECTATOR'S OWN origin plus
  its own view offset, never the tracked player's. [read] A spectator spawns with view offset
  (0, 0, 22), so the eye is its own origin plus 22 units up. [read] The visible set follows the
  tracked player only because the client keeps teleporting itself onto him (question 3 and 6).
  [derived]
- Multicasts (sounds, temp entities, muzzle flashes): the receiver's eye is likewise its own
  origin plus view offset. For the PHS kinds, anything within 1024 units of that eye is sent
  regardless of the PHS; otherwise the set is tested. [read] Tracking by `ptrack` does not change
  this. [read]
- Exception, the mod field `trackent` (a mod-defined entity field on the client's own entity; a
  value 1..32 naming a spawned client): when a mod sets it, the server uses the TRACKED player's
  origin plus HIS view offset for both the entity PVS and the multicast eye, and substitutes his
  stats and player record (see question 5). `ptrack` alone does none of this. [read]

Players (player info message, 42):
- The tracked player passes the visibility test unconditionally: the server sends him to his
  tracker even when he is outside the tracker's PVS, and even after he has turned spectator
  (spectators are otherwise never sent to anyone but themselves). He is still skipped when his
  client is not spawned. [read]
- The tracked player's record carries the usual "another player" fields: origin, frame, msec,
  the last command (view angles and moves, buttons and impulse always zeroed; a dead player's
  pitch zeroed and yaw replaced by his body yaw, roll left as commanded), velocity components that are non-zero,
  model, skin, effects. [read] Plus the weapon frame (flag bit 8, value 256) whenever his weapon frame is
  non-zero: this is the only place another player's weapon frame is sent. [read]
- Velocity: contrary to the summary in the brief, mvdsv sends non-zero velocity components of
  EVERY visible non-spectator player to every client, not only the tracked one. A spectator
  player's record (its own, or a tracked player who became a spectator) is cut down to origin,
  frame, the movement-type code, the on-ground and solid extension bits, and velocity only. [read]
- The spectator's own record is still sent to it (its own origin and velocity, movement-type code
  "spectator" 3, or "old spectator", or "fly", from its own movement type). [read]

Stats (stat update 3 / long stat update 38):
- Every stat is read from the TRACKED player's entity while tracking: health, weapon model index
  (stat 2), current ammo, armour, the four ammo counts, items (stat 15, with the level's sigil bits
  shifted into bits 28 and up, and the mod's extra items field, when present, shifted into bits 23
  and up), view height (stat 16, his view offset height; for a spectator it is sent even when the
  tracked player is dead). [read]
- The active-weapon stat (10) is filled only for a player or a tracking spectator; a free-flying
  spectator gets 0 there. [read]
- Sending rule: the server keeps, per client, the last value it sent for each stat and sends only
  the stats whose value differs, each as its own reliable message, every frame in which the
  reliable channel is free. Values 0..255 go as message 3 (one-byte value), others as message 38
  (four-byte value). [read] So switching targets sends exactly the stats that differ between the
  old and the new target, at once on the next frame; there is no "full resend" on a switch. [derived]
  The cache is cleared to zero at each spawn (connection and every map change), which forces every
  non-zero stat to be resent then. [read]
- When the tracked player disconnects the server keeps reading stats from that slot's (now
  unused) entity. [derived from question 4]

Messages that normally go only to the tracked player:
- Damage (19) and set angle (10) are written from the receiving client's OWN entity only: a
  tracker never gets the tracked player's damage flash or forced angles. [read]
- `sprint` (the mod's private print), `centerprint` and `stuffcmd` are mirrored to trackers only
  when the server setting `sv_specprint` (default 0) has the matching bit: 1 centre prints, 2
  prints, 4 stuffed text. [read] Each spectator also has a per-client mask, taken from its
  userinfo key `sp` when set and otherwise from `sv_specprint` at connect time. [read] For prints,
  the tracking spectator's own mask must have bit 2. For centre prints and stuffed text the source
  tests the TRACKED PLAYER's mask instead of the spectator's (an apparent mix-up; reproduced as
  read). [read] Both the classic mod VM and the second mod VM behave the same. [read]
- Sounds: no special case; a tracker hears what its own eye's PHS lets through. Voice chat to a
  tracker as if it were the tracked player exists in the source only as disabled code. [read]
- Team chat: a player's `say_team` reaches a tracking spectator when `sv_sayteam_to_spec` (default
  1) is on, the message is of the kind the server treats as forwardable to
  spectators (decided from the message text; not examined further), and the TRACKED player's team equals the speaker's (a non-tracking spectator is compared on its own
  team instead). [read]

Nails/projectiles: nothing tracking-specific; they follow the same PVS as other entities (the
spectator's own eye). [read]

### 3. Does the server move the tracker?

- No. The server never copies the tracked player's origin onto the spectator. The spectator moves
  by its own movement commands under its movement type (no-clip by default; the mod may change it).
  [read]
- The client moves itself with the teleport request (client message 6, "tmove": three coordinates
  in the connection's coordinate format, classically a 16-bit value in eighths of a unit). The
  server accepts it only from a spectator: it sets the spectator's origin to the three values and
  relinks it; from a player it is read and ignored. There is no range or validity check. [read]

### 4. Tracked player leaves, turns spectator, map changes

- Tracked player disconnects: the server does NOT reset the tracker. Nothing is printed. The
  tracker stops getting that slot's player info (the slot is no longer spawned) but keeps getting
  stats read from the slot's leftover entity, and its `goalentity` keeps pointing at that entity.
  [read for the absence of a reset and the skip; derived for the stale stats]
- Tracked player becomes a spectator (`observe`): not reset either; the tracker keeps receiving
  his cut-down spectator record (origin, frame, velocity) and his stats. [read]
- A new client taking the same slot is tracked from then on without any new `ptrack`. [derived]
- Recovery is the client's business: ezQuake notices the empty name or the spectator flag in the
  player's userinfo and unlocks or picks another target (question 6). [read]
- The tracker itself: becoming a player (`join`) clears tracking; becoming a spectator
  (`observe`, or being forced to spectate on connect) starts with no tracking; a fresh connection
  starts with none. [read]
- Map change: tracking is NOT cleared; the spectator's tracked slot carries across the map
  change, `goalentity` is re-pointed when its entity is rebuilt at spawn, and the stat cache is
  cleared so every stat is resent. Nothing tells the spectator that tracking survived. [read]

### 5. Related server commands

- No server command picks a player by name or user id, or cycles: `ptrack` with a slot is the only
  tracking command. Name and user-id lookup and cycling are client features (question 6). [read]
- The mod-side extension field `trackent` (question 2) is the server-driven alternative: the mod
  sets it to an entity number 1..32 of a spawned client and the server, for that client, uses the
  tracked player's eye for the entity PVS and multicasts, sends his stats, and replaces the
  client's own player record with the tracked player's data sent under the client's own slot
  number, with movement-type code "lock" (6) and that player's view angles as the command
  (forced view angles), and his weapon frame; the tracked player's own record is then not sent
  separately. A value outside 1..32 or naming an unspawned client is ignored. [read]
- Mod-side fields of the same family: `hideentity` (one entity a client is never sent) and
  `hide_players` (a client sees no other players). [read]
- Settings: `sv_specprint` (above), `sv_spectalk` (default 1), `sv_sayteam_to_spec` (default 1),
  `maxspectators`, `spectator_password`, `sv_spectatormaxspeed`. [read]

## Client side (ezQuake)

### 6. What the client sends and does while tracking

Commands a player sees: `track <userid | name>` (also `#n` for the n-th player in the
scoreboard's order), `track1`..`track4` and `trackteam` (multiview), `trackkiller` (tracks whoever
killed the tracked player, by user id), `autotrack` (runs a mod's `autotrack` alias when the mod
stuffed one, otherwise toggles `cl_hightrack`), settings `cl_chasecam` (default 1, "through the
eyes") and `cl_hightrack` (default 0, follow the top fragger). [read]

- `track` resolves the argument first as a user id, then as an exact player name (case-sensitive,
  then case-insensitive, then with colour codes stripped), then as `#n`. It refuses with
  `You must be connected to track`, `You can only track in spectator mode`, `Usage: track
  <userid> | <name>`, `track : no such player <arg>`, `track : no player with userid <n>`,
  `You cannot track a spectator`. [read]
- Locking on: each time the client locks onto a slot it queues, on the RELIABLE channel, a string
  command (message 4) `ptrack <slot>` with the 0-based slot. Unlocking (free fly) queues a bare
  `ptrack`. [read] It runs the player's alias `f_trackspectate` on a lock and `f_freeflyspectate`
  on an unlock, when they exist. [read]
- Repetition: `ptrack` is sent once per lock. After a lock the camera is not "settled" until the
  next frame finds a viewpoint; while it is unsettled the command builder locks again (another
  `ptrack`) on every frame. With the chase camera the viewpoint search wants some direction from
  the target where a trace ends 32..800 units away, so in open maps it settles on the next frame
  and `ptrack` goes once. [derived]
- Attack button: the first press while free turns tracking on (cycles to the first valid player
  from the last ideal target); the next press turns it off. While tracking, jump moves to the next
  valid player (slot order upward, wrapping at 32), and jump while holding move-down moves to the
  previous one. A valid player has a name and is not a spectator. With nobody valid it prints `No
  target found ...`. With `cl_hightrack` on, jump prints `cl_hightrack enabled. Unable to switch
  POV.` [read]
- Each frame while locked with `cl_chasecam` 1, in this order within the command build: the
  command's forward, side and up moves are zeroed; the client's view angles are set to the tracked
  player's angles from his latest player info (his command angles; for a dead player as the
  server rewrote them); the camera position is set to the tracked player's ORIGIN (no eye offset
  added here); when that differs at all from the recorder's own last received origin, a teleport
  request (message 6) with that origin is appended to the reliable channel and the client also
  moves its own predicted origin there at once. [read] Buttons are kept (that is how the camera
  is cycled). [read] The usual move command (message 3) then goes out carrying those zero moves
  and the tracked player's angles. [derived]
- With `cl_chasecam` 0 the client picks a fly-by point near the target (the best of twelve
  directions around his heading, 32..800 units), teleports there when more than 16 units away, and
  aims its view at the target. [read]
- The eye: the view origin is the predicted origin (equal to the teleported position) plus 1/16 on
  each axis, plus the view height: stat 16 when the server announced the view-height extension
  (mvdsv sends the tracked player's view offset, normally 22), otherwise a fixed 22; minus 16 when
  the tracked player's record says dead, plus 8 when it says gibbed. [read] The tracked player's
  model is hidden in first person; the weapon model is drawn only while tracking and locked with
  the chase camera; the weapon model comes from stat 2 and its frame from the tracked player's
  weapon-frame field. [read] View angles are interpolated between packets while tracking. [read]
- When the tracked player's name empties or he turns spectator, the client unlocks (bare
  `ptrack`) or, with `cl_hightrack`, picks the top fragger. [read]
- On a map change the client keeps whether tracking was on and the ideal target, but clears the
  current target; on the first frame it locks again on the ideal target or the next valid one and
  sends a fresh `ptrack`. Disconnecting, or the client's own spectator flag changing, resets
  tracking fully. [read for what is kept; derived for the re-send]

### 7. A client demo recorded while tracking

- The classic client demo records two record kinds, each preceded by a 4-byte little-endian float
  time and a type byte: a received server packet (type 1: a 4-byte length then the bytes as
  received) and each sent command (type 0: the command structure as stored in memory, then the
  client's three view angles as little-endian floats). [read] The classic command structure is 24
  bytes (msec byte, 3 pad bytes, three angle floats, three 16-bit moves, buttons byte, impulse
  byte); an ezQuake build with its antilag option adds a field, so the record size depends on the
  build. [derived from field sizes and 4-byte alignment; the build option read]
- The teleport requests and `ptrack` strings are outgoing reliable data and are not recorded.
  [read]
- What lands for the tracked view: the server's packets as received, so the tracked player's
  stats (health, armour, ammo, items, weapon model, active weapon, view height), his full player
  record with his weapon frame, and the recorder's own record whose origin is wherever the server
  last put it by the teleport requests (so it trails the tracked player by about a round trip).
  The recorded commands and the recorded view angles are the tracked player's angles as the
  client copied them. [derived] When recording starts mid-game the client first writes the
  connection's opening data, marking the recorder as a spectator by setting bit 7 (value 128) of
  the player-number byte, and writes the current non-zero stats (the tracked player's) as stat
  messages. [read]
- Playback needs to know, and the file does not say whom it tracked: ezQuake infers it on every
  recorded command while the recorder is a spectator. It locks onto the first non-spectator
  player present in the latest packet whose command pitch and yaw are both within 2 degrees of the
  recorded view angles and whose origin is within 200 units of the recorder's own origin on every
  axis; with no match for 0.3 seconds it falls back to free view. [read] While locked, playback
  takes origin, velocity and angles from the tracked player's record instead of the recorder's,
  hides his model, draws the weapon, and the status bar shows him. [read] A client without this
  inference would play the recording from the recorder's own (lagging) origin with the recorded
  angles: an approximately first-person view, without the weapon model if it hides it for
  spectators. [derived; no such client was checked]

### 8. What the server must do so a stock tracking client looks right

- Send the tracked player's stats to the tracker: weapon model index in stat 2 (the client draws
  the weapon from it), health, armour, ammo, the active weapon in stat 10 (ezQuake's multiview and
  hud read it), items in stat 15 (status bar icons and the powerup screen tint: quad, suit, ring
  and pentagram bits are read from this stat, ring also hides the weapon unless the player opts
  in), and view height in stat 16 when the view-height extension is announced. [read]
- Send the tracked player's weapon frame in his player record (flag bit 8) to the tracker; the
  weapon animation is otherwise frozen. [read]
- Send the tracked player's record even when he is outside the tracker's PVS, so the camera never
  loses him. [read for mvdsv's behaviour; derived that the client needs it, since it unlocks or
  flies by only from what it has]
- Send his command angles (the client copies them as the view) and his dead and gib flags (the
  client lowers or raises the eye from them). [read]
- Honour the teleport request from spectators and pick the tracker's entities and sounds from the
  spectator's own (teleported) eye. [read]
- Mark spectators with `*spectator` 1 in their userinfo: the client uses it to refuse tracking a
  spectator and to drop a target that turns spectator. [read]
- Powerup glow colours on the model are the player's effects bits, sent to everyone. The
  damage-flash tint is NOT given to trackers by mvdsv. [read]
- Team colours: a tracking ezQuake treats the tracked player's slot as "my teammate" for forced
  colours. [read]
