# The Quake mission packs on the classic status bar: answers

Read: id Software's released Quake source (github.com/id-Software/Quake, the NetQuake client
that builds WinQuake and GLQuake; both share one status bar). Program-side values: the 2021
Quake re-release QuakeC (github.com/id-Software/quake-rerelease-qc, GPLv2), its Scourge of
Armagon and Dissolution of Eternity code bases. That is the re-release, not the 1997 program.
Where a value is taken from it, the client's own branch agrees with it unless noted. Measured:
the gfx.wad inside /mnt/project-files/paks/quake1/{hipnotic,rogue}/pak0.pak and the base pak0.pak,
pictures read out and looked at.

Tags: [read] = stated by the source; [measured] = taken from the pack files; [derived] =
worked out from read facts.

Coordinates. "Bar" = the 320x24 main status bar, (0,0) its top-left. The inventory bar sits
directly above it, so its rows are bar y -24 to -1; "ibar" coordinates are the inventory bar's
own (ibar y = bar y + 24). Everything below is the same in both the windowed-centre and the
deathmatch-left-anchored placement the base game uses; the packs do not change the placement.
Characters drawn on the bar get the base game's extra +4 px in x; the x values below already
include it.

## 1. How the client knows the pack

- [read] Only by the command-line switches `-hipnotic` and `-rogue`. Each sets its own pack
  flag and clears the "standard Quake" flag. The same switch also adds the folder `hipnotic` or
  `rogue` (under the base directory) to the search path after `id1`.
- [derived] `-game hipnotic` alone (no switch) loads the pack's files but the client stays in
  standard mode: no pack pictures loaded, no pack drawing, the standard weapon protocol (below).
- [derived] Giving both switches sets both flags; both branches then run wherever they are
  independent (nothing stops it).
- [read] Everything in questions 2-7 depends on that switch, not on data: which extra pictures
  are loaded at start-up, every pack branch in the bar, the item-bit tests, the team border.
- [read] One protocol change also hangs on it, in both the server and the client half of the
  same program: the active-weapon byte in the per-frame client data. Standard mode sends the
  weapon value itself as a byte. Pack mode sends the index (0-31) of the lowest set bit of the
  weapon value, and the client stores 1 shifted left by that index as the active weapon. So in
  pack mode the active-weapon stat is always a single bit and can exceed 255 (laser cannon
  2^23, plasma gun 65536).
- [read] Edge case: in pack mode, when the weapon value is 0 the server writes no byte at all,
  while the client always reads one (it misreads the next field). [derived] A pack client on a
  standard server, or the reverse, misreads the weapon (byte value vs bit index).
- [read] The other thing that is data-driven, not switch-driven: the server builds the 32-bit
  items word it sends from the player's `items` field OR the `items2` field shifted left 23
  bits, if the loaded program defines a field named `items2`; otherwise it ORs in the
  `serverflags` global shifted left 28 bits (the four sigils). Both packs' programs define
  `items2` [read in the re-release QC], so in both packs bits 28-31 are no longer the sigils.

## 2. Scourge of Armagon: weapons

Bits [read; the re-release QC agrees]:
- laser cannon: 8388608 (bit 23), in `items`.
- Mjolnir: 128 (bit 7), in `items`.
- proximity gun: 65536 (bit 16), in `items`.

Pictures loaded (only with the switch) [read], sizes [measured]:

| Weapon | plain (owned) | lit (active) | flash frames 1..5 | size |
| --- | --- | --- | --- | --- |
| laser cannon | INV_LASER | INV2_LASER | INVA1_LASER .. INVA5_LASER | 24x16 |
| Mjolnir | INV_MJOLNIR | INV2_MJOLNIR | INVA1_MJOLNIR .. INVA5_MJOLNIR | 24x16 |
| grenade launcher, prox owned too, GL showing | INV_GREN_PROX | INV2_GREN_PROX | INVA1_GREN_PROX .. INVA5_GREN_PROX | 24x16 |
| prox gun, GL owned too, prox showing | INV_PROX_GREN | INV2_PROX_GREN | INVA1_PROX_GREN .. INVA5_PROX_GREN | 24x16 |
| prox gun alone | INV_PROX | INV2_PROX | INVA1_PROX .. INVA5_PROX | 24x16 |

