# The HUD: a layout the game sets and the player tunes

A HUD is the figures, bars, dials and the map drawn over the frame while a run is played. The
engine's HUD lives in `engine/hud` (`ge::hud`, `ge_hud`). It started (M10a) as a kit of
widgets that a game's view code places by hand. This page is the design for what it becomes:
**the HUD as data**. The game declares its elements and their default places in a table.
The player then moves, resizes, hides and re-skins any of them, with cvars, the console, a cfg
file, or by dragging them in an edit mode. The roadmap rows are M10d to M10h
(`docs/goals.md`). M10b draws all of it in the game's window.

Fall Line is the first user (a speedometer dial, a wind meter, the minimap, the wanted stars).
Shooter's Edge is the second, and it needs health, armour, ammunition, a mana-style resource,
a compass and a crosshair. The figures differ, but the machinery does not, so it all lives in
the engine and nothing in it names a game.

## The model

### Elements

An **element** is one thing on the HUD: the speedometer, the minimap, the health bar. A game
declares its elements once, at start-up, as a table (`ge::hud::Layout::add`, an `ElementSpec`
each; built in M10d, `engine/hud/layout.hpp`):

| Field | What it is |
| --- | --- |
| `id` | A short lower-case name, used in its cvars: `speed`, `map`, `health`. |
| `kind` | The widget that draws it: `readout`, `big`, `bar`, `pips`, `banner`, `feed` today; `dial`, `wind`, `compass`, `minimap` as M10f and M10g land. |
| `anchor`, `x`, `y`, `scale`, `alpha`, `show` | Its default place: one of the nine points of the safe area, an offset as a share of the safe area, a size and an opacity. |
| A kind's shape | A bar's length and ticks, the pips' symbol and size, the feed's column. |
| `under_corners`, `under` | The two clearance rules (below). |

Elements that share an anchor **stack** in table order, as the kit's `Stack` stacks boxes:
hiding one closes its gap, and an element's own offset moves it alone. Two rules keep the
centre clear. An element with `under_corners` on the top or bottom row's middle anchor drops past
its row's corner stacks when it would meet a box of either, so a narrow screen's air clock goes
under the corners' columns. An element `under` another stays at least a gap below that one's box
while it shows, so a prompt never sits on the air clock.

The kit's widgets (M10a) stay the drawing code. An element is a widget, a place and a shape.
Which elements show in which of the game's modes (on foot, on a machine, dead) is the game's
call, made every frame through the content below.

### Content: the game's values, written once a frame

The game writes each element's **content** every frame, by the index `add` returned: whether it
shows now, its words (value, caption, unit), its level (a bar's 0..1, or -1..1 centred), its pips
(count, lit, blinking off), its colour, and for a feed the feed and the clock. Nothing is looked
up by name in a frame. The HUD never reads the game's state. That split (a pure snapshot in,
pixels out) is what lets a lab, a test or a replay draw the HUD with no game running. Fall Line's
`fill_hud` writes its content from the M10a `HudSnapshot`. The instruments (M10f) and the
minimap (M10g) add the numbers their kinds read (a speed, a heading, a pose).

### The layout is cvars

Every element's place is a set of HUD cvars (`hud_` is the HUD's prefix, `docs/console.md`),
each bound to a field of the element (`console::Cvars::bind`, registered by `add_hud_cvars`). A
set writes the field directly, so the draw reads its own struct and never asks the console.

| Cvar | Meaning |
| --- | --- |
| `hud_<id>_show` | 0 hides it. |
| `hud_<id>_anchor` | `top_left` … `bottom_right`: which of the nine points it hangs from. A word that is none of the nine keeps the game's anchor. |
| `hud_<id>_x`, `hud_<id>_y` | Its offset from that point, as a share of the safe area's width and height (-1..1, y down), so a layout survives a change of resolution or aspect. |
| `hud_<id>_scale` | Its size, times the HUD's (0.25..4). |
| `hud_<id>_alpha` | Its opacity (0..1): every command it drew, faded. |
| `hud_<id>_skin` | Its skin (below; M10e). |
| `hud_<id>_…` | A kind's own settings as the kinds land: `hud_map_span` (metres across), `hud_map_shape` (`round`/`square`), `hud_speedo_max`, `hud_speedo_style` (`dial`/`number`). |
| `hud_scale`, `hud_alpha`, `hud_margin` | The whole HUD: its scale over the frame's, its opacity, and the margin (reference px) from the screen's safe edge. `hud_face` (its type) comes with the skins. |
| `hud_reset [<id>]` | A command: one element (or the whole HUD) back to the game's place. |
| `hud_edit` | 1 opens the edit mode (M10h). |

