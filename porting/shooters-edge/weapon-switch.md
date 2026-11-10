# The weapon after a shot: refire, the held switch, the pack, what others see

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of how long a
weapon stays in hand after a shot in the classic match mod (KTX) on the modern server (mvdsv), what
a dead player's backpack holds, what an onlooker can tell, and the community client's (ezQuake)
switch-after-fire settings. Our bots use it to infer which weapon a killed enemy's pack holds. No
code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-09) read KTX at commit `b1b23a9`,
mvdsv at `c261efe` and ezQuake at `d3ae205` (github.com/QW-Group). Each fact is marked **read**
(the behaviour as read), **derived** (follows from read facts put together) or **open** (the
source does not settle it). `deathmatch4.md` holds the deathmatch 4 pack rules; the pack-weapon
setting `k_frp` (0 the weapon in hand, 1 the best weapon with ammo, 2 the last weapon fired) and
`dp` are taken as known.

Terms: *refire* is the time from a shot until the player may fire or switch again. *A command* is
one movement command from the client (typically 77 a second); the player's weapon think runs
after each one, at the server's current time. Units: seconds (s), units (u).

---

## 1. Refire times

Normal deathmatch and team modes (read):

| Weapon | Refire |
| --- | --- |
| Axe | 0.5 s |
| Shotgun | 0.5 s |
| Super shotgun | 0.7 s |
| Grenade launcher | 0.6 s |
| Rocket launcher | 0.8 s |
| Nailgun, super nailgun, lightning gun | a stream, one shot every 0.1 s while fire is held, then 0.2 s after the last shot |

- A stream's first shot leaves in the command that starts it, then one every 0.1 s on the
  player's 0.1 s animation clock. A late tick is pulled back by at most 0.05 s (read). A tap
  shorter than 0.1 s fires one shot (derived).
- Quad changes no refire (read). Deathmatch 4, teamplay and the 4on4 usermodes change none (read).
- Changed only by (read): the CTF haste rune (axe, shotgun, grenade launcher 0.5 s minus a tenth
  of the rune power, super shotgun and rocket launcher 0.5 s minus a twentieth, so 0.3 s and 0.4 s
  at the default power 2.0, derived), `k_yawnmode` (super shotgun 0.8 s) and instagib
  (`k_instagib` 1 shotgun 1.2 s, 2 shotgun 0.7 s).
- Fire is ignored for 50 ms after a respawn (read).

## 2. A switch asked for during the refire is held

- The server keeps a command's weapon impulse when it is non-zero, and the mod leaves it pending
  while the refire runs, retrying after every later command (read). It is carried out by the first
  command at or after the refire end. Selects 1 to 8 and the cycles 10 and 12 are held; the CTF
  hook's select is never held (read).
- A newer non-zero impulse replaces the pending one (read).
- The check for the weapon and its ammunition comes only when the refire is over: `no weapon` or
  `not enough ammo`, and the impulse is dropped (read).
- A pending impulse ends a nailgun or lightning stream at its next 0.1 s tick even with fire held
  (read).
- The switch comes before the fire check in the same think, so the new weapon can fire in the
  command that selects it (read).
- No raise time (read): the new weapon counts at once. The body's pose is reset at once if it was
  in a firing pose or needs the other pose family, so a switch cuts a firing animation short.

**Earliest time a weapon leaves the hand** (derived): rocket launcher 0.8 s after the rocket,
grenade launcher 0.6 s after the grenade, nailguns and lightning gun 0.2 s after the last shot
(0.1 to 0.2 s after fire is released), each at the first command of that player at or after it.

## 3. Running out

- After the last round is fired the empty weapon stays in hand through the refire. At the first
  command strictly after the refire end the mod switches to the best weapon (read). So for 0.8 s
  after the last rocket a death leaves a launcher pack with 0 rockets under `k_frp` 0 (derived).
- The best weapon (read): with the userinfo key `w_rank`, its first weapon held with ammunition,
  else the axe. Without it: lightning gun (not when more than waist-deep), super nailgun, super
  shotgun, nailgun, shotgun, axe. The launchers are never picked by default.
- The same switch happens on firing an empty weapon and when a stream runs dry (read).

## 4. The pack

- With `k_frp` 0 the pack's weapon is the weapon in hand at death, as left by the last switch
  carried out (read). A switch still held by the refire does not count (derived).
- One weapon or none. An axe or shotgun pack is named so but adds only ammunition, since every
  player has both (read, derived). In CTF a hook in hand gives a pack with the hook and no name.
- Names (read): Axe, Shotgun, Double-barrelled Shotgun, Nailgun, Super Nailgun, Grenade Launcher,
  Rocket Launcher, Thunderbolt.
- No pack when he has no ammunition at all and holds none of the six droppable weapons (super
  shotgun to lightning gun) and, under `k_frp` 2, last fired none of them (read).
- `k_frp` 1's pick (read): the first held with ammunition of rocket launcher, lightning gun,
  grenade launcher, super nailgun, super shotgun, nailgun.
- Ammunition (read): all four kinds, all of each, as he had them at death, uncapped. Caps only
  under `k_yawnmode` (25 shells, 50 nails, 25 rockets, 25 cells) and fresh-teams packs
  (`k_freshteams`, `k_freshteams_limit_packs`, `k_freshteams_pack_shells` and its siblings). The
  picker is capped at 100 shells, 200 nails, 100 rockets, 100 cells. In deathmatch 3 and 5 a
  launcher pack lifts him to at least 5 rockets.
