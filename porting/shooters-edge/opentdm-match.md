# OpenTDM's matches: clean-room facts

A WORKING clean-room note of facts about what a player and an admin meet in a match on a Quake
II server running the OpenTDM match mod: the phases, ready, teams and captains, duels,
timeouts, overtime, the item and gameplay switches, scoring, the start and the end of a match,
stats, admin powers, leaving and rejoining, and configs. It extends
`server-settings-browse.md` § 10 (the serverinfo keys, the votes and the settings listing,
which are not repeated here). A reader session read the sources below and wrote these facts in
its own words with no code (`clean-room` skill). Sections follow the numbered questions the
reader answered.

Read at the projects' default branches, shallow clones taken 2026-11-23:

- OpenTDM: packetflinger/opentdm (last commit 2026-09-08, "the fork" below) is the current one
  read; the original notr1ch/opentdm (last commit 2012-07-12, "the original") was compared.
  Facts hold for both unless a line says "fork only" or "original".

Marks: **read** (the source does it), **derived** (follows from read facts, working shown),
**measured** (nothing was run: there are none). Text in "green" is Quake II's alternate
character set (each character plus 128). Times are in seconds of game time. The game steps at
the server's frame rate (10 Hz unless the server runs a faster multiple of 10, up to 60), but
weapon animation always advances at 10 Hz (one frame per 0.1 s). **read** Names a player or
admin sees (commands, settings, printed lines) are given as they appear. `<name>` is a player's
name, `<team>` a team's name, `m:ss` a clock.

## 1. Match phases

The phases are warmup, countdown, match ("playing"), overtime, sudden death, timeout and the
end scoreboard. **read**

- **Warmup** (the state at map load, after every match and after a game-mode change). Lasts
  until every player on both teams is ready (§2). HUD status text "Warmup"; each team's status
  slot reads "WARMUP", or "READY" once all of that team is ready. The match clock on the HUD
  shows the full match length (10:00 by default) and does not run. Kills score nothing.
  Serverinfo `time_remaining`, `Score_A`, `Score_B` read "WARMUP". **read**
- **Countdown**. Starts the frame both teams become all-ready. Length `g_match_countdown`,
  default 15 s. Lines at its start, in order: "Match Settings:" followed by the settings text
  (the `settings` listing), then "All players ready! Starting countdown (15 secs)..." (the
  number is the setting printed in shortest form). HUD status "Countdown", HUD clock counts the
  countdown down. When exactly 10.4 s remain the sound `world/10_0.wav` (the announcer counting
  ten to zero) plays to everyone at full volume with no falloff. At 5, 4, 3, 2 and 1 s left the
  number is printed on its own line. At 0 the match begins with "Fight!". **read** **derived**:
  a countdown shorter than 10.4 s never plays the sound, and a countdown that is not a whole
  number of seconds still prints only 5 to 1. During the countdown: nobody's attack button
  works, items cannot be picked up, teams are locked, and the kill command is refused
  silently. An open vote is cancelled when the countdown starts. **read**
- **Match**. Starts at the end of the countdown (§9). Length `g_match_time`, in SECONDS,
  default 600. HUD status "Match". When the clock reaches exactly 60 s, everyone hears
  `misc/talk1.wav` and reads "1 minute remaining." At 10.4 s left `world/10_0.wav` plays
  again. Under 60 s the HUD clock is drawn green. At 0 the match ends, or goes to overtime or
  sudden death if the scores are tied (§6). **read**
- **Overtime** (tie mode 1). HUD status "Overtime"; the clock counts the added time (§6). **read**
- **Sudden death** (tie mode 2). The HUD clock slot reads "Sudden Death". Ends the first frame
  the two team scores differ. **read**
- **Timeout**. Entered from match, overtime or sudden death (§5). The match clock stops; a
  separate "Timeout" clock appears on the HUD. Leaving it goes through a 10-second resume
  countdown, then back to the phase it interrupted. **read**
- **End scoreboard** (intermission). Starts when the match ends; HUD status "Match End". Lasts
  `g_intermission_time`, default 5 s (a fractional value is allowed). Then the server returns
  to warmup on the SAME map (§10). **read**

The original has the same phases, lines and defaults. **read**

## 2. Ready

- **Commands**: `ready`; `notready`, `unready`, `noready`. Both are only for players on a team:
  a spectator gets "You must be on a team to be ready." or "You must be on a team to be NOT
  ready." Neither toggles: `ready` when already ready, or `notready` when not ready, does
  nothing and prints nothing. From the match on (match, overtime, sudden death, timeout, end)
  both are ignored silently. Each is rate-limited to once per second per player (a too-early
  repeat gets "Command ignored due to rate limiting, wait a little longer."). **read**
- **Lines** (to everyone): "<name> is ready!" and "<name> is not ready!" **read**
- **Captain forcing**: `teamready` (`readyteam`) and `teamnotready` (`notreadyteam`), captain
  or admin only ("Only team captains or admins can use teamready/teamnotready."). Each
  teammate whose state would change reads "You were forced ready by team captain <name>!" (or
  "forced not ready") and is then set ready. **read** **derived**: `teamnotready` is broken in
  both versions: it prints the "forced not ready" line to each ready teammate but then applies
  the READY action to them, which does nothing because they are already ready, so nobody is
  made not ready. Each forced teammate also goes through the 1-second rate limit, so one who
  readied a moment before is skipped with the rate-limit line.
- **Admin forcing**: `readyall`, `forceready`, `allready`, `startcountdown` set every team
  player ready at once; `notreadyall`, `unreadyall` set every team player not ready. No line is
  printed. **read**
