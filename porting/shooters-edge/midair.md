# Midair: what the classic match mod does

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of the classic
match mod's (KTX) "midair" mode: rockets only, a hit counts only on an airborne target, points by
height. For our own server's midair mode. No code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-12) read KTX at commit `b1b23a9`
(github.com/QW-Group) and the example server configs it ships. Each fact is marked **read**
(the mod's behaviour as read), **derived** (follows from read facts put together) or **open**
(the source does not settle it). Midair is a sub-mode of deathmatch 4: `deathmatch4.md` holds
the rules it starts from (the pack rule, bonus powers, the item strip, `dmm4_invinc_time`).

Terms: *in a match* means after the countdown, while the game runs. *Prewar* is the warm-up
before it. Units are map units (u), seconds (s), health and armour points.

---
## 1. Turning the mode on and off

- The mode is one server setting, `k_midair`, 0 off (the default in the shipped server config)
  or 1 on (read). It is a flag on top of deathmatch 4: it exists only while `deathmatch` is 4,
  and it rides on whatever usermode is loaded (duel, FFA, team games); the usermode is not
  changed by it (read).
- Players switch it with the client command `midair`, a toggle (read). The command is open to
  every player and to admin spectators, with no vote (read). It is refused while a match is
  running (countdown or game) with a message to the caller that the command is locked while the
  match is in progress, and refused while race mode is on (read). So it is changed in prewar only.
- Turning it on is refused when `deathmatch` is not 4: the caller alone gets a line saying that
  midair requires dmm4 (read). Turning it off is allowed in any deathmatch mode (read).
- When it toggles, everyone gets a console line: the player's name, then "enables" or
  "disables", then the word Midair in the coloured (red) text (read).
- Turning it on also switches off the other deathmatch-4 sub-modes: instagib (`k_instagib`),
  LGC mode (`k_lgcmode`), ToT mode (`k_tot_mode`) and grenade mode (`k_dmm4_gren_mode`), all
  silently (read). Turning on instagib, LGC mode or ToT mode switches midair off (read).
- It is forced off: when a map loads with `deathmatch` other than 4 (read); when a player changes
  the deathmatch mode away from 4 (read); and by the settings every usermode applies when it is
  loaded, which include `k_midair 0` (read). A map's own usermode config can turn it back on
  (question 9).
- No map restart is needed (derived): the setting is read at each spawn, each hit and each
  respawn, and the item strip happens when the match starts (question 2).
- The help list of commands shows `midair` as "toggle midair mode"; the command's description
  is "turn midair mode on/off"; the list of known game modes includes "midair" (read).
- The match-start countdown's centred settings list shows a line "Midair on" while it is on
  (read). The serverinfo key `mode` gets the suffix "-midair" (read). The statistics file's mode
  field is "midair" (read). A duel's automatic demo name gets "_midair" after "duel" (read).

## 2. The spawn

During a match, every spawn in midair gives (read):
- Health 250.
- Red armour: 200 points at the red type's 80 % absorption, and the red armour icon.
- The axe and the rocket launcher only; the rocket launcher is in hand.
- 255 rockets; 0 shells, 0 nails, 0 cells. In deathmatch 4 a rocket shot does not use up
  ammunition during a match, so the rockets never run out (read).
- No spawn invincibility, whatever `dmm4_invinc_time` says: the mode forces it off (read).
- No powerup.

Prewar spawns do not get this kit; the deathmatch-4 loadout block is skipped before the match
(read). What a prewar spawn holds is the mod's ordinary prewar spawn (not specific to midair).

Items on the map (read): when the match starts, the mod removes, in midair (as in instagib, race
and rocket arena), every weapon pickup, every ammunition box (shells, spikes, rockets, cells),
every health box (small, large and mega), every armour (green, yellow, red) and all four
powerups (pentagram, biosuit, ring, quad). Anything else stays: teleporters, doors, buttons,
platforms, triggers (derived: only those kinds are named). Backpacks dropped by dead players still
appear when drop packs (`dp`) is on (read; see question 8 for what they give).

## 3. Damage

**When a rocket does damage.** Only during a match, any hit (direct or splash) from a rocket on
another player follows these rules (read):
- The hit counts only when the victim's height above the floor (below) is at least the minimum
  height set by `k_midair_minheight` (read):

  | `k_midair_minheight` | minimum height | name printed when it is set |
  | --- | --- | --- |
  | 0 | 64 u | ground |
  | 1 (default) | 128 u | bronze |
  | 2 | 256 u | silver |
  | 3 | 512 u | gold |
  | 4 | 1024 u | platinum |

  Any other value acts as 64 u (read). The names here are only labels for the setting; they do
  not match the scoring bands of question 5, whose thresholds are different (derived).
- A second, fixed rule also refuses rocket damage to a victim lower than 45 u, unless the victim
  is in water at least to the waist (read). Since the smallest minimum height is 64 u, this
  45 u rule never changes the outcome of a rocket hit; it matters only for knockback
  (question 4) (derived). There is no water exception from the minimum-height rule (read).
- A hit that counts kills outright: the damage taken is set to 9999 whatever the armour, so the
  victim always dies and is gibbed (read for 9999; derived that 250 minus 9999 is far below the
  gib threshold of -40).
- Direct hit and splash are treated the same (read). The rocket's own damage numbers are
  unchanged by the mode: 110 on a direct hit, 120 for the blast's centre falling off with
  distance (read); they only matter for armour and knockback here.

**How "in the air" is measured** (read): a line is traced straight down from the victim's origin,
2048 u long, passing through other players and monsters (only the world stops it). The height is
the bottom of the victim's bounding box minus the end point of that trace. If nothing is hit
within 2048 u the trace ends at its full length, so the height comes out at about 2024 u
(derived: the player box reaches 24 u below the origin). The on-ground flag and the velocity are
not used (read). Whether a player standing on the floor measures 0 or -1 depends on how the
server engine pads an entity's box, which the mod does not settle (open).

**Grounded and other hits** (read):
- A rocket on a target below the minimum height does no health damage. Armour is still drained:
  the armour's share (80 % of the rocket's damage, rounded up) is taken off the armour value
  before the midair rules zero the health damage (read; derived that a grounded player loses
  red armour to hits and may lose it entirely).