[read] Which row the client uses: "plain" when the weapon is owned and not active and not
flashing; "lit" when it is the active weapon; a flash frame during the first second after
pick-up (timing below). The base set is the same: INV_* plain, INV2_* lit, INVA1..5_* flash.

Positions (y = bar -16, i.e. ibar y 8, same row as the base weapons) [read]:
- laser cannon: x 176.
- Mjolnir: x 200.
- grenade launcher / proximity gun: x 96, the base grenade launcher's slot (base picture
  INV_RLAUNCH, which in the base set is the grenade launcher).
- [measured] The pack's own lightning gun pictures (INV_LIGHTNG, INV2_LIGHTNG, INVA1..5_LIGHTNG)
  are 32x16, not the base set's 48x16, so the lightning gun at x 144 ends at 175 and the laser
  cannon follows at 176, Mjolnir at 200 (ends 223). The pack's IBAR differs from the base one
  only in the region x 175-319, ibar y 8-23 (new slot frames there; the base bar's four sigil
  marks at the right end are gone). The pictures carry their key numbers: laser "9",
  Mjolnir "0", the grenade/prox pictures "6" and/or "ALT".
- [measured] Both packs' gfx.wad hold every base picture as well, so the pack wad replaces the
  base one whole.

Grenade launcher vs proximity gun at x 96 [read]. The base weapon pass draws the grenade
launcher (value 16, bit 4) at x 96 first, as in the base game. The pack pass then goes over it:
- Grenade launcher owned (16) AND proximity gun owned (65536):
  - if the grenade launcher is active or flashing, draw the GREN_PROX picture in the launcher's own state (lit or its flash frame),
  - otherwise, if the proximity gun is active or flashing, draw the PROX_GREN picture in the prox gun's state (lit or flash frame),
  - otherwise draw the plain INV_PROX_GREN.
  - When the launcher is lit or flashing, the prox gun's state is ignored (the launcher wins).
- Proximity gun owned, grenade launcher not owned: draw INV_PROX / INV2_PROX / INVA*_PROX by the
  prox gun's own state.
- Grenade launcher owned, proximity gun not owned: nothing extra; the base INV_RLAUNCH family
  stays.
- [derived] "Active" here is the single-bit active-weapon stat from question 1: 16 for the
  launcher, 65536 for the prox gun.

Flash timing [read], the same rule as the base weapons: the client keeps, per bit of the items
word, the client time at which that bit went from 0 to 1. Frame number n = whole tenths of a
second since then. For n below 10 (the first second) the picture is flash frame (n mod 5) + 1,
i.e. INVA1..INVA5 each for 0.1 s, twice. From n = 10 on, the lit picture if active, else the
plain one. For the pack weapons the time is taken from the weapon's own bit (23, 7, 16; the
launcher's 4). The bar is forced to redraw every frame while any flash runs.

## 3. Scourge of Armagon: items

- [read in the re-release QC] Wetsuit: `items2` value 2; empathy shields: `items2` value 4,
  (horn of conjuring: `items2` value 8). [derived] Sent shifted left 23: wetsuit = bit 24
  (16777216), empathy shields = bit 25 (33554432), horn = bit 26 (not drawn).
- [read] The bar tests exactly bits 24 and 25. (The client's own header names the wetsuit as
  bit 25 and the empathy shields as bit 26, but the bar does not use those names; it reads
  24 and 25, which is what the program sends.)
- [read] Pictures SB_WSUIT (bit 24) and SB_ESHLD (bit 25), [measured] 16x16 each.
- [read] Positions on the inventory bar's item row (bar y -16, ibar y 8): wetsuit x 288,
  empathy shields x 304.
