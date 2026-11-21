# Quake II single player: map repairs, item teams, weapon commands (facts)

Read from: Yamagi Quake II, the game library (baseq2), current master as of November 2026.
Marks: [read] read in the source; [derived] follows from facts read, not observed running; [measured] none in this document (nothing was run). "Ignoring case" means an ASCII
case-insensitive comparison.

## Map repairs

### 1. fact2's target_spawner
- [read] The repair applies only when the map's name is `fact2` (ignoring case) and the
  `target_spawner`'s origin, as parsed from the map, is exactly (-1504, 512, 72). The match is an
  exact equality on all three components; no tolerance.
- [read] The move is along +z by minus 8 units, so only z changes: the spawner ends at
  (-1504, 512, 64). x, y and the angles are untouched.
- [read] It is done once, when the spawner is spawned at map load, before any use, so every
  entity the spawner later creates appears at the lowered spot. [derived]
- [read] Every other `target_spawner` on fact2, and spawners on any other map, are unchanged.

### 2. fact1's intermission camera
- [read] Only the origin is replaced, with (1037, 1100, 222). The view angles are NOT changed:
  they stay those of the intermission spot that was chosen (an `info_player_intermission` picked
  at random among the first four in entity order, wrapping; else the first
  `info_player_start` in entity order; else the first `info_player_deathmatch`).
- [read] The replacement happens wherever an intermission view is actually set up on a map named
  `fact1` (ignoring case), with no mode test of its own. But the intermission view is set up only:
  in deathmatch (every level end); and in single player or co-op only when the level change's
  target contains `*` (the end of a unit). A single player or co-op change to a map without `*`
  skips the intermission view and leaves at once, so the repair has no effect there. [read]
- [open] Whether any exit of fact1 is a unit end (`*`) is map data the source does not hold.

### 3. Unnamed starts for eight maps
- [read] The list and source names are as stated: mine1 from `mintro`, mine2 from `mine1`, mine3
  from `mine2a`, mine4 from `mine3`, power2 from `power1`, waste1 from `power2`, waste2 from
  `waste1`, city2 from `city2NL`. Both the map name and the start's `targetname` are compared
  ignoring case.
- [read] The copy is an `info_player_start` with no `targetname`, at the named start's origin; its yaw is the named start's yaw, pitch and roll are 0. It gets no think of its own.
- [read] Every `info_player_start` is given a think 0.1 s (one 10 Hz frame) after it spawns; at
  map load the level clock is 0, so it runs at 0.1 s. Each start checks the pair list; only a
  start whose map and `targetname` match makes a copy. If two starts on the map carry that
  `targetname`, each makes a copy. [read]
- [read] There is no mode test: the copy is made in single player, co-op and deathmatch. (On the
  map `security` in co-op only, the think is replaced by the co-op spots fix; security is not in
  the list, so the eight maps are unaffected.)
- [read] It is made even when an unnamed start already exists; there is no check. [derived] When
  choosing a single player start with no named spawn point, the first unnamed start in entity
  order wins. The copy takes the first free entity slot (slots freed in the first 2 s of server
  time are reusable at once, which includes entities removed at load by skill or mode), else a
  new slot at the end; so it may land before or after a pre-existing unnamed start. If no slot
  at all is free the copy is silently not made.
- [derived] For `map mine1` typed at the console: the server runs two game frames right after
  loading the entities and before any client enters (level times 0.1 s and 0.2 s); the copy is
  made in the first of those, so it exists before the first player is placed. The console `map`
  command gives no spawn point name, so the player takes the first unnamed start.

