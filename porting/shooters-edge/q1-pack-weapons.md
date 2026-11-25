A WORKING AREA clean-room note (porting/, deleted when the port ends): facts written by a reader from source this tree may not copy; the implementer builds from this note alone.

# Quake mission packs: the held weapons, for weapon prediction (clean-room facts)

Read: the Scourge of Armagon QuakeC source (Hipnotic Interactive, 1997 release), the Dissolution
of Eternity QuakeC source (Rogue Entertainment, 1997 release), and id Software's released
base-game QuakeC for the comparisons in question 8. Checked against the compiled programs
shipped in each pack's pak0.pak (progs.dat), disassembled with this tree's `progs disasm` tool,
and against each pak's file directory: every item constant, the Mjolnir thresholds and timings,
the laser cannon's frames, the impulse numbers, the best-weapon orders, the player-frame hooks,
the weapon frame and the refire times sampled agree with the source read.

Tags: [read] = stated by the source or the shipped program; [measured] = taken from the pack
files; [derived] = worked out from read facts. Times are game seconds; "t0" is the moment of the
press (the weapon frame that starts the attack). "Think" is the player entity's frame think,
every 0.1 s as in the base game. Channels by number: auto 0, weapon 1, voice 2, item 3, body 4.

## 1. Weapon values and ownership bits

Scourge of Armagon (hipnotic) [read, values confirmed in the shipped program]:

- Laser cannon: `weapon` 8388608 (bit 23). Owned when `items` has 8388608.
- Mjolnir: `weapon` 128 (bit 7, the base game's unused "extra weapon" bit). Owned when `items`
  has 128.
- Proximity gun: `weapon` 65536 (bit 16). Owned when `items` has 65536. The pack gives up the
  base megahealth item bit (65536) for it: picking up a megahealth no longer sets any `items`
  bit, and the rot-down no longer clears one.
- No weapon uses `items2`. The pack's `items2` holds only power-ups (wetsuit 2, empathy shields
  4, horn of conjuring 8).
- The base weapons keep their base values (axe 4096, shotgun 1 ... lightning gun 64) and the base
  ammunition bits (shells 256, nails 512, rockets 1024, cells 2048).

Dissolution of Eternity (rogue) [read, values confirmed in the shipped program]:

- Lava nailgun: `weapon` 4096 (bit 12). Owned: `items` has 4096.
- Lava super nailgun: `weapon` 8192 (bit 13). Owned: `items` has 8192.
- Multi grenade launcher: `weapon` 16384 (bit 14). Owned: `items` has 16384.
- Multi rocket launcher: `weapon` 32768 (bit 15). Owned: `items` has 32768.
- Plasma gun: `weapon` 65536 (bit 16). Owned: `items` has 65536.
- Grapple (capture-the-flag only): `weapon` 8388608 (bit 23). Owned: `items` has 8388608.
- No weapon's ownership is in `items2`.
- The pack RENUMBERS base bits in `items`: the axe becomes 2048 (so a held axe's `weapon` is
  2048, not 4096), and the ammunition-type bits become shells 128, nails 256, rockets 512,
  cells 1024. The base weapons 1..64 keep their values. The three armour bits and megahealth
  move to `items2`: armour 1, 2, 4; megahealth 256. `items2` also holds the pack's
  ammunition-type bits: lava nails 8, plasma 16, multi rockets 32 (question 2), and power-ups
  (shield 64, anti-grav belt 128, earthquake 512, vengeance sphere 1024).
- [read] A lava or multi weapon is also granted without a pickup: on every weapon-select
  impulse, next/previous cycle, and some pickups, a player who owns the base gun and has more
  than 0 of the matching pack ammunition gets the pack gun's bit set in `items`, with a
  console line: "Lava Enabled" (nailgun + lava nails), "Super Lava Enabled" (super
  nailgun + lava nails), "Multi Grenades Enabled" (grenade launcher + multi rockets), "Multi
  Rockets Enabled" (rocket launcher + multi rockets). The plasma gun is never granted this way.

## 2. Ammunition

Scourge [read]:

| Weapon | Field drawn from | Per shot | Least to fire | `currentammo` while held | `items` ammo bit set |
| --- | --- | --- | --- | --- | --- |
| Laser cannon | `ammo_cells` | 1 cell per think (each think fires one or two bolts, question 3) | 1 | `ammo_cells` | cells (2048) |
| Mjolnir | `ammo_cells` | 0 for a melee swing; 15 for a lightning strike | none: it fires with 0 cells | `ammo_cells` | cells (2048) |
| Proximity gun | `ammo_rockets` | 1 | 1 | `ammo_rockets` | rockets (1024) |

- [read] Mjolnir is never "out of ammunition": the empty-weapon check that drops to the best
  weapon treats it like the axe, and so does the per-frame check that switches away from an
  empty weapon (question 9). A lightning strike needs at least 15 cells; fewer gives a swing.
- [read] Selecting the laser cannon by impulse needs at least 1 cell; Mjolnir needs none; the
  proximity gun needs at least 1 rocket.

Dissolution [read]:

The pack keeps the real stock of the base ammunition in four NEW fields, `ammo_shells1`,
`ammo_nails1`, `ammo_rockets1`, `ammo_cells1`, and its own in `ammo_lava_nails`,
`ammo_multi_rockets` and `ammo_plasma`. The base fields `ammo_shells`, `ammo_nails`,
`ammo_rockets`, `ammo_cells` (the ones the status bar shows) become a display copy, rewritten
after every shot, weapon change and ammunition pickup:

- while the held `weapon` value is 4096 or more (any of the five pack guns, or the grapple):
  shells shows `ammo_shells1`, nails shows `ammo_lava_nails`, rockets shows
  `ammo_multi_rockets`, cells shows `ammo_plasma`; - otherwise (any base weapon, including the axe at 2048): each shows its `...1` field.

[derived] So a client sees the four ammunition stats switch meaning when the player moves between
a base gun and a pack gun, and every base gun also spends from the `...1` fields.

| Weapon | Field drawn from | Per shot | Least to fire | `currentammo` while held | ammo-type bit set |
| --- | --- | --- | --- | --- | --- |
| Lava nailgun | `ammo_lava_nails` | 1 per think | 1 | `ammo_lava_nails` | `items2` 8 |
| Lava super nailgun | `ammo_lava_nails` | 2 per think; 1 when only 1 is left (fires a single lava nail) | 1 | `ammo_lava_nails` | `items2` 8 |
| Multi grenade launcher | `ammo_multi_rockets` | 1 | 1 | `ammo_multi_rockets` | `items2` 32 |
| Multi rocket launcher | `ammo_multi_rockets` | 1 (launches 4 rockets) | 1 | `ammo_multi_rockets` | `items2` 32 |
| Plasma gun | `ammo_plasma` | 1 | 1 | `ammo_plasma` | `items2` 16 |
| Grapple | none | 0 | none | 0 | none |

- [read] When a pack gun is held, none of the four `items` ammunition-type bits is set; the
  `items2` bit above is set instead. Base guns set the renumbered `items` bit (shells 128, nails
  256, rockets 512, cells 1024). Every weapon change clears all seven bits first.
- [read] Selection thresholds by impulse: lava nailgun 1, lava super nailgun 2 (by key 5) but 1
  (by impulse 61), multi launchers 1, plasma 1. Cycling thresholds are in question 6.

## 3. Firing, step by step

### Scourge: laser cannon [read unless marked]

- Press (t0): no `attack_finished` is set by the press itself; the first think runs at once.
- Think A (t0): player body frame "nail attack 1"; `attack_finished` = time + 0.1; `weaponframe`
  1; next think at time + 0.1. If the button is no longer held: nothing fires and the player goes
  back to the run frames (`weaponframe` 0). If cells are below 1: switch to the best weapon
  (question 7). Otherwise: 1 cell spent, muzzle flash, two bolts launched side by side, about
  4.2 units each side of centre and about 1.8 units above the origin, 12 units ahead on the level
  forward line, both autoaimed the same way; view kick of 1 degree up.
- Think B (t0 + 0.1): body frame "nail attack 2"; `attack_finished` = time + 0.1; `weaponframe` 4; same release and ammunition checks; 1 cell spent; ONE bolt launched from the centre line, 12
  units above the origin; 10% of these single bolts glow (dim light) and do 25 damage instead of
  18. Every bolt: 1000 units/s, 18 damage (halved on a second hit on the same victim), lives 5 s,
  ricochets off walls keeping 90% of its damage and stops on its third touch or by chance (15%).
