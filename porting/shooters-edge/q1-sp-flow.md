# Quake single player, reader B: level flow, saves, skill, co-op and the client's half

Read: id Software's Quake engine, the WinQuake tree of the GPL source release (single-player
engine, version 1.09 as it reports itself). No GPL copy of the 1.06 game program's QC source was
at hand, so everything the PROGRAM decides (the intermission's minimum wait, which buttons leave
it, the episode texts, coop respawn rules, which tracks it asks for) is stated only where the
engine's side settles it, and is otherwise matched by construction (we run the pack's own
progs.dat). Our side was read in /home/user/ge (files named below).

Marks: [read] in the source or our code, [derived] reasoned from what was read, [measured] none.

## Ranked differences

| # | Rank | Area | Difference (a player's words) | Coop? |
| --- | --- | --- | --- | --- |
| 1 | 1 | Episodes | A new game on the full game opens only the next episode in order and shows runes from earlier games; the original opens all four halls and starts every new game with no runes | Both |
| 2 | 1 | Screen tints | Quad, pentagram, ring and suit never tint the screen | Both |
| 3 | 1 | Pickup flash | Picking up anything gives no gold flash (the program's `bf` is dropped) | Both |
| 4 | 1 | Network coop | A player who joins a coop game over the network gets no intermission or finale and zeros for monsters and secrets | Only coop |
| 5 | 2 | Damage flash | The hurt flash is a red plus an amber layer of different strength; the original is one tint whose colour depends on how much armour took | Both |
| 6 | 2 | Liquid tints | Under water the screen goes teal at about a third; the original goes brownish at about a half; slime is much darker in the original | Both |
| 7 | 2 | Intermission screen | Ours darkens the camera view behind the tally; the original draws the pictures over the undimmed view | Both |
| 8 | 2 | Finale screen | Ours darkens the view and has no title picture; the original draws gfx/finale.lmp at the top over the undimmed view | Both |
| 9 | 2 | Intermission camera | The mouse turns the intermission camera, it has no idle sway, and nothing hides the held weapon there | Both |
| 10 | 2 | Death view | Dead, the solo panel does not replace the status bar, the weapon sinks instead of vanishing, and the eye height ignores the program's | Both |
| 11 | 2 | Centre print | Messages ("You found a secret area!", locked doors) appear near the top instead of about a third down; long lines are not cut at 40 | Both |
| 12 | 2 | Pause | No `pause` command, PAUSE key or pause picture in a campaign; the menu and console freeze coop games too, the joiners' as well | Both |
| 13 | 2 | Console | `kill`, `god`, `notarget`, `noclip`, `fly`, `give`, `restart`, `changelevel` and `skill` do nothing or are unknown in a campaign; `map e1m1` typed first starts a deathmatch | Both |
| 14 | 2 | Music | The tracks the program asks for at the intermission and the finales never play; only the map's own track does | Both |
| 15 | 2 | Attract loop | At start the menu is already up over the demos and they keep cycling behind it; the original plays demo1 full screen, a key brings the menu, and the loop waits while it is up | Neither |
| 16 | 3 | Level time | The solo panel's and the intermission's time read about 1.2 s less | Both |
| 17 | 3 | Save slots | The slot lines read differently (no padded kills column, "- EMPTY -"), the save prints other words, and the menu's SAVE writes our format, not s<n>.sav | No |
| 18 | 3 | New game | NEW GAME asks no "Are you sure" while a game runs | No |
| 19 | 3 | Coop size and saves | Eight players at most (the original: four, sixteen with a larger listen); a coop game with one player cannot be saved | Only coop |
| 20 | 3 | Pitch drift | A keyboard player's view does not drift to the slope unless `lookspring` is on, and drifts at a constant rate | Both |
| 21 | 3 | Loading and console | No loading plaque, no level-name banner in the console; `demos` lists files instead of returning to the loop; no `startdemos`, `stopdemo` | Both |
| 22 | 3 | Slow motion | `host_framerate` does nothing | Both |

Count: rank 1: 4, rank 2: 11, rank 3: 7.

## 1. The episodes on the start map (rank 1)

Original [read]: `map` (and so the menu's NEW GAME, which runs `maxplayers 1` and `map start`)
sets the carried server flags to 0 before the map starts; only `changelevel` carries the
program's `serverflags` on. Nothing in the engine gates episodes: on the full game every
episode hall stands open from the first visit, any episode can be played first, and the runes
on the HUD are only those taken in this game. The start map's own bars that need `registered`
are the only gate [read: the engine sets nothing else].

Ours [read]: `server/campaign_episodes.hpp` and `campaign.cpp` (`keep_episodes`,
`shut_episode_bars`): `sv_episodes_done`, archived by the window (`client/coop_mode.cpp`), is
added to the start map's `serverflags` on every start, including a new game, and the start map's
registered-only triggers in front of episodes not yet open are rewritten into message triggers.
So a fresh install opens episode 1 only; a done episode's gate shuts its hall; the HUD shows
runes from earlier games; and the program, seeing runes in `serverflags` on the start map,
takes away the inventory as it would after a real episode.

Test: on the full game's packs, `newgame` with `sv_episodes_done 0`, walk to each hall: the
original opens all four bars; ours opens one. With `sv_episodes_done 1`, read the HUD's runes on
the start map: the original shows none.

## 2. Powerup tints (rank 1)

Original [read]: each frame the client tints by the player's item bits, one powerup at a time in
this priority: quad pure blue (0, 0, 255) at 30/255; suit pure green at 20/255; ring grey
(100, 100, 100) at 100/255; pentagram yellow (255, 255, 0) at 30/255. The tint is on while the
bit is set; no fade (a flicker at the end is the program's doing).

Ours [read]: `client/world_fx.cpp` sets the quad and pentagram layers from the player entity's
effect bits 64 and 128, and never sets the suit or ring layers (no code writes them). The
single-player engine of this release defines only effect bits 1, 2, 4 and 8 [read], so its
program cannot be relying on 64 or 128 [derived]: with the pack's program none of the four
tints ever shows. Our layers also stack (the original shows one) and use other colours and
strengths (`app/effects.cpp`'s `screen_blend`).

Test: `se-campaign` on e1m1 giving the player each powerup by the program's own cheat (impulse
255 for the quad), capture the post blend: zero today.

## 3. The bonus flash (rank 1)

Original [read]: the console command `bf` sets a gold tint (215, 186, 69) at 50/255 that falls
by 100/255 a second (gone in half a second). The program sends `bf` to the player with
`stuffcmd` on every pickup; the client runs any stuffed text as console input.

Ours [read]: `client/listen.cpp`'s `take_events` keeps only a stuffed `menu_loadgame`; every
other stuffed line is dropped, and no `bf` command exists in the window. So no pickup ever
flashes, and anything else the program stuffs is lost too.

Test: touch a shells box on e1m1 in `se-campaign --pickups`, count stuffed lines run: 0 today.

## 4. Coop over the network: the program's messages (rank 1, coop only)

Original [read]: every client, local or remote, reads the same single-player messages: the
intermission message with no payload, the finale string, the cdtrack message with two bytes,
and at entry the four statistics (total secrets, total monsters, secrets, monsters as 11 to 14)
with their real values.

Ours [read]: `net/qw_frame.cpp` relays the program's all-clients and one-client byte streams to
a joiner only when they parse as the QuakeWorld protocol's messages (`well_formed`); a frame
whose stream does not parse is dropped whole. In that protocol the intermission (30) carries a
position and angles and the cdtrack (32) one byte (`net/qw_svc.cpp`), so the program's payload-
less intermission and two-byte cdtrack fail to parse, and that frame's other messages (a kill,
a secret) go with them [derived]. `net/qw_signon.cpp` sends statistics 11 to 14 as 0 at every
spawn [read]. A joiner therefore sees no intermission screen or finale text, and his solo panel
shows zero totals. The window's own player and its companions are local and unaffected.

Test: `tests/shooters_edge_qw_coop_test.cpp` with a scripted joiner touching e1m1's exit: count
the joiner's intermission and finale messages and his statistic 12.

## 5. The damage flash (rank 2)

Original [read]: on each damage message, a count of half the armour plus half the blood, at
least 10. The tint's strength gains 3 × count, capped at 150 (of 255), and falls by 150 a
second. Its colour: (200, 100, 100) when the armour took more than the blood, (220, 50, 50) when
the armour took some, else pure red. One layer. The face shows its pain frame for 0.2 s.

Ours [read]: `app/effects.cpp`: two layers, the blood's red (0.75, 0.05, 0.02) at blood × 3 / 300
capped at 0.45, and the armour's amber (0.62, 0.42, 0.12) at armour × 3 / 300 capped at 0.3,
each falling by 150 a second, with no minimum of 10. A small hit (under 20 points) flashes far
less than the original; any armoured hit has an amber cast the original never shows.

Test: the `client-data` chart of `se-campaign --fire=1` with the blend's colour and alpha per
blow beside the original's rule.

## 6. The liquid tints (rank 2)

Original [read]: by the contents of the leaf the eye is in: water (and any other liquid not
slime or lava) (130, 80, 50) at 128/255; slime (0, 25, 5) at 150/255; lava (255, 80, 0) at
150/255; none in air. Software view: the water warp (`r_waterwarp`, default 1) under any
liquid.

Ours [read]: `app/effects.cpp`: water (0.16, 0.34, 0.38) at 0.35, slime (0.22, 0.40, 0.06) at
0.55, lava (0.92, 0.36, 0.04) at 0.65, chosen when the program's `waterlevel` is 3 (eyes under).
Water is a different hue (teal against brown) and weaker; slime far brighter. The `v_` shares
and `gl_cshiftpercent` scale them.

## 7. The intermission screen (rank 2)

Original [read]: while the intermission message holds (and only when no menu or console is up)
the screen shows the camera view, no status bar (full-screen view), no centre print, no
console notify lines, and over it gfx/complete.lmp at (64, 24), gfx/inter.lmp at (0, 56) and the
big figures; drawn at the screen's own pixels from the top left, not darkened.

Ours [read]: `menu/level_screens.cpp`'s `draw_intermission` darkens the whole frame to 60 %
black first and centres and scales the 320 × 200 layout. The positions inside the layout match.

## 8. The finale (rank 2)

Original [read]: the finale message sets the second kind of intermission and centre-prints its
string; the screen shows the camera view with gfx/finale.lmp centred at y 16 and the text typed
at `scr_printspeed` (8 a second) from its arrival, held as long as the intermission lasts.
Text placement: up to four lines at 35 % of the screen height, more lines from y 48; each line
centred, and a line longer than 40 characters is cut at 40. The cutscene message is the same
without the title picture. Not darkened.

Ours [read]: `draw_level_text` darkens to 60 %, draws no title picture, places the block's middle
at 40 % of the 200-line layout and draws whole lines. The typing speed and the hold match.

## 9. The intermission camera (rank 2)

Original [read]: during any intermission the view is the player entity's origin and angles as
the server sends them (the program moves the player to its intermission spot), with the idle
sway forced on at scale 1 (pitch and yaw 0.3°, roll 0.1°, at 1, 2 and 0.5 radians a second), no
bob, no roll, and the held weapon not drawn. The player's mouse changes nothing on screen.

Ours [read]: `client/present.cpp` builds the intermission view as any other frame from
`listen.view`, which the mouse keeps turning after the program's setangle (`client/main.cpp`'s
`turn_view` runs every frame); idle sway is 0. `client/held.cpp`'s `step_held` hides the
weapon only when the player is dead (the demo path hides it at the intermission, the listen
path does not), so the weapon stays drawn unless the program empties the weapon model [derived:
what the program does to the weapon model is not settled by the engine source].

Test: `gpu-se-client` frames of e1m1's exit with mouse motion during the intermission.

## 10. The death view (rank 2)

Original [read]: with health at or below 0 the status bar is replaced by the score bar and the
solo panel (as if the scores key were held); the roll is fixed at 80°; the weapon is not drawn
at once; the eye stands at the program's view offset (the program lowers it on death and raises
it for a gibbed head).

