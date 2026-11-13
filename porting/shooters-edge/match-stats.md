# Match stats: how the classic match mod counts them and what it shows

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of how the
classic match mod (KTX) makes each per-player match statistic and what the players are shown when
a match ends and on demand during play. For our own server's end-of-match report (M59) and the
in-game stats commands. No code, no internal identifiers.

**Where each fact came from.** A research session (2026-11-13) read KTX at commit `b1b23a9`
(`b1b23a985dc2f1452ea2a6286760deada739de1b`, github.com/QW-Group). Each fact is marked **read**
(the behaviour as read), **derived** (follows from read facts put together) or **open** (the
source does not settle it). `accounts.md` § C holds the stats file's fields, `lastscores` and when
a match counts; `midair.md` holds the midair mode. Neither is repeated here.

Terms: *in a match* means after the countdown, while the game runs; *prewar* is the warm-up.
Units are map units (u), seconds (s), health and armour points. *Red text* is the console's
alternate (high-bit) character set; *gold brackets* are the console's square-bracket glyphs
(codes 144 and 145); *the rule* is a horizontal line drawn with the glyphs 157 (left end),
158 (middle) and 159 (right end). A *ghost* is a player who disconnected during the match; his
record is kept until the match ends.

**All per-player counters are cleared when a match starts** (the end of the countdown), except
the handicap (read). Counters that move in prewar (attacks, hits, damage) are therefore wiped
before the match (derived).

---

## 1. What everyone sees when a match ends

All of the end-of-match report is broadcast as high-priority console prints to every client
(players and spectators, and so into the server demo) (read). Nothing of it is a centre print
(read). Nothing is printed to one player alone at that moment except hidden lines a client does not
show (read; the `//finalscores` line is in `accounts.md` § 13).

**Order of the blocks** (read):
1. `The match is over` (or `The point is over` in hoonymode, where the report comes only after the
   final point).
2. `Demo markers:` with the rule, one line `Time: m:ss [<marker name>]` per marker, the rule,
   when any marker was set (not in race).
3. `Player statistics:` (red) and a 38-glyph rule.
4. Except in midair, a legend line: `Frags (rank) · efficiency`, with `friendkills ` inserted
   before the dot in team modes and CTF (the dot is glyph 143).
5. The per-player blocks, grouped by team (teams in the order their first member is met,
   players in slot order, ghosts included). In team modes and CTF each team starts with
   `Team [<team>]:` (gold brackets). A blank line follows each team's group (derived: printed when
   the search for mates reaches the end).
6. Team modes and CTF only, not midair: the team summary (below).
7. Not duel, not midair: the top lists (§ 2). Midair prints its own list instead (`midair.md` § 7).
8. Team modes and CTF only: the team scores.
9. When any ghost exists: a blank line, `<glyph 131> - disconnected player` (red word), a blank
   line.

Race prints its own race report instead of 3 to 9; clan arena prints none of this at match end
(read).

**One player's block, normal modes** (read). Names of ghosts carry glyph 131 in front.
- Team modes and CTF: a short rule of 10 glyphs comes BEFORE each player. Duel and FFA: the short
  rule comes AFTER each player.
- `<glyph 135> <name>:`
- `  <frags> (<rank>) <friendkills> <efficiency>%`. Rank is frags minus deaths. Friendkills (and
  its trailing space) only in team modes and CTF. Efficiency has one decimal (§ 3). In CTF, frags
  here are the score minus capture points.
- `Wp:` (red) followed, each only when it applies, in this order:
  - `lg<pct>% (<hits>/<attacks>)` when any lightning attack was made; pct one decimal.
  - `rl<pct>% (<virtual hits>/<rockets fired>)` when the percentage is not zero.
  - `gl<pct>%` (virtual hits over grenades fired) when not zero; no counts.
  - `sg<pct>% (<shotgun damage to enemies>)` when not zero.
  - `ssg<pct>% (<super shotgun damage to enemies>)` when not zero.
  The weapon label is red and the number follows it with no space or colon.
