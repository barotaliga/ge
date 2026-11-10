# Autotrack, specified

How the classic network game's programs pick "the best player" to watch in a recorded multiview
demo (MVD) or as a spectator, and switch between players on their own, as facts in our words
(README.md, the rules). It is for the session that makes Shooter's Edge's demo-to-video camera
follow the hottest player in a 4on4 team match; that session reads this note, never the source.

**Where each fact came from.** A research session (2026-11-06) read the current sources of
three programs: the community client **ezQuake** (its MVD autotrack, its MVD statistics tracker,
its parse of server hints), the match mod **KTX** (its spectator autotrack commands and the
hints it writes into the demo) and the server **mvdsv** (how a mod's demo-only text and hidden
data reach the MVD, and the per-player stats it records). Facts marked `verify` are checked by a
lab on a real 4on4 MVD before code depends on them.

## 1. The variants

| Program | Switch | What it is for |
| --- | --- | --- |
| ezQuake | cvar `mvd_autotrack` 1 | Client side, on an MVD or a QTV stream. A rating from a formula picked by match size (1on1, 2on2, 4on4) |
| ezQuake | `mvd_autotrack` 2 | The same with the `mvd_autotrack_custom` formula, whatever the size |
| ezQuake | `mvd_autotrack` 3 | With the multiview display on: one formula per view (`mvd_multitrack_1..4`), each view tracks its own best |
| ezQuake | `mvd_autotrack` 4 | A fixed ranking by powerups and big weapons; no formula, no frags |
| ezQuake | `mvd_autotrack_instant`, `mvd_autotrack_lockteam` | Switch whenever a better player exists; stay within one team |
| ezQuake | `demo_autotrack` | Follow the server's own choice recorded in the demo (§ 4) |
| ezQuake | `cl_hightrack` | Track the top fragger (most frags; a locked view moves only to someone with strictly more) |
| ezQuake | command `autotrack` | Toggles: on an MVD it turns on both `mvd_autotrack 4` and `demo_autotrack 1` (a hint found turns the client side off); live, it runs the server's `autotrack` alias, else falls back to `cl_hightrack` |
| KTX | spectator command `autotrack` | Event-driven: re-pick only when one of four events happens (§ 3). The same rule writes the demo hints |
| KTX | spectator command `autotrackktx` | Continuous: the KTX best rating re-checked every spectator frame |
| KTX | spectator command `auto_pow` | Continuous, by powerups only |
| KTX | spectator command `next_best` | One step: go to the best, or to the second best when already on the best |
| mvdsv | none of its own | It carries the mod's demo-only text and hidden blocks into the MVD (§ 4) |

KTX keeps a spectator's mode in his userinfo key `*at` across level changes.

**Best fit for a 4on4 video**: ezQuake's `mvd_autotrack 1` with the 4on4 values is the only
variant that weighs armour and frags together with weapons and powerups, and it runs on the demo
alone; KTX's event-driven rule supplies the switching discipline worth keeping (wait after a
death, switch on powerup events). Neither knows streaks; § 6 adds them.

## 2. The ratings

### ezQuake, modes 1 to 3: a formula over per-player variables

Every mode-1/2/3 rating is an expression held in a cvar, evaluated per player, plus a cvar of
14 numbers (weights) the variables read. The default expression is the same in all four sets:

    rating = armour_amount x armour_weight + 50 x weapon_weight + powerup_weight + frags

- **armour_weight**: by the armour bit in the items stat: green 1st value checked, then yellow,
  then red (normally only one is set); none gives 0.
