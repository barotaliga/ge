# Ground Zero: weapons, items and settings (facts)

WORKING-AREA facts page for Ground Zero's weapons, items and settings. It was written by a
clean-room reader from the mission pack's GPL game library: the Ground Zero game library as kept
by the Yamagi Quake II project (the `rogue` repository, HEAD of 2026-11-11), compared against
Quake II's own game library from the same project (HEAD of 2026-11-22) only to tell what the pack
changes. It holds facts only (numbers, orders of events, defaults, limits, edge cases, the names
a player or a mod sees), in the reader's own words. There is no code here. Each fact is marked
**[read]** (stated directly by the source), **[derived]** (worked out from several read facts)
or **[measured]** (none here: nothing was run). Where the source does not settle a question, the
answer says so.

Conventions used below: one server frame is 0.1 s; "units" are map units; "multiplier" is the
player's damage multiplier at the moment of firing (1, 2 with double damage, 4 with quad, 8 with
both in deathmatch; see question 13). A view-model frame run is written as four numbers: last
activate frame, last fire frame, last idle frame, last deactivate frame. The first fire frame is
always one after the last activate frame, the first idle frame one after the last fire frame, and
the first deactivate frame one after the last idle frame, as in the base game. A start offset is
"forward, right, up" in units; the right component is negated for a left-handed player and zeroed
for a centre-handed one, as in the base game.

## The item list

### 1. The whole item list, in client order

The list below is the order that numbers items for the client (slot 0 is empty; the configstring
for item N holds its pickup name). **[read]** Rows the pack inserts are marked **new**; every other
row is a base-game row in its base order. The base list has 39 named rows plus the generic
"Health" row; the pack's list has 60 named rows plus "Health" (61 slots after the empty slot 0).
**[read]**

Where the pack inserts rows **[read]**:

- ETF Rifle between Chaingun and Grenades.
- Prox Launcher between Grenade Launcher and Rocket Launcher.
- Plasma Beam between HyperBlaster and Railgun.
- Chainfist and Disruptor after BFG10K.
- Flechettes, Prox, Tesla, A-M Bomb and Rounds after Slugs.
- IR Goggles, Double Damage, compass, the three spheres, Doppleganger and Tag Token after Ammo Pack.
- Antimatter Pod and Antimatter Bomb (keys) after Airstrike Marker, before Health.

Columns: slot; classname; pickup name (exact case); world model; icon picture; view model; ammo
used; quantity (for ammo: amount given; for a weapon: ammo per shot; for a powerup: its
deathmatch respawn time in seconds); flags; deathmatch respawn. Every pickup sound not given in
the row is `items/pkup.wav`. All world models spin (rotate effect) unless the row says "no spin".
**[read]** (respawn column **[derived]** from the pickup rules, see the notes after the table).

| # | classname | pickup name | world model | icon | view model | ammo | qty | flags | DM respawn |
|---|---|---|---|---|---|---|---|---|---|
| 1 | item_armor_body | Body Armor | models/items/armor/body/tris.md2 | i_bodyarmor | | | | armour | 20 s |
| 2 | item_armor_combat | Combat Armor | models/items/armor/combat/tris.md2 | i_combatarmor | | | | armour | 20 s |
| 3 | item_armor_jacket | Jacket Armor | models/items/armor/jacket/tris.md2 | i_jacketarmor | | | | armour | 20 s |
| 4 | item_armor_shard | Armor Shard | models/items/armor/shard/tris.md2 | i_jacketarmor | | | | armour | 20 s |
| 5 | item_power_screen | Power Screen | models/items/armor/screen/tris.md2 | i_powerscreen | | | 60 | armour | 60 s |
| 6 | item_power_shield | Power Shield | models/items/armor/shield/tris.md2 | i_powershield | | | 60 | armour | 60 s |
| 7 | weapon_blaster | Blaster | (none) | w_blaster | models/weapons/v_blast/tris.md2 | | 0 | weapon, stay-coop | never in world |
| 8 | weapon_shotgun | Shotgun | models/weapons/g_shotg/tris.md2 | w_shotgun | models/weapons/v_shotg/tris.md2 | Shells | 1 | weapon, stay-coop | 30 s |
| 9 | weapon_supershotgun | Super Shotgun | models/weapons/g_shotg2/tris.md2 | w_sshotgun | models/weapons/v_shotg2/tris.md2 | Shells | 2 | weapon, stay-coop | 30 s |
| 10 | weapon_machinegun | Machinegun | models/weapons/g_machn/tris.md2 | w_machinegun | models/weapons/v_machn/tris.md2 | Bullets | 1 | weapon, stay-coop | 30 s |
| 11 | weapon_chaingun | Chaingun | models/weapons/g_chain/tris.md2 | w_chaingun | models/weapons/v_chain/tris.md2 | Bullets | 1 | weapon, stay-coop | 30 s |
| 12 **new** | weapon_etf_rifle | ETF Rifle | models/weapons/g_etf_rifle/tris.md2 | w_etf_rifle | models/weapons/v_etf_rifle/tris.md2 | Flechettes | 1 | weapon | 30 s |
| 13 | ammo_grenades | Grenades | models/items/ammo/grenades/medium/tris.md2 (no spin) | a_grenades | models/weapons/v_handgr/tris.md2 | grenades | 5 | ammo, weapon | 30 s |
| 14 | weapon_grenadelauncher | Grenade Launcher | models/weapons/g_launch/tris.md2 | w_glauncher | models/weapons/v_launch/tris.md2 | Grenades | 1 | weapon, stay-coop | 30 s |
| 15 **new** | weapon_proxlauncher | Prox Launcher | models/weapons/g_plaunch/tris.md2 | w_proxlaunch | models/weapons/v_plaunch/tris.md2 | Prox | 1 | weapon | 30 s |
| 16 | weapon_rocketlauncher | Rocket Launcher | models/weapons/g_rocket/tris.md2 | w_rlauncher | models/weapons/v_rocket/tris.md2 | Rockets | 1 | weapon, stay-coop | 30 s |
| 17 | weapon_hyperblaster | HyperBlaster | models/weapons/g_hyperb/tris.md2 | w_hyperblaster | models/weapons/v_hyperb/tris.md2 | Cells | 1 | weapon, stay-coop | 30 s |
| 18 **new** | weapon_plasmabeam | Plasma Beam | models/weapons/g_beamer/tris.md2 | w_heatbeam | models/weapons/v_beamer/tris.md2 | Cells | 2 | weapon | 30 s |
| 19 | weapon_railgun | Railgun | models/weapons/g_rail/tris.md2 | w_railgun | models/weapons/v_rail/tris.md2 | Slugs | 1 | weapon, stay-coop | 30 s |
| 20 | weapon_bfg | BFG10K | models/weapons/g_bfg/tris.md2 | w_bfg | models/weapons/v_bfg/tris.md2 | Cells | 50 | weapon, stay-coop | 30 s |
| 21 **new** | weapon_chainfist | Chainfist | models/weapons/g_chainf/tris.md2 | w_chainfist | models/weapons/v_chainf/tris.md2 | (none) | 0 | weapon, melee | 30 s |
| 22 **new** | weapon_disintegrator | Disruptor | models/weapons/g_dist/tris.md2 | w_disintegrator | models/weapons/v_dist/tris.md2 | Rounds | 1 | weapon | 30 s |
| 23 | ammo_shells | Shells | models/items/ammo/shells/medium/tris.md2 (no spin) | a_shells | | | 10 | ammo | 30 s |
| 24 | ammo_bullets | Bullets | models/items/ammo/bullets/medium/tris.md2 (no spin) | a_bullets | | | 50 | ammo | 30 s |
| 25 | ammo_cells | Cells | models/items/ammo/cells/medium/tris.md2 (no spin) | a_cells | | | 50 | ammo | 30 s |
| 26 | ammo_rockets | Rockets | models/items/ammo/rockets/medium/tris.md2 (no spin) | a_rockets | | | 5 | ammo | 30 s |
| 27 | ammo_slugs | Slugs | models/items/ammo/slugs/medium/tris.md2 (no spin) | a_slugs | | | 10 | ammo | 30 s |
| 28 **new** | ammo_flechettes | Flechettes | models/ammo/am_flechette/tris.md2 (no spin) | a_flechettes | | | 50 | ammo | 30 s |
| 29 **new** | ammo_prox | Prox | models/ammo/am_prox/tris.md2 (no spin) | a_prox | | | 5 | ammo | 30 s |
| 30 **new** | ammo_tesla | Tesla | models/ammo/am_tesl/tris.md2 (no spin) | a_tesla | models/weapons/v_tesla/tris.md2 | Tesla (itself) | 5 | ammo, weapon | 30 s |
| 31 **new** | ammo_nuke | A-M Bomb | models/weapons/g_nuke/tris.md2 | p_nuke | | A-M Bomb (itself) | 300 | powerup | 300 s |
| 32 **new** | ammo_disruptor | Rounds | models/ammo/am_disr/tris.md2 (no spin) | a_disruptor | | | 15 | ammo | 30 s |
| 33 | item_quad | Quad Damage | models/items/quaddama/tris.md2 | p_quad | | | 60 | powerup, instant-use | 60 s |
| 34 | item_invulnerability | Invulnerability | models/items/invulner/tris.md2 | p_invulnerability | | | 300 | powerup, instant-use | 300 s |
| 35 | item_silencer | Silencer | models/items/silencer/tris.md2 | p_silencer | | | 60 | powerup, instant-use | 60 s |
| 36 | item_breather | Rebreather | models/items/breather/tris.md2 | p_rebreather | | | 60 | powerup, stay-coop, instant-use | 60 s |
| 37 | item_enviro | Environment Suit | models/items/enviro/tris.md2 | p_envirosuit | | | 60 | powerup, stay-coop, instant-use | 60 s |
| 38 | item_ancient_head | Ancient Head | models/items/c_head/tris.md2 | i_fixme | | | 60 | (none) | 60 s |
| 39 | item_adrenaline | Adrenaline | models/items/adrenal/tris.md2 | p_adrenaline | | | 60 | (none) | 60 s |
| 40 | item_bandolier | Bandolier | models/items/band/tris.md2 | p_bandolier | | | 60 | (none) | 60 s |
| 41 | item_pack | Ammo Pack | models/items/pack/tris.md2 | i_pack | | | 180 | (none) | 180 s |
| 42 **new** | item_ir_goggles | IR Goggles | models/items/goggles/tris.md2 | p_ir | | | 60 | powerup, instant-use | 60 s |
| 43 **new** | item_double | Double Damage | models/items/ddamage/tris.md2 | p_double | | | 60 | powerup, instant-use | 60 s |
| 44 **new** | item_compass | compass | models/objects/fire/tris.md2 | p_compass | | | 60 | powerup | 60 s |
| 45 **new** | item_sphere_vengeance | vengeance sphere | models/items/vengnce/tris.md2 | p_vengeance | | | 60 | powerup, instant-use | 60 s |
| 46 **new** | item_sphere_hunter | hunter sphere | models/items/hunter/tris.md2 | p_hunter | | | 120 | powerup, instant-use | 120 s |
| 47 **new** | item_sphere_defender | defender sphere | models/items/defender/tris.md2 | p_defender | | | 60 | powerup, instant-use | 60 s |
| 48 **new** | item_doppleganger | Doppleganger | models/items/dopple/tris.md2 | p_doppleganger | | | 90 | powerup | 90 s |
| 49 **new** | dm_tag_token | Tag Token | models/items/tagtoken/tris.md2 (spins, plus the tag-trail effect) | i_tagtoken | | | 0 | powerup, not giveable, own spawn rule | see question 19 |
| 50 | key_data_cd | Data CD | models/items/keys/data_cd/tris.md2 | k_datacd | | | | key, stay-coop | none |
| 51 | key_power_cube | Power Cube | models/items/keys/power/tris.md2 | k_powercube | | | | key, stay-coop | none |
| 52 | key_pyramid | Pyramid Key | models/items/keys/pyramid/tris.md2 | k_pyramid | | | | key, stay-coop | none |
| 53 | key_data_spinner | Data Spinner | models/items/keys/spinner/tris.md2 | k_dataspin | | | | key, stay-coop | none |
| 54 | key_pass | Security Pass | models/items/keys/pass/tris.md2 | k_security | | | | key, stay-coop | none |
| 55 | key_blue_key | Blue Key | models/items/keys/key/tris.md2 | k_bluekey | | | | key, stay-coop | none |
| 56 | key_red_key | Red Key | models/items/keys/red_key/tris.md2 | k_redkey | | | | key, stay-coop | none |
| 57 | key_commander_head | Commander's Head | models/monsters/commandr/head/tris.md2 (gib trail, no spin) | k_comhead | | | | key, stay-coop | none |
| 58 | key_airstrike_target | Airstrike Marker | models/items/keys/target/tris.md2 | i_airstrike | | | | key, stay-coop | none |
| 59 **new** | key_nuke_container | Antimatter Pod | models/weapons/g_nuke/tris.md2 | i_contain | | | | key, stay-coop | none |
| 60 **new** | key_nuke | Antimatter Bomb | models/weapons/g_nuke/tris.md2 | i_nuke | | | | key, stay-coop | none |
| 61 | (no classname) | Health | (set per health entity) | i_health | | | | (none) | 30 s (mega health: 20 s after its extra health has worn off) |