- **What starts the countdown**: each team has at least one player AND every player on each
  team is ready. There is no minimum beyond one player per team and no requirement that teams
  be equal in size. **read** **derived**: 1 against 3, all ready, starts a match.
- **Pseudo duel**: if, when the countdown starts, each team has exactly one player, the match
  is treated as a duel even in team mode (team names become the two player names, duel spawn
  rules, serverinfo `match_type` "Duel"). **read**
- **Nagging**: while not all are ready, if both teams have players, there are at least 2 team
  players, and at least half of all team players are ready, then 20 s later and every 20 s
  after that everyone reads "Waiting on <name>, <name>, ..." listing every team player not
  ready. **read**
- **Ready safety** (fork only, `g_ready_attention`, default 0): when on, in warmup and not in a
  duel, once at least half of all team players are ready, NO team player on either team can
  fire (the attack button is cleared). **read**
- **What aborts the countdown**: any change that leaves a team not all-ready or empty: a player
  un-readies, a team player goes to spectators (by choice or removed by a captain) or
  disconnects. Joining a team is refused during the countdown. Then: every client is
  sent `stopsound` (cutting the announcer), everyone reads "Countdown aborted!", any demos the
  mod started are stopped (`stop` sent to recording players), a server multi-view recording is
  stopped, pseudo-duel names are undone, and the state returns to warmup. **read**

## 3. Teams

- **Count and names**: two teams plus spectators. Defaults: first team "Hometeam" with skin
  `male/grunt`; second team "Visitors" with skin `female/jezebel` in the fork, `female/athena`
  in the original. Spectators are called "Spectators". Settings `g_team_a_name`,
  `g_team_b_name`, `g_team_a_skin`, `g_team_b_skin`. Team names hold at most 15 characters.
  **read**
- **Joining**: `join <team>` or `team <team>`; `<team>` may be `1`, `A` or the first team's
  name, `2`, `B` or the second's, or `0`, `S`, `O`, `spec`, `obs` for spectators (any case).
  With no argument: "You are not on a team. Use join 1 or join 2 to join a team." or "You are
  on the '<team>' team." Also the menu (`menu`, `ctfmenu`, `inven`) with "*Join <team>" /
  "*Leave <team>" rows showing "(N players)", a "*Spectate" row and a "*Voting Menu" row.
  Joining is rate-limited to once per 2 s. Spectating: `observer`, `spectate`, `chase`, `spec`,
  `obs` (with an optional chase mode, §15). **read**
- **Join refusals**, in check order: "Teams are locked during countdown."; "Match already in
  progress." (any phase but warmup; the `join` command says "You can't change team in the
  middle of a match." first); in duel mode "Only one player is allowed on each team in 1v1
  mode."; a multi-view relay client "MVD clients cannot join a team."; "You are already on team
  '<team>'."; "Team '<team>' is locked." (an invite from that team's captain bypasses the lock); "Team
  '<team>' is full." **read**
- **Lines**: "<name> joined team '<team>'" (no full stop), "<name> left team '<team>'" when
  switching or spectating. In duel mode "<name> joined the game." **read**
- **Size limit**: `g_max_players_per_team`, default 4 (0 = no limit). During a match, an
  accepted invite is also limited to the larger of the two team sizes when the match started
  ("The team is already full."). **read**