**Who sets what.** The game's defaults are its element table: they are the cvars' defaults, so
`reset` and `hud_reset` return to them. A game that wants a different layout for a mode (or a
shipped variant) runs its own lines with plain `set`, which never reaches the player's config.
The HUD's cvars carry no archive flag until a player's own `seta` gives one, so `cfg_save` keeps
exactly what the player set and a later `exec` restores it. `hud_reset` also drops the archive
flag, so a reset place is not saved. No HUD cvar is a server setting. A competitive server may
one day want to forbid one (a minimap in a mode that should not have it). That would be a later
row, using the console's server tier, and it is not planned here.

## Skins: the player's and the game's own textures

A **skin** is a directory of images, `skins/<name>/`, that replaces an element's drawn parts.
Each part is optional, and a missing part falls back to the built-in look:

| Part | Used by | How it is drawn |
| --- | --- | --- |
| `frame.png` | every kind | **Nine-slice.** The corners are kept at their size, the edges stretch one way and the middle both ways, so one small picture frames a box of any size. The border width is in `skin.cfg`. |
| `fill.png`, `empty.png` | `bar` | The full and empty parts of the bar, cut at the value. They are cut, not stretched, so a pattern keeps its grain. |
| `icon.png` | `readout`, `big`, `bar` | A picture beside the figure: a shell, a heart, a mana flask. |
| `pip_on.png`, `pip_off.png` | `pips` | The symbol, lit and unlit, replacing the charset's star or heart. |
| `face.png`, `needle.png` | `dial`, `wind`, `compass` | The instrument's face, and its needle turned about a pivot that `skin.cfg` names. |
| `plate.png`, `marks.png` | `minimap` | The minimap's rim, and a sheet of map symbols. |

`skin.cfg` holds the numbers a picture cannot carry: the nine-slice border, the needle's pivot,
the value's colour and face. Images are PNGs in the game's data directory, loaded once, and
drawn by the 2D layer as image boxes, tinted by the element's colour; a frame is one box in the
layer's nine-slice mode (M22e). The PNG decoder moves
from `engine/lab` into the runtime for this. Later, a skin part may also be a procedural
texture recipe (M31), baked once and cached like any other texture.

## The instruments

These are four new kinds that are **drawn** rather than printed. Each reads the numbers in its content and draws
itself. None of them reads a game.

- **`dial`: a speedometer, a tachometer or any needle gauge.** It has a range, an optional
  **knee** that squeezes the low end into the first part of the sweep, so the band the player
  spends the run in gets most of the dial, and an optional **red band**. It has ticks and
  figures on a ring, a needle, and the figure in digits in the face's lower half. The needle
  is eased toward its value in the view (critically damped, about 0.1 s), so a bumpy reading
  does not make it jitter. This is drawing only, and it never touches the simulation.
- **`wind`: which way the air moves, turned heading-up.** The top of the dial is the way the
  player faces. The bold arrow through the middle is the wind he **feels**: the air where he
  is, less his own velocity, so at speed in still air it points straight back at him. A small
  mark on the rim stands on the side the **weather's** wind blows from and points inward, so the
  two never cross. The figure under it is the felt wind. Past a gale it turns the alarm colour,
  and under a calm floor no arrow is drawn.
- **`compass`: a strip of headings across the top.** The current heading is centred and the
  cardinal points are marked. A game can add its own marks (an objective, a teammate).
- **`minimap`**: below.

