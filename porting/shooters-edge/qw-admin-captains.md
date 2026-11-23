# QuakeWorld server mod: admins, referees and captains (clean-room facts)

This is a WORKING-AREA note of facts about the QuakeWorld competitive server mod's admins,
referees and captains, as players and server admins meet them: how one becomes an admin (code,
election), what an admin may do to the match and to players, which of those players may do by
vote, captain elections and team picking, team names, team and server locks, and coaches. It was
written by a clean-room reader from KTX (QW-Group, main branch at commit b1b23a98, 2026-11-08)
and its example server configs. It contains no code: only behaviour, defaults, limits, orders of
events and the names and messages a player or admin sees. Each fact is marked **[read]** (seen
in the source), **[derived]** (concluded from what was read) or **[open]** (not settled by the
source). Nothing was measured on a running server.

Conventions for the messages below: words in *italics* are printed in the highlighted (red/gold)
character set; `[x]` stands for the gold bracket glyphs the mod puts round numbers and team
names; `<name>` is the player's name. "Broadcast" means every client reads it; "to the player"
means only that client.

## Becoming an admin

### 1. Commands and the password

- The command is `admin` (works for players and spectators, also in matchless mode) [read].
  It is a toggle: typed by an admin it gives the rights up (see 2) [read].
- Two ways to give the password [read]:
  - As an argument: `admin <code>`. The text must equal the setting `k_admincode` exactly
   (case-sensitive string comparison). If `k_admincode` is empty or the word `none`, the
   argument form never succeeds [read].
  - With no argument: the mod enters a code-entry state and the player types the code as six
   digits on the number keys (impulses 1 to 9), one key per digit, most significant first
   [read]. The six digits are put together as a number and compared with `k_admincode` read
   as a number [read]. Consequences [derived]: in this way the code must be a six-digit
   number with no digit 0 (0 cannot be typed, only impulses 1 to 9 are taken); an empty code
   reads as 0 and never matches. While the state is active the number keys do not change
   weapon [read].
- Settings [read]:
  - `k_admins`: admins allowed at all (0 = no, 1 = yes). The example config sets 1; with 0 both
   `admin` and `elect` refuse.
  - `k_admincode`: the admin code. The example config sets it empty ("disabled").
  - `k_allowvoteadmin`: allow election of an admin by vote (0/1); example config 1.
  - The mod registers these with no built-in default; an unset setting reads as 0 / empty
   [derived], so a server that does not exec a config has no admins.
- Other ways [read]: there is a "VIP" mechanism (VIP with admin rights becomes admin by typing
  `admin` without a code; a "not kickable" VIP; a VIP who may stay when spectators are kicked),
  but in this release every client's VIP flags are hard-wired to none, so it is inert [read].
  There is no address list or key file for admins in the mod [read]. Admin rights can also be
  won by election (question 3). The server's own rcon is separate and not part of this.

### 2. Messages, rate limit, persistence

All read.

- `admin` when `k_admins` is 0: to the player "*NO admins* on this server!"
- `admin` with no argument: to the player "Use *numbers* or *impulses* to enter code". After
  each of the first five digits: "`N` *more to go*" (5, 4, 3, 2, 1).
- `admin` again while entering digits: "*admin* code canceled".
- `admin` while the same player has an admin election running: "Abort *elect*ion first".
- Wrong code (either way): to the player "*Access denied*...". There is no broadcast.
- Rate limit: after a wrong code, a new attempt within 5 seconds is refused with
  "Wait `N` second(s)!" (singular "second" when N is 1; the count is the seconds left,
  rounded). The limit is per player and applies to both entry ways. There is no lock-out after
  many attempts [read].
- Success (by code, VIP, or election): broadcast "`<name>` *gains admins status*!"; to the new
  admin "Please give up admin rights when you're done" and "Type *commands* for info".
  Becoming admin clears every vote the player had cast [read].
- Giving it up (`admin` typed by an admin, real or elected): broadcast "`<name>` is no longer an
  *admin*". If the admin was in kick mode, kick mode ends too ("Kicking process terminated").
  All admin rights (real and elected) are removed at once.