- Then A, B, A, B ... every 0.1 s while the button is held: 10 cells per second, alternating a
  twin bolt (weapon frame 1) and a single bolt (weapon frame 4). Frames 2, 3, 5 and 6 of the
  view model are never shown by the program [read; the model has 7 frames, measured].
- Stops at the first think after release (that think still sets `attack_finished` = time + 0.1
  and leaves `weaponframe` 0), or when cells run out.
- The first shot is at the press itself.
- Quad sound is checked at every think, as the base nailgun does.

### Scourge: Mjolnir [read unless marked]

- Press (t0): the player body animation is chosen by cells: under 30 cells one six-frame swing
  (body frames 32-37 of the hammer-holding player model), 30 or more another (frames 38-43); the view frames are the same for both. `attack_finished` = t0 + 0.8.
- t0, t0 + 0.1, t0 + 0.2: `weaponframe` 1, 2, 3.
- t0 + 0.3: `weaponframe` 4 and the blow (question 5). The blow sets `attack_finished` =
  time + 0.4, that is t0 + 0.7, replacing the 0.8; when it becomes a lightning strike it sets
  time + 1.5 instead, that is t0 + 1.8.
- t0 + 0.4, t0 + 0.5: `weaponframe` stays 4. t0 + 0.6: back to the run frames, `weaponframe` 0.
- [derived] With the button held, the next swing starts at the first weapon frame at or after
  t0 + 0.7 (t0 + 1.8 after a strike). No release check: a press always completes the swing.
- [derived] With 15-29 cells the "light" body swing plays but a strike can still happen.
- The view model has 5 frames (0-4) [measured].

### Scourge: proximity gun [read]

- Press (t0): the grenade launcher's player and view frames, unchanged; the grenade is thrown at
  the press; `attack_finished` = t0 + 0.6; 1 rocket spent; view kick 2 degrees up. Held: one
  throw per 0.6 s. No per-shot alternation. Firing-end differences: question 5.

### Dissolution: lava nailgun and lava super nailgun [read]

- Exactly the base nailgun's timing and frames: the first think at the press; every think
  (0.1 s) the body alternates "nail attack 1" and "nail attack 2", `weaponframe` advances by
  one and wraps from 9 back to 1, `attack_finished` = time + 0.2, muzzle flash, quad sound
  check, then a shot. Release is checked at the top of each think (nothing fires, run frames,
  `weaponframe` 0).
- Lava nailgun: alternating barrel offset, 4 units to the right on body frame 1 and 4 to the
  left on body frame 2; 1 lava nail per think; view kick 2 degrees.
- Lava super nailgun: with 2 or more lava nails, one lava super spike from the centre (2 nails); with exactly 1, a single lava nail at the side offset as the lava nailgun.
- Out of lava nails at a think: the console line "Out of Lava Nails", then switch to the best
  weapon.
- Lava nail: speed 1000 units/s, life 6 s. Damage: 9 to players (armour ignored), 15 to monsters.
  Lava super spike: 18 to players (armour counted at half its type), 30 to monsters. The lava
  creature takes none.

### Dissolution: multi grenade launcher [read]

- The grenade launcher's frames and timing: thrown at the press, `attack_finished` = t0 + 0.6
  (0.4 with the Hell rune, question 8), 1 multi rocket spent, view kick 2 degrees. Held: one
  per 0.6 s.

### Dissolution: multi rocket launcher [read]

- The rocket launcher's frames and timing: fired at the press, `attack_finished` = t0 + 0.8
  (about 0.533 with the Hell rune), 1 multi rocket spent, 4 rockets launched in one moment,
  view kick 2 degrees.
- Spread: single player, the four leave from 8 units ahead and 16 up at 1000 units/s forward
  with side velocities of 80 and 40 units/s to the right and 40 and 80 to the left, then home: a rocket whose path
  already meets a monster locks on at once; otherwise every 0.2 s it re-aims and locks onto a
  monster in line; locked rockets steer straight at the target at 1000 units/s every 0.1 s; they explode after 4 s if unlocked. Deathmatch and co-op: no homing; yaw offsets of -6.6,
  -3.3, +3.3, +6.6 degrees, autoaimed, 1000 units/s, explode after 4 s.