- [read] Items are never blinked (the item "flash frame" branch can never be taken in this
  client, base or pack): an owned item's picture is always drawn; the bar only redraws every
  frame for 2 s after pick-up.
- Keys move [read]: with the switch, the two keys are NOT drawn on the inventory bar (base:
  key 1 at x 192, key 2 at x 208, ibar y 8). Instead they are drawn on the main bar, after the
  bar picture and before the numbers: key 1 (silver, bit 17) at bar (209, 3), key 2 (gold,
  bit 18) at bar (209, 12). [measured] The pack's SB_KEY1 and SB_KEY2 are 16x9 (base 16x16),
  so they stack in the space left of the ammo icon (x 209-224; the ammo icon starts at 224).
  The key slots at x 192 and 208 on the inventory bar stay empty.
- [read] Other items keep the base places: invisibility x 224, invulnerability 240, biosuit
  256, quad 272 (ibar y 8).
- Sigils [read]: still drawn in this pack at x 288, 296, 304, 312 (8x16) for bits 28-31,
  after (on top of) the pack items. [derived] Because the program defines `items2`, bits 28-31
  come from `items2` values 32, 64, 128, 256, which the pack defines nothing for: the sigils do
  not appear in practice.
- [read] No change to the ammo counters, the face, the armour number or icon, the health, the
  ammo icon or the colours of the numbers.

## 4. Dissolution of Eternity: how the extra items arrive

- [read] No new message or stat: the same 32-bit items word of the per-frame client data.
  Bits 0-22 are the program's `items`; bits 23-31 are its `items2` bits 0-8 (shifted left 23,
  see question 1). The active weapon is the pack-mode single-bit stat.
- Values [read in the client; the re-release QC agrees]:

| Thing | In the program | Bit in the items word | Value in the items word |
| --- | --- | --- | --- |
| lava nailgun | `items` 4096 | 12 | 4096 |
| lava super nailgun | `items` 8192 | 13 | 8192 |
| multi grenade launcher | `items` 16384 | 14 | 16384 |
| multi rocket launcher | `items` 32768 | 15 | 32768 |
| plasma gun | `items` 65536 | 16 | 65536 |
| shells / nails / rockets / cells (ammo icon bits) | `items` 128 / 256 / 512 / 1024 | 7-10 | 128 / 256 / 512 / 1024 |
| axe | `items` 2048 | 11 | 2048 |
| armour 1 / 2 / 3 | `items2` 1 / 2 / 4 | 23 / 24 / 25 | 8388608 / 16777216 / 33554432 |
| lava nails (ammo icon bit) | `items2` 8 | 26 | 67108864 |
| plasma ammo (ammo icon bit) | `items2` 16 | 27 | 134217728 |
| multi rockets (ammo icon bit) | `items2` 32 | 28 | 268435456 |
| power shield | `items2` 64 | 29 | 536870912 |
| anti-grav belt | `items2` 128 | 30 | 1073741824 |
| megahealth | `items2` 256 | 31 | 2147483648 |

- [read] Keys, invisibility, invulnerability, biosuit and quad keep the base bits 17-22.
- [derived] The base weapons keep bits 0-6, but the base ammo, axe, armour and megahealth bits
  all moved; that is why the bar has its own tests for armour and the ammo icon in this pack.
- [derived] Collisions: the program's grappling hook is `items` 8388608 = bit 23, the same bit
  as armour 1, so a grapple owner without better armour shows the green armour icon. `items2`
  values from 512 up (the pack's earthquake 512 and vengeance sphere 1024) shift past bit 31
  and are lost.

## 5. Dissolution of Eternity: weapons, inventory bar and ammo

Pictures loaded with the switch [read], sizes [measured]: R_INVBAR1, R_INVBAR2 (320x24); R_LAVA,
R_SUPERLAVA, R_GREN, R_MULTIROCK (24x16); R_PLASMA (48x16); R_AMMOLAVA, R_AMMOMULTI,
R_AMMOPLASMA (24x24). No lit or flash variants of the pack weapons exist or are loaded.

Inventory bar picture [read]: R_INVBAR1 when the active-weapon stat is 4096 or more (any of the
five pack weapons; [derived] also the grappling hook, 2^23), otherwise R_INVBAR2. The base IBAR
is never drawn in this pack. [measured] Both are the base bar with a second ammo icon added in
each counter's box (a red nail pack, a red rocket pack and a blue plasma orb beside the base
nails, rockets and cells). R_INVBAR1 shows the pack ammo icons bright and the base ones dim,
R_INVBAR2 the reverse. Neither has the base bar's sigil marks.

