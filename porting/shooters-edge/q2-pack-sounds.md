# Quake II mission packs: the client's own sounds (facts)

Clean-room facts, no code. Read: Q2PRO master (601a8df8, 2025-12) and The Reckoning and
Ground Zero game libraries as released under the GPL (yquake2 ports, 2026-11). The full
answer document lives in the session's scratchpad; these are the facts worth keeping.
Not covered here: pack temp entities 26, 28, 30, 33, 35, 38, 39, 42-44, 46, 47, 51-55 and
the sounds lashit, tesla, disrupthit and rocklx1a (another note).

Channels: auto 0, weapon 1. Attenuation: none 0, normal 1, idle 2, static 3. A player
flash byte with bit 128 (silencer) plays at volume 0.2 instead of 1. Monster flashes are
always volume 1. Every flash sound plays on the firing entity's weapon channel. None of
these sounds are random or pitched, with one exception (the hyper soldier below).

## Player muzzle flashes (svc_muzzleflash)

| No. | Sent by | Sound |
| --- | --- | --- |
| 16 | ion ripper ("boomer"), The Reckoning, silencer bit honoured | `weapons/rippfire.wav`, normal |
| 17 | blue hyperblaster (not sent by a player) | none |
| 18 | phalanx, second barrel only, silencer bit honoured | `weapons/plasshot.wav`, normal |
| 30 | ETF rifle, Ground Zero, never silenced | `weapons/nail1.wav`, normal |
| 31 | remaster only (prox) | `weapons/grenlf1a.wav` + `weapons/proxlr1a.wav` on auto at 0.1 s |
| 32 | classic: not sent (shotgun 2) | `weapons/shotg2.wav`; remaster protocol: `weapons/nail1.wav` |
| 33 | heat beam, every firing frame | none (the game loops `weapons/bfg__l1a.wav` on the player) |
| 34 | blaster 2 (not sent by a player) | `weapons/blastf1a.wav` |
| 35 | disruptor, never silenced | `weapons/disint2.wav`, normal |
| 36-39 | armed nuke every 0.1 s, by damage x1/x2/x4/x8 | none; 100 ms light red/yellow/blue/cyan |

The prox launcher sends the base grenade flash (8). The chainfist and the trap send no
flash; their sounds are the game's (`weapons/sawidle.wav`, `weapons/sawhit.wav`, and
`weapons/trapcock.wav`, `weapons/traploop.wav`).

## Monster muzzle flashes (svc_muzzleflash2), Ground Zero's 138-210

The Reckoning adds no monster flash numbers. All at volume 1, weapon channel.

- `infantry/infatck1.wav`, attenuation NONE: 138, 152 (carrier machine gun left 1, 2).
- `infantry/infatck1.wav`, normal: 141 (turret machine gun).
- `chick/chkatck2.wav`: 142 (turret rocket). `soldier/solatck2.wav`: 143 (turret blaster).
- `tank/tnkatck3.wav`: 144 stalker, 145 daedalus, 146 medic commander's blaster, 149 and
  156-190 (every widow blaster, sweep and run number).
- `weapons/disint2.wav`: 148 (widow disruptor).
- `tank/rocket.wav`: 191 (carrier rocket 1) only.
- Silent: 139, 153 (carrier machine gun right), 140 (carrier grenade), 147, 150, 154, 155
  (rails), 151 and 195-210 (beams), 192-194 (carrier rockets 2-4, left out on purpose).

The Reckoning's hyper soldier (skins 2-3) sends monster flash number 17, the PLAYER blue
hyperblaster number, which the monster table reads as tank machine gun 14: the client plays
one of `tank/tnkatk2a.wav` .. `tank/tnkatk2e.wav` (1 in 5 each), normal attenuation. Keep
that sound; it is what players of the pack hear.

## Temp entities, effects, precache

- 41 (bubble trail 2, the heat beam through water): `weapons/lashit.wav`, auto, normal.
- 27, 29, 32, 34, 36, 37, 40, 45, 48, 49, 50: no client sound. 31 plays the base rail sound
  but no pack sends it.
- No effect bit (tracker, plasma, trap, ion ripper, tag trail, sphere, double damage...)
  starts a client sound; every pack loop is an entity's looping-sound field set by the game.
- The client precaches only `weapons/tesla.wav` and `weapons/disrupthit.wav` as pack sounds,
  every map, unconditionally. Flash sounds load on first play. Nothing depends on the game
  folder's name.

## A disc copier's `.mds` sheet (measured on Ground Zero's disc copy, 2026-11-23)

Measured by reading the sheet's bytes (1380 bytes, beside a 643,603,680-byte `.mdf`); nothing
from a copier or any other reader's source. Little-endian throughout.

- The signature `MEDIA DESCRIPTOR` at 0, then version 1.5. At 80 the session block's offset (96).
- The session block: its first sector (-150) and end (263,060), the session number, at 10 the
  count of track blocks as one byte (14: three lead-in entries and eleven tracks), at 11 the
  lead-in entries' count (3), the first and last track (1, 11), at 20 the track blocks' offset.
- A track block is 80 bytes: the mode at 0 (0xAA the data track, 0xA9 audio), 8 at 1 (a full
  96-byte subchannel after each sector), the control byte at 2, the point at 4 (1..11 the tracks,
  0xA0..0xA2 the lead-in's entries, all else zero), the start as minutes, seconds and frames at
  9..11, at 12 the offset of its extra block, at 16 its sector size in the image (2448), at 36
  its first sector (the logical address of its index 1), at 40 its byte offset in the image (64
  bits), at 48 the count of image files (1), at 52 the footer's offset.
- The extra block (8 bytes a track): the pregap in sectors (150 each), then the length in
  sectors from this track's index 1 to the next's (its pregap included), or to the end for the
  last.
- Offsets: track 1's is 0 and its first sector 0 (its pregap is not in the image). Every later
  track's byte offset is its first sector minus 150, times 2448: the image holds the pregap
  before index 1, so the image's sector n is the disc's logical sector n throughout.
- The footer at 1352: the name's offset (1368) and a flag for UTF-16 (1); the name is `*.mdf`,
  meaning the sheet's own name with `.mdf`.
- Measured on the result: the data track's first sector starts with the sync pattern and mode
  1; audio read as the first 2352 bytes of each 2448-byte sector at that offset has an adjacent
  sample correlation of 0.989 (music); one byte off gives 0.002. The ten audio tracks (2..11)
  came out at the sheet's lengths (track 2: 13,234 sectors).

