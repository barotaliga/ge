# Porting a sector-based sprite shooter

For whoever brings the first **sector-based sprite shooter** into Shooter's Edge: the family of
early-1990s shooters whose maps are a flat plan of rooms with a floor and a ceiling height, and
whose monsters, items and held weapons are flat pictures. `docs/porting-games.md` says where each
piece of any classic game lives; this page is the research for this family, kept so the port can
start from it. It is the M61 row (`docs/goals/M61.md`). Nothing here is built yet.

**Its settings keep their own meanings.** This family's console names (its `crosshair`, gamma,
sound and mouse settings) are settled by a clean-room reader per name before any is registered,
then registered in the game's own layer as the same setting, its own meaning or its own spelling
(`docs/porting-games.md` § One name, one meaning). A name another game already has is never
reused for this family's meaning by accident: the window prints the clash and its test fails.

**The verdict: viable.** The maps convert into real 3D geometry and play both ways; sprite
figures and sprite weapons fit the engine's sprite pass and view model; the file formats are small
and publicly documented. The work is bigger than the sequel's port because the map and the move
are a different family, not a new version of one. The hard parts are named in § Risks.

## What the family is

| Piece | What the files hold |
| --- | --- |
| The archive | One lump directory (an `IWAD` for the game, `PWAD`s over it); a later lump of the same name wins; marker lumps bracket the sprites (`S_START`..`S_END`) and the flats (`F_START`..`F_END`) so a `PWAD` adds to those ranges instead of replacing them |
| A map | A marker lump and its lumps: `THINGS` (start spots, monsters, items), `LINEDEFS` (walls, with flags, a special and a tag), `SIDEDEFS` (a side's three textures and offsets, its sector), `VERTEXES`, `SECTORS` (floor and ceiling heights and flats, light level, a special and a tag), and the prebuilt `SEGS`, `SSECTORS`, `NODES`, `REJECT` and `BLOCKMAP`. Later games of the family add an extended binary form (a line's arguments, a thing's height) and a text form |
| Wall textures | Built at load from patches: `TEXTURE1`/`TEXTURE2` place `PNAMES`' patches on a canvas |
| Flats | 64 x 64 raw palette indices, for floors and ceilings |
| Pictures | The column-post format: per column, runs of opaque pixels; the gaps are transparent. Patches, sprites, the status bar and menus all use it |
| Sprites | Named by four letters, a frame letter and a rotation digit: `0` is one picture for every angle, `1`..`8` are eight views 45 degrees apart; one lump may serve two rotations mirrored (`A2A8`) |
| The palette | `PLAYPAL`, 14 palettes of 256 colours (the normal one, red tints for pain, gold for pickups, green for the radiation suit); `COLORMAP`, 34 maps of 256 indices (32 light levels, the invulnerability map, black) |
| Sounds | 8-bit unsigned mono with a small header (format, rate, count), mostly at 11025 Hz |
| Music | A compact MIDI form, played on an FM chip through an instrument bank lump, or as General MIDI |
| Demos | Input recordings: one command a tick per player, at 35 ticks a second, replayed by running the game again |

Every format above has a public specification; read those freely. The behaviour (what each line
special does, the monsters' thinking, the weapons' numbers, the move) comes from the original
program, which is GPL: it is read only by a reader subagent (`clean-room` skill), into notes under
`porting/` with no code in them. The first round of those notes is kept: the move, blocking,
auto-aim, every line and sector special with its speeds and waits, sight and sound, randomness,
demos, the lockstep network play, the drawing rules, the held weapon and every monster's size,
health and attacks, for the first game and its sequel.

## The maps: one file, two worlds

A sector map is 2.5D: no room over a room, every sector's floor and ceiling flat (slopes come in
later games), walls always vertical. That turns into 3D cleanly.

**The converter** (a plugin, `plugins/sector/`, so every game of the family shares it):

1. Each sector's outline is walked from its sides into closed loops, with holes. Broken sectors
   (an unclosed loop, a line with the same sector on both sides) are mended or reported with the
   line's index.
2. Each sector is cut into convex pieces. The floor and ceiling become polygons at their heights;
   a sky ceiling is marked as sky.
