# Dissolution of Eternity: its program's deathmatch, team modes, runes, hook, boxes and co-op (read)

Release read: the GPL game program source of Quake mission pack 2, "Dissolution of Eternity" (Rogue
Entertainment, 1997; the team, rune, hook and capture the flag features carry a Jan/Feb 1997 credit to a
contractor working for id Software for Rogue). The copy read carries no later localisation: every message is
plain English text in the program. The 2022 id Software GPL re-release of the same pack was opened only to
confirm it is the same program with English messages replaced by localisation keys; nothing below comes from
the re-release unless said so.

Marks: [read] the source states it directly. [derived] follows from what the source does, worked out by
the reader. [engine] a fact about the standard Quake engine, not settled by this program. [open] the source
does not settle it.

Notation for messages: text in double quotes is what the player reads. [nl] is a line break. Text inside
«guillemets» is printed in Quake's alternate (coloured, high-bit) character set: each character is the
ordinary letter with 128 added, which the standard console and centre-print draw in the brown/red font
[read for the bytes, engine for how it looks]. A name that is a player's own name is written <name>.

## 1. Message of the day

- When it is armed: on every client connect, in every mode (single player, coop, deathmatch), with no
  check of deathmatch or coop [read]. Since the engine calls the connect hook again on every level load,
  it is armed again on each new level [engine].
- First showing: on the first player frame strictly later than 3 seconds after the connect [read].
- Repeats: it is re-shown once per second (each showing arms the next for 1 second later, checked strictly
  later, so on frame granularity) [read]. It is shown 5 times in all (at about 3, 4, 5, 6 and 7 seconds),
  except with teamplay 3 where it is shown 3 times (about 3, 4 and 5 seconds) [read]. Then it stops for the
  rest of that level [read].
- How long it stays: it is a centre print, refreshed each second; the last one fades after the engine's
  centre-print time (scr_centertime, default 2 seconds), so the message is on screen from about 3 s to
  about 9 s after connect (3 s to about 7 s with teamplay 3) [derived; the fade time is engine].
- Centre print or console: centre print only; nothing goes to the console [read].
- What prints depends only on the teamplay value [read]:
  - teamplay 0, 1, 2 (and any value other than 3 to 6): nothing is printed. The program holds texts for
  teamplay 1 ("Team play: No Friendly Fire") and 2 ("Team play: Friendly Fire") but they are disabled,
  so a black-box check must see NO message in those modes [read].
  - teamplay 3: "Welcome to[nl]The Dissolution of Eternity![nl]By Rogue Entertainment[nl][nl]Deathmatch Tag![nl]" [read]
  - teamplay 4: "Welcome to[nl]The Dissolution of Eternity![nl]By Rogue Entertainment[nl][nl]CAPTURE THE FLAG![nl][nl]You are «RED» team![nl]"
  or the same with «BLUE» for a blue player [read].
  - teamplay 5: as teamplay 4 but with an extra line "(One Flag Mode)" right after "CAPTURE THE FLAG!",
  then the blank line and "You are «RED» team!" or «BLUE» [read].
  - teamplay 6: as teamplay 4 but with an extra line "(Three Team Mode)" right after "CAPTURE THE FLAG!",
  then the blank line and "You are «RED» team!", «BLUE» or «GREY» [read].
  - The team named is the player's team at the moment of each showing (by 3 s he has normally been
  assigned) [derived]. Any player whose team is not red counts as blue in modes 4 and 5 [read].
- Because nothing checks deathmatch, a single player or coop game started with teamplay 3 to 6 shows the
  same text [derived].