- Damage: 60-75 on impact plus 75 radius (half on the shambler and the dragon), versus the base
  rocket's 100-120 and 120.

### Dissolution: plasma gun [read]

- Press (t0): `attack_finished` = t0 + 1.0 (about 0.667 with the Hell rune) is set FIRST, then
  one think of the lightning gun's body frame "light 1": muzzle flash, `weaponframe` advanced by
  one (0 to 1), quad sound check, one plasma bolt fired, and in the same moment back to the run
  frames, `weaponframe` 0.
- [derived] The client never sees a firing view frame: `weaponframe` is 0 again before the
  frame is sent. One bolt per press; held, one bolt every 1.0 s. No alternation.
- Bolt: leaves from 24 units ahead and 16 up, starts nearly still, and after 0.1 s flies at
  1250 units/s along the autoaimed direction; removed after 5 s. Impact: 80-100 to the thing
  hit (half on the shambler), 70 radius, then up to 5 arcs to visible monsters or players within
  320 units, 50 each.

### Grapple (Dissolution, capture-the-flag) [read]

- Press: `attack_finished` = t0 + 0.1. Not covered further here.

## 4. Sounds when firing

All are played at full volume with normal attenuation unless stated [read].

Scourge:

- Laser cannon: "hipweap/laserg.wav" on the weapon channel from the player, once per bolt
  launched, so twice in the twin-bolt think (the second replaces the first on the same channel)
  and once in the single-bolt think. No random choice. A bolt that ricochets plays
  "hipweap/laserric.wav" (weapon channel of the bolt, static attenuation); a bolt that stops
  plays "enforcer/enfstop.wav" (same).
- Mjolnir: nothing at the press. At the blow (t0 + 0.3): a creature hit, no sound (blood only); a wall hit, "hipweap/mjoltink.wav" on the weapon channel; nothing within reach,
  "knight/sword1.wav" on the weapon channel; a lightning strike, "hipweap/mjolslap.wav" (auto
  channel) and "hipweap/mjolhit.wav" (weapon channel), both from a short-lived entity at the
  struck ground point. The pak also carries "hipweap/mjoltap.wav" and "hipweap/mjolwam.wav",
  which the program never names [measured].
- Proximity gun: "hipweap/proxbomb.wav" on the weapon channel at the press (where the grenade
  launcher plays "weapons/grenade.wav"). Landing: "weapons/bounce.wav". Triggered:
  "hipweap/proxwarn.wav".

Dissolution:

- Lava nailgun: "weapons/rocket1i.wav" on the weapon channel at each single lava nail (as the
  nailgun). Lava super nailgun: "weapons/spike2.wav" on the weapon channel per super spike (as
  the super nailgun).