Pickup sounds **[read]**: armour `misc/ar1_pkup.wav` (shard `misc/ar2_pkup.wav`), power armour
`misc/ar3_pkup.wav`, every weapon (new ones included) `misc/w_pkup.wav`, every ammo row (Flechettes,
Prox, Tesla, A-M Bomb and Rounds included) `misc/am_pkup.wav`, everything else `items/pkup.wav`; health
plays its size sound as in the base game.

Digit width beside the icon **[read]**: 3 for armour, ammo, the A-M Bomb and health; 2 for
powerups, the compass, the spheres and keys; 0 for weapons, power armour, the Doppleganger and the
Tag Token.

The player-model weapon number (the linked weapon model a third person sees, in the high byte of
the skin number) for the new weapons **[read]**: Disruptor 12, ETF Rifle 13, Plasma Beam 14, Prox
Launcher 15, Chainfist 16. Base weapons keep 1 to 11 (hand grenades 6). The Tesla has 0 (no linked
weapon model).

Sounds and models each row loads with it (precache), new rows **[read]**:

- ETF Rifle: `weapons/nail1.wav`, `models/proj/flechette/tris.md2`.
- Prox Launcher: `weapons/grenlf1a.wav`, `weapons/grenlr1b.wav`, `weapons/grenlb1b.wav`, `weapons/proxwarn.wav`, `weapons/proxopen.wav`.
- Plasma Beam: `models/weapons/v_beamer2/tris.md2`, `weapons/bfg__l1a.wav`.
- Chainfist: `weapons/sawidle.wav`, `weapons/sawhit.wav`.
- Disruptor: `models/items/spawngro/tris.md2`, `models/proj/disintegrator/tris.md2`, `weapons/disrupt.wav`, `weapons/disint2.wav`, `weapons/disrupthit.wav`.
- Prox (ammo): `models/weapons/g_prox/tris.md2`, `weapons/proxwarn.wav`.
- Tesla: `models/weapons/v_tesla2/tris.md2`, `weapons/teslaopen.wav`, `weapons/hgrenb1a.wav`, `weapons/hgrenb2a.wav`, `models/weapons/g_tesla/tris.md2`.
- A-M Bomb: `weapons/nukewarn2.wav`, `world/rumble.wav`.
- IR Goggles: `misc/ir_start.wav`. Double Damage: `misc/ddamage1.wav`, `misc/ddamage2.wav`, `misc/ddamage3.wav`.
- Spheres: vengeance `spheres/v_idle.wav`; hunter `spheres/h_idle.wav`, `spheres/h_active.wav`, `spheres/h_lurk.wav`; defender `models/proj/laser2/tris.md2`, `models/items/shell/tris.md2`, `spheres/d_idle.wav`.
- Doppleganger: `models/objects/dopplebase/tris.md2`, `models/items/spawngro2/tris.md2`, `models/items/hunter/tris.md2`, `models/items/vengnce/tris.md2`.

Notes on the rows:

- "instant-use" means the item is used the moment it is picked up when deathmatch flag 16 (instant
  items) is set, as in the base game; the pack gives that mark to IR Goggles, Double Damage and the
  three spheres, not to the compass, the Doppleganger or the A-M Bomb. **[read]**
- None of the new weapons carries the stay-in-coop mark. **[read]**
- Weapons respawn 30 s after pickup in deathmatch (or stay, with weapons-stay); ammo 30 s; armour
  20 s; power armour, powerups, the spheres, the Doppleganger and the A-M Bomb after their quantity
  in seconds; keys never respawn. Items dropped by players never respawn. **[read]**
