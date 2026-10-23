# QuakeWorld: the working notes

**A WORKING AREA**, like the rest of `porting/`: deleted when the port ends. The design is
`docs/quakeworld.md`; this directory holds what only matters while the game is being built: the
survey of existing implementations, and the specifications the building sessions read.

| File | What |
| --- | --- |
| `README.md` | This page: the clean-room rules for these notes |
| `survey.md` | What today's community clients and servers do, their licences, and which features the plan takes |
| `configs.md` | The console language at its modern extent, as a specification in our words |

More notes land with their rows: `formats.md` (archives, BSP, MDL, SPR, WAD2, LMP, WAV),
`quakec.md` (the bytecode, the field layout, the builtins by number), `protocol.md` (version 28,
the extensions), `movement.md` (the measured movement model), `demos.md` (QWD and MVD).

## The rules for a note

The implementations surveyed here are GPL. A note is how their behaviour reaches this tree
without their code (`docs/quakeworld.md` § Licensing and the clean room):

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
