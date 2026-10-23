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
| `progs.md` | QuakeC bytecode: the file, the records, the globals, every opcode; the engine-side fields and the builtins to fill with M18c |
| `movement.md` | The player's movement: the server's settings and the model, as starting values for `se-parity` |
| `protocol.md` | The classic protocol, version 28: connectionless packets, the net channel, the messages to fill with M18g |

Still to come with its row: `demos.md` (QWD and MVD, M18h). Every fact marked `verify` is
checked by a lab before code depends on it.

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
