# The single-player campaign, specified

What the engine does for the classic game's **single-player program** (the `progs.dat` inside
the player's own `pak0.pak`), as rules and numbers in our words (README.md, the rules). This
note is what the building session reads; it never opens the existing implementations.

**Where each fact came from.** A research session (2026-10-30) read the 1996 source release's
single-player engine (the one that is not the network game) and the single-player game program's
QC source as released under the GPL. Nothing was measured yet: the facts marked `verify` are
checked by a lab before code depends on them. Movement is its own note (`frame-move.md`); what
a campaign's program does (monsters, their senses, the level flow), for writing our own
campaign mod, is `campaign-rules.md`.

**The shape of the work.** The campaign is the player's own program run on our VM, so the game
plays exactly as it did: the engine supplies the host the program expects. Everything below is
the difference between that host and the multiplayer host M18o/M18p built (`gameloop.md`,
`builtins.md`, `progs.md`).

## Telling the two programs apart

- A program whose engine-side layout crc is **5927** is a single-player program; **54730** is
  the multiplayer one. The crc is the header's second word (`progs.md` § The file). The host is
  chosen by it: today a 5927 program is refused at load; it is accepted once the campaign host
  exists. Anything else stays refused.
- The single-player program in the shareware pack is 5927. `verify`: read its header from the
  pack in the `pak` lab and print it.

## The single-player layout

The same file format, the same opcodes; only the engine-side globals and fields differ. Against
the multiplayer layout (`progs.md` § The engine-side interface):

- **Globals**: no `newmis`. Three globals the engine writes at map start follow `mapname`:
  `deathmatch`, `coop`, `teamplay`. Everything after shifts by two words.
- **Fields**: no `lastruntime`; a `punchangle` vector follows `avelocity`; an `idealpitch`
  float follows `v_angle`.

The full table, offsets in words (vectors three words; unmarked are floats):

### Globals (28 reserved words, then 64; 92 in all)

| Offset | Name | Offset | Name |
| --- | --- | --- | --- |
| 28 | self (entity) | 59 | v_forward v |
| 29 | other (entity) | 62 | v_up v |
| 30 | world (entity) | 65 | v_right v |
| 31 | time | 68 | trace_allsolid |
| 32 | frametime | 69 | trace_startsolid |
| 33 | force_retouch | 70 | trace_fraction |
| 34 | mapname (string) | 71 | trace_endpos v |
| 35 | deathmatch | 74 | trace_plane_normal v |
| 36 | coop | 77 | trace_plane_dist |
| 37 | teamplay | 78 | trace_ent (entity) |
| 38 | serverflags | 79 | trace_inopen |
| 39 | total_secrets | 80 | trace_inwater |
| 40 | total_monsters | 81 | msg_entity (entity) |
| 41 | found_secrets | 82 | main (function) |
| 42 | killed_monsters | 83 | StartFrame (function) |
| 43–58 | parm1 … parm16 | 84 | PlayerPreThink (function) |
| | | 85 | PlayerPostThink (function) |
| | | 86 | ClientKill (function) |
| | | 87 | ClientConnect (function) |
| | | 88 | PutClientInServer (function) |
| | | 89 | ClientDisconnect (function) |
| | | 90 | SetNewParms (function) |
| | | 91 | SetChangeParms (function) |

### Fields (105 words, in order)

modelindex, absmin v, absmax v, ltime, movetype, solid, origin v, oldorigin v, velocity v,
angles v, avelocity v, **punchangle v**, classname (string), model (string), frame, skin,
effects, mins v, maxs v, size v, touch (function), use (function), think (function), blocked
(function), nextthink, groundentity (entity), health, frags, weapon, weaponmodel (string),
weaponframe, currentammo, ammo_shells, ammo_nails, ammo_rockets, ammo_cells, items, takedamage,
chain (entity), deadflag, view_ofs v, button0, button1, button2, impulse, fixangle, v_angle v,
**idealpitch**, netname (string), enemy (entity), flags, colormap, team, max_health,
teleport_time, armortype, armorvalue, waterlevel, watertype, ideal_yaw, yaw_speed, aiment
(entity), goalentity (entity), spawnflags, target (string), targetname (string), dmg_take,
dmg_save, dmg_inflictor (entity), owner (entity), movedir v, message (string), sounds, noise,
noise1, noise2, noise3 (strings).

**The design consequence.** The loop (`plugins/shooter`) reads and writes entities through its
own `Entity`, mirrored into the VM's field memory around each call (`program.hpp`). A layout is
therefore a TABLE the mirror reads (name, offset, type, and whether the engine owns it), not a
struct: the multiplayer layout and this one are two rows of tables, and a third game's program
form (another classic game later, M18t) adds its own without touching the loop. The optional
fields looked up by name (`gravity`, `items2`, ...) work the same in both.

## Builtins

The same numbers as the multiplayer table (`builtins.md`) from 1 to 78, with these differences:

| # | Single player | Multiplayer |
| --- | --- | --- |
| 23 bprint | `(s ...)`: every argument concatenated, broadcast to every client | `(level, s ...)` |
| 24 sprint | `(client, s ...)`: concatenated, to one client; a non-client is warned and ignored | `(client, level, s ...)` |
| 48 particle | `(origin, direction, colour, count)`: a particle burst sent to every client in the unreliable stream (direction scaled by 16 and clamped to a signed byte per axis, count 255 meaning 1024). `verify` the scale | absent |
| 44 aim | autoaim: see below | as `builtins.md` |
| 70 changelevel | queues one `changelevel <map>` console line; a second call in the same map is ignored | as `builtins.md` |
| 75–77 precache_*2 | the same as 19, 20, 68 (the "registered only" name is the compiler's matter alone) | the same |
| 79–82 | absent (logfrag, infokey, stof, multicast are the multiplayer server's) | present |

The message destinations of `WriteByte` and the rest are 0 a broadcast datagram, 1 one client's
reliable stream (`msg_entity`), 2 every client's reliable stream, 3 the signon (initial state a
joining client receives). There is no multicast.

**Autoaim** (`aim`, the server setting `sv_aim`, default 0.93): from the shooter's origin 20 up,
a line along `v_forward` 2048 long; if it hits an entity whose `takedamage` is the aim value (2)
and is not a team-mate, the answer is `v_forward`. Otherwise every entity that takes aimed
damage (not the shooter, not a team-mate) is a candidate: the direction to its box centre, and
its cosine with `v_forward` must be at least the best so far, starting at `sv_aim`; a clear line
to it makes it the best. With a best target the answer keeps `v_forward`'s horizontal direction
scaled to the target's distance along it, takes the target's height difference as its z, and is
normalised (so it corrects pitch only); with none it is `v_forward`. A setting of 1 or more
turns autoaim off. On by default here, unlike the multiplayer server's default.

## The hooks and the joining client

- The program's hooks are the same globals as the multiplayer layout's (`main`, `StartFrame`,
  `PlayerPreThink`, `PlayerPostThink`, `ClientKill`, `ClientConnect`, `PutClientInServer`,
  `ClientDisconnect`, `SetNewParms`, `SetChangeParms`). The spectator hooks and the named
  extension hooks are not looked for.
- **A new client's spawn parms**: on connecting to a map it has not carried parms into, the
  engine calls `SetNewParms` (self the client) and keeps `parm1..parm16` as the client's 16
  spawn parms. On a level change (below) it keeps what `SetChangeParms` returned. On a load it
  keeps the save's.
- **Entering the game**: the client's entity is cleared to zeros, `colormap` = its number,
  `team` = its shirt colour + 1, `netname` its name; `parm1..16` = its spawn parms; `time` the
  server's; then `ClientConnect` and `PutClientInServer`, with `self` the player. (A loaded game
  skips all of it: the entity is as saved.)
- The client is then sent the four campaign statistics (below), its view angles (`angles`
  pitch and yaw, roll 0) and its client data.

## A map's start

- **Skill** is the `skill` setting rounded to the nearest whole number and clamped to 0..3,
  written back to the setting; it filters the map's entities (`progs.md` § The map's entities,
  bits 256/512/1024) and is the program's `skill` through `cvar("skill")`.
- `coop` set forces `deathmatch` to 0. The program's `coop` global is written when coop is on,
  else its `deathmatch` global; `teamplay` from its setting. `verify` both are written (the
  release writes one of the two).