- A client can ask to be told: if the player's `ev` info key has the bit 128, the mod runs the
  client alias `on_admin` when they become admin; bit 256 runs `on_unadmin` when they stop.
- Persistence [read, derived]: admin status is carried across a map change (for players and
  spectators) in the per-client data the server keeps between levels. On a fresh connection
  (a disconnect and reconnect) that data is cleared, so the player must log in again. Admin
  status is not stored on disk.
- "You are not an admin" is what a non-admin reads when they type an admin-only command
  [read].

### 3. Levels and electing an admin

- Two levels [read]:
  - **Real admin**: by code (or VIP). Shown as "real admin" in messages.
  - **Elected admin**: by vote. Can do everything a real admin can except: kick or force to
   spectator a real admin (or a not-kickable VIP); use commands whose permission setting is
   "real admins only" (1, see below); survive `nospecs` as a spectator (real admins are
   spared, elected ones are not).
  - Permission settings with the scale 0 = no one, 1 = real admins only, 2 = any admin
   (real or elected), 3 and 4 = "judges" (not implemented: refused with "*judges* is not
   implemented in this mode"), 5 = everyone [read]. Refusals: "*no one* can use this command",
   "you must be a *real admin*", "you must be an *admin*", or "server is misconfigured,
   command *skipped*" for a value out of range. Settings on this scale: `k_free_mode`
   (who may change the game mode: `1on1`, `2on2`, `4on4`, `ffa`, `ctf`, ...; example
   config 5), `k_allowcountchange` (who may raise/lower `maxclients`/`maxspectators`; each
   game-mode command sets it to 1), `k_ip_list` (who may list everyone's IP address with
   `iplist`; each mode command sets 1; others see only their own IP), `allow_toggle_practice`
   (practice mode; example 5) [read].
  - There is no separate referee or judge role [read].
- Election, command `elect` (players and spectators) [read]:
  - Refused: while entering a code ("Finish entering the code first"); already admin ("You are
   already an admin"); any election already running on the server ("An election is already in
   progress"; only one election of any kind — admin, captain, coach, colour suggestion,
   late join — at a time); `k_admins` 0 ("*NO admins* on this server!"); `k_allowvoteadmin` 0
   ("Admin election is not allowed on this server"); within the block time after an earlier
   election of theirs ended ("Wait `N` second(s)!"); a spectator while a match is in progress
   (silently ignored).
  - Start: broadcast "`<name>` has *requested admin* rights!"; to every other player (not
   spectators) "Type *yes* in console to approve"; to the candidate "Type *elect* to abort
   election". The candidate's own vote is counted from the start [read].
  - Voting: `yes` (players only; spectators cannot vote) — broadcast "`<name>` gives his vote"
   (the possessive follows the player's gender setting) [read], then "`[N]` more vote(s)
   needed" while short. A second `yes`: "--- your vote is still good ---". The candidate
   typing `yes`: "You cannot vote for yourself". `no` withdraws a cast vote: broadcast
   "`<name>` withdraws his vote" and the count again. `no` without a cast vote does nothing.
  - Votes needed: the round-up of (`k_vp_admin` percent × number of human players on the
   server, spectators and bots not counted), at least 2 [read]. `k_vp_admin` is clamped to
   51..100 percent; example config 51 [read]. So with 2 humans both must agree; with 3, 2; with
   4, 3 [derived].
  - Timeout: 60 seconds after the start, broadcast "The voting has timed out." and
   "Election aborted" [read].
  - Abort: the candidate types `elect` again — broadcast "`<name>` *aborts election*!". If the
   candidate disconnects or the map changes the election is dropped [read].
  - When an election ends (success, timeout or abort) the candidate cannot start another one
   for 30 seconds [read].
  - Success: the "gains admins status" messages of question 2, with the elected level. A
   spectator candidate is not made admin if a match started meanwhile [read].

## What an admin can do

### 4. Admin commands

How commands are gated [read]: every mod command is marked for players, spectators or both,
and separately "needs admin when typed by a player" and/or "needs admin when typed by a
spectator". Most match settings (time, frags, modes, lockmode, spectator talk, ...) need **no**
admin when a player types them, only when a spectator does: on a KTX server any player may
change them before the match without a vote. The commands below need admin from anyone unless
noted. Players are named by user id (the number in the server's user list) or by exact,
case-sensitive name; ids are tried first.

- **`forcestart`** — start the match without everyone ready [read].
  - Silently ignored during a match, after it, or for a non-admin. Refused in practice mode
   ("*Server in practice mode*"). An admin who is a player and not ready is made ready first; if
   that fails, "Ready yourself first". If one is already running: "forcestart already in
   progress!". If the start rules fail (teams, members — see 8): the reason, then "Can't
   issue!". With no players: "Can't issue! More players needed."
  - Then broadcast "`<name>` forces matchstart!", the server's status info key reads
   `Forcestart`, and a 10-second count runs, centre-printing "`N` second(s) left before game
   starts" each second, with "*Get ready*!" added for players not yet ready [read].
  - Each second the start rules are re-checked; if they fail: broadcast "Forcestart canceled".
   If the admin who issued it (as a player) un-readies, or a spectator admin uses
   `forcebreak`: broadcast "`<name>` interrupts countdown" [read].
  - At the end, **every player still not ready is disconnected**: broadcast "`<name>` was kicked
   by admin forcestart", to them "Bye bye! Pay attention next time." Then the normal match
   countdown starts (`k_count`, example 10 s) [read]. If no ready player is left:
   "Can't start! More players needed." [read].
- **`forcebreak`** — stop the match [read]. Before a match, a spectator admin's `forcebreak`
  cancels a running forcestart. In the countdown a spectator admin stops it. During the
  match: broadcast "`<name>` forces a break!" and the match ends unfinished. Silently ignored
  for non-admins or with no match. (A player's own `break` is a vote — question 5.)
- **`forcemap <map>`** — change map now [read]. Refused during a match (except in matchless
  mode): "Match currently in progress. Use *break* or *forcebreak* to terminate."; no argument:
  "Usage: forcemap *<mapname>*"; a map not in the server's map list: "Map *`<map>`* not
  available on this server." Success: broadcast "`<name>` forces a map change to *`<map>`*!".
- **`kick`** [read]. With an argument `kick <id or name> [reason...]`: kicks that player,
  spectator, or a client still connecting. Not found: "kick: client `<x>` not found". The
  reason, if given, is broadcast in gold brackets after the kick line. Without an argument it
  enters **kick mode**: "Kicking process started", a separator line, "Type *y* to kick, *n* for
  next, *kick* to leave"; the mod then walks through every player and spectator asking
  "Kick *player*/*spectator* `<name>`?"; `y` kicks the one shown and moves on, `n` skips; `kick`
  again leaves ("Kicking process terminated"). Kick mode times out after 60 seconds
  ("Your *kick* mode has timed out") and ends if the admin loses admin rights or leaves. Nobody
  to kick: "Can't find anybody to kick". Typing `kick` with an argument while in kick mode
  just leaves kick mode.
  - The kick: broadcast "`<victim>` was kicked by `<admin>`", to the victim "You were kicked
   from the server", the victim is disconnected, and the server bans their address for 30
   seconds so they cannot come straight back [read].
  - Kicking oneself: broadcast "`<name>` kicked himself", to them "Say "bye" and then type
   "disconnect" next time", same 30-second ban [read].
  - An elected admin kicking a real admin: "You can't kick real admin » `<name>` as elected
   admin" (the glyph is an arrow); no kick [read].
  - Allowed during a match [read].
- **`mkick <id> [id ...] [reason...]`** — kick several by user id (numbers only; the first
  non-number starts the reason) [read]. Usage line: "mkick <id1 [id2 [id3 ...]] [reason]>".
  An id not found: "mkick: client `<id>` not found", the rest go on.
- **`ban`, `banip`, `banrem`** — listed in the mod's command help ("timed ban by uid/nick",
  "timed ban by ip", "remove ban / banlist") but the mod only hands them to the server
  program's own commands of the same name [read]. Their arguments, durations and messages
  are the server's, not settled by the mod [open].
- **`force_spec [<id or name> | -<spectator id> | *]`** [read]. A player named is made to
  reconnect as a spectator; a spectator named (or a negative number, a spectator's user id)
  is made to reconnect as a player; `*` moves every not-ready player (except the admin) to
  spectator. With no argument it uses the admin's `fs` info key; if that is empty it prints a
  help text explaining `setinfo fs "*"`, `setinfo fs "playername"` and `/force_spec
  "playername"`. The target reads "You were forced to reconnect as spectator/player by the
  admin" and their client is told to reconnect. Not found: "can't find specified players".
  Elected admins cannot do this to real admins ("You can't force_spec real admin » ... as
  elected admin"). Allowed during a match.
- **`lock`** — temporary server lock [read]. Broadcast "`<name>` *locked server* for 15
  seconds"; during those 15 seconds every connecting player is refused with "*server is
  temporary locked*: `N` second(s)". `lock` again: "`<name>` *unlocked server*". When the time
  runs out: broadcast "*server unlocked*". Allowed during a match.
- **`lockmap`** — toggle `k_lockmap` (example config 0) [read]. When on, non-admins' map
  votes are refused: "MAP IS LOCKED!" / "You are NOT allowed to change!". Messages: before a
  match broadcast "`<name>` locks map" / "`<name>` unlocks map"; during one, only to the admin
  "Map is locked" / "Map unlocked". (With `k_lockmap` on, an empty server also does not fall
  back to its home map [read].)
- **`prewar`** — cycle `k_prewar` 0 → 1 → 2 → 0 (example config 1) [read]. 1: "Players may fire
  before match"; 2: "Players may fire and jump when *ready*"; 0: "Players may *not* fire before
  match". Broadcast before a match, only to the admin during one; at 2 and 0 everybody's fire
  button is released.
- **`exclusive`** — toggle `k_exclusive` (example config 1): when on, once a match has
  started no more players may join than took part at the start ("Sorry, server is full" /
  "Please reconnect as spectator") [read]. Ignored during a match. Note: needs admin from
  players and spectators.
- **`infolock`** — toggle whether only admins (rather than all spectators) receive the extra
  spectator info ("Only *admins* can receive specinfos" / "All *spectators* can receive
  specinfos"); ignored during a match [read].
- **`tkfjump`, `tkrjump`, `hdptoggle`, `fp`, `fp_spec`, `noga`, `mctf`, `race_break_all`** —
  further admin-only toggles of game rules (team quad-jumps, handicap lock, flood protection,
  CTF options, race) [read]; not part of the referee role proper.
- **Time and frag limit** — `timeup`/`timedown` (±5 min; going up from 0 goes 0 → 1 → 3 → 5,
  then +5), `timeup1`/`timedown1` (±1 min), `time5`…`time30` (set), `fragsup`/`fragsdown`
  (±10; down from 1 goes to 0) [read]. Limits: time 0 to `k_timetop` minutes (example 30); frags
  1 to 100 (0 is reachable only by stepping down from 1); time and frags cannot both
  be 0 ("You need some timelimit or fraglimit at least") [read]. Messages: broadcast "*Match
  length set to* `N` *minute(s)*", "*Fraglimit set to* `N`", or to the player "*timelimit*
  still `N`" when unchanged. Ignored during a match. **Any player** may use these; a spectator
  needs admin [read].
- **`upplayers` / `downplayers` / `upspecs` / `downspecs`** — `maxclients` / `maxspectators` ±1,
  between 1 and `k_maxclients` / `k_maxspectators` (each game-mode command sets
  `k_maxspectators` 4); gated by `k_allowcountchange` (question 3); broadcast "`<name>` set
  *maxclients* to `N`"; at the cap: "*maxclients* reached". Ignored during a match [read].
- **`pause`** — any player during the match (and an admin spectator) [read]. Refused when the
  server's `pausable` is off, unless the one asking is an admin: admins may pause even then.
  The pause takes effect 3 seconds later; unpausing is announced "`<name>` unpaused the game
  (will resume in 3 seconds)"; "Too late to pause. Please wait for match to finish." with 3
  seconds or less left.
- **Mute**: the mod has no mute command [read]. **Ready-all**: there is none; `forcestart` is
  the tool [read]. **Forcing teams**: no direct "put player X on team Y" command [read]; the
  tools are `force_spec`, captains (6), and the team votes of question 5.
- Admins' votes also act as a **veto** that passes at once: if any admin votes for
  `pickup`, `rpickup`, `nospecs`, `teamoverlay`, `votecoop`, a hook style or `antilag`, it
  passes immediately, announced as "... by admin veto" (e.g. "console: admin veto for
  pickup", "No spectators mode on by admin veto") [read]. In a map vote, the map an admin voted
  for is the one counted, but it still needs the normal number of votes [read].
- The command list for the person's own class and level is shown by `commands` [read].

### 5. The same by vote for ordinary players

All read unless marked.

- Common rule: votes needed = round-up of (percent × human players), percent clamped to 51..100; spectators
  and bots are not counted and cannot vote. If the only human is with bots, 1 vote
  suffices (not for elections). Percent settings (all 51 in the example config): `k_vp_break`,
  `k_vp_map`, `k_vp_admin`, `k_vp_captain`, `k_vp_coach`, `k_vp_pickup`, `k_vp_rpickup`,
  `k_vp_nospecs`, `k_vp_teamoverlay`, `k_vp_coop`, `k_vp_hookstyle`, `k_vp_antilag`,
  `k_vp_suggestcolor`, `k_vp_privategame`.
- Stop the match: `break` (alias `notready`) during the match is a vote — broadcast
  "`<name>` *votes for stopping the match* (`N`)" with N still needed; again withdraws:
  "`<name>` *withdraws* *his* vote (`N`)". Passes with "*Match stopped by majority vote*". At
  least 1 vote. In the countdown, any ready player's `break` simply stops the countdown:
  "`<name>` *stops the countdown*". Before the match it just un-readies: "`<name>` *is not
  ready*".
- Change map: `votemap <map>` or simply typing the map's name; not during a match; not in the
  first 7 seconds of a map (15 in matchless mode): "Wait N second(s)!". Messages: "`<name>`
  *suggests map* `<map>`", "`<name>` *agrees on map* `<map>`" ("to" instead of "on" with fewer
  than 3 players), "`<name>` *would rather play on* `<map>`"; passes with "*Majority* votes for
  mapchange." Unknown map: "Map '`<map>`' not available on this server".
- Becoming admin: `elect` (question 3).
- Kick, ban, force to spectator, server lock, prewar, map lock, forcestart: no vote form
  [read]. Spectators can be removed by vote with `nospecs` (below).
- `nospecs` — toggles "no spectators" mode before a match; needs at least 2 players to start a
  vote (an admin may do it alone); at least 2 votes (1 to turn it off); when it turns on every
  spectator is disconnected except real admins and coaches. "`<name>` *votes for nospecs on*!",
  "*No spectators mode on by majority vote*". It turns itself off when the server empties.
- `pickup` — everybody goes back to no team, colour 0 and the base skin, un-readied: "console:
  a pickup game it is then". Refused during captain picking ("No pickup when captain
  stuffing").
- `rpickup` — random teams: at least 4 players and at least 3 votes; players are dealt at
  random into teams red (colour 4) and blue (13) (and yell, colour 12, in three-team modes); if
  the deal changes nobody's team it re-deals up to 3 more times ("console: Team layout did
  not change. *random pickup* again (tries left: N)"). Refused during captain or coach picking.
- `swapall` (CTF), `teamoverlay`, `votecoop`, the hook styles, `antilag`, `voteprivate`,
  `suggestcolor`: further votes of the same shape.
- Settings changes (time, frags, modes, lockmode, spectator talk, powerups, ...): no vote at
  all; any player may change them before the match (question 4), within the `k_free_mode`
  permission for game modes.

## Captains and teams

### 6. Captain mode

All read unless marked.

- Started by election: a player types `captain`. Refused: during a match or intermission
  (silently); not a team or CTF mode ("No team picking in non team mode"); fewer than 3
  players ("Not enough players present"); two captains already ("Only 2 captains are
  allowed"); another election running ("An election is already in progress"); in the 30-second
  block ("Wait N second(s)!"); no team set ("Set your team name first"); in CTF a team other
  than red or blue ("Must be team red or blue for ctf"; for red/blue the colour is forced to 4
  or 13); same team name as the existing captain ("A *captain* with team `[team]` already
  exists" / "Choose a new team name"); same top and bottom colours as the existing captain
  ("A *captain* with your color already exists" / "Change to new color").
- The vote is the election of question 3 with `k_vp_captain`: broadcast "`<name>` has
  *requested captain* status!", others "Type *yes* in console to approve", candidate "Type
  *captain* to abort election"; 60-second timeout; at least 2 votes. Success: "`<name>` becomes
  a *captain*"; with only one: "One more *captain* should be elected". A candidate or captain
  may not change team ("You may *not* change team").
- When the second captain is elected: if fewer than 3 players remain, "Not enough players
  present" / "Captain modes exited". Otherwise broadcast "Both *captains* elected" / "Team
  picking begins". Every other player is un-readied and reset to no team, colour 0 and no skin,
  and gets a number 1, 2, 3, ... shown as their frag count on the scoreboard. Each captain
  reads "Use *numbers* or *impulses* to choose". Each captain's team name and colours are
  remembered as the team's.
- Who picks first is random (50/50). Broadcast "`<captain>` is picking". The captain presses
  the number key / `impulse N` of a player's number (numbers up to 32). Order of turns: first
  captain once, then the second captain twice, then alternating one each: A, B, B, A, B, A, ...
  Wrong captain: "It's *not* your turn"; no such number: "No such player. Pick another one".
- A pick: broadcast "`<player>` set to team `[team]`"; to the player "You were picked by
  `<captain>`" / "Time to go ready"; the player's team and colours are set to the captain's
  and their number cleared. A picked player cannot change to another team.
- When one unpicked player is left he goes to the captain whose turn it is, automatically.
  Then broadcast "All players chosen. Captain modes exited" and each captain reads "You are no
  longer a *captain*"; the captain role ends there (it does not last into the match).
- During picking: nobody can fire ("can't fire until in captains mode"), weapon-change keys
  do nothing [derived from the impulse handling], unpicked players cannot set a team
  ("You may *not* change team"), and `pickup`, `rpickup` and `swapall` are refused. A player
  who joins during picking gets the lowest free number. A player who leaves is skipped; if
  that leaves nobody unpicked, picking ends as above.
- A captain typing `captain` again resigns: "`<name>` is no longer a *captain*". A captain
  leaving the server: "A *captain* has left". Either way, during picking "Player picking
  aborted" and the numbers are cleared; then "`[1]` captain present" or "*No* captain present".
- Status: `status1` shows "*Team picking* in progress" or "`[1]` *captain* present" [read].

### 7. Team names

- There is no mod command to name a team. A team's name is the players' own `team` info key
  (`team <name>` in the client) [read]; a captain's team name becomes the team of everyone he
  picks.
- Refusals by the mod, each with the client's team put back [read]:
  - During a match or its countdown (and in co-op): "You may *not* change team during game".
  - A captain or captain candidate (and a coach or coach candidate): "You may *not* change
   team".
  - During captain picking, a player may only hold his captain's team (unpicked: none):
   "You may *not* change team".
  - A ready player in team or CTF mode setting an empty team: "Empty *team* not allowed".
  - A ready player in CTF setting a team other than red or blue: "You must be on team red or
   blue for CTF".
  - `ready` with no team in team, CTF or clan-arena modes: "Set your *team* before ready!".
- Length and character limits: the mod sets none [read]. Any limit comes from the server or
  the client [open].

### 8. Locking

All read unless marked.

- `lockmode` (setting `k_lockmode`, values 0, 1, 2; each game-mode command sets 1; one example
  per-mode config sets 0): the command cycles 0 → 1 → 2 → 0 before a match (ignored during one)
  and is open to **any player** (a spectator needs admin). Messages: "*Server locking* off"; "*Teamlock
  on* - only players in existing teams can connect during game"; "*Server locked* -
  players cannot connect during game".
  - 2: during a match no player may join: "Match in progress, server locked" / "Please
   reconnect as spectator".
  - 1: during a match, in duel or FFA only a returning player with the same name as one who
   left the match may come back ("Duel in progress, server locked" / "Match in progress,
   server locked" + "Please reconnect as spectator"); in team or CTF only a player whose team
   already has a player (or a player who left) may join, and only while that team is smaller
   than the largest team ("Set your team before connecting" / "or reconnect as spectator"; "Your
   team has already maximum player count" / "Reconnect as spectator").
  - 0: anybody may join during a match. With 0, players who leave during a match are also not
   kept for a return [read].
  - In every mode, during a team match a player with no team is refused ("Set your team before
   connecting"), and in CTF one not on red or blue.
- `exclusive` / `k_exclusive` (question 4): no more players than took part at the start.
- `lock` (admin, 15 seconds) and `lockmap` / `k_lockmap` (admin): question 4.
- Before a match nothing stops joining except the server's slots [read].
- Start rules that act like a lock on team shape: `k_lockmin` and `k_lockmax` (fewest and most
  teams; example config 0 and 32; each mode command sets 0 and 64 first, then the mode sets its
  own), `k_membercount` (fewest players in each team; set by the mode, e.g. 1 in 2on2, 2 or 3
  in larger modes). Refusals at start: "`N` more team(s) required!", "Get rid of `N` team(s)!",
  "*Server wants at least* `N` *players in each team*" / "*Waiting...*", "Get rid of players
  with empty team", and in duel "Get rid of `N` player(s)!".
- Status: `status2` prints "*Server locking*: off / team / all" and "*Teaminfo* (cur: N min: N
  max: N)".

### 9. Maximum players per team

- No setting [read]. During a match with team lock (1), a joining player is refused once his
  team has as many players as the largest team had when the start rules were last checked
  [derived]. The minimum per team is `k_membercount` (question 8). The overall caps are the
  server's `maxclients`/`maxspectators`, moved by `upplayers`/`downplayers` up to
  `k_maxclients`/`k_maxspectators` (question 4).

### 10. Other things a QuakeWorld player would miss

All read unless marked.

- **Coach**: a spectator types `coach` to request coach status for his team (same election
  as captain, `k_vp_coach`, 60 s, at least 2 votes; refusals as for captain: team mode only,
  3+ players, a team set, no other coach of the same team, at most 2 coaches). Messages:
  "`<name>` has *requested coach* status!", "`<name>` becomes a *coach*", "One more *coach*
  should be elected", "`<name>` is no longer a *coach*", "`[1]` coach present" / "*No* coach
  present". A coach can only track (follow) players of his own team: a switch to an enemy
  jumps to a team-mate, or, if there is none, "Found no suitable player to track". A coach
  cannot change team, survives `nospecs`, is shown with a "c" in the `klist` player list, and blocks
  `rpickup` and `swapall` ("No random pickup when coach stuffing"). A coach who resigns while
  `nospecs` is on is disconnected. Coach status carries across a map change like admin status.
  Quirk: the check that gives the second coach number 2 looks among players, not spectators,
  so both coaches may get the same number [derived]; it is not settled whether that changes
  anything players see [open].
- **Spectator talk**: `silence` toggles `k_spectalk` (example 0): whether players hear
  spectators during the game. Before the match any player may toggle it; during the match only
  an admin. Messages: "Spectalk on: *players can hear spectators during game*" /
  "Spectalk off: *players cannot hear spectators during game*"; during the match "Spectalk on:
  *players can now hear spectators*" / "Spectalk off: *players can no longer hear
  spectators*" (this also flips the server's spectator-talk setting at once).
- **Removing spectators**: `nospecs` vote (question 5) or `kick` on a spectator.
- **Kick mode** walk-through with `y`/`n` (question 4), unusual to other games.
- **Forcestart disconnects the unready** (question 4) — a strong referee tool QuakeWorld
  players expect.
- **Status**: `status1` shows any election in progress as "*Admin* election in progress:
  `[votes/needed]` vote(s)" (also Captain, Coach, Late Join, Suggest Color), the captain and
  coach states; `status2` shows server locking, prewar and the temporary lock's seconds; `iplist`
  marks admins with "A".
- **Idle bot**: `k_idletime` (example 0 = off) kicks players who stay not ready; not
  admin-driven [read; details not examined].
- **Practice mode**: `practice` (gated by `allow_toggle_practice`), during which forcestart is
  refused.

## Rows: what our QuakeWorld server has of these

The checklist for our own server (the engine's referee in QuakeWorld's words,
`games/shooters_edge/net/qw_referee.hpp`, held by `tests/shooters_edge_qw_referee_test.cpp`).
Statuses as in `q1-qw-ledger.md`: done, missing (rank 1 noticed at once, 2 sometimes, 3 rare),
not-taken (why named). Our own program runs these; a mod's program keeps its own admin.

| Id | Item | Status | Ours | Rank |
| --- | --- | --- | --- | --- |
| A1 | `admin <code>` against `k_admincode` (exact; empty or `none`: nobody) | done | the book's login, `k_admincode` an endpoint setting kept out of the server info | |
| A2 | `k_admins 0`: "NO admins on this server!" | done | `k_admins` (unset: 1, the example config's) | |
| A3 | a wrong code: "Access denied...", then "Wait N second(s)!" for 5 s | done | the book's wait after a wrong try alone | |
| A4 | success and giving it up, the lines to everyone and to the admin | done | "gains admins status!", "is no longer an admin"; the admin is pointed at `acommands` | |
| A5 | "You are not an admin" for an admin's word | done | | |
| A6 | admin kept across a map change, lost on a reconnect | done | forgotten when the slot frees or another user id takes it | |
| A7 | `admin` with no code: the code typed on the number keys | missing | the code is typed after the word | 3 |
| A8 | `elect`: an admin by vote (`k_allowvoteadmin`, `k_vp_admin`), elected admins' limits | missing | | 2 |
| A9 | `forcebreak`: "<name> forces a break!", silent outside a match | done | the book's `endmatch` | |
| A10 | `forcemap <map>`: refused in a match, usage, an unknown map, the line | done | the book's `changemap`; without sv_lobby always taken | |
| A11 | `kick <id or name> [reason]`: the lines, the reason, a 30-second ban | done | user id first, then the exact name; an admin is not kicked (ours) | |
| A12 | `kick` alone: kick mode, `y`/`n` through every player | missing | | 3 |
| A13 | `mkick <id> [id ...] [reason]` | missing | | 3 |
| A14 | `forcestart`: the 10-second count, "Get ready!", the unready dropped, cancelled by the admin's break | done | needs sv_lobby; ours hands over to the lobby's countdown | |
| A15 | `lock`: joins refused 15 seconds, the lines | done | a joining player watches instead (as sv_lockjoin) | |
| A16 | `force_spec`: a player made a spectator or back | missing | | 2 |
| A17 | `lockmap`, `prewar`, `exclusive`, `infolock` toggles | missing | `sv_prewar` and `sv_lockjoin` are settings already | 3 |
| A18 | admins' votes as a veto | missing | | 3 |
| A19 | captain mode: `captain` elections, numbered picks in turn A B B A, the lines | missing | next: the engine's teams book in QuakeWorld's words | 1 |
| A20 | coaches | missing | | 3 |
| A21 | the engine's other referee words (`tl`, `ban`, `mute`, `shuffle`, `readyall`) | not-taken | not QuakeWorld's: left to a mod | |

Missing: 9 (1 / 2 / 6).
