# Deathmatch 4: what the classic match mod does

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of what the
classic match mod (KTX) does in deathmatch 4 ("dmm4"), for our own duel mode where both players
spawn fully armed (three-minute duels on povdmm4). No code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-09) read KTX at commit `f316310`
(github.com/QW-Group) and the example server configs it ships. Each fact is marked **read**
(the mod's behaviour as read), **derived** (follows from read facts put together) or **open**
(the source does not settle it). `server-settings.md` already holds the spawn invincibility
setting's basics, the tie-break gap of 2 and the povdmm4 usermode config (timelimit 3,
deathmatch 4, `k_overtime 2`).

Terms: *in a match* means after the countdown, while the game runs. *Prewar* is the warm-up
before it. Units are seconds (s), map units (u), health and armour points.

The mod has five sub-modes that exist only in deathmatch 4 and change these rules: midair,
instagib, LGC mode, Tribe of Tjernobyl (ToT) mode and grenade mode. They are left out here.

---

## 1. The spawn loadout and the weapon in hand

**Every spawn in a match** (read):
- Health 250. The maximum health stays 100, which only matters for health pickups (§ 3).
- Red armour, 200 points at the red type's 80 % absorption, replacing any other armour.
- 255 shells, 255 nails, 255 rockets, 255 cells.
- All eight weapons: axe, shotgun, super shotgun, nailgun, super nailgun, grenade launcher,
  rocket launcher, lightning gun.
- Then every weapon named in `k_disallow_weapons` is taken away again (§ 5). Every usermode sets
  it to 16, the grenade launcher, so a default spawn holds the other seven. The ammunition is
  not reduced for a disallowed weapon.
- Spawn invincibility for `dmm4_invinc_time` seconds (default 2 s, § 6). No other powerup.

**The first spawn of a match** is no different (read). When the countdown ends the mod marks
the match as running first, strips the map's items (§ 3), and only then places every player, so
the first spawn gets the same loadout and the same protection as every later one.

**Duel, FFA and team games** get the same loadout (read). The number of players and teamplay
do not change it.

**The weapon in hand** is the rocket launcher (read). If it is not held (disallowed), the mod
falls back to the best weapon held: the lightning gun if the player has cells and is not more
than waist deep in water, then super nailgun, super shotgun, nailgun, shotgun, axe. The grenade
and rocket launchers are never chosen by this fallback. A player may override the order with the
userinfo key `w_rank`, the digits 1 to 8 in preference order (read).

## 2. The warm-up's loadout

The dmm4 loadout and the spawn protection apply only in a running match (read). In prewar every
deathmatch mode, dmm4 included, gives (read):
- all eight weapons, rocket launcher in hand,
- 1000 health and 1000 armour points, but with no armour type, so the armour absorbs nothing
  (derived),
- 100 shells, 200 nails, 100 rockets, 100 cells.

Also in prewar (read): no ammunition is used up, armour is never worn down, a single hit of more
than 450 kills outright, `k_disallow_weapons` is not applied, and the map keeps all its items but
none can be picked up.

## 3. Health and armour over time, and the items

**No rot, no regeneration** (read, derived). Health above 100 rots only while the player owns a
picked-up megahealth (1 point a second down to 100), and the megahealth is stripped in dmm4, so
the spawn's 250 stays until damage takes it. Armour does not decay. Nothing regenerates. Health
rises only through backpacks (§ 4).

**What the match strips**, at the moment it starts (read):
- every weapon pickup (super shotgun, nailgun, super nailgun, grenade launcher, rocket launcher,
  lightning gun),
- every ammunition box (shells, nails, rockets, cells),
- the megahealth.

**What it keeps** (read): the 15 and 25 health boxes, the green, yellow and red armours, and the
powerups (quad, pentagram, ring, biosuit). Powerups stay but are hidden and untouchable while
`k_pow` is 0 (or per powerup `k_pow_q`, `k_pow_p`, `k_pow_r`, `k_pow_s`). The shipped 1on1
usermode sets `k_pow 0`, so a 1on1 dmm4 duel has no powerups unless a player turns them on with
the `powerups` command (read, derived). `k_noitems 1` (the `noitems` command) strips everything,
health, armour and powerups included (read). Which items povdmm4 holds and where is the map's,
not the mod's (open).