- **weapon_weight**: the weight of the FIRST weapon the player holds in the order rocket
  launcher, lightning gun, grenade launcher, super nailgun, nailgun, super shotgun, shotgun, axe
  (the client's weapon-order setting, default `78654321`, then that same order as a fallback).
  Only the item bits count; ammo is ignored. It is truncated to an integer.
- **powerup_weight**: the SUM of ring, quad and pentagram weights for each one held (item bits).
- **frags**: the scoreboard frags.
- Dead players are NOT excluded: a dead player is rated like a living one from his stats.

The 14 weights, in the order the cvar lists them:

| Set (cvar `mvd_autotrack_<set>_values`) | axe | sg | ssg | ng | sng | gl | rl | lg | ga | ya | ra | ring | quad | pent |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `1on1` | 1 | 2 | 3 | 2 | 3 | 5 | 8 | 8 | 1 | 2 | 3 | 0 | 0 | 0 |
| `2on2` | 1 | 2 | 3 | 2 | 3 | 5 | 8 | 8 | 1 | 2 | 3 | 500 | 900 | 1000 |
| `4on4` | 1 | 2 | 4 | 2 | 4 | 6 | 10 | 10 | 1 | 2 | 3 | 500 | 900 | 1000 |
| `custom` | 1 | 2 | 3 | 2 | 3 | 4 | 6 | 6 | 1 | 2 | 3 | 500 | 900 | 1000 |
| `mvd_multitrack_1..4` (mode 3) | 1 | 2 | 3 | 2 | 3 | 5 | 8 | 8 | 1 | 2 | 3 | 0 | 0 | 0 |

The multitrack expressions default to frags alone, the weapon weight alone, health alone and
the armour weight alone for views 1 to 4. A values string that does not hold exactly 14 numbers,
or an expression that fails to evaluate, turns autotrack off with a console message.

**The set by match size**: at match start the client counts named non-spectator players: 2 picks
`1on1`, 4 picks `2on2`, 8 picks `4on4`, any other count (6, 7, 10 ...) also picks `4on4`.

**Worked 4on4 numbers**: red armour 200, RL and quad, 15 frags: 200x3 + 50x10 + 900 + 15 = 2015.
Yellow 100, LG, 30 frags: 200 + 500 + 30 = 730. Fresh spawn (shotgun, no armour), 40 frags:
0 + 100 + 0 + 40 = 140. A full stack outweighs a quad carrier with nothing; frags matter only
between players with the same kit.

**The expression language** (for a custom formula): numbers, `+ - * /`, parentheses, `mod`,
`div`, comparisons (`< <= > >= = == !=`), `and`/`or`/`xor`, and `%` followed by one letter for a
variable. The variables: `a` armour amount, `A` armour weight, `h` health, `f` frags, `W` weapon
weight, `p` powerup weight, `g` deaths so far, `c` / `C` / `d` the current life's time in seconds
/ frag gains / frag losses, `v` / `V` / `w` the average life's time / frag gains / frag losses,
`I j J k l m` how many times a super shotgun, nailgun, super nailgun, grenade launcher, rocket
launcher, lightning gun was taken, `K L M` how many grenade launchers, rocket launchers,
lightning guns were lost, `n N o O` mega, green, yellow, red armour taken, `x X` rings taken /
lost, `y Y` quads taken / lost, `z` pents taken, `u` the player's slot (a tie-breaker), `F` team
frags (declared, never filled: always 0). Any other letter is 0. **`%C` is already a kill
streak**: the number of frag increases since the player's last death (each increase counts once
however large); the default formula does not use it.

### ezQuake, mode 4: the fixed ranking

Players are compared pairwise, starting from the one on screen. Tiers, best first: pentagram with
RL; quad with RL or LG; pentagram; quad; RL or LG; anything else. The higher tier wins; on equal
tiers the incumbent (the one already chosen) stays. No armour, health, ammo or frags.

### KTX `autotrackktx` and the demo hints: a sum of fixed bonuses

Only LIVING players are rated (health above 0, and in Clan Arena also not eliminated):

| Holding | Points |
| --- | --- |
| Pentagram active | 99999 |
| Quad active | 9000 |
| Ring OR biosuit active (once, even with both) | 4000 |
| RL with rockets > 0 | 1500 |
| LG with cells > 0 | 500 |
| GL with rockets > 0 | 200 |
| SNG with nails > 0 | 100 |
| SSG with shells > 0 | 50 |
| Each frag | 1 |

The terms add: RL + LG + GL with ammo is 2200 plus frags. Armour and health do not count. The
highest total wins; ties go to the lowest entity (slot) number. A second best is kept the same
way for `next_best`. In the race mode the best is always the racer.

### KTX `auto_pow`

Living players only: pentagram 4000, quad 2000, ring 1000 (biosuit deliberately 0), plus frags.
Ties to the lowest slot.

## 3. The switching rules

### ezQuake `mvd_autotrack`

- **Paused** while the serverinfo key `status` reads `Standby` or `Countdown` (any case).
- **Cadence**: at most once every 0.5 s of the client's REAL time (not demo time: at 4x playback
  it evaluates every 2 s of game time).
- **When it may switch** (modes 1, 2, 4, unless `mvd_autotrack_instant 1`): only at a "switch
  moment", which is any of: the player on screen holds neither RL nor LG while someone else
  (within the locked team) does; the player on screen has neither quad nor pentagram while
  someone else has one; someone holds pentagram and RL together. Otherwise the view stays even if
  a better-rated player exists. With `instant` every evaluation may switch.
