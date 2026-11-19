WORKING AREA: deleted when porting ends.
Read: ezQuake 5eb5920a (2026-11-18), mvdsv 70f11356 (2026-11-18, protocol constants at qwprot 83716187), KTX b1b23a98 (2026-11-08); KTX's shipped vwep models measured.

# Visible weapons (vwep) in the classic network game

Clean-room facts from a reader (the `clean-room` skill): what the modern client, the server and
the common server mod do so that other players' figures carry the weapon they hold. Each fact is
marked **read** (from the source), **measured** (from the shipped model files) or **derived**.
The numbered sections answer the questions the reader was given.

## 1. The per-player message (svc 42)

- **read** The value in the impulse byte is the player's `vw_index` field, converted from float
  to a byte with no offset and no range check on this path. It is the same number the builtin
  `precache_vwep_model` returned for that list entry: wire value i selects list entry i (0-based).
- **read** The server first forces the command's buttons and impulse to 0 for every player entry
  ("never send buttons or impulses"), then, when all three hold, puts `vw_index` in the impulse:
  the receiving client's `*z_ext` has bit 0x80, the list is not empty (at least one name was
  registered this level), and the loaded program defines a field named `vw_index`. So a client
  without the bit always gets impulse 0.
- **read** The impulse travels inside the delta-coded command (delta from an all-zero command):
  the command's bits byte has bit 6 (0x40) set and the impulse is one byte, only when the value
  is not zero. A zero value is simply absent.
- **read** 0 means "no view weapon": the client draws the ordinary body with its own model index
  (normally the standard player model) and nothing else.
- **read** Whether the command flag (bit 1 of the playerinfo flags) is set is decided per entry.
  Another player's entry, sent to a player or a spectator, always has it, so the view weapon
  travels; a spectator chasing someone receives that player's entry like any other, with the
  value. An entry describing a spectator keeps only its velocity bits: no command, no value. The
  receiver's own entry has the flag cleared (no value), except under the lock move type (forced
  view angles), when the command and so the value is sent. A mod's "track entity" field, which
  makes a client see another player as itself, turns that player's entry into the receiver's own
  (no value, unless locked).
- **read** On receipt, when the server's `*z_ext` (from the server info) has bit 0x80 and the
  entry's gib flag (bit 10 of the playerinfo flags) is clear, the client sets the player's view
  weapon to the command's impulse; otherwise to 0. This happens whether or not the command flag
  was in the message.