- Side effect with teamplay 4, 5 or 6: each showing also sends the console command "teamplay N" (N the
  server's value) to that client, so the client's own teamplay setting is set to the server's [read]. It
  prints nothing to the player [derived].

## 2. Runes

- Settings: runes exist only when deathmatch is non-zero AND bit 1 of the cvar gamecfg is set (gamecfg
  odd) [read]. Default gamecfg is 0, so runes are off by default [read: the program calls the default
  "disabled"]. No teamplay value, serverflags or other cvar matters [read]. Whether the engine registers a
  gamecfg cvar at all is engine business; if it does not, the program reads 0 and runes never appear
  [derived].
- Models and sounds are precached at world spawn only when both conditions hold then [read]; turning
  gamecfg on mid-level does not give runes until the next level [derived].
- Kinds: four [read].
  | Kind (as the pickup message names it) | Model | Effect | Sound when it acts |
  | --- | --- | --- | --- |
  | Earth Magic, RESISTANCE | progs/end1.mdl | health damage taken is halved (after armour has taken its share) | runes/end1.wav, at most once per second |
  | Black Magic, STRENGTH | progs/end2.mdl | all damage the holder deals is doubled (stacks with Quad, so 8x) | runes/end2.wav on firing, at most once per second |
  | Hell Magic, HASTE | progs/end3.mdl | refire delay times two thirds for the weapons with a fixed delay: axe 0.5 s, shotgun 0.5 s, super shotgun 0.7 s, grenade and multi-grenade 0.6 s, rocket and multi-rocket 0.8 s, plasma gun 1.0 s. Nailguns, lava nailguns, thunderbolt and grapple are not sped up | runes/end3.wav, at most once per second |
  | Elder Magic (spelled "Edler" in the message), Regeneration | progs/end4.mdl | +5 health once per second while health is below 100, capped at 100 | runes/end4.wav each time it heals |
  [read for all; the Black and Earth effects apply only in deathmatch, read]. Extra [read]: a Megahealth
  picked up by an Elder holder does not rot down and does not set the "super health" state; it respawns as
  plain health does (20 s in deathmatch).
- Where they spawn: each rune is placed at an info_player_deathmatch spot. The spots are taken in entity
  order, one after another, continuing where the last rune left off and wrapping to the first spot [read].
  So at map start the four runes go to the 1st, 2nd, 3rd and 4th info_player_deathmatch in the map (with
  fewer spots they wrap and share) [derived].
- When: the first info_player_deathmatch entity spawned at map load schedules them; they appear 0.1 s
  later, once per level [read]. A map with no info_player_deathmatch gets no runes [derived].
- How they arrive: each is tossed from its spot with 300 units/s upward and a random horizontal speed of
  -300 to +300 units/s on each of x and y, then falls under gravity [read]. Pickup box 32 x 32 x 56 units
  (from 16 left/back to 16 right/forward, 0 to 56 up) [read].
- An untaken rune moves itself every 120 s: it jumps to the next info_player_deathmatch spot in the
  rotation with a new random toss [read].
- Pickup: only a living player. The player gets a centre print and the sound weapons/pkup.wav [read].
  Messages [read]:
  - "Earth Magic[nl][nl]RESISTANCE"
  - "Black Magic[nl][nl]STRENGTH"
  - "Hell Magic[nl][nl]HASTE"
  - "Edler Magic[nl][nl]Regeneration"
- One rune per player: touching a second rune while holding one gives the centre print "You already have a
  rune[nl]", at most once every 5 s while he keeps touching, and the rune stays [read].
- Status bar: nothing. The program sets no item bit for a held rune, so no icon shows [derived].
- On death (deathmatch or coop) the held rune is dropped at the dead player's origin with the same toss as
  above, and it then follows the 120 s relocation rule [read]. Runes are kept across nothing: a new level
  re-spawns them [derived].

## 3. Grappling hook

- When it is given: only in deathmatch (non-zero) with teamplay 4 or more (the capture the flag modes)
  [read]. It is part of the starting inventory in those modes, beside axe and shotgun [read]. It needs no
  flag entities in the map and no other cvar [derived]. It is never given in single player, coop, or
  teamplay 0 to 3 [read].
- Selecting it [read]:
  - impulse 22 selects the hook directly (only in deathmatch with teamplay 4 or more; otherwise nothing).
  - impulse 1 (the axe key) toggles: pressed while already holding the axe it selects the hook; otherwise
  it selects the axe.
  - weapon cycling forward (impulse 10) goes axe, then hook, then shotgun, and backward (impulse 12) goes
  shotgun, then hook, then axe, in those modes only [read].
  - Picking up a better weapon while the hook is out and attack is held does not switch away from it
  [read].
  - On a level change outside the capture modes a selected hook falls back to the axe [read].
- Firing: hold attack (button 0) with the hook selected [read]. It uses no ammo [read].
  - The hook leaves 16 units forward and 16 up from the player's origin at 800 units/s along the view, and
  disappears after 2 s if it hits nothing (so its reach is about 1600 units) [read].
  - Fire sound weapons/chain1.wav, view kick 2 degrees up [read].
  - Hitting sky drops it [read]. Hitting a teammate (same team) drops it [read]. Hitting another player:
  10 damage, sound player/axhit1.wav, then while held it follows that player, doing 1 damage every 0.1 s
  with blood and the sound pendulum/hit.wav; it lets go when that player dies or teleports [read].
  Hitting anything else: sound player/axhit2.wav and 1 damage if the thing can be damaged [read]. Any
  anchor also plays weapons/tink1.wav on the firer [read].
  - If attack was already released when it lands, it drops at once [read].
  - While anchored the player is pulled toward a point 16 units below the hook (unless jump is held) and
  16 units behind it along the view, at 1000 units/s, or at 10 times the remaining distance per second
  once within 100 units [read]. He is lifted off the ground on anchoring [read].
  - Release: letting go of attack (while still holding the hook weapon), dying, or teleporting [read]. After
  any release the next attack is allowed 0.25 s later [read]; the hook refires no faster than every 0.1 s
  [read].
  - Kill message: "<victim> was grappled by <attacker>" [read].
- Models [read]:
  - the hook in the world: progs/hook.mdl (frame 1 in flight, frame 2 when anchored),
  - the first-person weapon: progs/v_grpple.mdl,
  - the chain: no model is set by the program; it sends the engine's beam temporary entity from
  the hook to a point 16 units above the player's origin, every frame while the hook is more than 50
  units away. The standard engine draws that beam with progs/beam.mdl [engine]. The program precaches
  only progs/hook.mdl and progs/v_grpple.mdl for the hook (a chain of progs/bit.mdl links exists in the
  source only as disabled code) [read].
- Player animation while hooking reuses the axe attack frames and a frame of a death animation while
  being pulled [read]; the view-model frame is 1 when firing and 2 while out [read].

## 4. Team modes

Teams are by colour: a player's team is his pants colour plus one, as the engine sets it [read for the
program's use; engine for where it comes from]. The program understands these teamplay values [read]:

| teamplay | Mode | What it does |
| --- | --- | --- |
| 0 | none | plain deathmatch |
| 1 | teams, no friendly fire | a player cannot lose health to anyone of his own colour, himself included. Armour is still worn down by such hits [derived]. Health damage from other colours and from the world is normal |
| 2 | teams, friendly fire | full damage. Killing a teammate costs the killer 1 frag and prints one of (25% each) "<killer> mows down a teammate", "<killer> checks his glasses", "<killer> gets a frag for the other team", "<killer> loses another friend" [read] |
| 3 | deathmatch tag | no team protection; the tag token scoring below |
| 4 | capture the flag, two teams (red, blue), colours locked |
| 5 | capture the flag, one neutral flag (two teams) |
| 6 | capture the flag, three teams (red, blue, grey) |

Values above 6 are treated like the capture modes for colour locking, the hook and starting kit, but no flags
appear and every colour counts as legal [derived]. teamplay 1 to 6 also enable impulse 20 and 21 below.

Common to teamplay 1 and up [read]:
- impulse 20 tosses a backpack with ammo of the current weapon (shells 20, nails 20, lava nails 20, rockets
  10, multi rockets 10, cells 20, plasma 10, or all he has if less) plus the same amounts of ammo for
  weapons he does not own. With nothing to give: "No ammo available!". The pack lasts 120 s.
- impulse 21 (deathmatch 1 only) tosses the current weapon, not the axe, shotgun or hook; mission-pack
  variants are tossed as their base weapon and the player loses both. The thrower cannot re-take it for
  1 s; it lasts 120 s; picking it up says "You got the " plus its name.
- Team score report every 120 s, only in modes 4, 5, 6 (also once on the first player frame of a level):
  "«RED» team is leading by N points!", or "«RED» and «BLUE» teams are tied with N points!"; in mode 6 also
  «GREY» and "«RED», «BLUE» and «GREY» teams are tied with N points!". Points are the sum of the team's
  frags [read].

Deathmatch tag (teamplay 3) [read]:
- Needs a dmatch_tag_token entity in the map (removed in any other mode; model progs/sphere.mdl skin 1, a
  dim light, 32 unit cube). Without one, scoring is plain +1.
- Touching it: "<name> got the tag token!", sound runes/end1.wav; it floats 48 units above the holder.
  Every 30 s: "<name> has the tag token!".
- Holder kills someone: +3. On his 5th kill with it he gets Quad Damage for 30 s ("You got the Quad
  Damage", items/damage.wav). On the 10th, "<name> lost the tag token" and the token goes to a spawn spot.
- Killing the holder: +5 and the killer takes the token at once. Other kills: +1.
- Holder dies otherwise, or uses the kill command, or leaves: "<name> lost the tag token!"; it drops and
  goes back to a spawn spot 30 s later if nobody takes it.

Capture the flag (teamplay 4, 5, 6), all only in deathmatch [read unless marked]:
- Maps: the program names no map; it works on any map that places the entities below [read]. Which map(s)
  of the pack were meant for capture the flag is [open] in this source.
- Entities: item_flag_team1 (red flag, progs/ctfmodel.mdl skin 0, shows as the gold key icon on the status
  bar when carried), item_flag_team2 (blue flag, skin 1, the silver key icon), item_flag (the neutral flag
  of mode 5, skin 2, both key icons). Each team flag also creates a base marker progs/ctfbase.mdl with
  classname item_flagbase_team1 or item_flagbase_team2 (item_flagbase for the neutral flag).
  info_player_team1 and info_player_team2 are team spawn spots. func_ctf_wall is a solid wall only in
  modes 4 to 6 and removed otherwise. Sounds: misc/flagtk.wav (taken), misc/flagcap.wav (captured, heard
  everywhere), misc/flagret.wav (returned). Flags glow with a dim light.
- Mode 5 turns the two team flags into bases only (the team flag itself is removed); item_flag is removed
  outside mode 5; team flags are removed outside modes 4 to 6.
- Colours: red is colour 4, blue colour 13, grey colour 0 (third team only) [read]. A joining player is
  put on the team with fewest players (a tie is a coin toss; in mode 6 grey counts double, so it gets
  about half as many) and told "You have been assigned to «RED» team." (or «BLUE», «GREY»); his colour is
  set for him by a "color N" console command [read]. With gamecfg bit 8 a player who joins wearing a legal
  team colour keeps it [read]. The team is kept across levels [read]. The player model uses skin 1 in these
  modes, 0 otherwise [read].
- Changing colour: without gamecfg bit 16 the player is killed (obituary "<name> tried to change teams"),
  told "You cannot change teams.", and his colour is set back. Each attempt counts as a suicide; past 3 he
  is told "You were told you can't change teams.[nl]Go play color games somewhere else." and disconnected,
  and all see "<name> has bad color sense". With gamecfg bit 16 the change is allowed: he is killed
  ("<name> changed teams"), his frags are set to 0, and he joins the new team (or is assigned one if the
  colour is not a team colour).
- Friendly fire: same-team hits cost neither health nor armour, unless gamecfg bit 4 (health) or bit 2
  (armour) is set. Hurting oneself is not protected [derived].
- Starting kit every life: axe, shotgun, hook, 25 shells, 50 green armour; nothing is carried to the next
  level [read].
- Spawning: in modes 4 and 6 red and blue players spawn at their own team spots, the first free one (no
  player within 32 units) in rotation; with no team spot, or right after dying from a team change, at
  info_player_deathmatch [read]. Grey spawns at info_player_deathmatch [derived].
- Two-flag play (modes 4 and 6 for red and blue):
  - Taking the enemy flag: "<name> «got» the «RED» flag!" (or «BLUE»); the taker sees "«YOU GOT THE ENEMY
  FLAG»[nl][nl]«RETURN TO BASE»" (a grey taker: "«YOU GOT THE ENEMY FLAG»[nl][nl]«RETURN IT TO THE OTHER
  BASE»"); the flag's team sees "Your flag has been taken!"; the taker's team sees "Your team has the
  «RED» flag!"; a third team sees "«BLUE» team has the «RED» flag!" (or the matching colours). A player
  already carrying a flag cannot take another.
  - Capture: touch your own flag while it is at its base, carrying the enemy flag. All see "<name>
  «captured» the «BLUE» flag!" (the colour is the flag captured). Capturing team: centre "Your team
  captured the flag!"; the other team "Your flag was captured!"; grey in mode 6 "«BLUE» flag was
  captured!". Sound misc/flagcap.wav, no distance falloff. Both flags go home.
  - Returning your own dropped flag by touching it: "<name> «returned» the «RED» flag!", +1 frag, sound
  misc/flagret.wav; centre to all "Your flag has been returned to base!" or "Enemy flag has been returned
  to base!" (mode 6: "«RED» flag has been returned to base!").
  - A dead or leaving carrier drops the flag where he was: "<name> «lost» the «RED» flag!". It returns
  by itself after 40 s with the same centre prints and items/itembk2.wav. The kill command also drops it.
- One-flag play (mode 5): touching the neutral flag takes it: "<name> «got» the flag!", the taker sees
  "«YOU GOT THE FLAG»[nl][nl]«TAKE IT TO ENEMY BASE»", others "The flag has been taken!". Touching the
  enemy team's base marker with it captures: "<name> «captured» the flag!" (no centre prints). Dropped:
  "<name> «lost» the flag!"; after 40 s "The flag has been returned!". A dropped neutral flag is simply
  taken by whoever touches it.
- Three-team play (mode 6): grey captures by carrying a red or blue flag to the OTHER colour's base marker:
  "<name> «captured» the «BLUE» flag!" and so on; grey's capturer gets 8 and each grey teammate 4; the
  losing team sees "Your flag was captured!"; capturing team "Your team captured the flag!".
- Scores are frags only: capture +15 to the capturer and +10 to each teammate; return +1; taking a flag 0,
  fragging the enemy carrier +2 ("Enemy flag carrier killed: 2 bonus frags"), or nothing extra if he had
  held it under 2 s ("Enemy flag carrier killed, no bonus"); a kill with you or the victim within 400 units
  of your own carrier +1 ("<name> defends «RED»'s flag carrier"); a kill with you or the victim within 400
  units of your flag at its base +1 ("<name> defends the «RED» flag", mode 5 "<name> defends the flag"),
  on a capture, a teammate who returned the flag in the last 4 s +1 ("<name> gets an assist for returning
  the «RED» flag!") and one who fragged the enemy carrier in the last 6 s +2 ("<name> gets an assist for
  fragging the flag carrier!"). The «colour» in those lines is the scorer's team. A bonus for killing
  someone who had just hurt your carrier ("... against an agressive enemy") is in the program but can
  never trigger, since nothing records who hurt a carrier [derived].
- Scoreboard: the engine's frag board; there is no separate team score [derived]. impulse 23 gives a
  status line, for example "Your flag is in your base." and "The enemy flag is lying about.", "<name> has
  your flag.", "You have the enemy flag.", in mode 5 "The flag is at base!", "The flag is lying about!",
  "You have the flag!", "<name> of the «RED» team has the flag!"; outside the capture modes "Capture the
  Flag is not enabled." [read].

## 5. The six b_ ammo boxes

- Which items [read]: item_lava_spikes loads maps/b_lnail0.bsp (small, 25 lava nails) or maps/b_lnail1.bsp
  (spawnflag 1 "big", 50); item_multi_rockets loads maps/b_mrock0.bsp (5) or maps/b_mrock1.bsp (10),
  item_plasma loads maps/b_plas0.bsp (6) or maps/b_plas1.bsp (12).
- Yes, they are brush models: each item sets its model to that .bsp file, exactly as the base-game shells,
  nails, rockets and cells boxes do with their own b_ files [read]. The model is precached in the item's
  own spawn, so only when the item is placed in the map [read].
- What they need beyond a base-game box [read]: the six .bsp files in the pack's data; the player's three
  extra ammo counters; caps of 200 lava nails, 100 multi rockets, 100 plasma (a full player cannot take
  one). Pickup text "You got the lava nails", "You got the multi rockets", "You got the plasma", sound
  weapons/lock4.wav, a bonus flash. Owning the matching base weapon also prints "Lava Enabled", "Super Lava
  Enabled", "Multi Grenades Enabled", "Multi Rockets Enabled" or (with the thunderbolt) "Plasma Gun Enabled"
  and gives that weapon [read]. The status bar shows these counts in
  the normal ammo slots while a mission-pack weapon is selected [read]; showing them anywhere else is the
  engine's status bar, [open] here. Same box size as a base box (32 x 32 x 56) and respawn after 30 s only
  in deathmatch 1 [read].

## 6. Co-op (coop 1)

Differences from single player [read unless marked]:
- Spawn: players take info_player_coop spots in turn, falling back to info_player_start when the list runs
  out; no check for a free spot. A teleport flash and telefrag happen on each spawn. Same rule as base
  Quake [derived].
- Respawn after death: a copy of the corpse stays, the player gets back his inventory as it was at level
  start, and he spawns again (single player restarts the level instead).
- Death drops a backpack (and any flag or rune, though those exist only in deathmatch).
- Weapons stay: a weapon already owned cannot be picked up again, and weapons do not disappear. With that
  rule a pickup also gives mission-pack ammo: nailgun or super nailgun +20 lava nails, grenade or rocket
  launcher +3 multi rockets, thunderbolt +5 plasma (the same in deathmatch 2).
- Keys stay in place for the next player.
- Messages: "<name> exited the level" when someone touches an exit; "<name> entered the game" on join.
- Multi rockets do not home in coop (they fly straight, spread 3.3 and 6.6 degrees either side, and burst
  after 4 s; model progs/rockup_d.mdl instead of progs/rockup.mdl); plasma bolts lose their bright light.
- Cheats impulse 9 (all weapons) and 255 (quad) are refused.
- Monsters look for other coop players when they lose sight of their enemy.
- The ending: coop always gets a text finale, "You have destroyed Quake's[nl]Temporal Teleporter. His
  assault[nl]on Time has been defeated.", instead of the camera cut-scene (single player also falls back
  to it when the map lacks the camera).
- Same as single player: inventory carries between levels, except the pack's first level r2m1 resets it
  (single player and coop, not deathmatch), and the start map resets it when serverflags is set.
- In every mode, including coop, the kill command works at most 4 times per level; then "You have
  suicided too much already." and it is refused.
- No message of the day in coop unless teamplay is 3 to 6 (see 1) [derived].