- `RL skill: ad:<x> dh:<n>`: `ad` is the average rocket damage per virtual rocket hit, one
  decimal (§ 4); `dh` is the count of direct rocket hits. Not shown in lightning-gun challenge mode.
- Duel only: `   Speed: max:<x> average:<x>`, one decimal each (§ 10).
- Only for human players and when the setting `k_socd` is 1 or more (unset reads as 0, derived):
  `Movement: Perfect strafes:<pct>% (<perfect>/<changes>) SOCD detections:<d>/<v>`, then
  `. SOCD movement assistance detected!` when flagged, then the detector's version in square
  brackets.
- `Armr&mhs: ga:<n> ya:<n> ra:<n> mh:<n>`: pickups of green, yellow, red armour and mega health.
  Not in lightning-gun challenge mode.
- Team modes and CTF: `Powerups: Q:<n> P:<n> R:<n>` (quad, pentagram, ring pickups).
- CTF: `RuneTime: res:<p>% str:<p>% hst:<p>% rgn:<p>%` when runes are on (whole percent of match
  time each rune was held, truncated); `     CTF: pickups:<n> caps:<n> returns:<n>`,
  ` Defends: flag:<n> carrier:<n>`.
- Team mode (not CTF): `      RL: Took:<n> Killed:<n> Dropped:<n>`, then ` Xfer:<n>` only when not
  zero; the same line for `      LG`. Took is pickups of the weapon when not already owned; Killed
  is enemies killed who carried that weapon (§ 7); Dropped is deaths whose backpack held it; Xfer
  is in `accounts.md` § 12.
- `  Damage: Tkn:<n> Gvn:<n> EWep:<n> Tm:<n> Self:<n> ToDie:<n>`, whole numbers. `EWep` appears
  only in team mode with deathmatch 1. `ToDie` is damage taken divided by deaths, truncated,
  99999 with no deaths (§ 6).
