# QuakeWorld deathmatch rules: the stock game program

**A WORKING NOTE** (README.md's rules). A reader session (2026-11-21, the `clean-room` skill) read
the stock multiplayer game program's GPL release and answered our questions in its own words, with
no code. Built from it: the arena's Quake rules (`games/shooters_edge/rules`, `rules q1`, M18x),
the gap list `q1-qw-gaps.md`. The modern match mod (`messages.md`, `deathmatch4.md`) changes some
of these behind its own settings; where it does, its notes win.

Read: id Software's Quake release source, GPL v2, as published on GitHub (default branch,
commit bf4ac424). It holds the QuakeWorld tree (its game program's QC sources, and a second copy
of them that differs only in licence headers) and the QuakeWorld client and server. It does NOT
hold the single-player Quake game program's QC sources, so every "how does Quake's own
progs.dat differ" part below is open unless stated otherwise.

Marks: **[R]** read directly from the source; **[D]** derived by working out what the source
does (arithmetic, combining two places); **[M]** measured (none in this document: nothing was
run). Units: distances in map units (u), speeds in u/s, times in seconds of server game time.

Some engine facts used below [R]: the program's random number is uniform on [0, 1] *inclusive*
of both ends (a 15-bit integer divided by 32767); its round-to-nearest rounds halves away from
zero; a number printed as text is shown as an integer when whole, otherwise with one decimal.
Player hull is 32 x 32 x 56 u, from 24 u below the origin to 32 u above it, so the box centre is
4 u above the origin [R]. Channels named below are the sound channels a mod sees: voice, body,
weapon, item, auto (auto never cuts another sound) [R]. "ATTN_NONE" sounds are heard map-wide,
"idle" attenuation is the quietest of the attenuated ones [R].

The per-command order for a living player [R]: the pre-think (rules check, liquids, then death
handling or jumping, then the empty-weapon switch), then the player's animation think, then the
movement code, then the post-think (landing sounds, powerup timers, weapon firing and
impulses).

---

## The player in liquids

### 1. Entering and leaving water, slime and lava

- The liquid check runs once per player command, in the pre-think, before movement; it reads
  the liquid level and type left by the previous move [R]. Liquid level as the movement code
  sets it [R]: 1 if the point 1 u above the hull's bottom (origin z − 23) is in liquid; 2 if also the hull's vertical centre (origin z + 4) is,
  3 if also origin z + 22 (eye height) is. Liquid type is that of the lowest sampled point.
- Entry is when the level becomes 1 or more while the player's "in liquid" flag is clear [R].
  The sound, on the **body** channel, volume 1, normal attenuation [R]:
  - lava: `player/inlava.wav`
  - water: `player/inh2o.wav`
  - slime: `player/slimbrn2.wav`
  The flag is then set, so the sound plays once per entry [R].
- Leaving is when the level returns to 0 with the flag set: `misc/outwater.wav`, body channel,
  for any liquid; the flag is cleared [R].
- No liquid sounds at all for a dead player (health below 0) or a noclipping one: the whole
  liquid check is skipped [R]. (Health exactly 0 is not skipped [D].)
- Swimming sound: while jump is held and the level is 2 or more, `misc/water1.wav` or
  `misc/water2.wav` (50/50) on the body channel, at most once per second [R]. Jump is ignored
  while a water-jump is in progress [R].
- Landing in water from a fall (see 9): `player/h2ojump.wav`, body channel, instead of the
  landing sound and instead of falling damage, when the liquid type at landing is water [R].
- Splash sound for thrown objects (grenades, gibs, backpacks) crossing a water surface is played
  by the server's physics, `misc/h2ohit1.wav`; not for players [R].

### 2. Lava and slime damage

- Lava: 10 × liquid level per tick (10, 20 or 30), one tick every 0.2 s; with the biosuit one
  tick every 1 s, same amount [R].
- Slime: 4 × liquid level per tick (4, 8 or 12), one tick every 1 s; none at all while the
  biosuit is worn [R].
- The damage comes from the world (no attacker), goes through armour like any damage, and is
  stopped by the pentagram [R]. Quad never applies to it [D].
- The damage clock is reset to "now" on every entry (the in-liquid flag going from clear to
  set) [R]. Because the damage test runs before the entry handling in the same command, entering
  lava or slime gives one tick on the entry command (if the old clock had run out, which it has
  unless the player left less than a tick ago) and a second tick on the very next command, then
  the normal interval [D]. Jumping in and out therefore restarts the clock each time [D].
- No liquid damage at level 0, and none for water [R].

### 3. Drowning

- Air: 12 s, counted from the last command in which the eyes (level 3) were not under any
  liquid [R]. The air clock is refreshed to 12 s every command while level is below 3, and
  every command while the biosuit is worn, whatever the level [R]. Slime and lava drown you too
  (level 3 in any liquid) [D].
- When air runs out at level 3: one damage hit per second (the hit clock is shared with the pain
  clock: a hit happens only if the last pain or drown hit was more than its interval ago; each
  drown hit sets it 1 s ahead) [R]. The hit amount starts from 2 and grows by 2 before each hit,
  when it would exceed 15 it wraps to 10 [R]. Sequence of hits: 4, 6, 8, 10, 12, 14, 10, 12, 14,
  10, … [D]. The amount resets to 2 whenever the eyes come out [R]. It is world damage, through
  armour [R].
- Gasps on surfacing (the first command with level below 3), voice channel, volume 1, normal
  attenuation [R]:
  - air had already run out: `player/gasp2.wav`
  - air had not run out but less than 9 s was left (under for more than 3 s): `player/gasp1.wav`
  - under 3 s or less: no sound [D].
  A new life starts with full air, so no gasp at spawn [R].
- Drowning pain sound: `player/drown1.wav` or `player/drown2.wav` (50/50), voice channel, with
  one bubble sprite (`progs/s_bubble.spr`) rising from 24 u above the origin, whenever the
  player takes damage of any kind while the liquid type is water and the level is 3 [R]. It is
  not held back by the pain clock (see 5) [R].
- Drowning death (dying at level 3, from any cause): `player/h2odeath.wav`, voice channel, heard
  map-wide, plus five bubbles, one each 0.1 s while the body is still at level 3 [R].
- Drown obituaries: see 8 (they come from the liquid type at death, not from the cause) [R].

### 4. Velocity damping in liquids

- Applied by the movement code shared by the server and the client's prediction (not by the game
  program) [R].
- At liquid level 2 or 3, each move: drop = speed × waterfriction × level × frame time, where
  speed is the full 3D speed; new speed = max(0, speed − drop); all three components are scaled
  by new speed ÷ speed [R]. This replaces ground friction (ground friction and its edge doubling
  are not applied at level 2 or 3) [R]. At level 1 ordinary ground friction applies if on
  ground [R].
- waterfriction is the server setting `sv_waterfriction`, default 4, sent to clients with the
  other movement settings [R]. Frame time is the command's duration, capped at 0.1 s [R].
- Friction is skipped entirely during a water-jump [R]; a speed below 1 zeroes the horizontal
  velocity and stops [R].
- Related, same code [R]: swimming wish speed is capped at the max speed (`sv_maxspeed`, 320) and
  then scaled by 0.7; with no forward, side or up input the wish is 60 u/s downward,
  acceleration uses `sv_wateraccelerate` (default 10). Holding jump at level 2+ sets vertical
  velocity to 100 (water), 80 (slime) or 50 (lava).

### 5. Pain sounds while burning

- Any damage while the liquid type is slime or lava (any level, including only feet in it)
  plays `player/lburn1.wav` or `player/lburn2.wav` (50/50), voice channel, normal
  attenuation [R]. Water at level 3 plays the drown sounds instead (see 3) [R].
- These liquid pain sounds are chosen before the pain clock is consulted, so they are NOT
  limited by it: every damage tick plays one [R]. The pain clock (0.5 s) only limits the
  ordinary pain sounds [R].
- But every pain sound is played only when the pain animation starts, and the pain animation
  is refused while the player's weapon is mid-fire animation (any firing frame) and while the
  Ring is active [R]. So a player who is firing, or invisible, makes no pain sound at all,
  burning or not [D]. No pain sound once health is below 0 [R].
- Ordinary pain (not in liquid): if hit by an axe, `player/axhit1.wav`; otherwise one of
  `player/pain1.wav` … `pain6.wav` [R]. The pick rounds 1 + 5 × random to the nearest whole, so
  pain1 and pain6 each come 1/10 of the time and pain2–pain5 each 1/5 [D]. A telefrag pain
  plays `player/teledth1.wav` map-wide [R].

---

## A player's life

### 6. Spawning

- Spot choice [R]:
  1. A `testplayerstart` entity, if the map has one, always wins.
  2. Otherwise every `info_player_deathmatch` is a candidate; a spot is "free" when no live,
     solid entity whose class is player has its box centre within 84 u of the spot (the radius
     search skips non-solid entities, so corpses and dead players do not block a spot) [R], [D].
  3. If any spot is free, one free spot is picked at random; if none is free, one of all spots
     is picked at random [R].
  4. The pick rounds random × (count − 1) to the nearest whole index, so the first and last
     candidate in the list are each half as likely as each of the others [D]. (With one
     candidate it is always that one.)
  5. `info_player_start`, `info_player_coop` and `info_player_start2` are never used for
     spawning; a map with no deathmatch spot spawns everyone at the world origin [D].
