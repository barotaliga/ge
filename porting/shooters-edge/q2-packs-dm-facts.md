# Quake II mission packs: deathmatch rules (facts)

Facts from a clean-room reader. Project read: the released game source of Ground Zero (the yquake2 `rogue` repository, its first commit
ae4db9289f8949dde595cf83f473bd85dbb2dceb, which imports the released code) and of The Reckoning
(the yquake2 `xatrix` repository, its first commit 728382971588a44f929b1d6e8142bbb48ca1386c, the
released code). Later commits in those repositories were not used. Nothing was run, so
no fact here is measured.

Tags: [read] = stated by the source; [derived] = follows from reading several parts together, not
run. Times are in seconds of game time; the server runs 10 frames a second, so "frames" below are
tenths of a second.

## 1. The tag game (`gamerules` 2)

Where and when the token appears
- A map may place one or more `dm_tag_token` entities. One that spawns when `deathmatch` is 0, or when `gamerules` is set to anything but 2, removes itself at once. [read]
- A placed token is an ordinary item: model `models/items/tagtoken/tris.md2`, the rotate effect plus the tag-trail effect (entity effect bit 29, value 536870912), the glow render flag (512, pulsing light), and it drops to the floor 0.2 s after the map starts like any other item. [read]
- If the map has no `dm_tag_token` at all, one is created when the map finishes loading (deathmatch with `gamerules` nonzero only). Its place is chosen exactly as a player's respawn point would be: with dmflags "spawn farthest" (512) the deathmatch start farthest from any living player, otherwise a random `info_player_deathmatch` (at map load no player is alive, so every start is a candidate); with no deathmatch starts at all, an `info_player_start`. It is put 9 units above that start and then dropped to the floor. [read]
- There is no item respawn for the token: once taken, no token exists in the world until the holder loses it (below). [derived]

Who can take it
- Any living player who touches it. Dead players cannot. Nothing stops the player who last dropped it from taking it back once it is touchable. [read; the last part derived: the touch has no owner test]
- Taking it puts one "Tag Token" in the taker's inventory, makes the taker the holder, resets the holder's kill count to 0, and gives the pickup bonus below. The token in the world is removed. [read]
- The status bar shows the item icon `i_tagtoken` and the pickup name `Tag Token` for 3 s (the ordinary pickup icon and pickup string stats, indices 7 and 8), the screen flashes as for any pickup, and the sound `items/pkup.wav` plays. There is no stat of its own for the token; it appears only as an inventory item. It cannot be used and cannot be dropped with the drop command (it has no use and no drop action). The cheat command `give` never hands it out: `give all` skips it, and `give Tag Token` does nothing for the player and prints `item cannot be given` on the server's console only. [read]

The pickup bonus (also given to a player who takes the token by killing the holder)
- If the player's health is below their maximum health (100 by default), it is raised by 200 and capped at the maximum: in effect a full heal. Health already at or above the maximum is left alone. [read]
- The player then picks up a Body Armor exactly as if they had touched a dropped `item_armor_body` (the normal armor pickup rules decide whether it is taken; if it is not, nothing is given). Its pickup sound `misc/ar1_pkup.wav` plays. [read]
- When the bonus comes with a token pickup, the status bar ends showing the token, not the armor, because the token's pickup is recorded after the armor's. [derived]

Scoring (deathmatch, `gamerules` 2)
- The holder kills another player: +3 instead of +1, and the holder's kill count rises by 1. On the holder's 5th such kill the count returns to 0 and the holder is given a Quad Damage and uses it at once: 30 s of quad (added to any quad time already running), with the sound `items/damage.wav`. This repeats every 5 kills while the same player holds the token. [read]
- A teamkill by the holder (teams on, friendly fire) scores -1 as normal and does not count toward the quad. [derived]
- Another player kills the holder: +5 instead of +1 for the killer. This branch does not look at friendly fire, so a teammate who kills the holder also gets +5. [read; the teammate case derived]
- Holder killed by another player, normal case: the killer becomes the holder immediately (kill count 0) and gets the pickup bonus. No token is thrown into the world and no "Tag Token" is added to the new holder's inventory, so a holder who won it by a kill has no token item in the inventory. [read; the inventory consequence derived]
- Holder killed by another player when the kill was made by that player's hunter sphere, by a doppleganger's explosion, vengeance or hunter, or when the killer was already dead at the moment of the kill: the killer still gets +5, but the token is dropped from the dead holder (see "When the holder loses it") and nobody holds it. [read]
- Suicides (self-inflicted kills with an obituary) score -1 as normal; deaths to the world ("died.") score -1 as normal. Either way the holder loses the token. [read]
- Kills that involve no holder score as normal (+1, -1 for a teamkill). [read]
- There is no per-time bonus and no bonus for holding at map end. [read: nothing of the kind exists]