- Lava cool-down: "lavagun/snail.wav" on the weapon channel, once, after a burst ends: the
  program raises one program-wide flag at every lava press, and at the first weapon frame (of
  any player) whose `attack_finished` has passed it plays the sample if THAT player holds a lava
  gun, then lowers the flag. [derived] Single player: about 0.2 s after the last lava shot
  (when the last think's `attack_finished` lapses). In a multiplayer game another player's weapon
  frame can lower the flag first, so the sample may not play, or may play from a different
  player who holds a lava gun.
- Multi grenade launcher: "weapons/grenade.wav" on the weapon channel at the press (as the
  grenade launcher).
- Multi rocket launcher: "weapons/sgun1.wav" on the weapon channel at the press (the base rocket
  launcher's sample is the same one).
- Plasma gun: "plasma/fire.wav" on the weapon channel at volume 0.5 at the press; the bolt
  carries "plasma/flight.wav" on its own weapon channel; impact "plasma/explode.wav"; each arc
  "weapons/lhit.wav" on the voice channel. No "weapons/lstart.wav" (the lightning gun's start
  sample is not played).
- Runes (deathmatch with runes on): every attack press while carrying the Black Magic rune plays
  "runes/end2.wav" on the item channel, at most once a second; every refire shortened by the
  Hell Magic rune plays "runes/end3.wav" on the item channel, at most once a second.

## 5. Special cases

Scourge, Mjolnir [read unless marked]:

- The blow traces 32 units straight ahead (view direction, pitch included) from 16 units above
  the origin.
- Something that takes damage within 32: 50 damage (70 to a zombie), blood, no sound.
- Nothing hit within 32 AND at least 15 cells: a second trace 50 units straight down from the
  32-unit point. If ground is found between 15 and 50 units below that point (trace fraction
  above 0.3 and below 1), it is a lightning strike:
- Strike underwater (waterlevel above 1, waist deep or more): a DISCHARGE: every cell is spent and the player
  deals 35 x cells radius damage with itself as attacker; while it runs, anyone wearing the
  wetsuit takes no damage from it. `attack_finished` = time + 1.5.
- Strike out of the water: 15 cells spent; four lightning chains start at the struck point. Each lives
  0.8 s, looks within 350 units for a visible, living monster or player (never the wielder,
  never one already struck by another chain), nearest first, and strikes it: 80 damage on the
  first strike, 30 after, halved when the target lies more than about 72 degrees off the
  swing's direction; after a strike it waits 0.2 s, after a miss 0.1 s. With nothing to strike
  it draws a stray bolt 200 units out, up to 200 to either side.
- Otherwise (fewer than 15 cells, or ground too near or too far): the blow uses the last trace.
  [derived] When the second trace ran, a "wall" means the floor below the 32-unit point:
  "hipweap/mjoltink.wav" and a gunshot puff there when it found ground within 15 units, and the
  miss sample when it found none within 50.
- With 0 cells it swings for 50/70 forever; it never switches away by itself.
- Underwater with fewer than 15 cells, or with something within reach, it is an ordinary
  swing.

Scourge, proximity gun versus the grenade launcher, at the firing end [read]:

- Same launch velocity rule (600 forward and 200 up, with 10 units of random spread each way,
  or autoaim with 200 up when looking level), same origin, same view kick (2 degrees), same 0.6 s
  refire, same frames.
- Different: the sample (question 4); the model "progs/proxbomb.mdl"; a 2x2x2 box instead of a
  point; tossed (no bounce: it sticks where it lands, then grows to a 16-unit box); spin 100,
  600, 100 degrees/s instead of 300 on each axis; it can be shot (health 5) once armed; 1 rocket
  from `ammo_rockets`.
- Life: armed after 2 s, then checks every 0.25 s for a living monster or player (or an unarmed
  proximity grenade still in flight) within 140 units in clear sight; then the warning sample,
  and it explodes 0.5 s later for 95 radius damage. It also explodes when it lands on something moving, when what it stuck to moves, when
  it hits something that can be aimed at, after 15-25 s (random), and whenever more than 15 are
  alive in the world at its check; those triggers detonate it at once.
- The owner can be hurt by his own once armed.

Dissolution, multi grenade launcher versus the grenade launcher, at the firing end [read]:

- Same launch velocity rule, spin, origin, view kick, refire, frames and sample.
- Different: 1 from `ammo_multi_rockets`; model "progs/mervup.mdl"; fuse 1 s instead of 2.5 s; a direct hit on something that can be aimed at explodes it as an ordinary grenade (120
  radius); at the fuse it splits into 5 small grenades (yaw 0, 72, 144, 216, 288 degrees, thrown
  100 forward and 400 up with random spread), each exploding after 0.5-1.5 s or on a direct hit,
  90 radius damage when thrown by a player (60 by a monster).

Dissolution, plasma gun versus the lightning gun [read]:

- A projectile weapon, one shot per press, 1.0 s refire (the lightning gun: a beam every think,
  0.1 s after the press and 0.2 s per think).
- Shares the lightning gun's key (8), its body frame "light 1", its muzzle flash.
- No start sample; its own fire sample at half volume.
- Its own ammunition, `ammo_plasma`.
- Underwater (waterlevel above 1): it also discharges, the same way the lightning gun does: all
  plasma spent, 35 x the count as radius damage, self as attacker; the ammunition display is
  rewritten before the blast.
- Never chosen by the best-weapon rule.

## 6. Choosing

Scourge [read]:

- Impulse 225: laser cannon (not enough ammunition below 1 cell). Impulse 226: Mjolnir (no
  ammunition condition).
- Impulse 6: when the held weapon is the grenade launcher, it asks for the proximity gun; otherwise for the grenade launcher. If the asked weapon is the grenade launcher and the player
  does not own it, it asks for the proximity gun instead (and the ammunition test is redone).
  Both need 1 rocket. [derived] So 6 toggles grenade launcher to proximity gun to grenade
  launcher. Holding the grenade launcher without owning the proximity gun, 6 prints "no
  weapon." and does nothing.
- Impulses 1-5, 7 and 8 are the base game's. Mjolnir and the laser cannon have no number key
  besides 225 and 226.
- Impulse 10 (next) order: axe, shotgun, super shotgun, nailgun, super nailgun, grenade
  launcher, proximity gun, rocket launcher, lightning gun, laser cannon, Mjolnir, then the axe
  again. Needs: shotgun 1 shell, super shotgun 2 shells, nailgun 1 nail, super nailgun 2 nails,
  both launchers and the rocket launcher 1 rocket, lightning gun and laser cannon 1 cell; Mjolnir and the axe nothing. Each step skips a weapon not owned or not fed.
- Impulse 12 (previous): the exact reverse, with the same needs.
- Other new impulses (single player only, cheats): 200 wetsuit for 30 s, 201 empathy shields for
  30 s, 205 kills every monster; development: 202 and 203 list entities on the developer
  console, 206 toggles printing the player's position every frame. Impulse 9 (all weapons) also
  gives the three new weapons. None changes the weapon.

Dissolution [read]:

- New impulses: 60 lava nailgun (needs 1 lava nail), 61 lava super nailgun (needs 1), 62 multi
  grenade launcher (1 multi rocket), 63 multi rocket launcher (1), 64 plasma gun (1 plasma).
- Shared keys: 4 (nailgun / lava nailgun), 5 (super nailgun / lava super nailgun), 6 (grenade
  launcher / multi), 7 (rocket launcher / multi), 8 (lightning gun / plasma gun). Each asks for
  the pack gun when the player owns it AND either holds the base gun of that key or has no base
  ammunition for it (nails below 1 for 4, below 2 for 5, rockets below 1 for 6 and 7, cells
  below 1 for 8); otherwise it asks for the base gun. The ammunition test then follows what was
  asked (pack: lava nails 1 for 4 and 2 for 5, multi rockets 1, plasma 1).
- [derived] With both owned and both fed, the key toggles base to pack to base. With the base gun
  empty, the key always asks for the pack gun (pressing it while holding the pack gun re-selects
  it). With the pack gun owned and empty while the base gun is held, pressing the key again prints
  "not enough ammo." and keeps the base gun.
- On a change between the pair the player reads a line: to the base gun "Normal Nails", "Normal
  Grenades", "Normal Rockets", "Lightning Gun"; to a pack gun (from anything except its own lava
  or multi twin) "Lava Nails!", "Multi-Grenades!", "Multi-Rockets!", "Plasma Gun!".
- Impulse 1: in a deathmatch with `teamplay` 4 or more (capture the flag), pressing it while
  holding the axe asks for the grapple; otherwise the axe. Impulse 22: the grapple, in that mode
  only (otherwise "no weapon.").
- Impulse 10 (next) order: axe, (grapple, capture the flag only), shotgun, super shotgun,
  nailgun, lava nailgun, super nailgun, lava super nailgun, grenade launcher, multi grenade
  launcher, rocket launcher, multi rocket launcher, lightning gun, plasma gun, then the axe.
  Needs: lava nailgun 1 lava nail, lava super nailgun 2, multi launchers 1 multi rocket, plasma
  1, base guns as in Scourge but counted from the `...1` fields.
- Impulse 12 (previous): the reverse, plasma gun to lightning gun ... to shotgun, (grapple), axe,
  then the plasma gun. One difference from the forward needs: stepping back from the super
  nailgun to the lava nailgun needs 2 lava nails, not 1. [read]
- Every select and cycle first grants combo guns (question 1) and rewrites the ammunition
  display (question 2).
- Other new impulses (teamplay): 20 drops a backpack, 21 drops the held weapon (deathmatch 1
  with teamplay only; not for the axe, shotgun or grapple; a lava or multi gun is dropped as its
  base gun and both bits are taken away), 23 prints the flag status. Impulse 9 (all weapons) also
  gives the five pack guns, 200 lava nails, 100 multi rockets and 100 plasma.

## 7. Best weapon when out of ammunition

Scourge [read]: first match wins:

1. lightning gun, when waterlevel is 1 or less and cells 1 or more; 2. laser cannon, cells 1 or more (no water condition); 3. super nailgun, nails 2 or more; 4. super shotgun, shells 2 or more; 5. nailgun, nails 1 or more; 6. shotgun, shells 1 or more; 7. Mjolnir, when owned (no ammunition condition); 8. the axe.

The launchers are never chosen (as in the base game).

Dissolution [read; order confirmed in the shipped program]:

1. lightning gun, when waterlevel is 1 or less and `ammo_cells1` 1 or more; 2. lava super nailgun, lava nails 2 or more; 3. super nailgun, `ammo_nails1` 2 or more; 4. lava nailgun, lava nails 1 or more; 5. nailgun, `ammo_nails1` 1 or more; 6. super shotgun, `ammo_shells1` 2 or more; 7. shotgun, `ammo_shells1` 1 or more; 8. the axe (value 2048).

Never chosen: the plasma gun, both multi launchers, both base launchers.

## 8. Changes to the base weapons

Scourge [read]:

- Lightning gun beam: a target wearing the wetsuit takes no beam damage (all three of the beam's
  traces skip it).
- Lightning gun underwater discharge: same 35 x cells radius damage, but while it runs anyone
  wearing the wetsuit takes no damage; and the ammunition display is rewritten after the blast
  instead of before it.
- Shotgun and super shotgun: the first pellet of each shot that hits the world leaves a bullet
  hole sprite ("progs/s_bullet.spr", lives 300 s, at most 10 in the world). Damage, spread and
  timing unchanged.
- The automatic switch away from an empty weapon skips Mjolnir as well as the axe (question 9).
- Best weapon, cycle and impulse 6 orders as above.
- Megahealth bit given up (question 1).
- Unchanged: the axe, every refire, every frame, every sample, every ammunition cost and the
  nail, grenade and rocket behaviour.

Dissolution [read]:

- Item bits renumbered (axe 2048; ammunition-type bits 128-1024; armour and megahealth in
  `items2`), question 1.
- Base ammunition stock moved to `ammo_shells1`, `ammo_nails1`, `ammo_rockets1`,
  `ammo_cells1`; the base fields are a display copy (question 2). Each base gun's cost is
  unchanged.
- Lightning gun: the beam's damage traces start 16 units above the origin (the base starts them
  at the origin; the drawn beam already started 16 up in both). Otherwise unchanged, including
  the discharge.