- Picking up a weapon gives one ammo row's quantity of its ammo (ETF Rifle: 50 Flechettes; Prox
  Launcher: 5 Prox; Plasma Beam: 50 Cells; Disruptor: 15 Rounds; Chainfist: nothing), or 1000 with
  infinite ammo (deathmatch flag 8192). **[read]**
- Picking up ammo that is also a weapon (Grenades, Tesla) for the first time switches to it as in
  the base game, except the Tesla, which never causes an automatic switch. **[read]**
- The A-M Bomb, the Doppleganger, the hunter sphere and the vengeance sphere are removed at map
  load outside deathmatch; the defender sphere, IR Goggles, Double Damage and the compass appear in
  single player and coop too. **[read]**
- A player carries at most one A-M Bomb and at most one Doppleganger (a second pickup is refused); the
  Doppleganger cannot be picked up at all outside deathmatch. A sphere cannot be picked up while
  the player owns a launched sphere. Spheres and the other powerups obey the base skill limits on
  carried count (medium: refused at 2 carried; hard and above: refused at 1). **[read]**
- The compass's use prints the player's origin and view yaw (see question 22). It is a plain
  powerup with no drop. **[read]**
- Dropped items (from the drop command or a death) are drawn IR-visible as well as glowing, and
  every map entity spawned at load is marked IR-visible. **[read]**

### 2. Carry limits

**[read]** unless marked.

| ammo | start | with Bandolier | with Ammo Pack |
|---|---|---|---|
| Flechettes | 200 | 250 | 300 |
| Prox | 50 | 50 (unchanged) | 50 (unchanged) |
| Tesla | 50 | 50 (unchanged) | 50 (unchanged) |
| Rounds (Disruptor) | 100 | 150, only while `g_disruptor` is nonzero | 200, only while `g_disruptor` is nonzero |
| A-M Bomb | 1 carried (pickup refused when one is held); no ammo limit applies | same | same |

- The base limits do not change: Bullets 200/250/300, Shells 100/150/200, Rockets 50/50/100,
  Grenades 50/50/100, Cells 200/250/300, Slugs 50/75/100 (start / Bandolier / Ammo Pack).
- The Bandolier still adds only Bullets and Shells (one ammo row's quantity each) to the inventory.
- The Ammo Pack adds one ammo row's quantity of Bullets, Shells, Cells, Grenades, Rockets, Slugs,
  and now also Flechettes (50) and Rounds (15), each capped at its limit. It adds no Prox and no
  Tesla. It adds the 15 Rounds even when `g_disruptor` is zero (capped at 100). **[derived]**
- An ammo pickup is refused when that ammo is exactly at its limit. **[read]**

### 3. Classnames the library rewrites or removes

**[read]**

| classname in a map | what spawns |
|---|---|
| `weapon_nailgun` | renamed before spawning to `weapon_etf_rifle` (the ETF Rifle) |
| `ammo_nails` | renamed to `ammo_flechettes` |
| `weapon_heatbeam` | renamed to `weapon_plasmabeam` (the Plasma Beam) |
| `weapon_plasmabeam` | the Plasma Beam itself |
| `ammo_disruptor` | Rounds, only while `g_disruptor` is nonzero; otherwise removed at spawn in every mode |
| `weapon_disintegrator` | the Disruptor, only while `g_disruptor` is nonzero; otherwise removed at spawn in every mode |

The library also spawns five classnames of the other mission pack as Ground Zero items
**[read]**: `ammo_magslug` becomes Flechettes; `ammo_trap` becomes a Prox Launcher; `weapon_boomer`
becomes an ETF Rifle; `weapon_phalanx` becomes a Plasma Beam; `item_quadfire` becomes a sphere
chosen at random when it spawns (hunter 20 %, vengeance 40 %, defender 40 %).

### 4. Number keys, weapon order, best weapon on running out

- The game library holds no number-key slots: the number keys are console binds to `use <pickup
  name>` in the configs, which the library does not contain. Which key each new weapon shares is
  not settled by this source. **[read]**
- `use <pickup name>` finds the item by its pickup name (case-insensitive) and selects exactly that
  weapon; the library has no rule that a second press of a shared key moves to a sibling weapon.
  The messages "No <ammo> for <weapon>." and "Not enough <ammo> for <weapon>." are unchanged from
  the base game. **[read]**
- `weapnext` and `weapprev` walk the item list in slot order (question 1) and stop at the first
  owned weapon that can be selected. So the cycle forward is: Blaster, Shotgun, Super Shotgun,
  Machinegun, Chaingun, ETF Rifle, Grenades, Grenade Launcher, Prox Launcher, Rocket Launcher,
  HyperBlaster, Plasma Beam, Railgun, BFG10K, Chainfist, Disruptor, Tesla, then back to Blaster.
  Weapons without enough ammo are skipped (unless `g_select_empty` is set) because selection
  fails. **[derived]**
- The automatic choice when the current weapon runs dry changes **[read]**. In order, the first
  that applies is taken: Railgun (with any Slugs); Plasma Beam (with at least 2 Cells); ETF Rifle
  (with any Flechettes); Chaingun (Bullets); Machinegun (Bullets); Super Shotgun (more than 1 Shell); Shotgun
  (Shells); Blaster. The HyperBlaster is no longer in this list at all (in the base game it
  came second, after the Railgun). The Rocket Launcher, Grenade Launcher, Prox Launcher, BFG10K,
  Chainfist, Disruptor and the thrown weapons are never chosen, as before for the base ones.

## Each new weapon

### 5. Ammo, view-model frames, sounds, muzzle flash, start offset

Firing sounds of the ETF Rifle, Plasma Beam and Disruptor are not played by the game library: it
only sends the muzzle-flash number, and the client plays that flash's sound (the precache lists in
question 1 name the files). The library itself plays only the looping weapon sounds listed below.
**[read]** Every weapon below plays `items/damage3.wav` on each fire frame while quad is active,
or `misc/ddamage3.wav` while only double damage is active (see question 13). **[read]**

| weapon | ammo per shot | frames (activate, fire, idle, deactivate last) | fire frames | idle pause frames | muzzle flash | start offset (forward, right, up) |
|---|---|---|---|---|---|---|
| Chainfist | none (uses no ammo) | 4, 32, 57, 60 | 8, 9, 16, 17, 18, 30, 31 | none | none sent | 0, 8, eye height minus 4, from the origin |
| ETF Rifle | 1 Flechette per fire frame | 4, 7, 37, 41 | 6, 7 | 18, 28 | 30, never with the silencer bit | from the eye: 15, 8 (frame 6) or 6 (frame 7), minus 8 along the view's up axis |
| Prox Launcher | 1 Prox | 5, 16, 59, 64 (the Grenade Launcher's) | 6 | 34, 51, 59 | 8 (the grenade flash), with the silencer bit when silenced | 8, 8, eye height minus 8, from the origin |
| Plasma Beam | 2 Cells per fire frame | 8, 12, 39, 44 | 9, 10, 11, 12 | 35 | 33, with the silencer bit when silenced | 7, 2, eye height minus 3, from the origin |
| Disruptor | 1 Round | 4, 9, 29, 34 | 5 | 14, 19, 23 | 35, never with the silencer bit | 24, 8, eye height minus 8, from the origin |
| Tesla (thrown) | 1 Tesla | no activate or deactivate frames; fire run ends at 8, idle 9 to 32 | hold at frame 1, thrown at frame 2 | 21 | none | 0, minus 4 (to the left), eye height minus 22 along the view's up axis, from the origin |

**[read]**, with these details:

- **Chainfist** **[read]**: a press starts the run at frame 5. On reaching frame 6 the weapon picks
  one of three swings: swing A plays frames 6 to 13 (hits on 8 and 9), swing B jumps to 14 and plays
  to 23 (hits on 16, 17, 18), swing C jumps to 24 and plays to 32 (hits on 30, 31). The first swing
  is A with about 34 %, B 33 %, C 33 %. While the trigger is held at the end of a swing (frame 13,
  23 or 32) the run goes back to frame 6 and picks again, never repeating the swing just played:
  after A it is B two times in three and C one in three; after B it is A two in three and C one in
  three; after C it is A two in three and B one in three. Released at the end of A or B, the run
  jumps to frame 32 and goes idle. **[derived]** from those rules: swing A lasts 0.7 s with 2 hits,
  B 0.9 s with 3 hits, C 0.7 s with 2 hits (one hit per fire frame, one frame per 0.1 s).
  Looping sound: `weapons/sawhit.wav` while firing, `weapons/sawidle.wav` while raising and idle,
  none while lowering. Kick on each hit frame: the view origin moves back 2 units and the pitch kicks
  up 1 degree.
- **ETF Rifle** **[read]**: fire frames 6 and 7 each fire one flechette; while the trigger is held
  the run goes from frame 8 back to 6, so it fires one flechette every 0.1 s, alternating frame 6
  (offset right 8) and frame 7 (offset right 6). If the ammo runs out during the run the gun jumps
  to frame 8, plays `weapons/noammo.wav` (at most once a second) and switches weapon by the rule in
  question 4. Each shot restarts the player model's attack animation and kicks the view origin and
  angles by a random amount up to 0.85 on each axis. The start point is taken from the eye
  (origin plus view height), projected along the view's forward, right and up axes.
- **Prox Launcher** **[read]**: the Grenade Launcher's frames and kick (back 2 units, pitch up 1
  degree). Launch speed 600 (question 8).