- **read / derived** The client does not clear the command when the flag is absent: the player
  state lives in a ring of 64 frames that is not wiped per frame, so the impulse read is whatever
  that slot held before. For the entries that never carry a command (the receiver's own, a
  spectator's) the view weapon the client computes is therefore stale, not 0. There is no
  "keep the last value" rule; it is a side effect.
- **read** Under FTE's replacement-delta extension (the FTE second extension word, bit 0x08)
  players travel as entity deltas instead of svc 42, and the view weapon is the entity's
  second-model-index field (bit 27 of the update-field word; one byte, or a 16-bit short when the
  "16-bit" bit 9 is set because some index or frame exceeds 255). The server fills it for every
  player entry including the receiver's own, only when 0 < `vw_index` < 32 (else 0), and only when
  the list is not empty and the field exists; it does NOT check the receiver's bit 0x80 there.
  The client takes a value below 32 as the view weapon and anything else as 0. There is no gib
  test on this path.

## 2. The `//vwep` line

- **read** Sent by the server as a reliable stuffed text (svc 9) immediately before the reply to
  the client's model-list request with start index 0, i.e. once per sign-on: every level change
  and every connect or reconnect. Only when the client's `*z_ext` has bit 0x80 and the list has at
  least one name.
- **read** Content: `//vwep ` then the names separated by single spaces, then a newline. Names are
  taken in list order up to the first empty slot, at most 32 (the list's size). Each name is
  shortened: a leading `progs/` is removed when no further `/` follows it, and a trailing `.mdl`
  is removed when it is the only `.` in the name; anything else is sent as registered.
- **read** The line is built in a 1024-byte buffer; if it does not fit (the final newline was cut
  off) it is not sent at all.
- **read** The server's list is wiped at each level start and filled only by the builtin during
  loading; the builtin does not de-duplicate (the same name twice takes two slots).
- **read** The client recognises a stuffed text that starts with `//vwep ` and ends with a
  newline; it is handled internally, not run as a console command. It is split into words by
  the console's ordinary tokenizer (white space). The first 32 names are kept, extra ones are
  ignored.
- **read** Per name: `-` is kept as the empty-entry marker. A name containing `..` or starting with
  `/` or `\` is a fatal error (the client drops the connection with an "illegal model name"
  error). A name containing `/` is used as a full path; otherwise `progs/` is put in front. If the
  name has no `.`, `.mdl` is appended.
- **read** A line with no names at all turns the feature off, at any time. A line with names
  received while the client is fully in the game (active) is ignored with a developer message:
  the feature can be turned off mid-game but not on. Received during sign-on it overwrites the
  first N entries (the client's level state, list included, is wiped when the level's server data
  arrives).
- **read** Entry 0 is special: it is the body model drawn in place of the standard player model
  whenever a view weapon is drawn (see 4). `-` as entry 0 means "draw no body, the weapon model
  is the whole figure".
- **derived** Since wire value 0 means "no view weapon", entry 0 (the body) can never be selected
  as a weapon; weapons are entries 1 to 31.

## 3. Loading the models

- **read** After the ordinary models are loaded, the client checks the list unless the server's
  `*z_ext` lacks bit 0x80, or the list is empty (both checks skipped in multiview demo playback),
  or `cl_novweps` is non-zero; in those cases it goes straight on to the next sign-on step.
- **read** Path: as in 2 (`progs/<name>.mdl` for a bare name). `-` entries are skipped.
- **read** Each listed file goes through the same "have it or download it" check as any model:
  if missing and downloads are allowed, it is requested from the server, one at a time, before
  sign-on continues. No download during demo playback (except a relay connection that offers
  downloads).
- **read** Then each file is loaded; a missing or unloadable file is silently left out (no
  message).
- **read** The feature is turned on for the level only if entry 0 is `-` or entry 0's model
  loaded. If the body model is missing, the feature is off entirely for that level.
- **read** A missing weapon model (feature on): that player is drawn with the standard body
  (the player's own model index), no weapon, as if the value were 0.

## 4. Drawing

- **read** A player gets a view weapon when: the feature is on for the level, `r_drawvweps` is
  non-zero, the player's view weapon is non-zero, and the player's model is not the invisibility
  eyes model. The figure is not drawn at all when the player's model index is 0.
- **read** Then the weapon is added as a second model, and the body is drawn with list entry 0's
  model in place of the player's model (or no body when entry 0 is `-`). If the weapon entry is
  `-`, no weapon is added but the body is still swapped to entry 0 (a weaponless figure). If the
  weapon index is 32 or more, or its model did not load, nothing is swapped: the ordinary body is
  drawn.
- **read** The weapon copies from the body: origin, angles (the body's pitch is the view pitch
  divided by -3, yaw is the view yaw), frame number, the body's interpolation fraction, the
  effects bits (so powerup shells are drawn on the weapon too), the "behind wall" x-ray flag and
  the scale. Its skin is always 0. It gets no player colour translation and no custom player
  skin (those are applied only to the body). It is lit by the same rules as player models. Its
  alpha is not copied from the body (it starts opaque); the one exception is a racing mode
  where both fade by distance. The outline mode colours a weapon's outline in the player's top
  colour (the body's is split top and bottom by texture region).
- **read** Frame interpolation: when the body's frame changes, the weapon's previous frame is the
  body's previous frame if the view weapon index is unchanged since the last update; if the index
  changed, the previous frame is set to the new frame, so the weapon does not blend across the
  switch.
- **read** Gibs: a classic-path entry with the gib flag gets view weapon 0. Eyes: excluded at
  draw time. Dead players: drawn like the living, weapon included, if the index is not 0 (the
  server decides; see 6 for what the common mod sends).
- **read** In multiview demo playback only: a player drawn with the standard player model whose
  frame is a death frame (frames 41 to 102) is drawn with entry 0's body when it loaded and
  `r_drawvweps` is on, even with view weapon 0.
- **measured / derived** Frames 41 to 102 are exactly the death sequences in the shipped body
  model's frame list (`axdeth1` is frame 41, `deathe9` is frame 102).

## 5. Client settings and the local figure

- **read** `r_drawvweps`, default 1. 0: never draw view weapons (and no body swap); non-zero:
  draw them. Read every frame, so a change takes effect at once.
- **read** `cl_novweps`, default 0. Non-zero: the connect request's `*z_ext` is sent without bit
  0x80 (so the server sends neither the line nor the values), and the model check of 3 is skipped
  (feature off, also in demo playback). The first effect takes hold at the next connect, the
  second at the next level's sign-on.
- **read** Neither carries a flag that keeps it out of the saved configuration: the client's
  config writer saves them when they differ from the default (or always, with
  `cfg_save_unchanged 1`).
- **read** The local player's figure is drawn only in the third-person cameras (for example
  `cl_camera_tpp`), and only when not a spectator; then it goes through the same view-weapon
  test as anyone.
- **derived** Live on the classic path the server does not send the own entry's value (1), so the
  own figure's weapon comes from a stale slot: not reliable. Under replacement deltas the own
  value is sent and the own figure gets the right weapon. In a single-view client demo of
  oneself the own entry likewise has no command (stale). In multiview playback every player's
  weapon is derived from stats (8), so tracking oneself shows the right weapon.

## 6. The common modern server mod (KTX)

- **read** Settings: `k_allow_vwep` (default 0) and `k_vwep` (default 1). The models are
  registered at level start only when `k_allow_vwep` is non-zero; the feature is in effect when
  `k_allow_vwep` and `k_vwep` are both non-zero. The default 0 is set by the mod in its first
  frame, after the first level's spawn; KTX's example server configs do not mention either
  setting. So the feature is OFF by default; a server turns it on with `k_allow_vwep 1` (it takes
  effect from the next level load) and off with `k_allow_vwep 0`.
- **read** A player command `vwep` toggles `k_vwep` (with the usual setting-changed message),
  only before a match (refused while a match is in progress or the countdown runs) and only when
  `k_allow_vwep` is on; it then re-applies the weapon to every player at once. The match start
  announcement lists "VWep on" when it is in effect (not in race mode).
- **read** Registered in this order (indices are what the builtin returns):
  0 `progs/vwplayer.mdl` (the body), 1 `progs/w_axe.mdl`, 2 `progs/w_shot.mdl`,
  3 `progs/w_shot2.mdl`, 4 `progs/w_nail.mdl`, 5 `progs/w_nail2.mdl`, 6 `progs/w_rock.mdl`,
  7 `progs/w_rock2.mdl`, 8 `progs/w_light.mdl`, then, only when `k_instagib_custom_models` is
  set, 9 `progs/w_coil.mdl`, and last `-` (index 9, or 10 with the coil). So the line normally
  reads `//vwep vwplayer w_axe w_shot w_shot2 w_nail w_nail2 w_rock w_rock2 w_light -`.
- **read** The value is set whenever the current weapon is applied (every weapon switch, spawn,
  respawn, and the other moments the mod refreshes the weapon): axe 1, shotgun 2, super shotgun
  3, nailgun 4, super nailgun 5, grenade launcher 6, rocket launcher 7, lightning gun 8; the grapple
  hook 1 (the axe). No weapon: 0. The instagib shotgun, even with its custom coil view model, still
  sets 2; nothing ever sets the coil's index.
- **read** Forced to 0: whenever the feature is not in effect, and while the invisibility ring
  is active (the player is drawn as eyes); restored when the ring runs out.
- **read** Death: set to 9 ("null" entry), when the feature is in effect; the client then draws
  the weaponless body. Gibbing: set to 0. Clan arena and arena ghosts and waiting players: 0.
- **derived** With the coil registered, index 9 is `w_coil`, not `-`, so corpses would carry the
  coil model: a mismatch in the mod.
- **read** Intermission: the mod does not touch the value (nothing assigns it there).
- **read** The mod does not query the server for the extension; it assumes it is present.

## 7. The weapon models

- **measured** KTX ships `vwplayer.mdl`, `w_axe.mdl`, `w_shot.mdl`, `w_shot2.mdl`, `w_nail.mdl`,
  `w_nail2.mdl`, `w_rock.mdl`, `w_rock2.mdl`, `w_light.mdl` and `w_coil.mdl` in its example game
  folder's `progs/`. All are MDL version 6 (`IDPO`), one skin, synchronised frames (sync type 1),
  no flags, 143 single frames each. All ten have the same 143 frame names in the same order,
  starting `axrun1`..`axrun6`, `rockrun1`..`rockrun6`, `stand1`..`stand5`, `axstnd1`..`axstnd12`,
  `axpain1`..`axpain6`, `pain1`..`pain6`, `axdeth1`..`axdeth9`, `deatha1`..`deatha11`,
  `deathb1`..`deathb9`, `deathc1`..`deathc15`, `deathd1`..`deathd9`, `deathe1`..`deathe9`,
  `nailatt1`, `nailatt2`, `light1`, `light2`, `rockatt1`..`rockatt6`, `shotatt1`..`shotatt6`,
  `axatt1`..`axatt6`, `axattb1`..`axattb6`, `axattc1`..`axattc6`, `axattd1`..`axattd6`.
  All have eye position (-6, 0, -24).
- **measured** Sizes as vertices, triangles and skin: vwplayer 149, 294, 296 x 194. w_axe 16,
  28, 296 x 194. w_shot 53, 86, 356 x 109. w_shot2 61, 98, 324 x 116. w_nail 139, 236, 560 x 90.
  w_nail2 153, 262, 400 x 69. w_rock 111, 190, 424 x 186. w_rock2 111, 190, 680 x 154. w_light
  137, 226, 488 x 118. `w_coil.mdl` is byte-identical to `w_shot.mdl` (same SHA-256).
- **derived** The client draws the weapon with the body's frame number, so the scheme requires
  one weapon pose per body frame, same count and same order as the body. The shipped files meet
  that: every weapon has the body's 143 frames, frame for frame.
- **derived** The body's frame numbers are set by the server for the standard player model and
  the client uses the standard death-frame numbers on the swapped body, so `vwplayer.mdl` must keep
  the standard player model's frame numbering. That the shipped file has 143 frames with these
  names is measured; how its mesh and skin compare to the standard player model (for example the
  gun's triangles removed) cannot be settled here: the standard model is not in these trees.
  Note `w_axe` shares the body's skin size (296 x 194).
- **read** No licence or credit statement for the models appears in any of the three trees; the
  KTX tree as a whole is GPL-2.0. Their licence is open.

## 8. Recordings and relays

- **read** Single-view client demos (recorded by the client): when the feature was on and the
  list not empty, the recorder writes the `//vwep` line (names shortened as the server does,
  only if it fits under 1023 characters) as a stuffed text in the demo's start, just before the
  model list. Each player entry is re-written from what was parsed, including the delta-coded
  command with its impulse, so playback gets the view weapon through the same impulse path (with
  the server info's `*z_ext` recorded in the demo, as in 1).
- **read** Multiview demos (server recordings): the server writes no `//vwep` line and no
  per-player view-weapon value; the multiview player record has no command.
- **read** On multiview playback the client ignores what the demo says and uses a fixed list,
  `vwplayer w_axe w_shot w_shot2 w_nail w_nail2 w_rock w_rock2 w_light` (entries 0 to 8). For each
  player drawn with the standard player model and not gibbed, the view weapon is derived from that
  player's weapon stat (the precache index of the first-person weapon model): `v_axe` 1,
  `v_shot` 2, `v_shot2` 3, `v_nail` 4, `v_nail2` 5, `v_rock` 6, `v_rock2` 7, `v_light` 8, any other
  0. Dead standard bodies get the swapped body (4).
- **derived** The relay (TV) stream is the multiview format and the client plays it as
  multiview playback, so the same fixed list and stat rule apply. A mod's own weapons (for example
  the coil) show no view weapon there.

## 9. Impulse versus view weapon

- **read** The client does not try to tell them apart; it trusts the server's advertised
  extension bit (the server info's `*z_ext`, not what the client itself asked for). The servers
  never send a real impulse in this message (it is always zeroed first), so with the bit the byte
  can only be the view weapon.

## 10. Edge cases

- **read / derived** Names listed but no index ever set: the client loads (and may download) the
  models, every value is 0, everyone is drawn with the standard model. With KTX's `k_vwep 0` this
  is exactly the case, and even death sends nothing.
- **read** An index past the list but below 32: the entry is empty, nothing loaded, the ordinary
  body is drawn alone. 32 or more: same on the classic path; under replacement deltas the server
  never sends it and the client maps it to 0.
- **read** More than 32 registrations: the builtin raises a run error (the server's program stops
  with an overflow). The client keeps at most 32 names from any line. A line over 1024 bytes is
  never sent: then the client never learns a list, the feature stays off, values arriving in
  impulses are not drawn.
- **read** Entry 0 missing on the client: the feature is off for the level although values still
  arrive.
- **read** A list whose entry 0 is `-`: weapons are drawn with no body at all.
- **read** A `-` weapon entry: weaponless body (entry 0).
- **read** A client with `cl_novweps 1` on a replacement-delta server still receives the values
  but draws nothing (the model check was skipped).
- **derived** Index changes are not interpolated (4), so a weapon switch pops the weapon pose to
  the new frame.
