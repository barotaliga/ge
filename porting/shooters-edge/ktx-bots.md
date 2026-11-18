# The match mod's bots and the map change: a clean-room reader's facts

This note is a clean-room reader's answer document about the match mod's built-in bots and what
the mod and the reference server do across a map change. It was written from reading the match
mod's source (QW-Group KTX, master at commit b1b23a98, 2026-11-08) and the reference server's
source (QW-Group mvdsv, master at commit 70f11356, 2026-11-18). It contains no code: only
behaviour, orders of events, defaults and the names a player, an operator or the module sees
(console commands, cvars, info keys, file paths, export and system call names as
`qvm.md` names them). Each fact is marked **[read]** (seen in the source) or **[inferred]**
(concluded from what was read, not seen directly). It builds on `qvm.md` § Bots, § 3 (the
exports, the order at a map's start, bot frames) and does not repeat what is there.

## 1. How bots are added, and what a map needs

**The switch.** Bots exist only while the server cvar `k_fb_enabled` is exactly 1 [read]. The
mod creates its bot cvars itself on each map's first frame, with a console `set`, but only when
the cvar is empty or missing, so an operator's value survives [read]. Defaults the mod creates:
`k_fb_enabled 0`, `k_fb_options 0`, `k_fb_autoadd_limit 0`, `k_fb_autoremove_at 0`,
`k_fb_auto_delay 1`, `k_fb_skill 10`, `k_fb_debug 0`, `k_fb_admin_only 0`,
`k_fb_freeze_prewar 0`, `k_fb_health 100`, `k_fb_weapon 2`, `k_fb_break_on_death 1`,
`k_fb_quad_multiplier 4`, `k_fb_item_pickup_bonus 0`, `k_fb_easy_skill_mode 1`,
`k_fb_exit_intermission 0`, and per slot 0..31 the name cvars `k_fb_name_<n>`,
`k_fb_name_enemy_<n>`, `k_fb_name_team_<n>` (empty) [read]. Skill tuning cvars named
`k_fbskill_*` (aim, dodge, look-ahead, visibility, volatility…) are created as well [read].
These are plain server cvars (read with the cvar system call), not localinfo keys [read]. An
operator therefore puts `set k_fb_enabled 1` (and any other `k_fb_*`) in the server config
before the first map [inferred: `set` creates the cvar; a bare name would be an unknown command
before the mod has created it].

**The player command.** Everything goes through one client command, `botcmd`, allowed for
players and spectators and in matchless mode [read]. A client types `cmd botcmd <sub> …` (a
client that forwards unknown commands sends `botcmd …` itself) [inferred]. The `mod` server
console command does nothing in this mod (its console-command export returns 0 at once) [read],
so the server console has no bot command; the operator's levers are the cvars [read].

- `k_fb_admin_only`: 0 anyone; 1 the mod's admins only ("You must be an admin to use this
  command"); 2 real server admins only [read].
- While bots are disabled, the only sub-command is `botcmd enable`; it is refused during a match,
  in race mode and in CTF ("Cannot enable bots while …"), otherwise it sets `k_fb_enabled 1` and
  **changes map** to the next map in the mod's map cycle (the same map when `samelevel` is set or
  no cycle entry applies), then re-applies the last user mode [read]. Any other word prints "Bots
  not enabled: to turn on, /botcmd enable" [read]. Enabling needs a fresh map because bot
  support changes item entities' sizes [read: the mod's own reason when it reloads for it].
- With bots enabled, every sub-command except `disable` first requires the current map to be
  **bot-supported** (below), else "Map <name> not supported for bots" and "Bots not supported on
  this map." [read].
- Sub-commands (standard mode): `skill <0..20>` (sets `k_fb_skill`, for the next bot added),
  `addbot [skill] [team]` (skill only when the word starts with a digit; team up to 9 chars),
  `fill [skill]` (adds min(maxclients − players, 8) bots, then sets `k_fb_skill` to that skill),
  `removebot` (removes the most recently filled bot slot), `removeall`, `disable` (sets
  `k_fb_enabled 0`, re-applies the user mode), `health`, `weapon <n|random>`, `breakondeath`,
  `togglequad`, `quadmultiplier`, `itempickupbonus`, `easyskillmode`, `debug` [read]. The bit of
  value 2 in `k_fb_options` swaps in a route editor's command set (`addmarker`, `save`, …) [read].
