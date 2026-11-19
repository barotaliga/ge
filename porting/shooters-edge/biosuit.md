# The biosuit: what the first game does

WORKING AREA: deleted when porting ends. A clean-room reader's note, in our words, on the first
game's biosuit (the environment suit power-up): the item, its pickup, its 30 s of protection
from slime and drowning (and slower lava), its warning and end, death, and whether the classic
match mod can drop it. For our first-game rules beside the quad, pentagram and ring, and a
possible drop-at-death option like `sv_dropquad` and `sv_dropring`. No code, no internal
identifiers.

**Where each fact came from.** A reader session (2026-11-19) read the original game's
single-player QuakeC 1.06 (a public mirror's "original 1.06 sources" commit), the QuakeWorld
progs in id Software's Quake repository, that repository's engines (for the client's display
only) and KTX at commit `b1b23a9`. Nothing was run. **read** = as the source does it,
**derived** = follows from read facts. "SP" is the single-player progs, "QW" the multiplayer
progs. Beware: some public "1.06" mirrors carry later community fixes (one clears every
power-up's item bit at death); those are not the release.

## The item

- Class `item_artifact_envirosuit`; shown name `Biosuit`; model `progs/suit.mdl`; pickup sound
  `items/suit.wav`; warning sound `items/suit2.wav` (both precached by the spawn). (read)
- Box -16 -16 -24 to 16 16 32. Placed like every item: 0.2 s after spawn, raised 6 units and
  dropped to the floor, removed if it falls out of the level. (read)
- Item bit 2097152 (bit 21), the fifth inventory slot of the status bar (keys, ring, pentagram,
  suit, quad from bit 17). (read)
- No glow on the item or the wearer in SP or QW. KTX makes both glow green. (read)

## Pickup

- Players only, health above 0. Holding one already does not refuse it: the timer RESETS to
  30 s (not added) and the warning re-arms. (read)
- Sets the bit and an end time of now + 30 s. Message to the taker `You got the Biosuit` (QW:
  low print level); `items/suit.wav` on the taker's voice channel, normal attenuation; console
  command `bf` (bonus flash) to the taker; the item's map targets fire with the taker as
  activator. (read)
- KTX: the item channel for the sound; refused unless power-ups (`k_pow`) and the suit switch
  (`k_pow_s`, default 1) are on and, outside practice, a match is running; with `k_pow_pickup`
  (default 0) a running suit refuses another. (read)

## Respawn

- 60 s, deathmatch 4 included (the ring and pentagram take 300 s); `items/itembk2.wav` on
  return. (read)
- SP: only in deathmatch; in single player and coop a taken suit never returns. QW: always,
  coop included. KTX: 60 s (30 s in practice mode). (read)

## While it runs

Depth = water level, 1 feet, 2 waist, 3 eyes. Liquid damage is checked each player frame before
movement; the suit's upkeep each frame after it. (read)

| Hazard | Without the suit | With the suit |
| --- | --- | --- |
| Slime | 4 x depth every 1 s | none (read) |
| Lava | 10 x depth every 0.2 s (50/100/150 per s) | 10 x depth every 1 s (10/20/30 per s) (read; rates derived) |
| Drowning | 12 s of air, then every 1 s: 4, 6, 8, 10, 12, 14, then 10, 12, 14, 10 ... | air refilled to 12 s every frame: never drowns (read) |

- Entering a liquid resets the damage clock, so the first tick lands on the first frame in. (read)
- Liquid damage is ordinary world damage: armour absorbs it, the pentagram stops it. Nothing else
  reads the suit (not the lightning discharge, not any weapon). (read)
- No gasp on surfacing while worn; at the end the player has a full 12 s of air. The drowning
  step is not reset by the suit, only by surfacing. (derived)
- Client: green screen tint at 20 per cent while the bit is set (the quad's blue wins over it,
  it wins over the ring's and pentagram's); the `sb_suit` icon, flashing 2 s after it is
  gained. (read)

## Running out

- First frame with under 3 s left: `Air supply in Biosuit expiring` to the wearer (QW: high
  print level), `bf`, and `items/suit2.wav` (automatic channel). Once. Then `bf` alone once a
  second until the end, about three flashes in all. (read; the count derived)
- First frame past the end: the bit is removed and the timer cleared. No message, no sound.
  (read) KTX: the same.
- The timer is never sent to the client; only the item bit is. KTX adds spectator, demo and log
  lines ("<name> got Biosuit"), not a stat. (read)

## Death and level change

- Death zeroes the timer but removes only the ring's item bit, so the dead player keeps the suit
  icon and the green tint until respawn (SP 1.06, QW and KTX alike). Respawn in deathmatch
  rebuilds the items from the defaults. (read; the client consequence derived)
- A level change strips the suit (with keys and the other power-ups) from what is carried. (read)
- Nothing drops the suit. QW drops the quad and the ring under server-info keys `dq` and `dr`
  (non-zero = on); there is no key for the suit. SP drops no power-ups. (read)

## KTX and dropping it

- No setting drops the suit. `dq` (command `dropquad`) and `dr` (`dropring`) are off unless set,
  and also need power-ups and that item's switch on. KTX's drop accepts only the quad, ring and
  pentagram. (read)
- Its dropped power-up (the shape our option would copy): left at the dead player's origin during
  a running match when time remains, carrying its end moment; the first live player to touch it
  takes it under the normal pickup rules and gets the remaining seconds ADDED to any he holds,
  capped at 30 s from now; it never respawns. Spectators see "<name> dropped a <item> with N
  seconds left" and "<name> got a <item> with N seconds left". KTX's suit pickup already honours a
  carried time; only the drop is missing. (read)