**Respawn timers** for what is kept, the same as in deathmatch 1 (read): health boxes 20 s,
armour 20 s, quad and biosuit 60 s, pentagram and ring 300 s.

**Pickup rules in dmm4**:
- A player under any invincibility (spawn protection, pentagram, the backpack bonus) cannot take
  health boxes, armour or backpacks. Powerups can still be taken (read).
- Health boxes heal only up to 100, so they do nothing until the player is below 100 (read,
  derived).
- Armour is taken only if its type times its value beats the player's current type times value
  (the normal rule: green 100 at 30 %, yellow 150 at 60 %, red 200 at 80 %) (read). Against the
  spawn's red 200 that means green only below 37.5 points, yellow only below 112.5 and red only
  below 200 (derived).

## 4. Backpacks and the bonus powers

**The drop** (read): a dying player drops a backpack when `dp` is 1 (every usermode sets it), in a
running match only, except after the `kill` command. It holds the dead player's ammunition and
weapon in hand, is thrown up at 300 u/s with a random sideways speed of up to 100 u/s on each
horizontal axis, and vanishes after 120 s untaken. Dropping powerups on death (`dq`, `dr`) is
off in every usermode.

**The pickup in dmm4 ignores the contents** (read):
- It adds 10 health, with the private message "You get 10 additional health" and a screen flash.
- The sound is the pentagram's protection sound when the new health is above 250 and below 300,
  otherwise the normal weapon pickup click.
- When health reaches 300 or more, the taker gets **bonus powers**: 30 s of pentagram and 30 s of
  OctaPower (§ 7), cells set to 0, a loud announcement sound, and the broadcast "<name> gained
  bonus powers!!!". Health is not capped at 300.
- Five packs from a fresh spawn reach exactly 300 if no damage is taken (derived).

## 5. Settings and commands

**`dmm4_invinc_time`** (read): 0 or empty means the 2 s default, a negative value turns it off,
others are clamped to 0 to 30 s. Every usermode resets it to empty.

**`k_disallow_weapons`** (read): a bit mask of the weapons taken away at a dmm4 spawn in a match:
1 shotgun, 2 super shotgun, 4 nailgun, 8 super nailgun, 16 grenade launcher, 32 rocket launcher,
64 lightning gun, 4096 axe. Usermode default 16. When any is set, the settings shown at match
start include a line "Noweapon" with the disallowed weapons' short names.

**`spawn666time <seconds>`** (read): dmm4 only, otherwise "command allowed in dmm4 only". The
command clamps to 0 to 2 s (only the setting itself reaches 30). 0 turns protection off. It
broadcasts "<name> set spawn invincibility time to N.Ns". Without an argument, or during a match,
it prints "spawn invincibility time is N.Ns".

**`noweapon <name or digit>`** (read): dmm4 only, before the match. It toggles one weapon in
`k_disallow_weapons`: axe, sg, ssg, ng, sng, gl, rl, lg, or the digits 1 to 8 in that order. It
broadcasts "<name> allows <weapon>" or "<name> disallows <weapon>". An unknown name answers
"unknown weapon name <x>". Without an argument, or during a match, it prints "weapons disallowed:"
and the list, or "none". `no_lg` and `no_gl` are shortcuts.

**`dmm4`** (read): sets deathmatch 4 and also `timelimit 3`, and broadcasts "Deathmatch 4". The
time limit is set only by this command, not when a config sets `deathmatch 4`.

Not found (read): no dmm4 setting for rocket jumps, for the spawn's health, armour or
ammunition amounts, or an "all weapons" switch beyond `k_disallow_weapons`.

## 6. Spawn invincibility

**What it blocks** (read): it is the pentagram's protection. All damage to health is cancelled,
whatever the source: other players, the player's own rockets, lava, slime, drowning. Only the
`kill` command gets through.