- Runes (deathmatch with the rune setting on): the Hell Magic rune multiplies the refire set at
  the press by 2/3 for the axe (0.5 to about 0.333 s), shotgun (0.5), super shotgun (0.7 to about
  0.467), grenade launcher (0.6 to 0.4), rocket launcher (0.8 to about 0.533), and the pack's
  multi launchers and plasma gun; NOT the nailguns, lava nailguns or the lightning gun (their
  thinks set their own times) or the grapple. The Black Magic rune doubles damage and adds the
  sample of question 4.
- The automatic switch away from an empty weapon is removed (question 9).
- Impulse 1 may give the grapple in capture the flag.
- Best weapon and cycle orders as above.
- Unchanged: the frames, samples, damage and costs of the axe, shotgun, super shotgun, nailgun,
  super nailgun, grenade launcher and rocket launcher.

## 9. What a predictor must know

Both packs [read]:

- The weapon logic still runs once per player frame after the physics, at the same point as in
  the base game; neither pack moves it. It still returns at once while time is before
  `attack_finished`, then handles a pending impulse, then the attack button.
- `weaponframe` and `weaponmodel` keep their meaning (the frame index into the view model named by
  `weaponmodel`); every weapon change sets `weaponframe` 0 and stops any firing animation.
- `attack_finished` keeps its meaning. The new think-driven weapon (the laser cannon) sets it to
  time + 0.1 at each think, equal to the think interval, where the base nailguns use 0.2. [The
  source does not settle whether a weapon frame can slip in between two laser thinks; that
  depends on the engine's think scheduling.]

Scourge [read]:

- Mjolnir swaps the player's body model for "progs/playham.mdl" (third person only; the view
  model is "progs/v_hammer.mdl").