- When the match lasted more than 0 s: duel `    Time: Control:<s> (<p>%)` (whole seconds held,
  and that time as a truncated whole percent of the match's length, § 9); other modes
  `    Time: Quad:<s>` (whole seconds holding quad, § 8).
- Duel: ` EndGame: H:<health> A:<armour>` where armour is `0` or the armour kind (`ga`, `ya`,
  `ra`) followed directly by the value; with overtime also ` OverTime: H:<h> A:<a>`, the health and
  armour each player had when overtime began.
  Other modes: ` Streaks: Frags:<n> QuadRun:<n>` (§ 9).
- Not CTF, not lightning-gun challenge: `  SpawnFrags: <n>` (§ 9).
- Lightning-gun challenge mode, when the player fired the gun: `  LGC Score : [<score>]` (lightning
  accuracy in percent times frags, truncated), `  Overshaft : <p>% (<n>/<attacks>)`,
  `  Undershaft: <p>% (<n>/<attacks>)`, one decimal (§ 11).

**Number formats** (read): `%`-values with one decimal are rounded to one decimal by the print
routine. Damage and the team summary's percentages are printed with no decimals, rounded to the
nearest whole number. Values marked truncated above (ToDie, control seconds and percent, quad
seconds, rune percent) drop their fraction instead.

**Duel, FFA and team differ** (read): duel has no friendkills, no powerups line, no RL/LG lines,
no `EWep`, shows Speed, Control, EndGame and OverTime instead of Quad time and Streaks, and has no
top lists. FFA shows Streaks and Quad time, no powerups line, and the top lists. Team mode shows
everything above plus the team blocks.

**Instagib's player block** replaces the normal one (read): `<glyph 135> PLAYER: <name> <glyph 135>`,
then gold-bracketed headings each followed by red-labelled lines: `SCORES` (Efficiency one
decimal without a percent sign, Points, Frags as coilgun plus axe plus stomp gibs, Teamkills in
team modes, Deaths, Streaks, Spawns); `SPEED` (Maximum, Average, one decimal); `WEAPONS` (Coilgun
and Axe, each `<pct>% (<gibs>)` or `n/u` when never used); `GIBS` (Coilgun, Axe, Stomps),
`MULTIGIBS` (Total Multigibs, Maximum Victims); `AIRGIBS` (Total, Total Height, Maximum Height,
Average Height one decimal). The long rule follows each player outside team modes.

**The team summary** (team modes and CTF, not midair) (read):
- The rule, `[A] vs [B] ... match statistics:` (team names in gold brackets joined by ` vs `),
  the rule, a blank line, the red heading `weapons, powerups, armors&mhs, damage`, the rule.
- Per team, the sums over its players and ghosts:
  - `[<team>]: Wp:` then `lg<pct>%`, `rl<n>`, `gl<n>`, `sg<pct>%`, `ssg<pct>%`, each only when not
   zero. Percentages have no decimals. For `rl` and `gl` the number is the count of DIRECT hits,
   not a percentage. Instagib shows only `cg<pct>%`.
  - `Powerups: Q:<n> P:<n> R:<n>` and `Armr&mhs: ga ya ra mh` (not in instagib).
  - CTF: rune time percent of match time, `CTF` and `Defends` lines.
  - `RL: Took Killed Dropped Xfer` and `LG: ...`, Xfer always shown here.
  - `Damage: Tkn Gvn Tm`, with `EWep` between Gvn and Tm in deathmatch 1.
  - `Time: Quad:<s>`.
- The rule.

**Team scores** (team modes and CTF) (read):
- A blank line, `Team scores: frags · percentage` (red), the rule.
- Per team: `[<team>]: <frags>`; when ghosts of that team scored, ` + (<ghost frags>) = <total>`,
  then ` · <pct>%` with one decimal.
- The percentage is the team's total over the sum of every team's total, where a negative team
  total counts as 0 in the sum; 0.0 when the sum is not positive.
- The rule.

## 2. The top lists

Printed to everyone after the player blocks, in FFA, team modes and CTF; not in duel, midair,
race or clan arena (read). Heading: `[<map>] top scorers:` (map in gold brackets, words red), the
rule. Each list is one line `<label>: <name> [<value>]` (value in gold brackets); every further
player tied on the best value gets a line of its own indented by 13 spaces (read). Ties list ALL
tied players, in slot order, ghosts included and marked (read). There is no cap on entries.

| label (as printed, right-aligned to 11) | ranks by | shown when |
| --- | --- | --- |
| `Frags` | score (CTF: score minus capture points) | always |
| `Deaths` | deaths | always; with nobody dead, every player is listed with 0 |
| `Friendkills` | team kills | best value above 0 |
| `Efficiency` | efficiency (§ 3), one decimal with `%` | always |
| `FragStreak` | longest kill streak (§ 9) | best above 0 |
| `QuadRun` | longest quad streak | best above 0 |
| `RL Killer` | enemies killed who carried a rocket launcher | best above 0 and deathmatch 1 |
| `Boomsticker` | shotgun accuracy; shows `<pct>% (<shotgun enemy damage>)` | best above 0 and deathmatch 1 |
| `Survivor` | damage taken per death, truncated | best above 0 (see below) |
| `Annihilator` | damage given to enemies | best above 0 |
| `Captures` | flag captures | CTF, best above 0 |
| `FlagDefends` | flag defends | CTF, best above 0 |

(read for every row.) `Survivor`: the best value is taken over players' damage taken divided by
deaths, but a player with no deaths divides by zero there, while the comparison that picks whom
to name uses 99999 for him (read). So a deathless player is in practice never named, and the
outcome for him is not defined by the source (derived; open what a given platform prints). In
instagib the per-player pass does not feed `Survivor`, so it never shows there (derived), and
`Boomsticker` ranks coilgun accuracy (read).

The `laststats` command (§ 12) prints a different list, `Top performers`, to the asker only:
`Frags`, `Deaths`, `Efficiency` (whole percent, truncated), `Fragstreak`, `Annihilator` and
`Survivor` always; `Friendkills`, `Quadrun` and `Spawnfrags` only when the best is above 0; in
CTF `Captures` and `FlagDefends` always (read). It has no RL Killer or Boomsticker (read). Format
`   <label padded to 13>: <name> (<value>)`; tied players follow with a blank label (read).

## 3. Efficiency

- Normal modes: score divided by (score plus deaths), times 100; 0 when the score is below 1
  (read). The score is the frag count with its penalties (suicides, team kills), not the kill
  count (derived).
- CTF: the same with the score minus capture points (read).
- Rocket arena: wins over (wins plus losses), times 100; 0 with no rounds (read).
- It is computed at match end for the report, and on demand by the `stats` command (§ 12) (read).
- Shown: in each player's second line (one decimal and `%`), in the `Efficiency` top list, in the
  `stats` and `effi` commands' column, in `laststats` (`Effic` column: no decimals at 100% or
  more, one decimal otherwise, with a leading space for exactly 0) (read). Computed but not shown
  in midair (read).

