# Quake II: The Reckoning (`xatrix`), the pack's rules: weapons, items, settings, words, map things

**A WORKING note of facts**, read by a clean-room reader from the GPL release of the mission pack's
game library (the Yamagi Quake II project's `xatrix` repository, commit b9cf539, with its history)
and compared with the base game's library. No code was copied; every fact is in our words and
marked **[R]** read, **[D]** derived (the reasoning is given) or **[M]** measured. It records only
what the pack adds or changes against `q2-rules.md` (the base game's rules); the pack's monsters
are in the monster notes. "Original:" marks where the port changed id's 3.20 behaviour; the
original is what the pack shipped.

Units: map units, seconds, a step or frame is 0.1 s, degrees. Numbered sections follow the
reader's questions (`reckoning-a`).

## 1. The ion ripper (`weapon_boomer`)

- Pickup name **Ionripper** (item index 17). World model `models/weapons/g_boom/tris.md2`
  (rotating, effect bit 1), view model `models/weapons/v_boomer/tris.md2`, icon `w_ripper`,
  pickup sound `misc/w_pkup.wav`; the item also precaches `weapons/rg_hum.wav` and
  `weapons/rippfire.wav`. [R]
- Ammo **Cells**, **2 a shot** (the weapon's per-shot amount). The weapon pickup gives the cell
  item's amount, **50 cells** (1000 with deathmatch infinite ammo, held to the carry limit); respawn 30 s in deathmatch like every weapon. [R]
- Unlike every base weapon it is **not** a "stays in coop" item, so in coop it can be dropped (the
  base removes the drop action only from coop-stay items). [R] [D]
- Its player-model weapon index is **13** (`#w_ripper.md2`, question 24). [R]
- There are no weapon slots in this game: it is chosen by name (`use ionripper`, `use Ionripper`:
  item names match without regard to case) or by the HyperBlaster toggle (question 7); in the
  cycle order it sits after the HyperBlaster and before the Railgun. [R]
- **Frames** (last of each: activate, fire, idle, deactivate) **4, 6, 36, 39**: activate 0–4,
  fire 5–6, idle 7–36, deactivate 37–39. Fire frame **5**; pause frame **36**. [R] The pause never
  takes effect: at the last idle frame the frame wraps to the first idle frame before the pause
  test is made. [D]
- **Firing**: the shot leaves in the same step the attack is seen (frame 5, which the shot moves
  to 6), then 6 to 7, then 7 to 8 makes it ready; held, it fires again on the next step. So **one
  bolt every 0.3 s** while held and stocked with 2 or more cells (base period rule: 6 − 4 + 1 = 3
  steps). With 1 cell the attack gives the base's no-ammo click and the no-ammo change. [D]
- `use` on it with 1 cell prints "Not enough Cells for Ionripper." and with none "No Cells for
  Ionripper." (unless `g_select_empty` is set). [R]

## 2. The ion ripper's bolt

- **Damage 30 in deathmatch, 50 otherwise**; ×4 under the quad (the quad sound plays on the shot
  as for every weapon with fire frames). [R]
- **Speed 500** units/s. **Aim**: the view's yaw plus a uniform random **−1° to +1°** each shot
  (no alternation; pitch unchanged). Start: offset 16 forward, 7 right (the hand rule as base),
  view height − 8 up. View kick: the view origin pushed back 3 along the aim and pitch −3. [R]
- **The projectile**: a point (no size), the shot mask, no gravity, solid box (touches). Its own
  move kind: on hitting anything it **reflects perfectly**: the velocity's component along the
  surface normal is reversed with nothing lost (an overbounce of 2; components below 0.1 are set
  to 0, as the base's clip rule does), its facing is turned to the new direction, and it never
  comes to rest on a floor. **No bounce limit and no speed loss**: it bounces until its life ends.
  [R] [D]
- **Life 3 s**; then it sends temporary entity **25** (welding sparks) with a count of **0**, its
  position, a zero direction and colour **228 + random 0..3**, and is removed. [R] With a count of
  0 the client may draw no particles (client behaviour, not settled here). [D]
- **Touch**: its owner is ignored; the sky removes it silently; every other touch makes an impact
  noise monsters hear (when the owner is a player). Something that takes damage takes the damage
  with **knockback 1**, the energy flag, means of death **34**, and the bolt is removed (no
  temporary entity of its own: the energy flag gives the base's sparks). A wall or any
  non-damageable thing only bounces it. [R]
- When the line from the shooter's origin to the bolt's start is blocked, the bolt is moved back
  10 and touches at once (the base bolt rule). [R]
- Effects: effect bit **0x00100000** (1048576, the ion ripper's trail) and render flag **8** (full
  bright). Model `models/objects/boomrang/tris.md2`, looping sound `misc/lasfly.wav`. [R]
- Muzzle flash **16** (ORed with 128 when silenced). The library plays no firing sound itself; the
  client's handling of flash 16 does (the item precaches `weapons/rippfire.wav`). [R] [D]
- A damage radius of 100 is stored on the bolt but nothing reads it: no splash. [R]
- Deathmatch infinite ammo (dmflags 8192): no cells used. [R]
- Obituary: question 17.

## 3. The phalanx (`weapon_phalanx`)

- Pickup name **Phalanx** (index 19). World model `models/weapons/g_shotx/tris.md2` (rotating),
  view model `models/weapons/v_shotx/tris.md2`, icon **`w_phallanx`** (spelled with two l's),
  pickup sound `misc/w_pkup.wav`, precaches `weapons/plasshot.wav`. Not a coop-stay item. [R]
- Ammo **Mag Slug**, **1 a trigger pull** (taken when the second ball leaves); needs 1 to fire.
  The weapon pickup gives **10 mag slugs**. [R]
- While it is the weapon in hand, the player entity's looping sound is **`weapons/phaloop.wav`**
  (in the same place the base gives the railgun its hum and the BFG its hum; lava or slime's fry
  sound wins over it). [R]
- Player-model weapon index **12** (`#w_phalanx.md2`). [R]
- **Frames 5, 20, 58, 63**: activate 0–5, fire 6–20, idle 21–58, deactivate 59–63. Pause frames
  **29, 42, 55**; fire frames **7 and 8**. [R]
- **Timeline of a pull** [D]: the step that sees attack sets frame 6 (not a fire frame) and steps
  to 7; the next step fires ball one (frame 7 → 8); the next fires ball two (8 → 9); frames then
  step to 22, which makes it ready on the 15th step after the press. Held, the next pull starts on
  the 16th step: **a pair every 1.6 s**; ball one leaves **0.1 s** and ball two **0.2 s** after the
  press.
- The quad's sound `items/damage3.wav` plays at **both** fire frames under quad. [R]
- In the cycle order it sits after the Railgun and before the BFG10K; `use railgun` toggles to it
  (question 7). [R]

## 4. The phalanx's projectiles

- **Two a pull**, one per fire frame. [R]
- **Yaw offsets**: the frame 7 ball flies at the view's yaw **+1.5°**, the frame 8 ball at
  **−1.5°** (yaw grows counter-clockwise seen from above, so +1.5° is to the left). Pitch and roll
  are the view's. [R] [D]
- **Start**: offset 0 forward, 8 right, view height − 8 up, worked out from the unturned aim; only
  the flight direction is turned by ±1.5°. (The port's `aimfix` setting, default 0, instead
  recomputes the start from the turned aim; at the default this is the original behaviour.) [R]
- **View kick** for each ball: the view origin back 2 along the aim, pitch −2. [R]
- **Direct damage 70 + a random whole number 0..9**, drawn separately for each ball; ×4 under the
  quad. [R]
- **Splash**: ball one **120 damage over radius 120**; ball two **30 over radius 120**. Under the
  quad ball one's splash damage is ×4 (480) but ball two's stays 30 (its splash amount is set
  after the quad's multiply). Radii are never multiplied. [R] [D]
- **Speed 725** units/s; it lives 8000 / 725 = **11.03 s** and is then removed silently. [R] [D]
- A point projectile (no size), the shot mask, no gravity. Model **`sprites/s_photon.sp2`**,
  effects **0x01000000** (16777216, the plasma's trail and light) plus **0x00002000** (8192, cycle
  all frames at 10 Hz). Looping sound `weapons/rockfly.wav`. [R]
- **Muzzle flash 18** (ORed with 128 when silenced), sent with **ball one only**; the noise
  monsters hear is also made with ball one only. [R]
- **Touch**: the owner is ignored; the sky removes it; else an impact noise, then direct damage
  to what it hit if that takes damage (**knockback 0**, no flags, means of death **35**), then the
  radius damage to everything else in range (the one hit skipped; means of death **35**), then a
  temporary entity at the ball's position moved back by 0.02 s of its velocity (14.5 units) to
  the potentially visible set, and the ball is removed. [R] [D]
- **The explosion's temporary entity**: **28** (plasma explosion: type byte + position). In water
  the port sends **17** (rocket explosion in water) instead. **Original**: always 28 (the water
  choice was added in October 2022). [R]
- Deathmatch infinite ammo: no mag slug used. [R]

## 5. The trap (`ammo_trap`)

**The item**: pickup name **Trap** (index 13), world model `models/weapons/g_trap/tris.md2`
(rotating), view model `models/weapons/v_trap/tris.md2`, icon `a_trap`, pickup sound
`misc/am_pkup.wav`. It is ammo and weapon at once, like the Grenades: a pickup gives **1**, the
carry limit is **5** and nothing raises it. Precaches `weapons/trapcock.wav`,
`weapons/traploop.wav`, `weapons/trapsuck.wav`, `weapons/trapdown.wav`. It has no player-model
weapon index of its own (0). [R]

**In hand** (the hand grenade's own stepping, not the generic weapon's) [R]:

- Raising goes straight to ready at frame **16** (no raise frames); a new weapon chosen while
  ready is switched to at once (no lowering frames).
- Ready: idle frames **16–48**, looping back to 16 after 48; at **29, 34, 39 and 48** it holds
  with a chance of 15 in 16 each step.
- Attack with a trap: frames **1–15**, one a step. Frame **5** plays `weapons/trapcock.wav` on the
  weapon channel. At frame **11** the timer is lit: **3.2 s**, and the held sound
  `weapons/traploop.wav` loops. The frame holds at 11 while attack is held. Released, it steps on
  and the trap is thrown at frame **12** (the step after the release). Frame 15 waits until **1 s
  after the throw**, then frame 16 is ready.
- **Held to the end of the timer**: it is "thrown" with no time left, which makes the trap explode
  at once in the hand **as a grenade**: radius damage **125** (×4 quad) over **165**, means of
  death **24** (held grenade: "tried to put the pin back in" / "feels X's pain"); the explosion's
  temporary entity is the grenade rule's (in the air: 7, rocket explosion; in water 17). The
  trap is used up; the hand waits 1 s at frame 15. [R] [D]
- Attack with none: the "no ammo" click `weapons/noammo.wav` (at most once a second) and the
  no-ammo change. [R]
- Every throw uses one trap, **even with deathmatch infinite ammo** (the trap is never infinite).
  [R]
- **Dual fire does not speed it**; **quad** only multiplies the in-hand explosion; no quad sound
  plays for a trap. [R]
- **Edge** [D]: the base's weapon change rule fires a *hand grenade* when the hand still has a
  live timer, which is the case while the trap is held lit and for 1 s after any throw. Dying or
  switching weapon then throws a **hand grenade** (not a trap) with no time left: it explodes at
  once (125 over 165, means of death 15 or 16, the hand grenade's), and one trap is taken.

**The throw** [R]:

- Speed **400 + (3 − t) × 400 / 3**, where t is the time left on the 3.2 s timer (about 387 for an
  instant release, 800 after 3 s held). The library defines separate trap numbers (5 s, 300, 700)
  but never uses them; the grenade's 3 s, 400 and 800 are used.
- Start: offset 8 forward, 8 right, view height − 8 up. Velocity: aim × speed, plus up × (200 +
  random −10..10), plus right × random −10..10. Spin 300°/s in yaw.
- The base's bounce physics (overbounce 1.5; on a floor, below 60 up speed it stops).
- Box (−4, −4, 0) to (4, 4, 8), solid (blocks players and shots), shot mask. Model while thrown
  `models/weapons/z_trap/tris.md2`; looping sound `weapons/traploop.wav` for its whole life; class
  name **`htrap`**. It takes no damage.

**On the floor** [R]:

- **Life 30 s** from the throw (the clock restarts at 30 s when it catches something); when it
  runs out, wherever it is, it sends temporary entity **5** (explosion 1: a visual only) and is
  removed. **No damage.**
- It first thinks **1.0 s** after the throw, then every 0.1 s, and does nothing while not on the
  ground.
- **Opening**: frames 0 → 4, one per think on the ground. From the think after it reaches frame 4
  it carries effect bit **0x02000000** (33554432, the trap's glow) and its box shrinks to a point
  (it no longer blocks).
- **Pull** (every think on the ground, until it catches, including while opening): the
  candidates are every living monster or player within **256** of it and visible from it,
  **the thrower included**. Choice rule [D]: the first candidate found is taken as the choice, and
  each later candidate replaces it when nearer than the previous replacement's distance (the
  starting bound is 8000); so with two or more candidates the result is the nearest of all
  candidates after the first, and the first is kept only when it is alone.
- The chosen one is lifted 1 unit and taken off the ground. A **player** gets **+250 units/s
  toward the trap** added to its velocity each think (2500 units/s²). A **monster** is turned
  toward the trap at its own yaw speed and its velocity is **set to 256** along its facing.
  `weapons/trapsuck.wav` plays each think while pulling (voice channel, idle attenuation, 2).
- **Catch**: when the chosen one's origin is within **32** of the trap's: mass **under 400** →
  **100 000 damage** from the thrower (knockback 1, no flags, means of death **39**), which
  **invulnerability does not stop** (a special rule for 39) but god mode does; armour and power
  armour absorb as usual. So it dies and, past −40 health, gibs. Mass **400 or more** → the trap
  sends temporary entity 5 and is removed, with no damage. A player's mass is 200. [R] [D]
- **Feeding**: the trap goes to frame **5** and plays `weapons/trapdown.wav` once (idle
  attenuation). Each think: three gib models appear for one frame (0.1 s, not solid) in a ring
  about the trap turning by an angle that grows each think; ring radius 1 + w/2 and height w above
  the trap, where w starts at 62 and falls 2 each think. Model: `models/objects/gekkgib/torso/tris.md2`
  when the victim is a `monster_gekk`, else `models/objects/gibs/sm_meat/tris.md2` (a chest gib is
  chosen only when the trap's stored mass is above 200, which cannot happen: it is at most 399/4).
  [R] [D]
- When w reaches 18 (the 23rd think) frame 6, then 7, then 8 (**2.5 s after the catch**): the
  **food cube** is spawned and the trap is removed **1.0 s** later. [D]

**The food cube** [R] [D]:

- A **Health** item (pickup name "Health", icon `i_health`) with the model
  `models/objects/trapfx/tris.md2`, class name **`foodcube`**, glowing like any item.
- Gives health = **the victim's mass / 4 in deathmatch** (a player: **50**) or **mass / 10**
  otherwise (whole numbers), **past the maximum** (it ignores max health).
- Spawned 16 above the trap with 400 up speed; 0.2 s later it is dropped to the floor below like
  any item and keeps the 400 up speed (it hops up once). Starting inside a solid, it stays where it
  is with no vertical speed.
- Never respawns and never expires. In deathmatch with dmflags 1 (no health) it is not spawned.
- Pickup sound by amount: 2 → `items/s_health.wav`, 10 → `n_health`, 25 → `l_health`, anything
  else → `items/m_health.wav` (so a deathmatch cube of 50 plays `m_health`).

**Obituaries** (means of death 39): killed by another's trap, "<victim> caught in trap by
<thrower>"; by one's own, "<victim> sucked into his own trap." (no she/it variants). [R]

**The base hand grenade** is unchanged in the pack, except that the port added the quad's sound
to it in 2019. **Original**: no quad sound on the hand grenade. [R]

## 6. Monster-only bolts, and the player's hyperblaster

- The **player's HyperBlaster is unchanged**: the same frames, bolt, damage and ammo as the base.
  The only change is its `use` toggling to the Ionripper (question 7). [R]
- The monster-only projectiles in the library: the **blue blaster bolt** (model
  `models/objects/blaser/tris.md2`, loop `misc/lasfly.wav`, 2 s life, the base bolt's touch; the
  caller gives its effect, 0x00400000 = 4194304 for the blue trail), the **homing rocket** (model
  `models/objects/rocket/tris.md2`, effect 16, loop `weapons/rockfly.wav`; every 0.1 s it re-aims
  at 500 units/s toward the nearest living, visible player in front of it within 1024, never its
  owner), the ion ripper bolt above and a monster laser beam. Their numbers per monster belong to
  the monster notes. [R]

## 7. Weapon selection

**The cycle** (`weapnext` forward, `weapprev` backward, by item index, owned weapons only,
wrapping) [R]: Blaster (7), Shotgun (8), Super Shotgun (9), Machinegun (10), Chaingun (11),
Grenades (12), **Trap (13)**, Grenade Launcher (14), Rocket Launcher (15), HyperBlaster (16),
**Ionripper (17)**, Railgun (18), **Phalanx (19)**, BFG10K (20). Each candidate's `use` is tried,
and the scan stops at the first whose use makes it the coming weapon. [R] Because the HyperBlaster's
and Railgun's uses may pick the other weapon instead (below), a scan landing on them can leave the
other weapon chosen and keep scanning (the next accepted candidate then wins). [D]

**`use HyperBlaster`** [R]:

- HyperBlaster not in hand: chooses the HyperBlaster (or "No Cells for HyperBlaster." without
  cells and with `g_select_empty` 0).
- HyperBlaster in hand: chooses the **Ionripper** when owned (only the "no cells at all" check is
  made, so with 1 cell it is chosen and then cannot fire); else nothing happens.
- So with both owned, repeating `use hyperblaster` toggles between them. [D]

**`use Railgun`** [R]:

- No slugs: when there are mag slugs and the Phalanx is owned, the **Phalanx** is chosen; else
  "No Slugs for Railgun.".
- Slugs, Railgun not in hand: the Railgun.
- Slugs, Railgun in hand: the **Phalanx** when owned (without mag slugs: "No Mag Slug for
  Phalanx." and no change); else nothing.
- With the Phalanx in hand and no slugs, `use railgun` keeps the Phalanx (no toggle). [D]

**The `use` and `drop` commands** when the named item is not owned [R]: "hyperblaster" falls to
the Ionripper and "railgun" to the Phalanx when owned (and `use` then runs that weapon's own use,
so `use hyperblaster` without a HyperBlaster selects the Ionripper by its plain rule); otherwise
"Out of item: <the name as typed>". `use ionripper`, `use phalanx`, `use trap` follow the base
rule. `weaplast` is as the base.

**The automatic switch when out of ammo** [R]: Railgun (slugs) → **Ionripper (2 or more cells)**
→ HyperBlaster (cells) → Chaingun (bullets) → Machinegun (bullets) → Super Shotgun (2 or more
shells) → Shotgun (shells) → **Phalanx (a mag slug)** → Blaster. **Original**: the pack's chain
had a Phalanx test second and an Ionripper test third, but neither stopped the chain (and the
Ionripper test named an item that does not exist), so the last match or the Blaster always won:
the effective original order is Railgun, HyperBlaster, Chaingun, Machinegun, Super Shotgun,
Shotgun, Blaster, and **a pack weapon is never chosen automatically**. The Ionripper was put back
in November 2019, the Phalanx (last, above the Blaster) in August 2026. [R]

**Original `weapnext`/`weapprev`** [R] [D]: in id's pack release the two scans ran in the opposite
directions and tested the weapon in hand rather than the coming one for success, so neither
stopped early: each tried `use` on every owned weapon around the list (printing the "No ... for
..." line for each one without ammo), and the last accepted, the neighbour on the asked side, won.
The port took the base mission pack's version.

**Port-only commands**: `cycleweap`, `prefweap` (and the `g_quick_weap` preview) are the port's,
the same as its base library. [R]

**Picking up** a weapon or a trap switches as the base does (a newly owned weapon, or the first
trap, while holding the Blaster in deathmatch, always otherwise). [R]

## 8. Ammo

- **Mag Slug** (`ammo_magslug`, index 26): model `models/objects/ammo/tris.md2` (no rotation, no
  effect), icon `a_mslugs`, pickup sound `misc/am_pkup.wav`, gives **10**; respawn 30 s in
  deathmatch. Carry **50** at start, **75** after a Bandolier, **100** after an Ammo Pack. [R]
- The **Ammo Pack** also gives +10 mag slugs (held to the limit); the **Bandolier** gives none. [R]
- **Trap**: gives 1, carry 5; neither the Bandolier nor the Ammo Pack raises or gives traps. [R]
- With deathmatch infinite ammo (dmflags 8192), the base rule removes pure ammo items from the map
  at spawn: mag slugs are removed, traps (ammo and weapon) stay. [R] [D]
- `give ammo` / `give all` gives 1000 of each, held to the limits (so 5 traps). [R]

## 9. Base weapons the pack changes

None of their numbers: every base weapon's damage, frames, timing, spread and ammo are the same
as the base's. The pack's only changes to them are the Dual Fire double stepping (question 10),
the HyperBlaster and Railgun `use` toggles (question 7) and the no-ammo chain (question 7). [R]

## 10. Dual Fire (`item_quadfire`)

- Pickup name **DualFire Damage** (index 28), icon `p_quadfire`, world model
  `models/items/quadfire/tris.md2` (rotating), pickup sound `items/pkup.wav`; precaches
  `items/quadfire1.wav`, `items/quadfire2.wav`, `items/quadfire3.wav`. A powerup with instant use.
  [R]
- **Duration 30 s** (300 frames). Respawn **60 s** in deathmatch. The base powerup pickup limits
  apply (medium skill: carry at most 2; hard: 1). [R]
- **Use** (from the inventory: `use DualFire Damage`, or instantly, below): plays
  `items/quadfire1.wav` (item channel). Used while active, **its time adds** to what is left; otherwise it starts at 30 s. [R]
- **End warning**: at **3 s** left (30 frames) `items/quadfire2.wav` plays; the view blend and the
  shell flash in the last 3 s (shown when more than 30 frames remain, or when bit 4 of the frames
  left is set). `items/quadfire3.wav` is precached but **never played**. No sound while firing.
  [R]
- **What it does**: each step of the weapon in hand is **run twice per server frame** while it is
  active, so the view model's frames advance two at a time: firing, idling, raising and lowering
  all go twice as fast, and every period halves (railgun 1.6 → 0.8 s, ion ripper 0.3 s → one bolt
  per 1.5 frames, phalanx 1.6 → 0.8 s). It applies to the **Blaster, Shotgun, Super Shotgun,
  Machinegun, Chaingun, Grenade Launcher, Rocket Launcher, HyperBlaster, Railgun, BFG10K,
  Ionripper and Phalanx**; **not** to the hand grenade or the trap. Damage is unchanged. [R] [D]
- Edge [D]: when the first step of a frame completes a weapon change, the second step of that
  frame runs the old weapon's stepping numbers on the new weapon's frame 0.
- **With the quad**: independent; both may be active (×4 damage at double rate). [R]
- **HUD**: the timer slot shows icon `p_quadfire` and the seconds left (frames left / 10) when the
  quad is not active; the order of the timer slot is quad, **Dual Fire**, invulnerability,
  environment suit, rebreather. [R]
- **View blend** red 1, green 0.2, blue 0.5, alpha 0.08, only when the quad is not active (the
  quad's blue blend wins; no mixing); the order is quad, **Dual Fire**, invulnerability,
  environment suit, rebreather (the end-warning sound is in the same chain, so it is not heard
  while the quad runs). [R] [D]
- **Shell**: the player gets the **quad's effect bit 0x00008000** (32768, the blue shell), with the
  same last-3-s flashing: Dual Fire looks like the quad to others. [R]
- **Cleared** on death and at the intermission. [R]
- **Dropped on death** in deathmatch when **dmflags 65536** (Dual Fire drop; the pack's own flag,
  independent of the quad drop flag 16384) is set and more than 1 s (10 frames) remains. The
  dropped item lives for the time that was left and is touchable at once. Spread: with a weapon
  also dropped, the weapon goes 22.5° right and the powerups 22.5° left when the quad is dropped
  too, else the weapon 12.5° right and the Dual Fire 12.5° left; alone, straight ahead. [R]
- **Picking up a dropped Dual Fire** in deathmatch uses it at once with the time it had left (as
  the base's dropped quad). With dmflags 16 (instant items) every Dual Fire pickup is used at once; the activation sound is played after the pickup sound. [R] **Original**: (1) with dmflags 16 set,
  a dropped Dual Fire was used with a fresh 30 s instead of its remaining time; (2) the instant use
  happened before the pickup sound, so `items/pkup.wav` replaced the activation sound. Both were
  fixed in April 2020. [R]

## 11. The green key (`key_green_key`)

Pickup name **Green Key** (index 44), icon `k_green`, world model
`models/items/keys/green_key/tris.md2` (rotating), pickup sound `items/pkup.wav`; a key that stays
in coop. Nothing special: it behaves exactly as the base's blue and red keys. [R]

## 12. The item list's order

Index 0 is empty. Pack items in **bold**. [R]

| # | Class name | Pickup name |
| --- | --- | --- |
| 1 | `item_armor_body` | Body Armor |
| 2 | `item_armor_combat` | Combat Armor |
| 3 | `item_armor_jacket` | Jacket Armor |
| 4 | `item_armor_shard` | Armor Shard |
| 5 | `item_power_screen` | Power Screen |
| 6 | `item_power_shield` | Power Shield |
| 7 | `weapon_blaster` | Blaster |
| 8 | `weapon_shotgun` | Shotgun |
| 9 | `weapon_supershotgun` | Super Shotgun |
| 10 | `weapon_machinegun` | Machinegun |
| 11 | `weapon_chaingun` | Chaingun |
| 12 | `ammo_grenades` | Grenades |
| **13** | **`ammo_trap`** | **Trap** |
| 14 | `weapon_grenadelauncher` | Grenade Launcher |
| 15 | `weapon_rocketlauncher` | Rocket Launcher |
| 16 | `weapon_hyperblaster` | HyperBlaster |
| **17** | **`weapon_boomer`** | **Ionripper** |
| 18 | `weapon_railgun` | Railgun |
| **19** | **`weapon_phalanx`** | **Phalanx** |
| 20 | `weapon_bfg` | BFG10K |
| 21 | `ammo_shells` | Shells |
| 22 | `ammo_bullets` | Bullets |
| 23 | `ammo_cells` | Cells |
| 24 | `ammo_rockets` | Rockets |
| 25 | `ammo_slugs` | Slugs |
| **26** | **`ammo_magslug`** | **Mag Slug** |
| 27 | `item_quad` | Quad Damage |
| **28** | **`item_quadfire`** | **DualFire Damage** |
| 29 | `item_invulnerability` | Invulnerability |
| 30 | `item_silencer` | Silencer |
| 31 | `item_breather` | Rebreather |
| 32 | `item_enviro` | Environment Suit |
| 33 | `item_ancient_head` | Ancient Head |
| 34 | `item_adrenaline` | Adrenaline |
| 35 | `item_bandolier` | Bandolier |
| 36 | `item_pack` | Ammo Pack |
| 37 | `key_data_cd` | Data CD |
| 38 | `key_power_cube` | Power Cube |
| 39 | `key_pyramid` | Pyramid Key |
| 40 | `key_data_spinner` | Data Spinner |
| 41 | `key_pass` | Security Pass |
| 42 | `key_blue_key` | Blue Key |
| 43 | `key_red_key` | Red Key |
| **44** | **`key_green_key`** | **Green Key** |
| 45 | `key_commander_head` | Commander's Head |
| 46 | `key_airstrike_target` | Airstrike Marker |
| 47 | (none: the health items' shared entry) | Health |

The pack inserts six entries, so every base item after index 12 moves (the base's Grenade
Launcher at 13 becomes 14, and so on); the inventory and the configstrings follow these numbers.
[D]

## 13. Base items whose behaviour changes

- **Bandolier**: also raises the mag slug limit to 75 (no mag slugs given). **Ammo Pack**: also
  raises it to 100 and gives 10 mag slugs. [R]
- **Health**: the shared Health entry also serves the trap's food cube (question 5). [R]
- **Quad**: unchanged, except the death-drop spread when the Dual Fire is dropped too (question
  10). [R]
- Armour, power armour, the power cube, adrenaline, ancient head, silencer, rebreather,
  environment suit, invulnerability: unchanged, except that **invulnerability does not stop
  means of death 39** (the trap and `misc_nuke`). [R]

## 14. dmflags

- **65536**: Dual Fire drop: a dying player drops an active Dual Fire with more than 1 s left
  (question 10). It is the only bit the pack adds. [R]
- The library's shared header also names 131072, 262144, 524288 and 1048576 (another pack's
  flags) but never reads them. [R]
- Every base bit keeps its meaning; 16 (instant items) and 8192 (infinite ammo) touch the pack's
  items as described in questions 5, 8 and 10. [R]

## 15. Settings and commands

- The game library reports itself as **`xatrix`**. [R]
- The library registers the same settings as the base port's library, with two differences:
  `coop_pickup_weapons` defaults to **0** here (1 in the base port), and **`g_fix_triggered`**
  (default **0**, not archived) exists only here: a monster with spawn flag 2 (triggered) and no
  `targetname` always gets a console warning "triggered <class> at <position> has no targetname",
  and when the setting is 1 the triggered flag is cleared so it spawns normally. The base port's
  `g_commanderbody_nogod` is absent: the commander's body is always god-moded. [R]
- **Original**: the history shows these were added by the port and were not in id's pack library:
  `aimfix`, `coop_pickup_weapons`, `coop_elevator_delay`, `g_footsteps`, `g_monsterfootsteps`,
  `g_quick_weap`, `g_swap_speed`, `g_machinegun_norecoil`, `g_fix_triggered`, and the commands
  `teleport`, `listentities`, `cycleweap`, `prefweap`. [R]
- Client commands: the base port's list without `spawnentity` and `spawnonstart`. Server
  commands: as base (`test`, `addip`, `removeip`, `listip`, `writeip`). No pack-only setting or
  command. [R]

## 16. Damage rules

- **Means of death numbers after the base's 33**: **34** ion ripper, **35** phalanx, **36** brain
  tentacle, **37** blast-off (a fixbot's), **38** gekk, **39** trap. The friendly-fire bit is the
  base's. [R]
- **Invulnerability is ignored by means of death 39** (god mode is not). [R]
- A **`monster_gekk`** that takes damage sends temporary entity **26** (green blood) where the base
  sends 1 (blood). [R]
- Monsters may carry a "ignore pain" flag (0x00008000 in the monster flags) which skips their pain
  reaction; monster notes. [R]
- Power armour and armour: unchanged. No new damage flags. [R]

## 17. Lines a player reads

**Deathmatch obituaries the pack adds** (the base's form: "<victim> <message> <attacker><suffix>")
[R]:

| Means of death | Killed by another player | Killed by oneself |
| --- | --- | --- |
| 34 ion ripper | "<victim> ripped to shreds by <attacker>'s ripper gun" | the base's "killed himself/herself/itself." |
| 35 phalanx | "<victim> was evaporated by <attacker>" | the base's "killed himself/herself/itself." |
| 39 trap | "<victim> caught in trap by <attacker>" | "<victim> sucked into his own trap." (no gender variants) |

- **36 and 38** (brain tentacle, gekk): "<victim> that's gotta hurt." whatever the attacker (it
  is in the base's "no attacker needed" group), and the victim loses a point in deathmatch. [R]
- **37** (blast-off) and the nuke's 39 (the attacker is not a player): "<victim> died." [R] [D]
- Monsters are never named in obituaries (as in the base). [R]
- **Selection lines**: "No Cells for Ionripper.", "Not enough Cells for Ionripper.", "No Mag Slug
  for Phalanx.", "No Slugs for Railgun.", "No Cells for HyperBlaster.", "Out of item: <name>". [R]
- **Pickup** lines are the HUD's item names (question 12). No help computer text, centre print or
  other message is added by the library. [R]
- The port adds two coop-only lines on map `xsewer1`: "Overflow valve maintenance\nhatch A
  opened." and "Overflow valve maintenance\nhatch B opened." (a button's and a trigger's message,
  and a relay removed, so coop does not stall). Not in the original. [R]

## 18. The campaign

- **Open**: the library holds no list of units or maps, no start or end map, no movie or victory
  handling of its own: units, the next map and the cinematics are the maps' own data
  (`target_changelevel` and the worldspawn) and the engine's, handled by the base rules. Nothing
  in the library asks for `xin`, `xout` or `xu1`–`xu4`. [R]
- The map names the library itself knows (all port fixes): `xsewer1` (coop), `xsewer2` and
  `xcompnd2` (a mover's push height lifted 2 units for one numbered mover each), `xcompnd2` (one
  `trigger_relay` with target `t210` gets a 0.1 s delay so it fires before the player spawns),
  `xware` (an unnamed start removed in coop) and `xship` (coop spots up to 2500 away count as near
  a start, where other maps use 550); and entity override files for `industry` and `w_treat`. [R]

## 19. The help computer

Same as the base: the text is the maps' `target_help` messages; the library adds no goals text
and no layout change. [R]

## 20. Map things

**`func_object_repair`** [R] [D]:

- Point entity; class name changed to `object_repair` at spawn (the fixbot looks for that name); solid box of zero size; takes no damage; **health 100** (any map value overwritten); key
  **`delay`** default **1** s.
- First think 1.0 s after spawn, then every `delay`: it sends temporary entity **25** (welding
  sparks), count **10**, its origin, a zero direction, colour **224 + random 0..7**.
- **The fixbot** looks (every move, when not already busy with one and not in its 15 s back-off)
  for an `object_repair` within 1024 (when it scans) that is visible and has health **100 or more**, flies to it,
  and within 56 units welds: a 1 s start, then loops of 7 frames (0.7 s) throwing welding sparks
  (count 10, colour 224–231, from 24 forward, 0.8 left and 10 down of the bot), each loop taking
  **10 health** off the object, until the object's health is **below 0** (11 loops from 100).
- The object notices (at its next think) and 0.1 s later **fires its targets** (activator: itself),
  then 0.1 s later starts recovering: every `delay` it gains 1 health while at 100 or less and
  makes no sparks; above 100 it sparks again as before. It **never fires its targets again**,
  even if welded down a second time.

**`misc_amb4`**: plays `world/amb4.wav` on the voice channel at full volume with **no
attenuation** (heard everywhere), first 1 s after spawn, then **every 2 s**. **Original**: every
2.7 s (changed in January 2020). [R]

**`misc_nuke`**: no model, no sound, no flash from the library. When used (once; it then ignores
use): **every player** takes **100 000 damage** (knockback 1, no flags, means of death **39**,
the nuke as attacker: invulnerability does not stop it, god mode does), and **every monster
entity is removed outright** (no death, no gibs, no kill counted). [R]

**`misc_transport`** [R] [D]:

- Model `models/objects/ship/tris.md2`, box (−16, −16, 0) to (16, 16, 32), not solid, a pusher
  following `path_corner`s like a train. Without a `target` it is removed with a console line.
- `speed` default **300** (acceleration and deceleration equal to the speed).
- The library **always sets spawn flag 1**, which the train rule reads as "start on": it starts
  along its path 0.1 s after its path is found, **hidden** (not sent to clients). Its first use
  makes it visible wherever it has got to; later uses act as a train's (stop or resume only with
  the train toggle flag 2).
- No sounds (the library gives it none).

**`misc_viper_missile`** [R] [D]:

- Hidden point (not sent), box ±8, model index `models/objects/bomb/tris.md2` (never seen).
  `dmg` default **250**.
- When used: it finds the first entity whose `targetname` is its `target` and fires a **monster
  rocket** from its own origin straight at that entity's origin: speed **500**, direct damage
  `dmg`, radius damage `dmg` over radius `dmg` + 20 (250 over 270 by default), means of death 8
  and 9 (rocket, rocket splash), with monster muzzle flash **57** sent from the missile entity; then it is removed 0.1 s later. Its own angles are not used. A missing target entity is not
  checked for.

**`misc_crashviper`** (also added): as `misc_viper` with model `models/ships/bigviper/tris.md2`, `speed` default 300. [R]

**`rotating_light`** [R] [D]:

- Model `models/objects/light/tris.md2`, solid box of zero size, a stop mover.
- Spawn flags **1 START_OFF**, **2 ALARM**. When on it carries effect bit **0x00800000**
  (8388608, the spinning lights).
- `health` default **10** (a map value is kept); it always takes damage. Killed: temporary entity
  25 (count **30**, colour 224–231) at its origin, the lights off, use ignored, removed 0.1 s later.
- **Use toggles** on and off. Turned on with ALARM: `misc/alarm.wav` (voice, static attenuation 3)
  0.1 s later and then every 1 s until turned off. A light that **starts on** with ALARM makes no
  alarm sound until it has been used off and on again.
- `speed` default 32, read by nothing.

**`target_mal_laser`** (compared with `target_laser`) [R] [D]:

- Spawn flags **1 START_ON, 2 RED, 4 GREEN, 8 BLUE, 16 YELLOW, 32 ORANGE, 64 FAT** (the same
  colours as the base laser: the beam's colour skin values 0xf2f2f0f0, 0xd0d1d2d3, 0xf3f3f1f1,
  0xdcdddedf, 0xe0e1e2e3; FAT makes the beam width frame 16, else 4). No colour flag: skin 0.
- **It does not aim at a target**: its direction comes from its angles only (the `target` key is
  never looked up), and it starts at spawn rather than 1 s later.
- Keys: **`wait`** default **0.1**, **`delay`** default **0.1**, **`dmg`** default **5** (the base
  laser's default is 1).
- **Firing**: while on, the beam is visible all the time, but its trace, its damage and its end
  point are worked out only **once every `wait` + 0.1 s** (default 0.2 s), not every frame. Each
  trace deals `dmg` (knockback 1, energy, means of death 30, not to laser-immune things) to each
  player or monster along it and to the first other damageable thing, and sends the laser
  sparks temporary entity (count **8**, the beam's skin as colour) where it stops on a non-body
  every time (the base laser only when it is switched on or moves). The visible beam's end stays
  where the last trace ended.
- Switched on (by use or START_ON): the first trace comes `wait` + `delay` later (default 0.2 s).
  Use toggles; off hides it and stops the traces. Box ±8.

## 21. Base class names that change, and things a player can meet

- **`trigger_push`** spawn flag **2** (new): the push switches itself on and off, **`wait`** s
  each (default **10**): on for `wait` s, then off (no pushing) for `wait` s, and so on, on from
  spawn. While on, every 0.1 s it sends ten temporary entities **29** (tunnel
  sparks, count 1, zero direction, colour **116 + random 0..7**) from the centre of its box, each
  one higher than the last by `speed` × 0.01 × (its number + random 0..1). Flag 1 (once) and
  `speed` (default 1000, × 10 as velocity) as the base. [R]
- **`trigger_hurt`** toggling: turning it on only makes it touchable again; the base port also
  hurts everything already inside at once, this library does not. [R]
- **`monster_commander_body`**: always god-moded (no setting). [R]
- `target_laser`, `func_door`, `misc_viper`, `misc_strogg_ship`, `func_explosive`,
  `target_earthquake`, `func_train` and the other `trigger_*` classes: unchanged in behaviour
  (only formatting and safety checks differ). [R]
- **New spawnable classes**: `func_object_repair`, `misc_amb4`, `misc_crashviper`, `misc_nuke`,
  `misc_transport`, `misc_viper_missile`, `rotating_light`, `target_mal_laser`, and the monsters
  `monster_boss5`, `monster_chick_heat`, `monster_fixbot`, `monster_gekk`, `monster_gladb`,
  `monster_soldier_hypergun`, `monster_soldier_lasergun`, `monster_soldier_ripper` (monster
  notes); plus the items of question 12. [R]
- Entities a player meets that no map places: the thrown trap (`htrap`), the food cube
  (`foodcube`), the trap's one-frame gibs, the ion ripper bolt, the phalanx ball, the blue blaster
  bolt (`bolt`), and the repair object renamed `object_repair`. [R]

## 22. Temporary entities

The base library sends none of 25–29; this library sends [R] [D]:

| Number | Meaning | Bytes after the type | Sent by |
| --- | --- | --- | --- |
| 25 | welding sparks | count (1 byte), position, direction (1 byte index), colour (1 byte) | the ion ripper bolt's end (count 0, colour 228–231, zero direction); `func_object_repair` (count 10, colour 224–231, every `delay`); a destroyed `rotating_light` (count 30, 224–231); the fixbot's welder (count 10, 224–231) |
| 26 | green blood | position, direction | damage taken by a `monster_gekk` (the surface normal, potentially visible set) |
| 27 | blue hyperblaster | — | never sent by this library |
| 28 | plasma explosion | position | a phalanx ball's impact (0.02 s of its velocity back from it; in water the port sends 17 instead) |
| 29 | tunnel sparks | count (1 byte, always 1), position, direction (zero), colour (116–123) | a cycling `trigger_push` while on, ten every 0.1 s |

Also: the monsters' laser beam sends the base's laser sparks (count 10) where it stops on a
non-body; `target_mal_laser` sends laser sparks with count 8 at every trace. The trap's end and its
refusal of a heavy victim send 5 (explosion 1: position only). [R]

The trap's gibs carry a blood temporary entity's **number** inside their **effect bits** (on top of
effect 2): 1 for a red victim (which reads as effect 1, rotate) and 26 for a gekk (which reads as
effects 2, 8 and 16: gib trail, blaster light and trail, rocket light and trail). [R] [D]

## 23. Effect bits and render flags

| Bits | Value | On |
| --- | --- | --- |
| 0x00100000 | 1048576 | the ion ripper bolt (with render flag 8, full bright) |
| 0x00200000 | 2097152 | the gekk's acid gibs and head (green gib trail); the gibs add render flag 8, the head adds 8 to its *effects* (reading as effect 8, the blaster light) |
| 0x00400000 | 4194304 | the monsters' blue blaster bolt (given by the caller) |
| 0x00800000 | 8388608 | `rotating_light` while on |
| 0x01000000 | 16777216 | the phalanx ball (with 0x00002000, 8192: all frames at 10 Hz) |
| 0x02000000 | 33554432 | the trap once open on the floor |
| 0x00008000 | 32768 | a player with Dual Fire (the quad's blue shell) |

[R] The pack's library sets the player's render flags to 0 each frame (the base port sets 32768,
infrared-visible). [R]

## 24. Player weapon models

- In deathmatch and coop the library registers the player weapon models in this order:
  `#w_blaster.md2`, `#w_shotgun.md2`, `#w_sshotgun.md2`, `#w_machinegun.md2`,
  `#w_chaingun.md2`, `#a_grenades.md2`, `#w_glauncher.md2`, `#w_rlauncher.md2`,
  `#w_hyperblaster.md2`, `#w_railgun.md2`, `#w_bfg.md2`, **`#w_phalanx.md2`**, **`#w_ripper.md2`**.
  [R]
- The weapon index a player entity carries (in the high byte of its skin number) is the item's
  model number: Blaster 1 … BFG10K 11 as the base, **Phalanx 12**, **Ionripper 13**, so they meet
  `#w_phalanx.md2` and `#w_ripper.md2` in the list above (counting the client's default model as
  0). The **Trap has 0**: the default weapon model. [R] [D]

## 25. How the stock client draws 26 and 28 (a second reader, the base game's GPL client)

- **26, green blood** [R]: the bytes are blood's (position, direction index). Only specks: **30**
  (blood has 60), palette **223 + 0..7** (blood 232 + 0..7), each started −4..3 across and 0..7
  out along the direction (blood: 0..31), drifting −20..20 units/s on each axis, falling under the
  specks' gravity of 40 (blood 39.8), fading over 0.5 to 0.8 s. No sound, no light.
- **28, the plasma blast** [R]: position only, drawn exactly as the rocket's explosion (7): the
  explosion model `models/objects/r_explode/tris.md2` at a random yaw, 15 frames at 10 a second
  (either half of the model's 30 by a coin), a light of 350 fading with the model, colour (1, 0.5,
  0.5), 256 specks of 224 + 0..7, and `weapons/rocklx1a.wav` at normal attenuation. No number
  differs from 7.
