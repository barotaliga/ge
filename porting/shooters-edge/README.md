# Shooter's Edge: the working notes

**A WORKING AREA**, like the rest of `porting/`: deleted when the port ends. The design is
`docs/shooters-edge.md`; this directory holds what only matters while the game is being built:
the survey of existing implementations, and the specifications the building sessions read.

Shooter's Edge is compatible with QuakeWorld: its file formats, its QuakeC and its protocol. This
working area is the only place in the tree that names them; everywhere else they are "the
classic game", "QC" and "the classic protocol", and `make agents-check` holds it.

| File | What |
| --- | --- |
| `README.md` | This page: the clean-room rules for these notes |
| `survey.md` | What today's community clients and servers do, their licences, and which features the plan takes |
| `configs.md` | The console language at its modern extent, as a specification in our words (`engine/console` is built from it) |
| `installs.md` | The nQuake and nQuake2 installs: layouts, file kinds, the bundled clients' config order and search paths (clean-room facts) |
| `loc-editing.md` | Making and editing place names in game: the commands, the file written, loading (clean-room facts) |
| `formats.md` | The file formats: pak, pk3, the search order, BSP29 lump by lump, BSP2, MDL, SPR, WAD2, LMP, WAV |
| `progs.md` | QuakeC bytecode for the VM (M18c): the file and its checks, memory and value encodings, calls, every opcode's semantics, the engine-side globals and fields, the map's entity text, allocating and freeing entities |
| `builtins.md` | The modern server's builtins by number, its extension builtins and list, the monster moves, the hooks and when they are called, client commands, the extension fields (M18p) |
| `qc-strings.md` | The extended string builtins (the string set: stov, strstrofs, str2chr, chr2str, strconv, strpad, infoadd, infoget, the comparisons) and sprintf, each edge case, from a clean-room reader (M18l) |
| `entity-look.md` | An entity's look fields (scale, colormod, alpha) in QC and their bytes on the wire, the entity delta's and the player update's encodings, from a clean-room reader (M18l) |
| `sky-box.md` | A map's own sky box: its name in the worldspawn, the six pictures' paths and formats, their orientation, where the box is drawn; the sequel's rotation, from a clean-room reader (M18i12) |
| `player-skins.md` | Each player's skin (the name chosen, the picture and its cut, the fallbacks, the colours' rows, `noskins`, `baseskin`, `allskins`, 24-bit forms), from a clean-room reader (M18i15) |
| `fog-alpha.md` | A map's fog (its worldspawn keys, the curve, the sky's share, the command, the reset) and see-through liquids (the alpha settings, the server's permission, the drawing), from a clean-room reader (M18i13, M18i14) |
| `movement.md` | The player's movement at the modern server's defaults: the settings, every step of the move, the clip hulls and the trace, the test vectors' format (M18o1) |
| `gameloop.md` | The game loop: the game program's hooks, entities, the area tree, the server frame, thinking, tossing, pushers, touching, the player's packet and command, client prediction (M18o2) |
| `vectors/` | Measured test vectors: `pmove.txt` (the player's move in hand-built worlds), `framemove.txt` (the original move in the same worlds), `hulltrace.txt` (single traces by the original single-player trace in the same worlds: where the end point lands at corners and slants) and `entity.txt` (tossed, bounced and flying entities), for `se-parity` |
| `protocol.md` | The classic protocol, version 28: connectionless packets, the net channel, the messages to fill with M18g |
| `campaign.md` | The single-player program's host (M18s): its layout (crc 5927), the builtin differences, spawn parms, `serverflags`, level changes, intermission and finale, the statistics, episodes and the full game, saves |
| `scoreboard.md` | The scores key's boards (M18k3): the network client's players' and teams' scoreboards, what fills each row, the scorebar, the single-player solo scorebar and the intermission's positions |
| `frame-move.md` | The original player move and the movement setting `sv_playermove` (M18s): who jumps in each mix of program and move, the single-player frame |
| `campaign-rules.md` | What a campaign program does (monsters' senses and states, the roster, the level flow), for our own campaign variant mod (M18s7) |
| `deaths.md` | Monster deaths, gibs and blood: each kind's death sequences, gib health and pieces, the gib and head throw, blood at hits, the client's bursts and model-flag trails, bouncing, the zombie, the player (M18s7) |
| `deathmatch4.md` | Deathmatch 4 in the classic match mod: the armed spawn, spawn invincibility, the items stripped and kept, backpacks and bonus powers, OctaPower, its settings and commands, spawns and scoring |
| `midair.md` | Midair in the classic match mod (clean-room answers): the toggle and the minimum height, the rockets-only spawn, when a rocket kills, the height trace, knockback, the score bands, the messages and statistics |
| `weapon-switch.md` | The weapon after a shot in the classic match mod: refire times, the switch held until the refire ends, what a dropped pack holds, what onlookers can tell, the client's switch-after-fire settings |
| `demo-smoothing.md` | Recorded games drawn between their frames: the client's one-frame-behind playback, the rules for players, the tracked view and entities, where it snaps |
| `held-weapons.md` | The first-person weapon's frames: the axe's two swings, how a swing is picked, when the frame is set, what the client draws |
| `rocket-jump-helper.md` | The classic match mod's rocket jump helpers (impulses 164 and 156): what they do in one command, their failures and settings, from a clean-room reader (M18e) |
| `jump-walk-death.md` | The jump's and the swimmer's sounds, landing sounds, walking (there is none at the modern defaults: `+speed` is a run key), the view weapon and eye at death |
| `sequel.md` | The sequel (M18t2, for M18t3 and M18t4): the index of its notes, the game's shape, the demo pack measured, the game library's calls as our rules runner's |
| `sequel-formats.md` | The sequel's files: the search order, contents and surface flags, the collision lumps, WAL, PCX and the palette, MD2, SP2, the sky, player models, skins and sounds |
| `sequel-movement.md` | The sequel's player move (the packed state, the command, every step, snapping) and the brush trace and point contents it moves through |
| `sequel-rules.md` | The sequel's rules: the 10 Hz frame, entities and physics, items, armour, damage, every weapon and projectile, a player's life, deathmatch and `dmflags`, the demo maps' entities |
| `sequel-monsters.md` | The sequel's monsters on the demo maps: life, senses, the attack choice, the roster's numbers |
| `sequel-monsters-full.md` | The full sequel's other monsters (M18v7d3): the medic and its revival, the brains, iron maiden, gladiator, mutant, technician, icarus, shark, super tank, hornet, Jorg and the Makron; the step move; the demo roster's speeds |
| `sequel-movers.md` | The sequel's map movers and triggers (M18v7c): the plain and accelerated moves frame by frame, doors, buttons, plats, trains, rotators, walls, timers, the triggers, teleporters, riders, the exit and the targets |
| `sequel-demo.md` | The sequel's client recording (.dm2) and protocol 34's server messages: the framing, every message's layout, the entity and player deltas, the signon, the encodings; the demo's recording measured (M57f) |
| `sequel-campaign.md` | The sequel's single-player campaign (M18v7d2): the help computer and its counters, what carries across levels and units, cross-level bits, revisited levels, saves and the autosave, the remaining single-player entities, co-op, the end of the game |
| `sequel-hud.md` | The sequel's HUD (M18v4): the layout language word by word, the digit fields and their flashing, the 18 stats the stock game fills, the two status bars as elements and positions, the scoreboard, help computer and inventory screens, the centre print and the crosshair |
| `localization.md` | The re-release's text keys (clean-room answers): which prints carry `$` keys, how arguments fill `{}`/`{N}`, its extra builtins by number and by name, the words file's place and format, where an engine resolves them |
| `television.md` | The classic match broadcaster (clean-room answers): its stream protocol, handshake and auth, the delay, catch-up, chat and viewer lists, the server's side, discovery, HTTP and files (M57) |
| `forward.md` | The classic packet forwarder (clean-room answers): the route in the userinfo, the connect hop by hop, forwarding and timeouts, its queries, the client's route finder (M57) |
| `proxy.md` | The classic proxy and its compressed client demos, from public documents and demo bytes only: features, commands, link compression, recording, observers, what is known of .qwz (M57) |
| `voice.md` | Voice and chat targets in the voice extension: `voicetarg`, `unmuteall`, `vignore`, the chat line; no proximity routing anywhere |
| `hub.md` | The classic community's live-game and demo website (clean-room answers): its pages and API, the master and status queries, modes and match state, the broadcaster's advert, watching in a browser, the demo index (M56, `docs/hub.md`) |
| `accounts.md` | Logins and match stats on the classic servers and both games' stock clients (clean-room answers): the console login, the accounts file, the web login and its token, the status and scoreboard keys, the match stats file, `lastscores`, frag logs (M59, `docs/accounts.md`) |

The recorded-game formats are built (M18h, `games/shooters_edge/client/demo.hpp`) and their note
retired. Every fact marked `verify` is checked by a lab before code depends on it.

## The rules for a note

The implementations surveyed here are GPL. A note is how their behaviour reaches this tree
without their code (`docs/shooters-edge.md` § Licensing and the clean room):

- **Facts, in our words.** Byte layouts, numbers, message orders, measured curves, what a
  command does. Never a quotation of source, never a function transcribed into prose step by
  step, never a table copied out of source that the format itself does not define.
- **Say where each fact came from**: a public format description, the language reference, a
  measurement (name the lab and its recording), or a research session that read source (named
  as such, so the building session knows not to look further).
- **Separate agents.** A reader agent that reads GPL source to answer a question writes only the
  note (or a scratch answer document) with no code. The agent that builds or verifies the
  subsystem reads the note and the measurements, never the source, not even a grep hit. The
  `clean-room` skill is the procedure: the questions, the reader's brief, the screen for code.
- **Running is fine; shipping is not.** A lab may fetch and build an existing server or client
  into its cache to measure it. Nothing of it is committed, linked or distributed.

## The shareware release

The labs and the client that need a real map take `--game=<folder>`. The folder is laid out in
one step from the shareware release 1.06 zip as published, which its licence lets anyone keep and
pass on (its SHA-256 is `kKnownZips` in `plugins/classicenhance/source.hpp`):

```sh
ge tool classicenhance -- setup https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip \
    --basedir=<scratch>/classic --cache=<scratch>/cache
# then --game=<scratch>/classic/id1
```

The zip is fetched once into `<scratch>/cache/downloads/` and checked against the known digest; a
second `setup` reads that copy. Nothing it writes goes into the tree.

