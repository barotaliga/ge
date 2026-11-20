---
name: map-design
description: "Use to design, build or improve a multiplayer map for Shooter's Edge (or any game on the classic brush maps): the game's movement numbers (jump, speed jump, head jump, rocket and grenade jumps, team boosts) measured by the map-moves lab, layouts for duel, 2on2, 4on4, FFA and midair, places meant to be camped, held, fought over or run through, trick routes at each skill level, a plan's `modes`, `area` and `trick` statements, and the loop that holds a map to its intent with bots (the map-intent lab) until it plays as meant."
---

# Designing a map

A map is written as a Python script over the map SDK (`tools/mapsdk`, `docs/levels.md` § The map
SDK) or as a plan (`plugins/brush/compile/plan.hpp`; the `brush` skill owns the compiler), and
judged by bots and by the move, never by eye alone. This skill is how to decide
what goes where and how to check it did what it was for. Every number below is measured on our
own move and rules; re-measure after a physics or rules change (`labs map-moves`).

## 0. Study proven maps first

Before a new map, look at the maps people have played for decades and still pick: they are
balanced, they flow for speed jumps and tricks, and their camps are known. Quake's own
`dm2`, `dm3`, `dm4`, `dm6` and `e1m2` (in the player's packs), and the community's `aerowalk`,
`ztndm3`, `schloss`, `cmt3` (`cmt3b`), `cmt4` and `cmt5`. The community's maps are kept in the
project's shared folder (`maps/reference/`), never in the tree.

For the map nearest to the one you are making (a duel map: `aerowalk`, `ztndm3`, `dm6`; a team
map: `dm2`, `dm3`, `e1m2`, `schloss`, the `cmt` maps):

1. `labs map-overview --game=<folder> --map=<name>` draws it from above with its items and the
   jumps the jump finder calculates on it (§ 5); `scripts/overviews.sh` draws them all at once.
   `--intent=auto` drafts its `.intent` from its items (`plugins/shooter/intent_draft.hpp`: an
   area a room round each armour, power and key weapon) and attaches it as `<map>.intent`. Edit
   that draft in your scratchpad (the middle's fights, the routes, the boxes a wide room spills
   out of) and draw it again: naming its camps, routes and fights is the study. The drafts of
   the eleven maps are in the shared folder's `maps/reference/intent/`.
   `labs map-intent --game=<folder> --map=<name> --intent=<file or auto>` then plays bots on it
   and tables each area against its intent, so you see how the analytics read a map people
   already know.
2. Measure it against the table in §1: its gaps, ledge heights, run times between the big items,
   the ways into each camp. Copy the measures, never the shapes: our map is our own plan.
3. Say in the plan's header comment which maps you studied and what you took from each (a
   loop's length, how a camp is broken), so the next agent can check it.

## 1. Know the game you build for

Each game has its own move and rules. Build for one; a map for both is two plans.

**Quake** (the classic rules in `games/shooters_edge/rules`, the move in
`plugins/shooter/pmove.cpp`):

| What | Number | Where it comes from |
| --- | --- | --- |
| Player box | 32 × 32 × 56, origin 24 above the floor, eyes 22 above the origin | `plugins/shooter/brush_hull.hpp` |
| Step up without jumping | 18 | the move's step |
| Run speed | 320 units/s; gravity 800; jump 270 up | `MoveSettings`, `pmove.cpp` |
| Push from a hit | damage × 8 units/s, away from the blow | `rules/common_combat.cpp` `blow_push` |
| Rocket blast | 120 at the centre, less half the distance; its own shooter takes half; a teammate all of the push and no harm | `radius_damage`, `damage` |
| Respawn | armour, health, mega 20 s; weapons and ammo 30 s; quad 60 s; pentagram and ring 300 s | `rules/common_items.cpp` |
| Item boxes | weapons, armour, health, ammo from the origin up; a power from 24 below to 32 above | `item_start`, `power_start` |

**What a player reaches** (the `map-moves` lab; units; health spent in brackets). Re-run it and
copy the table here when the move or the rules change:

| Way | Highest ledge | Widest pit |
| --- | --- | --- |
| Jump, standing | 0 (a step only) | 0 |
| Jump, running 320 | 0 | 64 |
| Speed jump at 480 / 640 | 0 | 256 / 352 |
| Head jump (on a teammate), running | 64 | – |
| Rocket jump, at the feet | 256 (52) | 608 (52) |
| Rocket jump, the flip (behind) | 128 (46) | 672 (46) |
| Grenade jump | 144 (46) | 224 (44) |
| Team rocket boost | 512 (0) | 1024 (0) |
| Team double boost | 0 | 448 (0) |

The plain jump's ledges read 0 because the lab's ledges step by 32 and a jump climbs about 40:
a ledge up to 40 is a jump for anyone (the map-intent lab proves one where you need it).

Design by these rows, with a margin: a ledge meant for everyone is at most the standing jump less
8; a trick meant for experts sits within 85 % of its way's best (a jump at the very edge of the
table is made one time in ten, and a bot's proof is kinder than a hand).

**Quake II** has its own move (crouch, a higher step, a different blast); its numbers come when
the version 38 writer lands. Never reuse Quake's numbers for it.

## 2. Decide what the map is for

Write `modes` first. Each mode wants a different shape:

- **Duel** (1 on 1, `deathmatch 3`): small (a run across in 6 to 8 s), one strong armour and the
  mega far apart so neither player can hold both, the rocket launcher between them, every room
  with two ways out (a dead end is a death), sound cues (water, a lift) on the way to the big
  items so a listener can time his opponent. Item control decides a duel: lay the armours and the
  mega so a full loop takes about the 20 s they respawn in.
- **2on2**: a duel map grown a room: each team can split (one on armour, one on the launcher) and
  still help in 3 to 4 s. Two strong items each side of the middle.
- **4on4** (`deathmatch 1`, teamplay): two bases a team can hold (`hold`), one or two `camp`
  spots with one or two ways in and a window over an item, the quad in the open middle with
  several ways in (it is rushed, never held), the red armour where a held base can watch it,
  long corridors that cost time to cross (a held map must be taken back through a choke), water or
  a back way into each base so it can be broken without the front door.
- **FFA**: many spawns (12 or more, none in sight of another), no camp spots (a spot with one way
  in becomes a farm), loops everywhere, items spread so nobody stacks; weapons near spawns so a
  new player can fight back.
- **Midair** (rockets only, a hit counts by height): one tall open hall (512 or more up), a
  floor with few things to hide behind, platforms at several heights for a rocket jump to start
  from, no low ceiling anywhere a player flies. Score bands are by height: 256, 512, 1024.

## 3. Lay out intent, then shapes

Before any `room`, write what each place is for as an `area` with an `intent`, and each meant
way as a `trick`:

```
modes duel,2on2
area ra z=0..160 box 64,64 256,256 intent=camp note="one door and a window"
area mid z=-32..256 box 640,96 896,672 intent=fight
area run z=0..128 box 384,320 640,448 intent=route
trick ra-window from 300,200,24 to 180,120,104 kind=rj level=1 note="the way in for a rocket"
trick mega-gap from 400,640,24 to 400,900,24 kind=hop level=2
```

Intents: `camp` (one player holds it: few ways in, a view over what he guards), `hold` (a team's
base), `fight` (the sides meet), `route` (a way through: a corridor, stairs), `power` (a power's
spot), `perch` (high ground that sees a fight below), `spawn`. Tricks: `jump`, `hop` (a speed
jump at 480 to 640), `head` (off a teammate's head: early, before anyone has a launcher),
`rj`, `flip` (the rocket jump shot behind, the furthest), `gj`, `boost-rocket` (a teammate's
rocket at the feet), `boost-double` (a teammate's double barrel in the back). Give every trick a
`level`: 0 anyone, 1 practised, 2 expert. Then design the shapes to serve the intents:

- **A camp** has one or two ways in that a camper can watch from one spot, cover from the rest,
  and a way to be broken (a trick in through a window, a grenade over a wall): a camp nobody can
  break kills the map.
- **A fight** has several ways in from both sides, cover (pillars at least 48 wide so a player
  hides), and height on one side worth taking.
- **A route** is narrow enough to read (96 to 160 wide) and long enough to cost time; a route
  between two held places is where a choke forms.
- **Hard but possible**: every strong place gets a trick in that only `level=2` players make, and
  every trick has a way back down. Put the early tricks (head jumps, boosts) where they pay off in
  the first 30 s of a match (a weapon on a ledge, a short way to the armour).
- **Heights**: ledges for everyone at 40 or under; doorways 64 wide and 96 high at least; ceilings
  in rooms where rockets fly 192 or more; steps 16.

### Writing it as a Python script (the map SDK)

A script is the better source when the map is made of pieces you will change, repeat or take
from another map; a plan is enough for a few carved rooms. The script runs in a moment, so edit,
run, look:

```python
import sys; sys.path.insert(0, "tools/mapsdk")
from mapsdk import Map
m = Map("Name", textures={"floor": "floor", "wall": "wall", "ceil": "ceil"})
m.modes("duel")
m.room((640, 0, 0), (1408, 768, 384))                     # air, closed by 16-unit slabs
m.pit((896, 256, -96), (1152, 512, 0), liquid="water")
w = Map(textures=dict(m.tex))                            # one wing, then it and its mirror
w.room((0, 0, 0), (512, 768, 320)); w.item("item_armor2", (96, 640, 152))
w.area("ledge", (0, 512, 128), (512, 768, 320), "camp")
m.merge(w).merge(w.copy().mirror("x", 1024))            # the copy's area is ledge-2
m.hall((512, 256, 0), (640, 512, 128), "x")              # ends on both rooms' inner faces
m.build("/tmp/x/name.map")                               # name.bsp and name.intent beside it
```

`tools/mapsdk/examples/crossfire.py` is a whole duel map. Take a proven map's piece with
`Map.import_bsp("<map>", "/tmp/x/<map>.map", root=..., game=...)` then `.cut(lo, hi)` (sealed, so
it builds alone) and `merge` it in. A `RuntimeError` from `build` is the compiler's answer: a leak
names the entity it started from and the last place it passed, so probe there. Then look and play
as below: `labs map-overview --game=<folder> --map=<path>.bsp --intent=<path>.intent`, and copy the
.bsp into the folder's `maps/` for `labs map-intent --game=<folder> --map=<name>
--intent=<path>.intent`.

## 4. Build, prove, play, read, change

1. `secli map build <plan> --textures=<game folder>` (or `labs map-build --plan=<plan>` for the
   pictures). Fix every warning.
2. `labs map-intent --plan=<plan> --minutes=10`: every trick flown on the move (green made, red
   not), a match of bots for each mode in `modes`, each area held to its intent (the table's last
   column says why not). For a 4on4, `labs se-team --basedir=<a folder with the map> --map=<name>`
   too: its "items no route leads to" line must say none, and every key item must be taken.
3. Read the picture: where the dots are thick, the players live; rings are deaths. A camp with
   thin dots is not a camp; a route with rings in it is a fight.
4. Change the plan, not the bots. Then run it again with the same seed, and keep the before and
   after in the PR.

Bots follow the map's own plan (`games/shooters_edge/bots/map_plan.hpp` works out what to hold
from the items): an area the bots never use may still be good for people, so say so beside it
(`note=`) rather than bend the map to the bots. Recordings of people on the map (`--demos=`) are
the better judge when there are any.

## 5. Report: the overview and the photos

Every report on a map (a PR, a progress message) carries two things, on one page (`ge lab
publish`):

- **The intended gameplay from above**: `labs map-overview --game=<folder> --map=<name>
  --intent=<plan or .intent>`. It works on any version 29 map, ours or not. The floors are drawn
  from the map's own textures and light, the areas washed in their intent's colour and numbered,
  the tricks as arrows (green for anyone, amber practised, red experts; dashed through the air),
  the items as pictures of the game's own models (the pack's; badges where it has none), the
  powers glowing with their respawn, the starts and the teleporters. The panel beside it lists
  the areas with their notes. **The jump finder** (`plugins/shooter/jump_finder.hpp`) draws,
  unasked, one likely rocket jump a ledge, CALCULATED from the map: the take-off (the run, the
  jump, a ramp's rise or a rising lift), the rocket's burst where its line meets the world, the
  blast's push, then the arc through the map with the air steering it, its real path drawn from
  above. It keeps the highest jump onto each ledge that no walk, ride or teleporter matches (up a
  stair is no trick), and marks in violet the ones a ramp or a lift carries past a standing jump's
  reach (badge `RAMP` or `LIFT`). `--from=x,y,z` lists every ledge a jump reaches from one spot.
- **Every map at once**: `.agents/skills/map-design/scripts/overviews.sh <game folder> <out dir>
  [map ...]` runs the overview on each map (every `.bsp` in the folder's `maps/` when none is
  named), several at a time, and writes `<map>.png`, `<map>.jumps.txt` and `<map>.views.txt`;
  a `<map>.intent` put in the out dir is drawn with it. Run it over the reference maps (§ 0)
  before comparing.
- **A photo of every area from inside**: the lab writes `views.txt` (a place and a look an area,
  or spots spread over the floors when there is no intent), and
  `.agents/skills/map-design/scripts/photos.sh <game folder> <map> previews/map-overview/views.txt
  <out dir>` photographs each in the game's own window. The photos use the player's own textures:
  they go on the report page, never in the tree.

## Rules

- **Every number from a lab.** A height, a gap or a time in a plan's comment or this skill comes
  from `map-moves`, `map-intent` or `se-team`, never from memory of another game.
- **One game a plan.** A plan says which game it is for in its header comment.
- **No one else's maps.** Our maps are our own plans; a texture is named, never copied (the
  `brush` skill).