- **Locking**: `lockteam` / `lock` and `unlockteam` / `unlock`, captain or admin, not in duel
  mode; an admin not on a team names the team. An empty team cannot be locked ("You can't lock
  empty team!"). Only the caller reads "Team '<team>' is locked." / "is unlocked." A team that
  empties is unlocked automatically (and loses its spectator lock). Locks clear when the
  match ends and on reset. **read**
- **Captains**: the first player to join a team with no captain becomes captain, announced
  "<name> became captain of '<team>'" (not announced in duel mode, where a captain still exists
  silently). When the captain leaves the team the first other player found on it becomes
  captain, announced the same way. `captain` alone: "You are the captain of team '<team>'",
  "<name> is the captain of team '<team>'", or claims an empty captaincy. `captain <player>`
  hands it to a teammate (the new captain reads "<name> transferred captain status to you.").
  An admin may set either team's captain with `captain <team> <player>`. `captains` prints both:
  "Team '<team>' captain: <name>" (or "(none)"). Captaincy is unavailable in duel mode ("This
  command is unavailable in 1v1 mode."). Captains are kept across matches when players
  automatically rejoin (the default), cleared otherwise. **read**
- **What a captain may do** (each also open to admins): rename the team, change its skin,
  invite, pick (only if enabled), lock and unlock, remove a teammate, force ready, call
  timeouts (and, when `g_timeout_captain` is 1, only captains may), lock the team against
  spectators and invite spectators to watch it. **read**
- **Team name**: `teamname <name>`; admin `teamname <team> <name>`. Refused when
  `g_locked_names` is 1 ("Teamnames are locked."), in duel mode, outside warmup unless admin
  ("You can only change team name during warmup."), or when it contains a control character
  or a double quote ("Invalid team name, must not contain color text or quotes."). Broadcast
  "Team '<old>' renamed to '<new>'." **read**
- **Team skin**: `teamskin <model/skin>`; admin `teamskin <team> <model/skin>`. Refused when
  `g_locked_skins` is 1 ("Teamskins are locked."), outside warmup unless admin, or when not of
  the form model slash skin with letters, digits, `_` and `-` only, or when the model is
  `opentdm` ("Invalid model/skin."). No broadcast; every player's look on that team changes.
  **read**
- **Invite**: `invite <name or id>` (warmup, match or timeout). The invitee gets a centre print
  "You are invited to '<team>' by <captain>. Type ACCEPT in the console to accept." (on three
  lines); the captain reads "<name> was invited to join your team." Refused for oneself, for a
  player already on a team ("<name> is already on a team."), in a duel. `accept` joins
  ("No invite to accept!", "The invite is no longer valid." if the inviter is gone or no
  longer captain). Invites are cleared at match start and end. **read**
- **Pick**: `pickplayer`, `pick`, `p` `<name or id>` puts a spectator straight on the team,
  broadcast "<captain> picked <name> for team '<team>'." It works only when `g_tdm_allow_pick`
  is 1 (default 0); otherwise the command behaves exactly as `invite`. Rate limit 1 s. **read**
- **Remove**: `kickplayer`, `removeplayer`, `remove`, `k` `<name or id>` moves a teammate to
  spectators; they read "You were removed from team '<team>' by <captain>." Admins cannot be
  removed ("You can't kick an admin!"). Not in duel mode. **read**
- **Per-player looks**: `tskin <model/skin>` and `eskin <model/skin>` set how the player sees
  teammates and enemies; a spectator sees the first team in the team skin and the second in the
  enemy skin. **read**
- **Spectators onto a team**: only in warmup by joining; during a match only by an accepted
  invite, a pick, or a rejoin code (§13). **read**
- **Idle removal**: in warmup only, a team player with no input for `g_idle_time` seconds
  (default 300) is moved to spectators: "Removing <name> from team '<team>' due to inactivity."
  **read**

## 4. Duel (1v1)

Duel is game mode 2 (`g_gamemode`, voted as `mode 1v1` or `duel`). Its dmflags come from
`g_1v1flags`, default 1040 (force respawn and instant items, as for team mode). **read**

- One player per team; captain commands (team name, lock, remove, invite, pick, captain) are
  refused with "This command is unavailable in 1v1 mode." Team names are the two players' names,
  or "Player 1" / "Player 2" for an empty side. Join line "<name> joined the game." **read**
- Voting duel mode moves every team player who is not their team's captain to spectators.
  **read**
- Ready is the same; the ready-safety rule does not apply in duels. **read**
- Respawn points follow `g_1v1_spawn_mode` (§7) instead of the team setting. **read**
- Timeout lines omit the team name. **read**
- A player who disconnects mid-match gets an automatic timeout and a ghost (§13). **read**
- Scoring is the same as team mode (§8); `teamstats` shows the player's stats instead. **read**
- The pseudo duel (team mode with one player a side at countdown) gets the duel's names, spawn
  rules, disconnect timeout and refusal of invites. **read**

## 5. Timeouts

- **Commands**: `time`, `calltime`, `pause`, `ctime`, `hold` (all synonyms). The same word
  again resumes. **read**
- **Who may call**: a player on a team, or an admin ("Only players in the match may call a time
  out."). With `g_timeout_captain` 1 (fork only, default 0) only the two captains, and this
  check does NOT exempt admins ("Only team captains may call a time out."). Only in match,
  overtime or sudden death ("You can only call a time out during a match."). **read**
- **Length**: `g_max_timeout`, default 300 s; 0 disables timeouts ("Time out is disabled on
  this server."). An admin's timeout lasts 3600 s. **read**
- **Limits** (fork only): `g_timeout_limit`, default 2 per player, 0 unlimited; counted per
  player and reset at each match start; admins are exempt. Refusal "Timeout limit exceeded
  (2/2)" (printed WITHOUT a line end). The original has no limit. **read**
- **Lines**: to everyone "<name> (<team>) called a time out. Match will resume automatically in
  5:00." (duel: no team in brackets); to the caller "Match paused. Use 'time' again to resume
  play." (the word they typed). At 60 s left: "Automatically resuming in 1 minute." **read**
- **Resuming**: only the caller or an admin ("Only <name> or an admin can resume play."; if the
  caller has disconnected: "Match will automatically resume once <name> reconnects."; if no
  caller is recorded: "Only an admin can resume play."). The timeout also resumes when it runs
  out, when the caller leaves the team, and when the caller disconnects. Resume: everyone hears
  `world/10_0.wav` and reads "Game resuming in 10 seconds. Match time remaining: m:ss", the
  countdown is 10.4 s, 5 to 1 are printed, then "Fight!" and play continues in the phase that
  was interrupted. **read**
- **During the resume countdown**: the original caller trying again reads "You may not call
  another time out."; anyone else allowed to call a timeout cancels the countdown (everyone is
  sent `stopsound`) and starts a fresh timeout of their own. **read**
- **What freezes**: the game's own clock stops, so the match clock, item respawn timers,
  powerup timers, projectiles in flight, doors, platforms and every other world entity stand
  still. Team players are frozen in place (they may still turn their view, which is restored
  cleanly at resume); their weapons do not animate or fire; footsteps, lava and drowning
  effects stop; using, dropping and switching inventory, waving and the kill command are
  refused silently. Spectators move freely. **read** The timeout clock and the resume countdown
  run on real frames. **read**
- **Commands during a timeout**: only `commands`, `settings`/`matchinfo`, the timeout words,
  the rejoin words, `win`, the spectate words, `pickplayer`/`pick`, `invite`, `accept`,
  `admin`/`referee`, `weaponhud`/`hud`, `autoscreenshot`, `autorecord`, `ignorespecs`,
  `ignoreplayers`, `ignoreall`, and `vote`/`yes`/`no`; any other word is swallowed silently
  (not chat). A spectator's `chase` commands are ignored during a timeout. **read**
- **HUD**: "Timeout" with its own m:ss clock, green under 60 s. **read**
- **Edge cases**: if both teams are empty during a timeout the match ends ("Match canceled, no
  players remaining."). **read** A timeout called when a team has lost all its players but the
  match has not ended cannot happen in team mode, because an empty team ends the match at once
  (§13). **derived**

## 6. Overtime and ties

- **Setting**: `g_tie_mode`, default 1: 0 = the match ends tied, 1 = overtime, 2 = sudden
  death. Overtime length `g_overtime`, in SECONDS, default 60. **read**
- **Tie mode 0**: "Tie game, 7 to 7." (both numbers come from the first team's score, which is
  harmless because they are equal), then the end scoreboard. No winner is recorded. **read**
- **Tie mode 1**: "Scores are tied 7 - 7, adding 1 minute overtime." (minutes printed in
  shortest form, "minutes" when not exactly 1). The clock gets the overtime length. If the
  scores are still tied when it runs out, another overtime of the same length is added, with
  the same line, without limit. **read** **derived**: with the default 60 s the "1 minute
  remaining." line and sound fire at the start of each overtime, since the clock then reads
  exactly 60 s. **derived**: an overtime length of 0 would re-add zero time every frame while
  tied, printing the line each frame.
- **Tie mode 2**: "Scores are tied 7 - 7, entering Sudden Death!" The clock slot shows "Sudden
  Death" and the match ends the first frame the team scores differ, by any cause, including a
  suicide that lowers a score. **read**
- **Changing the time during play**: a vote of `timelimit` mid-match re-sets the end to match
  start plus the new limit; if that is already past, the match ends at once; if the new end is
  in the future during overtime or sudden death, they are cancelled: "Timelimit extended,
  overtime canceled!" (or "sudden death canceled!"). The admin `tl` command re-sets the end the
  same way but does not cancel overtime. **read**
- **Admin command**: `overtime`, `ot`, `tiemode` with `0`, `1 <minutes>` or `2`. Lines "New
  overtime: 2 minutes" or "New tie mode: no overtime" / "New tie mode: sudden death". Without
  arguments (or from a non-admin) it reports "Overtime is set to 1 minute." or "Game is set to
  tie mode." / "Game is set to sudden death." **read**

## 7. Items, weapons and gameplay switches

- **Removed weapons and powerups** (`g_itemflags`, `g_powerupflags`, bitmasks, default 0 =
  nothing removed): removal happens when the map's items are re-created from the map (at match
  start, on reset, and whenever the setting changes by vote or admin command): a removed item
  is simply not created; nothing replaces it. **read** Ammo goes with the weapons that use it:
  shells only when both shotguns are removed, bullets only when both machine gun and chaingun
  are removed, grenades when both hand grenades and the grenade launcher are removed, rockets
  with the rocket launcher, slugs with the railgun, and cells only when the hyperblaster, the
  BFG, the power screen and the power shield are all removed. **read** **derived**: because the
  hand grenade's pickup IS the grenade ammo box, removing hand grenades alone removes every
  grenade box from the map even when the launcher stays. The admin `bfg 0` toggles only the BFG
  bit and does not recompute ammo. The admin `powerups 0` removes every powerup, `powerups 1`
  restores all. **read**
- **Dmflags per mode** also remove items: team mode and duel default 1040 (force respawn,
  instant items); instagib default 142427, which includes no health, no items (powerups,
  adrenaline, bandolier, pack), no armour, infinite ammo (every weapon and ammo pickup removed),
  no falling damage, instant items and force respawn. **read** (the flag bits named here are the
  stock game's dmflags.)
- **Instagib**: everyone always has only the railgun and slugs, rail damage is 400 instead of
  100, nothing can be dropped or used. **read**
- **Item respawn times** (fork only, settings in seconds): weapons `g_respawn_weapon` 30, ammo
  `g_respawn_ammo` 30, armour `g_respawn_armor` 20, small health `g_respawn_health` 30,
  megahealth `g_respawn_mega` 20 (counted after its bonus has worn off), quad `g_respawn_quad`
  60, invulnerability `g_respawn_invuln` 300, backpack `g_respawn_backpack` 180, adrenaline
  `g_respawn_adren` 60, power shield and power screen `g_respawn_ps` 60. **read**
- **Armour timer** (fork only, server switch `g_armor_timer`, default 1, votable as
  `armortimer`): `armortimer` with no argument starts a 20 s countdown on the player's own HUD
  with the combat armour icon. `armortimer <mask>` sets which armour pickups start it
  automatically, as words `[-/+]all [-/+]ra [-/+]ya [-/+]ga` (also `body`, `combat`, `jacket`; shards
  never) or a number; reply "Armor timer mask set". The mask is also read from user info
  `amask`. Picking up a masked armour starts a 20 s timer with that armour's icon. `armortimer
  -h` prints the usage. When the server switch is off: "Armor timer disabled, vote to enable
  it: "vote armortimer 1"" or "Armor timer disabled in server config". **read**
- **Weapon timer** (fork only, `g_weapon_timer`, default 1, votable as `weapontimer`): the
  same with 30 s, the blaster icon for a manual start, words `[-/+]all sg ssg mg cg gl hb rl rg
  bfg` (also the long names), user info `wmask`. **read**
- **What the player sees of timers**: the HUD has two timer slots (number of seconds left and an
  icon). The first shows, in priority order, quad, the armour timer, envirosuit, rebreather.
  The second shows invulnerability, else the weapon timer; it moves into the first slot when
  the first is empty. **read**
- **Respawn modes** (spawn points chosen when a player respawns; the setting applies when the
  map has more than 2 deathmatch spawn points and the stock "spawn farthest" dmflag is off; that
  dmflag picks the point farthest from any player): **read**
  - `g_tdm_spawn_mode`: 0 "avoid closest (bugged)", 1 "avoid closest" (default), 2 "random".
  - `g_1v1_spawn_mode`: 0 and 1 as above (default 1), any other value "random"; adding 4
  means "(random on small maps)": maps with fewer than 6 spawn points use pure random.
  - "Avoid closest": find the spawn point nearest to any living player on a team, then the
  second nearest, and pick uniformly at random among all the others. Distances are to living
  team players only; spectators and the dead are ignored.
  - "Avoid closest (bugged)": the same intent, but the second point is found in the same single
  pass as the first, so it is only the nearest of the points that were not a new nearest when
  reached; depending on the map's point order it can miss the true second nearest.
  - "Random": uniform over all deathmatch spawn points.
  - Special window: during the first 1 s of a match and the first 5 s of warmup, every mode is
  replaced by a telefrag-safe random choice: points are shuffled and the first with no living
  team player within 64 units is used; if all are occupied, a point held by a TEAMMATE is
  tried with room 80 units to its left, right, behind, or in front, in that order; failing
  that, plain random. **read**
- **Teleporter modes** (`g_teleporter_nofreeze`, default 0, votable as `telemode`): "normal"
  (0) stops the player dead and holds them still for 160 ms on arrival, as stock; "no freeze"
  (1) keeps the player's horizontal speed, drops the vertical speed to zero, and points that
  speed along the destination's facing, so the player is thrown out of the destination at the
  speed they entered. Both telefrag whatever stands at the destination. **read**
- **Weapon switch modes** (`g_fast_weap_switch`, default 0, votable as `switchmode`): **read**
  - 0 "normal": the old weapon plays its whole put-away animation, then the new one its whole
  raise animation, one animation frame per 0.1 s.
  - 1 "faster": the put-away animation is skipped (the swap happens on the first weapon tick),
  the raise animation still plays.
  - 2 "instant": both are skipped; the new weapon is ready one weapon tick (0.1 s) after the
  swap.
  - 3 "insane": as instant, and a switch asked for while firing takes effect on the next
  firing-animation frame that is not a shot frame, instead of after the firing animation.
  - 4 "extreme": as insane, and also right after a shot frame.
  - **derived** stock frame counts at 0.1 s each (put-away frames, raise frames): blaster 3, 5; shotgun
  3, 8; super shotgun 4, 7; machine gun 4, 4; chaingun 3, 5; grenade launcher 5, 6; rocket
  launcher 4, 5; hyperblaster 4, 6; railgun 5, 4; BFG 3, 9. So a normal switch takes
  about 0.7 to 1.2 s, faster about 0.4 to 0.9 s (the raise alone), instant about 0.1 s,
  each plus up to one weapon tick of wait. The exact count may be off by one tick.
- **Gameplay bugs** (`g_bugs`, default 0, votable as `bugs 0/1/2`): "all gameplay bugs fixed"
  (0), "serious gameplay bugs fixed" (1), "no gameplay bugs fixed" (2). **read**
  - Telefrag: at 0 and 1 a spawn or teleport kills every solid thing overlapping the player's
  box (dead bodies excepted); at 2 the stock behaviour, which kills what it finds and fails
  the spawn if something survives.
  - Projectile born inside a wall: at 0 and 1 a rocket, grenade or BFG ball that starts inside
  something solid explodes at once; at 2 it does not (the stock "shoot through the wall up
  close" bug).
  - Double grenade on weapon change: switching away while a hand grenade action is pending
  throws the held grenade at all levels; at 1 and 2 it also throws a second grenade after one
  was already thrown; at 2 also after one blew up in the hand.
  - Quad grenades: at 0 a grenade gets quad damage only when its own thrower has quad; at 1 and
  2 any grenade released while some quad holder is the current shooter gets quad damage (the
  stock bug where a dying player's grenade became a quad grenade).
  - **derived**: level 1 fixes the telefrag and the inside-a-wall bugs and leaves the two
  grenade bugs.
  - Both versions behave the same. **read**

## 8. Damage and scoring

- **Team damage**: on by default. Teammates take full damage unless the stock "no friendly
  fire" dmflag (256) is set; none of the three default flag sets include it. With the flag set,
  team damage is zero but knockback still applies. **read**
- **Self damage**: stock (a player is hurt by their own splash, with the stock rocket-jump
  knockback). **read**
- **Frag scoring** (only during match, overtime, sudden death; never in warmup): killing an
  enemy gives the killer +1 and the killer's team +1. Killing a teammate gives the killer -1
  and their team -1, and the obituary line ends with " ** team kill **" in green. A suicide or
  death by the world (falling, lava, crushing, the `kill` command) gives the victim -1 and the
  victim's team -1. A telefrag of an enemy counts as a kill. **read**
- **Team score** is a running counter; it is the score shown on the HUD and in serverinfo. On
  the scoreboard each player's "Frags" is enemy kills minus team kills minus suicides, and
  "Net" is enemy kills minus team kills minus deaths. **read**
- **Kill command**: refused within 5 s of spawning, and during countdown, timeout and the end.
  **read**
- **Respawning**: the dead respawn by pressing attack after 1 s, or automatically after
  `g_respawn_time` (default 5 s) because force respawn is in every default flag set. **read**
- **Drops on death**: none in warmup or instagib. In a match the held weapon is dropped if it
  has ammo and is not the blaster, and quad is dropped (with its remaining time) only with the
  stock quad-drop dmflag. **read**

## 9. Starting a match

At the end of the countdown, in this order: the match end time is set; the record of who is
playing is made (one record per team player, used for stats and rejoins), and each team player
reads "Your join code for this match is NNNN" (the number in green) and has the user info
`joincode` set to it; every timeout counter is reset; the map's items are re-created exactly as
at map load, minus removed items (bodies are hidden, gibs and players' projectiles and door
and platform triggers are cleared first); "Fight!" is printed; both team scores are set to 0; pending
invites are dropped; every team player is killed silently and then respawned, so
warmup positions cannot pick spawn points, with the match loadout: the blaster only (instagib:
railgun and slugs), stock starting health, no armour. **read**

- Warmup loadout, for comparison: every weapon except the BFG with ammo to its maximum (bullets
  200, shells 100, rockets 50, grenades 50, cells 200, slugs 50), 100 body armour, holding the
  last weapon used or else the rocket launcher. **read**
- The "Match Settings:" broadcast happens at the START OF THE COUNTDOWN (§1), not at "Fight!".
  A `restart` vote (§12) also goes through a countdown and so prints it again. **read**
- Demos at countdown start (§10). **read**

## 10. The end

- **Lines**: normal end "Timelimit hit. Match ended." then "<team> wins, 12 to 7."; tie "Tie game,
  7 to 7."; a team emptied "Match ended." then "<team> wins by forfeit!"; both empty "Match
  canceled, no players remaining." An admin `break` first prints "<name> has ended the match."; an
  `abort` vote prints "Match aborted." Both then go through the same end, so **derived**:
  an aborted or broken match still announces "Timelimit hit. Match ended." and a winner by
  score. **read**
- **Stats**: each team player is shown their own stats tables (§11) at that moment. **read**
- **Scoreboard**: everyone, spectators included, is moved to an intermission spot (one of the
  map's first four intermission points at random, else the player start, else a deathmatch
  point), frozen, with the gun hidden and powerups cleared, showing the scoreboard. Players'
  projectiles still in flight are removed. Lasts `g_intermission_time` (5 s). It lists both
  teams' names and scores, the date and time, per team a header "<team>:<average ping>(<skin>)",
  then rows of Name, Frags, Dths, Net, Ping (pings capped at 999), sorted by Frags, then
  spectators with ping and whom they chase, and the server name at the bottom; "..and N more"
  when rows overflow. At the end every player who was in the match is listed, including those
  who disconnected; during play, disconnected and spectating players are left out. With
  `g_highlight_captain` 1 (fork only) captains get a star. **read**
- **Screenshot**: 5 server frames after the scoreboard appears, team players with
  `autoscreenshot` on, a player config asking for it, or `g_force_screenshot` 1 are sent
  `screenshot`. **read**
- **Demos**: at countdown start team players with `autorecord`, a player config asking for it,
  or `g_force_record` 1 are sent `record "<their team>-<other team>_<server short
  name>_<map>_<YYYYMMDD>-<HHMMSS>"`, with spaces and characters a file name cannot hold
  replaced by `_`; the server short name is `g_demo_hostname` (default: the host name). They are
  sent `stop` when the scoreboard ends (or the countdown aborts). A server multi-view recording
  (`g_record_mvd` 1, or 2 for compressed) is named "<server short name>_<YYYYMMDD>-<HHMMSS>_<map>"
  in the game's demos folder, written under a name starting with `_` and renamed when it
  stops. **read**
- **After the scoreboard**: (fork) "Please report issues at
  https://github.com/packetflinger/opentdm" to everyone (original: "Please report any bugs at
  www.opentdm.net."); demos are stopped; the match becomes the "last match" (its scoreboard,
  stats and top shots stay viewable); the server goes back to warmup ON THE SAME MAP: items
  reset, everyone not ready. With `g_auto_rejoin_match` 1 (default) team players stay on their
  teams silently and captains are kept; with 0 everyone becomes a spectator, captains are
  cleared and the team menu opens. **read**
- **Map change**: none automatic. The map changes only by a `map` or `smartmap` vote, the admin
  `changemap` ("New map: <map>"), or the stock end of level. The stock `timelimit` setting is
  unused (the server warns "The cvar 'timelimit' is no longer used. Did you intend to set
  g_match_time?" at map load) and the stock map-change intermission ends on the next frame.
  With `g_auto_rejoin_map` 1 (default) team players rejoin their teams on the new map in warmup.
  **read** **derived**

## 11. Stats

- **What is counted per player per match** (match phases only, never warmup): enemy kills,
  team kills, suicides, deaths, telefrags; per weapon (blaster, shotgun, super shotgun, machine
  gun, chaingun, hand grenades, grenade launcher, rocket launcher, hyperblaster, railgun, BFG):
  shots fired, hits, kills made with it, deaths suffered from it, damage dealt and damage
  received; kills, deaths, damage dealt and received while the attacker had quad, and the same
  for invulnerability; damage dealt to and received from teammates; pickups per item. **read**
- **Hits**: one hit per damage event (a shotgun blast or a splash counts once however many it
  touches), except the railgun, which counts each player it damages. Hitting oneself counts as
  damage received but not dealt or hit; hitting a teammate counts only as team damage. Damage to
  a corpse is ignored. **read**
- **Items tracked**: armour (not shards), weapons (not when weapons stay), powerups, megahealth; not
  small health, ammo, or anything dropped. "Miss" for an item is how many times anyone else
  took that item (all pickups in the match minus the player's own). **read** **derived**
- **Commands**: `stats`, `kills`, `accuracy`, `damage`, `weapons`, `items`, `killstats`,
  `deathstats` (all the same table) with an optional `<name or id>`; the `old` or `last`
  prefixed forms for the previous match; `teamstats` and its synonyms (`teamkills`,
  `teamaccuracy`, ...) and their `old`/`last` forms; `topshots`, `bottomshots`, `teamtopshots`,
  `teambottomshots` (previous match only); `oldscores`, `oldscore`, `lastscores`, `lastscore` to
  view the previous match's scoreboard (pressing the scores key in warmup also shows it). In
  warmup the plain commands show the previous match. "That information is not available yet."
  when there is none. **read**
- **Table**: "Stats for <name>:" then a header in green "Item | Acc. Kills Deaths Dealt Recvd
  Pick Miss", one row per weapon used or met and per item taken. Acc. is hits over shots as a
  percentage rounded UP; a dash when nothing was fired. Then a totals block: "Total | Kills Dths
  Sui TKs Tele Dealt Recvd TDealt TRecvd" with a "Player" row and a "Team" row (no team row in
  a duel). Team tables are headed "Team '<team>':". **read**
- **Privacy during play**: a team player cannot see the other team's stats until the match ends
  ("You can only see stats for the other team after the match has finished."), and their own
  "Miss" column shows a dash during the match. **read**
- **Top and bottom shots**: per weapon, the player with the highest (or lowest) accuracy among
  those with at least 10 shots (the BFG exempt) and at least one hit, showing Acc. to one
  decimal, Hits, Shots, Kills, Deaths, Name. **read**
- Every stats table a player asks for is also printed to the server console, so it lands in a
  server recording. **read**

## 12. Admin

- **Becoming one**: `admin <password>` or `referee <password>`; the password is
  `g_admin_password` (empty = "Admin is disabled on this server."). Success to everyone "<name>
  became an admin."; failure "Invalid password." (and the attempt is logged on the server).
  Rate limit 1 s. Typing it again while admin: "You are no longer registered as an admin."
  `acommands` prints the admin help. **read**
- **What an admin may do without a vote**: force ready or not ready (§2); `break` ends the
  match; `tl <minutes>` ("Timelimit set to <n>."); `powerups 0/1`, `bfg 0/1` (not during play: "You
  cannot change powerups during the match."); `overtime`; `obsmode speak/whisper/shutup`
  ("Obsmode is set to <word>."); `changemap <map>`; `kick`/`boot <id>`, `kickban <id>` (bans
  for 60 minutes), `ban <id> [minutes]` (default 60, capped at 720, no permanent: "You may not
  set permanent bans."), `unban`, `bans`; `shuffle` ("Teams randomized...": team players dealt
  alternately to the two teams in random order, all unready, the last one dealt to each team
  becoming its captain); `shufflesmartmaps`; `mute <id> [minutes]` (default 5, maximum 60) and
  `unmute`; every captain power on either team in any phase (name, skin, lock, pick, remove,
  captain, spectator lock); timeouts of an hour and resuming anyone's timeout; watching a
  spectator-locked team. An admin's vote decides a vote at once by default. Admins cannot be
  kicked, removed or muted. **read**
- **Not implemented**: `forceteam` does nothing; `serverdemorecord` and `serverdemostop` are
  accepted and do nothing (fork). **read**
- **Server console** (`sv` commands): `settings`, `savedefaults` (the current votable values
  become the defaults restored later), `applysettings` (after a config change: applies a
  game-mode change and its dmflags and resets to warmup), `demostatus`. **read**

## 13. Joining and leaving mid-match

- **A team player disconnecting during a match** (not warmup, not the end): their whole state
  is saved under their join code. Their kills already count in the team score, which is kept.
  **read**
  - Team mode: the match continues. If that leaves a team with no players, the match ends at
  once and the other team wins by forfeit. **read**
  - Duel (and pseudo duel) with `g_1v1_timeout` above 0 (fork default 60 s, original 90 s): an
  automatic timeout starts: "<name> disconnected and has 1:00 to reconnect." and the opponent
  reads "Your opponent has disconnected. You can allow them 1:00 to reconnect, or you can
  force a forfeit by typing 'win' in the console." A translucent ghost of the player stays
  where they were. `win` (only during such a timeout, otherwise "This command may only be
  used in 1v1 mode." or "You can only cause a forfeit during a time out caused by your
  opponent disconnecting.") ends the timeout at once. **read** **derived**: at the end of
  the timeout or after `win`, play resumes, the empty side ends the match, and the opponent
  wins by forfeit. With `g_1v1_timeout` 0 the disconnect ends the match at once.
  - A player who disconnects while their own timeout runs resumes it. **read**
- **Rejoining**: on reconnecting during the same match, the join code in user info (set by the
  server at match start) restores the player automatically; or, from spectator mode, `ghost`,
  `restore`, `recover`, `rejoin` `<code>` ("No client information found for code N.", "No
  match in progress.", "<word> can only be used from spectator mode."). A preferred code may be
  set in user info `joincode` before the match (a nonzero number not already taken; else "Your
  preferred join code could not be set."); otherwise the code is random from 1 to 9999. The
  return is announced "<name> rejoined team '<team>'" ("rejoined the game." in a duel), the
  player's name is set back to the saved one, and the same stats record continues. Refused if
  the team is already at the match's starting size. **read**
  - Team mode: score, ready state and admin rights come back; the player respawns fresh.
  - Duel: the full saved state comes back (position, health, armour, weapons, ammo, powerup
  timers), the ghost is removed, and if both players are present the timeout resumes with the
  10-second countdown. **read**
- **A spectator who connects mid-match**: sees "Match is currently paused and will auto resume
  in m:ss." during a timeout; otherwise the team menu and "Welcome to OpenTDM! Type 'commands'
  in the console for a brief command guide." (original: a longer welcome naming its web site).
  They cannot join a team (§3). The message of the day (`g_motd_message`, `\n` for line breaks,
  empty by default in the fork) is shown 10 s after entering, once per connection; `motd`
  toggles it. **read**
- **Names**: `g_allow_name_change_during_match` (default 1); when 0, a team player renaming
  after the countdown reads "You cannot change your name in the middle of the match!" and is
  set back. **read**
- **Other**: "<name> entered the game" on connect and "<name> disconnected" on leaving. If all
  players leave in warmup the open vote is cancelled, and a map that has run more than 7 days
  is reloaded. **read**

## 14. Configs

- **Server-side configs** are files `configs/<name>.cfg` in the game folder. The list is built
  when the game library starts (only names of letters, digits, `_`, `-` and `.`); **derived**:
  files added later are not listed until the server restarts the game. **read**
- **What one contains**: any server console commands; it is executed whole by the server
  (`exec configs/<name>.cfg`), followed by `sv applysettings`, which applies a game-mode
  change and that mode's dmflags and returns the server to warmup. **read** **derived**: so a
  config is in practice a list of `set g_... value` lines.
- **Listing**: `vote config` with no name prints "Available configs:", a dashed line, one
  "  <name>.cfg" per line (cut with "  ..." when long), then "Usage: vote config
  <configname>"; or "No configs are available." The name may be given with or without `.cfg`; bad
  characters: "Invalid config name."; missing: "Config '<name>.cfg' not found on server."; `g_allow_vote_config`
  0: "Voting config is disabled." When passed: "New config:
  <name>.cfg". **read**
- **Defaults**: the votable settings' startup values are remembered and restored when the
  server has had no team players and no input from anyone for 5 minutes in warmup ("No active
  players for five minutes, restoring default match settings."), after which `defaults.cfg` is
  executed. **read**
- **Web configs**: `vote webconfig <name>` fetches `<name>.cfg` over HTTP from
  `g_http_domain` plus `g_http_path` (fork default opentdm.org and "/configs/"; original
  opentdm.net and "/api/"): "Fetching web config '<name>', please wait...". The file is lines of
  a key, a tab, and a value; keys: timelimit (minutes), gamemode, tiemode, switchmode,
  telemode, map, overtime (minutes), weapons and powerups (bitmasks), chat, description, bugs,
  tdm_spawnmode, 1v1_spawnmode, armortimer, weapontimer, timeout_captain. A fetched config is
  reused for 6 hours. The vote starts only when the file arrives, and only if still in warmup
  with no other vote and the proposer still on a team. Lines on failure: "Web config '<name>'
  was not found. Visit opentdm.org for more information on web configs.", "Unable to parse the
  config file. It may be corrupt.", "The OpenTDM web config service is unavailable at the
  moment." When passed: "New web config: <name>". **read**

## 15. Other features a player meets

- **Chase modes**: `chase` (and the other spectate words) with `quad`, `invul`/`pent`/`666`,
  `killer`, `leader`/`topfragger`, `none`: follow whoever has quad, invulnerability, the last
  killer of the one you watch, or the top fragger. **read**
- **Spectator lock**: `speclock` (captain or admin; off when `g_command_mask` lacks bit 1)
  stops spectators watching that team; `specinvite <name>` toggles one spectator's permission; admins
  always may watch. Lines "Team is locked against spectators.", "<name> was invited to
  spectate team '<team>'." **read**
- **Spectator chat** (`g_chat_mode`, `obsmode`, votable as `chat`): 0 speak, 1 whisper
  (spectators' public chat goes only to spectators during play), 2 shutup (spectators cannot
  chat at all). **read**
- **Private message**: `talk <name or id> <text>` shows "{<name>}: <text>" to both; spectators
  cannot message players during a match. **read**
- **Player id**: `id` / `ident` toggles showing the name of the player under the crosshair.
  **read**
- **Chat macros**: `%h`/`%H` health, `%a`/`%A` armour, `%w`/`%W` weapon and ammo, `%n`/`%N`
  nearby teammates or players, `%m`/`%M` ammo names, `#h`/`#a` raw numbers. **read**
- **Weapon HUD**: `weaponhud`/`hud` toggles a weapon and ammo panel on the HUD ("Weapon hud
  enabled" / "Weapon hud disabled"); `g_weapon_hud` 0 not allowed ("Weapon hud is disabled for
  this server"), 1 allowed but off until toggled, 2 (default) on until toggled off, 3 forced on
  ("Weapon hud is forced for this server"). **read**
- **Player config** (fork only): with user info `stats_id`, a per-player config (team and enemy
  skins, auto record and screenshot, HUD offsets, timer masks) is fetched over HTTP; `playerconfig`
  shows it. **read**
- **Smart map** (fork only): a file (`g_smartmapfile`, default `smartmap.cfg`) lists map names,
  each followed by the team sizes it suits (1 to 4, up to 75 maps per size); `smartmap [size]`
  shows the shuffled list and the next map; `vote smartmap [size]` proposes the next one other
  than the current map. **read**
- **Maps**: `maplist`/`maps` lists the server's map list (`g_maplistfile`). **read**
- **Ping handicap** (fork only, `g_ping_handicap`): pings below the value are SHOWN as the value
  with a few milliseconds of fake jitter; play is unaffected. **read**
- **Vote attention** (fork only, `g_vote_attention`): 1 plays `g_vote_attention_sound` at a call; 2
  also stops players who have not voted from firing in warmup; 3 also freezes them in place
  until they vote. **read** **derived**: at 3 a spectator, who cannot vote, is frozen too
  while a warmup vote is open.
- **Mute**: a muted player reads "You are muted for N more seconds" when trying to chat. **read**
