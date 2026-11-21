# Quake II rules: the second gap list (2026-11-21)

Niclas, 2026-11-21: the Quake and Quake II gaps are filled before work starts on other games.
This is the game-rules half of the fresh gap analysis (single player after the last audit,
co-op, deathmatch, the client commands the game library answers); the server and client halves
are other notes. The list is worked through rank 1 first, in small PRs, until it is empty; a
landed row is recorded in a table at the end with its PR, as `q2-sp-gaps.md` does.

**Where these facts came from**: a clean-room reader read the game library of Yamagi Quake II
(yquake2, baseq2, master at commit a1d7e2b, 2026-11-16) and wrote what it does in our words, no
code. **[read]** read directly, **[derived]** worked out from several things read, **[ours]** what
our tree does (read from /home/user/ge). Messages are quoted exactly because they are what a
player sees. Ranks: **1** noticed at once in normal play, **2** noticed sometimes, **3** rare or
cosmetic. Areas: SP, coop, DM, commands.

Corrections to older notes found on the way: `q2-campaign.md`'s co-op spot repair radius is 550
units in this release (not 384); `q2-rules.md` is wrong that co-op always allows the cheats
(R24), that `give health n` has no floor (R39), that the flood lock line has two spaces (R40), on
the death drops' spread (R29) and the drop's start (R30), and it leaves out `cl_gun` from the
weapon icon in the help slot (R38).

## Status of each question

- A1 (single player after the last audit): answered. The SP world and monster audits hold where
  spot-checked (trigger_key and its co-op rules, the counter's lines, the secret on mine3, the
  fact1 secret exit). What is left is the death line, a hard-skill map fix, the screens' refresh
  and a handful of cosmetic rows: R2, R27, R35 to R38, R42 to R44, R48, R49.
- A2 (co-op): answered. Most of the co-op rules in `q2-campaign.md` § Co-op were never built:
  R2, R6, R7, R10, R19 to R23, R46.
- A3 (deathmatch): answered. R1, R3 to R5, R8, R9, R11 to R18, R29, R30, R32 to R34, R45, R47.
  This release has no `id` command and no CTF teams; teams are only the skin and model teams of
  `dmflags` 64 and 128.
- A4 (client commands): answered. R24 to R26, R28, R31, R39 to R41.

## Rank 1

**R1 (1, DM) The obituary lines are worded differently, every one of them.** [read] In
deathmatch and co-op the game prints one line to everyone at the medium print level. A death
the victim caused himself, or the world caused, reads `<victim> <words>.` (with a full stop); a
death another player caused reads `<victim> <words> <killer><tail>` with NO full stop. The
words by cause:

- World and self, whoever the attacker: suicide (the `kill` command) `suicides`; a fall
  `cratered`; crushed by a mover `was squished`; drowned `sank like a rock`; slime `melted`; lava
  `does a back flip into the lava`; an exploding wall, an exploding barrel `blew up`; the
  deathmatch exit without allow-exit `found a way out`; target_laser `saw the light`; a target
  blaster `got blasted` (never produced in this release, our note `q2-world-round4.md` agrees); a
  trigger_hurt, a splash or a bomb `was in the wrong place`.
- Killed by himself with a weapon: holding a cooked grenade `tried to put the pin back in`; his
  own grenade's or hand grenade's blast `tripped on his own grenade`; his own rocket's blast
  `blew himself up`; his own BFG ball's blast `should have used a smaller gun`; anything else
  `killed himself`. "his" and "him" follow the `gender` user setting: a value starting with
  `f` or `F`, or containing `crakhor`, gives `her`/`herself`; one starting with `m` or `M` gives
  `his`/`himself`; anything else, an empty value included, gives `its`/`itself`.
