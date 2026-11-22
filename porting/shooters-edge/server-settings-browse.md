# Server settings, browsing and changing them: clean-room facts

A WORKING clean-room note of facts about what a connected client can learn of a classic
server's settings (server info keys, status replies, listing commands) and how players change
those settings (direct commands and votes), for Shooter's Edge's "Server settings" menu page on
stock QuakeWorld servers (MVDSV with the KTX match mod) and stock Quake II servers (Q2PRO with
the stock deathmatch game library, and with the OpenTDM match mod). A reader session read the
sources below and wrote these facts in its own words with no code (`clean-room` skill).

Read at the projects' default branches, shallow clones taken 2026-11-22:

- MVDSV (QW-Group/mvdsv), last commit 2026-11-18, reports itself as version "1.20-dev".
- KTX (QW-Group/ktx), last commit 2026-11-08, reports itself as version "1.48-dev".
- Q2PRO (github.com/q2pro/q2pro mirror; skullernet/q2pro needed a login and could not be
  cloned), last commit 2025-12-11, with the stock deathmatch game library ("baseq2") that
  ships in the same repository.
- OpenTDM: packetflinger/opentdm (last commit 2026-09-08) is the current one read; the
  original notr1ch/opentdm (last commit 2012-07-12) was compared where they differ.

Marks: **read** (the source does it), **derived** (follows from read facts, working shown),
**measured** (nothing was run: there are none). Text in high-bit ("red" or "gold") characters
is called "coloured" below: in the QuakeWorld character set a printable character with 128
added draws in the alternate colour; KTX's "gold digits" are the digits 0 to 9 moved to codes
146 to 155 (the digit plus 98). Quake II's "green" text is likewise the character plus 128.

## A. QuakeWorld: MVDSV + KTX

### 1. Server info keys

General rules (MVDSV):

- **read** The serverinfo string is at most 512 bytes. A connected client receives the whole
  string once at connect (as a stuffed `fullserverinfo "<string>"` line) and then one update
  message (key, value) every time a key's value changes; a key whose value becomes empty is
  removed.
- **read** Any setting mirrored into serverinfo that is set to exactly "0" is stored as the
  empty string, so the key DISAPPEARS. A client must therefore treat a missing timelimit,
  fraglimit, teamplay, fpd, sv_antilag, pm_* key as 0. (There is a special case meant to keep
  `deathmatch 0` visible, but the ordinary setting path blanks "0" before it is reached, so
  **derived**: a server set to deathmatch 0 also shows no `deathmatch` key.)
