# The sequel's players' own sounds

This is a working note of clean-room facts about the sounds a player of the sequel makes: pain,
death, jump, landing, drowning, the water's sounds, and how a client finds a player's own (`*`)
sounds by its model. It was written by a reader who read id Software's GPL source release of
Quake II (3.21) for M18v4f, and contains no code. Each fact is marked **[read]**, **[measured]**
or **[derived]**. The sections are numbered after the questions they answered.

Conventions used below. Channels are the engine's per-entity sound channels: auto (0, never
replaces a playing sound), weapon (1), voice (2), item (3), body (4). A new sound on channel 1 to 7
of the same entity replaces whatever is playing on that entity and channel. Attenuation "normal"
is 1; "idle" is 2; "static" is 3; "none" is 0. Volume is 0 to 1. The server runs at 10 frames per
second (0.1 s a frame). Speeds are in map units per second. [read]

---

## 1. Every `*` sound the player's own code plays

All are played at volume 1, attenuation normal (1), no time offset. [read]

| Sound name | Event | Channel | Who plays it |
| --- | --- | --- | --- |
| `*pain25_1.wav`, `*pain25_2.wav` | took damage, health now below 25 | voice | server (sound message) |
| `*pain50_1.wav`, `*pain50_2.wav` | took damage, health 25 to 49 | voice | server |
| `*pain75_1.wav`, `*pain75_2.wav` | took damage, health 50 to 74 | voice | server |
| `*pain100_1.wav`, `*pain100_2.wav` | took damage, health 75 or more | voice | server |
| `*death1.wav` to `*death4.wav` | killed, not gibbed | voice | server |
| `*jump1.wav` | left the ground with the jump key held | voice | server |
| `*fall2.wav` | a hard landing (the "fall" event) | auto | client, from the entity's event |
| `*fall1.wav` | a very hard landing (the "fall far" event) | auto | client, from the entity's event |
| `*gurp1.wav`, `*gurp2.wav` | a drowning damage tick that does not kill | voice | server |

[read] That is the complete list: 17 names. No gasp, drown, splash, footstep, soft-landing or
lava-burn sound is a `*` sound; those are plain sounds (listed under 4 and 5). [read]

The two fall sounds are not sent as sound messages: the server sets an event on the player's
entity state and each client plays the sound itself when it sees the event, on that entity's
number, volume 1, attenuation normal, auto channel. [read]

## 2. Pain

- **When it is decided.** Once per server frame per player, at the end of the frame, from all the
  damage that player took during that frame (health damage plus what armour and power armour
  absorbed). If that total is zero, nothing plays. A hit fully absorbed by armour still counts,
  so it can still make a pain sound. [read]
- **Which level.** By the player's health after the frame's damage: below 25 gives the 25 set,
  below 50 the 50 set; below 75 the 75 set; otherwise the 100 set. [read]
- **Which variant.** `_1` or `_2`, an even random pick each time. [read]
- **How often.** At most once per 0.7 s: a pain sound plays only when the current level time is
  strictly later than 0.7 s after the last one. At 10 frames a second that is the 7th or 8th
  frame after, depending on floating-point rounding of the level time. [read for 0.7 s and the
  strict comparison; derived for the frame count]
- **The debounce is shared** with other player sounds that set the same "pain time": a
  falling-damage landing and a drowning tick set it to "now", which silences the normal pain sound
  in that frame only (they run before the pain decision in the same frame); a lava burn sound sets
  it 1 s ahead; the invulnerability "protect" sound (`items/protect4.wav`, item channel) sets it
  2 s ahead; the weapons' "out of ammo" click also uses it with a 1 s gap. [read]