- The base game switches an empty weapon to the best one at the start of the player frame when
  `attack_finished` has passed and `currentammo` is 0; Scourge exempts Mjolnir as well as the axe.
- The held `weapon` value can be 8388608, above the byte the base protocol sends (see
  `mission-packs-hud.md`, which covers the pack-mode weapon stat).

Dissolution [read unless marked]:

- The empty-weapon switch at the start of the player frame is REMOVED: an empty weapon stays held
  until the next press (the press finds it empty and switches; the nailguns' and lightning gun's
  thinks also switch when they run dry).
- The four base ammunition fields change meaning with the held weapon (question 2): predict a
  shot's cost against the copy the display shows, and expect the counts to jump when the weapon
  crosses the 4096 line.
- `items2` bits 8, 16, 32 mark the held pack gun's ammunition; the `items` ammunition-type bits
  are renumbered.
- The axe's `weapon` value is 2048.
- Hell rune: press refires are 2/3 of the table's values for the weapons listed in question 8.
- The plasma gun shows no firing view frame (question 3).
- [derived] The lava cool-down sample is not tied to a time a client can compute in a
  multiplayer game (question 4).
- The weapon logic handles an impulse only when one is pending; with none it goes straight to
  the button (no behaviour difference).

## 10. View models

Scourge [read; frame counts measured]:

- Laser cannon "progs/v_laserg.mdl" (7 frames); Mjolnir "progs/v_hammer.mdl" (5 frames); proximity gun "progs/v_prox.mdl" (7 frames).
- Projectiles and bodies: laser bolt "progs/lasrspik.mdl", proximity grenade
  "progs/proxbomb.mdl", player holding Mjolnir "progs/playham.mdl".

Dissolution [read; frame counts measured]:

- Lava nailgun "progs/v_lava.mdl" (9 frames); lava super nailgun "progs/v_lava2.mdl" (9); multi grenade launcher "progs/v_multi.mdl" (7); multi rocket launcher "progs/v_multi2.mdl"
  (7); plasma gun "progs/v_plasma.mdl" (5); grapple "progs/v_grpple.mdl".
- Projectiles: lava nail and lava super spike "progs/lspike.mdl"; multi grenade and its small
  grenades "progs/mervup.mdl"; multi rocket "progs/rockup.mdl" in single player and
  "progs/rockup_d.mdl" in deathmatch and co-op; plasma bolt "progs/plasma.mdl".
- The pak also holds "progs/v_star.mdl", which the program never names [measured].

## The missiles at launch, measured (2026-11-24)

Measured black box with `se-weapon-predict --packs=<folder>` (its launch table): each pack's own
program run on our server, single play, level, facing +x, the missiles new in the first frame
after one press. Places are after one 0.013 s frame of flight; the launch place below takes that
flight back off [measured, the launch derived].

- Laser cannon (twin bolts): progs/lasrspik.mdl, 12 ahead, 1.8 up, 4.2 to each side, 1000 units/s
  along the aim.
- Proximity gun: progs/proxbomb.mdl, from the origin, 600 along the aim and 200 up (as the
  grenade launcher).
- Lava nailgun: progs/lspike.mdl, as the base nailgun's nail (0 ahead, 16 up, 4 to the barrel's
  side, 1000); the lava super nailgun the same model from the middle.
- Multi grenade launcher: progs/mervup.mdl, as the grenade launcher.
- Multi rocket launcher (single play): four progs/rockup.mdl, 8 ahead and 16 up, 1000 along the
  aim with 80 and 40 units/s to each side.
- Plasma gun: progs/plasma.mdl, 24 ahead and 16 up, still in its first frame.
- For comparison the base game's nail leaves 0 ahead (not 8) and 16 up; the rocket 8 ahead.
