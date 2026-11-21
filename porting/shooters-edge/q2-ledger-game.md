# Quake II ledger: the game (the game library's rules as a player sees them)

Clean-room reader's checklist, 2026-11-21. Reference read: the Yamagi Quake II game library
(baseq2), every file of its game directory. No code; messages players see are quoted exactly.
Ours read from this tree (games/shooters_edge/server/q2_*, net/q2_*, rules/, the q2-*.md
notes). Rank-1 rows R1 to R7 and R13 of `porting/shooters-edge/q2-gaps2-rules.md` are treated as
landed. Row cross-references: R = q2-gaps2-rules.md, S = q2-gaps2-server.md, C =
q2-gaps2-client.md; "world NN" / "monsters NN" = q2-sp-world.md / q2-sp-monsters.md (all landed per
q2-sp-gaps.md).

## Totals

| kind | done | partial | missing | not-taken | rows |
| --- | --- | --- | --- | --- | --- |
| Client commands (what a player types; the game library answers them) | 17 | 13 | 7 | 0 | 37 |
| Server commands (`sv <word>` typed at the server console) | 0 | 4 | 2 | 0 | 6 |
| Cvars the game library registers or reads | 29 | 10 | 7 | 0 | 46 |
| User info keys the game reads | 4 | 5 | 0 | 0 | 9 |
| dmflags bits | 10 | 6 | 0 | 0 | 16 |
| Items (the item list, in its order; each is also a spawnable classname) | 16 | 26 | 0 | 0 | 42 |
| Spawnable entities (the spawn table; items above and monsters below are spawnable too) | 73 | 7 | 1 | 0 | 81 |
| Monsters | 26 | 0 | 0 | 0 | 26 |
| Player messages (printed, broadcast and centre-printed) | 50 | 16 | 15 | 0 | 81 |
| Server console lines (what an admin reads; `dprintf`) | 1 | 1 | 34 | 0 | 36 |
| Layouts: the status bars, the help computer, the scoreboard, the inventory, the stats | 8 | 8 | 1 | 0 | 17 |
| Rules a player sees (behaviour not in a row above) | 18 | 15 | 11 | 0 | 44 |
| **all** | 252 | 111 | 78 | 0 | 441 |

## Client commands (what a player types; the game library answers them)