- The pack spawns 24 u below the body's centre, moving up at 300 u/s and up to 100 u/s sideways
  on each axis, lasts 120 s, and always looks the same (read).
- `k_yawnmode` forces `k_frp` 2, and in deathmatch 1 a player not inside a refire at death drops
  a shotgun pack (read).

## 5. Who is told

- At the drop no player is told (read). For rocket launcher and lightning gun packs a demo-only
  line goes into the MVD: the mod's tag, `drop`, then the pack's entity number, the weapon's item
  bit and the dropper's entity number. The match log records weapon and counts.
- At the pickup (read): the picker gets `You get`, the weapon's name if it is new to him, then
  the non-zero counts; everyone near hears the one backpack sound. For rocket launcher and
  lightning gun packs another demo-only line, `bp`, gives the pack's and the picker's entity
  numbers.
- Spectators (read): with `k_spec_info` on (set to 1 by every usermode), each spectator picks a
  level with `moreinfo` (userinfo `mi`): 1 rocket launchers, 2 grenade launcher, rocket launcher
  and lightning gun, 3 every weapon, 0 and 4 none. They get "<name> got backpack with <weapon>"
  (team in front in team games), only when the picker lacked that weapon.
- Team chat a player sends himself (read): `tpmsg lost` adds "DROPPED rl" or "DROPPED lg" when
  that was the weapon in hand at death (wrong under `k_frp` 1 or 2, derived), `tpmsg took` says
  "took rl pack" or "took lg pack".

## 6. What an onlooker sees and hears

- Poses (read): every player's frame goes to everyone who sees him. Axe (and hook) run 0 to 5,
  stand 17 to 28; any gun run 6 to 11, stand 12 to 16. So axe against gun, not which gun.
- Firing frames, 0.1 s each (read): nailguns 103 and 104 alternating, lightning gun 105 and 106,
  both launchers 107 to 112, both shotguns 113 to 118, the axe one of four swings (119 to 122,
  125 to 128, 131 to 134, 137 to 140). Death with the axe plays the axe death, with a gun one of
  three others.
- Every gun shot (not the axe) sends a muzzle flash to all in view (read). The projectiles differ:
  small and large nails, grenade and rocket, the beam (read).
- The view frame and the weapon-in-hand stat go only to the player, spectators tracking him and
  the MVD (read). An opponent sees a gun model on the player only with visible weapons:
  `k_allow_vwep` 1 at map load (default 0), `k_vwep` 1 (default 1, the `vwep` command), and a
  client that supports them; it changes at the switch and goes at death.
- A switch makes no sound (read).
- Shot sounds (read), none shared: axe `weapons/ax1.wav`, shotgun `weapons/guncock.wav`, super
  shotgun `weapons/shotgn2.wav`, nailgun `weapons/rocket1i.wav`, super nailgun
  `weapons/spike2.wav`, grenade launcher `weapons/grenade.wav`, rocket launcher
  `weapons/sgun1.wav`, lightning gun `weapons/lstart.wav` at the start and `weapons/lhit.wav` at
  most every 0.6 s. A super shotgun with one shell sounds like the shotgun and a super nailgun
  with one nail like the nailgun (read). Quad adds `items/damage3.wav` at most once a second.

## 7. The client's switch-after-fire settings

All read, all defaulting to 0:

- `cl_weaponhide` 1: on releasing fire (`-attack`, or the release of a `+fire` key, once no key
  holds attack) send a select of the shotgun if he has it with a shell, else the axe. 2: the same
  only in deathmatch 1. Every weapon, no delay: the select rides the next command.
- `cl_weaponhide_axe` 1: hide to the axe always.
- `cl_weaponpreselect` (the `weapon` command only): 0 select at once; 1 remember the best weapon
  of the list and send it with the attack when `+attack` is pressed; 2 select at once while
  attack is held, else remember; 3 and 4 are 1 and 2 in deathmatch 1, 0 elsewhere.
- `cl_weaponforgetorder` 1: the best weapon is picked when the command is given and the list
  becomes it, then the shotgun and the axe.
- `+fire <weapons>` / `-fire`: one key that selects the first held weapon of its list and holds
  attack in the same command, and on release lets go and hides. `+fire_ar` stacks several such
  keys (anti-rollover).
- mvdsv can run the hide and preselect itself for a client that asks, with the same timing
  (derived); the client read leaves that off (`cl_pext_serversideweapon`, not built in).
- KTX's `cmd wreg <char> [+|-]<digits>` registers a server-side script run by sending the one
  character: the first held weapon of the list is selected (held through refire as usual), `+`
  holds attack on the server until a `-` script, at most 20 registered (read).
- KTX's impulses 156 and 164 switch to the rocket launcher and fire it back or down in one
  command, outside a refire, and do not switch back (read).

**The launcher's time in hand** (derived): with `cl_weaponhide` and a tap, the hide is held and
carried out at 0.8 s after the rocket plus at most one command interval (about 13 ms). A kill of
that player within 0.8 s of his last rocket finds the launcher in his pack under `k_frp` 0. Grenade
launcher 0.6 s, lightning gun 0.2 s after the last bolt.

## Open

- Which impact sound the client plays for each hit effect was not traced.