Drawing these needs four primitives the 2D layer lacks today: **lines** of a width (with round
ends), **filled convex polygons** (a needle, an arrowhead, the minimap's chevron), **arcs**
(the dial's band, a ring's segment) and **rotated image quads** (a skinned needle, the map's
ground turned heading-up). Each is added to the CPU raster and the GPU pass together, with the
CPU as the reference the GPU must match within a stated error. They land in the 2D layer as
M22e: `line` and `polyline` (round caps), `arc` (a ring sector, an optional round end),
`polygon` (convex, up to 16 points), and a `rotation` about a `pivot` on any box, which turns a
skinned needle about its base and the minimap's ground about the player.

## The minimap

The minimap is the map around the player, **heading-up**. The plate turns about him so that
what is ahead is up, and a bend he sees going left on the snow goes left on the plate. It has
two halves.

- **The engine's (`ge::hud`, kind `minimap`)**: a plate (round or square, with its rim and a
  north mark that rides the rim) over a **ground image**. A **pose** places the image: the
  centre in world metres, the heading, and the span (metres across the plate). **Marks** are
  drawn over it: points, polylines and icons, each with a kind, a colour and an optional
  heading. It also holds the zoom rule. The span opens with speed, from a few hundred metres at
  a crawl to more flat out, because what the plate owes the player is warning time, not
  distance. The span follows the speed with a lag of about half a second, so a bump in the
  reading does not pump the zoom. Aloft, it opens with height above the ground. The minimap
  never paints the ground itself. It turns one picture.
- **The mountain's (`plugins/mountain`)**: the ground image, **baked once a map** off the
  frame's thread. It reuses the map picture's drawing (`plugins/mountain/picture`): the snow
  shaded by height and lit from the upper left, the contours, rock where the face is too steep
  for snow, the groomed runs, the woods with every tree, the village's streets and its
  buildings. It is baked at about a pixel every metre and a half, and capped at a size a
  phone decodes. The plugin also supplies the marks a ski area has: the runs coloured by grade,
  the lifts' lines, the helipad.
- **Fall Line's**: its marks, filled every frame. These are the player's chevron, the other
  skiers as dots, and, while he is hunted, the hunters and the area being searched.

The signs follow the engine's conventions (heading 0 is +z, clockwise from above). The turn
from world to plate is stated once in the minimap's pose and tested both ways: a mark ahead
draws above the centre, and a mark to the right draws to the right.

## The edit mode

`hud_edit 1` pauses the run, dims the frame and draws every visible element's box over it, each
labelled with its id:

- **Select** an element by pointing at it, or by stepping through them on a pad.
- **Move** it by dragging. On release, it re-anchors to the nearest of the nine points, so a
  bottom-right element stays bottom-right on a wider screen. It snaps to the safe area's
  edges, the centre lines and the edges of other elements.
- **Resize** it by dragging a corner handle (or pinching on touch, or the shoulder buttons on
  a pad). This writes its scale.
- **Hide** it, **reset** it, or step through its **skins** from an inspector that shows its
  cvars as they change.
- **Live check**: the layout check (`layout_problems`) runs as the player drags. An overlap or
  an element off the screen is outlined in red but not refused. The player owns his own
  layout.
- **Leave**: Escape (or the pad's back button) keeps the changes, and `hud_edit 0` from the
  console does the same.

Every change is an ordinary cvar set, so the console sees it, `cfg_save` keeps it, and the
same edit made by typing `hud_map_x 0.1` gives the same pixels. The edit mode is engine code
over the input layer's pointer, touch and pad events. A game only supplies a stand-in snapshot,
so that every element is visible while it is being edited.

## How it is held

- **Layout tests.** For each layout (the game's table, and cfgs that move, hide and scale
  elements), the test checks that only the named element moved and that the layout check
  passes at 1280x720, 390x844 and 844x390 (`tests/hud_layout_test.cpp`,
  `tests/fall_line_hud_test.cpp`).
- **The cvar round trip.** Edit, `cfg_save`, a fresh start, `exec`: the same boxes come back.
- **`hud-layout`**: Fall Line's HUD with every widget up in the game's places, after a player's
  cfg, and after `hud_reset`, at the three viewports. A problem in the game's layout, or a box the
  reset did not put back, fails it.
- **Skins.** A skin with every part draws from its pictures. A skin missing a part falls back
  to the built-in look for that part alone.
- **Instruments.** A dial at known values puts its needle at known angles. The wind arrow
  points back at a still-air skier. The minimap draws a mark ahead above the centre.
- **Labs.** `hud-layout`, `hud-skins`, `hud-instruments`, `minimap` and `hud-edit`. Each draws
  its pictures at the three viewports and digests them, and any layout problem in a game's
  default layout fails it.