- **read** The out-of-band `status` request takes an optional number that is a sum of option
  bits: 0 (or no number) = the classic reply (serverinfo line plus one line per player); 1 = serverinfo; 2 = players; 4 = spectators; 8 = spectators shown as players with frags "S"; 16 = add each client's team; 32 = add the QTV stream list; 64 = add each client's login
  flag; 128 = add "b" (bot) or "h" (human). The reply is a connectionless "print" packet (four
  0xFF bytes then 'n') whose first line is the raw serverinfo string. Each client line is:
  userid, frags, minutes connected, ping, quoted name, quoted skin, top colour, bottom colour
  (colours clamped to 0..16), then the optional quoted fields in the order team, login flag,
  client type. Spectators in the plain listing have frags "-9999", a negative ping and the
  name prefixed by `\s\`.
- **read** The connected client's `serverinfo` command (non-overridable by the mod) prints
  the same string as a key and value list.

Keys MVDSV itself publishes (all **read**; defaults are MVDSV's own before any config):

| Key | Meaning, type, range |
| --- | --- |
| `*version` | server name and version, "MVDSV 1.20-dev" in the build read |
| `*z_ext` | decimal bitmask of the protocol extensions the server supports |
| `*gamedir` | the game directory (e.g. "qw") |
| `*progs` | which game module is loaded (a checksum for a QC progs, or the native library's extension) |
| `*qvm` | present when a native or compiled-module game is loaded: the library extension ("so", "dll", "dylib") |
| `*cheats` | "ON" while cheats are allowed, absent otherwise |
| `*admin` | the operator's admin contact text, absent when unset |
| `*csprogs`, `*csprogssize`, `*csprogsname` | client-side progs checksum, size and name, only when one is offered |
| `map` | the current map's file name without extension |
| `hostname` | server name, default "unnamed" |
| `hostport`, `countrycode`, `city`, `coords` | operator-set location strings, absent by default |
| `maxclients` | player slots, default 24 |
| `maxspectators` | spectator slots, default 8 |
| `deathmatch` | default 3 |
| `teamplay` | default 0 (so absent) |
| `timelimit` | minutes, default 0 (absent) |
| `fraglimit` | default 0 (absent) |
| `maxfps` | the server's packet-rate cap for clients, default 77 |
| `watervis` | 1 allows seeing through water, absent when 0 |
| `needpass` | bitmask: 1 = player password, 2 = spectator password, 4 = VIP spectator password; absent when none |
| `serverdemo` | the name of the server demo being recorded, absent when none |
| `epoch` | Unix time (seconds) written when a server demo starts |
| `broadcast` | shown when the server's broadcast-to-other-servers feature is switched (its own key name) |
| `maxpitch`, `minpitch` | view pitch limits, shown only when not the defaults 80 and -70 (ranges 0..89.9 and -89.9..0) |
| `sv_antilag` | lag compensation: absent/0 off, 1 or 2 on (KTX uses 2 for "on") |
| `sv_antilag_no_pred`, `sv_antilag_projectiles` | antilag refinements, absent by default |
| `pm_ktjump` | the jump fix, default 1 |
| `pm_bunnyspeedcap`, `pm_slidefix`, `pm_airstep`, `pm_pground`, `pm_rampjump` | movement switches, absent by default |
| `sv_safestrafe` | absent by default |
| `sv_bigcoords` | absent by default |

(This table lists the keys the server writes; it was compiled from the places the server sets
its info string and the settings it mirrors into it, not copied from any table in the source.)

Keys KTX adds (all **read**):

- `ktxver`: KTX's version string ("1.48-dev" in the build read), written at map start. This is
  the KTX version key.
- `status`, `matchtag`, `fpd`: already known to the asker.
- `mode`: the match mode in force, written by KTX every frame (so updated within one frame
  of any change). Its shape is `<base>` followed by zero or more `-<sub>` suffixes. The base
  is the name of the last mode command that ran: `1on1`, `2on2`, `3on3`, `4on4`, `10on10`,
  `ffa`, `ctf`, `hoonymode`, `blitz2v2`, `blitz4v4`, `2on2on2`, `3on3on3`, `4on4on4`, `XonX`,
  `wipeout`, `ca`, `tot`. The suffixes, appended in this order when the matching mode is on:
  `-race` (race mode), `-midair`, `-instagib`, `-lgc` (lightning gun contest), `-ca` (clan
  arena), `-wo` (wipeout), `-ra` (rocket arena), `-gm` (dmm4 grenade mode), `-df` (damage
  frags), `-yw` (yawn mode), `-bf` (blood fest). Example: `1on1-midair`.
  - **read** When no mode command has run since the server started (no `k_defmode` set and
   no player typed one), the key is not written at all. A server-wide mode reset (all players
   gone) forgets the last mode, but the key keeps its last value until the next mode runs
   (**derived**: the key is only ever written, never removed, by KTX).
  - **read** In "matchless" (free-for-all without matches) servers the base is `ffa`.
- No key named "ktxmode" or "ruleset" exists. The only "ruleset" word in KTX is that, when a
  `matchtag` is set, KTX makes every player's client say `f_ruleset` at match start and end
  (a client-side rules check). **read**
- KTX changes these MVDSV keys as players act: `deathmatch` (dmm commands, 1..5), `teamplay`
  (`tp`, 1..4), `timelimit` (time commands), `fraglimit` (frags commands), `maxclients` and
  `maxspectators` (player and spectator slot commands), `sv_antilag` (antilag vote, toggles
  between off and 2), `pm_airstep` (airstep command), `fpd`. KTX also sets `maxfps` to 77 if
  the server has no value. **read**
- KTX's own settings (`k_*`, `dq`, `dr`, `dp` and so on) are NOT in serverinfo. **read**

### 2. Settings a player can change, and how

General rules (all **read**):

- Every KTX command below is typed as a console word (KTX gives the client an alias per command,
  see question 5) or sent as `cmd <word> [args]`.
- A command marked for players may be typed by any player; a spectator may use the same command
  only when admin. Admin-only commands (`forcestart`, `forcebreak`, `forcemap`, `prewar`,
  `lockmap`, `kick`, `mkick`, `lock`, `exclusive`, `fp`, `hdptoggle`, `tkfjump`, `tkrjump`,
  `noga`, `mctf`, `force_spec`, `infolock`, `ban`, `banip`, `banrem`, `race_break_all`) answer
  "You are not an admin" to others.
- Rule changes are allowed only when no match is running (standby). During a countdown or a
  match, most toggles do nothing and print NOTHING; the mode commands, midair, instagib, lgc
  and arena instead print "Command is locked while match is in progress" ("match" coloured); with race mode on they print "race mode is on, please toggle it off by using race command
  first".
- Direct toggles take effect at once for everyone, with no vote; the broadcast line has the
  form `<player name> enables <thing>` or `<player name> disables <thing>`, `<thing>` coloured.
  This is the shape of every plain on/off toggle below unless stated otherwise.

| Command(s) | What it changes | Values | Line every player sees (after the change) |
| --- | --- | --- | --- |
| `1on1`, `2on2`, `3on3`, `4on4`, `10on10`, `ffa`, `ctf`, `hoonymode`, `blitz2v2`, `blitz4v4`, `2on2on2`, `3on3on3`, `4on4on4`, `XonX`, `wipeout`, `carena`, `tot` | the whole rule set for that mode (runs the mode's settings, then the server's config files for that mode and map); an optional argument is the match tag | none, or a tag | `<display name> settings enabled by <player>` (coloured), then the matchtag line. Allowed only when the server's `k_free_mode` permits (5 = all players; 0 none, 1 real admins, 2 elected admins too, 3 or 4 judges) and the mode is in `k_allowed_free_modes`; otherwise "Server disallows this command" or a permission line. |
| `dmm1` .. `dmm5` | deathmatch mode | 1..5 | `Deathmatch <n>` (n in gold digits); "dmm<n> already set" to the caller if unchanged. Leaving dmm4 turns midair and instagib off; entering dmm4 sets timelimit 3. |
| `dm` | shows the current deathmatch mode | | `Deathmatch <n>` |
| `tp` | teamplay | cycles 1, 2, 3, 4, 1; only in team or CTF modes | `Teamplay <n>`; in other modes the caller gets "console: non team mode disallows you to change teamplay setting" |
| `timeup` / `timedown` | timelimit | steps of 5 minutes, except 0, 1, 3, 5 step to each other; bounded 0..`k_timetop` (30 when unset) | `Match length set to <n> minute(s)`; "timelimit still <n>" to the caller if unchanged; refuses to reach 0 time and 0 frags together ("You need some timelimit or fraglimit at least") |
| `timeup1` / `timedown1` | timelimit | steps of 1 minute | as above |
| `time5`, `time10`, `time15`, `time20`, `time25`, `time30` | timelimit | that many minutes (bounded by `k_timetop`) | as above |
| `fragsup` / `fragsdown` | fraglimit | steps of 10, bounded 1..100 (`fragsdown` from 1 goes to 0); in hoonymode "No fraglimit in hoonymode" | `Fraglimit set to <n>` |
| `overtime` | overtime type | cycles off, time based, sudden death, tie-break, golden frag | `Overtime: time based` (plus `Overtime length: <n> minute(s)`), `Overtime: sudden death`, `Overtime: tie-break`, `Overtime: golden frag`, `Overtime: off` |
| `overtimeup` | overtime length | 1..10 minutes, wraps to 1 | `Overtime length set to <n> minute(s)` |
| `powerups` | all four powerups at once; with arguments `q`, `p`, `r`, `s` toggles quad, pentagram, ring, suit separately | on/off | `<player> enables/disables powerups` (or `quad`, `pent`, `ring`, `suit`). Refused with "Powerups are disabled with Instagib" or "... with Midair" |
| `powerups_pickup` | new powerups pickup rule | on/off | `<player> enables/disables new powerups pickup (no multi pickup)` |
| `spawn` | spawn model | cycles through six | the model's name: "pre-qtest nonrandom respawns", "Normal QW respawns", "KT SpawnSafety", "Kombat Teams respawns", "KTX respawns", "KTX2 respawns" |
| `discharge` | lightning discharge in water | on/off | `... discharges` |
| `fairpacks` | what a dead player's backpack holds | cycles off, best weapon, last fired weapon | `Fairpacks disabled`, `Fairpacks enabled - drop best weapon`, `Fairpacks enabled - drop last fired weapon` |
| `dropquad`, `dropring`, `droppack` | drop on death | on/off | `... DropQuad`, `... DropRing`, `... DropPacks` |
| `lockmode` | joining during a match | cycles team lock, full lock, off | `Teamlock on - only players in existing teams can connect during game`, `Server locked - players cannot connect during game`, `Server locking off` |
| `speed` | run speed | toggles 320 and the server's `k_highspeed` | `Maxspeed set to <n>` |
| `silence` | spectators heard by players | on/off | `Spectalk on: players can hear spectators during game` / `Spectalk off: players cannot hear spectators during game` |
| `fallbunny` | ankle break on falls | on/off | toggle line |
| `killquad`, `berzerk`, `vwep`, `airstep` | as named (airstep changes the `pm_airstep` key) | on/off | toggle line (`KillQuad`, `Berzerk mode`, `vwep`, `pm_airstep`) |
| `midair` | midair mode (needs dmm4: "Midair requires dmm4"); turns instagib, lgc, tot and grenade mode off | on/off | `... Midair` |
| `midair_minheight` | midair's minimum frag height | 0..4 | |
| `instagib` | instagib (needs dmm4) | cycles off, 1, 2, 3 | `Instagib disabled` or `Instagib enabled ...` |
| `lgcmode` | lightning gun contest (needs dmm4) | on/off | |
| `arena` | rocket arena (only from duel: "Set 1 on 1 mode first") | on/off | `... Rocket Arena` |
| `yawnmode`, `totmode`, `dmgfrags`, `gren_mode`, `fresh`, `freshpacks`, `freshguns`, `freshtime`, `nosweep`, `noitems`, `noweapon <w>` (`no_lg`, `no_gl`), `teleteam`, `spawnicide`, `spawn_show`, `spawn666time`, `teleportcap`, `instagib_coilgun_kickback`, `swapall` (CTF) | further rule switches, all standby-only | | |
| `upplayers`, `downplayers`, `upspecs`, `downspecs` | `maxclients` / `maxspectators` by 1, bounded 1..`k_maxclients` / `k_maxspectators` | | `<player> set maxclients to <n>`; needs the permission in `k_allowcountchange` (after a mode reset: 1 = real admins only) |
| `practice` | practice mode (reloads the map) | on/off | `Server in practice mode` / `Server in normal mode`; permission from `allow_toggle_practice` (same 0..5 scale) |
| `race` | race mode | on/off | |
| `pause` | pauses the game | on/off | |
| `qlag`, `qenemy`, `qpoint`, `skinforce`, `colorforce`, `pitchsl`, `yawsl` | bits of `fpd` (client feature limits) | on/off | |
| `ready`, `break`, `toggleready` | own ready state; `break` during a match is the stop vote | | |

Votes (anything another player must agree to), all **read**:

| Command | What it decides | Share needed (server setting, default) | Minimum votes |
| --- | --- | --- | --- |
| `<mapname>` alias or `votemap <map>` | change map | `k_vp_map`, 51% | |
| `break` (during a match) | stop the match | `k_vp_break`, 51% | 1 |
| `next_map` (matchless only) | go to next map | `k_vp_map` | 2 while counting down |
| `pickup` | a pickup game (everyone set to no team) | `k_vp_pickup`, 51% | |
| `rpickup` | random teams (needs at least 4 players) | `k_vp_rpickup`, 51% | 3 |
| `swapall` (CTF) | swap the teams | `k_vp_rpickup` | 3 |
| `antilag` | lag compensation on/off (needs 2 players unless admin) | `k_vp_antilag`, 51% | 2 |
| `nospecs` | no spectators on/off | `k_vp_nospecs`, 51% | 2 (1 to turn it back off) |
| `teamoverlay` | the team overlay on/off | `k_vp_teamoverlay`, 51% | 2 |
| `votecoop` | coop on/off | `k_vp_coop`, 51% | 1 |
| `voteprivate` | private game (logged-in players only); needs login | `k_vp_privategame`, 51% | 2 |
| `hook_smooth`, `hook_fast`, `hook_classic`, `hook_crhook` (CTF) | grappling hook style | `k_vp_hookstyle`, 51% | 1 |
| `elect` | elect yourself admin (needs `k_admins` and `k_allowvoteadmin`) | `k_vp_admin`, 51% | 2 |
| `captain` | elect yourself captain | `k_vp_captain`, 51% | 2 |
| `coach` (spectators) | elect yourself coach | `k_vp_coach`, 51% | 2 |
| `suggestcolor <top> <bottom> <name or id> ...` | set other players' colours | `k_vp_suggestcolor`, 51% | 2 |
| `latejoin <team>` (clan arena, during a match) | join a team late | 51% of that team's players | 1 |

- There is NO kick vote for players: `kick` and `mkick` are admin commands (`y` / `n` confirm
  an admin's kick). **read**
- "Player help" options a vote switches on: the team overlay (`teamoverlay`) and antilag
  (`antilag`) are the only ones. **read**

### 3. How KTX voting works

All **read** unless marked:

- **Share**: the needed count is the share (each `k_vp_*` setting, bounded to 51..100 percent,
  an unset or 0 value counting as 51) times the number of players NOT counting bots and NOT
  counting spectators, rounded up, then raised to the per-vote minimum in the table above. Only
  players can vote (`yes` and the toggling votes are player commands); a spectator can vote for
  a map only when admin, and a spectator may start a coach election. With bots present and exactly one human,
  one vote suffices (except private game and elections). A vote by an admin passes the
  non-election toggles at once ("admin veto"). Map votes by admins win over others.
- **Toggling votes** (break, pickup, rpickup, antilag, nospecs, teamoverlay, votecoop,
  voteprivate, hook styles, swapall): the same command both casts and withdraws one's vote.
  There are no separate yes/no words for these. There is no "no" vote: you either vote or not.
- **Elections** (`elect`, `captain`, `coach`, `suggestcolor`, `latejoin`): the candidate's
  own vote counts; others answer with `yes`; `no` withdraws a `yes` already given (it is not a
  vote against). Repeating the election command aborts it. Only one election at a time ("An
  election is already in progress"). An election times out after 60 seconds; after an abort
  the same player must wait 30 seconds before starting another ("Wait <n> second(s)!").
- **Map votes** never time out: they stay until the map changes or the vote passes. They cannot
  be cast in the first 7 seconds of a map (15 in matchless mode). With `k_lockmap` set, only an
  admin may vote ("MAP IS LOCKED! You are NOT allowed to change!").
- **When it is checked**: after every vote is cast, and every server frame (from the eleventh
  frame of a map on) for all open votes.
- **Lines everyone sees** (text in angle brackets is a value; words named "coloured" are in
  red high-bit characters):
  - Toggling votes: `<name> <coloured: votes for antilag on>!` followed by ` (<n>)` when more
   votes are still needed (n = how many more); `<name> <coloured: withdraws his antilag
   vote>!` ("her" when the player's `gender` userinfo is "f"). Same pattern with "nospecs on/off",
   "teamoverlay on/off", "coop on/off", "rpickup". Pickup is different: `<name> says pickup!
   (<n>)` / `<name> says no pickup`.
  - Break: `<name> votes for stopping the match (<n>)` ("votes for next map" in matchless) and
   `<name> withdraws his vote (<n>)`.
  - Passing: `Antilag mode on by majority vote`, `Teamoverlay on by majority vote`, `Coop mode
   on by majority vote`, `No spectators mode on by majority vote`, `hook style set to smooth by
   majority vote` (each also "by admin veto"), `Match stopped by majority vote`, `Majority
   votes for mapchange.`, `console: a pickup game it is then`, `Majority vote for Swapall`.
  - Map vote calls: `<name> suggests map <map>` (first vote), `<name> agrees on map <map>`
   ("to" instead of "on" with fewer than 3 players), `<name> would rather play on <map>`.
  - Elections: `<name> has requested admin rights!`, then to every other player `Type yes in
   console to approve`, and to the candidate `Type elect to abort election`; each yes
   `<name> gives his vote` then `<n> more vote(s) needed` (n in high-bit brackets); passing
   admin `<name> gains admins status!`; timeout `The voting has timed out.` and `Election
   aborted`; abort `<name> aborts election!`.
  - Failing: KTX has no "vote failed" line for toggling votes or map votes; they simply stay
   open (**derived**: no failure path exists other than withdrawals, the map changing, or a
   match starting, which clears the stop vote).
- **Parseable?** **derived**: the lines are stable formats with the player's name first, but
  the key words are in high-bit characters, so a client must strip bit 7 before matching
  (e.g. "votes for antilag on!"). Player names can contain the same words, so matching should
  anchor on the line's end.
- **Which need a vote, which do not**: every rule in the first table of question 2 is a direct
  toggle by any one player in standby; only the items in the votes table need agreement.

### 4. A command that prints the current values

All **read**. There is no single machine-readable dump; three commands print settings:

- `status1` ("show server settings") prints labelled pairs, two per line, each label in red,
  padded to 14 columns: `Maxspeed` (number), `Deathmatch` (number), `Teamplay` (number),
  `Timelimit` (minutes), `Fraglimit`, `Powerups` ("on" when all four are on, "off" when none, else the letters of
  those on from q, p, r, s), `Discharge`
  (on/off), `Drop Quad` (on/off), `Drop Ring` (on/off), `Fair Backpacks` (off/on/lst = last
  fired / unk), `Drop Backpacks` (on/off), `spec info perm` (adm/all), `more spec info`
  (on/off), `teleteam` (on/off: frags taken for team telefrags), `Berzerk` (on/off). The pairs
  share lines in this order: Maxspeed alone; Deathmatch with Teamplay; Timelimit with
  Fraglimit; Powerups with Discharge; Drop Quad with Drop Ring; Fair Backpacks with Drop
  Backpacks; spec info perm with more spec info; teleteam alone; Berzerk alone. Then, by
  state: "The match will start in <n> second(s)", "<Admin> election in progress: <votes>/<needed>
  votes", "Team picking in progress", "1 captain present", "1 coach present", "<type> overtime
  in progress" or "Match in progress" plus "<n> full minute(s) left".
- `status2` ("more server settings") prints one setting per line as `<red label>: <value>`:
  first the spawn model's name alone (red); `Server mode` (duel, FFA, CTF, team, unknown); `Server locking` (off/team/all, team and CTF modes only); `CTF settings: hook on/off, runes
  on/off, ga on/off` (CTF only); `Teaminfo (cur: <n> min: <n> max: <n>)` in standby; `Spectalk` (on/off); `Overtime` ("off", "<n> minute(s)", "sudden death", "<n> tie-break",
  "golden frag"); `QiZmo lag`, `QiZmo timers`, `QiZmo enemy reporting`, `QiZmo pointing` (on/off,
  from fpd bits 8, 2, 32, 128); `Admin election` (allowed/disallowed); `Check frametimes`
  (enabled/disabled); `Prewar` ("players may not fire before match", "players may fire before
  match", "players may fire and jump when ready"); a temporary lock line when set; `Command
  floodprot` (off, or "<n> commands allowed per <n> sec., skip commands for <n> sec., " then
  "kick after <n> warn." or "cmdfp kick disabled").
- `whovote` prints the open votes with counts "<votes>/<needed>" and the voters' names.
- `rules` prints a fixed paragraph per mode ("Server is in duel mode." and so on).
- `about` prints a framed panel: the server's name, version, build and date, then the mod's
  name ("KTX: Kombat Teams eXtreme"), version, build, date and web page, each line
  `<red label>....: <value>` right-aligned in 28 columns, with digit runs in gold digits.
- `options` prints only the list of match control commands and what each does, not values.
- `gamemodes` prints, one per line, the mode words this build knows: race, 1on1, 2on2, 3on3,
  4on4, 10on10, ffa, ctf, hoonymode, blitz2v2, blitz4v4, 2on2on2, 3on3on3, 4on4on4, XonX,
  wipeout, carena, tot, practice, midair, instagib, berzerk, lgcmode, arena, yawnmode,
  totmode (order: KTX's command order, **derived** from where each command is defined; it does
  not say which are allowed on this server).
- **derived** Parsing: labels and the words "on"/"off" are separate tokens, but labels are
  high-bit; strip bit 7, then split on the first run of spaces (status1) or the first ": "
  (status2). Values like deathmatch and timelimit in status1 are plain digits.

### 5. Does the server tell the client which commands it accepts?

All **read**:

- MVDSV sends no command list.
- KTX does: after a client enters, KTX stuffs `cmd cmdslist_dl 0` into the client, which
  answers by asking for batches; KTX then stuffs 15 aliases per request, each of the form
  `alias <command> cmd <three-digit number>`, with ` %0` appended when the command takes
  arguments and the client is ezQuake (detected from its version). A request for the next batch
  is stuffed after each batch. The client sees "Loading commands list..." at the start and
  "Commands loaded" at the end. Only commands valid for the client's role (player or
  spectator) and the server's mode (matchless or not) are sent; commands with no alias (`cm`,
  `info`, `uinfo`, `mapslist_dl`, `cmdslist_dl`) are left out. **derived**: the aliases are
  executed by the client and never printed, so a client learns the list only by watching
  stuffed text, not from a printed reply.
- KTX also stuffs `alias 1` .. `alias 32` as `impulse <n>`, and for players `notready`
  (= break), `kfjump` and `krjump`; for spectators `next_fav`.
- Map aliases: in the same way the client is sent one alias per map on the server's list,
  `alias <map> cmd cm <n>` (or `cmd votemap <map>` for some), so typing a map name votes for it.
- `commands [filter]` prints the commands for the caller's role as `<red name>.... <short
  description>`, first under "common commands for player:" and then "admin commands for player:"
  (or "spectator"), with a substring filter.
- A command may also be sent by its number: an all-digit command word is taken as the index
  into KTX's command list.

### 6. What happens to an unknown client command

All **read**:

- MVDSV first runs its own commands that the mod may not override (`new`, `spawn`, `begin`,
  `serverinfo`, `setinfo`, `download`, `pings`, `ptrack`, `pext`, and others), then hands any
  other command to the game module; if the module says it did not handle it, MVDSV replies to
  that client only: `Bad user command: <word>` (a high-priority print). Nobody else sees it.
- KTX claims, without a reply, every command whose word is ONE character long (it treats it as
  a registered weapon key; an unregistered one answers `unregistered wreg char - "<c>"` when the
  player has any registered), and every all-digit word that is a valid command number. Every
  other unknown word is passed back, so the client gets `Bad user command: <word>`.
- Known KTX commands that the caller may not use (wrong role, not allowed in matchless mode,
  too many commands too fast) are claimed and answer nothing or a short refusal ("You are not an
  admin", "command floodprot (<n> sec)", "You are a command flooder man!").
- **derived**: a new command word of two or more letters that is not all digits is safe to send
  to a stock MVDSV + KTX server: the only effect is one `Bad user command` line to the sender.
  Commands sent before the client is fully in the game are silently ignored by KTX.

### 7. Per-player settings without a vote

All **read**:

- Userinfo key `kf` (set by the client with `setinfo kf <n>`), a sum of bits: 1 = KTX sounds
  for team messages; 2 = automatic screenshot at match end; 32 = do NOT run the player's own
  `on_enter` alias on every map; 64 = show speed in prewar. KTX prints each change to the player,
  e.g. "auto screenshot: on".
- Userinfo key `ev`, event aliases KTX calls in the player's client: 1 = on_connect, 4 =
  on_matchstart, 8 = on_matchend, 16 = on_matchbreak, 128 = on_admin, 256 = on_unadmin (each
  change printed as "[on_matchstart] event: on" and so on). **derived**: automatic demo
  recording is left to the client: KTX runs the player's own match-start alias, it has no
  record command of its own (only race mode's `k_race_autorecord` server setting).
- Other userinfo keys KTX reads: `k_nick` or `k` (a short nick for team reports), `premsg`,
  `postmsg`, `k_sdir` (folder for team sounds), `mi` (spectator "more info"), `wpsx`, `di`,
  `nocolors`, `ti`, `lra`, `pbspeed`, `runes`.
- `kinfo <key> <value>` / `kuinfo` set and show such keys through the mod (`info` and `uinfo`
  are hidden aliases of the same).
- Per-player commands: `+wp_stats` / `-wp_stats` (weapon stats on screen), `+scores` /
  `-scores`, `sh_speed` (show own speed), `wreg` (register a weapon key), `mmode` and `multi`
  (message targets), `handicap` (own handicap level), `shownick`, `toggleready`, `socd`, the
  spectator camera commands (`autotrack`, `auto_pow`, `fav*`, `next_best`, `next_pow`), and the
  practice position commands (`pos_save`, `pos_move` and so on, only when cheats are allowed).
- `kfjump` and `krjump` are stuffed aliases (an impulse plus a jump), usable only when an admin
  allowed them (`tkfjump`, `tkrjump`).
- Team overlay is NOT per player: it is the `teamoverlay` vote.

## B. Quake II: Q2PRO + stock game library, and OpenTDM

### 8. Q2PRO server info keys

General rules (all **read**):

- The serverinfo string is built on demand from every setting marked "serverinfo", in the
  order the settings were created; empty values are left out (a "0" is NOT left out, unlike
  MVDSV); every character has bit 7 cleared and non-printing characters dropped; the string
  stops before 512 bytes (keys and values each at most 64 characters).
- A Quake II client is NOT sent the serverinfo while connected; it must ask (question 9).
- The out-of-band `status` reply (when `sv_status_show` is 1 or 2, default 2; rate-limited
  to 15 a second by default) is a connectionless "print" packet: the serverinfo line, plus
  `\uptime\<time>` when `sv_uptime` is 1 (compact) or 2 (verbose), then (when 2) one line per
  client: `<frags> <ping> "<name>"`. The `maxclients` value in it is the number of public slots
  (reserved slots hidden). The out-of-band `info <protocol>` reply is one line: hostname
  padded to 16, map padded to 8, then `<players>/<public slots>`.

Keys (all **read**; first value = Q2PRO's default before any config):

| Key | Meaning |
| --- | --- |
| `hostname` | server name, default "noname" |
| `version` | the engine's version string ("q2pro" followed by version, date, build and CPU) |
| `protocol` | 34 for the classic protocol, 36 when the extended protocol is in use |
| `port` | the server's port |
| `mapname` | the current map |
| `game` | the mod directory being run (default empty, so absent, for the base game) |
| `gamedir` | the active game directory |
| `gamename` | the game library's name: "baseq2" for the stock library, "OpenTDM" for OpenTDM |
| `gamedate` | the game library's build date |
| `deathmatch` | Q2PRO's default 1 (the stock library's own default would be 0); not changeable after the map starts |
| `cheats` | 0 or 1; latched |
| `maxclients` | default 8 (the stock library would say 4); latched |
| `maxspectators` | stock library: default 4 |
| `dmflags` | bit set, default 16 (instant items) |
| `fraglimit` | frags, default 0 |
| `timelimit` | minutes, default 0 |
| `needpass` | stock library: 0, or 1 = player password, 2 = spectator password, 3 = both |
| `anticheat` | present only when the anticheat service is connected: the required level |
| `uptime` | status reply only, see above |

`coop` is deliberately NOT in serverinfo. **read**

`dmflags` bits (meaning fixed by the stock game library: read from what the library does
with each bit; the numbering is the public dmflags value an operator types):

- 1 no health items
- 2 no items (powerups)
- 4 weapons stay
- 8 no falling damage
- 16 instant items (powerups used at pickup)
- 32 same level (the map does not change at the limit)
- 64 teams by skin
- 128 teams by model
- 256 no friendly fire
- 512 spawn farthest
- 1024 force respawn
- 2048 no armour
- 4096 allow exit (an exit ends the level in deathmatch)
- 8192 infinite ammo
- 16384 quad drop
- 32768 fixed field of view
- 65536 quad-fire drop, 131072 no mines, 262144 no double stacking, 524288 no nukes, 1048576 no
  spheres: these five are named by the shared header for the mission packs and are NOT used by
  the stock library (**read**: nothing in it tests them).

### 9. Asking a Q2 server for settings

All **read**:

- `info` (client command, handled by Q2PRO itself): prints the serverinfo as one line per key,
  key padded to 20 columns, a space, then the value.
- `sinfo` prints the caller's own connection facts (version, protocol, packet size, ping,
  movement rate, packet loss, connection time); not settings.
- Stock game `players`: one line per player sorted by frags, `<frags right-aligned in 3>
  <name>`, then an empty line and `<n> players`.
- Stock game `playerlist`: one line per client, `<mm>:<ss> <ping in 4> <score in 3> <name>`
  plus " (spectator)" for spectators, where mm:ss is time since joining.
- Stock game `score` and `help` show the scoreboard and the help computer; neither prints
  settings.
- There is no client command for the game's settings other than `info`.

### 10. OpenTDM

All **read** unless marked:

- **Serverinfo keys added**: `time_remaining` ("WARMUP" before the match, then `m:ss` left,
  "N/A" when unknown), `match_type` ("TDM", "ITDM" = instagib TDM, "Duel", "N/A"), `Score_A`
  and `Score_B` (team scores, "WARMUP" before the match), `revision` (the build's revision
  number, when built with one). `gamename` is "OpenTDM". OpenTDM keeps `dmflags` set from its
  per-mode flag settings (`g_tdmflags`, default 1040; `g_itdmflags`, default 142427; `g_1v1flags`, default 1040). Its match length is its own setting `g_match_time` (seconds,
  default 600), so the stock `timelimit` key stays 0 (**derived**: OpenTDM changes the match
  length without touching `timelimit`). The stock library's `maxspectators` and `needpass` keys
  are not created by OpenTDM.
- **Voting**: `vote <setting> <value>` proposes; `yes` / `no` (or `vote yes` / `vote no`) answer; `vote cancel` lets the proposer or an admin cancel ("<name> cancelled the vote"; the fork
  only). Only team players and admins may propose or vote ("You must be on a team to vote or
  propose new settings."). `yes` and `no` from anyone else, or with no vote open, are spoken as
  chat. Proposals are allowed only in warmup ("You can only propose new settings during
  warmup."), except `timelimit`/`tl`, `restart`, `kick`, `abort` which also work during a match
  ("You can only vote for a timelimit change or restart during a match."). Votes can be cast
  during a timeout. One vote at a time ("Another vote is already in progress."); the proposer
  may replace it, which prints "Vote canceled!". A proposer whose last vote failed must wait 10
  seconds ("You must wait a short time before proposing another vote."). A server setting
  (`g_vote_mask`, default all) can forbid each kind ("Voting for <thing> is not allowed on this
  server.").
- **Share and time**: the proposer's vote counts as yes. After every vote, among team players
  and admins: passes at once when yes > no + undecided; fails at once when no > yes + undecided.
  After `g_vote_time` seconds (default 30) it passes if yes > no among those who voted, else
  fails. An admin's vote decides at once when `g_admin_vote_decide` is set (default 1). Votes
  can be changed (`<name> changed his vote to NO.`); each player is rate-limited to one vote
  every 2 seconds.
- **Lines**: `<name> started a vote: <what>` (e.g. "timelimit 15", "map q2dm1", "kick <name>",
  "weapons -bfg", "powerups -quad", "mode TDM", "overtime enabled"); `<name> voted YES.` /
  `<name> voted NO.`; `Vote passed!`; `Vote failed.`; `Vote canceled.` (kick target left). An
  optional sound plays at a call (`g_vote_attention`, default off). The HUD shows a line through
  a configstring, refreshed every second: `Vote: <what>. Yes: <yes> (<needed>) No: <no> [<seconds
  left>]`, or `Vote: type 'vote' to see changes. Yes: ...` when longer than 63 characters; needed is half the team players plus one (even count) or half rounded up (odd count).
  **derived**: these lines are plain text (no high bit) and stable, so a client can parse them.
- **Settings by vote** (proposal words and accepted values): `timelimit`/`tl <1..1440 minutes>`; `map <name>` (no name lists the map list); `kick <name or id>`; `powerups <+/-item ...>` with
  `quad`, `invul`, `ps`, `powerscreen`, `silencer`, `rebreather`, `envirosuit`, `all`, or `1`/`0`; `weapons <+/-weapon ...>` with `shot`/`sg`, `sup`/`ssg`, `mac`/`mg`, `cha`/`cg`, `han`/`hg`,
  `gre`/`gl`, `roc`/`rl`, `hyper`/`hb`, `rail`/`rg`, `bfg`/`10k`, `all`; `gamemode`/`mode`
  `tdm`/`itdm`/`1v1` (or `duel`); `tiemode` `none`/`tie`/`normal`, `ot`/`overtime`, `sd`/`sudden
  death`; `telemode` `normal`/`freeze`/`q2` or `nofreeze`/`q3`; `switchmode`
  `normal`/`slow`/`default`/`q2`, `fast`/`faster`, `instant`, `insane`, `extreme`; `overtime`/`ot <1..1440 minutes>`; `config <name>` and `webconfig <name>`; `chat` `players`
  (also `team`, `nospec`, `whisper`, `1`) or `all` (also `everyone`, `speak`, `0`); `restart`; `abort`; `bugs <0/1/2>`. The fork adds `shuffle` (`randomize`, `mix`), `armortimer <0/1>`,
  `weapontimer <0/1>`, `timeoutlimit <0..100>` (0 = unlimited), `timeoutcaptain <0/1>`,
  `smartmap [players per team]`, `swap <id> <id>`. The in-game menu (`menu`) offers a voting
  page for the same kinds.
- **Listing**: `vote` with no argument prints the usage list of every setting and its values
  (one per line under " Options:"), or, while a vote is open, `Vote options: <what>.`
  `settings` (or `matchinfo`) prints the current settings, label then value, values in green
  (high bit), one per line, in groups separated by blank lines: `Game mode:` (Team Deathmatch,
  Instagib Team Deathmatch, 1 vs 1 duel); `Timelimit:` (<n> minute(s)); `Overtime:` (disabled,
  <n> minute(s), Sudden death); `Removed weapons:` (short weapon names); `Removed powerups:`; `Armor timer:` and `Weapon timer:` (enabled/disabled); `'<team>' skin:` for each team; `Weapon switch:` (normal, faster, instant, insane, extreme); `Teleporter mode:` (normal, no
  freeze); `Gameplay bugs:` (all / serious / no gameplay bugs fixed); `Timeouts:` (captains only,
  all players); `Timeout limits:` (<n> per player, unlimited); then `1v1 Respawn mode:` or `TDM
  Respawn mode:` (avoid closest (bugged), avoid closest, random, plus " (random on small maps)"
  for 1v1). The same text is broadcast as "Match Settings:" when a match starts. Edge case: when
  overtime is disabled its value has no newline of its own, so the blank line that normally
  follows is missing (**derived** from the order the lines are built). `commands` prints a fixed
  help text; `timelimit`, `overtime`, `powerups`, `bfg`, `obsmode` show single settings (and
  change them for admins).
- **Unknown commands**: OpenTDM passes unknown words to chat, like the stock library, except
  during a timeout or the end scoreboard, when they are swallowed silently. **read**
- Per-player toggles: `weaponhud`/`hud`, `autoscreenshot`, `autorecord`, `ignorespecs`,
  `ignoreplayers`, `ignoreall`, `id`, `playerconfig`. **read**

### 11. Votes in the stock Quake II game library

**read** Confirmed: the stock game library has no vote and no command that changes a server
setting. Its only commands are movement and inventory ones, `players`, `playerlist`, `score`,
`help`, `say`, `say_team`, `wave`, `kill`, `putaway` and the cheats. Q2PRO's server adds no vote
either. **read** Important edge case: in the stock library (and in OpenTDM outside timeouts) ANY
command word it does not know is broadcast to everyone as chat, `<name>: <word> <arguments>`
(cut to 150 characters, subject to flood protection). **derived**: a client must never send an
unknown command (for example a settings query of our own) to a stock Quake II server, or every
player sees it as a chat line from that player.