- The player is placed 1 u above the spot, facing the spot's angles (view snapped), velocity
  zeroed, view height 22 u, health 100, max health 100, air 12 s [R].
- Effect and sound [R]: a teleport flash (the temporary teleport effect, sent to everyone who
  can hear the spot) 20 u in front of the spawn point; 0.2 s later a random one of
  `misc/r_tele1.wav` … `misc/r_tele5.wav` (each 1/5), voice channel, from that point.
- Telefrag at spawn [R]: a trigger the size of the player's box grown by 1 u on every side
  (34 × 34 × 58 u) is placed at the spawn point for 0.2 s, and every entity is forced to re-touch
  triggers this frame. Anything touching it with non-zero health other than the spawner takes
  50000 damage from it (it always gibs a player, see 7). The spawner gets the frag (+1) and the
  victim's obituary is "was telefragged by" (see 8). Special cases with the pentagram [R]:
  - victim invulnerable, spawner not: the spawner dies instead ("Satan's power deflects …'s
     telefrag", spawner −1),
  - both invulnerable: both lose invulnerability and both die, each credited to the other's
     "Satan's power", each −1.
  The same trigger is placed at a teleporter's destination (see 22).
- Items at spawn, deathmatch 0–3 [R]: on every respawn after a death or `kill`, the default
  load-out: axe and shotgun, shotgun selected, 25 shells, no other ammo, no armour, 100 health.
  On the first spawn of a map the player carries over what they had when the previous map ended
  (if alive then): weapons, ammo, armour; health clamped into 50–100; shells raised to at least
  25; keys and all powerups removed. A player dead at the change, or new to the server, gets the
  default load-out [R], [D]. (In QuakeWorld the server asks the program for these carried values
  at every map change [R].) On entering the map named `start` with episode progress set, the
  default load-out is forced [R].
- deathmatch 4 [R]: 250 health, red armour 200 at 0.8, the pentagram for 3 s (its "almost burned
  out" warning fires at once), all ammo 255 and the nailgun, super nailgun, super shotgun,
  rocket launcher and lightning gun (no grenade launcher); if the server key `axe` is non-zero,
  no extra weapons and 0 shells instead (axe and shotgun only).
- deathmatch 5 [R]: 200 health, red armour 200 at 0.8, the pentagram for 3 s, every weapon,
  30 shells, 80 nails, 10 rockets, 30 cells.
- Each spawn also reads the server key `rj` and, if it is non-zero, adopts it as the rocket-jump
  multiplier (see 18); a later 0 does not reset it [R], [D].

### 7. Death

- Health is clamped to −99 at death [R]. Gib when health is below −40 after the clamp [R].
- Death order [R]: invisibility is cleared; then the server keys `dq` / `dr` may drop a
  carried quad / ring (see 14); all powerup timers are zeroed; the backpack is dropped; the
  weapon view is cleared; the view drops to 8 u below the origin; the body becomes non-solid and
  falls as a tossed object; if its vertical speed was below 10 it gets 0–300 u/s extra upward.
- Gib: the player becomes a bouncing head (`progs/h_player.mdl`), three gibs are thrown
  (`progs/gib1.mdl`, `gib2.mdl`, `gib3.mdl`, each removed after 10–20 s); thrown speed grows with
  how far below zero health went (×0.7 above −50, ×2 above −200, ×10 below) [R]. Sound, voice
  channel, map-wide: `player/gib.wav` or `player/udeath.wav` (50/50), or `player/teledth1.wav`
  for a telefrag [R].
- Not gibbed: death sound, voice channel, map-wide [R]:
  - eyes under liquid (level 3): `player/h2odeath.wav` plus 5 bubbles,
  - otherwise one of `player/death1.wav` … `death5.wav`, picked by rounding 1 + 4 × random:
     death1 and death5 1/8 each, death2–4 1/4 each [D].
  There is no separate lava or slime death sound [R].
- Death animation: holding the axe always plays the axe death (9 frames); otherwise one of five
  is chosen by 1 + floor(6 × random), with values 5 and 6 both giving the fifth, so four
  animations at 1/6 and the fifth at 1/3 [R], [D]. The cvar `temp1`, when non-zero, forces the
  choice [R]. Frames are 0.1 s apart; the five run 11, 9, 15, 9 and 9 frames, so the body is
  "fully dead" 1.0, 0.8, 1.4, 0.8 or 0.8 s after death (axe: 0.8 s) [D]. A gibbed player is fully
  dead at once [R].
- Respawn rule [R]: once fully dead, the player must first have all of attack, jump and use
  released (on some command), and then press any one of them; that respawns. There is no
  minimum time beyond the animation and no automatic respawn. While dead and on the ground the
  body's speed is reduced by 20 u/s per command [R].
- The corpse [R]: at respawn the dead body (model, frame, angles, colour map, movement type,
  velocity, size) is copied into a ring of 4 corpse entities, so the last four corpses stay in
  the map; corpses take no damage. A gibbed player leaves the head.
- Backpack [R]: dropped at every death through damage if the player has any ammo at all (the four
  kinds summed are non-zero); none if all ammo is 0. It holds all four ammo counts and the
  currently selected weapon (even the axe or shotgun). It is thrown from 24 u below the origin at
  300 u/s up and −100 to +100 u/s on each horizontal axis; box 32 × 32 × 56 u; removed after
  120 s. The `kill` command and disconnecting drop no backpack (they do not go through death) [D].
  Picking it up: see 15.

### 8. Obituaries and frag changes

All are broadcast at medium print level. "V" is the victim's name, "K" the killer's [R].
Checked in this order:

1. deathmatch above 3, victim killed by their own lightning-gun discharge self-kill (see 16):
   "V electrocutes himself." (followed by a newline and a stray space); V −1.
2. Telefrag: "V was telefragged by K"; K +1 (V unchanged).
3. Telefrag onto an invulnerable player: "Satan's power deflects V's telefrag" (V is the one who
   teleported in and died); V −1.
4. Both invulnerable: "V was telefragged by K's Satan's power"; V −1 (printed for each of the
   two).
5. Crushed (a door, plat or train blocked by the player):
   - teamplay non-zero, same non-empty team, not self: "K squished a teammate"; K −1,
   - K is a player other than V: "K squishes V"; K +1,
   - otherwise: "V was squished"; V −1.
   The "killer" of a door crush is whoever opened the door [R].
6. Killer is a player:
   - Self: V −1 and "V tries to put the pin back in" (own grenade), "V becomes bored with life"
     (own rocket); otherwise, if V was holding the lightning gun at liquid level 2+:
     "V discharges into the slime" / "… into the lava" / "… into the water." (water only has the
     full stop); otherwise "V becomes bored with life".
   - teamplay 2, same non-empty team: one of four (each 1/4): "K mows down a teammate",
     "K checks his glasses", "K gets a frag for the other team", "K loses another friend",
     K −1; V unchanged.
   - Otherwise K +1 and, by cause:
     - nail: "V was nailed by K"; super nail: "V was punctured by K",
     - grenade: "V eats K's pineapple", or if V's health is below −40 "V was gibbed by K's
       grenade",
     - rocket while K has quad and V's health below −40: 30% "V was brutalized by K's quad
       rocket", 30% "V was smeared by K's quad rocket", 40% "K rips V a new one",
     - rocket otherwise: "V rides K's rocket", or below −40 health "V was gibbed by K's rocket",
     - any other cause, by the weapon K is holding at that moment: axe "V was ax-murdered by K",
       shotgun "V chewed on K's boomstick"; super shotgun "V ate 2 loads of K's buckshot",
       lightning gun "V accepts K's shaft", or "V accepts K's discharge" if K is at liquid level
       2+.
     - If none of these match (for example K switched to a nailgun or launcher after an
       untagged kill), the program prints V and K with message pieces it never set for this
       call; what appears is not settled by the source [D].
7. Killer is not a player (the world, a trigger, a door with no player opener, a lava ball, a
   box): V −1 and, first by V's liquid type at death:
   - water: "V sleeps with the fishes" or "V sucks it down" (50/50),
   - slime: "V gulped a load of slime" or "V can't exist on slime alone",
   - lava: "V burst into flames" if health is below −15, else "V turned into hot slag" or
     "V visits the Volcano God" (50/50),
   then (not in liquid): exploding box "V blew up"; falling "V fell to his death"; a nail
   shooter "V was spiked"; a laser "V was zapped"; a lava ball "V ate a lavaball"; a level exit
   that kills (see 22) "V tried to leave"; anything else (trigger_hurt, drowning out of water …)
   "V died". Note that any world death while standing in a liquid, even falling or a hurt
   trigger, gets the liquid's message [D].
8. `kill` command (not an obituary path): "V suicides"; V −2 [R].
- Every frag change also writes a line to the server's frag log [R].
- teamplay 1 and 3 block team damage outright (see 19), so they have no team-kill message [D].

### 9. Falling damage

- At the end of each command, if the vertical velocity at the end of the previous command was
  below −300 u/s and the player is now on the ground [R]:
  - liquid type water: `player/h2ojump.wav`, body channel; no damage,
  - else, if it was below −650 u/s: 5 damage from the world (through armour; cause "falling") and
     `player/land2.wav`, voice channel,
  - else: `player/land.wav`, voice channel; no damage.
- Same in every deathmatch mode [R]. Not for dead players [R].

---

## Items

All items drop to the floor 0.2 s after the map starts (raised 6 u first); one that falls out of
the map is removed [R]. A taken item vanishes and becomes untouchable; a respawn restores it with
`items/itembk2.wav`, voice channel, at the item [R]. Only live players can take items [R].

### 10. Respawn times

| Item | dm 1 (and 0) | dm 2 | dm 3 | dm 4 | dm 5 |
| --- | --- | --- | --- | --- | --- |
| Health 15 / 25 | 20 s | never | 20 s | 20 s | 20 s |
| Megahealth | 20 s after its rot ends | never | as dm 1 | never (and no rot) | as dm 1 |
| Armour (all) | 20 s | never | 20 s | 20 s | 20 s |
| Weapons | 30 s | stay (never taken) | stay | not in the map | not in the map |
| Ammo boxes | 30 s | never | 15 s | not in the map | 15 s |
| Quad, biosuit | 60 s | 60 s | 60 s | 60 s | 60 s |
| Pentagram, ring | 300 s | 300 s | 300 s | 300 s | 300 s |

[R] for each cell; "not in the map" means the item's spawn removes itself or does nothing in
that mode (weapons spawn only at deathmatch 3 or below; ammo boxes not at 4) [R]. All times count
from the pickup, except the megahealth (see 12) [R].

### 11. Armour

- Green: absorbs 0.3, value 100. Yellow: 0.6, 150. Red: 0.8, 200 [R].
- Pick-up rule: taken only when (current absorption × current value) is less than (new absorption ×
  new value), i.e. below 30 / 90 / 160 [R]. Taking it replaces both outright (no adding) [R].
- Absorption of a hit of D (after quad) [R]: saved = ceil(absorption × D); if saved is at least
  the armour value, saved = the value and the armour is lost (absorption 0, the armour icon
  cleared); armour −= saved; health loses ceil(D − saved).
- Armour is consumed even when the hit is then cancelled by teamplay 1/3 protection (that check
  comes after armour) [R]; the pentagram check also comes after armour [R], so an invulnerable
  player's armour still wears down [D].

### 12. Health

- Small box (spawnflag 1, "rotten"): 15; normal box: 25; megahealth (spawnflag 2): 100 [R].
- Normal and small boxes: refused at 100 health or more; health is capped at 100 [R].
- Megahealth: refused at 250 or more; adds 100, capped at 250 [R]. 5 s after the pickup the rot
  starts: while the taker's health is above 100, −1 per second; when it is at or below 100 the
  super-health flag is cleared and the box respawns 20 s later (dm 2: never) [R]. The rot follows
  the player through death and respawn, and stops as soon as their health is 100 or less [D].
  In deathmatch 4 there is no rot and the box never returns [R].
- Overall health cap from any heal is 250 [R]. Amounts are rounded up [R].
- deathmatch 4: health boxes refused while any pentagram (the spawn one included) is active, see 19 [R].

### 13. Ammo and weapons

- Caps: shells 100, nails 200, rockets 100, cells 100 [R]. A box is refused if that ammo is at
  its cap; otherwise added and clamped [R].
- Boxes: shells 20 (large 40), nails 25 (large 50), rockets 5 (large 10), cells 6 (large 12) [R].
- Weapon pickups add: super shotgun 5 shells; nailgun 30 nails; super nailgun 30 nails; grenade
  launcher 5 rockets; rocket launcher 5 rockets; lightning gun 15 cells; clamped to the caps [R].
- Weapon stay (deathmatch 2, 3 and 5): a weapon you already own cannot be touched (nothing
  happens, no ammo); one you do not own gives the weapon and its ammo and stays in place [R].
- Picking up ammo: if the player's selected weapon was their "best" (see 17) before the pickup,
  they switch to the new best after it [R].

### 14. Powerups

- Durations: all four 30 s from pickup [R].
- Warning, when less than 3 s remain: a one-time print to the holder, a screen flash and a
  sound (auto channel, volume 1); then a flash every 1 s until it ends [R]:
  - Quad: "Quad Damage is wearing off" (deathmatch 4: "OctaPower is wearing off"),
     `items/damage2.wav`,
  - Pentagram: "Protection is almost burned out", `items/protect2.wav`,
  - Ring: "Ring of Shadows magic is fading", `items/inv2.wav`,
  - Biosuit: "Air supply in Biosuit expiring", `items/suit2.wav`.
- Quad: damage × 4 (deathmatch 4: × 8) for every hit the holder deals, self-damage included,
  except a door's crush [R]. Firing plays `items/damage3.wav`, body channel, at most once per
  second [R]. Blue glow (dim light + blue) while active [R].
- Pentagram: all damage is ignored (after knockback and armour, see 11, 18); when hit,
  `items/protect3.wav` on the item channel, rate-limited to once per 2 s by a timer kept on
  whichever entity the program was running for at the time (usually the attacker), so the limit
  is unreliable [R], [D]. Red glow (dim light + red) [R].
- Ring: the player is drawn as the floating-eyes model (`progs/eyes.mdl`, frame 0); no pain
  animation and so no pain sounds (see 5); `items/inv3.wav` at volume 0.5 with idle attenuation,
  auto channel, every 1 to 4 s (random) while it lasts [R].
- Biosuit: no drowning (air refreshed every command), no slime damage, lava ticks every 1 s
  instead of 0.2 s [R].
- Pickup sounds: on the voice channel of the taker: Quad `items/damage.wav`, Pentagram
  `items/protect.wav`, Ring `items/inv1.wav`, Biosuit `items/suit.wav` [R].
- Dropping (server keys) [R]: with `dq` non-zero, a player who dies holding the quad drops it
  (tossed like a backpack) with the time left; broadcast at low level "V lost a quad with N
  seconds remaining" (dm 4: "lost an OctaPower with"). With `dr` non-zero, the ring likewise:
  "V lost a ring with N seconds remaining". A dropped one vanishes when its time runs out; taking
  it gives the remaining time and broadcasts "P recovered a Quad with N seconds remaining!" (dm
  4: "recovered an OctaPower with") or "P recovered a Ring with N seconds remaining!" [R]. N is
  rounded to a whole second [R].
- deathmatch 4 quad ("OctaPower"): taking it zeroes the taker's armour and cells; when it ends
  the holder is set to 255 cells, armour 1 at 0.8 and health 100 [R].
- Map changes strip all powerups (see 6) [R].

### 15. Pickup messages and sounds

Messages go only to the taker at low print level; every pickup also flashes the taker's
screen [R].

| Pickup | Message | Sound (channel) |
| --- | --- | --- |
| Health | "You receive 25 health" (15, 100) | `items/health1.wav`; small `items/r_item1.wav`; mega `items/r_item2.wav` (item) |
| Armour | "You got armor" | `items/armor1.wav` (item) |
| Weapon | "You got the Double-barrelled Shotgun" / "nailgun" / "Super Nailgun" / "Grenade Launcher" / "Rocket Launcher" / "Thunderbolt" | `weapons/pkup.wav` (item) |
| Ammo | "You got the shells" / "nails" / "rockets" / "cells" | `weapons/lock4.wav` (item) |
| Powerup | "You got the Quad Damage" (dm 4 "OctaPower") / "Pentagram of Protection" / "Ring of Shadows" / "Biosuit" | as in 14 (voice) |
| Backpack | "You get " then "the W" if the weapon is new to the taker, then the non-zero ammo as "N shells", "N nails", "N rockets", "N cells", comma-separated | `weapons/lock4.wav` (item) |
| Key | "You got the silver key" (runekey, keycard by world type; gold likewise) | `misc/medkey.wav` / `misc/runekey.wav` / `misc/basekey.wav` |
| Rune | "You got the rune!" (centred) | `misc/runekey.wav` |

All [R]. The backpack's weapon names: "Axe", "Shotgun", "Double-barrelled Shotgun", "Nailgun",
"Super Nailgun", "Grenade Launcher", "Rocket Launcher", "Thunderbolt" [R]. Backpack extras:
in deathmatch 3 and 5, if the pack's weapon is a grenade or rocket launcher and the taker has
fewer than 5 rockets after it, they are raised to 5 [R]. deathmatch 4 backpacks: see 19.

---

## Weapons

### 16. Per weapon

Fire times are the minimum time before the next attack or impulse is accepted [R]. "Aim" is the
server's autoaim along the view [R]. In deathmatch 4 no weapon uses ammo [R].

- **Axe**: 20 damage (deathmatch 4 and 5: 75); reach 64 u from 16 u above the origin along the
  view; 0.5 s; no ammo [R]. The swing sound `weapons/ax1.wav` plays at the press; the hit lands on
  the third animation frame, 0.2 s later [R], [D]. Hitting a wall: `player/axhit2.wav` and a
  puff; hitting something damageable: blood, and the victim's pain sound is `player/axhit1.wav`
  [R]. One of four swing animations at random [R].
- **Shotgun**: 1 shell; 0.5 s; 6 pellets of 4 damage (max 24); each pellet's direction is the
  aim plus a uniform random offset of up to ±0.04 along the view's right and ±0.04 along its up
  axis; range 2048 u from 10 u ahead of the origin at 70% of the hull's height; `weapons/guncock.wav`
  [R].
- **Super shotgun**: 2 shells; 0.7 s; 14 pellets of 4 (max 56); spread ±0.14 right, ±0.08 up,
  `weapons/shotgn2.wav` [R]. With exactly 1 shell it fires a normal shotgun blast instead (still
  0.7 s) [R]. Pellets hitting the same target in a row are summed into one hit (so armour rounds
  once per run of pellets) [R].
- **Nailgun**: 1 nail; one nail per 0.1 s animation frame while attack is held (10 per second)
  [R], [D]; 9 damage per nail; speed 1000 u/s; nails spawn 16 u above the origin, alternately
  4 u right and 4 u left; `weapons/rocket1i.wav`; nails vanish after 6 s and vanish silently in
  the sky [R]. Holding a weapon-change impulse stops the stream [R].
- **Super nailgun**: 2 nails per shot, same rate, 18 damage per nail, 1000 u/s, centred,
  `weapons/spike2.wav`; with only 1 nail left it fires a single normal nail [R].
- **Grenade launcher**: 1 rocket; 0.6 s; speed: if the view is pitched at all, 600 along the
  view plus 200 up plus a random ±10 sideways and ±10 up; if the view is exactly level, aim × 600
  with the vertical set to 200; spin; fuse 2.5 s; explodes at once on touching a player (any
  entity that can be aimed at), otherwise bounces with `weapons/bounce.wav`; explosion: splash
  120 (below), no direct-hit damage; `weapons/grenade.wav` [R]. deathmatch 4: fire time 1.1 s
  and every shot costs the firer 10 damage [R].
- **Rocket launcher**: 1 rocket; 0.8 s; 1000 u/s; starts 8 u ahead and 16 u above the origin,
  flies 5 s at most; does not hit its owner; vanishes silently in the sky; `weapons/sgun1.wav`
  [R]. On impact: the thing hit (if it has health) takes 100 + 20 × random (100–120); then splash
  120 to everything else (the thing hit is excluded); the explosion effect is drawn 8 u back
  along the flight path [R].
- **Splash** (rockets, grenades, exploding boxes at 160) [R]: every damageable entity whose box
  centre is within (damage + 40) u of the explosion (160 u for rockets and grenades) takes
  damage − 0.5 × (distance from the explosion to its box centre), halved again for the attacker
  himself, if positive and if a line (ignoring other players) from the explosion reaches the
  target's origin or one of four points 15 u off it diagonally in the horizontal plane. So
  splash falls to 40 at 160 u and is cut off there [D].
- **Lightning gun**: 1 cell per 0.1 s frame while held (10 per second) [R], [D]; 30 damage per
  hit; the beam is drawn from 16 u above the origin up to 600 u to the first wall (ignoring
  players); damage traces from the origin to 4 u past that point: one centre line and two side
  lines; each different damageable entity they meet takes 30 [R]. The side lines' offset is
  built from the beam's own length rather than a unit vector, so they are not a fixed 16 u apart
  [D]. Sounds: `weapons/lstart.wav` on the auto channel at the press; `weapons/lhit.wav` on the
  weapon channel at most every 0.6 s while firing [R]. Fire time 0.1 s [R]. A vertical kick of
  +400 u/s to a player hit is tied to an unrelated global and in practice almost never
  happens [D].
- **Lightning gun discharge**: firing at liquid level 2 or more uses up ALL cells and deals
  splash of 35 × cells from the firer (with the firer as attacker, so the firer takes it halved,
  see splash), with no cause tag; no beam [R]. In deathmatch 4 and 5 there is a 50% chance
  instead of the firer alone taking 4000 damage ("electrocutes himself.") [R]. Victims' and
  firer's obituaries: see 8.

### 17. Weapon switching

- "Best weapon" order (used when out of ammo, and after ammo pickups) [R]: lightning gun (needs
  1+ cell and liquid level 0 or 1), super nailgun (2+ nails), super shotgun (2+ shells), nailgun
  (1+), shotgun (1+), else axe. Launchers are never chosen automatically [R].
- Out of ammo: each command, once the fire time has passed, if the selected weapon (not the axe)
  shows 0 ammo the player switches to the best weapon [R]; pressing attack with no ammo also
  switches instead of firing [R]. Nailgun and lightning gun streams switch when they run dry
  [R].
- On weapon pickup [R]: the player's userinfo key `w_switch` (0 or missing means 8) is a ceiling
  on the weapon's number (super shotgun 3, nailgun 4, super nailgun 5, grenade launcher 6, rocket
  launcher 7, lightning gun 8); if the new weapon's number is within it, the player switches
  when the new weapon ranks above the held one in the order lightning gun, rocket launcher,
  super nailgun, grenade launcher, super shotgun, nailgun, then the rest; never to the lightning
  gun while in a liquid. Backpacks do the same with the key `b_switch` (default 8) [R].
- Impulse selection [R]: 1 axe, 2 shotgun, 3 super shotgun, 4 nailgun, 5 super nailgun,
  6 grenade launcher, 7 rocket launcher, 8 lightning gun. Not owned: "no weapon." Owned but
  below the minimum (shotgun 1 shell, super shotgun 2, nailgun 1, super nailgun 2, launchers 1
  rocket, lightning gun 1 cell): "not enough ammo." Both printed to the player at high level.
- Impulse 10: next owned weapon with enough ammo, in the order axe, shotgun, super shotgun,
  nailgun, super nailgun, grenade launcher, rocket launcher, lightning gun, wrapping; impulse 12
  the same backwards [R].
- Impulses are acted on only when the fire time has passed [R].

### 18. Rocket self-damage and knockback

- Knockback, for any damage to a walking entity (players) from anything other than the world [R]:
  velocity += unit vector (from the inflictor's box centre to the target's origin) × damage × 8,
  where damage is after the quad multiplier and, for splash, after distance falloff and the self
  halving, but before armour. The inflictor is the rocket or grenade itself, the nail, or the
  firing player for hitscan weapons [R].
- Knockback is applied before the pentagram and teamplay checks, so invulnerable players and
  protected teammates are still pushed [R].
- Rocket jump modifier [R]: if the server key `rj` (see 6) is above 1 and the damage is the
  player's own (attacker and target the same player), an extra unit-vector × damage × rj is added.
- Self-damage from one's own rocket: splash only (the rocket never hits its owner directly):
  (120 − 0.5 × distance to own box centre) × 0.5, then quad, then armour [R], [D]. At point-blank
  under the feet (distance about 28 u from a floor impact to the box centre) that is about 53
  before armour [D].
- teamplay 1 removes self-damage too when the player has a non-empty team (knockback stays) [R].

---

## Rules and modes

### 19. Settings the program reads

Read every server frame [R]: `deathmatch`, `teamplay`, `fraglimit`, `timelimit` (minutes).

- **deathmatch 0/1**: standard; weapons taken and back in 30 s, ammo 30 s, health and armour
  20 s [R].
- **deathmatch 2** ("old rules"): weapons stay; health, megahealth, armour and ammo never come
  back; powerups do [R].
- **deathmatch 3**: weapons stay; ammo back in 15 s; health and armour 20 s; the backpack rocket
  floor (see 15) [R].
- **deathmatch 4**: no weapons or ammo in the map; the spawn kit in 6; no ammo use; axe 75,
  grenade launcher 1.1 s and 10 self-damage per shot; quad is × 8 "OctaPower" with the extras in
  14; lightning in water may electrocute only the firer (16); megahealth never rots or returns,
  while any pentagram (the spawn one included) is active the player cannot take health, armour
  or backpacks
  [R]. Backpacks in deathmatch 4 give no ammo but +10 health (sound `weapons/lock4.wav`, or
  `items/protect3.wav` if health ends between 251 and 299); "You get 10 additional health",
  reaching 300 or more grants the pentagram and quad for 30 s, zeroes cells, plays
  `boss1/sight1.wav` and broadcasts "P attains bonus powers!!!" [R].
- **deathmatch 5**: no weapons in the map, spawn kit in 6, weapon stay rules, ammo back in 15 s,
  axe 75, the lightning electrocution chance, the backpack rocket floor [R].
- **teamplay** (team = the player's userinfo key `team`; an empty team is never a teammate) [R]:
  - 1: no damage between teammates and no self-damage (knockback and armour loss still happen),
  - 2: teammates can be hurt; killing one costs the killer 1 frag with the messages in 8,
  - 3: no damage between teammates, self-damage allowed,
  - any non-zero value: crushing a teammate costs 1 frag.
  A door's crush ignores teamplay protection [R].
- Other settings and keys [R]: cvars `samelevel` (22), `temp1` (7), `registered` (the `start`
  map's episode choice); server or local info keys `rj` (18), `dq` and `dr` (14), `axe` (6), and
  a key named after the current map giving the next map (20); userinfo keys `team`, `w_switch`,
  `b_switch`. At every map load the program sets `sv_gravity` to 800 (100 on the map e1m8) [R].
- The single-player program's handling of these modes: open (not in the release read).

### 20. Fraglimit, timelimit, intermission

- Fraglimit: checked in each player's pre-think against that player's own frags (frags at or
  above the limit ends the map) [R]. Timelimit: minutes × 60 compared with the server's map
  clock, which starts at 1.0 s when the map loads, so the map ends 1 s before the stated number
  of minutes of play [R], [D]. Both are checked only while some player is in the game and not
  in intermission [D].
- Ending [R]: the next map is the first `trigger_changelevel`'s map in the map's entity order,
  or the same map if there is none (the map `start` picks the first episode not yet completed),
  0.1 s later the intermission starts.
- Intermission [R]: the CD track switches to 3; the camera is sent to every client: a random
  `info_intermission` (by its pitch/yaw/roll angles), or the `info_player_start` if there is
  none; every player becomes invisible, untouchable, unhurtable and frozen. The scoreboard is the
  client's own on receiving the intermission [R].
- Leaving it [R]: no earlier than 5 s after it began, the first player to press attack, jump or
  use sends everyone to the next map. There is no automatic timeout in the program. A player
  connecting during the intermission moves everyone on at once.
- Which map [R]: `samelevel` 1: the same map again; otherwise the server info or local info key
  named after the current map, if set (a rotation list); otherwise the next map chosen above.

### 21. Impulses and commands

- Impulses [R]: 1–8 weapon select, 10 next weapon, 12 previous weapon (17). 9 (the
  all-weapons cheat) is refused in every mode: it does nothing. 11 doubles the server's episode
  flags and adds 1 (a development leftover, available to anyone). Others are ignored.
- Spectators: impulse 1 jumps to the next `info_player_deathmatch` in turn [R].
- Commands the program handles: `kill` (the server refuses it when health is 0 or less with
  "Can't suicide -- allready dead!"), otherwise "V suicides", V −2 frags, an immediate respawn
  with the default kit and no backpack; the body left is posed in a death frame [R].
- `god`, `give`, `noclip` are server console commands, refused unless the server was started
  with `-cheats` ("You must run the server with -cheats to enable this command.") [R]. They are
  the server's, not the program's.

### 22. Other things a player notices

- Join and leave [R]: "P entered the game" (high); "P left the game with N frags" (high) plus
  `player/tornoff2.wav` map-wide, body channel; spectators: "Spectator P entered the game" /
  "Spectator P left the game" (medium).
- No sounds or prints on frags beyond the obituaries; no "you are the leader" kind of print [R].
- Jumping [R]: `player/plyrjmp8.wav`, body channel, on each jump from the ground (not while
  swimming).
- Level exits in deathmatch [R]: touching a `trigger_changelevel` broadcasts "P exited the
  level" and ends the map for everyone (to that trigger's map). `samelevel` 2: any exit instead
  kills the toucher (50000, "tried to leave", −1); 3: the same except on the map `start`.
- Teleporters [R]: a teleport flash at the start and 32 u in front of the destination, each with
  one of the five `misc/r_tele` sounds 0.2 s later; the telefrag trigger at the destination
  (6); the player arrives facing the destination's angles at 300 u/s forward, and the movement
  code holds them for 0.7 s. Only live, solid players and monsters teleport.
- Jump pads [R]: `trigger_push` throws at 10 × its speed along its direction, with
  `ambience/windfly.wav` (auto channel) at most every 1.5 s per player.
- Hurt triggers [R]: default 5 damage, at most once per second per trigger.
- Screen flashes [R]: every pickup and each powerup warning.

---

## Status

1 answered · 2 answered · 3 answered · 4 answered · 5 answered · 6 answered · 7 answered ·
8 answered · 9 answered · 10 answered · 11 answered · 12 answered · 13 answered · 14 answered ·
15 answered · 16 answered · 17 answered · 18 answered · 19 answered · 20 answered ·
21 answered · 22 answered. The single-player program's differences, asked on every point, are
open throughout: its sources are not in the release read.

---

# Round 2: the player's frames, map entities, the intermission

Read: id Software's Quake release source, GPL v2, on GitHub (default branch, commit bf4ac424),
its QuakeWorld game program and its QuakeWorld server. Marks: **[R]** read, **[D]** derived by
working out what the source does. Nothing was run. Frames are named as in the question.

Timing facts used throughout [R]: every animation step of the player sets the next step 0.1 s
ahead. The server runs a player's pending step once per player command, before the move, and
runs it if it is due within that command's duration (capped at 0.1 s). A step can therefore come
up to one command early, and a step whose time has passed runs at the command's time [D]. Doors,
plats, buttons and trains count time on their own movement clock.

---

## The player's frames

### 1. Pain

- Without the axe: pain1, pain2, pain3, pain4, pain5, pain6. With the axe: axpain1 … axpain6
  [R]. 0.1 s each, so 0.6 s in all [D]. The run/stand cycle follows, 0.1 s after pain6 [R].
- On the first frame the view-model frame is set to 0 and the pain sound is chosen (round 1,
  question 5) [R].
- Pain is not started at all while the view-model frame is non-zero, which means during any
  firing sequence. It is also not started while the Ring is active [R].
- A new hit during pain restarts at pain1 (or axpain1), because pain keeps the view-model frame
  at 0 [R], [D].
- Pain ends early when the player attacks (the firing sequence takes over), dies (the death
  sequence takes over), or switches weapon (switching restarts the run/stand cycle) [R], [D].

### 2. Firing

The first step of each sequence plays at the press, inside the same command [R]. The muzzle
flash is the muzzle-flash message, sent to everyone who can see the player [R].

- **Axe**: one of four swings, chosen at random with equal odds [R].
  - axatt1–axatt4 with view frames 1–4.
  - axattb1–axattb4 with view frames 5–8.
  - axattc1–axattc4 with view frames 1–4.
  - axattd1–axattd4 with view frames 5–8.

  Only the first four frames of each six-frame set are used [R]. The hit is on the third frame,
  0.2 s after the press [R], [D]. The run/stand cycle starts 0.1 s after the fourth frame [R].
  The axe makes no muzzle flash [R]. Its swing sound plays at the press [R].
- **Shotgun and super shotgun**: shotatt1–shotatt6 with view frames 1–6, 0.1 s each [R]. The
  shot is fired at the press, right after the first frame is set [R]. Muzzle flash on the first
  frame only [R]. Then the run/stand cycle starts [R].
- **Grenade and rocket launchers**: rockatt1–rockatt6 with view frames 1–6, 0.1 s each [R].
  The projectile is launched at the press, on the first frame [R]. Muzzle flash on the first
  frame only [R].
- **Nailgun and super nailgun while held**: nailatt1 and nailatt2 alternate every 0.1 s [R].
  - Each step sends a muzzle flash first [R].
  - It then stops, switching to the run/stand cycle, if attack is released, the intermission
     has begun, or an impulse is pending [R]. The step that stops still sent its flash [R].
  - Otherwise the view frame advances by one in a cycle 1 to 8, and one shot is fired [R]. The
     first shot is at the press [R].
  - On the plain nailgun the nail comes 4 u right on the nailatt1 steps and 4 u left on the
     nailatt2 steps [R].
- **Lightning gun while held**: light1 and light2 alternate every 0.1 s [R]. Each step sends a
  muzzle flash, then stops if attack is released or the intermission has begun [R]. A pending
  impulse does NOT stop it [R]. Otherwise the view frame advances in a cycle 1 to 4 and one beam
  is fired [R]. The first beam is at the press, after `weapons/lstart.wav` [R].
- A weapon change, including the automatic one when ammo runs out, cuts any sequence and
  restarts the run/stand cycle with view frame 0 [R].

### 3. Running and standing

- Confirmed: one frame every 0.1 s [R].
- The player counts as running when the server-side horizontal velocity has any non-zero x or
  y component [R].
- Run cycles: rockrun1–rockrun6 without the axe, axrun1–axrun6 with it [R].
- Stand cycles: stand1–stand5 without the axe, axstnd1–axstnd12 with it [R].
- Switching between running and standing restarts the other cycle at its first frame, in the
  same step [R].
- Every step of either cycle sets the view frame to 0 [R].
- While firing or in pain the cycle does not run, because those sequences own the player's
  steps. When they end, the cycle resumes at the count where it was left, not at its first
  frame [D].
- While the Ring is active the player is drawn as the eyes model with frame 0, whatever the
  cycle says [R].

### 4. Death

- Confirmed sequences [R]:
  - deatha1–deatha11
  - deathb1–deathb9
  - deathc1–deathc15
  - deathd1–deathd9
  - deathe1–deathe9
  - axdeth1–axdeth9, when holding the axe

  0.1 s per frame [R].
- On the last frame the player becomes "fully dead" and stops stepping, so that frame holds
  [R]. Pitch and roll of the body are zeroed at death [R].
- The corpse queue copies whatever frame the body shows at respawn, normally the last frame of
  its sequence [R]. The `kill` command and disconnecting set deatha11 first [R]. A gibbed player
  is the head model at frame 0 [R].

---

## Map entities in the deathmatch program

The server's loading rule, for every entity [R]:

- An entity with spawnflag 2048 (not in deathmatch) is always removed, whatever the
  `deathmatch` value.
- The skill flags (256, 512, 1024) are not checked at all, so those entities stay [R], [D].
- An entity with no class name, or whose class has no spawn function in the program, is freed.
  The server console prints "No spawn function for:" and the entity's fields [R].

### 5. `func_door_secret`

**Keys and defaults** [R]:

- `sounds` defaults to 3. A value above 3 leaves the door silent [D].
- `dmg` defaults to 2.
- `wait` defaults to 5 s.
- Speed is always 50 u/s. A `speed` key is overwritten.
- `t_width` and `t_length` override the two move distances.
- `message`
- `angle` gives the facing direction.
- Spawnflags: 1 open once, 2 first move left, 4 first move down, 8 "no shoot", 16 always
  shootable.

**Shooting** [R]:

- The door is shootable if it has no `targetname`, or if flag 16 is set. It is given 10000
  health, and any damage at all opens it.
- With a `targetname` and no flag 16, only its trigger or button opens it.
- While it moves it ignores further use.
- When it starts moving it stops being shootable, unless flag 8 is set. So flag 8 never stops
  the door being shot; it only keeps an already-shootable door shootable while it moves [D].
- At the end of its cycle it becomes shootable again, under the same condition as at spawn.

**The move** [R]:

1. Use fires the door's targets and clears its message. Then noise1 starts, and noise2 starts at
   once on the same voice channel, so noise2 cuts noise1 [D].
2. It moves to the side by its width. The side is right of its facing direction, or left with
   flag 2, or down by its height with flag 4.
3. On arrival: noise3, then a 1 s pause.
4. noise2, then it moves forward along its facing direction by its length.
5. On arrival: noise3. With flag 1 it stays open for good. Otherwise it waits `wait` seconds.
6. noise2, then it moves back to the first position.
7. On arrival: noise3, then a 1 s pause.
8. noise2, then it moves back to its start.
9. On arrival: noise3, and it is re-armed.

The distances default to the door's own size along those axes [R].

**Sounds by `sounds`** [R]:

| `sounds` | Start (noise1) | Moving (noise2) | Stop (noise3) |
| --- | --- | --- | --- |
| 1 | `doors/latch2.wav` | `doors/winch2.wav` | `doors/drclos4.wav` |
| 2 | `doors/airdoor2.wav` | `doors/airdoor1.wav` | `doors/airdoor2.wav` |
| 3 | `doors/basesec2.wav` | `doors/basesec1.wav` | `doors/basesec2.wav` |

**Touch** [R]: a player touching the door gets its `message` centre-printed, with
`misc/talk.wav` on the player's body channel. This happens at most once per 2 s, until the door
is first used.

**Blocking** [R]:

- Whatever blocks the door takes `dmg` at most once per 0.5 s, and the door keeps pushing.
- The cause is crushing, and the attacker is the door itself. A crushed player's obituary is
  "V was squished", −1 [D].
- Quad never multiplies door damage [R].
- Teamplay protection does not apply to doors [R].

### 6. `misc_explobox`, `misc_explobox2`

**The two boxes** [R]:

| | `misc_explobox` | `misc_explobox2` |
| --- | --- | --- |
| Model | `maps/b_explob.bsp` | `maps/b_exbox2.bsp` (registered content) |
| Size (the box's origin is its lowest corner) | 32 × 32 × 64 u | 32 × 32 × 32 u |

- Each is solid, does not move, and has 20 health [R].
- At spawn it is raised 2 u and dropped to the floor [R].
- It can be aimed at, so grenades burst on contact with it [R].

**Destroying it** [R]:

- When its health reaches 0 or below it explodes at once.
- There is no obituary for the box, and the shooter gets nothing.

**The blast** [R]:

- 160 damage at the box's origin, its lowest corner, reaching every damageable entity whose box
  centre is within 200 u.
- Each takes 160 − 0.5 × the distance to its box centre, with the same line-of-sight test as
  rockets.
- The box itself is excluded, because it has already stopped taking damage.
- The attacker is the box, so a player killed by the blast gets "V blew up" and −1. No player is
  credited, not even the one who shot the box [D].
- Nearby boxes caught in the blast explode in turn, within the same frame [D].

**What players see and hear** [R]:

- The explosion effect is drawn 32 u above the box's origin.
- The program sends no sound of its own. The client plays `weapons/r_exp3.wav` with the
  explosion effect.

### 7. `trap_spikeshooter`, `trap_shooter`

**Firing** [R]:

- `trap_spikeshooter` fires once each time it is used.
- `trap_shooter` fires on its own every `wait` seconds (default 1).
  - Its first shot comes at the map's start plus the `nextthink` key plus `wait`. The map clock
     starts at 1.0 s, but the `nextthink` key is added to 0, so the first shot is at game time
     `nextthink` + `wait` [D].
  - A `trap_shooter` can also be used to fire an extra shot [R].
- Direction comes from `angle`: −1 means up and −2 means down [R].

**Spikes** (no spawnflag 2) [R]:

- Sound `weapons/spike2.wav` on the shooter's voice channel.
- A nail at 500 u/s that vanishes after 6 s.
- 9 damage, or 18 with spawnflag 1 (super spike).
- The attacker is the shooter, so a player killed by it gets "V was spiked" and −1 [D].

**Laser** (spawnflag 2, registered content: `progs/laser.mdl`, `enforcer/enfire.wav`,
`enforcer/enfstop.wav`) [R]:

- On fire: `enforcer/enfire.wav` on the shooter's voice channel.
- The bolt flies straight, without gravity, at 600 u/s and glows with a dim light.
- It vanishes after 5 s, or silently in the sky.
- On any impact: `enforcer/enfstop.wav` on the bolt's weapon channel, static attenuation.
- If the thing hit has health: blood and 15 damage, cause laser, so the obituary is "V was
  zapped", −1 [D].
- Otherwise: a five-particle gunshot puff, 8 u back along its path.

### 8. Ambients, lights, fireballs and others

**Ambient sounds.** All are fixed looping sounds at their spot, with static attenuation [R]:

| Entity | Sample | Volume |
| --- | --- | --- |
| `ambient_suck_wind` | `ambience/suck1.wav` | 1 |
| `ambient_drone` | `ambience/drone6.wav` | 0.5 |
| `ambient_flouro_buzz` | `ambience/buzz1.wav` | 1 |
| `ambient_drip` | `ambience/drip1.wav` | 0.5 |
| `ambient_comp_hum` | `ambience/comp1.wav` | 1 |
| `ambient_thunder` | `ambience/thunder1.wav` | 0.5 |
| `ambient_light_buzz` | `ambience/fl_hum1.wav` | 0.5 |
| `ambient_swamp1` | `ambience/swamp1.wav` | 0.5 |
| `ambient_swamp2` | `ambience/swamp2.wav` | 0.5 |

**Removed at spawn** [R]: `air_bubbles` and `event_lightning`.

**`misc_fireball`** [R]:

- Throws a lava ball (`progs/lavaball.mdl`) from its spot.
  - The first one comes at a random time 0–5 s after the map starts.
  - After that, one every 3–8 s, at random.
- Each ball falls under gravity, with horizontal speed −50 to +50 u/s on each axis and vertical
  speed `speed` + 0–200 u/s.
- `speed` is meant to default to 1000, but the default is never set, so without the key the
  balls rise at only 0–200 u/s [D].
- On touching anything: 20 damage to it, if it takes damage, and the ball is removed. The
  obituary is "V ate a lavaball", or the liquid's message if the victim stood in a liquid, −1.
- An untouched ball is removed after 5 s.
- No sound.

**Lights** [R]:

- `light` is removed unless it has a `targetname`.
  - A targeted light with `style` 32 or more toggles that light style between "m" (on) and "a"
     (off) on each use.
  - Spawnflag 1 makes it start off.
- `light_fluoro`: the same switching, plus a looping `ambience/fl_hum1.wav` at 0.5.
- `light_fluorospark`: a looping `ambience/buzz1.wav` at 0.5. Its style defaults to 10, but
  nothing is switched at run time.
- `light_globe`: the sprite `progs/s_light.spr`, as a static entity.
- `light_torch_small_walltorch`: `progs/flame.mdl`.
- `light_flame_large_yellow`: `progs/flame2.mdl` frame 1.
- `light_flame_small_yellow` and `light_flame_small_white`: `progs/flame2.mdl` frame 0.
- All the flames are static entities with a looping `ambience/fire1.wav` at 0.5, static
  attenuation.


### 9. Triggers

**`trigger_multiple` and `trigger_once`** [R]:

- Only players touch them.
- With `angle` set, the trigger fires only when the player's facing direction is within 90° of
  the angle [D].
- Spawnflag 1 (no touch): the trigger is fired only by being used.
- `wait`: 0.2 s by default for `trigger_multiple`. `trigger_once` removes itself 0.1 s after
  firing.
- While waiting, the trigger ignores touches.

**`sounds`** [R]: the sound plays on the trigger's own voice channel when it fires.

| `sounds` | Sample |
| --- | --- |
| 1 | `misc/secret.wav` |
| 2 | `misc/talk.wav` |
| 3 | `misc/trigger1.wav` |

**`message`** [R]:

- Centre-printed to the activator, if a player, when the trigger fires.
- `misc/talk.wav` plays on the activator's voice channel only when the trigger has no
  `sounds`.
- With `delay` set, the message, the targets and the killtargets all happen `delay` seconds
  later. A delayed message always plays `misc/talk.wav` [D].

**`killtarget`** [R]:

- Removes every entity with that `targetname`.
- Because of how it is written, a trigger with a `killtarget` never fires its `target` [D].

**`health`** (shootable triggers) [R]:

- The trigger is solid and must be destroyed to fire. The one who dealt the killing damage is the
  activator.
- After `wait` its health is restored. A `trigger_once` is gone for good.
- Health together with spawnflag 1 stops the map load with an error.

**`trigger_secret`** [R]:

- Like `trigger_once`, but players only.
- `message` defaults to "You found a secret area!".
- `sounds` defaults to 1 (`misc/secret.wav`); 2 is `misc/talk.wav`.
- It adds to the found-secrets count and tells every client so, which is the secret counter on
  their status bar.

**`trigger_relay`** [R]: not touchable. When used it fires its targets at once, honouring
`delay`, `message` and `killtarget`.

**`trigger_counter`** [R]:

- Not touchable. `count` defaults to 2.
- Each use counts down.
- At each use before the last, the activator, if a player, sees one of these, unless spawnflag 1
  is set:
  - "There are more to go..." when 4 or more remain.
  - "Only 3 more to go..."
  - "Only 2 more to go..."
  - "Only 1 more to go..."
- At the last use: "Sequence completed!", then it fires like a `trigger_once`.
- Uses after that do nothing.

### 10. Keys and key doors in deathmatch

**Keys** [R]:

- `item_key1` and `item_key2` drop to the floor like items, and never respawn.
- A key is refused by a player who already has that key.
- Two players can each hold a key only if the map has two such key entities [D].

**When a key is lost** [R]:

- At respawn after a death (the default load-out has no keys).
- At a map change.
- By opening a key door, which takes the key away from the player.
- A key is never put in a backpack.
- So once its carrier dies, that key is gone for the rest of the map [D].

**Key names and sounds** by the world's type [R]:

| World type | Silver | Gold | Sound |
| --- | --- | --- | --- |
| 0 | "silver key" | "gold key" | `misc/medkey.wav` |
| 1 | "silver runekey" | "gold runekey" | `misc/runekey.wav` |
| 2 | "silver keycard" | "gold keycard" | `misc/basekey.wav` |

**Key doors** [R]:

- `func_door` with spawnflag 16 needs the silver key; spawnflag 8 needs the gold key.
- A key door's `wait` is forced to −1, so it stays open for good once opened.
- A player without the key gets one of these centre-printed:
  - "You need the silver key", "You need the silver runekey" or "You need the silver keycard"
  - "You need the gold key", "You need the gold runekey" or "You need the gold keycard"
- That message comes with the try sound on the door's voice channel:
  - `doors/medtry.wav` for world type 0
  - `doors/runetry.wav` for world type 1
  - `doors/basetry.wav` for world type 2
- The message and sound come at most once per 2 s.
- A door `message`, if any, is printed first, with `misc/talk.wav`.
- Opening it plays the use sound:
  - `doors/meduse.wav` for world type 0
  - `doors/runeuse.wav` for world type 1
  - `doors/baseuse.wav` for world type 2

### 11. The others

- **`func_episodegate`** appears as a solid wall only if the server's episode flags include one
  of its spawnflags. Otherwise it is not spawned and stays invisible [R].
- **`func_bossgate`** appears unless all four episode flags are set [R].
- **`trigger_onlyregistered`** [R]:
  - With the `registered` cvar non-zero, the first player to touch it fires its targets, with
     its message cleared, and it is removed.
  - Otherwise its `message` is centre-printed with `misc/talk.wav` on the body channel, at most
     once per 2 s.
- **`trigger_setskill`** is removed at spawn [R].
- **`trigger_monsterjump`** [R]:
  - Defaults: `speed` 200, `height` 200, angle 360.
  - It affects only monsters, so it does nothing in this program, where monsters never exist
     [D].
- **`misc_teleporttrain`** [R]:
  - Spins, with angular velocity 100, 200 and 300 °/s on pitch, yaw and roll.
  - Not solid. Model `progs/teleport.mdl` (registered content).
  - Rides its `path_corner` chain like a silent train, `speed` defaulting to 100.
  - Starts at once unless it has a `targetname`.
- **`info_null`** is removed. **`info_notnull`** is kept, doing nothing [R].
- **`info_player_coop`** and **`info_player_start2`** are kept but never used. Only the map
  `start` would use the latter, and it does not here [R], [D].
- **`testplayerstart`** has no spawn function, so the server frees it at load with "No spawn
  function for:". The spawn code that looks for it therefore never finds one [R], [D].

### 12. Monsters

- These monster classes are removed as they spawn [R]:

  | | | | | |
  | --- | --- | --- | --- | --- |
  | `monster_ogre` | `monster_demon1` | `monster_shambler` | `monster_knight` | `monster_army` |
  | `monster_wizard` | `monster_dog` | `monster_zombie` | `monster_boss` | `monster_tarbaby` |
  | `monster_hell_knight` | `monster_fish` | `monster_shalrath` | `monster_enforcer` | `monster_oldone` |

  No sound is precached and no console line is printed for them [R].
- Any other monster class has no spawn function, and the server frees it with "No spawn function
  for:" [R], [D].
- `path_corner` is kept [R].

### 13. Buttons, doors and plats

How Quake's single-player program differs is open: its sources are not in the release read. This
is what this program does.

**`func_button`** [R]:

- Defaults: `speed` 40, `wait` 1 s, `lip` 4 u.
- Sounds:

  | `sounds` | Sample |
  | --- | --- |
  | 0 | `buttons/airbut1.wav` |
  | 1 | `buttons/switch21.wav` |
  | 2 | `buttons/switch02.wav` |
  | 3 | `buttons/switch04.wav` |

  The sound plays on the button's voice channel when it moves in.
- With `health` set, the button must be shot (any damage that takes its health to 0 or below),
  and is not touchable. The shooter is the activator, and it can be shot again after it returns.
- Without `health`, only players can touch it.
- Once fully in, it fires its targets, switches to its alternate textures, and returns after
  `wait`.
- A `message` on a button is centre-printed to a player activator when it fires, with no
  `misc/talk.wav` (every button has a sound of its own) [D].
- Blocking does nothing; the button just stops.
- A `wait` of −1 is described for the map maker as "never return". The program does not handle
  it specially; it schedules the return 1 s in the past, and what the server's movement clock
  then does is not settled by the program [D].

**`func_door`** [R]:

- Defaults: `speed` 100, `wait` 3 s (−1 never returns), `lip` 8 u, `dmg` 2.
- Sounds (moving / stopping):

  | `sounds` | Moving | Stopping |
  | --- | --- | --- |
  | 0 | `misc/null.wav` | `misc/null.wav` |
  | 1 | `doors/doormv1.wav` | `doors/drclos4.wav` |
  | 2 | `doors/hydro1.wav` | `doors/hydro2.wav` |
  | 3 | `doors/stndr1.wav` | `doors/stndr2.wav` |
  | 4 | `doors/ddoor1.wav` | `doors/ddoor2.wav` |

- Doors that touch are linked and move as one, unless spawnflag 4 is set.
- A door with no `health`, `targetname` or key gets a trigger field. The field is the linked
  doors' combined box, grown by 60 u on each horizontal side and 8 u up and down. Any live
  entity touching it opens the door, at most once per second.
- `health`: shot open. Its health is restored when it closes.
- `message`: centre-printed with `misc/talk.wav` to a player touching it, at most once per 2 s,
  until the door is first used.
- Spawnflag 1 starts it open; 32 makes it a toggle.
- Re-triggering a door that is fully open restarts its wait.
- Blocked [R]:
  - The blocker takes `dmg` every time the move is blocked, with no rate limit.
  - The attacker is whoever opened the door, so a player who opened it is credited:
     "K squishes V", +1.
  - Unless `wait` is negative, the door reverses.

**`func_plat`** [R]:

- Defaults: `speed` 150, `sounds` 2.
- Sounds:

  | `sounds` | Moving | Stopping |
  | --- | --- | --- |
  | 1 | `plats/plat1.wav` | `plats/plat2.wav` |
  | 2 | `plats/medplat1.wav` | `plats/medplat2.wav` |

- Travel is `height` if set, otherwise the model's height minus 8 u.
- It rests at the bottom, unless it has a `targetname`.
  - A targeted plat starts at the top and waits for its first use. That use lowers it, and it
     is a normal plat after that.
- The plat starts when a live player stands in its trigger. The trigger is inset 25 u from each
  edge, or is a 1 u line on an axis 50 u or narrower, and covers the plat's travel. Spawnflag 1
  makes it only 8 u tall, at the bottom.
- At the top the plat waits 3 s, and the wait is renewed to 1 s while a player stands in the
  trigger. Then it goes down.
- Blocked: 1 damage, cause crushing, attacker the plat, so "V was squished", −1. It reverses
  each time.

**`func_train`** [R]:

- Defaults: `speed` 100, `dmg` 2.
- `sounds` 1: `plats/train1.wav` while moving and `plats/train2.wav` at each stop that has a
  `wait`. Otherwise silent.
- Blocked: `dmg` at most once per 0.5 s.

### 14. Teleporters and pushers

**`trigger_teleport`** [R]:

- Spawnflag 1 (players only) refuses anything not of the player class. Only players and monsters
  can teleport, and monsters never exist here, so in practice it changes nothing [D].
- Spawnflag 2 (silent) only removes the teleporter's looping hum: `ambience/hum1.wav`, volume
  0.5, static attenuation, at the trigger's centre.
- The teleport flashes and their `misc/r_tele` sounds play either way.
- The destination point is 27 u above the `info_teleport_destination`.
- A teleporter with a `targetname` works only for 0.2 s after it is used.

**`trigger_push`** [R]:

- Spawnflag 1 (push once) removes the pusher after its first touch by anything: a player, a
  grenade, a gib or a corpse.
- `speed` defaults to 1000, and the push sets velocity to 10 × `speed` along its direction. So
  the default sets 10000 u/s [D].
- Live entities and grenades are pushed.
- Players hear `ambience/windfly.wav` on the auto channel, at most every 1.5 s.

---

## The intermission's messages

### 15. At the intermission

0.1 s after the map's end is decided [R]:

1. A CD track message carrying track 3 is sent reliably to everyone.
2. An intermission message is sent reliably to everyone. It carries the camera's origin as three
   coordinates, then its pitch, yaw and roll as three angles.
   - The camera is a random `info_intermission`, using its `mangle` angles.
   - Failing that it is the `info_player_start`, whose angles reach the message as zeros,
     because only intermission points set that field [D].
3. Every entity of the player class, alive or dead, has these changed:
   - it stops taking damage
   - it becomes non-solid
   - it stops moving
   - its model index is set to 0, so it is invisible

The players' view offsets and views are not touched by the program; the client places its camera
from the message [R]. The program sends no scoreboard [R].

**At the next map's load in deathmatch** [R]:

- A player alive at the change keeps their weapons, ammo and armour.
  - Health is clamped to 50–100.
  - Shells are raised to at least 25.
  - Keys and powerups are dropped.
- A dead player gets the default load-out.
- These values are used for the first spawn on the new map only.

### 16. Map change while a player is dead

- The end checks run in each player's pre-think, before the death handling, so a dead player can
  end the map. That covers both the time limit and their own frag limit [R].
- Once the intermission runs, the pre-think goes straight to the intermission's button check, so
  dead players cannot respawn [R].
- After 5 s, a dead player pressing attack, jump or use sends everyone to the next map, like any
  other player [D].
- The dead body becomes invisible with the rest [R]. Any of the four corpses in the corpse queue
  stay visible [D].
- A player still dead at the change gets the default load-out on the new map [R]. This includes
  one who is only dying [D].

---

## The lightning gun and axe, detail

### 17. Lightning gun traces

**The drawn beam** [R]:

- It starts 16 u above the player's origin.
- It runs along the view's forward direction for 600 u, or to the first wall, passing through
  players and other movers.

**The three damage lines** [R]:

- All three run from the player's origin (not raised by 16 u) to 4 u past the beam's end.
- The centre line runs exactly so.
- The two side lines run parallel to it, shifted one way and then the opposite way by an offset
  in the horizontal plane.

**The side offset** [D]:

- It is meant to be 16 u to the side, but it is built from the unnormalised line.
- Both of its horizontal components are 16 × the negative of the line's y extent.
- When aiming along the x axis the offset is about zero, so the side lines coincide with the
  centre.
- When aiming along y it is up to 16 × 604 u, so the side lines lie far away, in walls or open
  space.

**What each line hits** [R]:

- Each line stops at the first solid or damageable entity, and damages it if it can take damage.
- An entity hit by the centre line is skipped by both side lines. One hit by the first side line
  is skipped by the second. So no entity is hit twice in one shot.
- Up to three different entities can be hit, at 30 damage each.

### 18. Axe timing

- All four swings hit on their third frame: 0.2 s after the press, by the step clock (round 1,
  question 16) [R], [D]. With the command-granular stepping it can come up to one command early
  [D].
- The trace is computed at the hit, not at the press [R]:
  - It uses the view angles at that moment.
  - It starts 16 u above the origin as it is at that moment.
  - It reaches 64 u.
- So turning during the first 0.2 s changes where the axe lands [D].
- Nothing the player can do between press and hit cancels the swing, except dying. Pain is
  refused while firing, and impulses wait for the fire time [D].

---

# Round 3: keys, the sigil, level exits, the info keys, key doors, the limits

### 19. Key models

| Key | World type 0 | World type 1 | World type 2 |
| --- | --- | --- | --- |
| Silver (`item_key1`) | `progs/w_s_key.mdl` | `progs/m_s_key.mdl` | `progs/b_s_key.mdl` |
| Gold (`item_key2`) | `progs/w_g_key.mdl` | `progs/m_g_key.mdl` | `progs/b_g_key.mdl` |

- Both keys use the same box: from 16 u to each side, 24 u below the origin to 32 u above it, so
  32 × 32 × 56 u [R].
- Like any item, a key is raised 6 u and dropped to the floor 0.2 s after the map starts [R].
- World type 2's models are registered content [R].
- The program gives keys no effect, no glow and no light, and does not move them after placing
  them [R].
- Any spin or bob a player sees would come from flags stored in the model file itself. This
  program does not settle that [D].

### 20. `item_sigil` in deathmatch

The sigil is kept: the program spawns it in every mode [R].

**At spawn** [R]:

- Without spawnflags the map fails to load, with an error naming the entity.
- Spawnflags 1, 2, 4 and 8 pick the model:

  | Spawnflag | Model |
  | --- | --- |
  | 1 | `progs/end1.mdl` |
  | 2 | `progs/end2.mdl` (registered content) |
  | 4 | `progs/end3.mdl` (registered content) |
  | 8 | `progs/end4.mdl` (registered content) |

- Box: from 16 u to each side, 24 u below the origin to 32 u above it.
- It drops to the floor like other items.

**Touched by a live player** [R]:

- The player gets "You got the rune!", centre-printed.
- `misc/runekey.wav` plays on the item channel of the player.
- The screen flashes.
- The sigil disappears and never respawns.
- Its spawnflags, the low 4 bits, are added to the server's episode flags. Those flags survive map
  changes, and they decide `func_episodegate`, `func_bossgate` and the `start` map's next episode
  [R], [D].
- It fires its targets.

The player gains nothing in their inventory from it [R].

### 21. `trigger_changelevel` in deathmatch

**Touch tests** [R]:

- The only test is that the toucher is of the player class. There is no alive test in the
  program.
- Dead players are non-solid, so they do not touch triggers in practice. This is assumed from
  how triggers work, and is not checked in the server's movement code [D].
- After the first touch the trigger stops responding [R].

**`samelevel`** [R]:

- It is read as a console variable (cvar), not as an info key.
- The server registers `samelevel` (default 0) as a cvar that is also shown in the server info.
- The server's `serverinfo` console command also sets a cvar of the same name, so setting it that
  way works too [R]. `localinfo` does not touch cvars, so setting it there has no effect [R], [D].

**What the trigger does:** see round 1, questions 20 and 22.

### 22. `rj`, `axe`, `dq`, `dr`

- All four are read as info keys on the world [R].
- For the world, the server looks in the server info first. If the key is missing or empty there,
  it looks in the local info [R].
- So both places work, and a non-empty server info value wins over the local info [R].

**When each is read** [R]:

| Key | Read |
| --- | --- |
| `rj` | at every spawn |
| `axe` | at every spawn in deathmatch 4 |
| `dq` | at every death |
| `dr` | at every death |

### 23. The next-map key

- It is read the same way, as an info key on the world whose name is the current map's name [R].
- The server info is checked first, then the local info when the server info has no value [R].
- A non-empty server info value wins [R].

### 24. Key door, player has the key

**Message** [R]:

- No key message is shown.
- The door's own `message` is centre-printed with `misc/talk.wav` first, if the door has one and
  the 2 s throttle allows. It is shown at most once, since using the door clears it.

**The key** [R]:

- It is taken from the player's inventory, in every mode, deathmatch included.
- The player does not keep it [R].

**What happens to the door** [R]:

- The touched door and its paired door stop responding to touch.
- The door's use sound plays on the door's voice channel. It is chosen by the world's type:

  | World type | Sound |
  | --- | --- |
  | 0 | `doors/meduse.wav` |
  | 1 | `doors/runeuse.wav` |
  | 2 | `doors/baseuse.wav` |

- The linked doors open together and never close, because their `wait` is −1.

### 25. `fraglimit`, `timelimit`, `deathmatch`, `teamplay`

**How the program reads them** [R]:

- All four are read as console variables (cvars), once every server frame. They are never read
  as info keys.
- `timelimit` is in minutes.

**On the server side** [R]:

- The server registers each as a cvar that is also shown in the server info.

  | Cvar | Default |
  | --- | --- |
  | `fraglimit` | 0 |
  | `timelimit` | 0 |
  | `teamplay` | 0 |
  | `deathmatch` | 1 |

- So the info a browser sees mirrors the cvar [R].
- The server's `serverinfo` console command, given one of these names, sets the cvar as well, so
  the program sees the change [R]. A `localinfo` key of the same name has no effect [D].

---

## A compatibility gap found on the way (measured, 2026-11-21)

Building the stock QuakeWorld game program's source with our QC compiler (`ge qc build`) fails:
the source calls a function declared with two parameters with none, and our compiler refuses a
call with fewer arguments than declared, where the source's own compiler evidently accepted it
[D]. A mod source written that way does not build with ours until our compiler accepts it. Our compiler now accepts it with a warning (2026-11-21, ledger row T1).