Damage
- While `gamerules` is 2 in deathmatch, any damage where neither the target nor the attacker is the holder is multiplied by 3/4 and rounded toward zero (so 1 point becomes 0). Damage to or from the holder is unchanged. This covers every source, including the world (falls, lava, drowning) and a non-holder's own splash damage. [read]
- If the reduced damage is 0 the hit is ignored entirely, knockback included. [read]
- Knockback is otherwise not changed. There is no speed change for the holder. [read]

What the holder looks like
- Every server frame while the holder is alive and not in intermission, the tag-trail effect bit (536870912) is added to the holder's entity. [read]
- On the deathmatch scoreboard every player's row may carry a picture: normally `tag1` on your own row and `tag2` on your killer's row; the holder's row shows `tag3` instead, even if it is your own or your killer's row. [read]

When the holder loses it
- Death (any cause in which they do not pass it to a killer, as above) or disconnecting throws a new token from the holder: it starts 24 units in front of the holder's view and 16 units down (pulled back by a trace against solid world if that point is inside a wall), moves forward at 100 units/s and up at 300 units/s, falls under gravity, uses a 30-unit cube (-15 to +15), shows the rotate and tag-trail effects and the glow, and nobody holds the token. The thrown token carries no classname (a mod sees an unnamed entity). [read]
- It cannot be taken for the first 1 s. At that moment, if its point is in lava or slime, it is moved 3 s later; otherwise 30 s later. "Moved" means it is placed at the deathmatch start farthest from every living player (by the distance to the nearest living player; with no living players the first deathmatch start in entity order) and falls from there; with no deathmatch start at all it tries again every 1 s. It is moved once only: it is not moved again if it lands somewhere bad. [read; "once only" derived]
- A dropped token can be taken by anyone, as above. [read]

Map change and other edges
- The holder is remembered by the game library across a map change: the rules' state is cleared only when the game library starts, not at each map. A player who held the token when the map ended is still treated as the holder on the next map (trail, +3 kills, full damage, `tag3`) without having the item, and when that player dies a token is thrown, so two tokens can be in the world at once. [derived; not run]
- Each token that is taken simply makes its taker the holder; there is one holder at a time. [derived]

Lines a player reads
- The tag rules print no message of their own: no centerprint and no print. The only words are the pickup name on the status bar, `Tag Token` (and `Body Armor` when the bonus armor is the last pickup recorded). Obituaries are the normal ones. [read]
- Sounds by name: `items/pkup.wav` (token pickup), `misc/ar1_pkup.wav` (bonus armor), `items/damage.wav` (the 5th-kill quad). [read]

## 2. `gamerules`