- The knockback is still applied (question 4).
- Damage that is not a rocket is dropped entirely for players, except these, which do normal
  damage: water (drowning), lava, slime, telefrags (all three kinds), the suicide command, and
  head stomps (read). So the axe does nothing to a player, and falling does nothing (derived).
- Anything that is not a player (a shootable button or door) takes normal damage (read).

**Self damage** (read): your own rocket never takes your health. Your armour is still drained by
the armour's share of the blast (derived from the same order as above).

**The attacker's own height** does not decide whether a hit counts (read); it only enters the
score band (question 5).

**Quad** in midair does not multiply damage (read; it changes the rocket's speed, question 8).

Prewar: as in every KTX mode, prewar hits do not reduce health unless the damage is enormous
(read; not specific to midair). The prewar damage readout on the status bar is shown in midair
only when the victim is on the ground (read).

## 4. Knockback

The blast's push is computed from the rocket's damage before any midair rule zeroes it, so a
hit that does no damage still pushes (read). Against the classic push (a factor of 8 times the
damage along the line from the blast to the target) the mode changes three things (read):
- When the damage is under 60 (a weak splash) and the target is not the shooter, the factor is 11
  instead of 8 (read). A direct hit (110) keeps 8 (derived).
- When the target is at 45 u or more above the floor, the whole push is multiplied by
  1 + (height - 45) / 64. At 109 u it doubles and at 173 u it triples (read for the rule,
  derived for the examples). This applies to the shooter's own rocket jumps too (read).
- When the target is below 45 u, an extra upward-only push is added: the vertical part of the
  direction times the damage times 6 for a weak splash on another player, or times 4 otherwise
  (read). So a grounded player is thrown into the air harder, and a self rocket jump from the
  floor gets this extra lift too (derived).

## 5. Scoring

- A kill by other means gives the killer no frag in midair: the ordinary +1 frag for killing
  another player is suppressed (read). This covers telefrags too (derived: a telefrag goes
  through the same ordinary-kill path).
- Every rocket hit that counts (question 3) on another player during a match gives the shooter
  points by the band of a second height (read): the victim's origin height minus the shooter's
  origin height at the moment the rocket was fired (read). This is not the trace height of
  question 3.

  | band | height difference | frags |
  | --- | --- | --- |
  | platinum | more than 1024 u | 8 |
  | gold | more than 512 u | 4 |
  | silver | more than 256 u | 2 |
  | bronze | anything else, including negative | 1 |

  (read). The hit must also have done health damage after the usual checks, so a hit on a
  teammate under teamplay 1 or 3, or on a god-mode player, scores nothing (read).
- Whether the server engine keeps the rocket's stored firing position untouched while it flies
  is outside the mod (open); the mod writes it once, at the shot (read).
- Minimum height for a kill to count: the minimum height of question 3, measured by the trace
  (read). There is no further bonus for a direct hit, distance or a spawn kill (read).
- Head stomp: landing on another player while falling faster than 650 u/s (1000 u/s on q1dm17)
  kills the player underneath outright in a match (read for the speeds and the kill). The victim
  loses 3 frags; the stomper gets no frag (the ordinary +1 is suppressed) but a stomp is counted
  in the statistics (read).
- Suicides and deaths to the world keep the mod's usual penalties: -1 for a world death, -2 for
  the suicide command, -1 for killing yourself otherwise (read; not specific to midair).
- No score is given in prewar (read).

## 6. Messages

- Each scoring hit prints one console line to everyone (read), in this order: a band icon (a
  single coloured character from the upper half of the console character set, different per band:
  code 137 for platinum, 136 for gold, 132 for silver, 139 for bronze), then the band name followed
  by the word "midair", padded to 15 characters, then the height difference with one decimal in a
  7-wide field inside square brackets, then "by" and the shooter's name, then "on" and the victim's
  name (read). The band name and the height are drawn with the console's digit-styled characters
  (the number glyphs) (read).
- The ordinary obituary line ("rides X's rocket" and the like) is not printed for kills in
  midair; the midair line takes its place (read). Obituaries outside the ordinary-kill path
  (deaths to the world, suicides, team kills) are printed as usual (read).
- No centre print and no sound are tied to a midair hit (read).
- Setting the minimum height (question 10) prints one console line to everyone saying the midair
  minimum height is set to the named level ("ground", "bronze", "silver", "gold" or "platinum")
  (read).

## 7. Statistics

Kept per player for the match (read): the count of scoring hits in total and per band (bronze,
silver, gold, platinum), head stomps, bonuses (bonus powers taken from packs, question 8), the
highest height difference scored, the sum of heights and the average height, plus the mod's
usual streak, spawn frags and rocket-launcher hits and attacks.

At the match's end, in midair the usual player statistics are replaced by (read):
- per player: name and frags, total midairs with bronze, silver, gold and platinum under it,
  stomps, streak, spawn frags, bonuses, max height and average height (one decimal), and
  rocket-launcher efficiency (hits over shots, as a percentage with one decimal),
- then a top performers list in place of the usual one: score, kills (midairs plus stomps),
  midairs, head stomps, streak, spawn frags, "bonus fiend", highest kill (height) and average
  height, each naming the leader(s) and the value.

The `laststats` command (any player or spectator, also in matchless) shows tables of the last
match (read): a midair table with columns Tot, Bro, Sil, Gol, Pla, Sto, Bon, Max H, Avg H; a
kill table with Frag, SpwnF (spawn frags), Strk (streak) and RL % (rocket efficiency); and the
top performers.

The statistics files carry a midair entry per player (read): the XML form has the attributes
stomps, bronze, silver, gold, platinum, total, bonus, totalheight, maxheight and avgheight; the
JSON form has an object `midair` with `stomps`, `midairs` (holding `bronze`, `silver`, `gold`,
`platinum`), `total` and `bonus`, each only when not zero.

## 8. Other rule changes

- Falling damage: none (read for the drop of non-listed damage; derived for falls).
- Lava, slime and drowning: normal damage (read). Telefrags: normal, they kill, but give no frag
  (questions 3 and 5).
- Respawn: a dead player is respawned automatically after 2 s instead of 5 s (read).
- Spawn protection: off (question 2).
- Powerups: none on the map (question 2). The `powerups` command in prewar is refused with a
  line to everyone that Powerups are disabled with Midair (read).
- Backpacks: deathmatch 4's pack rule stands, +10 health per pack (read). Reaching 300 health
  gives "bonus powers" (a line to everyone naming the player and a sound), which in midair is a
  quad for 30 s only, without the pentagram that deathmatch 4 otherwise adds; it counts as a bonus
  in the statistics (read). A player with an active quad ignores packs (read).
- Quad in midair: rockets fly at 2000 u/s instead of 1000 u/s and glow blue; damage is not
  multiplied (read).
- Team damage: the teamplay setting's usual rules (read; not changed by the mode).
- Weapons: only the axe and rocket launcher are held; the axe does no damage to players (derived,
  question 3). The "Noweapon" line of the countdown's settings list is hidden in midair (read).
- Match end: the mode sets no fraglimit or timelimit (read). The shipped per-map configs that
  turn it on set timelimit 3 and `k_overtime 2` (sudden death) (read).

## 9. Maps

- The mode changes no gravity and no speed (read: nothing in it touches them).
- The shipped example server configs include per-map usermode files for the maps endif and
  nacmidair; each sets timelimit 3, deathmatch 4, `k_overtime 2` and `k_midair 1` (read). The mod
  runs such a file (named after the map) whenever a usermode is applied on that map, after the
  usermode's own settings (read), so on those maps loading any usermode turns midair on.
- No map entity is changed beyond the item strip at match start (read).
- The stomp speed threshold is 1000 u/s on q1dm17 and 650 u/s elsewhere (read; not specific to
  midair).
- On povdmm4 the mod's bots stop seeking items in midair (read; bots only).

## 10. Variants and sub-options

- There is no instagib-like variant inside midair (read): instagib is a separate deathmatch-4
  sub-mode, and the two exclude each other (question 1).
- There is no "damage only" option: a hit that counts always kills (read).
- The one sub-option is `k_midair_minheight` (default 1, so 128 u; values 0 to 4, question 3).
  Its command `midair_minheight` (players and admin spectators, prewar only) cycles it upward,
  4 wrapping to 0, and prints the new level to everyone (read). It is refused, with a line to the
  caller that midair must be turned on to set the minimal frag height, while midair is off (read).
  Its help text is "midair minimum frag height" (read).
- `dmm4_invinc_time` is ignored in midair (read).