3. Each side becomes up to three walls: the **middle** on a one-sided line, the **upper** and
   **lower** where two sectors' heights differ, and a masked middle (a grate) on a two-sided line.
   The pegging flags decide where each texture starts.
4. The convex pieces become brushes: a slab under each floor and above each ceiling, and the void
   outside the one-sided lines sealed. The brushes feed the hulls the shooter's trace already
   uses (`plugins/shooter/brush_hull.hpp`), and the engine's light bake (`engine/bake`) lights them.
5. Every sector a special can move (a door, a lift, a crusher, a stair, a floor) becomes its own
   moving brush model: a door is the ceiling slab, a lift the floor slab, its sides wearing the
   walls' textures that the move uncovers.

**One sector machine drives both worlds.** The specials (doors, lifts, crushers, rising stairs,
moving floors and ceilings, teleporters, switches, light effects, scrolling walls) run in one
place that changes sector heights and light levels on the game's tick. The family's own move
reads those heights; the 3D world moves its brush models to them. A sector's light level and its
effects (flicker, glow, strobe) become light styles.

**Played as it was** (the family's game module): the original move on the 2D plan, in 16.16 fixed
point at 35 ticks a second. Actors are circles on the plan that block one another as if infinitely
tall in the first game; a step is up to 24 units; there is no jump and no looking up or down, and
shots find their height by auto-aim, the vertical slope to a target in the line of fire. The
`REJECT` table and the line of sight decide what a monster sees. Its random numbers come from the
game's own fixed table, which fits the determinism rule as it is (integers, no libm).

**Played under the first classic game's move**: the same map, its 3D brushes and the shooter
plugin's move, free aim, jumps and rocket jumps. Scale nearly matches (both players stand 56 units
tall and 32 wide), but the family runs far faster (about 290 units a second walking, 580 running and nearly 750 running diagonally, with no air control and no fall damage) and climbs 24-unit steps where the first classic
game climbs 18; the remix sets the step height from the map's family. Maps never built for jumping
may let a player out of bounds over a wall top or onto a sky ceiling; the converter can raise an
invisible clip over a sky-walled outdoor area.

**One conversion, a fit per move.** The geometry is converted once; what depends on the move
is a thin fitting pass over it, chosen by the move the match runs (`cr_physics` in remix):
- **The first classic game's move** traces three fixed hull sizes, so the brushes are compiled
  into those clip hulls (the map compiler already does). Its 18-unit step cannot climb the
  family's 24-unit stairs: the fit lays an invisible clip ramp up each such stair, or raises the
  step height for the map.
- **The sequel's move** traces any box against the brushes directly, so no hulls are compiled;
  its step is 18 units too, so the same stair fit applies, and its crouch needs no change.