- **Automatic bots**: with `k_fb_autoadd_limit` N > 0, during bot frames on a supported map with
  at least one human present, the mod adds one bot (at `k_fb_skill`) whenever the player count
  is below min(N, `maxclients`); with `k_fb_autoremove_at` M > 0 it removes the lowest-scoring
  bot when the count exceeds M; at most one change per max(`k_fb_auto_delay`, 1) seconds of game
  time. N > M with both set disables both [read].

**Skill.** Clamped to 0..20 [read]. Adding a bot computes its attributes from the skill (a
different table when `k_fb_easy_skill_mode` is 1, announced with a broadcast line "Using easy bot
skill mode" or "Using default bot skill mode") and then, when they exist, executes
`bots/configs/skill_all.cfg` and `bots/configs/skill_<NN>.cfg` (two digits) through the console
capture system call, to override the `k_fbskill_*` cvars [read]. Existence is tested by opening
the file through the game's search path [read].

**Routes.** A map is bot-supported only when (a) `deathmatch` is non-zero, (b) it is not race
or CTF, and (c) a route file is found [read]. Files are looked up through the game's file
search path (system call fs_open, read mode) in this order: when the cvar `k_entityfile` is
non-empty, `maps/<entityfile>.bot` then `bots/maps/<entityfile>.bot`; then
`maps/<mapname>.bot`, then `bots/maps/<mapname>.bot` [read]. The file is line text read
byte by byte, tokenized with the console tokenizer; lines starting `//` are skipped; the verbs
place markers by coordinates and set goals, zones, paths and flags between marker numbers
[read]. The mod's own repository ships 78 route files under its example configuration's
`ktx/bots/maps/`, including `dm2`, `dm3`, `dm4`, `dm6`, `e1m2`, `aerowalk`, `ztndm1`–`ztndm6`,
`ultrav`, `schloss`, `skull` [read]. **No routes are compiled into the module** [read]; the
module only carries extra per-map behaviour for dm3, dm4, dm6, amphi2 and povdmm4 (camping
spots, path tweaks) and for aerowalk (the quad the mod adds there becomes a route marker) [read].
On a map without a file (or with `deathmatch 0`), item markers are still created (teleporters
need them) but the map stays unsupported: no bot can be added by command or automatically
[read]. So our server must serve these `.bot` files from the game directory's search path for
bots to play on dm4, dm2, e1m2 or aerowalk [inferred].

## 2. The system calls when one bot is added, and the returned number

In order, for `botcmd addbot` [read]:

1. Clamp the skill; compute attributes; console capture (`exec`) of the skill configs if
   present (above).
2. Pick a free slot in the mod's own 32-entry bot table; pick a name (from the `k_fb_name_*`
   cvars or the mod's built-in names; in teamplay, the team with fewer members, ties broken at
   random, and its colours: red 4/4, blue 13/13, yellow 12/12, green 3/3 by default; outside
   teamplay random colours 0..13 each, or 11/12 in the "tot" mode).
3. **add_bot**(name, bottom colour, top colour, skin `"base"`). During this call the server runs
   client connect(0) and put client in server(0) for the new entity (`qvm.md`).
4. Result **0**: print "Error adding bot" to the commanding client (when the command was typed)
   and stop. The mod does not retry [read].
5. Otherwise the result is taken as the bot's **entity number** (1..32, equal to its client slot
   + 1) [read]. The mod stores it in its bot table and indexes the entity directly in its own
   entity array to set bot-private state (last command time = now, skill, table index) [read].
   It assumes the entity is already a connected, spawned player at this point [inferred].
6. **set_bot_userinfo**(n, `team`, the team name or empty, flags 0).
7. A broadcast print of a skill line, then the bot's attributes are set from the `k_fbskill_*`
   cvars.
8. **set_bot_userinfo**(n, `k_nick`, the name, 0).
9. **set_bot_userinfo**(n, `*skill`, the skill as decimal text, flags 1 = star key).

The returned number is later the argument of every set_bot_cmd, set_bot_userinfo and remove_bot
for that bot [read]. Removal: a broadcast "<name> left the game with <frags> frags", a sound on
the bot's body channel (`player/tornoff2.wav`, no attenuation), then **remove_bot**(n), then
the mod forgets the slot [read]. The step-7 print reads the skill of whatever entity the mod
thinks is "self" after add_bot returns; since the nested connect and put-in-server exports set
the mod's self to the bot, it prints the new bot's skill [inferred]. The server must restore the
`self` global after add_bot to the value before the call, as the reference does [read].

