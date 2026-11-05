# What a player reads: the server's messages in deathmatch

**A clean-room behaviour specification** (a WORKING document under `porting/`). It records,
in our own words, every message a player sees from a modern QuakeWorld deathmatch server
(the ktx game mod running on the mvdsv server), so that Shooter's Edge's server can print
the same words, at the same print level, with the same colouring, at the same moment.

- **What was read** (read-only clones outside this repository, November 2026):
  `QW-Group/ktx` at commit `f316310c15082834c97e54ea81780d87199a3230` (mod version string
  `1.48-dev`) and `QW-Group/mvdsv` at commit `dcc8355279fd1e180f9517881ceec57fd1e4c95f`
  (server version string `1.20-dev`), plus ktx's example server configuration.
- **No code is copied.** Only behaviour and the visible strings are recorded; the strings are
  the behaviour (players must see the same words). Names below are the cvars and commands a
  player or admin types.
- **Setup assumed:** ktx's shipped defaults (the example `ktx.cfg`) with one of its built-in
  modes applied: `1on1`, `2on2`, `4on4` (the server's default mode) or `ffa`. CTF, Rocket Arena,
  Clan Arena, race, midair, instagib, hoonymode and bloodfest are out of scope except where a
  line notes them.

## 0. Notation and the character rules

The classic charset has 256 glyphs. Bytes 128..255 repeat 0..127 in a second colour (brown,
the "red text" every client shows), and a few low bytes are special glyphs. In this document:

| Mark | Bytes on the wire | What it is |
| --- | --- | --- |
| `⟦text⟧` | every byte of *text* in 33..127 has 128 added; spaces (32) and bytes already ≥128 are left alone | "red text" (brown) |
| `⟪12⟫` | each decimal digit `0`..`9` sent as byte 146..155 (the digit + 98); other characters unchanged | the gold numbers |
| `⁅` `⁆` | bytes 144 and 145 | gold square brackets |
| `⁅[` `]⁆` (low) | bytes 16 and 17 | the same brackets from the low half (used once, noted) |
| `{135}` `{131}` `{132}` `{143}` | that single byte | decorative glyphs (135 a marker in warnings and stats rows, 131 the "disconnected" marker, 132 a marker before spectator item lines, 143 a centred dot used as a separator) |
| `{LINE38}` | byte 157, then 36 × byte 158, then byte 159 | a horizontal rule 38 glyphs wide |
| `{LINE16}` | byte 157, 14 × 158, 159 | a 16-glyph rule |
| `{LINE10}` | byte 157, 8 × 158, 159 | a 10-glyph rule |
| `␤` | byte 10 | newline (written out wherever its presence matters) |
| `{victim}`, `{killer}`, `{name}`, `{n}` … | the substituted text | placeholders; a name is the player's name bytes exactly as in their userinfo |

Rules:

1. **Red text** turns only 33..127 into 161..255; spaces stay plain, so a red phrase with spaces
   shows plain gaps. Digits made red this way (176..185) are *not* the gold numbers; ktx uses
   both, and this document keeps them apart: `⟦10⟧` is red digits, `⟪10⟫` gold.
2. **Gold numbers** in ktx are always the high-half copies 146..155 (not 18..27; both render as
   gold digits in every client, but to be byte-exact send 146..155). Padding to a width happens
   *after* the conversion, so `⟪9⟫` padded to width 2 is a space then byte 155.
3. **Names are never recoloured.** A player's name goes out as their raw name bytes; whatever
   high-bit characters they chose show as they chose.
4. **Print levels**: 0 low, 1 medium, 2 high, 3 chat. Every print is filtered per client by the
   client's `msg` userinfo key: a message below that level is not sent at all. The common
   clients default `msg` to 1, so **level-0 messages (item pickups) are hidden for most players**
   unless they set `msg 0`; a client that sends no `msg` key gets everything.
5. A client shows level 3 (chat) with the chat sound `misc/talk.wav`; the classic client also
   draws the whole chat line in the brown half. That is client behaviour; the server sends the
   chat text plain.
6. **bprint** = to every connected client (players and spectators) that passes rule 4, and into
   the server demo. **sprint** = to one client. **centerprint** = the text in the middle of the
   screen, replacing any earlier one; the client keeps it about 2 s (its `scr_centertime`)
   unless the server sends it again, so the server re-sends what must stay up.
7. Sounds marked "local" are sent as a stuffed `play <sample>` command (the client plays it at
   full volume, no position); "world" sounds are positional sounds on an entity.
8. **Random variants** below are chosen by drawing one uniform number r in [0,1) from the game's
   single shared random stream and taking variant ⌊r × N⌋ (N = number of variants listed, in the
   listed order). Each such message consumes exactly one draw; messages with one form draw
   nothing. Our server draws from `core::Rng`; matching ktx's stream is not required.
9. Gender: if the player's userinfo key `gender` is exactly `f`, the pronoun words are
   `her`/`herself`/`she`, otherwise `his`/`himself`/`he`. Written `{his:X}` = X's pronoun.

## 1. The phases, in one paragraph

A map starts in **prewar** (warm-up): players run around with every weapon, items cannot be
picked up, frags do not count and **no obituaries are printed at all**. When every player is
ready (and at least two are), a **countdown** of `k_count` seconds (10 by default; never less
than 3) runs as a centerprint. Then the **match** runs to the timelimit (or fraglimit); deaths
print obituaries and change frags. A tie at the timelimit adds **overtime**. At the end ktx prints
"The match is over", the statistics tables and goes to **intermission**; the same map then
reloads into prewar. The mode defaults that shape the messages:

| Mode (`k_defmode` is `4on4`) | deathmatch | teamplay | timelimit | overtime | powerups |
| --- | --- | --- | --- | --- | --- |
| `1on1` | 3 | 0 | 10 | time, 3 min | off |
| `2on2` | 3 | 2 | 10 | time, 3 min | on |
| `4on4` | 1 | 2 | 20 | time, 5 min | on |
| `ffa` | 3 | 0 | 20 | time, 5 min | on (drop quad and ring on death) |

All modes: fraglimit 0, `k_count` 10, `sv_antilag` 2, respawn model `k_spw` 4, discharge on,
teamkill telefrags count (`k_tp_tele_death` 1), lock mode 1, spectators may not talk to players
during a match (`k_spectalk` 0), `k_socd` 1 (movement statistics shown), no matchtag.

## 2. Obituaries (a)

**When:** at the moment of a player's death, **only while the match runs** (not in prewar, not
in the countdown, not in intermission; in those phases a death prints nothing and changes no
score). Monsters dying print nothing in deathmatch.
**How:** bprint at **level 1 (medium)** to everyone. No colouring: names as they are, the rest
plain. One line, ending in `␤`. No sound beyond the death itself.