- Default "0"; flags: latched only (not serverinfo, not archived). [read]
- It is read once when the game library starts (the engine's start of a game, which is when latched values take effect); the rules chosen then hold until the library starts again. Separately, several places test its current value at each map start or at each event (the token's spawn, the token's pickup, the hooks). [read; which engine commands restart the library is the engine's business and is not settled by this source]
- Values: 0 is normal deathmatch; 2 is tag. A deathball game is numbered 3 but is disabled in this release, so 3 behaves like any other unknown value. Any other nonzero value (1, 3, negatives, and so on) is reset to 0 inside the game when it is read (the game changes the value it uses, not the setting's text, so the console still shows what was typed). The value is taken as a whole number for the choice, so 2.5 picks tag, but the token's own checks then see a value other than 2 and remove every token. [read; the console-text and the 2.5 consequence derived]
- Outside deathmatch: the tag rules are still set up if the value is 2, but no token can exist (it removes itself outside deathmatch), no token is created at map load, and the damage change needs deathmatch. One leak: in cooperative play the kill-scoring hook still runs, so a player killing a teammate (always friendly fire in coop) loses 1 point where coop normally changes no score. [read; the coop consequence derived]

## 3. The four dmflags

- 131072 (no mines): in deathmatch, `ammo_prox` and `ammo_tesla` placed on the map are removed at spawn. `weapon_proxlauncher` is NOT removed, and picking it up still gives prox ammo with it, so prox mines remain possible. Nothing is taken from a player's inventory, the starting inventory is not changed, and drops are not affected. [read; the launcher consequence derived]
- 262144 (quad and double damage do not stack): nothing happens at pickup; both timers run as usual (a second pickup of either adds 30 s to its own timer). The flag acts only when a player fires: in deathmatch, if quad is running, the damage multiplier is 4 and double damage is ignored; when quad has run out, any remaining double damage gives 2. Without the flag, quad and double together give 8 in deathmatch. (Outside deathmatch they never stack, flag or not.) The multiplier also sets the mines' damage and their lifetime (see question 5). [read]
- 524288 (no nukes): in deathmatch, `ammo_nuke` (the A-M Bomb) placed on the map is removed at spawn. Nothing else changes. [read]
- 1048576 (no spheres): in deathmatch, `item_sphere_vengeance`, `item_sphere_hunter` and `item_sphere_defender` placed on the map are removed at spawn. Spheres made by a dying doppleganger are not affected. [read]
- None of the four flags takes items out of a player's inventory, changes the start, or changes what a dead player drops. [read]
- Interaction with `randomrespawn`: the flags do not stop a respawning item from being replaced by a forbidden one; a forbidden replacement is removed by the spawn rules as soon as it appears, leaving the spot empty from then on (see question 4). [derived]

## 4. `randomrespawn`

- Default "0"; flags: none (not latched, not serverinfo). Its value is read at each item respawn, so it can be changed mid-map. If it is nonzero when a deathmatch map loads, every giveable item's model and sounds are also loaded then. [read]
- When: only when an item that was picked up comes back after its respawn delay. Items as first placed by the map are never replaced. Items that never respawn are never replaced (dropped items, weapons under "weapons stay"). Respawns only happen in deathmatch. [read; the last two derived]
- If the respawning item belongs to an item team, the team member that comes back is chosen first as normal, then that member may be replaced. The replacement does not keep the team; what that does to the rest of the team is not settled by reading. [read; the team outcome open]
- Replacement rules, by what is respawning:
  - Power screen, power shield: never replaced. [read]
  - Health family (anything picked up as health, plus adrenaline): the small health (`item_health_small`) is never replaced; anything else becomes `item_health` with probability 0.60, `item_health_large` 0.30, `item_adrenaline` 0.09, `item_health_mega` 0.01 (a fresh roll each time, so it may come back as itself). [read]
  - Armor: armor shards are never replaced; any other armor becomes `item_armor_jacket` 0.60, `item_armor_combat` 0.30, `item_armor_body` 0.10. [read]
  - Everything else is replaced by an item of the same class, chosen uniformly among every giveable item of that class in the game's item list, itself included. The classes (derived by listing the item list's entries and their kind; items flagged both weapon and ammo count as ammo; items not giveable left out):
  - Class weapons (14): `weapon_blaster`, `weapon_shotgun`, `weapon_supershotgun`, `weapon_machinegun`, `weapon_chaingun`, `weapon_etf_rifle`, `weapon_grenadelauncher`, `weapon_proxlauncher`, `weapon_rocketlauncher`, `weapon_hyperblaster`, `weapon_plasmabeam`, `weapon_railgun`, `weapon_bfg`, `weapon_chainfist`. A floor blaster cannot be picked up, so a weapon spot that turns into a blaster stays a blaster for the rest of the map. [derived]
  - Class ammo (9): `ammo_grenades`, `ammo_shells`, `ammo_bullets`, `ammo_cells`, `ammo_rockets`, `ammo_slugs`, `ammo_flechettes`, `ammo_prox`, `ammo_tesla`. [derived]
  - Class powerups (13): `ammo_nuke`, `item_quad`, `item_invulnerability`, `item_silencer`, `item_breather`, `item_enviro`, `item_ir_goggles`, `item_double`, `item_compass`, `item_sphere_vengeance`, `item_sphere_hunter`, `item_sphere_defender`, `item_doppleganger`. [derived]
  - Class keys: any key becomes a random key of the 11 keys. [derived]
  - `item_bandolier`, `item_pack` and `item_ancient_head` have no class, so they always come back as themselves. [derived]
  - The disruptor and its ammo are not giveable in this release and are never chosen. [read]