The game answers these words; any other word is said as chat. During the intermission only
`players`, `say`, `say_team`, `score` and `help` are answered; every other word (the cheats, the
inventory, the unknown word's chat) is silently dropped. A command from a client not yet in the
game is ignored.

count: done 17, partial 13, missing 7, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cmd.players | `players` | lists connected players sorted by frags, `%3i name` per line, `...` when the text would overflow, then a blank line and `<n> players` | done | games/shooters_edge/server/q2_commands.cpp (R "checked and the same") |
| cmd.say | `say <text>` | chat to everyone as `Name: text`, text cut to 150 characters, a leading double quote strips the first and last characters; flood checked; also echoed to the server console | partial | q2_commands.cpp; flood lines and quote stripping differ (R40) |
| cmd.say_team | `say_team <text>` | chat as `(Name): text` to teammates only when dmflags 64 or 128 is set, else to everyone | partial | q2_commands.cpp; co-op teams by dmflags not honoured (R23); flood (R40) |
| cmd.score | `score` | toggles the deathmatch/co-op scoreboard layout; closes inventory and help; in single player shows nothing new | partial | q2_screens.cpp; scoreboard while dead differs (R34) |
| cmd.help | `help` | single player/co-op: toggles the help computer (always closes when open); deathmatch: same as `score` | partial | q2_screens.cpp, q2_campaign.cpp; refresh and toggle rule differ (R35) |
| cmd.use | `use <item name>` | uses an item by pickup name (case ignored, whole rest of line); `unknown item: <name>`, `Item is not usable.`, `Out of item: <name>` | partial | q2_fight.cpp, q2_server.cpp; our own wording (R25) |
| cmd.drop | `drop <item name>` | drops one of an item; `unknown item: <name>`, `Item is not dropable.`, `Out of item: <name>` | partial | q2_fight.cpp; wording (R25), weapon carries ammo (R18), co-op stay items droppable (R21), keys/power armour not droppable (R31) |
| cmd.give | `give <all/health/weapons/ammo/armor/Power Shield/<item>> [n]` | cheat: gives items; `give health n` sets health (at least 1); `unknown item`, `non-pickup item` | partial | q2_commands.cpp; co-op gate (R24), wording (R25), health floor and extra shield (R39) |
| cmd.god | `god` | cheat: toggles god mode, `godmode ON` / `godmode OFF` | partial | q2_commands.cpp; prints `God mode on`/`off` (R25), co-op gate (R24) |
| cmd.notarget | `notarget` | cheat: monsters ignore the player, `notarget ON` / `notarget OFF` | partial | q2_commands.cpp; wording (R25), co-op gate (R24) |
| cmd.noclip | `noclip` | cheat: fly through walls, `noclip ON` / `noclip OFF` | partial | q2_commands.cpp; wording (R25), co-op gate (R24) |
| cmd.inven | `inven` | toggles the inventory screen (closes score and help); the server sends the item counts | partial | q2_screens.cpp; counts not refreshed every frame while open (R36) |
| cmd.invnext | `invnext` | selects the next held item | done | q2_screens.cpp; also steps the chase camera for a spectator (missing, see rule.chase) |
| cmd.invprev | `invprev` | selects the previous held item | done | q2_screens.cpp |
| cmd.invnextw | `invnextw` | next held weapon | done | q2_screens.cpp |
| cmd.invprevw | `invprevw` | previous held weapon | done | q2_screens.cpp |
| cmd.invnextp | `invnextp` | next held powerup | done | q2_screens.cpp |
| cmd.invprevp | `invprevp` | previous held powerup | done | q2_screens.cpp |
| cmd.invuse | `invuse` | uses the selected item; `No item to use.`, `Item is not usable.` | done | q2_screens.cpp |
| cmd.invdrop | `invdrop` | drops the selected item; `No item to drop.`, `Item is not dropable.` | done | q2_screens.cpp (the drop rules behind it: cmd.drop) |
| cmd.weapprev | `weapprev` | switches to the previous usable weapon (from the one being switched to under g_quick_weap) | done | q2_fight.cpp |
| cmd.weapnext | `weapnext` | the next usable weapon | done | q2_fight.cpp |
| cmd.weaplast | `weaplast` | switches back to the last weapon | done | q2_commands.cpp |
| cmd.kill | `kill` | suicide, refused under 5 s after the respawn time and for spectators; obituary `suicides` | partial | q2_server.cpp; first placing on a map counts from map start in the original (R41) |
| cmd.putaway | `putaway` | closes score, help and inventory | done | q2_screens.cpp |
| cmd.wave | `wave <0-4>` | gesture: 0 flipoff, 1 salute, 2 taunt, 3 wave, other point; not while crouched or during a higher animation; prints the gesture's word to the player | done | q2_commands.cpp |
| cmd.playerlist | `playerlist` | per player `mm:ss ping score name` plus ` (spectator)`, `And more...` past the buffer | done | q2_commands.cpp |
| cmd.teleport | `teleport x y z` | cheat: moves the player to (x, y, z+10), stopped, held, view cleared, telefrags; `Usage: teleport x y z` | missing | R28 |
| cmd.spawnentity | `spawnentity classname x y z [pitch yaw roll [flags]]` | cheat: spawns any entity there; usage line on bad arity | missing | R28 |
| cmd.spawnonstart | `spawnonstart classname` | cheat: spawns one at every start/deathmatch/co-op spot, `Spawned entity at x y z` to the console | missing | R28 |
| cmd.listentities | `listentities <all/ammo/items/keys/monsters/weapons>` | cheat: lists matching entities `classname: x y z` to the server console | missing | R28 |
| cmd.cycleweap | `cycleweap <classname...>` | switches through a listed set of weapons; usage line with no names | missing | R26 |
| cmd.prefweap | `prefweap <classname...>` | switches to the first listed weapon held with ammo; usage line | missing | R26 |
| cmd.chat-fallback | any other word | the whole line said as chat (not during the intermission) | done | q2_commands.cpp |
| cmd.intermission-gate | the intermission's five | only players, say, say_team, score, help answered during the intermission | done | q2_commands.cpp `commands_in_intermission` |
| cmd.cheat-gate | the cheat gate | give, god, notarget, noclip, teleport, spawnentity, spawnonstart, listentities refused in deathmatch AND co-op unless `cheats` | partial | q2_commands.cpp gates deathmatch only and prints its own line (R24) |
| cmd.spectator-jump | jump / attack as a spectator | attack starts/stops chasing, jump steps the chase target | missing | R16 (see rule.chase) |

## Server commands (`sv <word>` typed at the server console)

count: done 0, partial 4, missing 2, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| svcmd.test | `sv test` | developer test, prints `Svcmd_Test_f()` | missing | rank 3, harmless |
| svcmd.addip | `sv addip <mask>` | adds an address mask to the filter list (a 0 octet or a short mask is a wildcard); `Usage:  addip <ip-mask>`, `IP filter list is full`, `Bad filter address: <text>` | partial | engine/server/bans.cpp answers `addip` with its own words and `*` wildcards; the `sv` form does not reach it (S30) |
| svcmd.removeip | `sv removeip <mask>` | removes an entry typed the same way: `Removed.` / `Didn't find <mask>.`; `Usage:  sv removeip <ip-mask>` | partial | engine/server/bans.cpp, own wording (S30) |
| svcmd.listip | `sv listip` | `Filter list:` then one `%3i.%3i.%3i.%3i` per entry | partial | engine/server/bans.cpp, own format (S30) |
| svcmd.writeip | `sv writeip` | writes `set filterban N` and one `sv addip` per entry to `<game>/listip.cfg`: `Writing <file>.` / `Couldn't open <file>` | partial | engine/server/bans.cpp writes `addip` lines (S30) |
| svcmd.unknown | any other `sv` word | `Unknown server command "<word>"` | missing | S30 |

## Cvars the game library registers or reads

Flags: SI = in the server info (server browsers and `status` see it), L = latched (takes effect
at the next map), A = archived, N = no-set (command line only), U = user info.

count: done 29, partial 10, missing 7, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cvar.gun_x | `gun_x` 0 | developer: moves the view weapon right by N units | missing | R45; the client has `cl_gun_x` (games/shooters_edge/client/cvar_layers.cpp), not this server name |
| cvar.gun_y | `gun_y` 0 | developer: moves the view weapon forward | missing | R45 |
| cvar.gun_z | `gun_z` 0 | developer: moves the view weapon down | missing | R45 |
| cvar.sv_rollspeed | `sv_rollspeed` 200 | side speed at which a player's view and figure roll fully | done | q2_commands.cpp |
| cvar.sv_rollangle | `sv_rollangle` 2 | most roll angle in degrees | done | q2_commands.cpp |
| cvar.sv_maxvelocity | `sv_maxvelocity` 2000 | per-axis speed cap for thrown and falling things | done | q2_commands.cpp |
| cvar.sv_gravity | `sv_gravity` 800 | gravity; reset to 800 or the world's `gravity` key at each map | done | q2_targets.cpp, q2_server.cpp (world 9) |
| cvar.dedicated | `dedicated` 0 (N) | read to know whether the server has a local player | done | q2_native.cpp; our rules know it from the host |
| cvar.cheats | `cheats` 0 (SI, L) | allows the cheat commands in deathmatch and co-op | partial | q2_commands.cpp: not latched, gates deathmatch only (R24) |
| cvar.gamename | `gamename` baseq2 (SI, L) | the game's name in the server info, for browsers | missing | R45, S27 |
| cvar.gamedate | `gamedate` build date (SI, L) | the build date in the server info | missing | R45, S27 |
| cvar.maxclients | `maxclients` 4 (SI, L) | player slots; forced to at least 1 | partial | net/q2_net_server.hpp default 8 (S35); q2_native.cpp passes it to a library |
| cvar.maxspectators | `maxspectators` 4 (SI) | the most spectators at once (a separate count, players' slots shared) | partial | net/q2_game.cpp synonym of `sv_maxspectators`, default 0 meaning "spectators take free slots" (differs in default and meaning) |
| cvar.deathmatch | `deathmatch` 0 (L) | deathmatch rules | done | q2_monsters.cpp, q2_server.cpp |
| cvar.coop | `coop` 0 (L) | co-op rules (ignored when deathmatch is on) | done | q2_targets.cpp |
| cvar.coop_pickup_weapons | `coop_pickup_weapons` 1 (A) | co-op: a weapon already held may be picked up again for its ammo | done | q2_server.cpp |
| cvar.coop_elevator_delay | `coop_elevator_delay` 1.0 (A) | co-op: a plat waits this long before rising | done | q2_map.cpp |
| cvar.skill | `skill` 1 (L) | 0 easy .. 3 hard+; clamped and forced back at each map | done | q2_server.cpp, q2_monsters.cpp |
| cvar.maxentities | `maxentities` 1024 (L) | the entity slots | missing | R45; ours has no such cap setting |
| cvar.g_footsteps | `g_footsteps` 1 (A) | when players' footsteps sound | done | q2_commands.cpp |
| cvar.g_monsterfootsteps | `g_monsterfootsteps` 0 (A) | monsters' footsteps | done | q2_commands.cpp, q2_monster_steps.cpp |
| cvar.g_fix_triggered | `g_fix_triggered` 0 | a monster waiting for a trigger nothing names stands from the start (and the console note `fixed TRIGGERED flag on ...`) | partial | q2_commands.cpp, q2_monsters.cpp; the console note is missing (see msg table) |
| cvar.g_commanderbody_nogod | `g_commanderbody_nogod` 0 (A) | the commander's body may be shot apart | done | q2_commands.cpp |
| cvar.dmflags | `dmflags` 0 (SI) | deathmatch rule bits (table below) | partial | q2_server.cpp; not in our server info (S27); bits: dmf table |
| cvar.fraglimit | `fraglimit` 0 (SI) | frags that end the level, `Fraglimit hit.` | partial | q2_server.cpp, q2_fight.cpp; not in our server info (S27) |
| cvar.timelimit | `timelimit` 0 (SI) | minutes that end the level, `Timelimit hit.` | partial | q2_server.cpp; not in our server info (S27) |
| cvar.password | `password` "" (U flag) | a player's password to join and to return from spectating (`none` = none) | partial | net/q2_net_server.cpp at connect; not asked on a spectator's return (R15) |
| cvar.spectator_password | `spectator_password` "" | a spectator's password (`none` = none) | partial | net/q2_game.cpp synonym `sv_spectatorpassword`; only at connect (R15) |
| cvar.needpass | `needpass` 0 (SI) | set by the game every frame: 1 player password asked, 2 spectator password asked | done | net/q2_net_server.cpp `status_text` |
| cvar.filterban | `filterban` 1 | 1: the filter list bans; 0: only listed addresses may enter | done | net/q2_game.cpp synonym `sv_banmode`, net/q2_admin.hpp |
| cvar.g_select_empty | `g_select_empty` 0 (A) | allows choosing a weapon with no ammo | done | q2_commands.cpp |
| cvar.run_pitch | `run_pitch` 0.002 | view pitch by forward speed | done | q2_server.cpp |
| cvar.run_roll | `run_roll` 0.005 | view roll by side speed | done | q2_server.cpp |
| cvar.bob_up | `bob_up` 0.005 | stride bob height | done | q2_server.cpp |
| cvar.bob_pitch | `bob_pitch` 0.002 | stride bob pitch | done | q2_server.cpp |
| cvar.bob_roll | `bob_roll` 0.002 | stride bob roll | done | q2_server.cpp |
| cvar.flood_msgs | `flood_msgs` 4 | chat lines allowed per window; above 10 forced to 10 with a console line | partial | q2_commands.cpp clamps silently (R40) |
| cvar.flood_persecond | `flood_persecond` 4 | the window in seconds | done | q2_commands.cpp |
| cvar.flood_waitdelay | `flood_waitdelay` 10 | seconds a flooder is muted | done | q2_commands.cpp |
| cvar.sv_maplist | `sv_maplist` "" | deathmatch map rotation (spaces, commas, line ends) | done | q2_server.cpp, q2_fight.cpp (R13 landed) |
| cvar.aimfix | `aimfix` 0 (A) | shots fly from the gun to the crosshair's point | done | q2_commands.cpp, q2_fight.cpp |
| cvar.g_machinegun_norecoil | `g_machinegun_norecoil` 0 (A) | no machinegun climb outside deathmatch | done | q2_commands.cpp |
| cvar.g_quick_weap | `g_quick_weap` 1 (A) | weapnext/weapprev step from the weapon being switched to, its icon shown 0.9 s | done | q2_commands.cpp |
| cvar.g_swap_speed | `g_swap_speed` 1 (A) | weapon lower/raise frames advanced per frame | done | q2_commands.cpp |
| cvar.game | `game` "" (read) | the mod folder, used by `sv writeip` for listip.cfg | done | q2_native.cpp; the folder the ban file goes to: engine/server/admin.hpp |
| cvar.cl_gun | `cl_gun` 2 (read) | the server process's own `cl_gun`: not 2 lets the help slot show the weapon icon | missing | R38 |

## User info keys the game reads

count: done 4, partial 5, missing 0, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ui.name | `name` | the player's name in prints, the scoreboard and the skin configstring | done | q2_commands.cpp, q2_fight.cpp |
| ui.skin | `skin` | model/skin; teams by skin or model under dmflags 64/128; a skin without `/` is its own team | partial | q2_fight.cpp; slashless skin is no team in ours (R33) |
| ui.hand | `hand` | 0 right, 1 left, 2 centre: shots' start offset; 2 also allows the help-slot weapon icon | partial | q2_fight.cpp offsets; help-slot icon missing (R38) |
| ui.fov | `fov` | field of view, 1..160, forced 90 by dmflags 32768 and during the intermission | partial | q2_fight.cpp; intermission 90 missing (R47) |
| ui.gender | `gender` | f/F or `crakhor` = her/herself, m/M = his/himself, else its/itself, in obituaries | done | q2_fight.cpp, q2_combat.cpp (R1 landed) |
| ui.spectator | `spectator` | deathmatch only: become/leave a spectator (checked at connect and when changed) | partial | net/q2_net_server.cpp at connect only, any mode (R15, R46) |
| ui.password | `password` | checked against `password` at connect and on returning from spectating | partial | net/q2_net_server.cpp at connect only (R15) |
| ui.ip | `ip` | the address the filter list checks; refusal `Banned.` | done | net/q2_net_server.cpp (S10 notes the client can supply it) |
| ui.rejmsg | `rejmsg` (set) | the refusal text a connect gets: `Banned.`, `Spectator password required or incorrect.`, `Server spectator limit is full.`, `Password required or incorrect.` | done | net/q2_net_server.cpp (texts carry an extra `Connection refused.` line, S22) |

## dmflags bits

Bits read in every mode: 4, 64, 128, 256, 8192 (except removing ammo and the BFG from the map);
the rest are deathmatch only (R23).

count: done 10, partial 6, missing 0, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| dmf.1 | no health | health items, adrenaline and the ancient head removed from the map | done | q2_server.cpp |
| dmf.2 | no powerups | powerups removed from the map | done | q2_server.cpp |
| dmf.4 | weapons stay | a taken weapon stays for others (a holder cannot take it again); `drop` of a weapon silently ignored; every mode | partial | q2_fight.cpp; deathmatch only in ours (R23); ours prints a line on drop (R25) |
| dmf.8 | no falling damage | | done | q2_fight.cpp |
| dmf.16 | instant powerups | powerups used on pickup | done | q2_server.cpp |
| dmf.32 | same map | the level's end reloads the same map | done | q2_fight.cpp (R13) |
| dmf.64 | skin teams | teams by the whole skin string | partial | q2_fight.cpp; slashless skins (R33), deathmatch only (R23) |
| dmf.128 | model teams | teams by the model part before `/` | partial | q2_fight.cpp; deathmatch only (R23) |
| dmf.256 | no friendly fire | teammates take no damage (with 64 or 128) | partial | q2_fight.cpp; deathmatch only, so co-op friendly fire is always on (R23) |
| dmf.512 | spawn farthest | the spot farthest from the nearest living player | done | q2_server.cpp |
| dmf.1024 | force respawn | a dead player respawns without a press | done | q2_server.cpp; the press rule without it differs (R9) |
| dmf.2048 | no armour | armour and power armour removed | done | q2_server.cpp |
| dmf.4096 | allow exit | a changelevel in deathmatch ends the level with `<name> exited the level.`; without it the exit kills (`found a way out`) | partial | q2_map.cpp; without the bit nothing happens in ours (R14) |
| dmf.8192 | infinite ammo | no ammo spent, weapons give 1000, ammo and the BFG removed in deathmatch | partial | q2_fight.cpp; deathmatch only (R23) |
| dmf.16384 | quad drop | the quad dropped at death | done | q2_fight.cpp; the spread of the two drops differs (R29) |
| dmf.32768 | fixed fov | every player's fov forced to 90 | done | q2_fight.cpp |

## Items (the item list, in its order; each is also a spawnable classname)

Every item's pickup rules, respawn times, sounds and icons were checked and found the same by
the R audit ("Checked and the same": armour, health, powerups, keys, weapons' ammo and
auto-switch, respawn times, instant items). Ours: games/shooters_edge/server/q2_items.hpp (the
table), q2_server.cpp (pickups), q2_use.cpp (use), q2_fight.cpp (drop, weapons),
q2_combat.hpp (weapons' numbers). Rows are partial only for an item-specific gap. Item teams
(the `team` key on items) are a rule row (rule.item-teams).

count: done 16, partial 26, missing 0, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| item.item_armor_body | Body Armor | armour 100 (max 200), protects 0.8 normal / 0.6 energy; not droppable | done | q2_items.hpp |
| item.item_armor_combat | Combat Armor | armour 50 (max 100), 0.6 / 0.3; not droppable | done | q2_items.hpp |
| item.item_armor_jacket | Jacket Armor | armour 25 (max 50), 0.3 / 0; not droppable | done | q2_items.hpp |
| item.item_armor_shard | Armor Shard | +2 armour on any armour (jacket when none); not droppable | done | q2_items.hpp |
| item.item_power_screen | Power Screen | front screen, cells absorb damage; usable (toggle), droppable (switched off first if last and on) | partial | q2_use.cpp; cannot be dropped in ours (R31) |
| item.item_power_shield | Power Shield | all-round shield, cells absorb damage; usable, droppable; `give` always adds one more | partial | q2_use.cpp; not droppable (R31); `give` gives only when none held (R39) |
| item.weapon_blaster | Blaster | always held, never placed in a map (no world model, no pickup); `give` answers `non-pickup item` | partial | q2_combat.hpp; the `give` line differs (R25) |
| item.weapon_shotgun | Shotgun | uses 1 shell; pickup gives its ammo; stays in co-op | partial | q2_combat.hpp; a `drop`ped one gives ammo back (R18); droppable in co-op (R21) |
| item.weapon_supershotgun | Super Shotgun | uses 2 shells | partial | as weapon_shotgun (R18, R21) |
| item.weapon_machinegun | Machinegun | uses 1 bullet, climbs | partial | R18, R21 |
| item.weapon_chaingun | Chaingun | spins up, 1-3 bullets a frame | partial | R18, R21 |
| item.ammo_grenades | Grenades | ammo AND the hand grenade weapon; 5 a box; first grenades switch to the hand grenade | partial | q2_fight.cpp; the switch test differs with a `count` key (R48); last grenades in hand: `Can't drop current weapon` wording (R25) |
| item.weapon_grenadelauncher | Grenade Launcher | uses 1 grenade | partial | R18, R21 |
| item.weapon_rocketlauncher | Rocket Launcher | uses 1 rocket | partial | R18, R21 |
| item.weapon_hyperblaster | HyperBlaster | uses 1 cell a shot | partial | R18, R21 |
| item.weapon_railgun | Railgun | uses 1 slug | partial | R18, R21 |
| item.weapon_bfg | BFG10K | uses 50 cells; removed from deathmatch maps under dmflags 8192 | partial | R18, R21; `Not enough Cells for BFG10K.` missing (R25) |
| item.ammo_shells | Shells | 10 a box; droppable | done | q2_items.hpp |
| item.ammo_bullets | Bullets | 50 a box | done | q2_items.hpp |
| item.ammo_cells | Cells | 50 a box (also power armour's fuel) | done | q2_items.hpp |
| item.ammo_rockets | Rockets | 5 a box | done | q2_items.hpp |
| item.ammo_slugs | Slugs | 10 a box | done | q2_items.hpp |
| item.item_quad | Quad Damage | ×4 damage 30 s; respawn 60 s; dropped at death under dmflags 16384 | partial | q2_use.cpp; death drop spread (R29) |
| item.item_invulnerability | Invulnerability | 30 s; respawn 300 s | done | q2_use.cpp |
| item.item_silencer | Silencer | 30 shots quieter | done | q2_use.cpp |
| item.item_breather | Rebreather | 30 s air; stays in co-op and cannot be dropped there | partial | q2_use.cpp; taken and gone in co-op, droppable (R20, R21) |
| item.item_enviro | Environment Suit | 30 s slime/lava protection; stays in co-op | partial | R20, R21 |
| item.item_ancient_head | Ancient Head | +2 max health (and health) | done | q2_server.cpp |
| item.item_adrenaline | Adrenaline | +1 max health, health to max | done | q2_server.cpp |
| item.item_bandolier | Bandolier | raises bullet/shell/cell/slug limits, gives some | done | q2_server.cpp |
| item.item_pack | Ammo Pack | raises every ammo limit, gives some; respawn 180 s | done | q2_server.cpp |
| item.key_data_cd | Data CD | key; one each in co-op, stays | partial | q2_targets.cpp; cannot be dropped outside co-op (R31) |
| item.key_power_cube | Power Cube | key; co-op marks per cube; only key allowed spawnflags | partial | q2_server.cpp (world 26); drop (R31); co-op unit's end keeps marks (R22) |
| item.key_pyramid | Pyramid Key | key | partial | drop (R31) |
| item.key_data_spinner | Data Spinner | key | partial | drop (R31) |
| item.key_pass | Security Pass | key | partial | drop (R31) |
| item.key_blue_key | Blue Key | key | partial | drop (R31) |
| item.key_red_key | Red Key | key | partial | drop (R31) |
| item.key_commander_head | Commander's Head | key, shown with the gib effect instead of rotating | partial | drop (R31) |
| item.key_airstrike_target | Airstrike Marker | key | partial | drop (R31) |
| item.health | Health (no classname) | the item behind `give health`, and the four item_health* spawns: health 10 (small 2, large 25, mega 100 past max) | done | q2_items.hpp, q2_server.cpp |
| item.spawnflags-check | item spawnflags | any spawnflags on an item other than a power cube are cleared, with a console line `<class> at <pos> has invalid spawnflags set` | partial | q2_server.cpp clears them; the console line is missing |

## Spawnable entities (the spawn table; items above and monsters below are spawnable too)

An unknown classname prints `<class> doesn't have a spawn function` to the console and is
removed. Entities left out by skill, deathmatch or co-op spawnflags are removed (`<n> entities
inhibited.` on the console). The world and monster audits (q2-sp-world.md 1-51,
q2-sp-monsters.md 1-68) are all landed per q2-sp-gaps.md; rows below are done unless a later gap
row names a difference.

count: done 73, partial 7, missing 1, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ent.item_health | item_health | 10 health, up to max (spawned through the Health item; removed by dmflags 1) | done | q2_items.hpp |
| ent.item_health_small | item_health_small | 2 health, may pass max | done | q2_items.hpp |
| ent.item_health_large | item_health_large | 25 health | done | q2_items.hpp |
| ent.item_health_mega | item_health_mega | 100 health past max, drains back after 5 s, respawn 20 s after the drain | done | q2_items.hpp, q2_server.cpp |
| ent.info_player_start | info_player_start | single-player start, named starts for level changes | partial | q2_server.cpp, q2_campaign.cpp; the eight maps' unnamed starts (R42) missing |
| ent.info_player_deathmatch | info_player_deathmatch | deathmatch spawn: a solid pad with the dmspot model; removed outside deathmatch; random spot avoids the "two nearest" | partial | q2_map.cpp (R5 landed); the two-nearest quirk (R32); spawn does not telefrag (R8) |
| ent.info_player_coop | info_player_coop | co-op spawn by slot order; renamed on 18 maps; three added on security | done | q2_campaign.cpp (R6 landed) |
| ent.info_player_intermission | info_player_intermission | intermission camera spot (one of the first four, random); fact1's camera moved | partial | q2_server.cpp; fact1 camera fix missing (R43); fov 90 missing (R47) |
| ent.func_plat | func_plat | platform with its trigger field; co-op waits coop_elevator_delay | done | q2_map.cpp |
| ent.func_button | func_button | pressed by touch, use or damage; fires targets | done | q2_map.cpp |
| ent.func_door | func_door | sliding door, teams, key-less message, trigger field, damage to open | done | q2_map.cpp |
| ent.func_door_secret | func_door_secret | the two-move secret door | done | q2_map.cpp (world 15) |
| ent.func_door_rotating | func_door_rotating | swinging door | done | q2_map.cpp |
| ent.func_rotating | func_rotating | constantly turning brush, may hurt, toggled | done | q2_map.cpp |
| ent.func_train | func_train | brush moving along path_corners (teleport corners, waits) | done | q2_map.cpp (world 32) |
| ent.func_water | func_water | movable water brush (a door that moves water) | done | q2_map.cpp |
| ent.func_conveyor | func_conveyor | conveyor surface pushing what stands on it, toggled | done | q2_targets.cpp |
| ent.func_areaportal | func_areaportal | opens and shuts an area portal (doors cut sight and hearing) | done | q2_map.cpp, q2_areas.cpp |
| ent.func_clock | func_clock | counting clock into a target_string (timer up/down, time of day) | done | q2_targets.cpp (world 3) |
| ent.func_wall | func_wall | brush toggled in and out of existence; `func_wall START_ON without TOGGLE` console line | done | q2_map.cpp (the console line: msg table) |
| ent.func_object | func_object | brush that falls under gravity when triggered | done | q2_targets.cpp |
| ent.func_timer | func_timer | fires its targets every wait ± random | done | q2_map.cpp |
| ent.func_explosive | func_explosive | breakable brush, debris, explosion; obituary `blew up` | done | q2_targets.cpp (world 6) |
| ent.func_killbox | func_killbox | kills everything inside when used | done | q2_targets.cpp |
| ent.trigger_always | trigger_always | fires its targets at map start | done | q2_map.cpp |
| ent.trigger_once | trigger_once | fires once (touch or use), message centreprinted | partial | q2_map.cpp; the `command` map's `*27` kept on hard (R27) missing |
| ent.trigger_multiple | trigger_multiple | fires on touch, waits | done | q2_map.cpp |
| ent.trigger_relay | trigger_relay | passes a use on | done | q2_map.cpp |
| ent.trigger_push | trigger_push | pushes (once flag), wind sound | done | q2_map.cpp |
| ent.trigger_hurt | trigger_hurt | hurts (slow, silent, toggle, start off, no protection); obituary `was in the wrong place` | done | q2_map.cpp |
| ent.trigger_key | trigger_key | needs an item: `You need the <item>`; co-op key from everyone, cube from its holders | done | q2_targets.cpp |
| ent.trigger_counter | trigger_counter | `<n> more to go...`, `Sequence completed!` | done | q2_targets.cpp |
| ent.trigger_elevator | trigger_elevator | sends a train to a pathtarget | done | q2_map.cpp (world 4) |
| ent.trigger_gravity | trigger_gravity | changes a toucher's gravity | done | q2_targets.cpp |
| ent.trigger_monsterjump | trigger_monsterjump | makes a monster jump | done | q2_targets.cpp |
| ent.target_temp_entity | target_temp_entity | throws a temporary effect | done | q2_targets.cpp |
| ent.target_speaker | target_speaker | a sound, looped or once, volume and attenuation | done | q2_map.cpp (world 12) |
| ent.target_explosion | target_explosion | explosion with delay and damage | done | q2_map.cpp (world 2) |
| ent.target_changelevel | target_changelevel | ends the level to a map (`*` ends a unit); deathmatch exit rules | partial | q2_map.cpp; deathmatch without 4096 should kill with `found a way out` (R14) |
| ent.target_secret | target_secret | counts a secret, sound; mine3's message fix | done | q2_map.cpp (world 19) |
| ent.target_goal | target_goal | counts a goal; the last goal stops the music | done | q2_map.cpp (world 24) |
| ent.target_splash | target_splash | sparks, water, blood etc.; may hurt | done | q2_targets.cpp (world 21) |
| ent.target_spawner | target_spawner | spawns an entity on use (monster spawns); fact2's moved 8 units down | partial | q2_targets.cpp (world 13); fact2 fix missing (R43) |
| ent.target_blaster | target_blaster | fires a blaster bolt on use | done | q2_targets.cpp |
| ent.target_crosslevel_trigger | target_crosslevel_trigger | sets a unit-wide flag | done | q2_map.cpp |
| ent.target_crosslevel_target | target_crosslevel_target | fires when the unit's flags match | done | q2_map.cpp |
| ent.target_laser | target_laser | a damaging laser beam; obituary `saw the light` | done | q2_targets.cpp (world 5) |
| ent.target_help | target_help | sets the help computer's lines, flashes the help icon | done | q2_map.cpp |
| ent.target_lightramp | target_lightramp | ramps a light style over time | done | q2_targets.cpp |
| ent.target_earthquake | target_earthquake | shakes players, rumble sound | done | q2_targets.cpp |
| ent.target_character | target_character | one digit of a target_string | done | q2_targets.cpp |
| ent.target_string | target_string | shows text on target_characters | done | q2_targets.cpp |
| ent.worldspawn | worldspawn | sky, skyaxis, skyrotate, sounds/music (CD track), gravity, message (level name), nextmap; the status bar; precaches | partial | q2_server.cpp; the `music` key (R44) missing |
| ent.viewthing | viewthing | developer: an animating model, `viewthing spawned` on the console | missing | rank 3, a developer entity no stock map uses |
| ent.light | light | toggled light styles (start off) | done | q2_targets.cpp (world 23) |
| ent.light_mine1 | light_mine1 | mine lamp model | done | q2_targets.cpp (world 25) |
| ent.light_mine2 | light_mine2 | mine lamp model | done | q2_targets.cpp |
| ent.info_null | info_null | removed at spawn (a target for lights only) | done | handled as no entity |
| ent.func_group | func_group | removed at spawn (an editor grouping) | done | handled as no entity |
| ent.info_notnull | info_notnull | a point other entities target | done | q2_map.cpp target lookup |
| ent.path_corner | path_corner | train and monster path point (teleport flag, wait, pathtarget) | done | q2_map.cpp, q2_monsters.cpp |
| ent.point_combat | point_combat | where a monster goes to fight, hold flag | done | q2_monsters.cpp (monsters 9) |
| ent.misc_explobox | misc_explobox | exploding barrel, pushable; obituary `blew up` | done | q2_targets.cpp |
| ent.misc_banner | misc_banner | waving banner | done | q2_targets.cpp |
| ent.misc_satellite_dish | misc_satellite_dish | dish that turns on use | done | q2_targets.cpp |
| ent.misc_gib_arm | misc_gib_arm | placed gib | done | q2_targets.cpp |
| ent.misc_gib_leg | misc_gib_leg | placed gib | done | q2_targets.cpp |
| ent.misc_gib_head | misc_gib_head | placed gib | done | q2_targets.cpp |
| ent.misc_deadsoldier | misc_deadsoldier | a dead soldier pose, gibbable | done | q2_targets.cpp |
| ent.misc_viper | misc_viper | the flying ship on a path | done | q2_targets.cpp |
| ent.misc_viper_bomb | misc_viper_bomb | the ship's bomb; obituary `was in the wrong place` | done | q2_targets.cpp |
| ent.misc_bigviper | misc_bigviper | the big ship model | done | q2_targets.cpp |
| ent.misc_strogg_ship | misc_strogg_ship | the Strogg ship on a path | done | q2_targets.cpp |
| ent.misc_teleporter | misc_teleporter | teleporter pad; telefrags at the exit | done | q2_map.cpp |
| ent.misc_teleporter_dest | misc_teleporter_dest | a teleport exit, a solid disc | done | q2_map.cpp (R5 landed) |
| ent.misc_blackhole | misc_blackhole | the black hole effect | done | q2_targets.cpp |
| ent.misc_eastertank | misc_eastertank | the posed tank model | done | q2_targets.cpp |
| ent.misc_easterchick | misc_easterchick | the posed chick | done | q2_targets.cpp |
| ent.misc_easterchick2 | misc_easterchick2 | the second posed chick | done | q2_targets.cpp |
| ent.turret_breach | turret_breach | the turret's gun, fires rockets | done | q2_targets.cpp |
| ent.turret_base | turret_base | the turret's base | done | q2_targets.cpp |

## Monsters

Every monster's moves, attacks, pain, deaths, sounds and boxes were audited in q2-sp-monsters.md
(68 rows) and q2-monsters*.md, all landed (q2-sp-gaps.md). Ours: q2_monster_kinds.hpp (the
kinds), q2_monsters.cpp (AI), q2_monster_steps.cpp, q2_monster_pain.cpp, q2_monster_poses.hpp,
q2_gibs.cpp; the C++ rules' campaign in games/shooters_edge/rules/campaign_*. Monsters print
nothing; a death a monster causes reads `<name> died.` (R2 landed). Kills give a co-op player 1
point each: rule.coop-score (R19).

count: done 26, partial 0, missing 0, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| mon.monster_berserk | Berserker | melee brute: spike and club, jump | done | q2_monster_kinds.hpp |
| mon.monster_gladiator | Gladiator | melee cleaver and railgun | done | q2_monster_kinds.hpp |
| mon.monster_gunner | Gunner | chaingun and grenades, ducks | done | q2_monster_kinds.hpp |
| mon.monster_infantry | Enforcer (infantry) | machinegun burst, melee, dying shots | done | q2_monster_kinds.hpp |
| mon.monster_soldier_light | Light Guard | blaster soldier | done | q2_monster_kinds.hpp |
| mon.monster_soldier | Shotgun Guard | shotgun soldier | done | q2_monster_kinds.hpp |
| mon.monster_soldier_ss | Machinegun Guard | machinegun soldier | done | q2_monster_kinds.hpp |
| mon.monster_tank | Tank | blaster, machinegun, rockets | done | q2_monster_kinds.hpp |
| mon.monster_tank_commander | Tank Commander | the tougher tank skin | done | q2_monster_kinds.hpp |
| mon.monster_medic | Medic | heals and raises dead monsters, hyperblaster | done | q2_monster_kinds.hpp |
| mon.monster_flipper | Barracuda Shark | swimmer, bites | done | q2_monster_kinds.hpp |
| mon.monster_chick | Iron Maiden | rockets and a slash | done | q2_monster_kinds.hpp |
| mon.monster_parasite | Parasite | drain tongue within 256 | done | q2_monster_kinds.hpp (monsters 10) |
| mon.monster_flyer | Flyer | flying blaster, bursts at death; jail5 flyers fixed | done | q2_monster_kinds.hpp, q2_monsters.cpp (monsters 11) |
| mon.monster_brain | Brains | tentacles, power screen | done | q2_monster_kinds.hpp |
| mon.monster_floater | Technician | flying, blaster and zap | done | q2_monster_kinds.hpp |
| mon.monster_hover | Icarus | flying blaster | done | q2_monster_kinds.hpp |
| mon.monster_mutant | Mutant | leaping claws | done | q2_monster_kinds.hpp |
| mon.monster_supertank | Super Tank | chaingun, rockets; boss | done | q2_monster_kinds.hpp |
| mon.monster_boss2 | Hornet (boss2) | flying boss: chainguns, rockets | done | q2_monster_kinds.hpp |
| mon.monster_boss3_stand | Makron standing (boss3) | the end-movie Makron, teleports out on use | done | q2_monster_kinds.hpp (world 22) |
| mon.monster_makron | Makron | final boss: BFG, railgun, hyperblaster | done | q2_monster_kinds.hpp |
| mon.monster_jorg | Jorg | Makron's walker: chainguns, BFG; drops Makron | done | q2_monster_kinds.hpp |
| mon.monster_commander_body | Commander's body | the dead commander, gibbable per g_commanderbody_nogod | done | q2_monsters.cpp |
| mon.misc_insane | Insane marine | the prisoners, poses and skins, crucified | done | q2_monster_kinds.hpp (monsters 47) |
| mon.turret_driver | Turret driver | the soldier who aims a turret_breach | done | q2_monster_kinds.hpp, q2_targets.cpp |

## Player messages (printed, broadcast and centre-printed)

Levels: medium = obituaries; high = most game lines; chat = chat and flood lock. A trailing
full stop is part of the text only where shown.

count: done 50, partial 16, missing 15, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| msg.obit.suicide | `<v> suicides.` | the `kill` command | done | games/shooters_edge/server/q2_combat.cpp (R1 landed) |
| msg.obit.falling | `<v> cratered.` | a fall | done | q2_combat.cpp |
| msg.obit.crush | `<v> was squished.` | crushed by a mover | done | q2_combat.cpp |
| msg.obit.water | `<v> sank like a rock.` | drowned | done | q2_combat.cpp |
| msg.obit.slime | `<v> melted.` | slime | done | q2_combat.cpp |
| msg.obit.lava | `<v> does a back flip into the lava.` | lava | done | q2_combat.cpp |
| msg.obit.explosive | `<v> blew up.` | exploding wall or barrel | done | q2_combat.cpp |
| msg.obit.exit | `<v> found a way out.` | the deathmatch exit without allow-exit | missing | R14: ours never kills at the exit |
| msg.obit.target-laser | `<v> saw the light.` | target_laser | done | q2_combat.cpp |
| msg.obit.target-blaster | `<v> got blasted.` | target_blaster (never produced in this release: its bolt carries the generic cause) | done | ours never prints it either (q2-world-round4.md) |
| msg.obit.wrong-place | `<v> was in the wrong place.` | trigger_hurt, splash, bomb | done | q2_combat.cpp |
| msg.obit.held-grenade-self | `<v> tried to put the pin back in.` | own grenade held too long | done | q2_combat.cpp |
| msg.obit.grenade-self | `<v> tripped on his/her/its own grenade.` | own grenade's or hand grenade's blast | done | q2_combat.cpp |
| msg.obit.rocket-self | `<v> blew himself/herself/itself up.` | own rocket's blast | done | q2_combat.cpp |
| msg.obit.bfg-self | `<v> should have used a smaller gun.` | own BFG blast | done | q2_combat.cpp |
| msg.obit.self-other | `<v> killed himself/herself/itself.` | any other self kill | done | q2_combat.cpp |
| msg.obit.blaster | `<v> was blasted by <k>` | | done | q2_combat.cpp |
| msg.obit.shotgun | `<v> was gunned down by <k>` | | done | q2_combat.cpp |
| msg.obit.sshotgun | `<v> was blown away by <k>'s super shotgun` | | done | q2_combat.cpp |
| msg.obit.machinegun | `<v> was machinegunned by <k>` | | done | q2_combat.cpp |
| msg.obit.chaingun | `<v> was cut in half by <k>'s chaingun` | | done | q2_combat.cpp |
| msg.obit.grenade-direct | `<v> was popped by <k>'s grenade` | | done | q2_combat.cpp |
| msg.obit.grenade-splash | `<v> was shredded by <k>'s shrapnel` | | done | q2_combat.cpp |
| msg.obit.rocket-direct | `<v> ate <k>'s rocket` | | done | q2_combat.cpp |
| msg.obit.rocket-splash | `<v> almost dodged <k>'s rocket` | | done | q2_combat.cpp |
| msg.obit.hyperblaster | `<v> was melted by <k>'s hyperblaster` | | done | q2_combat.cpp |
| msg.obit.railgun | `<v> was railed by <k>` | | done | q2_combat.cpp |
| msg.obit.bfg-laser | `<v> saw the pretty lights from <k>'s BFG` | | done | q2_combat.cpp |
| msg.obit.bfg-blast | `<v> was disintegrated by <k>'s BFG blast` | | done | q2_combat.cpp |
| msg.obit.bfg-effect | `<v> couldn't hide from <k>'s BFG` | | done | q2_combat.cpp |
| msg.obit.handgrenade | `<v> caught <k>'s handgrenade` | direct | done | q2_combat.cpp |
| msg.obit.hg-splash | `<v> didn't see <k>'s handgrenade` | blast | done | q2_combat.cpp |
| msg.obit.held-grenade | `<v> feels <k>'s pain` | caught in another's held grenade | done | q2_combat.cpp |
| msg.obit.telefrag | `<v> tried to invade <k>'s personal space` | telefrag (teleporters; spawns too in the original) | partial | q2_combat.cpp; spawn telefrags missing (R8) |
| msg.obit.died | `<v> died.` | any other death; every death in single player | done | q2_fight.cpp (R2 landed) |
| msg.entered | `<name> entered the game` | a player placed (deathmatch every map; else when maxclients > 1) | done | q2_server.cpp (R3 landed) |
| msg.disconnected | `<name> disconnected` | a player leaves | done | q2_server.cpp (R3 landed) |
| msg.timelimit | `Timelimit hit.` | | done | q2_fight.cpp (R4 landed) |
| msg.fraglimit | `Fraglimit hit.` | | done | q2_fight.cpp (R4 landed) |
| msg.exited | `<name> exited the level.` | deathmatch exit with dmflags 4096 | done | q2_map.cpp |
| msg.sidelines | `<name> has moved to the sidelines` | becoming a spectator | missing | R15 |
| msg.joined | `<name> joined the game` | a spectator coming back | missing | R15 |
| msg.spec-password | `Spectator password incorrect.` | changing to spectator with the wrong password | missing | R15 |
| msg.spec-full | `Server spectator limit is full.` (no line end) | changing to spectator when full | missing | R15 (ours has it only as a connect refusal) |
| msg.password | `Password incorrect.` | returning to play with the wrong password | missing | R15 |
| msg.chase-none | `No other players to chase.` (centre) | a spectator's attack with nobody to follow | missing | R16 |
| msg.need-key | `You need the <item>` (centre) | trigger_key without its item | done | q2_targets.cpp |
| msg.counter | `<n> more to go...` (centre) | trigger_counter | done | q2_targets.cpp |
| msg.counter-done | `Sequence completed!` (centre) | trigger_counter's last | done | q2_targets.cpp |
| msg.entity-message | `<message>` (centre, with talk1 sound) | an entity's `message` key when it fires its targets, a door's when touched while locked | done | q2_map.cpp (world 8, 18) |
| msg.power-cells | `No cells for power armor.` | turning on power armour without cells | done | q2_use.cpp |
| msg.no-ammo | `No <ammo> for <weapon>.` | choosing a weapon without ammo | done | q2_fight.cpp |
| msg.not-enough-ammo | `Not enough <ammo> for <weapon>.` | under one shot's worth (super shotgun, BFG) | missing | R25: ours prints the `No` line |
| msg.cant-drop | `Can't drop current weapon` | dropping the only weapon in hand, or the last grenades in hand | partial | q2_fight.cpp says `You can't drop the weapon in your hand.` (R25) |
| msg.use-unknown | `unknown item: <name>` | use/drop of an unknown name | partial | q2_commands.cpp `There is no item called <name>.` (R25) |
| msg.not-usable | `Item is not usable.` | | partial | `<name> can't be used.` (R25); invuse's matches |
| msg.not-dropable | `Item is not dropable.` | | partial | `<name> can't be dropped.` (R25); invdrop's matches |
| msg.out-of-item | `Out of item: <name>` | use/drop of an item not held; cycleweap/prefweap | partial | `You have no <name>.` (R25) |
| msg.no-item-use | `No item to use.` | invuse with nothing selected | done | q2_screens.cpp |
| msg.no-item-drop | `No item to drop.` | invdrop with nothing selected | done | q2_screens.cpp |
| msg.give-unknown | `unknown item` | give of an unknown name | partial | `There is no item called <name>.` (R25) |
| msg.give-nonpickup | `non-pickup item` | give Blaster | partial | `The Blaster is always in your hands.` (R25) |
| msg.god | `godmode ON` / `godmode OFF` | | partial | `God mode on`/`off` (R25) |
| msg.notarget | `notarget ON` / `notarget OFF` | | partial | `Notarget on`/`off` (R25) |
| msg.noclip | `noclip ON` / `noclip OFF` | | partial | `Noclip on`/`off` (R25) |
| msg.cheats | `You must run the server with '+set cheats 1' to enable this command.` | the cheat refusal | partial | `Cheats are off on this server (cheats 1 turns them on).` (R24) |
| msg.usage-teleport | `Usage: teleport x y z` | | missing | R28 |
| msg.usage-spawnentity | `Usage: spawnentity classname x y z <angle_x angle_y angle_z> <flags>` | | missing | R28 |
| msg.usage-spawnonstart | `Usage: spawnonstart classname` | | missing | R28 |
| msg.usage-listentities | `Usage: listentities <all\|ammo\|items\|keys\|monsters\|weapons>` | | missing | R28 |
| msg.usage-cycleweap | `Usage: cycleweap classname1 classname2 .. classnameN` | | missing | R26 |
| msg.usage-prefweap | `Usage: prefweap classname1 classname2 .. classnameN` | | missing | R26 |
| msg.flood-wait | `You can't talk for <n> more seconds` | talking while muted | partial | q2_commands.cpp adds a full stop; also applies in single player (R40) |
| msg.flood-lock | `Flood protection: You can't talk for <n> seconds.` (chat level) | the mute starting | partial | q2_commands.cpp lower-case `you` (R40) |
| msg.chat | `Name: text` / `(Name): text` | say / say_team | done | q2_commands.cpp |
| msg.players | `%3i name` lines, `...`, blank, `<n> players` | `players` | done | q2_commands.cpp |
| msg.playerlist | `mm:ss ping score name[ (spectator)]`, `And more...` | `playerlist` | done | q2_commands.cpp |
| msg.wave | `flipoff` / `salute` / `taunt` / `wave` / `point` | `wave`'s echo to the player | done | q2_commands.cpp |
| msg.rejmsg | `Banned.`, `Spectator password required or incorrect.`, `Server spectator limit is full.`, `Password required or incorrect.` | connect refusals the game library sets | partial | net/q2_net_server.cpp adds `Connection refused.` (S22) |
| msg.intermission-scores | (nothing printed) | the intermission shows the layout only | partial | q2_server.cpp prints every player's frags as text (R12) |
| msg.help-icon-weapon | (help slot weapon icon) | see layout.help-slot | missing | R38 |

## Server console lines (what an admin reads; `dprintf`)

count: done 1, partial 1, missing 34, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| con.startup | `Game is starting up.` / `Game is <name> built on <date>.` / `==== ShutdownGame ====` | library start and end | missing | rank 3; ours logs its own lines |
| con.no-spawn | `<class> doesn't have a spawn function` | unknown classname | missing | ours drops it silently |
| con.not-field | `'<key>' is not a field. Value is '<value>'` | unknown entity key | missing | |
| con.incomplete-field | `<func>: entity <n>: incomplete '<key>' field` | a vector key with fewer than three numbers | missing | |
| con.teams | `<n> teams with <m> entities.` | mover teams found | missing | |
| con.inhibited | `<n> entities inhibited.` | entities left out by skill or mode | missing | |
| con.invalid-spawnflags | `<class> at <pos> has invalid spawnflags set` | item spawnflags cleared | missing | |
| con.path-corner | `path_corner with no targetname at <pos>` | | missing | |
| con.target-missing | `<class> at <pos> target <name> does not exist` | a path's target | missing | |
| con.viewthing | `viewthing spawned` | | missing | with ent.viewthing |
| con.func-wall | `func_wall START_ON without TOGGLE` | | missing | |
| con.viper | `misc_viper without a target at <pos>` / `<class> without a target at <pos>` | ships | missing | |
| con.no-target-count | `<class> with no target at <pos>`, `<class> with no count at <pos>` | | missing | |
| con.teleporter | `Couldn't find destination` / `teleporter without a target.` | | missing | |
| con.turret | `<class> at <pos> needs a target` / `<class> at <pos> has bad item: <item>` | | missing | |
| con.no-distance | `<class> at <pos> with no distance set` | a rotating door or a button | missing | |
| con.train | `train_next: bad target <t>`, `connected teleport path_corners, see <class> at <pos>`, `train_find: no target`, `train_find: target <t> not found`, `func_train without a target at <pos>` | | missing | |
| con.elevator | `elevator used with no pathtarget`, `elevator used with bad pathtarget: <t>`, `trigger_elevator has no target`, `trigger_elevator unable to find target <t>`, `trigger_elevator target <t> is not a train` | | missing | |
| con.timer | `func_timer at <pos> has random >= wait` | | missing | |
| con.combattarget | `<class> at <pos>, combattarget <t> not found` | | missing | |
| con.picktarget | `G_PickTarget called with NULL targetname`, `G_PickTarget: target <t> not found` | | missing | |
| con.usetargets | `Think_Delay with no activator`, `entity was removed while using killtargets`, `WARNING: <class> used itself.`, `entity was removed while using targets` | | missing | |
| con.speaker | `target_speaker with no noise set at <pos>` | | missing | |
| con.no-message | `<class> with no message at <pos>` | target_help | missing | |
| con.no-map | `target_changelevel with no map at <pos>` | | missing | |
| con.bad-target | `<class> at <pos>: <t> is a bad target` | target_laser | missing | |
| con.lightramp | `<class> at <pos> target <t> (<class> at <pos>) is not a light`, `<class> target <t> not found at <pos>`, `target_lightramp has bad ramp (<r>) at <pos>`, `<class> with no target at <pos>` | | missing | |
| con.untargeted | `untargeted <class> at <pos>` | target_earthquake etc. | missing | |
| con.flood-clamp | `flood_msgs lowered to max: 10` | | missing | R40 |
| con.spawned | `Spawned entity at <x> <y> <z>` | spawnonstart | missing | R28 |
| con.listentities | `<class>: <x> <y> <z>` | listentities | missing | R28 |
| con.bad-movetype | `<class>:<n> has bad movetype: <n>` | | missing | |
| con.connected | `<name> connected` | a client connects | partial | net/q2_net_server.cpp logs its own line |
| con.fixed-triggered | `fixed TRIGGERED flag on <class> at <pos>` | g_fix_triggered | missing | |
| con.trigger-key | `no key item for trigger_key at <pos>`, `item <i> not found for trigger_key at <pos>`, `<class> at <pos> has no target` | | missing | |
| con.say-echo | `Name: text` on the dedicated console | chat echoed to the server | done | q2_commands.cpp (verify it reaches the dedicated console) |

## Layouts: the status bars, the help computer, the scoreboard, the inventory, the stats

count: done 8, partial 8, missing 1, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| layout.statusbar-sp | single-player status bar program | health number and icon at the bottom left, ammo, armour, selected item, the pickup icon and name, the timer, the help icon | done | games/shooters_edge/server/q2_bar.cpp |
| layout.statusbar-dm | deathmatch status bar program | the same plus frags at top right, `SPECTATOR MODE` (stat 17) and `Chasing <name>` (stat 16) | done | q2_bar.cpp (the program); the two stats are never set (layout.stat-chase) |
| layout.stat-health | health stat | the health, never below −99, the health icon | partial | q2_bar.cpp shows down to −999 (R37) |
| layout.stat-ammo | ammo stat | the weapon's ammo icon and count | done | q2_bar.cpp |
| layout.stat-armor | armour stat | power armour's cells (flashing icon when on, off when out) else armour | done | q2_bar.cpp |
| layout.stat-selected | selected item stat | the selected item's icon | done | q2_bar.cpp |
| layout.stat-pickup | pickup stat | the last pickup's icon and name for 3 s (0.7 s for a cycleweap switch) | partial | q2_bar.cpp; the cycleweap flash missing (R26) |
| layout.stat-timer | timer stat | quad, invulnerability, suit, rebreather in that order with seconds left | done | q2_bar.cpp |
| layout.stat-help | help slot | the help icon blinking while unread; otherwise, with hand 2 or fov > 91 and the server's `cl_gun` not 2, the weapon icon | partial | q2_bar.cpp blinks; the weapon icon is missing (R38) |
| layout.stat-layouts | layouts bits | bit 1 a layout shown (scoreboard/help, and always when dead or intermission in deathmatch), bit 2 inventory (alive only) | partial | q2_bar.cpp, q2_screens.cpp; the dead-in-deathmatch scoreboard (R34) |
| layout.stat-frags | frags stat | the score | done | q2_bar.cpp |
| layout.stat-chase | chase stat (16) and spectator stat (17) | the followed player's name configstring, and the spectator flag | missing | R16 |
| layout.help | the help computer | `help` picture background; skill name `easy`/`medium`/`hard`/`hard+` top right; level name; the two help lines; ` kills     goals    secrets` and the counts | partial | q2_campaign.cpp `campaign_help_screen` same text; not rebuilt every 32 frames while open, toggle rule (R35) |
| layout.scoreboard | the deathmatch/co-op scoreboard | up to 12 players by score, two columns of 6, `tag1` on the viewer, `tag2` on his killer, `client x y n score ping minutes`, refreshed every 32 frames, spectators left off | done | q2_screens.cpp `q2_scoreboard` (ping capped at 9999 in ours: harmless) |
| layout.inventory | the inventory | the server sends every item's count; the client lists held items with the selection | partial | q2_screens.cpp; counts taken once when opened (R36) |
| layout.intermission | the intermission view | frozen at the spot, no gun or model, scoreboard in deathmatch and co-op, fov 90; leaves on a button after 5 s, no time limit | partial | q2_server.cpp; fov (R47), deathmatch leaves by itself after 20 s (R11), text scores printed (R12) |
| layout.dead-view | the dead player's view | looks at his killer, lies on the floor, the scoreboard in deathmatch | partial | q2_server.cpp; scoreboard toggle on death (R34) |

## Rules a player sees (behaviour not in a row above)

count: done 18, partial 15, missing 11, not-taken 0

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| rule.spawn-telefrag | spawn telefrag | placing a player kills whatever is in his box (100 000 damage, obituary telefrag, +1) | missing | R8 |
| rule.respawn-press | deathmatch respawn | after 1 s, on a FRESH press of attack (held through death does not count); dmflags 1024 forces | partial | q2_server.cpp: held attack respawns, first chance at exactly 1.0 s (R9) |
| rule.coop-respawn | co-op respawn | after 1 s on a fresh press of ANY button; body left, teleport effect, 14×8 ms hold; back as he entered the level, keys kept | partial | q2_server.cpp: attack only, no effect or hold in co-op (R10) |
| rule.sp-death | single player death | any button after 1 s asks for the load menu | done | q2_server.cpp |
| rule.intermission-leave | leaving the intermission | after 5 s, the first button from anyone; no time limit | partial | q2_server.cpp leaves by itself after 20 s in deathmatch (R11) |
| rule.intermission-spot | the intermission spot | one of the first four `info_player_intermission`, else start or deathmatch spot | done | q2_server.cpp |
| rule.next-map | the next map | dmflags 32, sv_maplist (case ignored, wrap), the world's nextmap, the first changelevel, the same map | done | q2_fight.cpp (R13 landed) |
| rule.limits | fraglimit / timelimit tests | any client's score reaches the limit; level time ≥ minutes × 60 | done | q2_fight.cpp (R4 landed) |
| rule.dm-exit | deathmatch exits | without 4096 a changelevel kills its toucher (10 × max health, cause exit, −1) | missing | R14 |
| rule.spectator-change | spectating mid-match | the `spectator` setting applied 5 s after the last respawn, passwords and the limit checked, score reset, announced | missing | R15, S8 |
| rule.spectator-sp | spectator outside deathmatch | ignored in co-op and single player | missing | R46 |
| rule.chase | the chase camera | spectators follow players: attack starts/stops, jump and invnext/invprev step; the camera's place; the bar shows `Chasing <name>` | missing | R16 |
| rule.item-teams | item teams | items sharing `team` show one random member at a time | missing | R17 |
| rule.drop-weapon-ammo | dropped weapons | a `drop`ped weapon gives no ammo; one dropped at death gives its usual | partial | q2_fight.cpp gives ammo for both (R18) |
| rule.coop-score | co-op score | +1 a monster killed, carried across levels, kept over respawn; deaths change nothing | partial | R2 landed stops the negatives; monster kills still score nothing (R19) |
| rule.coop-stay | co-op stays | weapons, keys, rebreather and suit stay when taken; one each | partial | rebreather and suit taken (R20) |
| rule.coop-nodrop | co-op no-drop | the staying items answer `Item is not dropable.` | missing | R21 |
| rule.coop-unit-end | a unit's end in co-op | every key and the cube marks cleared | partial | marks carried (R22) |
| rule.dmflags-coop | dmflags in co-op | 4, 64, 128, 256, 8192 read in every mode | missing | R23 |
| rule.coop-carry | co-op carried state | each player's carried state and co-op score kept across a level change | done | R7 landed |
| rule.coop-spots | co-op spots | slot order, renames, security's three, the 550-unit fallback | done | q2_campaign.cpp (R6 landed) |
| rule.dm-spots | deathmatch spots as pads | the dmspot disc, solid | done | q2_map.cpp (R5 landed) |
| rule.random-spot | random spot | the "two nearest" left out as the walk finds them (not exactly the two nearest) | partial | true two nearest (R32) |
| rule.farthest-spot | farthest spot | dmflags 512: farthest from the nearest living player | done | q2_server.cpp |
| rule.death-drops | death drops | the weapon in hand and, under 16384, the quad (used at once with its time left); the two 45° apart | partial | ±11.25° in ours (R29) |
| rule.drop-start | a dropped item's start | 24 ahead along the full view, 16 below the origin, traced; box ±16, a random turn up to 45° | partial | from below the eye, ±15, no turn (R30) |
| rule.give-details | give's details | `give health n` at least 1; `give all` and `give Power Shield` always add one | partial | R39 |
| rule.flood | flood protection | deathmatch and co-op only; lines and quote stripping | partial | R40 |
| rule.kill-first-spawn | kill after a first placing | the 5 s counted from the map's start on a first placing | partial | R41 |
| rule.unnamed-starts | eight maps' unnamed starts | mine1..city2 get an unnamed copy of a named start | missing | R42 |
| rule.map-fixes | fact2 spawner, fact1 camera, command `*27` | three built-in map repairs | missing | R27, R43 |
| rule.map-fixes-landed | mine3 secret, fact1 secret exit, jail5 flyers, the 18 co-op renames, security spots | built-in repairs | done | q2_map.cpp, q2_monsters.cpp, q2_campaign.cpp |
| rule.music | the world's `music` key | the CD track configstring by name when set | missing | R44 |
| rule.first-grenades | first grenades | taking grenades with none held switches to the hand grenade (deathmatch: from the blaster only) | partial | R48 |
| rule.login-flash | login flash | deathmatch always, else only when maxclients > 1 | done | q2_server.cpp `announce_entry` (R49, landed with R3) |
| rule.teams | teams | by skin (64) or model (128); friendly fire −1; say_team | partial | slashless skin (R33), co-op (R23) |
| rule.pickups | pickup rules | armour, health, mega drain, adrenaline, head, bandolier, pack, powerup skill limits, keys, weapons' ammo and auto-switch | done | q2_server.cpp (R "checked and the same") |
| rule.respawn-times | item respawns | armour 20 s, weapons and ammo 30 s, powerups by item; the respawn event; instant items | done | q2_server.cpp |
| rule.power-armor | power armour | use and sounds, switched off when cells run out, first one auto-on in deathmatch | done | q2_use.cpp |
| rule.weapons | weapons | every weapon's frames, damage, spread, kick, quad sound, silencer, the hand grenade's cook, g_swap_speed | done | q2_fight.cpp, q2_combat.hpp (monsters audit 2-5, view notes) |
| rule.player-world | the world on the player | falling damage and sounds, drowning and air, lava and slime with the suit, footsteps, jump sound, pain sounds by health | done | q2_fight.cpp (q2-player-sounds.md) |
| rule.view | the player's view | damage blend and kick, powerup tints and their warning blinks, underwater tint, bob, roll, fall dip, gun bob | done | q2_blend.cpp, q2_server.cpp (world 27; q2-view.md) |
| rule.save | single-player saves and level keeping | save and load, levels kept within a unit, the autosave at each level | done | q2_save.cpp, q2_campaign.cpp |
| rule.unit-end-sp | the unit's end in single player | the intermission and the movie, keys stripped | done | q2_campaign.cpp (world 1) |