## 4. Weapon accuracy

**An attack** (read):
- Axe: one per swing.
- Shotgun: one per pellet, 6 per shot. Super shotgun: one per pellet, 14 per shot (21 in the yawn
  mode). A super shotgun with exactly one shell fires as the shotgun and counts there.
- Instagib coilgun (the shotgun slot): one per shot.
- Nailgun and super nailgun: one per nail fired, one nail per 0.1 s while the trigger is held
  (read). A super nailgun with fewer than 2 nails fires a nailgun nail, counted for the nailgun.
- Grenade launcher, rocket launcher: one per projectile.
- Lightning gun: one per beam pulse, that is once per 0.1 s while the trigger is held (read). A
  water discharge counts no attack (read).

**A hit** (read):
- Axe, shotgun pellets, lightning pulses: the attack's trace stops on an entity that can take
  damage AND is a player. One per pellet; one per pulse.
- Nails, rockets, grenades: the projectile touches an entity that can take damage and is a
  player (a direct impact). A grenade explodes on such a touch. Splash never adds to this count.
- Team mates count as hits (read). The shooter himself never counts: projectiles do not touch
  their owner and traces skip him (read; derived that no self hit is possible).
- Monsters and shootable doors or buttons never count (read: only players).
- A dead player is not solid until he respawns, so traces and missiles pass through his body and
  cannot score a hit (derived from the death making him non-solid, read).
- A hit counts whatever happens to the damage afterwards (pentagram, team protection) (read).
- Instagib coilgun: one shot can pass through up to 32 entities; the third and later entities
  struck by one shot have their hit taken back, so one shot counts at most two hits (derived from
  the correction read; open whether the first trace step can ever be a non-player).

**Rocket and grenade launcher: three hit counts** (read).
- *Direct hits* (`hits`): the impacts above. Shown as `dh` on the RL skill line, as `rl`/`gl` in
  the team summary and in `+wp_stats`.
- *Real hits*: for each damage event on an ENEMY player (any player but oneself in FFA and duel)
  from a rocket or grenade, direct or splash, that took health or armour.
- *Virtual hits*: the same events where the damage would have taken health or armour before the
  pentagram, god mode or team-protection checks zero it (so a hit on a pentagram holder counts
  virtually). One rocket that damages three enemies by splash makes three virtual hits (derived).
- Self and team-mate damage never count as real or virtual hits (read).
- The shown `rl` and `gl` percentages (player block, `laststats`, `midair`) are virtual hits over
  projectiles fired, times 100; a percentage above 100 is possible with multi-enemy splash
  (derived).
- `ad` (average damage) on the RL skill line is the sum, over virtual rocket hits, of the armour
  absorbed plus the health damage before the protection checks AND before the victim's
  remaining-health cap, divided by the virtual rocket hits (read). So `ad` counts overkill while
  damage given does not (derived).