"Gibbed" below means the victim's health after the hit is below −40. "Quad" means the killer
holds Quad Damage at that moment.

### 2.1 Killed by another player (not a teammate)

Score: killer **+1**. Victim unchanged. (Exceptions: none in the default modes.)

| Cause | Text (exact, including the final newline) |
| --- | --- |
| Axe | `{victim} was ax-murdered by {killer}␤` |
| Shotgun | `{victim} chewed on {killer}'s boomstick␤` |
| Shotgun, gibbed | `{victim} was lead poisoned by {killer}␤` |
| Super shotgun | `{victim} ate 2 loads of {killer}'s buckshot␤` |
| Super shotgun, killer has quad (gibbed or not) | `{victim} ate 8 loads of {killer}'s buckshot␤` |
| Nailgun (2 variants) | `{victim} was body pierced by {killer}␤` · `{victim} was nailed by {killer}␤` |
| Super nailgun (3 variants, drawn always) | `{victim} was punctured by {killer}␤` · `{victim} was perforated by {killer}␤` · `{victim} was ventilated by {killer}␤` |
| Super nailgun, gibbed (replaces the drawn variant; the draw still happens) | `{victim} was straw-cuttered by {killer}␤` |
| Grenade | `{victim} eats {killer}'s pineapple␤` |
| Grenade, gibbed | `{victim} was gibbed by {killer}'s grenade␤` |
| Rocket | `{victim} rides {killer}'s rocket␤` |
| Rocket, gibbed | `{victim} was gibbed by {killer}'s rocket␤` |
| Rocket, gibbed **and** killer has quad (3 variants, drawn only in this case) | `{victim} was brutalized by {killer}'s quad rocket␤` · `{victim} was smeared by {killer}'s quad rocket␤` · `{killer} rips {victim} a new one␤` |
| Lightning beam | `{victim} accepts {killer}'s shaft␤` |
| Lightning beam, gibbed | `{victim} gets a natural disaster from {killer}␤` |
| Lightning discharge in water (2 variants) | `{victim} drains {killer}'s batteries␤` · `{victim} accepts {killer}'s discharge␤` |
| Stomp (landing on the victim's head; 5 variants) | `{victim} softens {killer}'s fall␤` · `{victim} tried to catch {killer}␤` · `{victim} was jumped by {killer}␤` · `{victim} was crushed by {killer}␤` · `{killer} stomps {victim}␤` |
| Telefrag (killer teleported or spawned onto the victim) | `{victim} was telefragged by {killer}␤` |
| Squished by a mover the killer rode | `{killer} squishes {victim}␤` |
| Grappling hook (CTF only) | `{victim} was hooked by {killer}␤` |
| Any other weapon (should not happen) | `{victim} killed by {killer} ?␤` |

The pentagram has no variant of its own (a pent holder cannot be killed except by telefrag
rules in 2.5).

### 2.2 Killed by a teammate (team modes only: both on the same non-empty team)

Score: killer **−1** (for a telefrag too, because `k_tp_tele_death` is 1). Victim unchanged.

| Cause | Text |
| --- | --- |
| Telefrag | `{victim} was telefragged by {his:victim} teammate␤` |
| Squished | `{killer} squished a teammate␤` |
| Stomp (2 variants) | `{victim} was jumped by {his:victim} teammate␤` · `{victim} was crushed by {his:victim} teammate␤` |
| Anything else (4 variants) | `{killer} checks {his:killer} glasses␤` · `{killer} loses another friend␤` · `{killer} gets a frag for the other team␤` · `{killer} mows down a teammate␤` |

Note whose pronoun: the telefrag and stomp lines use the **victim's**, "checks … glasses" the
**killer's**.

### 2.3 Killed by oneself

Score: victim **−1**; **−2** for the `kill` console command.

| Cause | Text |
| --- | --- |
| Own grenade | `{victim} tries to put the pin back in␤` |
| Own rocket (2 variants) | `{victim} discovers blast radius␤` · `{victim} becomes bored with life␤` |
| Own discharge, standing in slime | `{victim} discharges into the slime␤` |
| Own discharge, standing in lava | `{victim} discharges into the lava␤` |
| Own discharge, in water (2 variants) | `{victim} heats up the water␤` · `{victim} discharges into the water␤` |
| Discharge in deathmatch 4/5 that kills only the firer (half the time) | `{victim} electrocutes {himself:victim}␤` |
| Squished while being one's own attacker | `{victim} was squished␤` |
| The `kill` command | `{victim} suicides␤` |
| Anything else | `{victim} somehow becomes bored with life␤` |

The `kill` command itself answers the player by sprint at level 2 when refused:
`Only one suicide in 1 second␤` (a second `kill` within a second; nothing else happens).
A dead player's `kill` is ignored silently. After `kill` the player respawns at once; after
any other death the earliest respawn is 5 s.

### 2.4 Killed by the world

Score: victim **−1**.

| Cause | Text |
| --- | --- |
| Falling | 2 variants: `{victim} cratered␤` · `{victim} fell to {his:victim} death␤` |
| Drowning | 2 variants: `{victim} sleeps with the fishes␤` · `{victim} sucks it down␤` |
| Slime | 2 variants: `{victim} gulped a load of slime␤` · `{victim} can't exist on slime alone␤` |
| Lava | 2 variants: `{victim} turned into hot slag␤` · `{victim} visits the Volcano God␤`; if health ends below −15, instead (after the draw): `{victim} burst into flames␤` |
| Crushed by a door or platform (no player rider to blame) | `{victim} was squished␤` |
| Exploding box | `{victim} blew up␤` |
| A map's nail shooter (spikes from a trap) | `{victim} was spiked␤` |
| A map's laser | `{victim} was zapped␤` |
| A lava ball | `{victim} ate a lavaball␤` |
| A level-exit trigger (deathmatch kills whoever touches it during a match) | `{victim} tried to leave␤` |
| A hurt trigger, or anything else | `{victim} died␤` |

Monsters (only on maps that keep them): `{victim}` followed by: ` was shot by a Grunt␤`,
` was eviscerated by a Fiend␤`, ` was mauled by a Rottweiler␤`, ` was fried by a Dragon␤`,
` was blasted by an Enforcer␤`, ` was fed to the Rotfish␤`, ` was slain by a Death Knight␤`,
` was slashed by a Knight␤`, ` was destroyed by an Ogre␤`, ` became one with Shub-Niggurath␤`,
` was exploded by a Vore␤`, ` was smashed by a Shambler␤`, ` was slimed by a Spawn␤`,
` was vomited on by a Vomitus␤`, ` was scragged by a Scrag␤`, ` joins the Zombies␤`
(unknown monster: ` killed by monster? :)␤␤`, two newlines). Score: victim −1.

### 2.5 Telefrags against the pentagram

| Case | Text | Score |
| --- | --- | --- |
| A mortal teleports onto a pent holder: the teleporter dies | `Satan's power deflects {victim}'s telefrag␤` | victim −1 |
| Two pent holders telefrag each other: both lose the pent, both die, one line each | `{victim} was telefragged by {killer}'s Satan's power␤` | each victim −1 |

These two are checked before the teammate rule, so they apply in team games too.

### 2.6 What is not there

- No "changed teams" death: a team change is refused during a match (section 3.3).
- No killing sprees, first-blood or multi-kill lines during play; streaks appear only in the
  end statistics.
- No obituary to the victim alone: the bprint is the only line.

## 3. Arriving, leaving, renaming (b)

### 3.1 A player connects

| When | Who prints | To whom, how | Text |
| --- | --- | --- | --- |
| Prewar or countdown | mod | bprint, level 2 | `{name} entered the game␤` |
| During a match, coming back with the same name within the match (stats kept), duel/ffa | mod | bprint, level 2 | `{name} ⟦rejoins the game with⟧ {n} ⟦frag⟧⟦s⟧␤` (`s` omitted when n is 1; the plural `s` is red too) |
| Same, team modes | mod | bprint, level 2 | `{name} ⁅{team}⁆ ⟦rejoins the game with⟧ {n} ⟦frag⟧⟦s⟧␤` |
| During a match, known name but its stats are gone | mod | bprint, level 2 | `{name} ⟦reenters the game without stats⟧␤` (team modes: `{name} ⁅{team}⁆ ⟦reenters the game without stats⟧␤`) |
| During a match, a new player allowed in (team modes: their team already plays) | mod | bprint, level 2 | `{name} ⟦arrives late⟧␤` (team modes: `{name} ⁅{team}⁆ ⟦arrives late⟧␤`) |

In a running 1on1 or ffa only a returning name may join; in team modes only a player whose team
is already playing and not full. A refused player gets one sprint at level 2 and is
disconnected (a stuffed `disconnect`):

- duel: `Duel in progress, server locked␤Please reconnect as spectator␤`; ffa: the same with `Match` for `Duel`.
- team: `Match in progress, server locked␤Set your team before connecting␤or reconnect as spectator␤`
- team full: `Match in progress, server locked␤Your team has already maximum player count␤Reconnect as spectator␤`
- no team: `Match in progress,␤Set your team before connecting␤or reconnect as spectator␤`
- all slots of the match taken: `Sorry, server is full␤Please reconnect as spectator␤`
- a returning team player on another team: `Please join your old team and reconnect␤`

Then the welcome centerprint starts (section 5.1).

### 3.2 A player leaves or drops

| When | Who | How | Text |
| --- | --- | --- | --- |
| The client disconnects normally (any phase) | engine | bprint, level 2 | `{name} dropped␤` |
| Silent for longer than the server's timeout | engine | bprint, level 2 | `{name} timed out␤` |
| Then, if a match is running | mod | bprint, level 2, plus the world sound `player/tornoff2.wav` heard at any distance | `{name} left the game with {n} frags␤` (n = their frags, no rounding, e.g. `-2`; `frags` always plural) |
| Kicked by an admin | engine | bprint, level 2 / sprint, level 2 to the kicked | `{name} was kicked{reason}␤` / `You were kicked from the game{reason}␤` (reason is empty, or ` ({words})` holding the words the admin typed after the user id) |

The order on a normal disconnect during a match is "dropped" then "left the game with".
A player who leaves during the match is kept as a "ghost" whose stats appear in the end tables
marked with `{131}`.

### 3.3 Name and team changes

- **Name**: engine, bprint level 2, only for a player already in the game (not a spectator):
  `{oldname} changed name to {newname}␤`. Allowed in every phase. More than five changes within
  five seconds: bprint `{name} was kicked for name spamming␤` and the player is told
  `You were kicked from the game for name spamming␤`. A taken name is renamed by the engine to
  `({k}){name}` with a counter.
- **Team** (the `team` userinfo key): no broadcast ever. During a match (or countdown) a player's
  change is undone and they get a sprint at level 2: `You may ⟦not⟧ change team during game␤`.
  In prewar a *ready* player may not clear their team: `Empty ⟦team⟧ not allowed␤`.

### 3.4 Spectators

| When | How | Text |
| --- | --- | --- |
| A spectator joins | sprint level 2 to every other client; during a match only to other spectators (`k_ann` 0) | `Spectator {name} entered the game␤` |
| A spectator leaves | same rule | `Spectator {name} left the game␤` |

The engine prints nothing for spectators joining or dropping.

### 3.5 Lag

The mod watches each player's packets (`allow_timing` 1). A player silent for more than 2 s:
bprint level 2 `{135}⟦WARNING:⟧ {name} is timing out!␤`, repeated every 20 s while it lasts
(the player also glows). When they are back: bprint level 2 `{name} ⟦is back from lag⟧␤`.
Any phase.

## 4. The match flow (c)

### 4.1 Prewar: ready and break

All at level 2. The commands are `ready` and `break` (ktx has no `notready` or `unready`; clients
often alias them to `break`. An unknown command gets the engine's `Bad user command: {cmd}␤`
at level 2).

`ready`, by a player:

| Situation | Answer |
| --- | --- |
| Already ready | sprint `Type break to unready yourself␤` |
| Team mode and no team set | sprint `Set your ⟦team⟧ before ready!␤` |
| Using a handicap | sprint `{135}⟦WARNING:⟧ you are using handicap!␤` (and becomes ready) |
| Match or intermission running | nothing |
| Otherwise | bprint `{name} ⟦is ready⟧ - no matchtag set␤`; team modes `{name} ⟦is ready⟧ ⁅{team}⁆ - no matchtag set␤`; with a matchtag `… - {matchtag}` in place of `- no matchtag set` |

Then the start check runs (only after a ready, never on a timer):

1. Team-count and team-size rules. In team modes each failure prints once, to everyone (bprint):
   `Get rid of players with empty team␤`, `{n} more team{s} required!␤`, `Get rid of {n} team{s}!␤`,
   and `⟦Server wants at least⟧ {n} ⟦players in each team⟧␤⟦Waiting...⟧␤` (this last one only when
   every player is ready, to be less noisy; n is 1 for 2on2, 3 for 4on4). In 1on1 with three or more
   players: `Get rid of {n} player{s}!␤`. Any failure stops here.
2. If not every player is ready, stop (silently).
3. If fewer than two players are ready, stop (silently).
4. bprint `All players ready␤`, then bprint `Timer started␤`, and the countdown begins (4.2).

`break` (or ready-toggling off), by a player:

| Situation | Answer |
| --- | --- |
| Not ready, or intermission | nothing |
| Prewar | bprint `{name} ⟦is not ready⟧␤` |
| Countdown, more than one second left | bprint `{name} ⟦stops the countdown⟧␤`; the countdown ends, the centerprint is cleared, players stay ready except this one |
| Countdown on its last second | nothing (cannot stop it) |
| Match running | a vote to stop (4.7) |

Spectators typing `ready`/`break` get sprint `Command not allowed␤` (in the default setup).

If the team rules fail while a countdown runs (someone left), bprint `Aborting...␤` and the
countdown stops.

### 4.2 The countdown

**At its start** (the same instant as "Timer started"):

- every client hears the local sound `items/protect2.wav`;
- the engine begins the server demo and bprints at **level 3**:
  `Server starts recording (disk):␤{demo file name}␤` (the name is the mode, the teams or players
  and the map, e.g. `4on4_red_vs_blue[dm3]` or `duel_alice_vs_bob[dm6]`, then the date as
  `YYYYMMDD-HHMM`, then `.mvd`; characters a file system dislikes are replaced by the engine);
- if the server's `fpd` serverinfo hides client features, bprint level 2
  `QiZmo:⟦ timer lag enemy point⟧ disabled␤` listing only the hidden ones (default: nothing).

**Every second**, for k = `k_count` down to 1 (10, 9, … 1: the first one appears at once, the
next ones one second apart), the server sends **one centerprint to every client** (players and
spectators). The client centres each line. For `4on4` defaults it is exactly:

```
⟦Countdown⟧: ⟪10⟫␤
␤
␤
Deathmatch  ⟪1⟫␤
Mode  ⟦T e a m⟧␤
Respawns  KT2␤
Antilag     ⟪2⟫␤
Teamplay    ⟪2⟫␤
Timelimit  ⟪20⟫␤
Overtime    ⟪5⟫␤
Powerups   ⟦on⟧␤
SOCDv2  ⟦stats⟧␤
␤
no matchtag␤
␤
␤
```

(The text ends with those three newlines; it has no further terminator.) Each line is
`{label}` then a space then the value right-aligned in a fixed width counted in bytes, so the
padding spaces come before the value:

| Line | Width | Shown when | Value |
| --- | --- | --- | --- |
| `⟦Countdown⟧:` | 2 | always (then two blank lines) | k in gold digits: `⟦Countdown⟧:  ⟪9⟫` for one digit |
| `Deathmatch` | 2 | always | deathmatch in gold |
| `Mode` | 8 | always | `⟦D u e l⟧` (1on1), `⟦T e a m⟧` (2on2, 4on4), `⟦F F A⟧` (ffa), `⟦C T F⟧`; the letters red, the spaces between them plain |
| `Respawns` | 4 | always | the respawn model, plain: `KT2` for the default (`QW`, `KTS`, `KT`, `KTX` for the others) |
| `Antilag` | 5 | antilag on | its number in gold |
| `NoItems`, `Midair`, `Instagib`, `Yawnmode`, `Airstep`, `VWep`, `TmOverlay` | 5,6,4,4,5,8,3 | only when that option is on (none by default) | `⟦on⟧` |
| `Teamplay` | 4 | team modes only | teamplay in gold |
| `Timelimit` | 3 | timelimit set | minutes in gold |
| `Fraglimit` | 3 | fraglimit set | in gold |
| `Overtime` | 4 | timelimit set and overtime on | minutes in gold for time overtime; `⟦sd⟧` sudden death; `{n} ⟦tb⟧` tie-break; `gold` golden frag |
| `Powerups` | 4 | powerups on | `⟦on⟧`, or the red letters of the enabled ones (`⟦qprs⟧` subsets) |
| `SOCDv2` | 6 | always | `⟦stats⟧` by default (`⟦allow⟧`, `⟦warn⟧`, `⟦kick⟧`) |
| `Dmgfrags`, `NoSweep` | 4, 5 | when on | `⟦on⟧` |
| blank line, then `Handicap in use` | | someone has a handicap | |
| blank line, then `matchtag ⟦{tag}⟧` and two blank lines, or `no matchtag` and two blank lines | | always, last | |

So `1on1` shows `Mode  ⟦D u e l⟧`, no Teamplay line, `Timelimit  ⟪10⟫`, `Overtime    ⟪3⟫` and
no Powerups line; `ffa` shows `Mode    ⟦F F A⟧`, `Deathmatch  ⟪3⟫`, `Timelimit  ⟪20⟫`,
`Overtime    ⟪5⟫`, `Powerups   ⟦on⟧`.

**On k = 5, 4, 3, 2, 1:** every client hears the local sound `buttons/switch04.wav` (once per
second), and each player who is *not* ready gets sprint level 2 `⟦Get ready⟧!␤`.

**About 0.8 s into the last second** players are frozen invisible where they stand.

**At zero** (k_count seconds after "Timer started"): the centerprint is cleared (an empty
centerprint to all), every player is respawned at a spawn point with the teleport fog and one of
the world sounds `misc/r_tele1.wav` … `misc/r_tele5.wav` (one draw, uniform over the five), and:

- bprint level 2 `matchdate: {YYYY-MM-DD HH:MM:SS ZONE}␤` (local server time, e.g.
  `matchdate: 2026-11-05 14:03:11 CET`);
- bprint level 2 `⟦The match has begun!⟧␤`.

There is no separate "FIGHT!" centerprint: "The match has begun!" is the start signal. The
hostname gets a mark (byte 135) appended for the match, and for team games ` ({team1} vs.
{team2})` with each team cut to 4 characters; it is restored at the end.

The `ready` command between prewar and the end is answered by nothing.

### 4.3 The clock during the match

The match timer ticks once a second from the start. All its lines are bprint level 2.

| When | Text |
| --- | --- |
| Each whole minute boundary while minutes remain (with timelimit 20: after 1 min `19`, … down to `1`) | `⁅⟪{m}⟫⁆ minute{s} remaining␤` (`minute` singular for 1) |
| Same instant, team games with exactly two teams | `⟦Team⟧ ⁅{team}⁆ leads by ⟪{n}⟫ frag{s}␤` or, level scores, `The game is currently a tie␤` |
| In the last minute, with 30, 15, and 10, 9, … 1 seconds left | `⁅⟪{n}⟫⁆ second{s}␤` |

Nothing is printed at the start of the match about time, and nothing at "0 seconds"; when the
time is up the match ends (4.5) or overtime starts (4.4). Note the brackets are bytes 144/145
and the digits gold. 1on1 and ffa print only the minute and second lines.

### 4.4 Overtime

At the timelimit, overtime happens only when the top two are level: the top two players in 1on1
or ffa, the two teams in a two-team game (with three or more teams, or any other case, there is
no overtime). Then, level 2:

1. `time over, the game is a draw␤`
2. for time overtime (the default): `⁅⟪{m}⟫⁆ minute{s} overtime follows␤` (m = 3 in 1on1 and 2on2,
   5 in 4on4 and ffa). The clock then continues with the same announcements as 4.3 for the extra
   minutes. If it is still level when they run out, the same two lines print again and another
   overtime follows.
3. for sudden death (`k_overtime 2`): `Sudden death ⟦overtime begins⟧␤`; the first frag that
   breaks the tie ends the match. (Tie-break: `tie-break ⟦overtime begins⟧␤`; golden frag:
   `Golden Frag ⟦overtime begins⟧␤`.) The clock stops printing.

### 4.5 The end: "The match is over" and the tables

The match ends when time runs out (no overtime), when a frag reaches the fraglimit (checked
after every death; no extra line), or when a vote stops it (4.7). Everything below is bprint at
level 2, in this order, in the same server frame:

1. `The match is over␤`
2. (if anyone dropped demo markers) `⟦Demo markers⟧:␤{LINE38}␤` then `⟦Time⟧: {m}:{ss} ⁅{marker}⁆␤` per marker, then `{LINE38}␤`.
3. **Player statistics** (all modes).
4. **Team summary** (team modes only).
5. **Top scorers** (every mode except 1on1).
6. **Team scores** (team modes only).
7. If anyone left during the match: `␤{131} - ⟦disconnected⟧ player␤␤`.

**3. Player statistics.** Header:

```
␤⟦Player statistics⟧:␤
{LINE38}␤
⟦Frags⟧ (⟦rank⟧) {143} ⟦efficiency⟧␤                      ← 1on1, ffa
⟦Frags⟧ (⟦rank⟧) ⟦friendkills ⟧{143} ⟦efficiency⟧␤         ← team modes
```

(The space after "friendkills" is inside the red phrase, so the dot follows it directly.)
Then players: in team modes grouped by team, each group opening with `Team ⁅{team}⁆:␤`, groups
separated by an empty line `␤`. Each player block is the lines below. In team modes the block
is **preceded** by `{LINE10}␤`; otherwise **followed** by it.

| Line | Shown | Shape |
| --- | --- | --- |
| name | always | `{135} {name}:␤` (a player who left: `{135} {131}{name}:␤`) |
| score | always | `  {frags} ({rank}) {eff}%␤` with rank = frags − deaths, eff one decimal = frags ÷ (frags + deaths) × 100, or 0.0 when frags < 1; team modes put teamkills before eff: `  {frags} ({rank}) {tk} {eff}%␤` |
| weapons | always | `⟦Wp⟧:` then, each only when non-zero, ` ⟦lg⟧{p}% ({hits}/{shots})`, ` ⟦rl⟧{p}% ({direct}/{shots})`, ` ⟦gl⟧{p}%`, ` ⟦sg⟧{p}% ({damage})`, ` ⟦ssg⟧{p}% ({damage})`, all p one decimal; then `␤` |
| rockets | always | `⟦RL skill⟧: ⟦ad⟧:{avg damage per direct hit, 1 decimal} ⟦dh⟧:{direct hits}␤` |
| speed | 1on1 | `⟦   Speed⟧: ⟦max⟧:{1 decimal} ⟦average⟧:{1 decimal}␤` (the three leading spaces are plain) |
| movement | default `k_socd` | `⟦Movement⟧: ⟦Perfect strafes⟧:{p}% ({a}/{b}) ⟦SOCD detections⟧:{c}/{d} [SOCDv2]␤`, with `. SOCD movement assistance detected!` inserted before ` [SOCDv2]` when flagged |
| armour | always | `⟦Armr&mhs⟧: ⟦ga⟧:{n} ⟦ya⟧:{n} ⟦ra⟧:{n} ⟦mh⟧:{n}␤` |
| powerups | team modes | `⟦Powerups⟧: ⟦Q⟧:{n} ⟦P⟧:{n} ⟦R⟧:{n}␤` |
| RL, LG | team modes | `⟦      RL⟧: ⟦Took⟧:{n} ⟦Killed⟧:{n} ⟦Dropped⟧:{n}␤`, adding ` ⟦Xfer⟧:{n}` before the newline when packs were passed to teammates; the same with `⟦      LG⟧` |
| damage | always | team modes in deathmatch 1: `⟦  Damage⟧: ⟦Tkn⟧:{n} ⟦Gvn⟧:{n} ⟦EWep⟧:{n} ⟦Tm⟧:{n} ⟦Self⟧:{n} ⟦ToDie⟧:{n}␤`; otherwise the same without the EWep pair. ToDie is damage taken per death, 99999 for no deaths |
| time | when the match lasted | 1on1: `⟦    Time⟧: ⟦Control⟧:{s} ({pct}%)␤`; others: `⟦    Time⟧: ⟦Quad⟧:{s}␤` |
| end state | 1on1 | ` ⟦EndGame⟧: ⟦H⟧:{health} ⟦A⟧:{ga\|ya\|ra}{armour}␤` (or `0` for no armour; the armour letters plain); after an overtime also ` ⟦OverTime⟧: ⟦H⟧:{h} ⟦A⟧:…␤` with the values at the moment overtime began |
| streaks | not 1on1 | `⟦ Streaks⟧: ⟦Frags⟧:{best streak} ⟦QuadRun⟧:{best frags in one quad}␤` |
| spawn frags | always | `  ⟦SpawnFrags⟧: {n}␤` (kills within 2 s of the victim's spawn) |

Example (1on1, ffa-style trailing rule):

```
{135} alice:␤
  31 (19) 72.1%␤
⟦Wp⟧: ⟦lg⟧34.5% (310/898) ⟦rl⟧41.2% (28/68) ⟦gl⟧12.0% ⟦sg⟧28.0% (412)␤
⟦RL skill⟧: ⟦ad⟧:97.3 ⟦dh⟧:61␤
⟦   Speed⟧: ⟦max⟧:812.4 ⟦average⟧:401.0␤
⟦Movement⟧: ⟦Perfect strafes⟧:12.3% (40/325) ⟦SOCD detections⟧:0/7 [SOCDv2]␤
⟦Armr&mhs⟧: ⟦ga⟧:3 ⟦ya⟧:9 ⟦ra⟧:7 ⟦mh⟧:6␤
⟦  Damage⟧: ⟦Tkn⟧:3021 ⟦Gvn⟧:4410 ⟦Tm⟧:0 ⟦Self⟧:211 ⟦ToDie⟧:251␤
⟦    Time⟧: ⟦Control⟧:322 (53%)␤
 ⟦EndGame⟧: ⟦H⟧:87 ⟦A⟧:ra112␤
  ⟦SpawnFrags⟧: 2␤
{LINE10}␤
```

**4. Team summary** (team modes):

```
{LINE38}␤
⁅red⁆ vs ⁅blue⁆ ⟦match statistics⟧:␤
{LINE38}␤
␤⟦weapons⟧, ⟦powerups⟧, ⟦armors&mhs⟧, ⟦damage⟧␤
{LINE38}␤
```

then per team: `⁅{team}⁆: ⟦Wp⟧:` + ` ⟦lg⟧{p}%` ` ⟦rl⟧{direct hits}` ` ⟦gl⟧{hits}` ` ⟦sg⟧{p}%`
` ⟦ssg⟧{p}%` (each only when non-zero, percentages without decimals) `␤`;
`⟦Powerups⟧: ⟦Q⟧:{n} ⟦P⟧:{n} ⟦R⟧:{n}␤`; `⟦Armr&mhs⟧: ⟦ga⟧:{n} ⟦ya⟧:{n} ⟦ra⟧:{n} ⟦mh⟧:{n}␤`;
`⟦      RL⟧: ⟦Took⟧:{n} ⟦Killed⟧:{n} ⟦Dropped⟧:{n} ⟦Xfer⟧:{n}␤` (Xfer always shown here);
the same for `⟦      LG⟧`; damage `⟦  Damage⟧: ⟦Tkn⟧:{n} ⟦Gvn⟧:{n} ⟦EWep⟧:{n} ⟦Tm⟧:{n}␤`
(deathmatch 1; otherwise without EWep); `⟦    Time⟧: ⟦Quad⟧:{s}␤`. Then `{LINE38}␤`.

**5. Top scorers** (not 1on1):

```
⁅{map}⁆ ⟦top scorers⟧:␤
{LINE38}␤
      Frags: {name} ⁅{n}⁆␤
     Deaths: {name} ⁅{n}⁆␤
Friendkills: {name} ⁅{n}⁆␤          ← only if anyone teamkilled
 Efficiency: {name} ⁅{p}%⁆␤          ← p one decimal
 FragStreak: {name} ⁅{n}⁆␤          ← only if non-zero
    QuadRun: {name} ⁅{n}⁆␤
  RL Killer: {name} ⁅{n}⁆␤          ← deathmatch 1 only: most kills of rocket-launcher holders
Boomsticker: {name} ⁅{p}% ({damage})⁆␤   ← deathmatch 1 only: best shotgun accuracy
   Survivor: {name} ⁅{n}⁆␤          ← most damage taken per death
Annihilator: {name} ⁅{n}⁆␤          ← most damage given
␤{LINE38}␤
```

The numbers here are plain digits inside gold brackets. Ties list every tied player, the
second and later each on its own line indented by 13 spaces. A disconnected player's name is
prefixed by `{131}`. The labels are plain text, right-aligned to 11 characters plus `: `.

**6. Team scores** (team modes):

```
␤⟦Team scores⟧: ⟦frags {143} percentage⟧␤
{LINE38}␤
⁅red⁆: 112 {143} 56.0%␤
⁅blue⁆: 88 {143} 44.0%␤
{LINE38}␤
```

(percentage = the team's frags over the sum of all teams' positive frags, one decimal.)

After the tables: players are no longer ready, and 0.1 s later every client is sent to the
**intermission** view (with the intermission music track). The intermission lasts at least
7 s (1 + `demo_scoreslength` 6); after that any player pressing fire or jump reloads the map
(the same map in the default modes). The engine stuffs `changing` and `reconnect` to every
client; when the demo closes the engine bprints at level 3 `Server recording completed␤`.
The new map starts in prewar (section 1) with the welcome centerprint again.

### 4.6 Idle bot (off by default)

With `k_idletime` set (0 by default; the shipped 1on1 extra config sets 60), when at least half
the players are ready: bprint `␤server activates the ⟦idle bot⟧␤`; each unready player gets
sprint `console: {n} second{s} to go ready␤` every 5 s and every second below 5; at zero
bprint `server is tired of waiting␤match WILL commence!␤`, bprint `{name} was kicked by IDLE BOT␤`
per unready player (who also reads `Bye bye! Pay attention next time.␤`), and the countdown
starts. If readiness falls below half: `console: bah! chickening out?␤server disables the ⟦idle bot⟧␤`.

### 4.7 Stopping a match by vote

During a match `break` is a vote. Votes needed = ⌈0.51 × players⌉ (bots excluded).

| Event | bprint level 2 |
| --- | --- |
| A player votes | `{name} ⟦votes for stopping the match⟧ ({k})␤` with k the votes still needed; ` ({k})` is left out when none are needed |
| The same player types `break` again (withdraws) | `{name} ⟦withdraws⟧ ⟦{his:name}⟧ vote ({k})␤` |
| Enough votes | `⟦Match stopped by majority vote⟧␤`, then the whole end of 4.5 ("The match is over" and the tables) |

In 1on1 one player's `break` shows ` (1)`: both must vote.

## 5. Centerprints (d)

### 5.1 The welcome on connect

Shown to each player and spectator that connects, first 0.4 s after connecting, **re-sent every
0.7 s** so it stays, for 7 s (`k_motd_time` unset); then an empty centerprint clears it. It stops
early (without the clearing send) when the countdown or match starts, or when the player
fires. While the player holds the `+scores` or `+wp_stats` views it pauses. Default text
(no `k_motd1`… lines are set):

```
Welcome␤
␤
␤
{LINE16}␤
␤
Running ⟦KTX⟧ ⟦1.48-dev⟧ on ⟦MVDSV⟧ ⟦1.20-dev⟧␤
␤
Type "⟦commands⟧" for available commands␤
Type "⟦about⟧" for server details
```

(no newline after the last line). With `k_motd1`…`k_motdN` set, those lines (each followed by
a newline) replace `Welcome␤␤`. The mod and server names and versions are red, including their
digits and dots (red digits, not gold). Our server shows its own names and versions in the
same shape.

### 5.2 Prewar status

ktx shows **no** periodic prewar centerprint (no "waiting for players", no ready counts). The
prewar is quiet: the ready lines of 4.1 are the only status. (With `k_sready 1`, off by default,
unready players glow; no text.)

### 5.3 The countdown

As in 4.2: one centerprint per second for `k_count` seconds, cleared at the start. Because each
lasts the client's default ~2 s and a new one comes every second, it stays up continuously.

### 5.4 Admin force start (admins only)

An admin's `forcestart` shows, each second, a centerprint to players
`⟪{n}⟫ second{s} left before game starts␤⟦Get ready⟧!` (the second line only for unready
players) and to spectators just the first line; then the normal countdown. Breaking it:
bprint `{name} interrupts countdown␤`; rules failing: `Forcestart canceled␤`.

### 5.5 Pause (match only, needs a matchtag or `k_pause_without_matchtag`)

`pause`: for 3 s every client gets the centerprint `pausing␤␤{n}` (n = 3, 2, 1) with the local
sound `buttons/switch04.wav` each second; then a blank centerprint and bprint
`{name} paused the game. He has {n} remaining request(s).␤` (the word "He" is fixed, no gender).
`pause` again: bprint `{name} unpaused the game (will resume in 3 seconds)␤`, centerprint
`unpausing␤␤{n}` with the switch sound each second, then bprint `game unpaused␤`.
Refusals (sprint): `Pause is not allowed␤`, `Pause already in progress.␤`,
`Unpause is pending, {n} second{s}␤`, `Too late to pause. Please wait for match to finish.␤`.

### 5.6 Map texts

Doors and triggers centerprint their map-given messages; locked doors say
`You need the silver key`, `You need the gold key` (keycard and runekey forms on other map
themes). Rare in deathmatch.

## 6. Items and powerups (e)

### 6.1 The rule that changes most: no pickups outside the match

In ktx **nothing can be picked up in prewar or the countdown**: weapons, ammo, armour, health,
powerups and backpacks are untouchable until the match starts (players have every weapon and
full ammo in prewar instead). So no pickup line ever prints in prewar. (The classic game lets
items be taken at any time.)

### 6.2 Pickup lines

All are sprint at **level 0 (low)** to the player who took the item (so hidden at the common
`msg 1`), each with the screen flash (a stuffed `bf`) and a world sound on the taker.

| Item | Text | Sound |
| --- | --- | --- |
| Health box (rotten / normal) | `You receive 15 health␤` / `You receive 25 health␤` | the box's own sound (`items/r_item1.wav` / `items/health1.wav`) |
| Megahealth | `You receive 100 health␤` | `items/r_item2.wav` |
| Armour | `You got the Green Armor␤`, `You got the Yellow Armor␤`, `You got the Red Armor␤` | `items/armor1.wav` |
| Weapon | `You got the {weapon}␤` with `Double-barrelled Shotgun`, `nailgun` (lower case), `Super Nailgun`, `Grenade Launcher`, `Rocket Launcher`, `Thunderbolt` | `weapons/pkup.wav` |
| Ammo | `You got the shells␤`, `You got the nails␤` (`spikes` on maps with old-style ammo boxes), `You got the rockets␤`, `You got the cells␤` | `weapons/lock4.wav` |
| Powerup | `You got the Quad Damage␤` (in deathmatch 4: `You got the OctaPower␤`), `You got the Pentagram of Protection␤`, `You got the Ring of Shadows␤`, `You got the Biosuit␤` | the powerup's own sound |
| Backpack | `You get ` then `the {weapon}` if it holds a weapon the player lacks, then each non-zero ammo as `{n} shells`, `{n} nails`, `{n} rockets`, `{n} cells`, separated by `, `, then `␤` (several sprints that join into one line, e.g. `You get the Rocket Launcher, 5 rockets␤`) | `weapons/lock4.wav` |
| Backpack in deathmatch 4 | `You get 10 additional health␤`; reaching 300 health also bprints level 2 `{name} gained bonus powers!!!␤` | `items/protect3.wav` (250–299) or `weapons/lock4.wav`; bonus `boss1/sight1.wav` |

Weapon names in dropped backpacks are capitalised (`Nailgun`, `Shotgun`, `Axe` are possible).
In deathmatch 2, 3 and 5 a weapon the player already holds is not taken (no line); in
deathmatch 1 it is taken for its ammo. A full player takes nothing silently.

Compared with the classic game as it is commonly known: the armour line names the armour
(the classic line is a generic "You got armor"), pickups wait for the match, and ktx adds
the spectator lines below; the other wordings are the classic ones.

### 6.3 Powerups wearing off

Powerups last 30 s. With 3 s left the holder gets one sprint at **level 2**, a world sound, and
a screen flash every second until it ends:

| Powerup | Text | Sound |
| --- | --- | --- |
| Quad | `Quad Damage is wearing off␤` (deathmatch 4: `OctaPower is wearing off␤`) | `items/damage2.wav` |
| Pentagram | `Protection is almost burned out␤` | `items/protect2.wav` |
| Ring | `Ring of Shadows magic is fading␤` | `items/inv2.wav` |
| Biosuit | `Air supply in Biosuit expiring␤` | `items/suit2.wav` |

No line when it actually ends, and no broadcast when anyone takes a powerup (ktx prints no
"X got the Quad" to players). There are no powerup respawn timers in text: quad and suit
reappear after 60 s, pent and ring after 300 s, each with the respawn sound `items/itembk2.wav`.

### 6.4 What spectators read

Spectators can ask for sprint level 2 lines about item takes with their own userinfo key `mi`
(absent or 0, the default: none; 1: powerups, armours, megahealth and the rocket launcher;
2: also grenade and lightning launchers; 3: all weapons; 4: powerups only):
`{name} got {item}␤` (e.g. `bob got Quad Damage`), `{name} got Megahealth␤`,
`{name} got backpack with {weapon}␤`, `{name} got a {item} with {n} seconds left␤` (a dropped
powerup). In team modes each is prefixed `{132}⁅{team}⁆ ` with the team cut or padded to 4
characters, right-aligned (e.g. `{132}⁅ red⁆ bob got Quad Damage␤`).

### 6.5 Weapons and ammo refusals

Selecting a weapon not held: sprint level 2 `no weapon␤`. Selecting one without enough ammo:
`not enough ammo␤`. Both on each attempt.

## 7. Chat (f)

The mod formats chat (the engine's own formatting is only a fallback with the same shape).

| Said by | Command | Text sent (each recipient gets one level-3 print) | Recipients |
| --- | --- | --- | --- |
| Player | `say` | `{name}: {text}␤` | everyone |
| Player | `say_team` | `({name}): {text}␤` | the sayer, and players on the same team (in team modes only; in 1on1/ffa only the sayer); spectators tracking that team only if the text contains the "fake" marker byte 13 |
| Spectator | `say` in prewar | `{name}: {text}␤` | everyone |
| Spectator | `say` during a match (`k_spectalk` 0), or `say_team` | `[SPEC] {name}: {text}␤` | spectators only |

- `{text}` is everything after the command, with one pair of surrounding double quotes removed
  if present. A sayer with an empty name shows `!noname!`.
- The name and text are sent unchanged, both plain; no high-bit conversion. Typed ezQuake colour
  codes (`&cRGB`, `&r`) are kept for clients that support them and removed for clients whose
  userinfo has `nocolors`.
- With `sv_unfake` set, byte 13 in a public `say` becomes `#`.
- **Flood protection** (`k_fp` 1: 9 messages within 1 s, then silence for 1 s; level 2 is
  4 within 1 s / 5 s silence, level 3 is 5 within 3 s / 7 s): the message that breaks the limit
  is dropped and the sayer reads, at level 3, `FloodProt: You can't talk for {n} seconds.␤`;
  any message while silenced is dropped with `You can't talk for {n} more seconds␤` (n rounded
  up). Paused games do not count. Spectators have their own level (`k_fp_spec`, also 1).
- Private messages (`s-p` etc.) show `[{from}->]: {text}␤` to the receiver and
  `[->{to}]: {text}␤` to the sender, level 3.

## 8. Other common lines (g)

All bprint level 2 unless noted.

| When | Text |
| --- | --- |
| A mode is chosen with `1on1`, `2on2`, `4on4`, `ffa` … | `⟪1⟫ ⟦on⟧ ⟪1⟫ ⟦settings enabled by⟧ {name}␤` (`⟪2⟫ ⟦on⟧ ⟪2⟫`, `⟪4⟫ ⟦on⟧ ⟪4⟫`, `⟦ffa⟧`; from the server console `… ⟦settings enabled⟧␤`) |
| Overtime length changed (`k_exttime` command) | `⟦Overtime length set to⟧ {n} ⟦minute⟧⟦s⟧␤` |
| Map vote, first | `{name} ⟦suggests map⟧ {map}␤` |
| Map vote, agreeing | `{name} ⟦agrees⟧ ⟦on⟧ ⟦map⟧ {map}␤` (`⟦to⟧` instead of `⟦on⟧` with fewer than 3 players) |
| Map vote, another map | `{name} ⟦would rather play on⟧ {map}␤` |
| Map vote passes | `⟦Majority⟧ votes for mapchange.␤`, then the map changes |
| Map vote refused (sprint) | `Wait {n} second{s}!␤` (first 7 s of a map), `--- your vote is still good ---␤`, `Map '{map}' not available on this server␤`, `MAP IS LOCKED!␤You are NOT allowed to change!␤` |
| Generic election vote (`yes`) | `{name} gives {his:name} vote␤`, then `⁅{n}⁆ more vote{s} needed␤` (plain digits); `no`: `{name} withdraws {his:name} vote␤` |
| An election times out | `The voting has timed out.␤Election aborted␤` (one print) |
| A vote-holder leaves | `Election aborted␤` |
| `kill` while dead (engine) | sprint `Can't suicide -- already dead!␤` |
| A client overflows (engine) | `{name} overflowed␤` |
| Server demo removed or failed (engine, level 3) | `Server recording canceled, demo removed␤` (the countdown was stopped or aborted, or a match ended because every player left within its first 6 min without a matchtag), `Server recording stopped␤Max demo size exceeded␤` |
| Practice mode | sprint `⟦Server in practice mode⟧␤` on `ready` |

## 9. Q&A for the implementer

1. **Is the obituary sent to everyone at medium?** Yes: one bprint at level 1 to every client,
   players and spectators, and into the demo. It is the only obituary line; the victim gets no
   private one.
2. **Are deaths in prewar printed?** No. Outside the running match a death prints nothing and
   changes no frags or deaths. The countdown and intermission are outside too.
3. **How is a variant chosen?** One uniform draw r in [0,1), index ⌊r × N⌋ over the listed
   order. Only the messages that have variants draw; the rocket quad variants draw only when the
   victim is gibbed by a quad rocket; the super nailgun always draws, even when "straw-cuttered"
   replaces it; lava draws before checking "burst into flames".
4. **What counts as gibbed?** Victim health after the hit below −40 (lava's "burst into flames"
   uses −15).
5. **Frags in one table?** Enemy kill +1 to the killer; teamkill −1 to the killer (telefrag
   included); suicide −1, `kill` −2; world death −1; pent-deflected telefrag −1 to the would-be
   telefragger; double-pent telefrag −1 each.
6. **Which pronoun?** From the userinfo `gender` key (`f` → her/herself). Victim's pronoun in
   "fell to … death", "electrocutes …", team telefrag and team stomp; killer's in "checks …
   glasses".
7. **What does the countdown look like?** A centerprint per second, starting at `k_count` (10),
   to all clients: `⟦Countdown⟧: ⟪10⟫`, two blank lines, then the settings table (4.2), then
   `no matchtag`. Gold digits for numbers, red for words like `on`. The last 5 seconds add the
   local switch sound and "Get ready!" for the unready. At zero: blank centerprint, teleport
   sounds, `matchdate: …`, `⟦The match has begun!⟧`.
8. **How long does a centerprint stay?** The client's default, about 2 s. The server repeats the
   countdown every 1 s and the welcome every 0.7 s to keep them up, and clears both with an
   empty centerprint.
9. **Gold digits: 18..27 or 146..155?** ktx sends 146..155 everywhere. Brackets around them are
   144/145. Red text never turns digits gold.
10. **When are the time warnings?** At each whole minute while minutes remain
    (`⁅⟪19⟫⁆ minutes remaining`), then 30, 15, 10..1 seconds before the end (`⁅⟪10⟫⁆ seconds`),
    plus the team lead line at each minute in two-team games.
11. **Overtime trigger?** Top two level at the timelimit. Default is a time overtime of 3 min
    (1on1, 2on2) or 5 min (4on4, ffa), announced by `time over, the game is a draw` and
    `⁅⟪5⟫⁆ minutes overtime follows`, repeated while still level.
12. **Does fraglimit print anything special?** No: the match just ends with "The match is over".
13. **Are pickup lines shown?** They are sent at level 0 and only during a match; most clients
    filter level 0 by default (`msg 1`), so most players never see them. Powerup warnings are
    level 2 and always seen.
14. **Who prints "dropped" and "left the game"?** The engine prints `{name} dropped` (normal
    disconnect) or `{name} timed out`, in every phase; during a match the mod then adds
    `{name} left the game with {n} frags` with an unattenuated sound.
15. **Is there a team-change broadcast?** No. During a match the change is refused privately.
16. **Chat format?** `{name}: {text}`, team `({name}): {text}`, spectators-only `[SPEC] {name}: {text}`,
    level 3, plain bytes; the client adds the sound and the classic client the brown colour.
17. **How is padding counted in the countdown table?** In bytes, after colouring, right-aligned
    (the pad spaces sit before the value); there is always one space between label and field.
18. **What ends the match screen?** 0.1 s after the tables, intermission; after 7 s a fire or
    jump press reloads the map; the engine stuffs `changing` and `reconnect`.