Ours [read]: `client/scores.cpp`'s `scores_screen` shows the solo panel in a campaign only while
the scores key is held. The roll is 80° (`kDeadRollDeg`, a match). The weapon sinks out of view
over time (`view/hand.hpp`). The eye is fixed 16 units under the origin while dead
(`client/view.cpp`, `kDeadEyeUnits`), whatever the program sets, so a gibbed player's raised eye
is lost too.

## 11. The centre print (rank 2)

Original [read]: shown `scr_centertime` (2 s) of real time, hidden (but still counting) under the
menu or console, cleared at every level start and loading plaque; placed at 35 % of the screen
height when it has up to four lines, else from y 48; each line centred; a line past 40
characters is cut there, the rest of that line dropped.

Ours [read]: the time matches (`classic/app/look.hpp`'s 2 s). `hud/layout.cpp` places the centre
banner at 12 % from the top in every look and draws whole lines.

## 12. Pause (rank 2)

Original [read]: `pause` toggles the pause when `pausable` is 1 (else "Pause not allowed."),
prints "<name> paused the game" or "unpaused the game" to everyone, and the client draws
gfx/pause.lmp while paused (`showpause` 1). The PAUSE key runs it under the pack's default
config. Separately, a game with one player stops while the menu, console or chat line is up; a
game with more than one never stops for them.

Ours [read]: in a campaign the window has no `pause` command (the server's own `pause` exists
only on the match's command path, `server/commands.cpp`, which the window's console does not
reach for a campaign), so the key does nothing and there is no picture. `client/main.cpp` stops
the campaign whenever the console or menu is open, in coop too, including a networked coop whose
joiners then freeze with it. Whether our chat line pauses a single-player game (the original's
does) was not checked.

## 13. Console commands (rank 2)

Original [read]: from the console, refused only in deathmatch: `god` ("godmode ON/OFF"),
`notarget` ("notarget ON/OFF"), `noclip` ("noclip ON/OFF"), `fly` ("flymode ON/OFF"), `give`
(2 to 8 a weapon by its number, s n r c ammunition, h health, with a count), `kill` (the
program's suicide; "Can't suicide -- allready dead!" when dead); `restart`, `changelevel <map>`
("Only the server may changelevel" otherwise), `map <map>` (a new game), `skill` (a setting
read at the next map), `maxplayers` (1 sets `deathmatch 0`, more sets 1; refused while a
server runs). `nomonsters` exists and does nothing.

Ours [read]: the window's console in a Quake campaign has `map`, `newgame`, `coopgame`, `save`,
`load`, `sv` and `cmd`. The rest are unknown (the unknown-line hook reaches only rules that
answer their own words, and the program's rules do not). `cmd kill` works; `cmd god` and the
other cheats go to the match path, which wants `sv_cheats` and then toggles only noclip and fly
(god and give do nothing, notarget does not exist). `restart`, `changelevel` and `skill` work
only through `sv`. `map <name>` typed before any campaign starts a deathmatch match, because the
window chooses the campaign only when a `deathmatch 0` line was set (`wants_campaign`).

## 14. Music (rank 2)

Original [read]: at every map start the server sends the world's `sounds` as the track; the
client plays it, always looping. The program's own cdtrack messages (it sends one at the
intermission and at episode ends) change the track the same way.

Ours [read]: `client/take_in.cpp`'s `level_music` plays the map's `sounds` track, looped (a
match). `client/campaign_view.cpp` records the program's cdtrack and nothing plays it, so the
intermission and finale tracks never start and the level's track runs on.

## 15. The attract loop (rank 2)

Original: the pack's startup config (not part of the source release) runs `startdemos demo1
demo2 demo3` [derived]; the engine's part [read]: with no map asked for, demo1 plays full screen with no menu. A key that would type in the console opens the
main menu; while it is open the loop does not advance (the demo playing finishes and nothing
follows). ESCAPE from the main menu restores the loop and starts the next demo if none plays.
The loop stops for good on `map`, `connect`, `load`, `playdemo` by hand, or an error. `demos`
returns to the loop, `stopdemo` stops the demo playing.

Ours [read]: `client/main.cpp` with `menu_attract` (`auto`: the profile's list, not the pack's
config) opens the menu at once over the first recording, and the recordings keep cycling behind
the menu; ESCAPE closes the menu to watch.

## 16. The level time (rank 3)

Original [read]: the solo panel's time and the intermission's completed time are the server's
clock as the client last heard it, which starts at 1.0 s on every map and runs two 0.1 s frames
before anyone enters; after a load it continues the saved clock. Shown as whole minutes and
seconds (truncated).

Ours [read]: `client/scores.cpp` subtracts `entered_at`, the server's time when the player
entered (1.2 s or later on a fresh map; 1.2 s after loading a classic save), so both read about
1.2 s less [derived].

## 17. Save slots and their words (rank 3)

Original [read]: the comment line is 39 characters: the level's title (the world's message),
then from column 22 the text "kills:" with the kills and the total each in a field of three
separated by a slash, every space then turned into an underscore. The menu reads s0.sav to
s11.sav from the game folder, turns underscores back into spaces, and shows "--- UNUSED SLOT
---" for a missing one. `save` prints "Saving game to <path>..." and "done."; `load` prints
"Loading game from <path>...". The menu's SAVE runs `save s<n>`, a classic text file.

Ours [read]: `server/q1_save.cpp`'s comment is the title (or the map's name) then " kills:" with
unpadded numbers, any length; the menu's empty slot reads "- EMPTY -"; `save` prints "saved
<name>"; the menu's SAVE writes our own save (`saves/s<n>.save`), the classic file only when
asked for by a name ending in .sav.

## 18. NEW GAME (rank 3)

Original [read]: with a game running, NEW GAME asks "Are you sure you want to start a new
game?" first. It runs `maxplayers 1` (so `deathmatch 0`) and `map start`, leaving `coop` as it
was. Ours [read]: `menu/main_menu.cpp` runs `newgame` at once, which also sets `coop 0`.

## 19. Coop size and coop saves (rank 3, coop only)

Original [read]: `maxplayers` is capped at 4 unless the engine was started for more (16 at most),
and saving is refused only when more than one player slot exists ("Can't save multiplayer games."),
so `coop 1` with one player saves. Ours [read]: eight players (`kCoopPlayers`), and
`campaign_save_problem` refuses any game with `coop` on.

## 20. Pitch drift (rank 3)

Original [read]: on the ground, unless the player looks with the mouse under `+mlook`, the pitch
drifts toward the program's ideal pitch on its own, starting at rest and speeding up by
`v_centerspeed` (500) degrees a second each second, whatever `lookspring` says. Looking up or
down by keys, or holding `+mlook`, stops it; it starts again when `+mlook` is released with
`lookspring` 1, on `centerview`, or after running forward at full speed for `v_centermove`
(0.15 s). Not in noclip, not in the air, not in a demo.

Ours [read]: `plugins/shooter/controls.cpp`'s `spring_view` drifts only with `lookspring` on,
while moving, looking by keys (`freelook` off, `+mlook` not held) and not pitching the mouse, at a
constant 90° a second; letting `+mlook` go with `lookspring` on and `freelook` off levels the view;
there are no `v_centermove` or `v_centerspeed`. A mouse-look player sees no difference; a keyboard
player does.

## 21. Loading and the console's lines (rank 3)

Original [read]: a level change stops all sounds and shows gfx/loading.lmp, the screen frozen
until the client is in; each map start prints a "VERSION 1.09 SERVER (<crc> CRC)" line and a
rule with the level's title in the console (cleared from the notify lines when loading ends).
Ours [read]: a spinner over the old world while the new one is made; no such lines. `demos`
lists the game's recordings (`client/game_adapter.cpp`); `startdemos` and `stopdemo` do not
exist.

## 22. host_framerate (rank 3)

Original [read]: the host frame is the real time since the last, at most 72 a second, clamped to
0.001 to 0.1 s, replaced by `host_framerate` when that is above 0 (slow or fast motion). Ours
[read]: the campaign steps a fixed 1/72 s per frame (`kCampaignStep`); no `host_framerate`. At
72 frames a second the two agree.

## Question by question

1. Game start: answered. Attract loop row 15; NEW GAME rows 1 and 18; the skill hall is the
   program's (it runs `skill <n>` through the server console, which our server's `skill` setting
   takes and carries, a match); `skill` is not archived in the original and resets to 1 each
   launch; ours is a server setting with the same default [read].
2. Level change: answered. Parms and flags match; the intermission and finale screens rows 7, 8,
   9; music row 14; time row 16; loading row 21. The minimum wait and the leaving button are
   the program's.
3. Episode ends: answered for the engine's part. The sell screen opens the help pages in both
   [read]; the end texts are the program's, drawn as in row 8; nothing in the original engine
   treats the end map specially; row 1 for the runes and halls.
4. Death: answered. Row 10; respawn is the program's (`restart` through the server console in
   single player, which ours handles, a match); coop respawn rules are the program's.
5. Saves: answered. Rows 17 and 19; the refusals and the file match otherwise (below).
6. Skill: answered. Match (below).
7. The client: answered for the listed items; rows 2, 3, 5, 6, 9, 10, 11, 16, 20. The status
   bar was checked only in part (below).
8. Music and sound: answered. Row 14; ambient sounds match; this release has no builtin that
   stops a sound, so there is nothing to compare for `stopsound`.
9. Pause: answered. Row 12.
10. Coop: answered in part. Rows 4, 12, 19; the coop status bar's frags strip was not checked.
11. Anything else: answered. Rows 13, 21, 22.

## What matches (checked, not to redo)

- `skill`: rounded to the nearest whole number (half up), clamped 0 to 3 and written back at
  every map start; read by the program at once mid-map through the setting [read both].
- Map start: `coop` forces `deathmatch` 0; the clock starts at 1.0 s and two 0.1 s physics frames
  run before anyone enters [read both].
- `changelevel`: SetChangeParms for every player, their parms and the program's `serverflags`
  kept; `map`: `serverflags` 0 and SetNewParms on entry (but see row 1); `restart`: the same map,
  the parms and flags it was entered with [read both].
- The four statistics sent at entry (listen and local seats), killedmonster and foundsecret each
  adding one [read both].
- The solo panel's layout (Monsters and Secrets with fields of three at x 8, Time at x 184, the
  title centred on x 232) and the intermission's figure positions [read both]; ours cuts the
  title at 22 characters, the original does not (cosmetic).
- `scr_centertime` 2 and `scr_printspeed` 8, the typed text counted in server time, held through
  the intermission [read both].
- The sell screen opens the help pages [read both].
- Damage kick: count at least 10, `v_kickroll` and `v_kickpitch` 0.6, `v_kicktime` 0.5; dead
  roll 80°; `cl_bob` 0.02, `cl_bobcycle` 0.6, `cl_bobup` 0.5, `cl_rollangle` 2, `cl_rollspeed`
  200, `v_idlescale` 0 outside the intermission [read both].
- Status bar thresholds: health and armour figures turn to the alternate colour at 25 or less,
  ammunition at 10 or less; 666 and the disc while invulnerable [read both].
- Saves: version 5; 16 parms, the skill as a whole number, the map, the time, 64 light styles
  ("m" for an unset one), the globals block, one block an entity; loading restarts the map, then
  rebuilds every entity, restores the time and the light styles; refused in the intermission
  and with a dead player; 12 menu slots s0 to s11; no quicksave keys and no autosave in either,
  and `load` from the console stops a demo playing and loads [read both].
- Friendly fire in coop is the program's own rule in both: our `sv_friendlyfire` and
  `sv_friendlypush` are read only by our own campaign rules, never with the pack's program
  [read].
- A `changelevel` takes every player at once, nobody waits; a player joining mid-level gets
  SetNewParms [read both].
- `impulse` reaches the program (its own cheats, 9 and 255) [read].
- `nomonsters` has no effect in the original [read]; ours has no such setting.
- The frame: one physics frame per host frame, at most 72 a second, in single player and coop
  alike [read both].

## Follow-up 1

### F1. How the tints combine

- There are four layers, always taken in the same order: the contents first, then the damage,
  then the bonus, then the powerup [read]. Each layer has a colour (0 to 255 per channel) and a
  strength in 255ths. The client only rebuilds the picture when some layer's colour or strength
  has changed since the last frame [read].
- **Software client** [read]: it works on the palette, one entry at a time, with the layers in
  the order above. Each layer moves every channel toward its own colour by its strength / 256 of
  the gap still left, in integer steps. The result then goes through the gamma table. Nothing is
  clamped, and nothing needs it: a strength of at most 150 moves a channel only part way to a
  colour inside 0 to 255 [derived]. No setting scales the layers here. The contents layer for
  plain air is the one setting a player can change: the `v_cshift r g b percent` command sets
  its colour and strength, and the default strength is 0 [read].
- **GL client** [read]: it builds one colour and one alpha, starting from black at alpha 0. For
  each layer with a strength above 0, its share is strength × `gl_cshiftpercent` / 100 / 255.
  The new alpha is the old alpha plus share × (1 − old alpha). The colour moves toward the
  layer's colour by share / new alpha. At the end the alpha is clamped to 0 to 1.
  `gl_cshiftpercent` (default 100) scales every layer alike. The colour and alpha are drawn as
  one blended quad over the view; `gl_polyblend` 0 turns that quad off (default 1). In GL the
  blend is rebuilt every rendered frame, right after the eye's leaf is found [read].
- The software and GL rules therefore differ: the software one chains linear mixes on the
  palette, the GL one composites over (over-blending) [derived].
- Ours [read]: `app/effects.cpp`'s `screen_blend` composites like the GL rule, but with its own
  order (medium, suit, ring, pentagram, quad, armour, then blood) and its own shares. The
  player-facing scales are `gl_cshiftpercent`, `gl_polyblend` and the `v_` shares
  (`app/look.hpp`).

### F2. The damage tint

- **When it is set** [read]: on every damage message. A count is worked out as half the armour
  plus half the blood, at least 10. The strength **adds** 3 × count to whatever strength is left
  from earlier blows, then is clamped to 0 to 150. The colour is **replaced** by the newest
  blow's: (200, 100, 100) when its armour is more than its blood, (220, 50, 50) when it has any
  armour, else (255, 0, 0).
- It then falls by 150 a second of real frame time, whether or not the game is paused [read].
- **Same count as the kick** [read]: yes. That count also drives the view kick: roll is count ×
  `v_kickroll` × the sideways share of the blow's direction, pitch is count × `v_kickpitch` ×
  the forward share, and both last `v_kicktime`. The face's pain frame (0.2 s) is set by the same
  message.
- **On death** [read]: nothing special. The killing blow's message adds to the tint like any
  other, and the tint fades as usual while dead.
- **At a level start** [read]: all of the client's state, every layer included, is cleared to
  zero when the server-info message arrives. That happens on every map start, `restart`,
  `changelevel` and load. A flash still showing is cut off there.

### F3. The bonus flash

- [read] Every `bf` sets the colour to (215, 186, 69) and the strength back to exactly 50. A
  second `bf` while one is fading resets it to 50; it does not add.
- It falls by 100 a second, so it lasts half a second [derived].
- It is cleared at a level start with the rest of the client's state (see F2) [read].

### F4. The contents tint

- **What chooses it** [read]: the contents of the map leaf that the view origin is in, checked
  every rendered frame. The view origin is the eye after the bob, the stair smoothing and the
  view offsets. The program's `waterlevel` plays no part.
  - Software: the leaf is checked after the world is drawn.
  - GL: the leaf is checked as the frame is set up.
- **Which tint** [read]: empty and solid give the air setting (strength 0 unless `v_cshift`
  changed it). Lava and slime give their own tints. Every other leaf type gives the water tint.
- **While dead, at the intermission and in noclip** [derived from the above]:
  - Dead: the tint still applies, from wherever the lowered eye is.
  - Intermission: it applies from the intermission spot (that view is rendered the same way).
  - Noclip: it follows the eye through any leaf. With the eye inside a solid there is no tint, with the eye in a liquid there is the liquid's tint.
- **When it is skipped** [read]: the software client tests the water warp separately (any leaf
  of water or deeper liquid). Nothing updates the tint while the console is forced full screen,
  because the view is not rendered then.
- Ours [read]: chosen by the program's `waterlevel` being 3 and its `watertype`
  (`client/world_fx.cpp`). That differs in noclip, at the intermission, and where the eye and the
  body are in different leaves (the surface line).

### F5. The powerup tint

- [read] It is worked out again every frame from the item bits in the player's last client data,
  in this order: quad, suit, ring, pentagram. The first bit that is set gives the tint; with none
  set the strength is 0. Nothing else is special:
  - no fade,
  - no timer of the client's own,
  - when the program clears a bit for a moment, the tint is off for that moment.
- The ring's tint is grey at 100 out of 255, whenever the bit is set. Its only other effects are
  separate: the view weapon is not drawn while the ring's bit is set (see the main section).
- **The four bits** [read], all four as given in the question:
  - `IT_QUAD` 4194304
  - `IT_SUIT` 2097152
  - `IT_INVISIBILITY` 524288
  - `IT_INVULNERABILITY` 1048576
- Ours reads none of these bits (row 2).

### F6. The commands the 1.06 program sends

- **What was searched**: the string table of the progs.dat in the shareware pack
  (/mnt/project-files/paks/quake1/pak0.pak; layout crc 5927) [read]. These strings end in a
  newline and look like commands:
  - "bf\n"
  - "restart\n"
  - a lone "\n"
  - the word "skill"

  There is no "menu_loadgame", "v_cshift", "centerview", "disconnect" or "help" among them.
  `changelevel` is a builtin, not a command line.
- **Which builtin sends each** [derived]: the strings alone do not say which builtin carries
  which line. What can be said:
  - "bf\n" is a client command, so it must go to the player's client through `stuffcmd`.
  - "restart\n" works only in the server's own console, so it must be `localcmd`. That fits the single-player respawn.
  - The skill triggers build a server-console line of their own: "skill ", then the trigger's text, then "\n".
- **What the engine does with each** [read]:
  - `bf` on the client: the bonus flash of F3.
  - `restart` on the server: the same map again, with the parms it was entered with.
  - `skill <n>` on the server: sets the setting, which takes effect at the next map start (and at once wherever the program reads the setting).
- **Ours** [read]:
  - `bf` is dropped (row 3). It is the only stuffed command this program sends [derived from its strings].
  - `restart` and `skill` reach our server's console through `localcmd` (`server/commands.cpp`), so those match.
  - `menu_loadgame` is the re-release's program, not this one.

## The final boss's death and the game's ending (clean-room answers, 2026-11-26)

Ours follows this since the PR that rebuilt the ending (`games/shooters_edge/rules/campaign_bosses.cpp`), keeping our own closing words and falling back to the start when a map has no intermission spot or no teleport ball instead of stopping. Marks as below: [read], [derived], [engine], [open].

Time origin: t = 0 is the server frame in which the telefrag blow lands. A monster's animation
frame lasts 0.1 s [read].

### Background facts the answers rely on

- The boss (classname `monster_oldone`) spawns with 40000 health, takes damage, and its pain
  reaction puts its health back to 40000, so only one blow of more than 40000 kills it [read].
  In deathmatch it is removed at spawn, so the whole sequence below never happens there [read].
- The telefrag blow is 50000 points, dealt by the telefrag box (classname `teledeath`) that a
  teleporter places at its destination for 0.2 s; the box is both the inflictor and the
  attacker [read].
- The boss's spawn does NOT give it the monster flag (it does not go through the common monster
  start) [read]. Consequence: the generic death handling does not count it as a kill and sends
  no kill message for it; the finale counts it itself (Q1) [derived].
- On death the generic handling, in this order, before the boss's own death routine: records the
  attacker as its enemy, prints no obituary (obituaries are printed only for players) [read],
  makes it unhurtable, clears its touch, fires the boss's own `target` if the map gave it one
  (with the attacker as activator) [read], then runs the boss's death routine, which is the
  finale [read]. The boss's think (its idle loop) is NOT changed by death [read].

### 1. What happens, in order, with times

All at t = 0, in the same server frame as the blow, in this order [read]:

1. The killed-monsters count goes up by one and a kill message (svc 27, killed monster) is
   written to all clients reliably [read].
2. The intermission exit time is set to now plus 10,000,000 seconds (about 115.7 days), so no
   exit is ever allowed [read; days derived], and the intermission flag is set to 1 [read].
3. The first entity with classname `info_intermission` is found. If there is none, the server
   stops with the error text "no info_intermission" [read]. There is NO random choice among
   several intermission spots here: always the first one found [read].
4. The first entity with classname `misc_teleporttrain` is found and removed at once. If there
   is none, the server stops with the error text "no teleporttrain" [read]. (On `end` this is
   the moving teleport ball [derived from the classname; the map's contents not read].)
5. A finale message (svc 31) with an EMPTY string is written to all clients reliably [read].
   No intermission message (svc 30) is sent at any point of this sequence [read].
6. Every entity with classname `player` (all clients, in co-op too) is moved to the spot (see
   Q5 for everything changed on each) [read]. The view is placed at the spot's origin with no
   eye height (the eye offset is set to zero), facing the spot's `mangle` angles (pitch, yaw,
   roll), turned there at once by the fix-angle flag [read].
7. A timer entity is spawned to act at t = 1.0 s [read].

Later steps are in Q2-Q4. The engine's handling of an empty svc 31 (what the client draws) is
not in this source [engine].

### 2. What the boss does, and when

- t = 0 to t = 3 s: the boss keeps playing its idle loop (46 frames, `old1` to `old46`, 0.1 s
  each, looping) because its think is untouched by death [read; continuity derived]. It plays no
  sound in the idle loop [read]. It stays solid as before (a slide box) but cannot be hurt and
  has no touch [read].
- t = 1.0 s: a teleport splash temporary entity (svc 23, effect 11, the teleport splash) is
  broadcast UNRELIABLY at the boss's origin minus 100 units on y (z and x unchanged) [read].
  The sound `misc/r_tele1.wav` plays on the boss, voice channel (2), volume 1, attenuation 1
  (normal) [read]. The timer is rescheduled for t = 3.0 s [read].
- t = 3.0 s: the boss's next think is switched to the first thrash frame (its next-think time is
  NOT changed, so the thrash starts at the boss's next scheduled tick, within 0.1 s after t = 3)
  [read; the delay derived]. The sound `boss2/death.wav` plays on the boss, voice channel,
  volume 1, attenuation normal [read]. Light style 0 (the default world light) is set to the
  pulsing pattern `abcdefghijklmlkjihgfedcb` [read], which the first thrash frame then overrides
  with a single letter within at most 0.1 s [derived]. The timer entity is not removed by the
  program and is left with no further think [read: nothing removes it].
- Thrash (call its first tick T, with T in (3.0, 3.1] s): frames `shake1` to `shake15`, one per
  0.1 s, each setting light style 0 to one letter: m, k, k, i, g, e, c, a, c, e, g, i, k, m, m
  [read]. After `shake15` a pass counter on the boss (zero at spawn unless the map sets it
  [derived]) goes up by one; while it is not yet 3 the boss loops back to `shake1` [read]. So the
  15-frame pass plays 3 times (45 frames, 4.5 s) [derived], then `shake16` to `shake19` set the
  light to g, c, b, a [read], then `shake20` runs the final step (Q3, Q4) [read]. `shake20` is
  reached at T + 4.9 s, i.e. between t = 7.9 and 8.0 s [derived: 49 ticks of 0.1 s after T].
  Total: 50 thrash frames [derived].
- Frame numbers: the idle frames are model frames 0-45 and `shake1` is 46 [derived from the frame
  list's order]. The frame list declares the name `shake12` twice, so the numbers from `shake12`
  on depend on how the compiler treats a repeated name; the source does not settle whether
  `shake20` is model frame 65 or 66 [open; measure on the shipped program].
- Screen shaking: none. The program sends no view kick, punch or shake to any player during the
  sequence; "shake" is only the name of the boss's model frames [read].
- No explosions are thrown at any time; the only pieces are the gibs at the end (Q3) [read].

### 3. When the boss is removed, and its pieces

All in the `shake20` tick, t about 7.9-8.0 s [derived], in this order [read]:

1. The sound `boss2/pop2.wav` plays on the boss, voice channel, volume 1, attenuation normal
   (on the same entity channel as the earlier death sound, so it cuts that sound off if it is
   still playing [engine]).
2. 50 gibs are thrown [derived: 2 heights x 5 x 5]. They start on a grid around the boss's
   origin: x offsets -64, -32, 0, 32, 64; y offsets -64, -32, 0, 32, 64; z offsets +16 and +112
   units [read]. For each, a random number picks the model: below 0.3 `progs/gib1.mdl`, below
   0.6 `progs/gib2.mdl`, else `progs/gib3.mdl` (about 30/30/40 percent) [read]. Each gib uses
   the heaviest throw: velocity x and y each uniform in -1000..+1000 u/s, z uniform in
   2000..3000 u/s [derived: base x, y in -100..100, z in 200..300, times 10 for a blow below
   -200]; it bounces, is not solid, has zero size, spins at a random 0-600 degrees/s on each axis,
   frame 0, and is removed after 10-20 s (uniform) [read]. Random draws per gib, in order: the
   model pick, then the velocity (x, y, z), then the three spin rates, then the lifetime [read].
3. The finale text is sent (Q4).
4. A stand-in body is spawned: model `progs/player.mdl`, frame 1, yaw 290 degrees (pitch and
   roll 0), at the boss's original origin minus 32 on x and minus 264 on y (z unchanged). It is
   not solid, has no movement and no think [read].
5. The boss itself is removed (deleted, not merely made invisible) [read].
6. The music change and the light reset follow (Q6).

The boss's origin is moved around the grid while the gibs are placed, but the entity is removed
in the same tick, so no client sees it moved [derived].

### 4. The finale text, the next map, and the button

- When: in the `shake20` tick, t about 7.9-8.0 s, after the gibs and before the stand-in body
  [read; time derived]. It is svc 31 (finale) written to all clients reliably [read].
- The exact text (line breaks are newline characters in the string) [read]:

> Congratulations and well done! You have  
> beaten the hideous Shub-Niggurath, and  
> her hundreds of ugly changelings and  
> monsters. You have proven that your  
> skill and your cunning are greater than  
> all the powers of Quake. You are the  
> master now. Id Software salutes you.

- The next map: the finale does not set the server's next map [read]. It copies the boss's own
  `map` key (from the map file) onto each player's `map` field [read]; whether that key is set on
  `end` is a map question this source does not settle [open].
- A button press: during the intermission each player's pre-think only checks for exit, and an
  exit happens only once the exit time has passed AND attack, jump or the third button is held
  [read]. Since the exit time is 10,000,000 s away, no button does anything: the player waits
  forever, the game stays on the finale text until the user leaves by the console or menu
  [derived]. The engine's own reaction to keys on the finale screen is not in this source
  [engine].
- Exception: a client CONNECTING while the intermission flag is set forces an exit at once: the
  flag goes to 2, the exit time to now plus 1 s, and, because `end` is none of the four
  episode-end maps that have text at that step, the server changes level to its stored next map
  (or the same map if the console variable `samelevel` is set) [read; that `end` matches none of
  them derived from the four map names, `maps/e1m7.bsp`, `maps/e2m6.bsp`, `maps/e3m6.bsp`,
  `maps/e4m7.bsp`]. What that stored next map is on `end` is not settled by this source [open].

### 5. The players during the sequence; co-op

For every entity with classname `player` at t = 0, alive or dead, in single player and co-op
alike [read]:

- Eye offset set to zero (so the view sits exactly at the spot) [read].
- Angles set to the spot's `mangle`, with the fix-angle flag so the client turns at once [read].
  The program writes the same angles to the AIM angles of a different entity (the one the
  engine had as the "other" party of the touch that dealt the blow), not to the players' aim
  [read]; in the teleport path that party is the boss [derived], so the players' aim angles are
  not set by the finale [derived].
- Next think set to now + 0.5 s (their existing think, not a new one) [read].
- Made unhurtable, not solid, no movement, and invisible (model index 0) [read].
- Moved to the spot's origin [read].
- The weapon is not changed by the program (no weapon, ammo or view-model change) [read]. Whether
  the held weapon is drawn on the finale screen is the engine's decision [engine].
- From then on each player's per-frame pre-think does only the exit check, and the post-think
  stops early because the eye offset is zero: no weapon frames, firing, fall damage, power-up
  timers or other per-frame player rules run [read].
- No per-player difference in co-op: every client gets the same treatment and the same broadcast
  messages; there is no co-op branch in the finale [read]. The one co-op difference is the
  connecting-client exception in Q4, which a joining player can trigger [read].
- A player who is not yet connected at t = 0 is not moved [derived: only existing `player`
  entities are walked]; connecting triggers the Q4 exception instead [read].

### 6. Music and flags

- Music: yes, once, in the `shake20` tick after the boss is removed: svc 32 (CD track) with
  track 3 and loop track 3, to all clients reliably [read]. No other track change in the
  sequence [read].
- Light: light style 0 is set to `m` (the normal level) as the very last step [read].
- Flags and globals set: the intermission flag (1) and the intermission exit time (now plus
  10,000,000 s) at t = 0; the killed-monsters count up by one at t = 0 [read]. The server flags
  (the rune bits) are not touched [read]. No console variable is set [read]. No svc 30
  (intermission) and no svc 33 (sell screen) are sent [read].

### Summary timeline

| Time | Event | Mark |
| --- | --- | --- |
| 0 | blow; boss unhurtable; kill counted, svc 27; intermission flag 1, exit never; `misc_teleporttrain` removed; svc 31 with empty text; all players moved to the first `info_intermission`, frozen, invisible, unhurtable | read |
| 0-3 s | boss keeps idling, silent | derived |
| 1.0 s | teleport splash (svc 23, effect 11) 100 u along -y from the boss; `misc/r_tele1.wav` on the boss | read |
| 3.0 s | `boss2/death.wav` on the boss; light style 0 pulses | read |
| 3.0-3.1 s | thrash starts: `shake1`-`shake15` three times, light stepping m..a..m | derived |
| 7.5-7.9 s | `shake16`-`shake19`, light g, c, b, a | derived |
| 7.9-8.0 s | `boss2/pop2.wav`; 50 gibs; svc 31 with the finale text; stand-in `progs/player.mdl`; boss removed; svc 32 track 3 loop 3; light style 0 `m` | read, times derived |
| after | nothing: no exit on any button | derived |