- Other weapons: the shown percentage is hits over attacks, times 100 (read).

## 5. Mid-air outside the midair mode

- In a normal match nothing counts rockets on airborne players: there is no such counter (read).
- Instagib counts *air gibs* (read): when a player is killed by another player and the KILLER is
  standing on the ground (the engine's on-ground flag), the victim's height is measured by a
  trace straight down from his origin, 2048 u long, through players and monsters, as the bottom of
  his box minus the trace's end, plus 1 u. A height above 45 u counts an air gib and adds the
  height (whole units) to the total and the maximum. Heights of 250 u to under 400 u print
  `AirGib from <killer>: height <n>`, 400 u to under 1000 u `Great AirGib ...`, 1000 u and up
  `Amazing AirGib ...` (red label) to everyone.
- The midair mode's bands, points and bonuses are in `midair.md` §§ 3, 5 and 8.

## 6. Damage given, taken, team and self

Counted for every damage event where both the attacker and the victim are players (read).
- **The amount** is the armour the hit absorbed plus the health it took, after the quad multiplier
  (4, or 8 in deathmatch 4), the CTF runes and the handicap (read).
- Armour absorbed: the armour's protection fraction times the damage, rounded up, capped at the
  armour left (read).
- Health taken: the damage minus the absorbed part, rounded up, then zeroed by god mode, the
  pentagram, or team protection (teamplay 1 for mates and oneself, teamplay 3 for mates only,
  teamplay 4 for mates), and then CAPPED at the victim's health before the hit (read). So a gib's
  overkill is not counted (derived).
- So a hit on a pentagram holder counts only the armour he lost; a team hit in teamplay 1 or 3
  counts only the mate's armour (derived; teamplay 4 does not touch armour at all, read).
- With the damage-frags setting `k_dmgfrags` on: the health part is taken before the protection
  checks, still capped at health except against a pentagram holder, and telefrags count nothing
  (read).
- **Which counter**: oneself is *self*; a player on the attacker's team, when teamplay is on, is
  *team*; anyone else is *given* for the attacker and *taken* for the victim (read). Taken counts
  only damage from enemies: world, self and team damage are not in it (derived).
- *EWep*: the part of given damage dealt to enemies carrying a rocket launcher or lightning gun in
  their inventory (not necessarily in hand) (read).
- Per weapon: the given and team damage are also booked to the weapon by the kind of damage (axe,
  shotgun, super shotgun, nails, grenade, rocket, lightning beam or discharge) (read).
- Prewar damage is counted but wiped at match start (derived).

## 7. Kills per weapon

- Each damage by a weapon stamps the victim with that kind of damage; the death is credited to
  the kind on him when health reached zero, so the final blow decides (read).
- When the killer is a player (read):
  - killing oneself: a suicide for that weapon,
  - a team mate in team mode or CTF: a team kill for that weapon,
  - otherwise: a kill for that weapon, a death to that weapon for the victim, and an *enemy-weapon
   kill* for EVERY weapon the victim carried (axe and shotgun included).
- Lightning beam, discharge into water on others and one's own discharge all credit the lightning
  gun (read).
- Telefrags, head stomps, squishing and all world deaths credit no weapon (read).
- The overall counters (read): every death adds one to the victim's deaths; a team kill adds one
  to the killer's friendkills; a death to oneself adds one to one's suicides; a death to the world
  books the "suicide" on the world, so the victim's suicide count does not move (derived); a kill
  of another player adds one to the killer's kills.
- A plain telefrag is an ordinary kill: +1 frag and a kill, no weapon (read). The special telefrag
  outcomes (against a pentagram holder, a double pentagram telefrag, the spawn-point penalty)
  take a frag from the victim instead, after the killer's kill count has already moved (derived).
- Kills count only in a match, never in prewar (read).

## 8. Item pickups and time held

- **Counted** (read), only in a match: mega health, the 15 and 25 health boxes (only when they
  actually heal), green, yellow and red armour (only when accepted: its protection times value
  must beat the armour worn), quad, pentagram and ring (each pickup, dropped ones included).
  The biosuit is not counted (read).
- **Weapons**: *took* is a pickup of a weapon not already owned; a separate total counts every
  pickup; spawn-only variants leave out backpacks; *dropped* is a death whose backpack held that
  weapon (read).
- **Time held** (read): the clock starts at pickup and stops (adding to the total) when:
  - quad, pentagram or ring runs out, the holder dies, disconnects, or the match ends,
  - an armour is replaced by a pickup of another kind, the holder dies, disconnects, or the match
   ends.
  A second pickup of the same thing restarts the clock after banking the time so far, so the time
  stays continuous (derived). Armour shot down to zero keeps its clock running until one of the
  events above (derived). Health and mega health have no time (read).
- **Weapons' time**: from a pickup of a weapon not owned until death, disconnect or match end
  (read).
- Shown: quad seconds in the player block and team summary; mm:ss in the `laststats` tables `Item
  times` (RA, YA, GA, and in team modes Quad, Pent, Ring) and `Weapon times` (read).

## 9. Spawn frags, streaks, quad runs, control

- **Spawn frag**: an ordinary kill of an enemy within 2 s of the victim's last spawn (read). Team
  kills, suicides and world deaths never count (read).
- **Streak**: each death the killer causes adds one to his current run when the two are on
  different teams or teamplay is off (read). Every death of a player, any cause, closes his run
  into his best and sets it to 0 (read). In duel and FFA a suicide first adds one to the suicider's
  own run, so his best can be one higher than his kills (derived). The match end closes every
  open run (read).
- **Quad run**: the same, counted only while the killer's quad is active (read). Picking up a quad
  closes the current quad run into the best and starts a new one; the run is not closed when the
  quad expires, only at death, the next quad or the match end (read).
- **Control** (duel only): starts when a kill brings the killer's current run to 2 or more and
  he does not already hold it; stops at his death, disconnect or the match end (read).

## 10. Speed

- Horizontal speed in u/s (the length of the velocity's x and y), sampled once in every pass of
  the player's end-of-move think while he is alive and the match is running (read).
- Max is the highest sample; average is the sum over the number of samples (read).
- How often that think runs (per server frame or per client move) is the server engine's
  business (open). Shown with one decimal in duel's player block and in instagib (read).

## 11. Lightning gun

- Accuracy is per pulse: one attack per 0.1 s pulse, one hit per pulse whose single trace (600 u
  from 16 u above the origin) stops on a player (read).
- No distance buckets in a normal match (read).
- In the lightning-gun challenge mode (`k_lgcmode` non-zero), and only in duel, hits and misses are
  also counted in 20 distance buckets of 35 u up to 700 u (farther goes in the last) (read).
  A hit's distance is from the beam's start to the victim; a miss's is to the opponent (read).
  Missed pulses before a player's first hit of a burst count as *undershaft*; misses after a kill
  until the trigger is released count as *overshaft* (read).
- `lglf` in `+wp_stats` (§ 12) is the lightning accuracy of the burst that last made a kill; it only
  updates in the challenge mode, so in a normal match it reads 0.0 (derived).

## 12. On demand during play

- **`scores`** (players and spectators): time left or frags left and the team scores (read).
  **`+scores`/`-scores`**: the same as a centre print refreshed every 0.8 s (read).
- **`stats`** and **`effi`** (both the same): only in a match; otherwise `no game - no statistics`
  (read). Prints to the asker `Player statistics:`, the legend `Frags rank [friendkills] · efficiency`,
  a rule, then one line per player grouped by team: in team modes and CTF the team in gold
  brackets cut to 4 characters, the name cut to 10 characters, frags, rank (frags minus deaths),
  friendkills in team mode, efficiency with one decimal and `%` (read). Ghosts are not listed
  (read). Rocket arena prints its own table instead (read).
- **`+wp_stats`/`-wp_stats`**: a centre print of the player's own weapon stats (a spectator's
  tracked player's), refreshed every 0.3 s, in prewar and in a match, not in the countdown or race
  (read). Shown: `lg:<pct>` (one decimal), `lglf:<pct>`, `rl:<direct hits>`, `gl:<direct hits>`,
  `axe:<hits>`, `sg`, `ssg`, `ng`, `sng` percentages (one decimal), in that order, labels red, no
  `%` sign (read). The userinfo key `wps` picks them as a bit mask: axe 1, sg 2, ssg 4, ng 8,
  sng 16, gl 32, rl 64, lg 128, lglf 256; unset means gl, rl and lg (read). `lg` and `rl` show
  even at zero once selected (derived). Lines wrap at 40 characters (read). The key `lw` adds
  that many blank lines above (negative: below), `lw_x` that many spaces left (negative: right), and
  `ktpl` puts everything on one line and adds 12 blank lines above (read). A spectator tracking
  nobody sees `Tracking no one (+wp_stats)` (read).
- **`wp_reset`**: players only, prewar only; clears one's own weapon counters (read).
- **`laststats`** (players and spectators): the last match's report as tables, to the asker
  only; refused during the countdown or a match with `Game in progress`, and before any match
  ended with `Laststats data empty` (read). Tables, each a red heading with `|`-separated column
  labels and a rule, then one row per player (name padded to 20) (read):
  - normal modes: `Kill Statistics` (Frag, Kill, Death, Suici, TKill, SpwnF, Effic); `Item Taken`
   (GA, YA, RA, MH, Quad, Pent, Ring); `Weapon Efficiency` (LG%, RL%, GL%, SNG%, NG%, SSG%,
   SG%; `-` when unused, RL and GL by virtual hits); `Weapon Damage` (enemy damage per weapon,
   the same seven); `Weapon Taken`, `Weapon Dropped`, `Weapon Kills`, `Enemy Weapon Killed` (LG,
   RL, GL, SNG, NG, SSG); `Damage statistics` (Taken, Given, EWeap, Team, Self, ToDie),
   `Item times`; `Weapon times`; CTF adds `CTF statistics` (Fla, Cap, Ret, DFl, DCa, Res, Str,
   Hst, Rgn); team modes add `Team Items Taken` (RA, YA, GA, MH, Q, P, R), `Team Weapons` (LGT,
   LGK, LGD, LGX, RLT, RLK, RLD, RLX: took, enemy-weapon kills, dropped, transferred), `Team
   Frags+Damage` (Frags, Given, Taken, EWep, Team) and in CTF `Team CTF statistics`; then `Top
   performers` (not duel, § 2) and the ghost legend. Quad, Pent and Ring columns read `-` outside
   team modes.
  - instagib: `Instagib statistics` (Coil, Axe, Stmp, Mult, MMax, Air, TotH, MaxH, Ring) and
   `Kill statistics` (Point, Frag, Death, SpwnF, Strk, CG %). The CG % column prints the coilgun
   HIT COUNT with a `%` sign, not a percentage (read).
  - lightning-gun challenge: `LGC statistics` (Score, Over, Under, LG %).
  - midair: `midair.md` § 7.
  Defects (read): in CTF the Death column shows capture points minus deaths. The tables read the
  live player records, so players who left after the match are missing and ghosts are gone
  (derived: ghosts are removed at the match end). The command also re-runs the stats file writer of
  `accounts.md` § 12 (read), which writes only while a server demo is named (open whether the name
  is still set after the match).
- **Hidden lines for client HUDs** (read): a player or spectator whose userinfo has `wpsx` set
  gets, whenever a weapon's counts change, a hidden line `//wps <player number> <weapon> <attacks>
  <hits>` (weapons named `axe`, `sg`, `ssg`, `ng`, `sng`, `gl`, `rl`, `lg`), also written into the
  demo; a spectator gets all weapons of a newly tracked player.