### 4. command's trigger_once *27
- [read] Condition: map name `command`, classname `trigger_once`, model `*27`, all ignoring case.
- [read] Only the not-on-hard spawn flag is cleared; its value is 1024 (hex 400). Nothing else of
  the entity changes. It is cleared while entities are parsed, before the skill filter, so the
  trigger survives at skill 2 (hard) and skill 3 (hard plus). Its not-on-easy and not-on-medium
  flags, if set, still apply. [read] (After the filter, all five skill and mode flags are cleared
  from every entity anyway, so the trigger's behaviour is otherwise unchanged.)

## Item teams (R17)

### 5.
- Who joins [read]: any entity spawned as an item (any classname in the item list: weapons,
  ammo, armour, health, powerups, keys, power cubes) that has a `team` key. Team grouping itself
  is done for all entities by exact, case-sensitive string equality of `team`, in entity order; the lowest-numbered entity with that string is the team's master. There is no exclusion of keys
  or power cubes.
- Timing [read]: an item drops to the floor 0.2 s after it spawns (two frames). At that moment a
  teamed item is hidden. The master alone then schedules the choice 0.1 s later. At map load
  that is 0.3 s of level time. [derived from 0.2 + 0.1]
- Odds [read]: the members are counted along the team (master first); one is picked by a
  non-negative integer from the engine's random generator taken modulo the count, so each member
  has 1/count (modulo bias negligible). [derived for the bias]
- The shown one [read]: made drawn (sent to clients), made solid as a trigger (touchable), and
  given the item-respawn event (the same event a deathmatch item respawn sends), also on this
  first choice.
- A hidden member [read]: not solid (cannot be touched) and not sent to clients (not drawn). It
  keeps its place and its toss movement.
- Outside deathmatch [derived]: a taken item is removed from the world (unless it is a co-op
  "stays in co-op" item such as a weapon or key, which is simply left in place), and nothing ever
  reschedules a choice, so no hidden member ever shows. In co-op a weapon or key with that flag
  is never consumed by a pickup, so the shown one simply stays.
- Deathmatch [read]: the taken one is hidden for its respawn time (the item's own time: for
  example 30 s for weapons, ammo and ordinary health, 20 s for armour; powerups use their own
  amount). When that ends, the choice is made again over the whole team (the taken one included)
  with the same equal odds, and the chosen member is shown with the item-respawn event. Under the
  weapons-stay dmflag a taken weapon is never hidden, so no new choice happens. The mega health
  hides for 5 s plus the time its bonus decays, then 20 s, then re-chooses. [read]
- Dropped items [read]: items thrown by a player or a death carry no `team` key, so they are never
  in a team. Keys and power cubes [read]: the source does not exclude them; whether any stock map
  teams them is map data the source does not hold. [open]
- Edge cases [derived]: if the team's master is not an item (a mover sharing the same `team`
  string), or the master item is removed for starting inside a solid, no choice is ever
  scheduled and every member stays hidden. A member removed for starting solid stays in the count
  and can be picked, in which case nothing visible appears for that round.

## Weapon commands (R25, R26)

### 6. Lines printed
All of these go to the issuing player only, at the high print level; none is centre-printed.
[read] Each line ends with one line feed. `<x>` marks a filled blank.

- `use` (the whole rest of the command line is the name, matched to an item's pickup name
  ignoring case):
  - `unknown item: <name as typed>`
  - `Item is not usable.`
  - `Out of item: <name as typed>`
  - choosing a weapon through `use`: `No <ammo pickup name> for <weapon pickup name>.` and
     `Not enough <ammo pickup name> for <weapon pickup name>.` (question 7). A weapon already in
     hand: nothing. Power armour with no cells: `No cells for power armor.`
- `drop`:
  - `unknown item: <name as typed>`
  - `Item is not dropable.`
  - `Out of item: <name as typed>`
  - `Can't drop current weapon` (no full stop): the weapon in hand or the one being switched to,
     when only one is held; also dropping grenades when the hand grenade is in hand and the drop
     would leave none. A weapon `drop` under weapons-stay prints nothing and does nothing.
- `give`, `god`, `notarget`, `noclip` in deathmatch or co-op with cheats off:
  `You must run the server with '+set cheats 1' to enable this command.` (single player has no
  such gate).
- `give`: `unknown item` and `non-pickup item` (no full stop on either). Other `give` forms print
  nothing.