## 3. Each frame: bot frames are required

- Start frame with argument **1**: if `k_fb_enabled` is 1 the mod runs its bot frame; else
  nothing. It does **not** run the ordinary start-frame work then [read]. Argument **0**: the
  ordinary start frame only (on the first, also reading `mapname` and `modelname` by infokey)
  [read].
- The bot frame counts its own calls (the counter lives in module memory, so restarts with each
  map's module load) [read/inferred]: on the 3rd it creates a helper entity (classname
  `fb_dropper`) and reads `sv_accelerate` and `sv_maxspeed`; on the 20th it loads the route file
  (§ 1); from the 21st on it runs door routes, pre-physics, and for every bot: rocket-jump
  check, weapon choice, firing, hazard avoidance (temporarily making all players non-solid), then
  **set_bot_cmd** with its command; then the automatic add/remove (§ 1) [read]. At 77 Hz the
  routes load about 0.25 s into the map [inferred].
- The command's msec is computed by the mod from the game time since its last command, times
  1000, truncated, with the fractions carried and added back as whole milliseconds; 12 when there
  is no history [read]. Angles are the bot's desired view angles (roll 0); forward/side/up are a
  direction scaled to speed; buttons bit 0 fire, bit 1 jump; dead bots press jump to respawn; with
  `k_fb_freeze_prewar` 1 bots stand still with no buttons before the match [read].
- The mod also hooks the ordinary player path for bots: player prethink and postthink run bot
  work when bots are enabled [read]. So the server must run each bot's stored command through
  the same prethink → move → postthink path as a real client's, as `qvm.md` says [read].
- **Rate**: the reference's bot frames run at the server cvar `maxfps` (default 77; values
  below 20 or above 1000 mean 77), at most one bot frame per server frame, with leftover time
  carried, and with `frametime` = 1 / rate; only while the map is active and after the first
  physics frame [read]. The mod reads `frametime` only in debug paths and in the blocked handler
  below [read].
- **The blocked retry (not yet in `qvm.md`)**: when a bot's move is clipped by anything (the
  player move reports any plane hit or being stuck), it is the first attempt for this command,
  and the bot entity's `blocked` function word is non-zero (the mod sets it for every bot at
  spawn), the reference does **not** store the move's result. Instead it sets `self` = the bot
  and writes three globals: `trace_endpos` = the origin the move reached (in entity terms),
  `trace_plane_normal` = the velocity it ended with, `trace_allsolid` = the entity's flags with
  the on-ground bit set or cleared by the move's result (and `trace_ent` = the ground entity when
  on ground); calls the **edict blocked** export; then runs the bot's stored command again, once,
  without another retry [read]. The mod's handler may call set_bot_cmd again to replace the
  stored command with a new direction (it tests the new direction with walkmove on the helper
  entity) [read]. Without this retry bots still play but hug walls [inferred].
- **If start frame(1) is never called**: no helper entity, no route file loaded, so the map is
  never supported; every `botcmd` sub-command but `disable` is refused, automatic bots never come,
  and a bot added any other way would stand idle with no commands [read/inferred]. Today's
  add_bot returning 0 gives "Error adding bot" — but only on a supported map, which needs bot
  frames first [read/inferred].

## 4. Bots across a map change

- The reference does **not** keep bots across a map change [read]. When a new map is spawned it
  first saves spawn parms for every spawned client **including bots** (set change parms export
  with `self` = the bot) [read], then frees every bot slot: frags and name cleared, userinfo and
  short info emptied, slot free, bot mark cleared, **with no client disconnect export and no
  roster message** (real clients re-sign on and get a fresh roster) [read]. Then the module is
  shut down and unloaded [read].
- The mod does not re-add them itself: its bot table is module memory and starts empty on each
  map [read]. Bots return on the new map only through automatic adding (`k_fb_autoadd_limit`,
  § 1, once a human is in and the route file has loaded) or by players typing `botcmd` again
  [read].
- A bot dropped any other way (kick, server drop) goes through the remove path: client disconnect
  export, then the slot freed and a roster update queued [read].

## 5. Mod state kept by the server across a map change