- **The family's own move** uses the 2D plan and needs no fit.
- **Both 3D moves** get the clip brushes over sky-walled areas, and may get a scale on the map
  (the family runs much faster, so its rooms feel small at the classic moves' speeds); a scale
  is one number the fit applies to every coordinate, held per map family.
- **Items** are swapped by role whatever the move (`docs/porting-games.md` § Remix); the fit only
  places them, since the family's things have no height and stand on the floor below them.

**Map tricks to keep**: self-referencing sectors (deep water, invisible bridges), a missing texture
that floods a floor into view, two sky sectors whose shared upper wall is never drawn, and lines
that only block monsters or only the player. Each is a rule in the converter with a test map.

## Sprite monsters and sprite weapons

- **Figures**: a sprite family reads into `plugins/morph`'s sprite form with eight rotations and
  their mirrors, and the engine's sprite pass (`engine/render/sprites.hpp`) stands it `upright` and
  picks the rotation from the angle between the eye and the figure's heading. Under free aim a
  figure seen from far above looks flat; classic enhance can add a generated depth for its light,
  and a figure stays a picture by design.
- **Remix**: a sprite monster walks a first-classic-game map (it is a box and a picture, so the
  shooter's trace and bots take it), and a 3D monster walks a converted sector map. The family's
  monsters keep their own thinking in their own module; remix reaches them by role.
- **Sprite weapons**: the held weapon is a picture at the bottom of the screen that bobs and plays
  its frames; it goes in the view model's slot (`engine/viewmodel`) as a flat layer, so the player
  switches between it and a 3D weapon with one setting.
- **Classic enhance on sprites**: the trained upscaler that enhances textures takes the pictures
  too (the alpha comes from the column gaps); the height and normal maps it already makes let a
  sprite weapon and a sprite figure catch the scene's light and the muzzle flash; the palette's
  bright colours, which no light level of `COLORMAP` darkens, become the glow map. Each is one
  `ce_` switch, as every effect is.

- **3D figures and items in a sector map**: the converted map is real 3D, so another game's 3D
  models stand in it as they do in their own maps. To sit them in the family's look, the classic
  look's palette and banded-light pass covers the whole frame, models included; a switch can go
  further and bake a 3D model at load into eight rotations of pictures at the family's pixel
  size, so it is drawn as a sprite like the rest. What an item does comes from the game that
  owns the rules (§ Remix in `docs/porting-games.md`), never from how it is drawn.

## The look and the sound

- **The classic look**: light falls off with distance through `COLORMAP`'s levels, banded, and the
  palette tints for pain and pickups cover the screen. Both fit a post pass over the palette.
  The partial-invisibility smear is a screen-space shimmer behind the figure.
- **The enhanced look**: the baked light per sector, the engine's dynamic lights, shadows and
  sky. The sky texture is a band around the horizon that repeats four times, drawn as a panorama.
- **The pixels** were drawn for a 320 x 200 screen shown at 4:3, so each was 1.2 times taller than
  wide; drawing square changes nothing in play, and the classic look can stretch it back.
- **Sounds** are sampled voices; classic enhance's sound stage takes them to 48 kHz.
- **Music** needs a MIDI sequencer and an FM voice built on the engine's synth (`engine/audio`),
  playing the instrument bank lump's patches, so any game can play MIDI. The well-known FM chip
  emulators are under GPL or LGPL, so ours is written here.

## Where each piece lives

| Layer | The family's pieces |
| --- | --- |
| Engine | The MIDI sequencer and FM voice (`engine/audio`); the lump directory and its marker ranges (`engine/vfs`, beside its wads) |
| Shooter plugin | Moving brush models already; bots on the converted map; the protocol, server, demos' shared machinery |
| `plugins/sector/` (new) | The map reader (binary, extended and text forms), the converter to 3D, the sector machine (specials and light effects), the 2D move and its blockmap |
| `plugins/morph` | The column-post pictures, the sprite families with rotations, the textures built from patches |
| `plugins/classicenhance` | A content row for each game of the family; the upscaler, depth and glow on sprites |
| Game module | Each game's rules, monsters, weapons' numbers, specials' table, animated textures and switch pairs by name, menus, its demo format |

## Risks

| Risk | Weight | What answers it |
| --- | --- | --- |
| Demos playing back as recorded need every rule exact to the bit, bugs included | High | Choose the parity goal first: the spirit (fun, close) first, exact only if old demos must play. The fixed-point sim makes exact possible later |
| Converting real maps: broken sectors and tricks | Medium | The converter's rules, a lab that converts every map of the player's own files and reports each failure by line |
| Moving sectors as brush models where several sectors share a move or a special changes a sector's floor texture | Medium | The sector machine owns the heights; brush models follow; a texture change swaps the slab's material |
| Maps played with jumps and free aim break their intended routes | Medium | Clip brushes over sky areas; accepted as remix |
| Music without an FM chip emulator | Medium | Our own FM voice; General MIDI as the fallback |
| Licensing of the data | Low | The player brings the game's files; the free shareware file may sit in the project folder for labs, never in the tree |
| The original's source is GPL | Known | The clean room, as for the first two classic games |

## The plan, if the port is approved

1. **Notes**: the reader subagent's answers on the specials, the move, the monsters and the
   weapons, kept in `porting/`.
2. **The map in 3D**: the reader and converter, a lab that draws a real map of the player's files
   from inside, walked with the first classic game's move.
3. **The sector machine**: doors, lifts, stairs and crushers moving in the 3D world.
4. **Sprites**: figures with rotations, items, the sprite weapon in the view model, in remix too.
5. **The family's module**: its move, auto-aim, monsters and weapons at 35 ticks.
6. **Sound and music**, then classic enhance on all of it.