- **Suppressed by:**
  - god mode (the cheat): no pain sound at all; [read]
  - an active invulnerability powerup: no pain sound; [read]
  - death: the hit that kills does not count toward the frame's damage total, nor does any hit
   on a body already dead, so a death or a corpse being shot makes no pain sound from those hits.
   [derived: a killing hit stops before it is added to the frame's total]
  - the team-damage rule (a hit refused as friendly fire adds nothing); [read]
  - under water: NOT suppressed by being under water as such; only the drowning tick itself
   replaces the pain sound for that frame. [read]
- **Edge case, derived.** If earlier hits in the same frame did not kill but a later one did,
  those earlier hits still count, so a pain sound can be issued in the same frame as the death
  sound. It goes out after the death sound on the same voice channel and so replaces it on the
  client. [derived]
- The pain animation and the screen flash use the same frame total; the sound is independent of
  whether the animation starts. [read]

## 3. Death

- **Normal death:** one of `*death1.wav` to `*death4.wav`, an even random pick of the four, voice
  channel, played once, at the moment of death (not repeated for further hits on the corpse).
  Crouched or standing changes only the animation, not the sound. [read]
- **Gibbed** (health below -40 after the hit): no `*` sound; the plain `misc/udeath.wav` plays on
  the body channel instead. The same happens later if a corpse is shot below -40. [read]
- **Drowned:** the death itself still plays a `*deathN` sound. The final drowning tick (the one
  whose damage is at least the remaining health) first plays the plain `player/drown1.wav` on the
  voice channel; the death sound then follows in the same frame on the same channel, so on the
  client the death sound replaces the drown sound. [read for both sounds; derived for which one is
  heard]
- Falling to death: the hard-landing event (fall sound) is set from the health before the damage,
  so a lethal fall gives both the fall sound and a death sound. [read]

## 4. Jump and landing

**Jump.** `*jump1.wav` plays when, over one movement command, the player was on the ground before
the move, is not on the ground after it, the jump key (up move) is held at 10 or more, and the
player is not in water at all (water level 0). Same in deathmatch, cooperative and single player:
no mode condition. [read] It is checked per movement command, not per server frame. [read]
Consequences: walking or being knocked off a ledge while holding jump also plays it; a jump made
from water does not. [derived]

**Landing.** Once per server frame the game measures the landing as a number D:
- if the player was moving down last frame, is now moving up relative to that (slowed) and is not
  on the ground, D comes from last frame's vertical speed,
- otherwise, only if the player is on the ground now, D comes from the change in vertical speed
  since last frame,
- in both cases D is that speed squared times 0.0001 (speed in units per second). [read]

D is then scaled by water: head under water (level 3) means no landing at all; waist deep
(level 2) multiplies D by 0.25; feet in water (level 1) by 0.5. Noclip, and a player not shown as
the player model (for example a gibbed head), skip it. [read]

| D | Sound | Damage |
| --- | --- | --- |
| below 1 | nothing | none |
| 1 to below 15 | a footstep event: one of `player/step1.wav` to `player/step4.wav`, random, body channel, only if the client setting `cl_footsteps` is on (default 1) | none |
| 15 up to and including 30 | the "fall short" event: plain `player/land1.wav`, auto channel | none |
| above 30, below 55 | the "fall" event: `*fall2.wav` | yes |
| 55 and above | the "fall far" event: `*fall1.wav` | yes |

[read] In landing speeds (no water): 100, about 387, about 548 and about 742 units per second for
D = 1, 15, 30, 55. [derived]

Above 30 the damage is (D - 30) / 2, truncated to a whole number, at least 1; armour applies to it
normally. The fall/fall-far event is set only if the player was alive
before the damage. The normal pain sound is silenced for that frame. In deathmatch the damage is
skipped when the server's `dmflags` has the "no falling damage" bit (8); the sound still plays.
[read] There is no upper cap on fall damage. [read]

Above 15 the view also dips: half of D, at most 40, over 0.3 s. [read]

Unrelated to landings, running on the ground faster than 225 units per second also sends footstep
events once per bob cycle (the same plain step sounds). [read]

## 5. Water

None of the water sounds below is a `*` sound except the gurps. [read]

- **Feet entering** any liquid (water level from 0 to anything): `player/lava_in.wav` for lava,
  `player/watr_in.wav` for slime or water; body channel. [read]
- **Leaving completely** (from any level to 0): `player/watr_out.wav`, body channel. [read]
- **Head going under** (to level 3): `player/watr_un.wav`, body channel. [read]
- **Air.** While the head is not under water, the air clock is reset every frame to 12 s ahead.
  With a rebreather or environment suit active and the head under water, it is reset to 10 s
  ahead every frame, so the player never drowns. Noclip also keeps the air full. [read]
- **Gasp on surfacing** (head from under water to above), voice channel:
  - more than 12 s under (air ran out): `player/gasp1.wav`; [read for "air ran out"; derived for
   12 s]
  - more than about 1 s but not out of air: `player/gasp2.wav` (the "just breaking surface"
   breath); [read: air time left under 11 s]
  - about 1 s or less: no sound. [derived]
  - Surfacing with a rebreather or suit active leaves about 9.9 s of air on the clock, so it plays
   `player/gasp2.wav`. [derived]
- **Rebreather / suit breathing** under water: `player/u_breath1.wav` and `player/u_breath2.wav`
  alternating, auto channel, every 25 frames (2.5 s), on frames a whole multiple of 25 away from
  the rebreather's end frame. [read]
- **Drowning.** Once the air is out (more than 12 s under), and while alive, about one tick a second (exact timing in 10):
  the drowning damage grows by 2 each tick, starting at 4 and capped at 15 (4, 6, 8, 10, 12, 14,
  15, 15, ...); it ignores armour. It resets to its start whenever the head is out of water.
  Each tick's sound, voice channel, volume 1, attenuation normal:
  - if the tick's damage is at least the current health: plain `player/drown1.wav`,
  - otherwise an even random pick of `*gurp1.wav` or `*gurp2.wav`.
  The tick silences the normal pain sound for that frame. [read] The gurp plays even in god mode,
  because the drowning sound is not gated by god mode (god mode only zeroes the damage). [derived]
- **Lava** (not water, but the same code): alive, not invulnerable and the shared pain time
  passed: plain `player/burn1.wav` or `player/burn2.wav` at random, voice channel, then the pain
  time is set 1 s ahead. [read]

## 6. How the client resolves `*name`

- **When.** At the moment a sound starts playing (a sound message, or a client-side event), if the
  sound's name begins with `*`, it is resolved against the entity number the sound is attached to.
  A `*` sound by itself is a placeholder that never loads any data. [read]
- **Which model.** The client reads the player-skin config string for slot (entity number minus 1).
  That string is "name\model/skin" as the server builds it from the player's `name` and `skin`
  userinfo. The model is the text after the first backslash, up to the first `/` (or to the end
  if there is no `/`). If the string is empty or the model comes out empty, the model is `male`.
  [read]
- **First try.** The file `players/<model>/<name without the star>` from the game's file system
  (pak files and folders), for example `players/female/pain25_1.wav`. Note this is under
  `players/`, not under `sound/`. If that file exists it is used. [read]
- **Fallback.** If it does not exist, the sound plays `sound/player/male/<name without the star>`
  (folder `player`, singular, under `sound/`), with no further check; if that file is missing too,
  the sound is silent. The fallback is not the model folder `players/male/`. The model's existence
  itself is not checked: a model the client cannot draw (and draws as male) still has its own
  folder tried first for sounds. [read]
- **Cache.** The result is remembered by the pair (model name, sound name), not by client slot:
  every player using the same model shares it, and a player who changes model simply resolves
  under the new name. A fallback is remembered as an alias under the model's name, so the
  existence check runs once per model and sound. [read] These remembered entries are dropped at
  the end of the next level load's sound registration (they are not re-registered then) and are
  resolved again on first use. [derived]
- **Loading.** When first resolved outside a level load, the file is read immediately, at the
  moment it is first played. [derived]
- **Not a player.** Any entity number works the same way: the config string read is the one at
  that number minus 1 in the player-skin block. For an entity that is not a player that slot is
  normally empty, so it plays the `male` set (with the `players/male/` folder tried first). Entity
  numbers past the player-skin block (above 256) would read into the next block of config strings,
  the game never plays a `*` sound on such an entity. [read for the lookup; derived for the
  consequences]
- **Auto-download.** The client's automatic download skips every sound whose name begins with `*`,
  so a model's own sounds are never downloaded from a server. [read]

## 7. How the server sends it

- The game registers the literal string, star and `.wav` included (for example `*pain50_2.wav`),
  as an ordinary sound config string; its sound index is what goes in the sound message. The
  server treats it exactly like any other sound: nothing special for `*` names. [read]
- The sound message always carries the entity number (16 bits: entity number times 8 plus the
  channel 0 to 7). That entity number is what the client uses to find the model. [read]
- The volume and attenuation bytes are omitted when they are 1 and 1, which is the case for every
  player sound; no position is sent for a player (the client uses the entity's own position). It
  goes unreliably to the clients in the potentially hearable set of the player's position. [read]
- The fall sounds are not sound messages at all (entity events, see 1). [read]

## 8. Precache

- Every level (worldspawn, in every mode) precaches all 17 names: `*death1.wav` to
  `*death4.wav`, `*fall1.wav`, `*fall2.wav`, `*gurp1.wav`, `*gurp2.wav`, `*jump1.wav`, and the
  eight `*pain<25|50|75|100>_<1|2>.wav`. The CTF game module precaches the same list. [read]
- The plain player sounds precached beside them: `player/gasp1.wav`, `player/gasp2.wav`,
  `player/watr_in.wav`, `player/watr_out.wav`, `player/watr_un.wav`, `player/u_breath1.wav`,
  `player/u_breath2.wav`, `misc/udeath.wav`, `player/fry.wav`, `player/lava1.wav`,
  `player/lava2.wav`. `player/land1.wav`, `player/drown1.wav`, `player/burn1.wav`,
  `player/burn2.wav`, `player/lava_in.wav` are not precached at level start: the game registers
  the server-sent ones on first use, and the client registers `player/land1.wav` itself. The client registers `player/step1.wav` to `player/step4.wav` itself. [read]
- The client does not preload any model's sound set when a player's userinfo arrives: on a
  player-skin config string it loads the model, skin, weapon models and icon only. The model's
  sounds are resolved and loaded lazily on first play (see 6). [read]

## 9. Model differences

- The game code picks every `*` sound the same way for every model: the names, thresholds,
  random picks and channels have no model or gender condition. [read]
- The only model-dependent part is the client's file lookup (6): the files in each model's
  `players/<model>/` folder, with the male fallback. So female and cyborg differences live only in
  the data files. [read]
- The `gender` userinfo key (values `male`, `female`, `none`; default `male`) changes only the
  wording of death messages, never a sound. The client setting `gender_auto` (default 1) sets
  `gender` from the `skin` model: `male` or `cyborg` gives male, `female` or `crackhor` gives
  female, anything else none, unless the player set `gender` by hand. [read]
- The client's `skin` setting defaults to `male/grunt`; a malformed userinfo is replaced by the
  server with name `badinfo` and skin `male/grunt`. [read]
- Which files the retail models actually ship in their folders (and whether
  `sound/player/male/` holds the full set) is in the game's data, not the source. [open]

## 10. Drowning damage, settled

- **Stored value when the air runs out: 2.** Every server frame in which the head is not under
  water, the stored drowning damage is set to 2 (and the air clock to 12 s ahead). While the head
  is under water but air remains, the stored value is not touched, so it is still 2 at the moment
  the air runs out. [read]
- **The +2 comes before the damage is dealt.** On each tick the stored value is raised by 2, then
  capped at 15, and only then dealt as damage (and compared with health to pick `player/drown1.wav`
  over a gurp). [read]
- **First three ticks: 4, then 6, then 8.** The full run is 4, 6, 8, 10, 12, 14, 15, 15, ...
  [derived from the two facts above] So the earlier note's "starting at 2" is the stored value,
  not the first damage dealt.
- **Edge case.** A player who comes into the world with the head already under water and never
  surfaces starts from whatever the stored value was left at (it is not reset when the player
  respawns): 0 on a never-used player slot, so a first tick of 2, or the previous life's value.
  [derived]
- **When the first tick comes: on the same frame the air runs out**, not one second later. The
  "next drowning tick" time is cleared to 0 when the player enters the world and is only ever set
  to one second after a tick, so it is always already in the past when the air runs out. [read for
  the clear and the one-second step; derived for "always in the past"]
- **When the air runs out.** The air clock is 12 s after the last frame the head was out; the air
  has run out on the first frame whose level time is strictly later than that. At 10 frames per
  second that is 12.1 s after the last frame with the head out (12.0 s if floating-point rounding
  of the level time falls the other way). [read for strict comparison; derived for the frame]
- **Later ticks.** Each tick sets the next one to 1 s later, also with a strict comparison, so
  ticks come every 1.1 s at 10 frames per second (1.0 s when rounding falls the other way), and
  only while health is above 0. [read for the 1 s and the strict test; derived for 1.1 s]

## 11. Entity events on the wire

- **Numbers** (the event byte in the entity state, an enumeration in this order) [read]:

| Number | Event | Set on | Client does |
| --- | --- | --- | --- |
| 0 | none | | nothing |
| 1 | item respawn | an item that reappears | `items/respawn1.wav`, weapon channel, volume 1, attenuation idle (2); respawn particles |
| 2 | footstep | the player | one of `player/step1.wav` to `player/step4.wav` at random, body channel, volume 1, attenuation normal, only when `cl_footsteps` is on |
| 3 | fall short | the player | `player/land1.wav`, auto channel, volume 1, attenuation normal |
| 4 | fall | the player | `*fall2.wav`, auto channel, volume 1, attenuation normal |
| 5 | fall far | the player | `*fall1.wav`, auto channel, volume 1, attenuation normal |
| 6 | player teleport | the player (and the teleporter it used) | `misc/tele1.wav`, weapon channel, volume 1, attenuation idle (2); teleport particles; no interpolation from the old position |
| 7 | other teleport | a non-player thing moved instantly (a monster sent along by a teleporting path corner, a train jumping to a teleporting path corner) | no sound and no particles; only no interpolation, the entity drawn at its new position at once |

- **On which entity.** The footstep, fall short, fall and fall far events are set on the player's
  own entity (its number is its client slot plus 1). [read] The player's own entity is always
  included in its own client's frame, and an entity with an event is sent even when it has no
  model, effect or sound; other clients receive it under the usual visibility rules. [read]
- **Player teleport (6)** is set on the player when it respawns in deathmatch or cooperative play,
  and on both the player and the teleporter's source entity when a player uses a teleporter.
  [read] It is not set on the first spawn into a level. [derived: only the respawn and the
  teleporter set it]
- **One frame only.** The event is set during a server frame, sent in that frame's snapshot, and
  the server clears every entity's event right after the snapshots are sent. So it lasts exactly
  one frame and is never repeated. [read] It is not delta compressed: it is written whenever it is
  nonzero, flagged by its own bit in the entity update; an absent byte means 0. [read] A client
  that loses that one packet never hears it. [derived]
- **One per entity per frame.** An entity carries only one event a frame. The landing events are
  set first; the running footstep (on the ground faster than 225 units per second, once per bob
  cycle) is set only when no other event was set that frame. [read]
- **Footstep with `cl_footsteps` off (0): nothing at all**, no sound and no other effect; the
  event is simply ignored. [read] The setting defaults to 1. [read] The other events have no
  client setting.
- **When the client fires them:** once per received frame, after the frame is parsed, for every
  entity in the frame that carries a nonzero event. [read]