- **Cvars** carry nearly all settings: `k_*` match settings, `k_fb_*` bot settings, the map
  cycle `k_ml_<n>`, and internal underscore cvars used as cross-map memory: `_k_last_xonx` (last
  user mode), `_k_last_cycle_map`, `_k_worldspawns` (incremented on each map; 1 means the
  server's first map, which applies `k_defmode`), `_k_lastmap`, `_k_host`, `_k_team`,
  `_k_nospecs`, `_k_pow_last`, `_k_captteam`, `_k_captcolor`, `_k_coachteam` [read]. The mod
  registers every one with `set` only when empty, then trusts the existing value [read]. So
  server cvars must survive the map change, `set` must create an unknown cvar, and a cvar the mod
  set must keep its value [inferred].
- **Localinfo**: numeric keys hold "ghosts" of players who left mid-match (key = the player's
  user id → name; a team id → team name); key `1` and `666` are cleared at match start or end;
  read back by infokey on the world [read]. They are written with console `localinfo` commands
  through the console buffer and executecmd [read].
- **Serverinfo**: `ktxver` (the mod's version), `status` ("Standby", "Forcestart",
  "<n> min left"), `fpd`, `matchtag`, written with console `serverinfo` commands [read].
- **Per-client userinfo** (kept by the server for a client that stays connected): star keys
  such as `*at` (autotrack, commented in the mod as kept "so we can restore it on level change"),
  `*is` (info set already done), and others the mod sets [read]. The server must keep a real
  client's userinfo across the map change [inferred].
- **Spawn parms**: set change parms writes parms 1–10 from the player's inventory (one rule for
  `deathmatch 0`, another when `k_matchless` is set, else the prewar defaults) and parm 11 =
  admin level, 12 = coach, 13 = a per-player flags word, 14 = handicap, 15 = ready [read]. Set new
  parms writes the default inventory and zeros 11–15 [read]. At put client in server the mod
  copies parms 1–16 into its own per-slot table [read]. So for admin rights, coach, handicap and
  ready to survive, the server must hand each returning client its saved parms in the globals
  before client connect, and new parms only to a client new to the server [read: the reference
  sets new parms once when a client first connects and copies the saved parms at each spawn].
- **The changelevel system call** in the reference checks `maps/<name>.bsp` exists (else prints
  "Can't find …" and does nothing), refuses a second changelevel in the same map, and queues the
  console command `map <name>` (with the entity file as a second word when given) [read]. The
  `map` command spawns the new server, and that always begins with saving spawn parms for every
  spawned client (players and bots alike) before anything is unloaded [read]; `serverflags` is
  the only other game value carried [read].

## 6. What a real client sees of a bot

- add_bot marks the new slot for a **full client update**, sent to every client (and to a
  demo being recorded) at the next send: frags, **ping**, packet loss, enter time, then the
  short userinfo with the slot's user id [read]. Keys starting `_` are stripped from it [read].
- The short info gets the short-info keys from the bot's userinfo and **all its star keys**, so
  `*bot 1` reaches clients [read]. (Short-info keys: `name`, `team`, `skin`, `topcolor`,
  `bottomcolor`, `gender`, `*auth`, `*flag`, `*skill`, plus `chat` only in an experimental build
  [read].)
- Each later set_bot_userinfo of a short-info key or a `_` key sends a `setinfo` message to all
  clients: here `team` and `*skill` are broadcast; `k_nick` is not [read].
- **Ping**: a bot's ping is its `*skill` value as a number, or 10 when the key is empty [read];
  this is what the scoreboard, the `pings` client command, the demo's periodic ping updates and
  the server's `status` reply show. Packet loss is 0 [inferred: a bot's slot is zeroed at add and
  never measures loss].
- Frags changes are broadcast like any player's [read]. The bot is a spawned player in every
  client's entity updates; after each bot command runs, the reference keeps it as the bot's
  last command with buttons cleared, sets the bot's local time to the server time and asks for no
  delta [read]. For demos the bot's view angles are taken from its `v_angle` [read].
- Nothing is ever sent to a bot: its reliable and unreliable buffers are emptied each frame;
  damage taken is written to the demo only [read]. Bots are skipped by timeouts, the speed check,
  remote-console sender lookup and the `mod` command's sender lookup [read]. The server's
  `status` listing with the client-type option marks a bot `b`, a human `h` [read].
- The reference fills a per-entity `visclients` mask for bots each frame; the mod has no such
  field, so it is unused [read].