- **Plasma Beam** **[read]**: the beam fires on each of frames 9 to 12; while the trigger is held
  and at least 2 Cells remain, the run goes from 13 back to 9, so it fires every 0.1 s. Releasing
  or dropping below 2 Cells sends the gun to frame 13 (idle). While firing, the view model is
  `models/weapons/v_beamer2/tris.md2` and the looping sound is `weapons/bfg__l1a.wav`; otherwise the
  view model is `models/weapons/v_beamer/tris.md2` and there is no loop. No view kick. Each shot
  restarts the player model's attack animation. While the Plasma Beam is the current weapon the gun
  does not sway with view movement (gun angles held at zero), so the drawn beam lines up.
- **Disruptor** **[read]**: kick back 2 units and pitch up 1 degree. It subtracts its Round even
  with infinite ammo (deathmatch flag 8192), unlike the other new weapons, which honour that flag.
  The bolt carries a looping sound, `weapons/disrupt.wav`.
- **Tesla** **[read]**: the throw follows the hand grenade's hold rule without the grenade's
  in-hand detonation: on frame 1 the hold timer starts (3.2 s); the Tesla stays on frame 1 while the
  trigger is held, and is thrown on frame 2 after release. Throw speed is 400 units/s plus 133.3 units/s for every
  second between the hold frame and the throw beyond the first 0.2 s (less below it), capped at
  800; **[derived]** a tap (throw one frame after the hold frame) gives 386. The view model is
  `models/weapons/v_tesla2/tris.md2` on frames 2 to 8 and `models/weapons/v_tesla/tris.md2`
  otherwise. No throw sound is played (the grenade's frame-5 sound is never reached). Switching
  away is instant (no lowering frames).

### 6. Chainfist

- Reach 64 units, traced as a line from the start point (question 5) along the aim. **[read]**
- Damage per hit: 15 in single player and coop, 30 in deathmatch, times the multiplier. **[read]**
- Knockback: none. A nominal kick of 100 is passed but the hit is flagged to give no knockback.
  **[read]**
- Armour: the hit is the "destroy armour" kind: armour (and power armour) still absorb and lose
  their share as usual, but the target takes the full damage to health anyway; god mode and
  invulnerability still protect. **[read]**
- On a hit on anything that can take damage, the wielder is pulled: 75 units/s added along the
  view's forward axis and 75 units/s along its up axis. **[read]**
- Rate while held: one hit per fire frame (question 5), so 2 or 3 hits per swing. **[derived]**
- Sounds: only the loops of question 5. The melee hit, miss and wall sounds are switched off for
  the Chainfist. **[read]**
- Effects **[read]**: hitting a wall or anything that cannot take damage sends temporary entity 0
  (gunshot puff) at the impact point with the surface normal, to the impact's PVS. Hitting a player
  or monster sends temporary entity 42 (more blood) instead of the usual blood (1); hitting a
  "mechanical" thing (prox mines and Teslas) sends 46 (electric sparks).
- Smoke: on idle frames 42 and 51, with probability 7/8 times 0.4 (0.35 in all) on each pass, the
  wielder alone (unicast, unreliable) gets temporary entity 45 (chainfist smoke, a position only)
  at forward 8, right 8, eye height minus 4. A centre-handed player never gets the smoke.
  **[read]**/**[derived]** (the 0.35)

### 7. ETF Rifle

- Flechette speed 750 units/s, straight along the aim (no spread). **[read]**
- Damage 10 per flechette, knockback 3, both times the multiplier. **[read]**
- Hand: frame 6 fires from right 8, frame 7 from right 6, so both barrels sit on the weapon's
  side; a left-handed player gets the mirror, a centre-handed one 0 for both. **[read]**
- The flechette: model `models/proj/flechette/tris.md2`, drawn full-bright, zero-size box, flies
  without gravity, removed after 8000/750 s (about 10.7 s). **[read]**/**[derived]**
- On a sky surface: removed with no effect. On a wall or anything that cannot take damage: removed
  and temporary entity 55 (flechette impact) is sent with the position and a direction (the surface
  normal) to the PVS. On something that can take damage: damage only; the blood (1) or electric
  sparks (46) come from the general damage path. **[read]**
- Armour **[read]**: flechette damage skips regular armour entirely. Power armour still works but
  at half strength: it can save at most half of what its cells would normally cover, and every point
  it saves costs twice the usual cells (power screen: 2 cells a point; power shield: 1 cell a
  point, against the normal 1 cell per 1 or 2 points).

### 8. Prox launcher and the prox mine

- Flight **[read]**: launched at 600 units/s along the aim plus 200 units/s (±10) along the view's
  up axis and ±10 along its right axis; normal gravity; bounces; box ±6 units; model
  `models/weapons/g_prox/tris.md2` with the grenade trail effect; IR-visible. It cannot be damaged
  while flying. Unlanded, it explodes on its own after 45 s (multiplier 1), 30 s (2), 15 s (4) or
  10 s (8).
- Landing **[read]**: on a sky surface it is removed. If the point 10 units behind the surface it
  hit is in slime or lava it explodes. Touching a monster, a player or another damageable thing
  (another mine, a Tesla, an A-M Bomb, a Doppleganger) other than its thrower explodes it at once; touching
  the thrower does nothing (it bounces on). On the world it sticks to any surface (floor,
  wall or ceiling) and stops dead. On a moving brush entity (door, platform) it sticks only to an
  upward-facing surface (normal steeper than 0.7 up) and then rides the mover **[derived]: it keeps
  bounce physics with zero speed, so it rests on and moves with the mover**. On any other
  non-world entity: an upward-facing surface explodes it; any other surface lets it bounce on; a
  bounce that would leave it moving up faster than 60 units/s is ignored. **[read]**
- Once stuck **[read]**: it turns to face out of the surface, becomes shootable (health 20), gains
  a trigger field 192 units across (±96 on each axis) centred on it, and starts opening.
- Arming **[read]**: the opening animation runs frames 0 to 9, one per think (nominally 0.05 s,
  but thinks run at most once per 0.1 s frame), with `weapons/proxopen.wav` at frame 0.
  **[derived]** It is armed about 1.0 s after landing. While opening it still belongs to the thrower
  (so the thrower does not collide with it); once armed it belongs to no one and its thrower can
  bump into it and shoot it.
- At the moment it arms **[read]**: it looks within 202 units for any living monster or player
  (the thrower included) and, in deathmatch only, any player start, deathmatch start, coop start or
  teleporter destination, that it can see. If it finds one it plays `weapons/proxwarn.wav` and
  explodes at once (so a mine placed within sight of a spawn point or teleporter exit in deathmatch
  never stays).
- Trigger **[read]**: afterwards, any monster or player (the thrower included) entering the
  ±96-unit field makes it play `weapons/proxwarn.wav` and explode 0.5 s later. Only monsters and
  players set it off; items and projectiles do not.
- Life after arming **[read]**: 45 s at multiplier 1, 30 s at 2, 15 s at 4, 10 s at 8; with
  `strong_mines` nonzero, always 45 s. At the end it explodes. While waiting it animates frames 9
  to 13 every 0.1 s.
- Explosion **[read]**: radius damage 90 times the multiplier at the mine (360 with quad, 180 with
  double, 720 with both), falling off by half a point per unit of distance, within 192 units; the
  thrower takes half, as for any radius damage. Sends temporary entity 8 (grenade explosion) when the
  mine is resting on the ground, else 7 (rocket explosion) (so a wall- or ceiling-stuck mine shows
  the rocket explosion) **[derived for the stuck cases]**. Plays `items/damage3.wav` if its damage
  is at least 4 times normal, `misc/ddamage3.wav` if exactly twice. Kills read as the prox mine
  (question 21).
- Shot down **[read]**: a mine killed by another mine's blast explodes 0.1 s later (so chains
  ripple); killed by anything else, it explodes at once.