**What it does not block**:
- Armour: the armour's share of each hit (80 % for red) is still taken off the armour points, and
  armour that runs out is lost (read). A protected player hit by rockets keeps his health but
  loses armour.
- Knockback: hits push a protected player exactly as they push anyone (derived: the push is
  applied apart from the damage cancel).

**What ends it** (read): only its timer, or death. Firing does not end it. Pickups do not end it
(health, armour and packs are refused while it lasts).

**The protected-telefrag cases** (read). Spawning goes through the same placement as a teleport,
so these apply to spawns too:
- A protected player arriving on another protected player: both lose protection and both die.
  Broadcast: "<victim> was telefragged by <name>'s Satan's power".
- An unprotected player arriving on a protected one: the arriving player dies instead.
  Broadcast: "Satan's power deflects <name>'s telefrag".
- An arrival on an unprotected player kills him (the ordinary telefrag).

**What is seen and heard** (read):
- The pentagram icon is on the status bar while it lasts.
- No red glow in dmm4, for spawn protection or for a real pentagram.
- Because 2 s is under the 3 s warning mark, the warning starts at once: the private message
  "Protection is almost burned out", a screen flash and the pentagram's running-out sound, then a
  screen flash each second until it ends. A time over 3 s starts the warning when 3 s remain.
- An absorbed hit makes the protected player emit the pentagram's protection sound, at most once
  every 2 s.

## 7. Damage changes and ammunition

**OctaPower** (read): in dmm4 the quad is named "OctaPower" ("You got the OctaPower", "OctaPower
is wearing off") and multiplies damage by 8 instead of 4. On pickup it **strips** the holder's
armour and cells to 0. When it wears off after 30 s it **restores** 255 cells, sets armour to 1
point of red, and caps health at 100. Knockback grows with the multiplied damage, so its rockets
push 8 times as hard (derived). The bonus powers of § 4 end the same way, since they include an
OctaPower (derived).

**The axe** hits for 75 in dmm4, against 20 in deathmatch 1 (read).

**The lightning gun underwater** (read): fired while more than waist deep in a running match, a
coin flip (50 %) kills the firer outright (4000 damage to himself). Otherwise all his cells are
spent in a normal discharge: radius damage of 35 per cell when discharges are on (`k_dis`, default
1). In deathmatch 1 it is always the discharge.

**Unchanged** (read): self-damage from one's own rockets (half the splash, as everywhere),
falling damage, ordinary telefrags, and rocket knockback (8 u/s of speed per point of damage,
after the quad factor, along the line from the explosion's centre to the target).

**Ammunition is never used up** in a dmm4 match, for any weapon (read). The counters stay at 255
(derived), except cells after an OctaPower pickup or an underwater discharge.

## 8. Spawn points under `k_spw 4`

Nothing in spawn selection is specific to dmm4. It follows `k_spw`, which every usermode sets to
4, "KTX2 respawns" (read). In a match (read unless marked):
- The candidates are the map's deathmatch spawn points.
- A point is ruled out while a live other player is within 84 u of it, but only if that player
  spawned or left a teleporter less than 0.78 s ago (derived: choosing a point sets a 2.6 s
  window, then the spawn's own teleport placement shortens it to 0.78 s). A player standing there
  longer does not block it, and an arrival overlapping him telefrags him.
- One remaining point is chosen uniformly at random.
- If it is the player's previous point, the choice is made once more, so a repeat is rarer but
  possible. The previous point is remembered only on maps with more than two points.
- If every point is ruled out, any point is chosen at random.
- There is no rule for distance from the opponent and no line-of-sight test.

## 9. Scoring

No frag rule is specific to dmm4 (read). The general rules:
- A kill of another player: +1. Killing oneself: -1, or -2 with the `kill` command.
- A kill made while the killer is invincible counts normally.
- In both protected-telefrag cases (§ 6) the player who dies loses 1 frag and nobody gains one.
- A kill within 2 s of the victim's spawn is counted as a "spawn frag" in the statistics, with no
  effect on the score.
- `k_dmgfrags 1` replaces kill frags with one frag per 100 damage dealt.
- Tie-break overtime ends at a gap of 2 in dmm4 (`server-settings.md`).