- **Choice**: the best rating. The player on screen is the starting candidate and is replaced
  only by a STRICTLY higher rating, so ties keep him; among the others the first in slot order
  wins a tie. No margin, no minimum time on a player beyond the 0.5 s cadence.
- **Death**: no rule of its own. A dead player keeps his item bits until he respawns (`verify`:
  the mod clears ring and pentagram at death, quad later), so a death usually triggers a switch
  only at the respawn, when he is down to the shotgun and someone else holds RL or LG.
- **Team lock** (`mvd_autotrack_lockteam 1`, and only when the server's `teamplay` is non-zero):
  candidates are limited to the team of the player on screen.
- **Switching** is done by issuing the client's `track` command with the chosen player's name.
- The implementation mixes slot numbers and list positions when seeding the incumbent; take the
  intent (incumbent kept on ties) as the behaviour.

### KTX `autotrack` and the demo hints: event driven

A re-pick is requested only by these events (one shared flag for the demo, one per spectator):

1. **The first rocket launcher of the match** is taken from its map item (once per match).
2. **The tracked player dies** (for the demo: the player named in the last hint written).
3. **Any powerup is taken** (quad, pentagram, ring, biosuit).
4. **A quad or pentagram is about to respawn**: 2 s before it reappears, the item looks for living
   players within 500 units that it can see (line of sight); that player becomes the hint. The
   intent is the nearest, but the distance kept is never updated, so the LAST qualifying player
   in slot order wins (a quirk). Skipped while the best powerup player holds quad or pentagram.
   Ring and biosuit respawns do not predict.

(A fifth, "powerup ran out and he has no RL or LG", is wired but disabled.)

Once requested, the decision runs every server frame (the server's tick, 13 ms by default) until
it resolves:

- A hint (event 4's player) is used if he is alive; otherwise the best by the KTX rating.
- If nobody is alive, nothing is written; the request stays pending only if there was no previous
  pick.
- **Death wait**: if the tracked player is dead and died less than 2 s ago, the decision waits
  (the request stays pending), so the switch comes 2 s after the death, or earlier if he
  respawns first (and then he may simply be picked again).
- If the pick is the player already tracked, nothing is written. Otherwise the hint is written.
- No margin and no minimum time on a player; the events themselves are the rate limit.

`autotrackktx` and `auto_pow` re-check every spectator frame with the same 2 s death wait and
switch as soon as the best changes.

## 4. The server's hints in the MVD

- **What**: KTX writes, in the frame of a decision, two console texts in one `svc_stufftext`
  each: `//at_dbg <reason>` then `//at <userid>`. The reason is one of `first_rl`, `death`,
  `powerup_take`, `powerup_predict`, `race_status_changed`. The userid is the player's server id
  as carried in `svc_updateuserinfo`, not the slot.
- **Where**: mvdsv puts a mod's demo-only text in a record addressed to all viewers (the MVD's
  "all" target), and never sends it to a live client. Every MVD reader sees it.
- **Following it**: ezQuake with `demo_autotrack 1` turns every `//at <id>` into `track <id>` the
  moment it is read, and turns its own `mvd_autotrack` off with "Server autotrack found, client
  autotrack turned off".
- **Gaps**: no hint exists before the first event (normally the first RL pickup), nor in demos
  from other mods. A reader needs its own rule until the first hint.
- **Other KTX demo-only markers** (all `svc_stufftext`, all viewers; entity numbers are edict
  numbers, players are slot + 1): `//ktx matchstart`; `//ktx took <item> <respawn s> <player>`
  (a pickup; respawn 0 for a mega health that rots); `//ktx timer <item> <s>`; `//ktx drop
  <backpack> <items bit> <player>` (a backpack with RL or LG); `//ktx bp <backpack> <player>`
  (such a backpack picked up); `//ktx expire <backpack>`; `//wps <slot> <weapon> <attacks>
  <hits>` (weapon statistics).

## 5. What a reader gets from the MVD

The per-player data a rating needs is all in the stream (our reader,
`games/shooters_edge/client/demo.hpp`, already keeps the first three):

- **Stats per player**: mvdsv writes `svc_updatestat` (a byte value) or `svc_updatestatlong` in a
  record addressed to that player's stats, whenever a value changes: health, armour amount,
  current ammo, shells, nails, rockets, cells, the active weapon's item bit, the weapon model and
  the items bits (weapons, armour type, powerups; the high bits carry the sigils). Health is
  clamped at -99 when a player dies.
- **Alive or dead**: health above 0, or the multiview player record's dead bit (set when health
  is 0 or less) and gib bit.
- **Frags**: `svc_updatefrags` per slot. In a KTX team game an enemy kill is +1 to the killer, a
  teamkill -1 to the killer, a suicide -1 (-2 by the `kill` command), a death to the world -1 to
  the victim. So an enemy kill is a +1 step; ezQuake's tracker counts every rise as a kill.
  (`verify` that a mode scoring frags by damage is off: then frags are not kills.)
- **Deaths**: a health change from above 0 to 0 or less (ezQuake counts deaths this way).
- **Who killed whom, exactly**: KTX on a server with hidden-data support writes a damage block
  for every hit involving a player (§ below); the last one whose target is the victim, at or
  before the death, names the killer. The obituary line (`svc_print` at the medium level, "victim
  phrase killer") also names both, but needs a phrase table.
- **Pickups**: item bits appearing, or the `//ktx took` markers.
- **Match state**: the serverinfo key `status` (`Standby`, `Countdown`, then "N min left").

**Hidden blocks** (mvdsv): a record of the MVD's "multiple" target with a player mask of 0,
which ordinary clients skip. Its payload is a run of blocks, each a little-endian 32-bit body
length, a 16-bit type and the body; a length of -1 ends the run, and a type of 0xFFFF is an
extension prefix (skip the block). Types: 0 antilag positions, 1 a player's move command, 2 and 8
weapon choices, 3 embedded demo info, 4-6 commentary, 7 **damage done**. Damage done is 8 bytes,
four 16-bit values: the death type (bit 15 set for splash damage), the attacker's entity number,
the target's entity number (players are slot + 1) and the damage, uncapped by the victim's health
and clamped to 0..9999. Our reader counts and skips hidden records today.

## 6. Our proposal: the hottest player by streaks (not a fact about any program)

**Inputs**, per player, from the stream alone (no server hints needed): the § 2 ezQuake 4on4
base, plus `streak` = enemy kills since his last spawn, `recent` = enemy kills in the last
`window` seconds, alive. An enemy kill is a +1 frag step (cross-checked with a damage block when
present); teamkills, suicides and world deaths never add, and a death resets `streak`.

**Rating** (living players only; the dead are not candidates):

    hot = base + streak_weight x streak + recent_weight x recent
    base = armour x armour_weight + 50 x weapon_weight + powerup_weight + frags   (4on4 values)

| Setting (our names) | Default | Why |
| --- | --- | --- |
| `autotrack` | 1 | 0 off, 1 hottest, 2 follow the server's `//at` hints, falling back to 1 |
| `autotrack_streak` | 150 | Six kills in one life weigh as much as a quad (900) |
| `autotrack_recent` | 200 | Two quick kills (2 x (150 + 200) = 700) outweigh a fresh RL pickup (+400 over a shotgun) |
| `autotrack_window` | 15 s | About one fight in 4on4 |
| `autotrack_margin` | 200 or 15 % | A challenger must beat the incumbent by the larger of the two |
| `autotrack_dwell` | 4 s | Least time on a player before an ordinary switch |
| `autotrack_deathwait` | 2 s | KTX's wait: show the death, then move |
| `autotrack_lockteam` | 0 | 1: stay within one team |
| `autotrack_lookahead` | 3 s | Offline only: count kills in the next 3 s as recent, so the cut lands before the fight |

**Rules**: evaluate every 0.5 s of DEMO time (deterministic, independent of the video's speed),
paused during standby and countdown; start on the best at match start. Switch when (a) the
incumbent died at least `deathwait` ago (to the best living, no margin), or (b) a challenger
beats him by the margin and he has been on screen `dwell`, or (c) at once (dwell waived, margin
kept) when a challenger has just taken quad or pentagram or just made a kill that lifts his
`recent` to 2 or more. Ties keep the incumbent, then the lowest slot. Never switch more than once
a second. The look-ahead is possible because the video export reads the whole demo: a first pass
records each kill's time and killer, the camera pass reads ahead.

**Labs to prove it** (`verify` before tuning): over a real 4on4 MVD, chart each player's `hot`
against time with the tracked player marked; count switches per minute (aim 4-8), the share of
kills that happen on screen (killer or victim tracked), and the time on screen of each streak of
3 or more; compare with following the `//at` hints and with ezQuake's mode 1.
