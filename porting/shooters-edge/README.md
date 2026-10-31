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
| `formats.md` | The file formats: pak, pk3, the search order, BSP29 lump by lump, BSP2, MDL, SPR, WAD2, LMP, WAV |
| `progs.md` | QuakeC bytecode for the VM (M18c): the file and its checks, memory and value encodings, calls, every opcode's semantics, the engine-side globals and fields, the map's entity text, allocating and freeing entities |
| `builtins.md` | The modern server's builtins by number, its extension builtins and list, the monster moves, the hooks and when they are called, client commands, the extension fields (M18p) |
| `movement.md` | The player's movement at the modern server's defaults: the settings, every step of the move, the clip hulls and the trace, the test vectors' format (M18o1) |
| `gameloop.md` | The game loop: the game program's hooks, entities, the area tree, the server frame, thinking, tossing, pushers, touching, the player's packet and command, client prediction (M18o2) |
| `vectors/` | Measured test vectors: `pmove.txt` (the player's move in hand-built worlds), `framemove.txt` (the original move in the same worlds), `hulltrace.txt` (single traces by the original single-player trace in the same worlds: where the end point lands at corners and slants) and `entity.txt` (tossed, bounced and flying entities), for `se-parity` |
| `protocol.md` | The classic protocol, version 28: connectionless packets, the net channel, the messages to fill with M18g |
| `campaign.md` | The single-player program's host (M18s): its layout (crc 5927), the builtin differences, spawn parms, `serverflags`, level changes, intermission and finale, the statistics, episodes and the full game, saves |
| `scoreboard.md` | The scores key's boards (M18k3): the network client's players' and teams' scoreboards, what fills each row, the scorebar, the single-player solo scorebar and the intermission's positions |
| `frame-move.md` | The original player move and the movement setting `sv_playermove` (M18s): who jumps in each mix of program and move, the single-player frame |
| `campaign-rules.md` | What a campaign program does (monsters' senses and states, the roster, the level flow), for our own campaign variant mod (M18s7) |

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
- **Separate sessions.** A session that reads GPL source to answer a question writes only the
  note. The session that builds the subsystem reads the note and the measurements, not the
  source.
- **Running is fine; shipping is not.** A lab may fetch and build an existing server or client
  into its cache to measure it. Nothing of it is committed, linked or distributed.