- `serverflags` (the program's global) = the server's carried flags (below); `mapname` the map;
  the world's model `maps/<map>.bsp`.
- The server's clock starts at **1.0**, and two physics frames of 0.1 s run before any client
  joins (things settle, doors find their places).

## Carrying a game from map to map

The campaign is a chain of maps with the player's state carried between them, and a set of flags
carried for the whole game:

- **`serverflags`**: a float the program owns (bits 1, 2, 4, 8 are the four episodes' runes, read
  by the start map's gates). The server keeps a copy across a level change; `map` (a new game)
  sets it to 0. The HUD shows the runes: the client data's items word carries `serverflags << 28`
  in its top bits when the program has no `items2` field.
- **`changelevel <map>`** (the console command the builtin queues): for every active client the
  engine calls `SetChangeParms` (self the client) and keeps `parm1..16` as its spawn parms;
  `serverflags` is copied out; then the new map starts as above. Clients stay connected and
  re-enter (the parms come back in `ClientConnect`'s `parm1..16`).
- **`map <map>`**: a new game. The server is shut down and restarted on the map, `serverflags` 0,
  every client's parms from `SetNewParms`.
- **`restart`**: the same map again, keeping the parms the clients entered it with (a death in
  single player is followed by the program's own `restart` when the player presses a button).
- The program keeps the rest itself: the start map gives a fresh inventory when a rune is already
  held (it compares the world's model with `maps/start.bsp` and resets the parms).

## The campaign's statistics

- Four globals the program counts: `total_secrets`, `total_monsters` (as things spawn),
  `found_secrets`, `killed_monsters` (as they happen). The program announces each find and kill
  with a one-byte message of its own (`foundsecret`, `killedmonster`) on the all-clients stream.
- A joining client is sent all four as statistics 11 total secrets, 12 total monsters, 13
  secrets, 14 monsters; afterwards each message adds one to 13 or 14 on the client.
- The HUD's solo panel (shown with the scores key) is the map's message, monsters `found/total`,
  secrets `found/total` and the time on the map (minutes and seconds).

## The level's end: intermission, finale, cutscene

These are the program's: it writes the messages, the engine and the client carry them.

| Message (byte id) | Payload | The client does |
| --- | --- | --- |
| intermission (30) | none | full screen, the view frozen at the program's intermission spot; the intermission overlay: `gfx/complete.lmp`, `gfx/inter.lmp`, the completed time (minutes:seconds), secrets found/total, monsters killed/total, drawn in the big number glyphs |
| finale (31) | a string | full screen, no overlay; the string centred and typed out a character at a time at `scr_printspeed` (default 8 characters a second) from when it arrived, held until the next message |
| cutscene (34) | a string | as finale (a scripted camera; the classic campaign does not use it) |
| sellscreen (33) | none | opens the help pages (the pack's ordering screens) |
| cdtrack (32) | track byte, loop byte | music: we have none (by design); recorded and ignored |
| damage (19) | armour byte, blood byte, the damage's source point | the view's damage flash and the kick toward the source |
| setangle (10) | three angles | the view snaps to them |

The program drives the order: on touching an exit it notes the next map; on an episode's last
map the intermission is followed (on a button press after a 2 s minimum, 5 s in deathmatch) by
the episode's finale text, then the next map. A shareware game that finishes its episode gets
the sell screen. All four runes held show a last finale.

## The client data

Each frame a client gets, besides the multiplayer server's statistics: the damage message when
its `dmg_take` or `dmg_save` is non-zero (then both are cleared; the source point is the
`dmg_inflictor`'s box centre), a `setangle` when `fixangle` is set (then cleared), and its
`punchangle` and `idealpitch` (the view's recoil kick and the pitch the view drifts to with
`lookspring`).

- **The punch** decays on the server by 10 degrees a second of length (the vector's length
  shrinks toward 0), applied to the view, never to the aim. The program sets it when firing.
- **The ideal pitch** (`sv_idealpitchscale`, default 0.8): only on the ground. Six points 36,
  48 ... 96 units ahead along the yaw, each traced 160 down from eye height; if any trace starts
  in a solid or finds no floor, the ideal is left as it was; the floor heights' successive
  differences must all be the same slope (within 0.1) over at least two steps, and then the ideal
  pitch is minus that step times the scale; a flat run sets 0. The client drifts the view toward
  it when the player is not looking with the mouse.

## Episodes and the full game

The classic game was sold as a free first episode and a full game of four. Both work the same
way and the content decides which is which:

- **`registered`** is a setting the engine sets to 1 when the packs on the search path carry
  `gfx/pop.lmp` (it is in the full game's second pack and not in the shareware pack), else 0. A
  shareware pack alone plays episode 1; adding the full game's `pak1.pak` beside it opens the
  rest. `verify` against both packs with the `pak` lab: which pack carries the file.
- The program reads `cvar("registered")`. The start map's ways into episodes 2–4 are behind a
  trigger that fires only when it is 1 (and otherwise prints its message), and the shareware
  episode's end sends the sell screen instead of carrying on. Nothing else in the engine changes:
  episodes 2–4 are maps the shareware pack simply does not have.
- The old engine also checked the file's bytes against a table and refused modified games when
  not registered. We do neither: the pack's identity is classic-enhance's (`verify_pack`), and
  any readable pack is the player's own copy.

## Saves

- **Our saves** (`engine/save`, M25: a save is the state, never the inputs) hold the campaign:
  the program's globals and every entity's fields by the layout table, the light styles, the
  server's time, the skill, the map, the spawn parms and `serverflags`. Only single player
  saves; not during an intermission; not with a dead player.
- **The classic text save** (`.sav`) is readable and writable as an option, for players who
  bring their old saves: a version line (5), a comment line (the map's title and kills, spaces
  as underscores), the 16 spawn parms one a line, the skill, the map's name, the server's time,
  64 light style strings (`m` for an unset one), then a `{ }` block of the program's globals by
  name and one `{ }` block per entity (a free one as an empty block) with its non-zero fields
  by name in the map entity text's syntax. On load the map is started (its spawn functions run),
  then every entity is cleared and rebuilt from its block, the server's time set, and the server
  paused until the client is in. `verify` with a save written by an existing engine.

## The server's frame in single player

The single-player host runs the loop once per host frame (not per command): see
`frame-move.md` § The frame. It also pauses the game (no physics, no thinking) while the
console or the menu is up in a single-player game.

## Commands a campaign adds

`map`, `changelevel`, `restart`, `save <name>`, `load <name>`, `skill` (a setting, read at the
next map), `god`, `notarget`, `noclip`, `fly`, `give`, `kill`; in a game with more than one
player `coop` and `deathmatch` as settings. The menu's SINGLE PLAYER ▸ NEW GAME is `maxplayers
1`, `deathmatch 0`, `coop 0`, then `map start`.
