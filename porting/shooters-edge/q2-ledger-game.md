# Quake II ledger: the game (the game library's rules as a player sees them)

Clean-room reader's checklist, 2026-11-21. Reference read: the Yamagi Quake II game library
(baseq2), every file of its game directory. No code; messages players see are quoted exactly.
Ours read from this tree (games/shooters_edge/server/q2_*, net/q2_*, rules/, the q2-*.md
notes). Rank-1 rows R1 to R7 and R13 of `porting/shooters-edge/q2-gaps2-rules.md` are treated as
landed. Row cross-references: R = q2-gaps2-rules.md, S = q2-gaps2-server.md, C =
q2-gaps2-client.md; "world NN" / "monsters NN" = q2-sp-world.md / q2-sp-monsters.md (all landed per
q2-sp-gaps.md).

## Totals

Counts: `make ledger` counts this ledger's rows by section and the missing rows by rank (`porting/ledger-count.mjs`; `make ledger ARGS=--missing` for the missing count alone). No total is kept here by hand.

## Client commands (what a player types; the game library answers them)

The game answers these words; any other word is said as chat. During the intermission only
`players`, `say`, `say_team`, `score` and `help` are answered; every other word (the cheats, the
inventory, the unknown word's chat) is silently dropped. A command from a client not yet in the
game is ignored.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cmd.players | `players` | lists connected players sorted by frags, `%3i name` per line, `...` when the text would overflow, then a blank line and `<n> players` | done | games/shooters_edge/server/q2_commands.cpp (R "checked and the same") |
| cmd.say | `say <text>` | chat to everyone as `Name: text`, text cut to 150 characters, a leading double quote strips the first and last characters; flood checked; also echoed to the server console | done | q2_commands.cpp: flood only in deathmatch and co-op, the leading quote strips the first and last characters |
| cmd.say_team | `say_team <text>` | chat as `(Name): text` to teammates only when dmflags 64 or 128 is set, else to everyone | done | q2_commands.cpp: teams by dmflags 64/128 in every mode (fight_skin_mates) |
| cmd.score | `score` | toggles the deathmatch/co-op scoreboard layout; closes inventory and help; in single player shows nothing new | done | q2_screens.cpp, q2_bar.cpp: in deathmatch the scoreboard bit stays on while dead; death flips the toggle |
| cmd.help | `help` | single player/co-op: toggles the help computer (always closes when open); deathmatch: same as `score` | done | q2_screens.cpp: help while open always closes; rebuilt every 32 frames while open |
| cmd.use | `use <item name>` | uses an item by pickup name (case ignored, whole rest of line); `unknown item: <name>`, `Item is not usable.`, `Out of item: <name>` | done | `games/shooters_edge/server/q2_fight.cpp`, `games/shooters_edge/server/q2_commands.cpp` |
| cmd.drop | `drop <item name>` | drops one of an item; `unknown item: <name>`, `Item is not dropable.`, `Out of item: <name>` | done | q2_drops.cpp: a drop-thrown weapon gives no ammo, co-op stay items `Item is not dropable.`, keys and power armour droppable |
| cmd.give | `give <all/health/weapons/ammo/armor/Power Shield/<item>> [n]` | cheat: gives items; `give health n` sets health (at least 1); `unknown item`, `non-pickup item` | done | q2_commands.cpp: health at least 1; give all and give Power Shield always add one through the pickup |
| cmd.god | `god` | cheat: toggles god mode, `godmode ON` / `godmode OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.notarget | `notarget` | cheat: monsters ignore the player, `notarget ON` / `notarget OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.noclip | `noclip` | cheat: fly through walls, `noclip ON` / `noclip OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.inven | `inven` | toggles the inventory screen (closes score and help); the server sends the item counts | done | q2_screens.cpp: counts refreshed every frame while open |
| cmd.invnext | `invnext` | selects the next held item | done | q2_screens.cpp; also steps the chase camera for a spectator (missing, see rule.chase) |
| cmd.invprev | `invprev` | selects the previous held item | done | q2_screens.cpp |
| cmd.invnextw | `invnextw` | next held weapon | done | q2_screens.cpp; held by `tests/shooters_edge_q2_screens_test.cpp` |
| cmd.invprevw | `invprevw` | previous held weapon | done | q2_screens.cpp |
| cmd.invnextp | `invnextp` | next held powerup | done | q2_screens.cpp |
| cmd.invprevp | `invprevp` | previous held powerup | done | q2_screens.cpp; held by `tests/shooters_edge_q2_screens_test.cpp` |
| cmd.invuse | `invuse` | uses the selected item; `No item to use.`, `Item is not usable.` | done | q2_screens.cpp |
| cmd.invdrop | `invdrop` | drops the selected item; `No item to drop.`, `Item is not dropable.` | done | q2_screens.cpp (the drop rules behind it: cmd.drop); held by `tests/shooters_edge_q2_screens_test.cpp` |
| cmd.weapprev | `weapprev` | switches to the previous usable weapon (from the one being switched to under g_quick_weap) | done | q2_fight.cpp; held by `tests/shooters_edge_q2_commands_test.cpp` |
| cmd.weapnext | `weapnext` | the next usable weapon | done | q2_fight.cpp |
| cmd.weaplast | `weaplast` | switches back to the last weapon | done | q2_commands.cpp |
| cmd.kill | `kill` | suicide, refused under 5 s after the respawn time and for spectators; obituary `suicides` | done | q2_server.cpp: respawn_time 0 on a first placing (the map start) |
| cmd.putaway | `putaway` | closes score, help and inventory | done | q2_screens.cpp |
| cmd.wave | `wave <0-4>` | gesture: 0 flipoff, 1 salute, 2 taunt, 3 wave, other point; not while crouched or during a higher animation; prints the gesture's word to the player | done | q2_commands.cpp |
| cmd.playerlist | `playerlist` | per player `mm:ss ping score name` plus ` (spectator)`, `And more...` past the buffer | done | q2_commands.cpp |
| cmd.teleport | `teleport x y z` | cheat: moves the player to (x, y, z+10), stopped, held, view cleared, telefrags; `Usage: teleport x y z` | done | `games/shooters_edge/server/q2_commands.cpp`, `q2_map.cpp` (`map_place_player`) |
| cmd.spawnentity | `spawnentity classname x y z [pitch yaw roll [flags]]` | cheat: spawns any entity there; usage line on bad arity | not-taken | the game this port follows has no such command (q2-entity-keys answers); a cheat outside it |
| cmd.spawnonstart | `spawnonstart classname` | cheat: spawns one at every start/deathmatch/co-op spot, `Spawned entity at x y z` to the console | not-taken | not in the game this port follows; a cheat outside it |
| cmd.listentities | `listentities <all/ammo/items/keys/monsters/weapons>` | cheat: lists matching entities `classname: x y z` to the server console | done | `games/shooters_edge/server/q2_commands.cpp` |
| cmd.cycleweap | `cycleweap <classname...>` | switches through a listed set of weapons; usage line with no names | done | `games/shooters_edge/server/q2_commands.cpp` (`weapon_list`) |
| cmd.prefweap | `prefweap <classname...>` | switches to the first listed weapon held with ammo; usage line | done | `games/shooters_edge/server/q2_commands.cpp` (`weapon_list`) |
| cmd.chat-fallback | any other word | the whole line said as chat (not during the intermission) | done | q2_commands.cpp |
| cmd.intermission-gate | the intermission's five | only players, say, say_team, score, help answered during the intermission | done | q2_commands.cpp `commands_in_intermission` |
| cmd.cheat-gate | the cheat gate | give, god, notarget, noclip, teleport, spawnentity, spawnonstart, listentities refused in deathmatch AND co-op unless `cheats` | done | q2_commands.cpp `cheats_allowed` gates give, god, notarget, noclip, teleport and listentities in deathmatch and co-op with the game's line; spawnentity and spawnonstart are not in this game (not taken rows) |
| cmd.spectator-jump | jump / attack as a spectator | attack starts/stops chasing, jump steps the chase target | done | q2_spectate.cpp, q2_screens.cpp |

## Server commands (`sv <word>` typed at the server console)

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| svcmd.test | `sv test` | developer test, prints `Svcmd_Test_f()` | done | q2_server.cpp sq_console |
| svcmd.addip | `sv addip <mask>` | adds an address mask to the filter list (a 0 octet or a short mask is a wildcard); `Usage:  addip <ip-mask>`, `IP filter list is full`, `Bad filter address: <text>` | done | games/shooters_edge/net/q2_admin.cpp (`sv_filter`): the mask's 0 or missing parts a wildcard on the server's ban list; the usage, `Bad filter address: <text>` and `IP filter list is full` (1024); a matching client dropped |
| svcmd.removeip | `sv removeip <mask>` | removes an entry typed the same way: `Removed.` / `Didn't find <mask>.`; `Usage:  sv removeip <ip-mask>` | done | games/shooters_edge/net/q2_admin.cpp: `Removed.` / `Didn't find <mask>.`, the usage |
| svcmd.listip | `sv listip` | `Filter list:` then one `%3i.%3i.%3i.%3i` per entry | done | games/shooters_edge/net/q2_admin.cpp: `Filter list:` and the `%3i` columns, a wildcard as 0 |
| svcmd.writeip | `sv writeip` | writes `set filterban N` and one `sv addip` per entry to `<game>/listip.cfg`: `Writing <file>.` / `Couldn't open <file>` | done | games/shooters_edge/net/q2_admin.cpp: `set filterban N` and `sv addip` lines to `<game>/listip.cfg`, `Writing <file>.` / `Couldn't open <file>`; read back at the start (engine/server/bans.hpp) with its mode |
| svcmd.unknown | any other `sv` word | `Unknown server command "<word>"` | done | q2_server.cpp sq_console; addip and its kin left to the network server |

## Cvars the game library registers or reads

Flags: SI = in the server info (server browsers and `status` see it), L = latched (takes effect
at the next map), A = archived, N = no-set (command line only), U = user info.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cvar.gun_x | `gun_x` 0 | developer: moves the view weapon right by N units | done | q2_commands.cpp; plugins/classic/q2_view.cpp moves the gun offset along the view's right; on the wire's gun offset |
| cvar.gun_y | `gun_y` 0 | developer: moves the view weapon forward | done | the same, along the view's forward |
| cvar.gun_z | `gun_z` 0 | developer: moves the view weapon down | done | the same, down |
| cvar.sv_rollspeed | `sv_rollspeed` 200 | side speed at which a player's view and figure roll fully | done | q2_commands.cpp |
| cvar.sv_rollangle | `sv_rollangle` 2 | most roll angle in degrees | done | q2_commands.cpp |
| cvar.sv_maxvelocity | `sv_maxvelocity` 2000 | per-axis speed cap for thrown and falling things | done | q2_commands.cpp |
| cvar.sv_gravity | `sv_gravity` 800 | gravity; reset to 800 or the world's `gravity` key at each map | done | q2_targets.cpp, q2_server.cpp (world 9) |
| cvar.dedicated | `dedicated` 0 (N) | read to know whether the server has a local player | done | q2_native.cpp; our rules know it from the host |
| cvar.cheats | `cheats` 0 (SI, L) | allows the cheat commands in deathmatch and co-op | done | q2_commands.cpp: server info, latched; q2_server.cpp applies latched settings as each map starts |
| cvar.gamename | `gamename` baseq2 (SI, L) | the game's name in the server info, for browsers | done | q2_commands.cpp: baseq2, server info, latched |
| cvar.gamedate | `gamedate` build date (SI, L) | the build date in the server info | done | q2_commands.cpp: server info, latched; a fixed date so every build serves the same info |
| cvar.maxclients | `maxclients` 4 (SI, L) | player slots; forced to at least 1 | not-taken | Our design (regraded 2026-11-23, no `partial` grade): net/q2_net_server.hpp default 8 (S35); q2_native.cpp passes it to a library |
| cvar.maxspectators | `maxspectators` 4 (SI) | the most spectators at once (a separate count, players' slots shared) | not-taken | Our design (regraded 2026-11-23, no `partial` grade): net/q2_game.cpp synonym of `sv_maxspectators`, default 0 meaning "spectators take free slots" (differs in default and meaning) |
| cvar.deathmatch | `deathmatch` 0 (L) | deathmatch rules | done | q2_monsters.cpp, q2_server.cpp |
| cvar.coop | `coop` 0 (L) | co-op rules (ignored when deathmatch is on) | done | q2_targets.cpp |
| cvar.coop_pickup_weapons | `coop_pickup_weapons` 1 (A) | co-op: a weapon already held may be picked up again for its ammo | done | q2_server.cpp |
| cvar.coop_elevator_delay | `coop_elevator_delay` 1.0 (A) | co-op: a plat waits this long before rising | done | q2_map.cpp |
| cvar.skill | `skill` 1 (L) | 0 easy .. 3 hard+; clamped and forced back at each map | done | q2_server.cpp, q2_monsters.cpp |
| cvar.maxentities | `maxentities` 1024 (L) | the entity slots | not-taken | Our design (regraded 2026-11-23, no `partial` grade): q2_commands.cpp: 1024, latched, kept for configs; this server sizes itself to the map and caps nothing |
| cvar.g_footsteps | `g_footsteps` 1 (A) | when players' footsteps sound | done | q2_commands.cpp |
| cvar.g_monsterfootsteps | `g_monsterfootsteps` 0 (A) | monsters' footsteps | done | q2_commands.cpp, q2_monster_steps.cpp |
| cvar.g_fix_triggered | `g_fix_triggered` 0 | a monster waiting for a trigger nothing names stands from the start (and the console note `fixed TRIGGERED flag on ...`) | done | q2_commands.cpp, q2_monsters.cpp; the reference game has neither this setting nor a line for it, so ours prints none (the trigger_once line is con.fixed-triggered) |
| cvar.g_commanderbody_nogod | `g_commanderbody_nogod` 0 (A) | the commander's body may be shot apart | done | q2_commands.cpp |
| cvar.dmflags | `dmflags` 0 (SI) | deathmatch rule bits (table below) | done | q2_server.cpp; in the status answer's server info (games/shooters_edge/net/q2_admin.cpp, `q2_status_keys`); bits: dmf table |
| cvar.fraglimit | `fraglimit` 0 (SI) | frags that end the level, `Fraglimit hit.` | done | q2_server.cpp, q2_fight.cpp; in the status answer's server info |
| cvar.timelimit | `timelimit` 0 (SI) | minutes that end the level, `Timelimit hit.` | done | q2_server.cpp; in the status answer's server info |
| cvar.password | `password` "" (U flag) | a player's password to join and to return from spectating (`none` = none) | done | net/q2_net_server.cpp at connect; q2_spectate.cpp on returning (the gate) |
| cvar.spectator_password | `spectator_password` "" | a spectator's password (`none` = none) | done | net/q2_game.cpp; q2_spectate.cpp mid-match |
| cvar.needpass | `needpass` 0 (SI) | set by the game every frame: 1 player password asked, 2 spectator password asked | done | net/q2_net_server.cpp `status_text` |
| cvar.filterban | `filterban` 1 | 1: the filter list bans; 0: only listed addresses may enter | done | net/q2_game.cpp synonym `sv_banmode`, net/q2_admin.hpp |
| cvar.g_select_empty | `g_select_empty` 0 (A) | allows choosing a weapon with no ammo | done | q2_commands.cpp |
| cvar.run_pitch | `run_pitch` 0.002 | view pitch by forward speed | done | q2_server.cpp |
| cvar.run_roll | `run_roll` 0.005 | view roll by side speed | done | q2_server.cpp |
| cvar.bob_up | `bob_up` 0.005 | stride bob height | done | q2_server.cpp |
| cvar.bob_pitch | `bob_pitch` 0.002 | stride bob pitch | done | q2_server.cpp |
| cvar.bob_roll | `bob_roll` 0.002 | stride bob roll | done | q2_server.cpp |
| cvar.flood_msgs | `flood_msgs` 4 | chat lines allowed per window; above 10 forced to 10 with a console line | done | q2_commands.cpp: above 10 lowered to 10 with the console line |
| cvar.flood_persecond | `flood_persecond` 4 | the window in seconds | done | q2_commands.cpp |
| cvar.flood_waitdelay | `flood_waitdelay` 10 | seconds a flooder is muted | done | q2_commands.cpp |
| cvar.sv_maplist | `sv_maplist` "" | deathmatch map rotation (spaces, commas, line ends) | done | q2_server.cpp, q2_fight.cpp (R13 landed) |
| cvar.aimfix | `aimfix` 0 (A) | shots fly from the gun to the crosshair's point | done | q2_commands.cpp, q2_fight.cpp |
| cvar.g_machinegun_norecoil | `g_machinegun_norecoil` 0 (A) | no machinegun climb outside deathmatch | done | q2_commands.cpp |
| cvar.g_quick_weap | `g_quick_weap` 1 (A) | weapnext/weapprev step from the weapon being switched to, its icon shown 0.9 s | done | q2_commands.cpp |
| cvar.g_swap_speed | `g_swap_speed` 1 (A) | weapon lower/raise frames advanced per frame | done | q2_commands.cpp |
| cvar.game | `game` "" (read) | the mod folder, used by `sv writeip` for listip.cfg | done | q2_native.cpp; the folder the ban file goes to: engine/server/admin.hpp |
| cvar.cl_gun | `cl_gun` 2 (read) | the server process's own `cl_gun`: not 2 lets the help slot show the weapon icon | done | q2_bar.cpp reads the server's own `cl_gun` (absent = 2, never shows) |

## User info keys the game reads

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ui.name | `name` | the player's name in prints, the scoreboard and the skin configstring | done | q2_commands.cpp, q2_fight.cpp |
| ui.skin | `skin` | model/skin; teams by skin or model under dmflags 64/128; a skin without `/` is its own team | done | q2_fight.cpp: a skin with no slash is its own team |
| ui.hand | `hand` | 0 right, 1 left, 2 centre: shots' start offset; 2 also allows the help-slot weapon icon | done | q2_fight.cpp offsets; q2_bar.cpp help-slot weapon icon |
| ui.fov | `fov` | field of view, 1..160, forced 90 by dmflags 32768 and during the intermission | done | q2_fight.cpp: 90 during the intermission |
| ui.gender | `gender` | f/F or `crakhor` = her/herself, m/M = his/himself, else its/itself, in obituaries | done | q2_fight.cpp, q2_combat.cpp (R1 landed) |
| ui.spectator | `spectator` | deathmatch only: become/leave a spectator (checked at connect and when changed) | done | q2_server.cpp sq_user_info, q2_spectate.cpp; net/q2_net_server.cpp keeps the seat in step |
| ui.password | `password` | checked against `password` at connect and on returning from spectating | done | q2_spectate.cpp on the way back to play; net/q2_net_server.cpp at connect |
| ui.ip | `ip` | the address the filter list checks; refusal `Banned.` | done | net/q2_net_server.cpp (S10 notes the client can supply it) |
| ui.rejmsg | `rejmsg` (set) | the refusal text a connect gets: `Banned.`, `Spectator password required or incorrect.`, `Server spectator limit is full.`, `Password required or incorrect.` | done | net/q2_net_server.cpp (texts carry an extra `Connection refused.` line, S22) |

## dmflags bits

Bits read in every mode: 4, 64, 128, 256, 8192 (except removing ammo and the BFG from the map);
the rest are deathmatch only (R23).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| dmf.1 | no health | health items, adrenaline and the ancient head removed from the map | done | q2_server.cpp; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.2 | no powerups | powerups removed from the map | done | q2_server.cpp; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.4 | weapons stay | a taken weapon stays for others (a holder cannot take it again); `drop` of a weapon silently ignored; every mode | done | q2_drops.cpp, q2_server.cpp: read in every mode |
| dmf.8 | no falling damage | | done | q2_fight.cpp; held by `tests/shooters_edge_q2_fight_test.cpp` |
| dmf.16 | instant powerups | powerups used on pickup | done | q2_server.cpp; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.32 | same map | the level's end reloads the same map | done | q2_fight.cpp (R13); held by `tests/shooters_edge_q2_fight_test.cpp` |
| dmf.64 | skin teams | teams by the whole skin string | done | q2_fight.cpp: every mode; slashless skin a team |
| dmf.128 | model teams | teams by the model part before `/` | done | q2_fight.cpp: every mode |
| dmf.256 | no friendly fire | teammates take no damage (with 64 or 128) | done | q2_fight.cpp: every mode (co-op with 64/128) |
| dmf.512 | spawn farthest | the spot farthest from the nearest living player | done | q2_server.cpp; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.1024 | force respawn | a dead player respawns without a press | done | q2_server.cpp; the press rule without it differs (R9) |
| dmf.2048 | no armour | armour and power armour removed | done | q2_server.cpp; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.4096 | allow exit | a changelevel in deathmatch ends the level with `<name> exited the level.`; without it the exit kills (`found a way out`) | done | q2_map.cpp: without it the exit strikes 10 x max health, cause exit |
| dmf.8192 | infinite ammo | no ammo spent, weapons give 1000, ammo and the BFG removed in deathmatch | done | q2_fight.cpp, q2_server.cpp, q2_drops.cpp: every mode; the map's ammo and BFG removed in deathmatch only; held by `tests/shooters_edge_q2_pickups_test.cpp` |
| dmf.16384 | quad drop | the quad dropped at death | done | q2_fight.cpp; the spread of the two drops differs (R29) |
| dmf.32768 | fixed fov | every player's fov forced to 90 | done | q2_fight.cpp |

## Items (the item list, in its order; each is also a spawnable classname)

Every item's pickup rules, respawn times, sounds and icons were checked and found the same by
the R audit ("Checked and the same": armour, health, powerups, keys, weapons' ammo and
auto-switch, respawn times, instant items). Ours: games/shooters_edge/server/q2_items.hpp (the
table), q2_server.cpp (pickups), q2_use.cpp (use), q2_fight.cpp (drop, weapons),
q2_combat.hpp (weapons' numbers). Rows are partial only for an item-specific gap. Item teams
(the `team` key on items) are a rule row (rule.item-teams).

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| item.item_armor_body | Body Armor | armour 100 (max 200), protects 0.8 normal / 0.6 energy; not droppable | done | q2_items.hpp |
| item.item_armor_combat | Combat Armor | armour 50 (max 100), 0.6 / 0.3; not droppable | done | q2_items.hpp |
| item.item_armor_jacket | Jacket Armor | armour 25 (max 50), 0.3 / 0; not droppable | done | q2_items.hpp |
| item.item_armor_shard | Armor Shard | +2 armour on any armour (jacket when none); not droppable | done | q2_items.hpp |
| item.item_power_screen | Power Screen | front screen, cells absorb damage; usable (toggle), droppable (switched off first if last and on) | done | q2_drops.cpp: droppable, switched off first when the last and on |
| item.item_power_shield | Power Shield | all-round shield, cells absorb damage; usable, droppable; `give` always adds one more | done | q2_drops.cpp droppable; q2_commands.cpp give always one more |
| item.weapon_blaster | Blaster | always held, never placed in a map (no world model, no pickup); `give` answers `non-pickup item` | done | `games/shooters_edge/server/q2_combat.hpp`, `games/shooters_edge/server/q2_commands.cpp` |
| item.weapon_shotgun | Shotgun | uses 1 shell; pickup gives its ammo; stays in co-op | done | q2_drops.cpp: drop gives no ammo; not droppable in co-op |
| item.weapon_supershotgun | Super Shotgun | uses 2 shells | done | as weapon_shotgun |
| item.weapon_machinegun | Machinegun | uses 1 bullet, climbs | done | as weapon_shotgun |
| item.weapon_chaingun | Chaingun | spins up, 1-3 bullets a frame | done | as weapon_shotgun |
| item.ammo_grenades | Grenades | ammo AND the hand grenade weapon; 5 a box; first grenades switch to the hand grenade | done | q2_fight.cpp fight_took: none held before the pickup |
| item.weapon_grenadelauncher | Grenade Launcher | uses 1 grenade | done | as weapon_shotgun |
| item.weapon_rocketlauncher | Rocket Launcher | uses 1 rocket | done | as weapon_shotgun |
| item.weapon_hyperblaster | HyperBlaster | uses 1 cell a shot | done | as weapon_shotgun |
| item.weapon_railgun | Railgun | uses 1 slug | done | as weapon_shotgun |
| item.weapon_bfg | BFG10K | uses 50 cells; removed from deathmatch maps under dmflags 8192 | done | as weapon_shotgun |
| item.ammo_shells | Shells | 10 a box; droppable | done | q2_items.hpp |
| item.ammo_bullets | Bullets | 50 a box | done | q2_items.hpp |
| item.ammo_cells | Cells | 50 a box (also power armour's fuel) | done | q2_items.hpp |
| item.ammo_rockets | Rockets | 5 a box | done | q2_items.hpp |
| item.ammo_slugs | Slugs | 10 a box | done | q2_items.hpp |
| item.item_quad | Quad Damage | ×4 damage 30 s; respawn 60 s; dropped at death under dmflags 16384 | done | q2_drops.cpp: death drops 45 degrees apart |
| item.item_invulnerability | Invulnerability | 30 s; respawn 300 s | done | q2_use.cpp |
| item.item_silencer | Silencer | 30 shots quieter | done | q2_use.cpp |
| item.item_breather | Rebreather | 30 s air; stays in co-op and cannot be dropped there | done | q2_server.cpp stays in co-op; q2_drops.cpp not droppable there |
| item.item_enviro | Environment Suit | 30 s slime/lava protection; stays in co-op | done | as item_breather |
| item.item_ancient_head | Ancient Head | +2 max health (and health) | done | q2_server.cpp |
| item.item_adrenaline | Adrenaline | +1 max health, health to max | done | q2_server.cpp |
| item.item_bandolier | Bandolier | raises bullet/shell/cell/slug limits, gives some | done | q2_server.cpp |
| item.item_pack | Ammo Pack | raises every ammo limit, gives some; respawn 180 s | done | q2_server.cpp |
| item.key_data_cd | Data CD | key; one each in co-op, stays | done | q2_drops.cpp: droppable outside co-op |
| item.key_power_cube | Power Cube | key; co-op marks per cube; only key allowed spawnflags | done | q2_campaign.cpp: a unit's end clears the marks; droppable outside co-op |
| item.key_pyramid | Pyramid Key | key | done | q2_drops.cpp |
| item.key_data_spinner | Data Spinner | key | done | q2_drops.cpp |
| item.key_pass | Security Pass | key | done | q2_drops.cpp |
| item.key_blue_key | Blue Key | key | done | q2_drops.cpp |
| item.key_red_key | Red Key | key | done | q2_drops.cpp |
| item.key_commander_head | Commander's Head | key, shown with the gib effect instead of rotating | done | q2_drops.cpp |
| item.key_airstrike_target | Airstrike Marker | key | done | q2_drops.cpp |
| item.health | Health (no classname) | the item behind `give health`, and the four item_health* spawns: health 10 (small 2, large 25, mega 100 past max) | done | q2_items.hpp, q2_server.cpp |
| item.spawnflags-check | item spawnflags | any spawnflags on an item other than a power cube are cleared, with a console line `<class> at <pos> has invalid spawnflags set` | done | q2_server.cpp: cleared with the console line |

## Spawnable entities (the spawn table; items above and monsters below are spawnable too)

An unknown classname prints `<class> doesn't have a spawn function` to the console and is
removed. Entities left out by skill, deathmatch or co-op spawnflags are removed (`<n> entities
inhibited.` on the console). The world and monster audits (q2-sp-world.md 1-51,
q2-sp-monsters.md 1-68) are all landed per q2-sp-gaps.md; rows below are done unless a later gap
row names a difference.

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| ent.item_health | item_health | 10 health, up to max (spawned through the Health item; removed by dmflags 1) | done | q2_items.hpp |
| ent.item_health_small | item_health_small | 2 health, may pass max | done | q2_items.hpp |
| ent.item_health_large | item_health_large | 25 health | done | q2_items.hpp |
| ent.item_health_mega | item_health_mega | 100 health past max, drains back after 5 s, respawn 20 s after the drain | done | q2_items.hpp, q2_server.cpp |
| ent.info_player_start | info_player_start | single-player start, named starts for level changes | done | `games/shooters_edge/server/q2_server.cpp`, `games/shooters_edge/server/q2_campaign.cpp` (`campaign_unnamed_starts`) |
| ent.info_player_deathmatch | info_player_deathmatch | deathmatch spawn: a solid pad with the dmspot model; removed outside deathmatch; random spot avoids the "two nearest" | done | q2_map.cpp pads; q2_server.cpp the pair quirk and the spawn telefrag |
| ent.info_player_coop | info_player_coop | co-op spawn by slot order; renamed on 18 maps; three added on security | done | q2_campaign.cpp (R6 landed) |
| ent.info_player_intermission | info_player_intermission | intermission camera spot (one of the first four, random); fact1's camera moved | done | q2_fight.cpp fov 90 |
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
| ent.func_timer | func_timer | fires its targets every wait ± random | done | q2_map.cpp; held by `tests/shooters_edge_q2_targets_test.cpp` |
| ent.func_explosive | func_explosive | breakable brush, debris, explosion; obituary `blew up` | done | q2_targets.cpp (world 6) |
| ent.func_killbox | func_killbox | kills everything inside when used | done | q2_targets.cpp |
| ent.trigger_always | trigger_always | fires its targets at map start | done | q2_map.cpp |
| ent.trigger_once | trigger_once | fires once (touch or use), message centreprinted | done | `games/shooters_edge/server/q2_map.cpp`, `games/shooters_edge/server/q2_server.cpp` (the `command` map's `*27`) |
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
| ent.target_changelevel | target_changelevel | ends the level to a map (`*` ends a unit); deathmatch exit rules | done | q2_map.cpp: the deathmatch exit kills without 4096 |
| ent.target_secret | target_secret | counts a secret, sound; mine3's message fix | done | q2_map.cpp (world 19) |
| ent.target_goal | target_goal | counts a goal; the last goal stops the music | done | q2_map.cpp (world 24) |
| ent.target_splash | target_splash | sparks, water, blood etc.; may hurt | done | q2_targets.cpp (world 21) |
| ent.target_spawner | target_spawner | spawns an entity on use (monster spawns); fact2's moved 8 units down | done | `games/shooters_edge/server/q2_targets.cpp`, `games/shooters_edge/server/q2_server.cpp` (fact2's spot) |
| ent.target_blaster | target_blaster | fires a blaster bolt on use | done | q2_targets.cpp; held by `tests/shooters_edge_q2_targets_test.cpp` |
| ent.target_crosslevel_trigger | target_crosslevel_trigger | sets a unit-wide flag | done | q2_map.cpp |
| ent.target_crosslevel_target | target_crosslevel_target | fires when the unit's flags match | done | q2_map.cpp |
| ent.target_laser | target_laser | a damaging laser beam; obituary `saw the light` | done | q2_targets.cpp (world 5) |
| ent.target_help | target_help | sets the help computer's lines, flashes the help icon | done | q2_map.cpp |
| ent.target_lightramp | target_lightramp | ramps a light style over time | done | q2_targets.cpp |
| ent.target_earthquake | target_earthquake | shakes players, rumble sound | done | q2_targets.cpp |
| ent.target_character | target_character | one digit of a target_string | done | q2_targets.cpp |
| ent.target_string | target_string | shows text on target_characters | done | q2_targets.cpp |
| ent.worldspawn | worldspawn | sky, skyaxis, skyrotate, sounds/music (CD track), gravity, message (level name), nextmap; the status bar; precaches | done | `games/shooters_edge/server/q2_server.cpp` |
| ent.viewthing | viewthing | developer: an animating model, `viewthing spawned` on the console | done | q2_targets.cpp: the banner model's frames 0-6 looped at 10 a second from half a second in, a 32x32x56 solid box; q2_lines.cpp says `viewthing spawned` |
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
| ent.misc_actor | misc_actor | a scripted figure that walks a path, says its lines and fights back; `untargeted misc_actor at <pos>`, `misc_actor with no target at <pos>`, `misc_actor has bad target <t> at <pos>` | done | q2_actor.cpp on the monster rules (its row in q2_monster_kinds.hpp): stands until used, walks its points, its lines at spawn and use, the taunts, fights only what a point or a monster's blow names; no stock map places one |
| ent.target_actor | target_actor | a misc_actor's path point: its message, jump, shoot and attack flags; `target_actor with no targetname at <pos>` | done | q2_actor.cpp actor_arrive: message to all as chat, JUMP, SHOOT (nothing), ATTACK with HOLD, the pathtarget fired, on to its target; the no-targetname line in q2_lines.cpp |
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

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| msg.obit.suicide | `<v> suicides.` | the `kill` command | done | games/shooters_edge/server/q2_combat.cpp (R1 landed) |
| msg.obit.falling | `<v> cratered.` | a fall | done | q2_combat.cpp |
| msg.obit.crush | `<v> was squished.` | crushed by a mover | done | q2_combat.cpp |
| msg.obit.water | `<v> sank like a rock.` | drowned | done | q2_combat.cpp |
| msg.obit.slime | `<v> melted.` | slime | done | q2_combat.cpp |
| msg.obit.lava | `<v> does a back flip into the lava.` | lava | done | q2_combat.cpp |
| msg.obit.explosive | `<v> blew up.` | exploding wall or barrel | done | q2_combat.cpp |
| msg.obit.exit | `<v> found a way out.` | the deathmatch exit without allow-exit | done | q2_combat.cpp, q2_map.cpp |
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
| msg.obit.telefrag | `<v> tried to invade <k>'s personal space` | telefrag (teleporters; spawns too in the original) | done | q2_targets.cpp targets_telefrag on every placing |
| msg.obit.died | `<v> died.` | any other death; every death in single player | done | q2_fight.cpp (R2 landed) |
| msg.entered | `<name> entered the game` | a player placed (deathmatch every map; else when maxclients > 1) | done | q2_server.cpp (R3 landed) |
| msg.disconnected | `<name> disconnected` | a player leaves | done | q2_server.cpp (R3 landed) |
| msg.timelimit | `Timelimit hit.` | | done | q2_fight.cpp (R4 landed) |
| msg.fraglimit | `Fraglimit hit.` | | done | q2_fight.cpp (R4 landed) |
| msg.exited | `<name> exited the level.` | deathmatch exit with dmflags 4096 | done | q2_map.cpp |
| msg.sidelines | `<name> has moved to the sidelines` | becoming a spectator | done | q2_spectate.cpp |
| msg.joined | `<name> joined the game` | a spectator coming back | done | q2_spectate.cpp |
| msg.spec-password | `Spectator password incorrect.` | changing to spectator with the wrong password | done | q2_spectate.cpp, `spectator 0` stuffed |
| msg.spec-full | `Server spectator limit is full.` (no line end) | changing to spectator when full | done | q2_spectate.cpp, no line end, `spectator 0` stuffed |
| msg.password | `Password incorrect.` | returning to play with the wrong password | done | q2_spectate.cpp, `spectator 1` stuffed |
| msg.chase-none | `No other players to chase.` (centre) | a spectator's attack with nobody to follow | done | q2_spectate.cpp (centre) |
| msg.need-key | `You need the <item>` (centre) | trigger_key without its item | done | q2_targets.cpp |
| msg.counter | `<n> more to go...` (centre) | trigger_counter | done | q2_targets.cpp |
| msg.counter-done | `Sequence completed!` (centre) | trigger_counter's last | done | q2_targets.cpp |
| msg.entity-message | `<message>` (centre, with talk1 sound) | an entity's `message` key when it fires its targets, a door's when touched while locked | done | q2_map.cpp (world 8, 18) |
| msg.power-cells | `No cells for power armor.` | turning on power armour without cells | done | q2_use.cpp |
| msg.no-ammo | `No <ammo> for <weapon>.` | choosing a weapon without ammo | done | q2_fight.cpp |
| msg.not-enough-ammo | `Not enough <ammo> for <weapon>.` | under one shot's worth (super shotgun, BFG) | done | `games/shooters_edge/server/q2_fight.cpp` |
| msg.cant-drop | `Can't drop current weapon` | dropping the only weapon in hand, or the last grenades in hand | done | `games/shooters_edge/server/q2_fight.cpp` |
| msg.use-unknown | `unknown item: <name>` | use/drop of an unknown name | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.not-usable | `Item is not usable.` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.not-dropable | `Item is not dropable.` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.out-of-item | `Out of item: <name>` | use/drop of an item not held; cycleweap/prefweap | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.no-item-use | `No item to use.` | invuse with nothing selected | done | q2_screens.cpp |
| msg.no-item-drop | `No item to drop.` | invdrop with nothing selected | done | q2_screens.cpp |
| msg.give-unknown | `unknown item` | give of an unknown name | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.give-nonpickup | `non-pickup item` | give Blaster | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.god | `godmode ON` / `godmode OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.notarget | `notarget ON` / `notarget OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.noclip | `noclip ON` / `noclip OFF` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.cheats | `You must run the server with '+set cheats 1' to enable this command.` | the cheat refusal | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.usage-teleport | `Usage: teleport x y z` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.usage-spawnentity | `Usage: spawnentity classname x y z <angle_x angle_y angle_z> <flags>` | not-taken | goes with cmd.spawnentity |
| msg.usage-spawnonstart | `Usage: spawnonstart classname` | not-taken | goes with cmd.spawnonstart |
| msg.usage-listentities | `Usage: listentities <all\|ammo\|items\|keys\|monsters\|weapons>` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.usage-cycleweap | `Usage: cycleweap classname1 classname2 .. classnameN` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.usage-prefweap | `Usage: prefweap classname1 classname2 .. classnameN` | done | `games/shooters_edge/server/q2_commands.cpp` |
| msg.flood-wait | `You can't talk for <n> more seconds` | talking while muted | done | q2_commands.cpp |
| msg.flood-lock | `Flood protection: You can't talk for <n> seconds.` (chat level) | the mute starting | done | q2_commands.cpp |
| msg.chat | `Name: text` / `(Name): text` | say / say_team | done | q2_commands.cpp |
| msg.players | `%3i name` lines, `...`, blank, `<n> players` | `players` | done | q2_commands.cpp |
| msg.playerlist | `mm:ss ping score name[ (spectator)]`, `And more...` | `playerlist` | done | q2_commands.cpp |
| msg.wave | `flipoff` / `salute` / `taunt` / `wave` / `point` | `wave`'s echo to the player | done | q2_commands.cpp |
| msg.rejmsg | `Banned.`, `Spectator password required or incorrect.`, `Server spectator limit is full.`, `Password required or incorrect.` | connect refusals the game library sets | done | The game's words; games/shooters_edge/net/q2_net_server.cpp adds `Connection refused.` after a game's refusal, as the server does with a library's |
| msg.intermission-scores | (nothing printed) | the intermission shows the layout only | done | q2_server.cpp prints nothing |
| msg.help-icon-weapon | (help slot weapon icon) | see layout.help-slot | done | q2_bar.cpp |

## Server console lines (what an admin reads; `dprintf`)

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| con.startup | `Game is starting up.` / `Game is <name> built on <date>.` / `==== ShutdownGame ====` | library start and end | done | q2_server.cpp `==== InitGame ====` when the rules are made; net/q2_game.cpp `==== ShutdownGame ====` on quit and restart, printed by engine/server/main.cpp's last drain |
| con.no-spawn | `<class> doesn't have a spawn function` | unknown classname | done | q2_lines.cpp: `<class> doesn't have a spawn function` (and `ED_CallSpawn: NULL classname`), the entity removed; the class list is the game's spawn table and item list |
| con.not-field | `'<key>' is not a field. Value is '<value>'` | unknown entity key | done | q2_lines.cpp key_lines: `ED_ParseEdict: <key> is not a field`, keys matched ignoring case, `_` keys passed over, inhibited entities too |
| con.incomplete-field | `<func>: entity <n>: incomplete '<key>' field` | a vector key with fewer than three numbers | done | q2_lines.cpp key_lines: `ED_ParseField: couldn't parse '<key>'` for a vector key short of three numbers |
| con.teams | `<n> teams with <m> entities.` | mover teams found | done | q2_lines.cpp (the Q2PRO wording, no full stop) |
| con.inhibited | `<n> entities inhibited.` | entities left out by skill or mode | done | q2_lines.cpp (the Q2PRO wording), skill and deathmatch flags only |
| con.invalid-spawnflags | `<class> at <pos> has invalid spawnflags set` | item spawnflags cleared | done | q2_server.cpp with q2_pos |
| con.path-corner | `path_corner with no targetname at <pos>` | done | q2_lines.cpp; left out |
| con.target-missing | `<class> at <pos> target <name> does not exist` | a path's target | done | q2_lines.cpp q2_combat_point_missing on arrival: the pick line, then the line; the target cleared |
| con.viewthing | `viewthing spawned` | done | q2_lines.cpp |
| con.func-wall | `func_wall START_ON without TOGGLE` | done | q2_lines.cpp; the toggle added |
| con.viper | `misc_viper without a target at <pos>` / `<class> without a target at <pos>` | ships | done | q2_map.cpp, `(0 0 0)` |
| con.no-target-count | `<class> with no target at <pos>`, `<class> with no count at <pos>` | done | q2_targets.cpp |
| con.teleporter | `Couldn't find destination` / `teleporter without a target.` | done | q2_lines.cpp at spawn; q2_map.cpp each touch |
| con.turret | `<class> at <pos> needs a target` / `<class> at <pos> has bad item: <item>` | done | q2_targets.cpp needs a target and the pick line; q2_lines.cpp `has bad item` for turret_driver and every monster at spawn |
| con.monster-start | `<class> at <pos> has target with mixed types`, `<class> at <pos> has a bad combattarget <t> : <class> at <pos>`, `<class> can't find target <t> at <pos>`, `<class> in solid at <pos>` | a frame after the load, each monster started | done | q2_lines.cpp q2_start_lines (added by the 2026-11-22 audit) |
| con.droptofloor | `droptofloor: <class> startsolid at <pos>` | an item placed inside solid, removed | done | q2_server.cpp drop_items (added by the 2026-11-22 audit) |
| con.no-distance | `<class> at <pos> with no distance set` | a rotating door or a button | done | q2_map.cpp |
| con.train | `train_next: bad target <t>`, `connected teleport path_corners, see <class> at <pos>`, `train_find: no target`, `train_find: target <t> not found`, `func_train without a target at <pos>` | done | q2_map.cpp train_find, train_next, connected teleports; q2_lines.cpp the lower corner less 1 |
| con.elevator | `elevator used with no pathtarget`, `elevator used with bad pathtarget: <t>`, `trigger_elevator has no target`, `trigger_elevator unable to find target <t>`, `trigger_elevator target <t> is not a train` | done | q2_lines.cpp q2_start_lines says the set-up lines once a frame after the load; q2_map.cpp the use lines at each use |
| con.timer | `func_timer at <pos> has random >= wait` | done | q2_lines.cpp |
| con.combattarget | `<class> at <pos>, combattarget <t> not found` | done | q2_lines.cpp q2_lost_combat on a monster's first enemy: the pick line, then the line, once |
| con.picktarget | `G_PickTarget called with NULL targetname`, `G_PickTarget: target <t> not found` | done | q2_lines.cpp q2_pick_missing before the train, elevator, turret, combat point, combattarget and monster target lines |
| con.usetargets | `Think_Delay with no activator`, `entity was removed while using killtargets`, `WARNING: <class> used itself.`, `entity was removed while using targets` | done | q2_map.cpp `WARNING: Entity used itself.`, both removed-while-using lines, and `Think_Delay with no activator` when a use with nobody (kNoActivator: a blocked door's way back, a self-started clock) is put off |
| con.speaker | `target_speaker with no noise set at <pos>` | done | q2_lines.cpp |
| con.no-message | `<class> with no message at <pos>` | target_help | done | q2_map.cpp |
| con.no-map | `target_changelevel with no map at <pos>` | done | q2_lines.cpp |
| con.bad-target | `<class> at <pos>: <t> is a bad target` | target_laser | done | q2_targets.cpp: `target_laser at <pos>: <t> is a bad target` as the laser starts |
| con.lightramp | `<class> at <pos> target <t> (<class> at <pos>) is not a light`, `<class> target <t> not found at <pos>`, `target_lightramp has bad ramp (<r>) at <pos>`, `<class> with no target at <pos>` | done | q2_lines.cpp the bad ramp and no target at spawn; q2_ramp_light the first use's not-a-light and not-found lines |
| con.untargeted | `untargeted <class> at <pos>` | target_earthquake etc. | done | q2_lines.cpp target_earthquake |
| con.flood-clamp | `flood_msgs lowered to max: 10` | done | q2_commands.cpp |
| con.spawned | `Spawned entity at <x> <y> <z>` | spawnonstart | not-taken | goes with cmd.spawnonstart |
| con.listentities | `<class>: <x> <y> <z>` | listentities | done | `games/shooters_edge/server/q2_commands.cpp` |
| con.bad-movetype | `<class>:<n> has bad movetype: <n>` | not-taken | a guard no map can reach: every entity's move type is set by the game's own code |
| con.connected | `<name> connected` | a client connects | done | q2_server.cpp sq_connect when maxclients is above 1 |
| con.fixed-triggered | `fixed TRIGGERED flag on <class> at <pos>` | g_fix_triggered | done | q2_lines.cpp `(0 0 0)` |
| con.trigger-key | `no key item for trigger_key at <pos>`, `item <i> not found for trigger_key at <pos>`, `<class> at <pos> has no target` | done | q2_lines.cpp |
| con.say-echo | `Name: text` on the dedicated console | chat echoed to the server | done | `games/shooters_edge/server/q2_commands.cpp` `say`: each chat line is pushed to the server's console |

## Layouts: the status bars, the help computer, the scoreboard, the inventory, the stats

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| layout.statusbar-sp | single-player status bar program | health number and icon at the bottom left, ammo, armour, selected item, the pickup icon and name, the timer, the help icon | done | games/shooters_edge/server/q2_bar.cpp |
| layout.statusbar-dm | deathmatch status bar program | the same plus frags at top right, `SPECTATOR MODE` (stat 17) and `Chasing <name>` (stat 16) | done | q2_bar.cpp (the program); the two stats are never set (layout.stat-chase) |
| layout.stat-health | health stat | the health, never below −99, the health icon | done | q2_bar.cpp: never below -99 |
| layout.stat-ammo | ammo stat | the weapon's ammo icon and count | done | q2_bar.cpp |
| layout.stat-armor | armour stat | power armour's cells (flashing icon when on, off when out) else armour | done | q2_bar.cpp |
| layout.stat-selected | selected item stat | the selected item's icon | done | q2_bar.cpp |
| layout.stat-pickup | pickup stat | the last pickup's icon and name for 3 s (0.7 s for a cycleweap switch) | done | `games/shooters_edge/server/q2_bar.cpp`, `games/shooters_edge/server/q2_commands.cpp` (0.7 s) |
| layout.stat-timer | timer stat | quad, invulnerability, suit, rebreather in that order with seconds left | done | q2_bar.cpp |
| layout.stat-help | help slot | the help icon blinking while unread; otherwise, with hand 2 or fov > 91 and the server's `cl_gun` not 2, the weapon icon | done | q2_bar.cpp: the weapon icon with hand 2 or fov over 91 and cl_gun not 2 |
| layout.stat-layouts | layouts bits | bit 1 a layout shown (scoreboard/help, and always when dead or intermission in deathmatch), bit 2 inventory (alive only) | done | q2_bar.cpp |
| layout.stat-frags | frags stat | the score | done | q2_bar.cpp |
| layout.stat-chase | chase stat (16) and spectator stat (17) | the followed player's name configstring, and the spectator flag | done | q2_spectate.cpp spectate_stats: the target's bar, 16 the skin configstring, 17 the flag |
| layout.help | the help computer | `help` picture background; skill name `easy`/`medium`/`hard`/`hard+` top right; level name; the two help lines; ` kills     goals    secrets` and the counts | done | q2_campaign.cpp, q2_screens.cpp refresh |
| layout.scoreboard | the deathmatch/co-op scoreboard | up to 12 players by score, two columns of 6, `tag1` on the viewer, `tag2` on his killer, `client x y n score ping minutes`, refreshed every 32 frames, spectators left off | done | q2_screens.cpp `q2_scoreboard` (ping capped at 9999 in ours: harmless) |
| layout.inventory | the inventory | the server sends every item's count; the client lists held items with the selection | done | q2_screens.cpp |
| layout.intermission | the intermission view | frozen at the spot, no gun or model, scoreboard in deathmatch and co-op, fov 90; leaves on a button after 5 s, no time limit | done | q2_server.cpp: no time limit while a person plays (bots alone: 20 s), no text scores; fov 90 |
| layout.dead-view | the dead player's view | looks at his killer, lies on the floor, the scoreboard in deathmatch | done | q2_bar.cpp |

## Rules a player sees (behaviour not in a row above)

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| rule.spawn-telefrag | spawn telefrag | placing a player kills whatever is in his box (100 000 damage, obituary telefrag, +1) | done | q2_targets.cpp targets_telefrag from spawn_player |
| rule.respawn-press | deathmatch respawn | after 1 s, on a FRESH press of attack (held through death does not count); dmflags 1024 forces | done | q2_server.cpp: a fresh press since death, first chance after 1 s |
| rule.coop-respawn | co-op respawn | after 1 s on a fresh press of ANY button; body left, teleport effect, 14×8 ms hold; back as he entered the level, keys kept | done | q2_server.cpp: any fresh button; teleport effect and hold |
| rule.sp-death | single player death | any button after 1 s asks for the load menu | done | q2_server.cpp |
| rule.intermission-leave | leaving the intermission | after 5 s, the first button from anyone; no time limit | done | q2_server.cpp: no time limit while a person plays (a bots-only match leaves after 20 s) |
| rule.intermission-spot | the intermission spot | one of the first four `info_player_intermission`, else start or deathmatch spot | done | q2_server.cpp |
| rule.next-map | the next map | dmflags 32, sv_maplist (case ignored, wrap), the world's nextmap, the first changelevel, the same map | done | q2_fight.cpp (R13 landed) |
| rule.limits | fraglimit / timelimit tests | any client's score reaches the limit; level time ≥ minutes × 60 | done | q2_fight.cpp (R4 landed) |
| rule.dm-exit | deathmatch exits | without 4096 a changelevel kills its toucher (10 × max health, cause exit, −1) | done | q2_map.cpp |
| rule.spectator-change | spectating mid-match | the `spectator` setting applied 5 s after the last respawn, passwords and the limit checked, score reset, announced | done | q2_spectate.cpp: 5 s from the respawn mark (death + 1 s, a respawn, a switch), the spectators' password then the limit (every wish counted, his own too), score reset, said to all; a respawn takes the wish unchecked |
| rule.spectator-sp | spectator outside deathmatch | ignored in co-op and single player | done | q2_spectate.cpp spectate_wish: deathmatch only |
| rule.chase | the chase camera | spectators follow players: attack starts/stops, jump and invnext/invprev step; the camera's place; the bar shows `Chasing <name>` | done | q2_spectate.cpp: attack starts/ends, the up move starts or steps, invnext/invprev step; 30 back, 20 up (16 more in the air), off walls, ceiling and floor; dead target lying at roll 40 |
| rule.item-teams | item teams | items sharing `team` show one random member at a time | done | `games/shooters_edge/server/q2_server.cpp` (`show_item`) |
| rule.drop-weapon-ammo | dropped weapons | a `drop`ped weapon gives no ammo; one dropped at death gives its usual | done | q2_drops.cpp |
| rule.coop-score | co-op score | +1 a monster killed, carried across levels, kept over respawn; deaths change nothing | done | q2_monsters.cpp: +1 a monster |
| rule.coop-stay | co-op stays | weapons, keys, rebreather and suit stay when taken; one each | done | q2_server.cpp |
| rule.coop-nodrop | co-op no-drop | the staying items answer `Item is not dropable.` | done | q2_drops.cpp fight_droppable |
| rule.coop-unit-end | a unit's end in co-op | every key and the cube marks cleared | done | q2_campaign.cpp strip_keys clears the cube marks |
| rule.dmflags-coop | dmflags in co-op | 4, 64, 128, 256, 8192 read in every mode | done | q2_fight.cpp fight_anyflag |
| rule.coop-carry | co-op carried state | each player's carried state and co-op score kept across a level change | done | R7 landed |
| rule.coop-spots | co-op spots | slot order, renames, security's three, the 550-unit fallback | done | q2_campaign.cpp (R6 landed) |
| rule.dm-spots | deathmatch spots as pads | the dmspot disc, solid | done | q2_map.cpp (R5 landed) |
| rule.random-spot | random spot | the "two nearest" left out as the walk finds them (not exactly the two nearest) | done | q2_server.cpp spawn_spot |
| rule.farthest-spot | farthest spot | dmflags 512: farthest from the nearest living player | done | q2_server.cpp |
| rule.death-drops | death drops | the weapon in hand and, under 16384, the quad (used at once with its time left); the two 45° apart | done | q2_drops.cpp: 45 degrees apart, the weapon to the right |
| rule.drop-start | a dropped item's start | 24 ahead along the full view, 16 below the origin, traced; box ±16, a random turn up to 45° | done | q2_drops.cpp toss: 24 along the full view, 16 below the origin, box +-16; the random yaw is not taken (dropped items turn by the clock, so it never shows) |
| rule.give-details | give's details | `give health n` at least 1; `give all` and `give Power Shield` always add one | done | q2_commands.cpp |
| rule.flood | flood protection | deathmatch and co-op only; lines and quote stripping | done | q2_commands.cpp |
| rule.kill-first-spawn | kill after a first placing | the 5 s counted from the map's start on a first placing | done | q2_server.cpp |
| rule.unnamed-starts | eight maps' unnamed starts | mine1..city2 get an unnamed copy of a named start | done | `games/shooters_edge/server/q2_campaign.cpp` |
| rule.map-fixes | fact2 spawner, fact1 camera, command `*27` | three built-in map repairs | done | `games/shooters_edge/server/q2_server.cpp` |
| rule.map-fixes-landed | mine3 secret, fact1 secret exit, jail5 flyers, the 18 co-op renames, security spots | built-in repairs | done | q2_map.cpp, q2_monsters.cpp, q2_campaign.cpp |
| rule.music | the world's `music` key | the CD track configstring by name when set | done | `games/shooters_edge/server/q2_server.cpp` |
| rule.first-grenades | first grenades | taking grenades with none held switches to the hand grenade (deathmatch: from the blaster only) | done | q2_fight.cpp |
| rule.login-flash | login flash | deathmatch always, else only when maxclients > 1 | done | q2_server.cpp `announce_entry` (R49, landed with R3) |
| rule.teams | teams | by skin (64) or model (128); friendly fire −1; say_team | done | q2_fight.cpp |
| rule.pickups | pickup rules | armour, health, mega drain, adrenaline, head, bandolier, pack, powerup skill limits, keys, weapons' ammo and auto-switch | done | q2_server.cpp (R "checked and the same") |
| rule.respawn-times | item respawns | armour 20 s, weapons and ammo 30 s, powerups by item; the respawn event; instant items | done | q2_server.cpp |
| rule.power-armor | power armour | use and sounds, switched off when cells run out, first one auto-on in deathmatch | done | q2_use.cpp |
| rule.weapons | weapons | every weapon's frames, damage, spread, kick, quad sound, silencer, the hand grenade's cook, g_swap_speed | done | q2_fight.cpp, q2_combat.hpp (monsters audit 2-5, view notes) |
| rule.player-world | the world on the player | falling damage and sounds, drowning and air, lava and slime with the suit, footsteps, jump sound, pain sounds by health | done | q2_fight.cpp (q2-player-sounds.md) |
| rule.view | the player's view | damage blend and kick, powerup tints and their warning blinks, underwater tint, bob, roll, fall dip, gun bob | done | q2_blend.cpp, q2_server.cpp (world 27; q2-view.md) |
| rule.save | single-player saves and level keeping | save and load, levels kept within a unit, the autosave at each level | done | q2_save.cpp, q2_campaign.cpp |
| rule.unit-end-sp | the unit's end in single player | the intermission and the movie, keys stripped | done | q2_campaign.cpp (world 1) |