- `god`: `godmode ON` / `godmode OFF`.
- `notarget`: `notarget ON` / `notarget OFF`.
- `noclip`: `noclip ON` / `noclip OFF`.
- `invuse`: `No item to use.`; `Item is not usable.`
- `invdrop`: `No item to drop.`; `Item is not dropable.`

### 7. One shot's worth
[read] The least ammo is the weapon's own per-shot amount; the ammo line is skipped entirely when
the `g_select_empty` setting is non-zero (default 0, archived) and for the hand grenade (which is
itself an ammo item). Weapon pickup name, ammo pickup name, least amount:
- Blaster: no ammo, never refused.
- Shotgun: Shells, 1.
- Super Shotgun: Shells, 2.
- Machinegun: Bullets, 1.
- Chaingun: Bullets, 1.
- Grenade Launcher: Grenades, 1.
- Rocket Launcher: Rockets, 1.
- HyperBlaster: Cells, 1.
- Railgun: Slugs, 1.
- BFG10K: Cells, 50.

[read] The first blank is the ammo's pickup name, the second the weapon's pickup name, exactly as
spelled above, for example `Not enough Shells for Super Shotgun.` and `Not enough Cells for
BFG10K.` `No ...` is printed when the ammo count is 0, `Not enough ...` when it is above 0 but
below the amount. So only the Super Shotgun (1 shell) and the BFG10K (1 to 49 cells) can ever
print `Not enough`. [derived]

### 8. cycleweap and prefweap
- [read] The notes' order is confirmed for both commands: cycleweap starts after the position of
  the weapon being switched to (else the one in hand) in the list, wrapping; if that weapon is not
  in the list it starts at the first name. It skips the weapon in hand. It takes the first name
  that is a held weapon with enough ammo; failing that the first held one short of ammo; failing
  that the first not held. prefweap scans from the first name and does not skip the weapon in
  hand (choosing it then does nothing, silently).
- [read] Class names: `weapon_blaster`, `weapon_shotgun`, `weapon_supershotgun`,
  `weapon_machinegun`, `weapon_chaingun`, `ammo_grenades` (the hand grenade),
  `weapon_grenadelauncher`, `weapon_rocketlauncher`, `weapon_hyperblaster`, `weapon_railgun`,
  `weapon_bfg`. Names are matched ignoring case, both against the item list and against the
  current weapon's name when finding the start.
- [read] "Enough ammo" here: 1 for the hand grenade, else the amounts of question 7. These two
  commands test ammo themselves, regardless of `g_select_empty`; the switch itself then goes
  through the same path as `use`, so a held weapon short of ammo prints the question 7 line
  (unless `g_select_empty` is on, in which case it is selected).
- [read] An unknown class name, or a class name that is not a weapon (for example an armour), is
  skipped silently. If no listed name is a weapon, nothing is printed and nothing happens.
- Lines [read]:
  - no names: `Usage: cycleweap classname1 classname2 .. classnameN` and
     `Usage: prefweap classname1 classname2 .. classnameN`
  - the fallback is a weapon not held: `Out of item: <weapon pickup name>` (for example
     `Out of item: Railgun`).
  - the fallback is held but short of ammo: the `No ...` or `Not enough ...` line of question 7.
  All at the high print level, each ending in a line feed.
- [read] cycleweap with three or more names, when the switch was accepted (the chosen weapon is
  now the one being switched to), shows the weapon's icon and pickup name in the pickup slot of
  the status bar for 0.7 s. prefweap never does.
- [read] During the intermission both are ignored, as are `use`, `drop`, `give`, `god`,
  `notarget`, `noclip`, `invuse`, `invdrop` and every other command except `players`, `say`,
  `say_team`, `score` and `help`. Nothing is printed and the word does not become chat.
- [read] When dead there is no test: the commands run and print as usual. [derived] A weapon
  switch requested while dead is discarded, because the weapon handling clears any pending switch
  every frame while health is below 1.
