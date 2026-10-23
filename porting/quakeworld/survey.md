# Survey: today's QuakeWorld implementations

Research for `docs/quakeworld.md` § Modern features, written 2026-10-23. Read for ideas, never
for code: every project below is GPL (README.md, the rules).

## The projects

| Project | What it is | Licence | What we take as ideas |
| --- | --- | --- | --- |
| The original release (1996–97, source released 1999) | The game, its client, its server and the QuakeC game logic | GPL-2.0 | The behaviour every compatible program keeps: the protocol, the movement, the QuakeC interface |
| ezQuake | The competitive community's client | GPL-2.0 | The element HUD, demo playback with seeking and the demo browser, independent physics and render rates, raw mouse, the advanced config language (conditionals, `set_calc`, message triggers, teamplay macros and location files), the server browser, multiview of MVDs |
| FTE | A client and server of both the single- and multiplayer protocols, with the widest extension set | GPL-2.0-or-later | The protocol extension bits (float coordinates, transparency, more entities, replacement deltas, voice), the QuakeC extension registry (`checkextension` and the extension builtins and fields), client-side QuakeC, scripted particles, skeletal models, `.pk3dir` development directories, splitscreen, IPv6 |
| MVDSV | The competitive community's server | GPL-2.0 | Server-side MVD recording, spectators and QTV streams, lag compensation for hitscan, float coordinates as a per-client extension, the stream's extension flags |
| KTX | The competitive game mod, run by MVDSV | GPL-2.0 | Built as a native library or a bytecode for a different VM, not QuakeC: running it would need a second VM or loading native code at run time, which the engine does not do (non-goal). Its rules (match modes, ready-up, timers) are a QuakeC mod's to reimplement if wanted |
| LibreQuake | Free replacement game data | BSD-3-Clause | The labs' content: maps, models, sounds, textures (fetched, hash-checked, not committed) |

## Decisions taken from the survey

- **Extensions in tiers** (`docs/quakeworld.md` § The network): first the ones a public server
  negotiates today (float coordinates, transparency, more entities, larger lists, MVD flags),
  then the QuakeC extensions mods ask for most, then client-side QuakeC.
- **Our own QuakeC compiler**, because the existing compilers are GPL and the labs need
  bytecode built in the tree.
- **No KTX compatibility in the first rows**: it is not QuakeC. A server that wants its rules
  runs a QuakeC mod with them.
- **The config language at its fullest** (`configs.md`), because players' configs are part of
  their muscle memory.

## Sources for the specifications

- The file formats: public descriptions of the pak, BSP, MDL, SPR, WAD2 and LMP layouts (the
  community's format specifications and wikis), checked against the free content's files.
- QuakeC: the language reference shipped with the original QuakeC release, and public lists of
  the builtins by number and the extension names.
- The protocol: public descriptions of version 28's messages and of the extension bits, checked
  against captures of existing programs on loopback (`qw-interop`).
- Movement: measured, `qw-parity`.