- Per player: the library sets no limit on how many mines a player may have out. **[read]**
- Teleporters: only players are teleported, as in the base game; a mine touching a teleporter
  trigger is unaffected. **[read]**
- Quad and double: they multiply the damage at the moment of firing and shorten the life (above); `strong_mines`
  restores the 45 s life after arming only (the unlanded timer still shortens).
  **[read]**/**[derived]**

### 9. Plasma beam (heat beam)

- Damage per fire frame: 15 in deathmatch and 15 in single player, times the multiplier.
  Knockback per frame: 75 in deathmatch, 30 in single player, times the multiplier. Energy damage
  (energy armour protection applies). **[read]**
- Cells: 2 per fire frame, so 20 Cells/s and 150 damage/s held (multiplier 1). **[derived]**
- Range: 8192 units, an instant line from the start point (question 5) along the aim. **[read]**
- Water **[read]**: if the line enters water (starting out of water), temporary entity 43 (heat
  beam sparks: a position and a direction) is sent at the entry point with the water surface normal,
  the line continues through the water, and the damage is halved. If the shooter starts under
  water, the line ignores water, there is no halving and no entry sparks. In either water case a
  bubble trail (temporary entity 41: two positions) is sent from the water entry point (or the
  start) to the end point.
- On a wall or other surface that cannot take damage (not sky, and only when no water was crossed)
  temporary entity 44 (heat beam steam: a position and a direction) is sent at the impact with the
  surface normal, to the impact's PVS. Hitting a sky surface does nothing. **[read]**
- The beam itself **[read]**: every fire frame sends temporary entity 38 (heat beam) to every
  client (not limited to a PVS), carrying the shooter's entity number, the start point (the traced
  start of question 5) and the end point (where the line stopped; through water, the end beyond the
  water). The library computes a second, "entity" offset (2, 7, minus 3) but sends nothing from it.
  The library gives the beam no spin; any spin or offset on screen is the client's.
- Monsters that fire the same beam send temporary entity 39 instead, with the same fields. **[read]**

### 10. Disruptor

- Bolt **[read]**: speed 1000 units/s; model `models/proj/disintegrator/tris.md2` with the tracker
  effect and the loop `weapons/disrupt.wav`; zero-size box; no gravity.
- Damage **[read]**: 30 in deathmatch, 45 in single player, times the multiplier.
- Tracking **[read]**: at the moment of firing, a line is traced 8192 units along the aim; if it
  hits a living monster, player or damageable thing, that is the target. If the line hits the world
  instead, a second trace with a 32-unit cube (±16) is tried and its living monster, player or
  damageable thing becomes the target. (If the first line hits some other non-world entity that is
  not a valid target, no second trace is made.) With a target, every 0.1 s the bolt turns to point
  straight at it (no turn-rate limit) at 1000 units/s: at a player's eye (origin plus view height),
  at the centre of a monster's box, or at its origin when it has no box. If the target dies or goes
  away the bolt bursts at once where it is. With no target it flies straight and vanishes after
  10 s. A bolt that has a target has no time limit.
- On impact with a living monster or player **[read]**: 0 damage on contact with knockback 3
  times the damage (90 in deathmatch, 135 in single player, times the multiplier), plus 140 units/s
  upward for anything that is not a flier or swimmer. Then a "pain" helper damages the target every
  0.1 s, starting 0.1 s after impact, while no more than 0.5 s have passed: **[derived]** 5 ticks (4 if the
  time difference rounds just above 0.5 s) of one fifth of the damage each (6 a tick in deathmatch, 9 in single player, at multiplier 1), for the
  full damage over 0.5 s. These ticks skip power armour, count as energy damage and give no
  knockback. While being hurt, a monster shows the tracker-trail effect and a player shows it through
  the same effect bit on the player's entity.
- On a dead body (health 0 or less) it does 4 times the damage at once (120 in deathmatch, 180 in
  single player) with knockback 3 times the damage. On anything else that can take damage (not a
  monster or player) it does the full damage at once. All Disruptor damage skips power armour.
  **[read]**
- "Disintegrate" **[read]**: a player killed by the Disruptor is always gibbed (set to minus 100
  health and gibbed with 400 damage), so no corpse is left, only gibs and a head (in deathmatch, 8
  small meat gibs, since below minus 80). A monster killed by the ticking damage takes a further hit
  equal to its gib threshold (500 if it has none), which gibs it. The pain helpers on a player are
  removed when the player dies, respawns by the kill command, or disconnects.
- Burst **[read]**: on any impact (and when the target is lost) temporary entity 47 (tracker
  explosion) is sent with one field, the position, to the PVS; the bolt is removed. A sky surface
  removes the bolt with no burst.
- The library plays no impact sound itself (`weapons/disrupthit.wav` is only loaded). **[read]**
- Obituary: "annihilated", question 21. **[read]**

## Throwables and powerups

### 11. Tesla mine

- Throw **[read]**: speed from the hold rule of question 5 (386 to 800 units/s) along the aim, plus
  200 units/s (±10) along the view's up axis and ±10 along its right axis; normal gravity; bounces; box
  ±12 units across, 0 to 20 units high; model `models/weapons/g_tesla/tris.md2` with the
  grenade trail effect; IR-visible; "mechanical" (shows electric sparks, 46, when hit).
- It can be shot from the moment it is thrown: health 20 in deathmatch, 30 otherwise. **[read]**
- Each bounce plays `weapons/hgrenb1a.wav` or `weapons/hgrenb2a.wav` (even chance); if the point
  20 units behind the surface it touched is slime or lava, it blows up big (below). **[read]**
- Arming **[read]**: 3 s after the throw it checks for slime or lava (removed, small explosion) and
  starts opening: frames 0 to 14, one every 0.1 s, with `weapons/teslaopen.wav` at frame 0; its skin
  becomes 1 at frame 10 (where it also makes a weapon noise that alerts monsters to the thrower),
  2 at frame 12, 3 at frame 14. It activates 0.1 s after the last frame. **[derived]** It is active
  about 4.5 s after the throw.
- On activation **[read]**: in water, slime or lava it blows up big. In deathmatch, if a player
  start, deathmatch start, coop start or teleporter destination lies within 192 units and in sight,
  it is removed (small explosion). Otherwise it sets up its zap field: ±128 units on both horizontal
  axes, from its base up to 128 units above its origin. In deathmatch it stops belonging to its
  thrower (the thrower can then bump into it).
- Zapping **[read]**: every 0.1 s, every entity whose box overlaps the field is considered: it must
  be alive (health 1 or more) and be a monster, a damageable thing (prox mines, other Teslas, A-M
  Bombs, Dopplegangers), or a player, players only in deathmatch (never in single player or coop).
  A line is traced from 16 units above the Tesla's origin to the target's origin; if nothing blocks
  it (or it reaches the target), the target takes 3 damage times the multiplier, knockback 8 (none
  for monsters that walk; fliers and swimmers are pushed), ordinary damage (armour applies),
  credited to the thrower. Every qualifying target is zapped each frame; there is no cap on the
  number of targets. **[derived]** In deathmatch the thrower is zapped like anyone else, which reads
  as a suicide.
- The zap's picture **[read]**: temporary entity 33 (lightning) per target per frame, to the PVS of
  the start point, with four fields in this order: the target's entity number (16 bits), the Tesla's
  entity number (16 bits), the end position (where the traced line stopped), the start position (16
  units above the Tesla's origin).
- Sounds while zapping **[read]**: `items/damage3.wav` per zap if its damage is at least 4 times
  normal, `misc/ddamage3.wav` if exactly twice. The library plays no zap sound of its own.
- Life **[read]**: 30 s after activation it is removed (small explosion).
- Explosions **[read]**/**[derived]**: the "small" explosion (removed, destroyed by damage, expired,
  refused near a spawn point) is the grenade explosion with 3 times the multiplier as damage and a
  radius of 0, so it hurts nothing **[derived]**. The "big" one (water, slime or lava) is the grenade
  explosion with 150 times the multiplier as damage within 200 units, with the quad or double sound
  as above; it reads as grenade shrapnel in the obituaries (question 21). The effect is temporary
  entity 8 when resting on the ground, else 7 (18 and 17 when under water), to the PHS.