Weapons [read]: the base weapon pass runs unchanged (base pictures, base flash, slots x 0, 24,
48, 72, 96, 120, 144 at ibar y 8). Then, only while the active weapon is a pack weapon, its
picture is drawn over the base weapon's slot:
- lava nailgun (4096): R_LAVA at x 48 (the nailgun's slot),
- lava super nailgun (8192): R_SUPERLAVA at x 72,
- multi grenade launcher (16384): R_GREN at x 96,
- multi rocket launcher (32768): R_MULTIROCK at x 120,
- plasma gun (65536): R_PLASMA at x 144 (48 wide, the lightning gun's slot).
- [derived] Owning a pack weapon while it is not active shows nothing of it: the slot shows the
  base weapon's state (or nothing if the base weapon is not owned). The pack picture is drawn
  whether or not the base weapon is owned, and has no flash. [measured] The pack pictures are
  red, carry the key number (4-8), and look "lit".

Ammo icon on the main bar (x 224, y 0) [read]: the first owned of, in this order: shells (128)
SB_SHELLS, nails (256) SB_NAILS, rockets (512) SB_ROCKET, cells (1024) SB_CELLS, lava nails
(bit 26) R_AMMOLAVA, plasma ammo (bit 27) R_AMMOMULTI, multi rockets (bit 28) R_AMMOPLASMA.
Note the crossing: plasma ammo shows the picture named R_AMMOMULTI and multi rockets the one
named R_AMMOPLASMA. [measured] By eye, R_AMMOMULTI shows a dark box with four blue lights (looks
like plasma ammo) and R_AMMOPLASMA a red cylinder (looks like a rocket), so the crossing in the
client matches the pictures' content; the wad names are what is swapped.

Ammo counters [read]: unchanged in the client: four 3-digit counters on the inventory bar's top
row (bar y -24, ibar y 0), from the four ammo stats, at x 48k+10, 48k+18, 48k+26 for counter
k = 0..3 (shells, nails, rockets, cells), drawn with the gold digit characters (codes 18-27),
blank positions skipped. [read in the re-release QC] The program keeps separate stores and
copies into the four sent ammo fields: when the weapon value is 4096 or more, shells, lava
nails, multi rockets, plasma; otherwise shells, nails, rockets, cells. So the second, third and
fourth counters switch family together with the R_INVBAR1/R_INVBAR2 swap; shells never change.
[read] The developer `give` command in this pack follows the same rule: `give n`, `give r`,
`give c` set the normal stores and the shown counter only while the weapon value is 64 or less,
`give l`, `give m`, `give p` set lava nails, multi rockets, plasma and the shown counter only
while it is above 64.

The big ammo number (bar x 248, y 0, red below 11) is the current-ammo stat as in the base game.

## 6. Dissolution of Eternity: items

- [read] Only R_SHIELD1 and R_AGRAV1 are loaded. R_SHIELD2-5 and R_AGRAV2-5 are in the wad
  [measured, 16x16 each] but never loaded or drawn by this client. [measured] By eye, 1 and 5
  are the dull icon and 2, 3, 4 progressively less bright highlighted versions: a pick-up blink
  set like the base SBA1-5 frames, unused.
- [read] Power shield (bit 29): R_SHIELD1 at x 288; anti-grav belt (bit 30): R_AGRAV1 at x 304,
  both at bar y -16 (ibar y 8). Always drawn while owned (no blink; the 2-second redraw only).
- [read] The four sigils are not drawn at all in this pack (the sigil pass is replaced by the
  two items). Keys stay on the inventory bar at x 192 and 208 (base places; the pack's keys are
  16x16 [measured]); invisibility 224, invulnerability 240, biosuit 256, quad 272.
- [read] Armour icon at bar (0, 0) tests armour 3, then 2, then 1 at bits 25, 24, 23; the armour
  number (x 24, red at 25 or below) and the invulnerability "666" with the disc are unchanged.

## 7. Dissolution of Eternity: the team border

- [read] Drawn in place of the face, at bar (112, 0), when all hold: the pack switch is on, the
  game is not single-player (maximum clients other than 1), and the client's own `teamplay`
  setting is 4, 5 or 6. The pack's menu names these "Capture the Flag", "One Flag CTF" and
  "Three Team CTF" (1 "No Friendly Fire", 2 "Friendly Fire", 3 "Tag", anything else "Off").
  [derived] The value read is the local console variable on the machine drawing the bar.
- [read] When it applies, no face is drawn at all (also no quad, invisibility or
  invulnerability face). It is only reached on the normal bar, not when the scoreboard bar
  replaces it (scores key held or health 0 or below).
- [measured] R_TEAMBORD is 24x24: a frame with a solid light grey interior at picture x 2-21,
  y 3-20.
- [read] Colours come from the viewing player's own scoreboard entry: shirt colour c (high
  nibble of the player's colours) fills palette index 16c + 8; pants colour p (low nibble)
  fills 16p + 8. Two filled rectangles, 22 px wide, starting at bar x 113: shirt at bar y 3-11
  (9 rows), pants at bar y 12-20 (9 rows). [derived] They cover the frame's interior rows
  exactly and overlap its inner edge by 1 px left and right.
- [read] On top: the player's own frag count, as a right-aligned 3-character field, at
  bar x 113, 120, 127 (7 px apart, so 8-px glyphs overlap by 1 px), bar y 3. If the shirt
  colour is 0 (fill index 8), the digits use the gold digit characters (codes 18-27) and blank
  positions are skipped; [derived] a minus sign then comes out as character code 15. Otherwise
  the normal white characters are used, blanks drawn as spaces.
- [read] No team score and nothing else is drawn on it. The health number beside it is drawn
  as usual.

## 8. Overlays, frag list, intermission, finale, scoreboard

- [read] No pack branch in any of them: the deathmatch overlay (ranking picture and rows), the
  mini frag list to the right of the bar on wide screens, the frag row on the inventory bar in
  multiplayer, the single-player scoreboard text on the score bar, the intermission and the
  finale are identical with either switch.
- [measured] Neither pack's pak0.pak carries ranking, complete, inter or finale pictures (their
  only gfx files are gfx.wad and the console background), so those come from the base game.
- [derived] They differ only through data: the pack's gfx.wad supplies SCOREBAR, the number
  and character pictures.

## 9. Anything else for either pack

- [read] Start-up: the pack switch raises the minimum memory by 0x100000 bytes (1 MiB) over the
  base game's minimum.
- [read] Developer `give` with Scourge of Armagon: `give 6` gives the grenade launcher,
  `give 6a` the proximity gun, `give 9` the laser cannon, `give 0` Mjolnir, and `give 2`, 3, 4, 5, 7, 8
  the base weapons as in the base game. With Dissolution of Eternity: `give s` sets shells in both stores, and the
  split nails/lava-nails, rockets/multi-rockets, cells/plasma rule of question 5.
- [read] Single-player menu: each pack has its own episode and level list for the menu's
  "new game"; Dissolution of Eternity's multiplayer setup offers teamplay 0-6 with the names in
  question 7 (base: 0-2).
- [read] No pack branch in the view, the crosshair, the centre print, the console or the
  screen layout.
- [read] The QuakeWorld client in the same release has no pack support.