- Killed by another player (words, then the tail after the killer's name): blaster `was blasted
  by`; shotgun `was gunned down by`; super shotgun `was blown away by` + `'s super shotgun`; machinegun
  `was machinegunned by`; chaingun `was cut in half by` + `'s chaingun`; grenade
  launcher, direct hit `was popped by` + `'s grenade`, blast `was shredded by` + `'s shrapnel`; rocket,
  direct `ate` + `'s rocket`, blast `almost dodged` + `'s rocket`; hyperblaster `was
  melted by` + `'s hyperblaster`; railgun `was railed by`; BFG laser `saw the pretty lights
  from` + `'s BFG`, BFG ball's blast `was disintegrated by` + `'s BFG blast`, BFG burst `couldn't
  hide from` + `'s BFG`; hand grenade, direct `caught` + `'s handgrenade`, blast `didn't see` +
  `'s handgrenade`, held too long `feels` + `'s pain`; telefrag `tried to invade` + `'s personal
  space`.
- Anything else (no player attacker and no world cause): `<victim> died.`
- Scores in deathmatch [read]: a world or self line takes 1 from the victim; a killer line gives
  the killer 1, or takes 1 from him when friendly fire (a teammate under `dmflags` 64 or 128); the
  `died.` line takes 1 from the victim.

Ours [ours]: our own wording (`q2_combat.cpp`, `q2_fight.cpp`): one line per weapon whatever the
hit was (no direct and blast split: the original has 18 killer lines, ours 11 weapon lines and
a few others), every line ends with a full stop,
no gender, self deaths `blew themselves up` / `gave up` / `held on to a grenade too long`, world
deaths `fell too far`, `sank into the lava` and so on. `q2-rules.md` § Deathmatch says "our
wording is our own"; if 1-1 is the goal that decision is reversed by this row.

**R2 (1, SP; also coop) Deaths outside deathmatch.** [read] In single player every player death
prints `<name> died.` and nothing else (the world and weapon lines above are deathmatch and
co-op only). In co-op the world and self lines and the killer lines of R1 apply (a killer line
for a co-op mate's kill, marked friendly fire, no score change), and a death by a monster is
`<name> died.` (monsters have no lines of their own in this release). Ours [ours]: a monster's
kill reads `<name> was killed by <monster>.`, and the world and self lines are ours in every
mode.

**R3 (1, DM) Players entering and leaving are announced.** [read] At the high print level, to
everyone, no full stop: `<name> entered the game` each time a player is placed in a deathmatch,
which is on joining AND at the start of every map (every player is re-entered when the level
changes); in co-op and single player the same line only when `maxclients` is above 1.
`<name> disconnected` when a player leaves (any mode). The entry also throws the login flash at
him (deathmatch always; otherwise only when `maxclients` is above 1), the leaving the logout
flash. Ours [ours]: the flashes exist; the two lines go only to the server's own log
(`net/q2_net_server.cpp`'s note), never to the players.

**R4 (1, DM) The limit lines.** [read] When `timelimit` (minutes) is reached: `Timelimit hit.`; when
any client's score reaches `fraglimit`: `Fraglimit hit.`; both to everyone at the high level,
then the level ends. Ours [ours]: `Time is up.` and `<name> reached the frag limit.`.

**R5 (1, DM) The deathmatch spawn spots are pads.** [read] In deathmatch every
`info_player_deathmatch` is made exactly as a teleporter destination: it shows the disc model
`models/objects/dmspot/tris.md2` with skin 0 and is a solid box from (−32, −32, −24) to (32, 32,
−16) about its origin, so a player stands on an 8-unit plate and shots hit it. Outside deathmatch
the spot is removed. Every `misc_teleporter_dest` is made the same way whether or not a
teleporter points at it. Ours [ours]: only a destination some teleporter names gets the disc
(`q2_map.cpp`); deathmatch spawn spots are invisible and not solid.

**R6 (1, coop) Co-op spawn spots repaired, and the fallback.** [read] Player n (n from 1 to 3)
of a co-op game takes the n-th `info_player_coop` whose name matches the level's spawn point
name (ours does this). This release adds three repairs:

- On 18 maps (jail2, jail4, mintro, mine1, mine2, mine3, mine4, lab, boss1, fact1, fact3,
  waste1, biggun, space, command, power2, strike, city2) each co-op spot, 0.1 s after the map
  starts, takes the name of the first named `info_player_start` (in map order) within 550 units
  of it, replacing its own name. Note: `q2-campaign.md` says 384 units; this release uses 550.
- On the map `security`, in co-op only, three co-op spots are added 0.1 s after the start, all
  named `jail3`, facing yaw 90, at (124, −164, 80), (252, −164, 80) and (316, −164, 80).
- When a co-op player other than the first still ends up on an `info_player_start` (no matching
  co-op spot), a fallback runs, and as written it looks at the map's FIRST co-op spot only, again
  and again (derived): when that spot is within 550 units of the start, players 1 and 2 are both
  put on it (and the second telefrags the first, R8) while player 3 stays on the start; when it
  is 550 units or farther the search never ends and the server hangs (derived, untested). The
  intent is plainly "the n-th co-op spot within 550 units"; which of the two to copy is a
  decision for us, not a fact.

Ours [ours]: none of the three; players 2 to 4 without a matching spot stand on the start, and
since ours has no spawn telefrag (R8) they stand inside each other.

**R7 (1, coop) Over the network, a co-op player's carried state is lost at every level change.**
[ours, derived from our code; confirm with a test] When the level changes, the network server
(`net/q2_net_server.cpp`) re-seats each client on its `begin` by calling the rules' connect,
and our Quake II rules' connect (`q2_server.cpp`) rebuilds the player record from nothing
unless the slot was restored from a save. That throws away the carried body (health, armour,
weapons, ammo, keys) and the score before the player is placed. The listen server in the window
(`net/qw_game.cpp`, the `se-q2-coop` lab) keeps them. [read] The original keeps each player's
whole carried state and his co-op score across a level change.

## Rank 2

**R8 (2, DM) A spawn telefrags.** [read] Whenever a player is placed (joining, respawning, a
spectator coming back), everything solid in his box is struck with 100 000 points that no armour,
god mode or invulnerability stops, the spawning player as attacker, telefrag as the cause
(obituary `tried to invade <spawner>'s personal space`, +1 to the spawner in deathmatch); this
repeats until the box is clear. A dead player still lying there and a body in the body queue
are solid too, so they are thrown apart (derived). Monsters are killed the same way. Ours
[ours]: teleporters telefrag (`q2_map.cpp`), a spawn does not; two players can stand in one
another.

**R9 (2, DM) Respawning needs a fresh press of attack.** [read] A dead player respawns on the
first server frame after 1 s has passed (strictly later: the first chance is 1.1 s after death,
derived from the 0.1 s frame) once attack has been pressed since he died; holding attack through
the death does not count, and a press during that first second does count once it is over
(derived). `dmflags` 1024 respawns him at that frame without a press. Ours [ours]: attack held
is enough (`q2_server.cpp`), and the first chance is at exactly 1.0 s.

**R10 (2, coop) The co-op respawn.** [read] A dead co-op player respawns, after the same 1 s,
on a fresh press of ANY button (not only attack). He leaves his body in the queue, shows the
teleport effect where he appears and is held still for 14 × 8 ms, as in deathmatch. He comes
back as he entered the level (health, armour, weapons, ammo, power-cube bits), except that the
keys he held when he died are kept, and the help computer's unread state carries over. Ours
[ours]: only attack respawns a co-op player; no teleport effect and no hold in co-op; the
entry body and keys are kept as described.

**R11 (2, DM) The intermission never leaves by itself.** [read] After 5 s (strictly more) the
first command with any button held, from any player, moves everyone on; there is no time limit.
Ours [ours]: in deathmatch it leaves by itself after 20 s.

**R12 (2, DM) No text score list at the intermission.** [read] The intermission shows each
player the scoreboard layout (deathmatch and co-op) and prints nothing. Ours [ours]: also
prints every player's frags and name as text lines to everyone.

**R13 (2, DM) Which map comes next.** [read] In this order: with `dmflags` 32 the same map; else
when `sv_maplist` is set, its words (split at spaces, commas, carriage returns and line feeds)
are searched for the current map's name ignoring case; found, the next word is the next map,
the first word when it was the last; NOT found, the list is ignored; then the world's `nextmap`
key; then the map's first `target_changelevel` (its whole map string); then the same map. Ours
[ours]: a map not in the list goes to the list's first word; with no list (or not found) the
world's `nextmap` and the changelevel are never read, so it is always the same map; names are
compared with case.

**R14 (2, DM) A deathmatch exit without allow-exit kills.** [read] Without `dmflags` 4096 a
`target_changelevel` fired in deathmatch strikes its toucher with ten times his maximum health
(1000 for a player at 100) and knockback 1000, armour applying, cause "exit": obituary
`<name> found a way out.` and −1. With 4096: `<name> exited the level.` to everyone and the
level ends. Ours [ours]: without 4096 nothing happens (`q2_map.cpp`); with it, the same line.

**R15 (2, DM) Becoming a spectator, or a player again, during a match.** [read] Spectators exist
only in deathmatch. A player changes his `spectator` user setting; at the start of a frame, once
5 s have passed since his last respawn time, the change is applied:

- To spectator: when `spectator_password` is set (not empty, not `none`) the value he set must
  equal it, else `Spectator password incorrect.` to him and his client is told `spectator 0`.
  When the spectators already number `maxspectators` or more: `Server spectator limit is full.`
  (no line end) and `spectator 0`.
- Back to play: when `password` is set his `password` setting must equal it, else
  `Password incorrect.` and his client is told `spectator 1`.
- Then his score is set to 0, he is placed again (a spectator flies through walls, is not
  solid, not sent, has no weapon), and everyone reads `<name> has moved to the sidelines` or
  `<name> joined the game` (high level, no full stop). A returning player gets the login flash
  and the 14 × 8 ms hold.

Ours [ours]: spectator is fixed at connect (`net/q2_net_server.cpp`); a later change of the
setting does nothing (`q2_server.cpp` stores it only).

**R16 (2, DM) The chase camera.** [read] A spectator who presses attack starts following the
first player in slot order who is not a spectator (none: centre print `No other players to
chase.`); attack while following stops it. Jump (up move of 10 or more, once per press) starts
following or steps to the next player; `invnext`, `invnextw`, `invnextp` step to the next,
`invprev`, `invprevw`, `invprevp` to the previous, skipping spectators and wrapping. The camera
sits 30 units behind the followed player's eye along his view, his view pitch capped at 56
degrees downward for this, never lower than 20 units above his origin, 16 higher while he is in
the air, pulled in where a wall is in the way, then moved 2 units forward, and kept 6 units from
a floor or ceiling; it looks along his view, or lies on its side toward his killer when he is
dead; prediction is off. The follower's status numbers are the followed player's, with the
spectator stat (17) set and the chase stat (16) naming the followed player's skin configstring,
which the deathmatch status bar shows as `SPECTATOR MODE` and `Chasing <name>`; his own
scoreboard and inventory still work. A followed player who leaves or turns spectator moves the
camera on, or ends it when nobody is left. Ours [ours]: no chase camera; stats 16 and 17 are
never set although our status bar program reads them (`q2_bar.cpp`).

**R17 (2, DM) Item teams.** [read] Items that share a `team` key form a group: when they have
dropped to the floor all of them are hidden, and 0.1 s later one member chosen at random shows
(with the respawn event). In deathmatch, when the shown one is taken, after its respawn time one
member chosen at random again shows (maybe another). Outside deathmatch only the first random
member ever shows (derived: nothing respawns there). Which stock maps use item teams is map
data the source does not hold. Ours [ours]: the `team` key is read for movers only; every
member of an item team shows.

**R18 (2, DM) A weapon dropped with `drop` carries no ammunition.** [read] A weapon thrown by the
`drop` command, when picked up, gives the weapon only, no ammo; a weapon dropped at death gives
its usual ammo (the weapon's own pickup amount, or 1000 under `dmflags` 8192). Ours [ours]:
both give the ammo (`q2_fight.cpp` hands every drop to the ordinary pickup), so dropping and
taking back a weapon makes ammunition.

**R19 (2, coop) The co-op score.** [read] Each monster (not a good one) a player kills gives him 1
point; the score is shown on the co-op scoreboard, is carried from level to level, and kept over
a respawn (the higher of the kept and the current). Deaths, suicides and killing a co-op mate
change nothing. Ours [ours]: monster kills score nothing (`q2_monsters.cpp`), while a suicide or
a world death takes 1 and killing a mate takes 1 from the killer in every mode
(`q2_fight.cpp`).

**R20 (2, coop) The rebreather and the environment suit stay in co-op.** [read] In co-op these
two, like every weapon and every key, stay in the world when taken, and a player who holds one
already cannot take another (the skill limit on powerups applies first). Ours [ours]: they are
taken and gone; only weapons and keys stay.

**R21 (2, coop) The items that stay cannot be dropped in co-op.** [read] Once such an item has
been spawned in a co-op game, every weapon, the rebreather, the suit and every key answer `drop`
with `Item is not dropable.`. Ours [ours]: weapons and powerups can be dropped in co-op.

**R22 (2, coop) A unit's end also clears the power-cube marks.** [read] At a co-op unit's end
(a changelevel with `*`) every player loses every key AND his record of which power cubes he has
taken; within a unit that record carries from level to level, and cube marks are numbered afresh
on each map, so a mark taken on one level refuses the cube with the same number on the next
level of the unit (derived; the original's own quirk, kept by ours). Ours [ours]: keys are
stripped, the marks are carried into the next unit, so cubes there can be refused.

**R23 (2, coop) `dmflags` reaches beyond deathmatch.** [read] These bits are read in every mode:
4 (weapons stay: a held weapon is left for others, a weapon `drop` is silently ignored), 8192
(infinite ammunition: shots spend none, weapon pickups give 1000; only the removal of ammo and
the BFG from the map is deathmatch-only), 64 and 128 (teams, so in co-op 256 with 64 or 128
stops damage between teammates, and `say_team` goes to the team). Only 1, 2, 2048 (items removed
from the map), 8, 16, 32, 512, 1024, 4096, 16384 and 32768 are deathmatch-only. Ours [ours]:
every bit is read in deathmatch only (`q2_fight.cpp`), so a co-op server's `dmflags 320`
(no friendly fire with skin teams) does nothing; friendly fire in co-op is always on.

**R24 (2, commands) The cheat gate covers co-op.** [read] `give`, `god`, `notarget`, `noclip`,
`teleport`, `spawnentity`, `spawnonstart` and `listentities` are refused in deathmatch AND in
co-op while `cheats` is 0, with `You must run the server with '+set cheats 1' to enable this
command.` at the high level. Single player always allows them. Our note `q2-rules.md` says co-op
always allows them; that is wrong for this release. Ours [ours]: co-op allows them, and the
refusal reads `Cheats are off on this server (cheats 1 turns them on).`.

**R25 (2, commands) The command answers are worded differently.** [read] Exactly, to the player
at the high print level unless said:

- `use`, `drop` (the whole rest of the line is the item's pickup name, case ignored):
  `unknown item: <name>`, `Item is not usable.`, `Item is not dropable.`, `Out of item: <name>`
  (the name as typed).
- Choosing a weapon without its ammunition (unless `g_select_empty`): `No <ammo> for <weapon>.`
  when there is none, `Not enough <ammo> for <weapon>.` when there is less than one shot's worth
  (the super shotgun with 1 shell, the BFG with under 50 cells).
- Dropping the weapon in hand, or the one being switched to, when it is the only one: `Can't drop
  current weapon` (no full stop); the same line for the last grenades while the hand grenade is
  in hand. A weapon `drop` under weapons-stay: nothing at all.
- `give`: `unknown item` (no name, no full stop); the Blaster: `non-pickup item`.
- `god`: `godmode ON` / `godmode OFF`; `notarget`: `notarget ON` / `notarget OFF`; `noclip`:
  `noclip ON` / `noclip OFF`.
- `invuse` / `invdrop`: `No item to use.` / `No item to drop.`, and `Item is not usable.` /
  `Item is not dropable.` for a selected item without that action.

Ours [ours] (`q2_commands.cpp`, `q2_fight.cpp`): `There is no item called <name>.`,
`<name> can't be used.`, `<name> can't be dropped.`, `You have no <name>.`, `No <ammo> for
<weapon>.` for both cases, `You can't drop the weapon in your hand.`, `Weapons stay in this match,
so none can be dropped.`, `The Blaster is always in your hands.`, `God mode on`/`off`,
`Notarget on`/`off`, `Noclip on`/`off`. `invuse` and `invdrop` match.

**R26 (2, commands) `cycleweap` and `prefweap` are missing.** [read] Both take weapon class names
(`weapon_shotgun`, `ammo_grenades` for the hand grenade, case ignored) and are used in key binds.

- `cycleweap <name> ...`: the search starts after the weapon being switched to (else the one in
  hand) in the list, wrapping, and takes the first listed weapon that is not the one in hand,
  is held, and has at least one shot's ammo (1 for grenades). Failing that, the first held one
  short of ammo is tried (its use prints R25's ammo line), else the first not held, which prints
  `Out of item: <pickup name>`. When more than two names are given and the switch was accepted,
  the new weapon's icon and name show in the pickup line for 0.7 s. With no names:
  `Usage: cycleweap classname1 classname2 .. classnameN`.
- `prefweap <name> ...`: the first listed weapon held with enough ammo (the one in hand
  included, which then does nothing); fallbacks and messages as above. With no names:
  `Usage: prefweap classname1 classname2 .. classnameN`.
- Both are ignored during the intermission, like every command but the five chat and screen ones.

Ours [ours]: neither exists, so the word is said as chat.

**R27 (2, SP) The `command` map's trigger on hard.** [read] On the map `command` the
`trigger_once` whose model is `*27` loses its not-on-hard spawn flag before the flags are
applied, so it exists at skills 2 and 3 too. What the trigger does in the map is map data the
source does not hold (it is a repair, so without it a hard game likely misses an event). Ours
[ours]: not present.

## Rank 3

**R28 (3, commands) The debugging commands are missing.** [read] Under R24's gate:

- `teleport x y z` (exactly three numbers, else `Usage: teleport x y z`): the player is moved to
  (x, y, z + 10), stopped, held still for 20 × 8 ms, his view angles cleared, and whatever stands
  there is telefragged.
- `spawnentity <classname> x y z [pitch yaw roll [spawnflags]]` (4 to 8 words after the command,
  else `Usage: spawnentity classname x y z <angle_x angle_y angle_z> <flags>`): makes that entity
  there, as a map would spawn it.
- `spawnonstart <classname>` (exactly one word, else `Usage: spawnonstart classname`): one such
  entity at every `info_player_deathmatch`, every `info_player_coop` and every
  `info_player_start` (those that still exist in the mode), printing `Spawned entity at x y z`
  to the server console for each.
- `listentities <all|ammo|items|keys|monsters|weapons> ...` (one or more words; none prints
  `Usage: listentities <all|ammo|items|keys|monsters|weapons>`, an unknown word prints the usage
  and the rest still apply): prints `<classname>: x y z` for every entity whose class name starts
  `ammo_`, `item_`, `key_`, `monster_` or `weapon_` as asked (`all`: every entity with a class
  name), to the SERVER console, not the player.

Ours [ours]: none.

**R29 (3, DM) The death drops' spread.** [read] When a dying player drops both his weapon and the
quad, the weapon is thrown with his yaw lowered by 22.5 degrees (to his right) and the quad with
it raised by 22.5 (to his left), 45 degrees apart; either alone goes straight ahead. Ours [ours]: ±11.25 (22.5 apart), and our
extra powerup drops (the `sv_drop*` settings) spread the same way.

**R30 (3, DM) A dropped item's start, box and turn.** [read] A dropped item starts 24 units ahead
along the full view (pitch included) and 16 units below the player's ORIGIN (not his eye),
pulled back to where a box of ±16 traced from his origin stops; its box is ±16 (map items ±15); its
yaw is turned a random amount up to 45 degrees either way. Ours [ours]: from 16 below the
eye (22 units higher), box ±15, no turn.

**R31 (3, commands) Keys and power armour can be dropped.** [read] Outside co-op every key can be
dropped (a general drop: one thrown, count down by one). The power screen and shield can be
dropped; when it is the last one and it is switched on, it is first switched off (with its
sound). Armour, the health items, the bandolier and the ammo pack cannot (`Item is not
dropable.`). Ours [ours]: only weapons, ammunition and powerups.

**R32 (3, DM) The random spawn's "two nearest" is not exact.** [read] Walking the spots in map
order, a spot nearer than the nearest so far replaces it WITHOUT the old nearest becoming the
second; a spot nearer than the second (but not the first) replaces the second. Those two are left
out when there are more than 2 spots; one of the rest is chosen uniformly. So the excluded pair is
not always the two nearest (derived). Ours [ours]: the true two nearest.

**R33 (3, DM) A skin with no slash is a team.** [read] For both team bits a `skin` setting with no
`/` is its own team name whole; an empty skin is no team. Ours [ours]: under skin teams a skin
without a slash is no team.

**R34 (3, DM) The scoreboard while dead.** [read] In deathmatch the scoreboard layout is shown
whenever the player is dead or the intermission runs, whether or not he toggled it off; at death
his score toggle flips (so it may end up off), and a respawn clears it. Ours [ours]: a dead
player who had the scoreboard open has it closed.

**R35 (3, SP) The help computer refreshes and simply toggles.** [read] While open it is rebuilt
every 32 frames (kills, goals, secrets live) and marked read; `help` while it is open always
closes it. Ours [ours]: built once at opening; a second `help` after a new message rebuilds it
instead of closing (the older rule).

**R36 (3, SP) The inventory refreshes every frame.** [read] While open the inventory counts are
sent every frame. Ours [ours]: taken once when opened (`q2_screens.cpp`).

**R37 (3, SP) The health number stops at −99.** [read] The status bar's health is the health,
but never below −99. Ours [ours]: down to −999.

**R38 (3, SP) The weapon icon in the help slot.** [read] When the help computer is not blinking,
a player with `hand` 2 or a field of view above 91 gets the current weapon's icon in the help
slot, but only when the server process's `cl_gun` is not 2 (a dedicated server has none, so the
default 2 applies and it never shows there; on a listen server it is the host client's setting).
Ours [ours]: never shown. Our note `q2-rules.md` omits the `cl_gun` condition.

**R39 (3, commands) `give`'s details.** [read] `give health <n>` sets health to n but at least 1.
`give all` and `give Power Shield` always give one more power shield through the pickup rules (a
second, third ...; in deathmatch the first one switches on). Ours [ours]: health is set without
the floor (our note says no clamp, wrong for this release); the shield only when none is held.

**R40 (3, commands) Flood protection.** [read] Only in deathmatch and co-op (single player never
floods). Messages: `You can't talk for <n> more seconds` (no full stop, high level) while locked,
and `Flood protection: You can't talk for <n> seconds.` (capital Y, one space after the colon,
chat level) when it locks; our note's "two spaces" is wrong for this release. A `flood_msgs`
above 10 is forced to 10 with `flood_msgs lowered to max: 10` on the server console. A said text
that starts with a double quote loses its first and its last character whatever the last is.
Ours [ours]: applies in single player too, ends both lines with a full stop, lower-case `you`,
clamps silently, and strips quotes only when both ends are quotes.

**R41 (3, commands) `kill` on a first spawn.** [read] `kill` is refused under 5 s after the
player's respawn time, and on a player's first placing on a map that time is 0 (the map's
start), so a player joining a match already 5 s old may `kill` at once; spectators are refused.
Ours [ours]: 5 s after every placing.

**R42 (3, SP) Unnamed starts made for eight maps.** [read] So that a map loaded from the console
starts in the right place, 0.1 s after the start a copy of a named `info_player_start` is added
as an unnamed one (its origin and yaw): mine1 (from the start named `mintro`), mine2 (`mine1`),
mine3 (`mine2a`), mine4 (`mine3`), power2 (`power1`), waste1 (`power2`), waste2 (`waste1`),
city2 (`city2NL`). It is added only when an entity slot is free. Ours [ours]: none.

**R43 (3, SP) Two more map fixes.** [read] On `fact2` the `target_spawner` at its known spot is
moved 8 units down. On `fact1` the intermission camera is placed at (1037, 1100, 222) whatever
spot was chosen (an area portal there garbled the view). Ours [ours]: neither.

**R44 (3, SP) The world's `music` key.** [read] The world's `music` key, when set, is the CD track
configstring as given (a name); otherwise `sounds` as a number. Ours [ours]: `sounds` only.

**R45 (3, DM) Settings the game library adds that ours lacks.** [read] `gamename` (server info,
latched: `baseq2`), `gamedate` (server info, latched: the build date) which server browsers
show; `gun_x`, `gun_y`, `gun_z` (default 0: the view weapon moved right, forward and down by
that many units, for development); `maxentities` (latched, 1024). Ours [ours]: none of them.

**R46 (3, coop) A spectator setting outside deathmatch is ignored.** [read] In co-op and single
player the `spectator` user setting is ignored at connect and later: the player plays. Ours
[ours]: a client asking to spectate becomes a spectator in any mode.

**R47 (3, DM) The intermission's field of view.** [read] During the intermission every player's
field of view is 90. Ours [ours]: the player's own.

**R48 (3, SP) The first grenades switch to the hand grenade.** [read] Picking up grenades when
none were held switches to the hand grenade (always outside deathmatch, from the blaster only in
deathmatch). Ours [ours]: the switch happens when the count after the pickup equals a box's
amount, which differs when the map gave the box a `count` (derived).

**R49 (3, SP) No login flash in single player.** [read] The login flash at a player placed on a map
is thrown in deathmatch always, otherwise only when `maxclients` is above 1. Ours [ours]: every
non-spectator's arrival throws it.

## Counts

49 gaps. Rank 1: 7 (DM 4, coop 2, SP 1). Rank 2: 20 (DM 10, coop 6, commands 3, SP 1).
Rank 3: 22 (SP 9, DM 7, commands 5, coop 1).

## Checked and the same

[read] against [ours], no difference found:

- Pickup rules: armour (base, maximum, the better-protection conversion, the shard), health
  (small and mega past the maximum, the mega's 5 s wait and 1 a second drain, its 20 s
  respawn), adrenaline, ancient head, bandolier and pack limits, powerups' skill limits, keys
  (co-op: one each, cube marks), weapons' ammo and auto-switch, `coop_pickup_weapons`,
  `coop_elevator_delay`, the first touch firing targets taken or not.
- Respawn times (armour 20 s, weapons and ammo 30 s, powerups by item), the respawn event,
  instant items (`dmflags` 16), the quad dropped at death used at once with its time left.
- Power armour: use and its sounds, `No cells for power armor.`, switched off with its sound when
  cells run out, the deathmatch auto-switch-on of the first one, the status bar's flashing icon.
- `dmflags` 1, 2, 8, 16, 32, 512 (farthest: distance to the nearest living player, first spot
  when none is living), 1024, 2048, 8192 (in deathmatch), 16384, 32768.
- `fraglimit` and `timelimit` tests (any client in use; level time ≥ minutes × 60).
- The status bar programs (single and deathmatch, identical text), the stat slots (health,
  ammo, armour and power armour, pickup line 3 s, timers in the order quad, invulnerability,
  suit, rebreather, selected item, layouts bit 2 only while alive, frags), the help computer's
  layout text and skill names, the scoreboard layout (12 rows, two columns of 6, the tags, ping,
  minutes), its 32-frame refresh, spectators left off it.
- The intermission: spot chosen 0 to 3 steps on with wrap-around, dead respawned first, frozen,
  no gun, the 5 s wait, single player going straight on without `*`, co-op showing the
  scoreboard at a unit's end, keys stripped.
- Teams by model (the part before the slash), `say`/`say_team` lines `Name: text` and
  `(Name): text`, the 150-character cut, unknown words said as chat, the five commands allowed
  in the intermission, `players` and `playerlist` formats and their cut-offs, `wave` (words and
  refusals), `weaplast`, `weapnext`/`weapprev` with `g_quick_weap` (0.9 s), `inven`, `invnext`
  family, `invuse`, `invdrop`, `putaway`, `score`/`help` toggles and the deathmatch `help`.
- trigger_key in co-op (key from everyone, cube from its holders), `You need the <item>`,
  `<n> more to go...`, `Sequence completed!`, the mine3 secret's message, fact1's secret exit,
  the jail5 flyers, the co-op spot by slot order.
- Single player's death: any button after 1 s asks for the load menu.

## Landed

| Date | PR | Rows |
| --- | --- | --- |
| 2026-11-21 | the rank-1 PR | R1 (every obituary word for word, with the victim's gender), R2 (single player's `<name> died.`; co-op scores nothing), R3 (`entered the game` and `disconnected` to everyone, with the login flash), R4 (`Timelimit hit.`, `Fraglimit hit.`), R5 (every teleporter exit and, in deathmatch, every spawn spot a solid disc; the window's prediction clips against boxes), R6 (the 18 maps' co-op spots renamed, security's three added, the fallback: the n-th co-op spot within 550 units of the start), R7 (a player seated again after a level change keeps what he carried and his score; deathmatch scores start afresh each map), R13 (the next map's order) |