- Monsters **[read]**: a monster damaged by a Tesla turns to attack the Tesla and marks the field as
  an area to avoid; a medic stops healing to do so.
- Deploying limit: none in the library. **[read]**

### 12. A-M Bomb (nuke)

- Throw **[read]**: using the A-M Bomb (from the inventory) throws it from the player's origin (not
  the eye) at 100 units/s along the view direction plus 200 units/s (±10) along the view's up axis
  and ±10 along its right axis; normal gravity; bounces with `weapons/hgrenb1a.wav` or
  `weapons/hgrenb2a.wav` (even chance) on every touch; box ±8 units across, 0 to 16 high; model
  `models/weapons/g_nuke/tris.md2` with the grenade trail effect; IR-visible. Its multiplier is the
  thrower's at the moment of throwing.
- Countdown **[read]**: 10 s from the throw to the blast.
- The first 4 s: `weapons/nukewarn2.wav` once a second.
- The last 6 s: it animates frames 6 to 11 in a loop, one every 0.1 s; every 0.1 s it sends a
  muzzle flash from itself, 36 at multiplier 1, 37 at 2, 38 at 4, 39 at 8; it beeps
  `weapons/nukewarn2.wav` every 0.5 s while more than 3 s remain and every 0.3 s in the last 3 s; it
  stops belonging to its thrower; and it explodes at once if it is in slime or lava.
- The beeps are heard without the usual hearing-area limit and with an attenuation of 1.8/1.4
  (about 1.29) at multiplier 1, 0.9 at 2, 0.6 at 4, 0.36 at 8 (a lower value carries farther).
- Destroying it **[read]**: for the first 4 s it has 10000 health (practically indestructible); for
  the last 6 s it has 1 health, so any damage sets it off at once, credited to its thrower. A bomb
  killed by another bomb's blast is removed without exploding.
- Blast **[read]**: damage 400 times the multiplier. The kill radius is 512 units at multiplier 1,
  and 512 plus a quarter of 512 per multiplier step above that: 768 with double, 1024 with quad, 1536
  with both. Distance is measured from the bomb to the centre of the target's box. Within the kill
  radius the damage is 10000; between the kill radius and twice it, the damage falls linearly from
  the full damage at the kill radius to 0 at twice the radius; beyond, nothing. Knockback equals the
  damage. Only players, monsters and damageable things are hurt. The thrower is not exempt.
  Teammates are hurt even with deathmatch flag 256 (no friendly fire).
- Bodies **[read]**: a player inside the kill radius is marked so as not to gib: they die with no
  gibs and no gib sound, leaving only a head. A player killed farther out with health below minus 80
  is also spared the gibs; between minus 40 and minus 80 they gib normally.
- Effects and sound **[read]**: `items/damage3.wav` (at least 4 times) or `misc/ddamage3.wav`
  (exactly twice); `weapons/grenlx1a.wav` at full volume with no attenuation (heard everywhere); temporary
  entity 53 (big explosion, one position field) to the PVS; temporary entity 51 (nuke
  blast, one position field) to every client.
- Screen flash **[read]**: each player hurt by the blast gets a white screen blend for 2 s whose
  strength starts at 1 and falls linearly to 0. Then the library walks the players from slot 1 up:
  a player in direct sight of the bomb gets 2 s, one out of sight within 2048 units 1.5 s, one
  farther 1 s (never shortening a longer flash). **[derived]** The walk stops at the first slot that
  is not a connected player or that already has a 2 s flash from this blast, so players after such a
  slot get no flash unless they were hurt.
- Quake **[read]**: for 3 s after the blast, every 0.1 s, every player standing on the ground is
  thrown: ground contact cleared, a random amount between minus 150 and 150 units/s added to each
  horizontal velocity axis, vertical speed set to 100 times 100 divided by the player's mass (50
  units/s for the standard mass of 200). `world/rumble.wav` plays every 0.5 s at volume 0.75 with no
  attenuation. Monsters are not thrown. There is no view-angle shake.
- dmflags 524288 removes `ammo_nuke` at map load in deathmatch and keeps random respawn from turning
  another item into one (question 19). **[read]**

### 13. Double damage

- Duration 30 s per use (300 frames), added to any time left. Use sound `misc/ddamage1.wav`.
  **[read]**
- Multiplier **[read]**: quad gives 4. Double gives 2 more on top, but only in deathmatch or when
  quad is not active: so in deathmatch quad plus double is 8, and in single player or coop it is 4.
  With dmflags 262144 (no stacking) quad alone counts while both are active (4). The multiplier
  applies to every weapon's damage (and to the knockback where the weapon multiplied knockback under
  quad in the base game), to the prox mine, the Tesla and the A-M Bomb at the moment they are thrown.
- Firing sound **[read]**: on each fire frame `items/damage3.wav` while quad is active, else
  `misc/ddamage3.wav` while double is active (also on a hand-grenade throw: `items/damage3.wav` when
  the multiplier is 4 or more, `misc/ddamage3.wav` when it is exactly 2).
- Shell **[read]**: the player entity carries effect bit 0x08000000 (the double-damage shell),
  blinking off on frames whose remaining-frame count has bit 4 clear, during the last 3 s.
- Screen **[read]**: a gold blend (0.9, 0.7, 0, alpha 0.08) with the same blink, shown only while
  quad is not active. At exactly 3 s left (and only while quad is not active) `misc/ddamage2.wav`
  plays as the end warning.
- HUD **[read]**: timer icon `p_double` with the whole seconds left, shown when quad is not active
  (priority: quad, double, invulnerability, environment suit, rebreather, owned sphere, IR goggles).
- Death clears it. **[read]**

### 14. IR goggles

- Duration 60 s per use (600 frames), added to any time left. Use sound `misc/ir_start.wav`.
  **[read]**
- While active **[read]**: the player state's refresh flags carry 4 (IR goggles) and the screen has a
  red blend (1, 0, 0, alpha 0.2); in the last 3 s both blink off on frames whose remaining count has
  bit 4 clear. The client does the IR drawing.
- What shows in IR **[read]**: every player always carries render flag 0x8000 (IR-visible), alive or
  dead; every map entity spawned at load and every randomly respawned item is IR-visible; so are
  dropped items, prox mines, Teslas, A-M Bombs, spheres and the Doppleganger's base.