- The dmflags exclusions inside the choice test the item that is respawning, not the candidates: with "no mines" a respawning `ammo_prox` or `ammo_tesla`, with "no nukes" a respawning `ammo_nuke`, and with "no spheres" a respawning vengeance or hunter sphere, come back as themselves (although such items are normally not on the map at all under those flags). Candidates are never filtered, so forbidden items can be chosen and are then removed on spawn. The sphere test misspells the defender sphere's classname, so it never matches the defender. [read; consequences derived]
- About 1 time in 32768 the uniform pick lands on no item and the item comes back as itself. [derived from the random source's range]
- The replacement appears at once where the old item stood, with the usual respawn effect, is visible to infrared goggles, and settles to the floor 0.2 s later as any newly spawned item does. [read]
- Respawn time: the delay before it appears is the old item's. The replacement's own delay (by its kind) applies after it is next picked up. [derived]

## 5. `strong_mines`

- Default "0"; flags: none. Read when each mine finishes opening, in any game mode. [read]
- It affects prox mines only (not tesla mines). A prox mine's damage is 90 times the firer's damage multiplier (1, 2, 4 or 8 with quad and double damage), radius 192. [read]
- Normally a landed mine lives, after it finishes opening, 45 s at multiplier 1, 30 s at 2, 15 s at 4, 10 s at 8 (45 s for any other value), then explodes. With `strong_mines` nonzero it lives 45 s whatever the multiplier, and keeps its multiplied damage. [read]
- A mine still in flight that never lands keeps the multiplier-based fuse (45, 30, 15, 10 s) either way. [read]

## 6. `huntercam`

- Default "1"; flags: serverinfo and latched. [read]

## 7. Other deathmatch rules an operator would notice

The Reckoning
- No new settings: its setting list is the base game's. [read]
- One new dmflags bit, 65536 (drop DualFire): when a player dies in deathmatch with more than 1 s (10 frames) of DualFire Damage left, an `item_quadfire` is thrown from the body. It is picked up like any item, starts at once on pickup with the time the dead player had left, and disappears when that time would have run out. When the dead player also drops a weapon, the weapon is thrown 12.5 degrees to one side and the DualFire to the other; if a quad is dropped too (dmflags 16384), the weapon goes 22.5 degrees one way and the quad and the DualFire both 22.5 degrees the other way, together. [read; the shared direction derived]
- `item_quadfire` ("DualFire Damage") respawns 60 s after pickup in deathmatch; on use it runs 30 s (added to any time left) with the sound `items/quadfire1.wav`, a fading warning `items/quadfire2.wav` 3 s before the end, and the quad shell effect on the player. Its HUD timer icon is `p_quadfire`. Under dmflags "no items" (2) it is removed from maps, like the other powerups. [read; the last point derived]
- Traps (`ammo_trap`, item name "Trap", at most 5 carried): every trap thrown uses one even with dmflags "infinite ammo" (8192). [read] A test that seems meant to keep "infinite ammo" from giving 1000 traps with a weapon compares against a name no weapon has, so it never excludes anything. [derived]
- Respawn times in deathmatch, as read: weapons 30 s, ammo 30 s, armor 20 s, health 30 s, mega health 20 s after its holder's extra health wears off, powerups by item (quad 60 s). [read]

Ground Zero
- New settings beyond the base game's: `gamerules`, `huntercam`, `strong_mines`, `randomrespawn` (questions 2 to 6), `sv_stopspeed` (default "100", no flags: the speed floor in the friction the game library applies to the entities it moves itself, such as monsters and spinning objects; every mode), and `g_showlogic` (default "0", no flags; a debugging switch that prints monsters' reasoning to the server console; no deathmatch effect). [read] Players' own movement is done by the engine, not by this friction. [derived]
- New dmflags bits: the four of question 3. [read]
- The disruptor (`weapon_disintegrator`) and its ammo (`ammo_disruptor`) are removed from every map in every mode in this release. [read]
- Deathmatch-only items: outside deathmatch `item_doppleganger`, `ammo_nuke`, `item_sphere_vengeance` and `item_sphere_hunter` are removed at spawn (the defender sphere stays), and a doppleganger cannot be picked up outside deathmatch. [read]
- dmflags "no items" (2) also removes the three spheres and the doppleganger (but not the A-M Bomb). dmflags "infinite ammo" (8192) removes ammo items that are only ammo, so `ammo_flechettes` and `ammo_prox` go but `ammo_tesla` and `ammo_grenades` stay. [read; the item lists derived]
- With teams and dmflags "no friendly fire" (256), the A-M Bomb still hurts teammates. [read]
- Respawn delays of the new items in deathmatch: IR goggles 60 s, double damage 60 s, compass 60 s, vengeance sphere 60 s, hunter sphere 120 s, defender sphere 60 s, doppleganger 90 s, A-M Bomb 300 s. Weapons 30 s, ammo 30 s, armor 20 s, health 30 s, mega health 20 s as in The Reckoning. [read]
- Carry limits in deathmatch: one A-M Bomb, one doppleganger; a sphere cannot be picked up while you own an active sphere (silently; the message for it is disabled). [read]
- Double damage (`item_double`): 30 s per use, added to any time left, sound `misc/ddamage1.wav`. [read]
- Comparison with the base game's own respawn times was not made (the base game's source was not read).