- HUD **[read]**: icon `p_ir` with seconds left, lowest in the timer priority (question 13).
- It is not cleared at death; it is cleared on respawn and at intermission. **[read]**/**[derived]**

### 15. The Doppleganger (decoy)

- Deathmatch only (removed at map load otherwise; pickup refused outside deathmatch). Carry at most
  one. **[read]**
- Use **[read]**: a point 48 units ahead of the player (yaw only) is searched for a free spot within
  32 units that has ground below within 64 units; if none is found nothing happens and the item is
  kept (no message). Otherwise the item is used, a spawn-growth effect appears at the spot, and the
  decoy is placed there.
- What it is **[read]**: an invisible base (box minus 16 to 16 across, minus 24 to 32 high, tossed
  by gravity, IR-visible, damageable, health 30) and on it a figure that copies the player's own
  entity look at the moment of use (model, skin, weapon model) 8 units up, playing the standing
  animation frames in a loop at 10 frames a second and now and then (10 % a frame when it has
  finished turning, at most once a second) turning at 30 degrees a frame to a random yaw between 0
  and 350.
- Life **[read]**: 30 s; then base and figure vanish in an explosion effect (temporary entity 5),
  with no damage.
- When it dies **[read]**: the base and figure vanish in the same harmless explosion effect. If
  something other than its owner hurt it before the killing blow, it releases a sphere aimed at the
  killer: a hunter sphere when that earlier attacker is more than 768 units away, else a vengeance
  sphere. **[derived]** A decoy killed by a single blow releases no sphere, because the attacker is
  only recorded by hits that do not kill. These spheres
  hunt at once, live 15 s, and their kills read as the doppleganger's (question 21). They never hurt
  the owner.
- Monsters: the library gives monsters no rule about the decoy (it is a deathmatch item). Not
  settled further by this source. **[read]**
- Limit per player: one carried; nothing limits how many are out. **[read]**

### 16. Spheres: defender, hunter, vengeance

Common **[read]**:

- Using one launches it from the top of the player's box; it lives 30 s from launch. A player may
  own one launched sphere at a time: using another prints "Only one sphere at a time!" and keeps the
  item; a sphere cannot be picked up while one is owned.
- While idle it follows its owner: it aims for a point 4 units above the top of the owner's box,
  with a velocity of 5 times the remaining offset per second; once a second (on whole seconds), if
  it cannot see its owner, it jumps straight to that point.
- Drawn full-bright and IR-visible. HUD: timer icon `p_defender`, `p_hunter` or `p_vengeance` with
  the whole seconds of life left (below the rebreather in priority).
- All spheres explode harmlessly (temporary entity 5) at intermission and when their time runs out; disconnecting
  or the kill command removes the owner's sphere.
- Hunter and vengeance spheres exist only in deathmatch; the defender also appears in single player
  and coop.

Defender **[read]**:

- Model `models/items/defender/tris.md2` with a second model `models/items/shell/tris.md2`; loop
  `spheres/d_idle.wav`.
- The owner takes half damage (at least 1) while owning it, shown by the half-damage effect bit
  0x40000000 on the owner. This halving comes before invulnerability, armour and power armour.
- Its target is whoever last hurt the owner (not the owner). Every 0.4 s at most, if it can see the
  target, it fires a green bolt from 2 units above itself: speed 1000 units/s, model
  `models/proj/laser2/tris.md2` with the blaster and tracker effects, removed after 2 s. On a hit it
  deals 10 direct damage plus 30 radius damage (falling off by half a point per unit) within 128
  units to others around, never to the owner; on a wall, the 30 radius damage and temporary entity
  30 (green blaster impact: position and direction). It stops shooting when the target dies.
- It explodes (harmlessly) when the owner dies.

Hunter **[read]**:

- Model `models/items/hunter/tris.md2`; loop `spheres/h_idle.wav`; turns to the owner's yaw at 40
  degrees a frame while idle.
- It does nothing while its owner lives. When the owner is killed by someone else, it takes the
  killer as its target and, with at least 15 s of life left (topped up to 15 s if less), chases:
  at 500 units/s straight at the target (at a player's eye height) while it can see it, playing
  `spheres/h_active.wav`; when it cannot, it flies to the last place it saw the target (500 units/s
  when farther than 500 units, the distance per second when 20 to 500 away, arriving in one frame
  when nearer than 20), and waits there facing the target playing `spheres/h_lurk.wav`. It glows with
  the blaster and tracker effects while hunting. It explodes when the target dies or its time ends.
- Touching the target or any other damageable thing: 10000 damage of the destroy-armour kind
  (armour cannot save it; invulnerability and god mode still do), knockback 1; touching the world is
  ignored; touching any other non-damageable entity: 512 radius damage within 256 units (never to
  the owner). Then it explodes. A plain corpse in the body queue never sets it off.
- Hunter camera (`huntercam`, question 19): when the hunt starts with the killer 192 units or more
  away, the owner's corpse is gibbed (`misc/udeath.wav`, 4 small meat gibs and a skull) and the
  owner's view rides with the sphere: field of view 140, view height 8, no model, not solid, a 10-unit
  box, view turned toward the killer each frame. When the sphere hits something or times out, the
  owner's view stops where it is. This does not happen when deathmatch flag 1024 (forced respawn) is
  set.

Vengeance **[read]**:

- Model `models/items/vengnce/tris.md2`; loop `spheres/v_idle.wav`; spins at 30 degrees a second on
  two axes.
- When a hit leaves its owner below 25 health (and the attacker is not the owner), it takes the
  attacker as its target with at least 15 s of life left, glows with the rocket effect and flies
  straight at the target at 500 units/s whether or not it can see it.
- Touching the target or any damageable thing: 10000 destroy-armour damage as for the hunter; touching
  the world or any other non-damageable thing: 512 radius damage within 256 units (never to
  the owner). Then it explodes. It explodes when the target dies or its time ends.

When the owner dies **[read]**: the defender explodes at once. A hunter or vengeance sphere explodes
at once only if it has no target yet; one already hunting carries on. The hunter is the only one
that acquires its target from the killing blow itself.

### 17. key_nuke and key_nuke_container

**[read]**

| classname | pickup name | model | icon |
|---|---|---|---|
| key_nuke_container | Antimatter Pod | models/weapons/g_nuke/tris.md2 (spins) | i_contain |
| key_nuke | Antimatter Bomb | models/weapons/g_nuke/tris.md2 (spins) | i_nuke |

- Both are ordinary keys (stay in coop, droppable, digit width 2, pickup sound `items/pkup.wav`); the
  library gives neither any behaviour beyond being a key that map triggers can require. The
  container does nothing by itself.
- On the map `rboss`, outside deathmatch, every player entering the level is given one Antimatter
  Bomb key and has it selected.

## Settings and flags

### 18. The new dmflags bits

All four act only in deathmatch. **[read]**

| value | effect |
|---|---|
| 131072 (no mines) | `ammo_prox` and `ammo_tesla` are removed at map load; random respawn never turns an item into either. The Prox Launcher itself is not removed. |
| 262144 (no stacking double) | while quad is active, double damage adds nothing: the multiplier stays 4 instead of 8. Without it, quad plus double is 8 in deathmatch. |
| 524288 (no nukes) | `ammo_nuke` (the A-M Bomb) is removed at map load; random respawn never turns an item into one. |
| 1048576 (no spheres) | all three sphere items are removed at map load; random respawn never turns an item into one. |

The base flag 2 (no items) now also removes the three spheres and the Doppleganger, as well as the
powerups it removed before (which now include IR Goggles, Double Damage and the compass, since they
use the powerup pickup). **[read]** The base flag 8192 (infinite ammo) removes only items that are
pure ammo (so Prox, Flechettes and Rounds go; Grenades and Tesla, which are also weapons, stay) and
the BFG10K, as before. **[read]**

### 19. gamerules, huntercam, strong_mines, randomrespawn

| setting | default | flags | effect |
|---|---|---|---|
| `gamerules` | "0" | latched | Chooses a deathmatch variant at game start. "2" or "tag" selects Tag; anything else (and every value outside deathmatch) is forced back to "0", normal deathmatch. The value 3 (a ball game) exists in the source but is not selectable in this release. **[read]** |
| `huntercam` | "1" | server info, latched | Nonzero: the hunter sphere's camera of question 16 runs when its owner is killed by someone 192 units or more away, unless deathmatch flag 1024 (forced respawn) is set. **[read]** |
| `strong_mines` | "0" | none | Nonzero: an armed prox mine always lives 45 s, whatever the multiplier it was fired with. **[read]** |
| `randomrespawn` | "0" | none | Nonzero in deathmatch: when a picked-up item respawns it may come back as another item (below); every item that can be given is precached at map load for this. **[read]** |

Tag (`gamerules` 2) **[read]**:

- The token is `dm_tag_token`, "Tag Token", model `models/items/tagtoken/tris.md2`, icon
  `i_tagtoken`. A map without one gets one at a spawn point. It exists only in Tag.
- Picking it up makes the player the holder, heals them to full (adds 200, capped at their maximum)
  and gives them Body Armor as a pickup. The holder shows the tag-trail effect and has the picture
  `tag3` on the scoreboard.
- A kill by the holder scores 3; every 5th kill in a row as holder gives the holder a Quad Damage
  that is used at once. Killing the holder scores 5 and makes the killer the new holder (with the
  same heal and Body Armor), except when the kill was by a hunter sphere or one of the doppleganger's
  attacks, or the killer is already dead: then the token is dropped instead.
- The holder's death or disconnect drops the token. A dropped token can be taken after 1 s; if
  nobody takes it, it moves to the farthest deathmatch spawn point after 30 s (3 s if it lies in lava
  or slime).
- Damage between two players neither of whom holds the token is cut to three quarters.
- Other kills score as in deathmatch; a suicide costs 1.

Random respawn's groups **[read]**:

- The choice is made when the item comes back, not at map load; the new item appears where the old
  one was and the old one is removed. If no substitute is found the item returns as itself.
- Power armour (screen and shield) always returns as itself.
- Health: stimpacks (`item_health_small`) return as themselves. Any other health, and Adrenaline,
  becomes `item_health` 60 %, `item_health_large` 30 %, `item_adrenaline` 9 %, `item_health_mega`
  1 %.
- Armour: shards return as themselves. Any other armour becomes `item_armor_jacket` 60 %,
  `item_armor_combat` 30 %, `item_armor_body` 10 %.
- Everything else becomes an item picked with equal chance from all the item-list rows of the same
  kind: weapons with weapons, ammo with ammo (Grenades and Tesla count as ammo), powerups with
  powerups (the A-M Bomb, compass, IR Goggles, Double Damage, spheres and Doppleganger are
  powerups), keys with keys. Rows that cannot be given (the Tag Token) and rows barred by dmflags
  131072, 524288 or 1048576 are left out. Items with no kind (Ancient Head, Bandolier, Ammo Pack)
  return as themselves.
- **[derived]** Edge cases: the weapon group includes the Blaster row, which has no world model and
  no pickup; and the Disruptor and Rounds rows, which are removed on spawning when `g_disruptor` is
  zero. The source does not guard against either choice.

### 20. Other new settings and commands

Settings **[read]**:

| setting | default | flags | effect |
|---|---|---|---|
| `g_disruptor` | "0" | none | Nonzero lets `weapon_disintegrator` and `ammo_disruptor` spawn and lets the Bandolier and Ammo Pack raise the Rounds limit (question 2). Zero removes both at spawn in every mode. |
| `coop_baseq2` | "0" | latched | Nonzero: in coop, map entities are filtered by skill alone, as in single player, ignoring the "not in coop" spawn flag and the "coop only" rule. |
| `sv_stopspeed` | "100" | none | Registered by this library (the stopping speed used by its non-player friction). |
| `coop_pickup_weapons` | "0" here (the base library registers "1") | archived | Unchanged meaning; only the default differs. |

The ball game's settings (`dball_team1_skin`, `dball_team2_skin`, `goallimit`) are registered only
when that game starts, which this release never does (question 19). **[read]**

Commands **[read]**:

- `disguise`: sets the player disguised, with no cheat check. Monsters do not pick a disguised
  player as a sight target, and the player's own movement and impact noises are not heard; firing a
  weapon (a weapon noise) lets monsters find the player for 0.5 s, and the disguise is lost as soon
  as a monster takes the player as its enemy.
  The player entity carries the "use disguise" render flag. The disguise is kept across levels.
- `entcount`: prints "<n> entites active" (spelled that way) to the server console.
- The base library's `spawnentity` and `spawnonstart` commands are absent from this library.
- `give all` and `give <name>` skip the Tag Token ("item cannot be given" on the developer console
  when named). `kill` also removes the player's sphere and any Disruptor damage in progress.

## Obituaries and words

### 21. New obituary lines

Format **[read]**: a kill by another player prints "<victim> <message> <killer><suffix>" followed by
a line break, with no full stop; the suffix is appended directly to the killer's name. A suicide
prints "<victim> <message>." with a full stop. Both print in deathmatch and coop.

Kills by another player **[read]**:

| cause | line |
|---|---|
| Chainfist | `<victim> was shredded by <killer>'s ripsaw` |
| ETF Rifle | `<victim> was perforated by <killer>` |
| Plasma Beam | `<victim> was scorched by <killer>'s plasma beam` |
| Tesla zap | `<victim> was enlightened by <killer>'s tesla mine` |
| prox mine | `<victim> got too close to <killer>'s proximity mine` |
| A-M Bomb | `<victim> was nuked by <killer>'s antimatter bomb` |
| vengeance sphere | `<victim> was purged by <killer>'s vengeance sphere` |
| defender sphere bolt (direct hit) | `<victim> had a blast with <killer>'s defender sphere` |
| hunter sphere | `<victim> was killed like a dog by <killer>'s hunter sphere` |
| Disruptor | `<victim> was annihilated by <killer>'s disruptor` |
| doppleganger's vengeance sphere | `<victim> was purged by <killer>'s doppleganger` |
| doppleganger's hunter sphere | `<victim> was hunted down by <killer>'s doppleganger` |
| (worded, never caused) doppleganger explosion | `<victim> was blown up by <killer>'s doppleganger` |
| (worded, never caused) disintegrator | `<victim> lost his grip courtesy of <killer>'s disintegrator` |

Suicides **[read]**:

- The doppleganger explosion has its own suicide line, which this library never causes: "<victim>
  got caught in his own trap.", "... her own trap." for a female player, "... it's own trap." (spelled
  that way) for a neutral one.
- Suicides with the new weapons, mines, Tesla and A-M Bomb use the base fallback: "<victim> killed
  himself." / "herself." / "itself.".

Consequences worth knowing **[derived]**:

- The defender bolt's 30-point splash has no cause attached, so a kill by that splash prints the
  plain "<victim> died." and costs the victim a point, as an unworded death does.
- A Tesla's big explosion (water, slime or lava) reads as grenade shrapnel: "<victim> was shredded by
  <killer>'s shrapnel", or as a suicide "tripped on his own grenade." (her / its).
- Every thrown hand grenade is now marked as "held" (question 23), so its splash kills read "<victim>
  feels <killer>'s pain" and its splash suicides "<victim> tried to put the pin back in."; the player
  struck directly by the grenade still reads "<victim> caught <killer>'s handgrenade".
- The library words no new monster attack: in coop, a player killed by a monster prints "<victim>
  died." as before.

### 22. Other prints a player reads

**[read]**

- "Only one sphere at a time!" (to the player, high priority), on using a sphere while one is out.
- The compass prints to its user "Origin: <x>,<y>,<z>    Dir: <yaw>" (four spaces before "Dir"),
  with the origin rounded to whole units and the yaw as a whole number of degrees from 0 to 359.
- No pickup print beyond the base's item name on the status bar (from the item's pickup name), no
  A-M Bomb countdown text, no sphere or Doppleganger messages, no Tag messages.
- "Can't drop current weapon" (base) still guards the last Grenades while holding Grenades; it does
  not guard the Tesla, so a player may drop their last Teslas while holding them. **[derived]**

## Damage changes for the base game's weapons

### 23. What the pack changes in the base weapons and damage rules

Weapons **[read]**:

- Every base weapon multiplies by the new multiplier (4 quad, 2 double, 8 both in deathmatch)
  instead of a fixed 4 under quad; knockback is multiplied where it was multiplied under quad.
  Base damage numbers are unchanged (Blaster 15 in deathmatch / 10 otherwise, Shotgun 12 pellets,
  Super Shotgun 20, Railgun, BFG10K 200 in deathmatch / 500 otherwise, Grenade Launcher 120 with a
  160-unit radius, Rocket Launcher 100 to 119 with 120 radius damage over 120 units).
- Each fire frame plays `misc/ddamage3.wav` under double damage when quad is not active.
- Hand grenades **[read]**: thrown from 2 forward, 6 right, eye height minus 14 along the view's up
  axis (the base threw from 8, 8, eye height minus 8); throw speed capped at 800; the quad or double
  sound plays at the throw; and every throw marks the grenade as "held", which changes the splash
  obituaries (question 21). The in-hand detonation, the timer (3 s) and the 125 damage with a 165
  radius are unchanged.
- The Railgun's line and the BFG10K's lasers now also strike damageable things (prox mines, Teslas,
  A-M Bombs, the Doppleganger) as they do monsters and players.
- The out-of-ammo choice changes as in question 4 (the HyperBlaster is dropped from it).

Damage rules **[read]**:

- New damage kinds: skip regular armour with half-strength power armour (flechettes); skip power
  armour (the Disruptor); destroy armour, where armour still absorbs but the full damage reaches
  health (the Chainfist, the hunter and vengeance spheres).
- The defender sphere halves damage to its owner before invulnerability and armour.
- With team no-friendly-fire (flag 256) teammates still take A-M Bomb damage.
- Blood: the Chainfist sends heavier blood (42); "mechanical" things (prox mines, Teslas) send
  electric sparks (46) when they take damage.
- Monsters struck by a Tesla attack the Tesla; a deathmatch variant (Tag) may change damage.
- Easy skill halving, the surprise double damage on monsters, and the knockback formula are
  unchanged.

The player **[read]**:

- In deathmatch, a player gibbed below minus 80 health throws 8 small meat gibs instead of 4.
- The player's gravity is the server gravity times the player's own gravity factor, which a gravity
  trigger can set and which resets to 1 after every move (the base game ignored the factor for
  players).
- Every player is always IR-visible. While riding the hunter camera the view height is 8.
  **[derived]** The library never clears the hunter-camera mark from the player, so the source does
  not settle whether a later life keeps the 8-unit view height; that would need a run to measure.
- The weapon-change animation and the other player frames are unchanged.
