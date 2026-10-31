---
name: held-weapons
description: "Use to make or change a first-person held weapon in gameengine2 (Shooter's Edge's view models, games/shooters_edge/content/models/view): a Blender builder over the shared weapon kit (weapon_kit.py: bevelled parts, procedural materials, the bake to one texture and a glow map, the shelf packer), the one pair of hands and jointed arms every weapon shares (shoulder, elbow and wrist by two-bone IK, the se_view data set, kBuiltScale, kShoulderM), the weapon's `fire` clip and how the hand plays it, staging a candidate beside the game's model (the se-weapon-stage lab, scripts/stage.sh: the stage, bench, wire, skin, the tip on the screen, GIF and WebM), and putting a built weapon in the game (ge asset build, the look's offset, CREDITS.md, models.tsv). Load assets and labs beside it."
---

# Held weapons

A held weapon is a BUILT asset: a Blender Python builder makes it at its true size from shared parts
and the game's numbers, bakes every material into one texture, and `ge asset build` commits the
`.glb` beside a stamp. The game draws it in first person with the hands holding it and plays its
`fire` clip on every shot. Nothing is hand-exported, and no picture comes from anyone else.

| File | What |
| --- | --- |
| `games/shooters_edge/content/models/view/weapon_kit.py` | The kit: parts, materials, hands and arms, IK, clips, the unwrap, packer and bake, the export |
| `games/shooters_edge/content/models/view/<name>/` | One weapon: `asset.txt`, `build.py`, the `.glb`, its stamp (`axe/` is the worked example) |
| `games/shooters_edge/assets/view_data.cpp` | The `se_view` data set: the built scale, the shoulders, the arm, every weapon's offset |
| `games/shooters_edge/view/weapons.hpp/.cpp` | The set's looks (offset, kick, refire, flash), `kBuiltScale`, `kShoulderM`, the arm, the loader and the mount |
| `games/shooters_edge/view/hand.hpp/.cpp` | Switch, kick, flash, and the `fire` clip played on a shot (`HandFrame::frame_a/b/t`) |
| `engine/viewmodel/arm.hpp` | The arm's anatomy: `arm_angles`, `arm_faults`, `kHumanArm`, `arm_poses` off a built model's nodes |
| `engine/viewmodel/held.hpp` | `HeldModel`, `held_with_clips`, `held_clip_frames`, `fit_mount` (`grip_at_origin`, `measured_on`) |
| `engine/assets/gltf.hpp` | `LoadOptions::keep_textures`: the texture and the emissive map kept whole as the skin and its glow |
| `games/shooters_edge/labs/se_weapon_stage_lab.cpp` | The staging lab |
| `.agents/skills/held-weapons/scripts/stage.sh` | The loop: a builder into a scratch `.glb`, staged, the numbers printed |

## The loop

```sh
.agents/skills/held-weapons/scripts/stage.sh shotgun                       # the tree's builder
.agents/skills/held-weapons/scripts/stage.sh shotgun --builder=/tmp/x.py   # a draft elsewhere
.agents/skills/held-weapons/scripts/stage.sh minigun --slot=nailgun        # into another slot
```

It builds the `assets` and `labs` executables, prints the `se_view` data set into
`build/<preset>/stage/`, runs the builder headless (Blender from `GE_BLENDER`, the PATH or the macOS
app) into `build/<preset>/stage/<name>.glb` without touching the tree or the stamp, and runs
`se-weapon-stage --weapon=<slot> --candidate=<that glb>`. It prints, in order:

- **the kit's report**: the islands' fill of the texture (55 to 70 % is good), each wrist's bend at
  rest and at worst, any hand past its arm's reach, triangles, vertices, nodes and clips;
- **the staged table**: the game's model and the candidate, their size held (the weapon alone,
  without its hands), the share of the picture it actually covers (`viewmodel::held_coverage`, its
  pixels drawn alone; stated 0.04 to 0.4; the built set covers 0.05 to 0.13) and where the muzzle
  lands. A box's share is no measure for a long gun: its back passes behind the eye and its box
  blows up to the whole screen;
- **the tip on the screen** at six frames of each clip (0 left/top, 1 right/bottom): tune a swing
  by these numbers, then look;
- **the arms**: the ARMS table (each joint's angle at rest, least and most) and any joint out of
  its range, named with its clip and time;
- **the pictures**: `stage-` (first person by day, torch, lamps, fired in the dark), `bench-`
  (right, left, above, front three-quarters), `wire-`, `skin-` (the texture, glow outlined in
  cyan), `arm-` (the arms' bones from the side and above over every frame),
  `anim-<w>-candidate-<clip>` (a strip), and `<w>-candidate-<clip>.gif` and `.webm` to
  watch. Send the GIF or the WebM to the person directing the work: they judge motion, not strips.

Look at every picture its caption names. When it is right: put the builder in its folder, run
`GE_BLENDER=<blender> build/<preset>/ge asset build <name>`, and land it (below).

## The frame and the size

- **Blender's frame is the view's**: +X right, +Y away from the eye (the muzzle's way), +Z up,
  metres, the weapon at its TRUE size. The game's mount turns it muzzle-forward (`muzzle_negative`)
  and draws it at `kBuiltScale` (0.5) with its ORIGIN, the grip hand's hold, at the look's
  `offset_m`. The kit moves the right hand's grip centre to the origin itself. An upright melee
  weapon uses the same frame, its edge toward +Y.
- **The player sees the left side (-X) and the top.** Put the detail there (a side-saddle of
  shells, pins, a maker's rivet).
- **A built weapon's offset is where the hand is in the view**, half its true distance from the
  eye: 0.07 to 0.2 m forward, not the 0.3 to 0.45 m a toy-scale kit model needed. Too far and the
  kit prints a hand past its arm's reach. A long gun sits with its grip near and central, as a
  shouldered gun does.
- **`kit.size`** scales the weapon's parts (never the hands) as they are added: build at the
  numbers you know, then make it the size it should be (the axe: a war axe built 0.74 to hatchet
  length). A hand's `centre` and `grip_r` are given at the size the weapon ends up.

## Parts

All parts are bmesh solids with a bevel modifier limited by angle and its normals hardened, so flat
faces shade flat and the bevels catch the light. Every part names its material.

| Call | Makes |
| --- | --- |
| `box(name, size, loc, mat, bevel, rot, taper)` | A bevelled box; `taper` narrows its +Y end |
| `lathe(name, profile[(r, along)], loc, mat, axis)` | A profile turned round an axis: a barrel with its bore (a closed profile), a bead, a cap |
| `tube` / `cyl` | A hollow or solid cylinder along an axis |
| `prism(name, profile[(y, z)], width, mat, width_at=)` | A side profile pushed across X: guards, triggers, blades; `width_at(y, z)` thins a blade to its edge |
| `loft(name, rings[(y, cx, cz, rx, rz, power)], mat, rot=)` | Superellipse sections along Y: stocks, grips, pumps, hafts (`rot=(pi/2, 0, 0)` stands it up along Z) |
| `screw(name, loc, mat, r, axis)` | A head standing out of a face along `axis` (`"-X"` the left) |
| `cable(name, points, r, mat)` | A round tube swept through any points: a cable, a hose, a curved prong |

Cylinders want 24 to 32 sides; the hands and arms stay coarse (6 to 10).

## Materials: `surface(...)`

A Cycles node graph whose COLOUR the bake keeps: the engine's own light does the shading, so bake
the albedo with its wear and dirt and nothing else. The knobs: `mottle` (blotches), `streak` along an
axis (brushing), `grain=(dark, rings_per_m, axis)` (flat-sawn wood: rings round a pith outside the
part, so they cross it as long lines; spherical rings read as rope), `pattern=("diamond", lines_per_m,
depth)` (checkering, knurling), `stripes` (hazard bands), `zone=(axis, from, to, colour)` (a
polished edge), `wear=colour, wear_amount` (edges by the bevel shader against the normal, broken by
noise), `cavity` (ambient occlusion), `glow=True` (lights itself; the glow map).

- Base colours are LINEAR. Blued or forged steel below 0.1 reads black in the corridor: 0.12 to
  0.15 for forged steel, 0.05 to 0.07 for blued, with wear around 0.4.
- A glowing part's albedo is its glow colour (the engine draws a glowing texel at its own colour).

## The texture

One 1020 x 510 picture a weapon: the view-model atlas (`kMorphAtlasSize` 2048) packs skins with a
one-texel gutter (`pack_morph_atlas`), so two 1020-wide skins fit a row and four rows hold all
eight; at 1024 x 512 two built weapons with the free set's skins no longer fit, and the client
refuses to upload them ("their skins do not fit the atlas"). The kit unwraps by angle, turns each island to lie wider than tall and shelf-packs them at
the largest scale that fits (`_pack`), so texels stay square. Blender's own packer into the unit
square cannot do a 2:1 picture with rotation and left 40 % empty behind a long haft. Seams follow
the islands, and the bake's margin (8 px, extended) hides them. Expect 300 to 900 KiB a weapon;
`budget-kib` in `asset.txt` states the most.

## The hands and arms: one body for every weapon

- **The same hands everywhere.** One glove, cuff, sleeve and size, the left the right's mirror
  (built as the right at the mirror place, then mirrored). Never give a weapon its own hand. A
  builder only says where each holds: `kit.hand(side, centre, axis, knuckles, grip_r,
  index_curl, holds, pole, turn_range, grip)`.
  - **The hand settles itself.** At `finish` the kit tries every turn about the grip's axis
    within `turn_range` of `knuckles`, and every obliquity its `grip` allows, and keeps the hold
    whose arm is most natural from its shoulder; then it builds the hand there. `knuckles` is a
    starting guess, not the answer. The kit prints the hold it chose.
  - **`grip="power"`** for a handle, a haft or a pistol grip: it crosses the palm at about 70
    degrees to the forearm (the wrist toward the little finger's end). **`grip="cradle"`** for a
    fore-end or a pump: it lies along the palm at 30 to 60. A right-angle crossing forces the wrist
    toward the little finger by 25 degrees or more on every arm.
  - `axis` runs along the grip toward the index's end.
  - `knuckles` is the side the knuckles are on. A right hand on a pistol grip has them on the
    right (+X); a left hand under a fore-end has them bottom-right (0.7, 0, -0.7), with the
    fingers up the right side and the thumb along the left.
  - `index_curl` below 1 hooks the index onto a trigger (about 0.5).
  - `holds` is the part it rides (a pump).
- **The arm** is three rigid nodes, `hand_<s>_upper` under a `torso`, `_fore` under it, and
  `hand_<s>` under that. They join at the SHOULDER (fixed from the eye: `kShoulderM`), the ELBOW
  and the WRIST, each with a rounded cap.
- **The joints keep to a person's ranges** (`engine/viewmodel/arm.hpp`, `kHumanArm`, the one
  authority):
  - the elbow is a hinge, 0 to 145 degrees, and points DOWN AND OUT from the line between the
    shoulder and the wrist (its roll: 0 straight down, 90 straight out, -30 to 100 allowed), never
    up and never in across the body;
  - the upper arm rises at most 25 degrees above the shoulder and swings from 45 across the chest
    to 135 out and back;
  - the wrist extends 70 and flexes 75, and deviates 20 toward the thumb and 35 away;
  - the forearm twists 95 either way from thumb-up.

  Every frame of every clip the kit's IK tries the elbow's whole circle and keeps the place inside
  the ranges with the most natural arm: the wrist near straight, the elbow about 35 degrees out
  from straight down, the forearm not wrung, the upper arm not raised. A small pull toward the
  last frame's choice keeps a clip from jumping. The ranges reach the builders through the
  `se_view` data set, and the kit's angle code repeats `arm_angles` to search with.
- **Read the arm** in the kit's rest line ("elbow 60 deg bent pointing 35 (0 down, 90 out), wrist
  ..."), in the lab's ARMS table (each joint at rest, its least and most over the clips) and its
  `arm-<w>` sheet (the bones from the side and above over every frame; every elbow below its line),
  and in `shooters_edge_view_test`, which fails any frame outside a range. A fault means the hold
  or the grip's offset is wrong: move them, never widen a range.
- **A long gun is held bladed.** `kit.stance = 40` turns the body at rest (the left shoulder
  forward) so the support hand reaches the fore-end, and the gun sits near the middle and low
  (a shouldered gun points from the right shoulder to below the eye): the shotgun's offset is
  (0.035, -0.13, 0.12). Held high or to the right, the support arm crosses the body, its elbow
  goes to its inner limit and its forearm wrings past 95.
- **The shoulders never move unless the body does.** A clip moves them only through the `torso`
  group (an axe's swing turns the body into the blow). Parent the weapon to the torso
  (`kit.groups["body"]["parent"] = "torso"`) when the body should carry it.
- The nodes are named `<name>.hand...`, and the game leaves them out of the box it measures the
  weapon by (`load_weapon_model`'s `weapon_only`), so an arm reaching to the eye never shrinks the
  weapon or fails the share.

## Clips

`kit.clip("fire", {group: [(t_s, (dx, dy, dz), (rx, ry, rz)), ...]})`: each group's offset from
rest and its Euler turn about its pivot, smoothstepped between keys and keyed every frame at
60 a second, linear, so the export is exact. Every clip starts and ends at rest, and its length
is at most the weapon's refire. The game bakes clips at `kClipFps` (30) into held frames and the
hand plays `fire` from its start on each shot.

- **Moving parts** go inside `with kit.group("pump", pivot=..., parent="body"):`. A hand that
  `holds` the part rides it, and its arm follows by IK.
- **A swing is led by the arm.** Pivot the weapon at the shoulder (`pivot="shoulder_r"`) and turn
  it there, so arm and weapon move together and the wrist keeps its hold. Moving the weapon on
  its own makes the arm chase it, bends the wrist and runs past the arm's reach.
- **Small numbers.** About the shoulder, 0.2 rad is a big move on screen; the axe's strike is -0.21
  down and 0.25 across. Aim the strike at the tip table: low in front, just off the centre (the
  axe lands at 0.29, 0.58).
- The placement's kick (`WeaponLook::kick`) already throws the weapon back and up; a fire clip adds
  the mechanism (a pump racked, barrels turning, a drum, a hammer).

## Putting it in the game

1. `ge asset build <name>` (the manifest names `data: se_view` and `source: ../weapon_kit.py`).
2. In `weapons.cpp`, set the look's `name` (the folder) and its `offset_m` (where the grip hand is
   in the view). `length_m` matters only for a model dropped in.
3. Delete the free model it replaces (`git rm content/models/view/<old>.glb`) and its row in
   `CREDITS.md`, and add the built one to the `ge/built` table. Update `models.tsv` (`replaced`,
   `ge/built`). `ge credits check games/shooters_edge/content` must pass.
4. Run `shooters_edge_view_test`, `make lab LAB=se-viewmodel` and the staging lab, then rewrite
   the digests the change moved (`ge digest <id> --write`).
5. Record the weapon in `docs/goals/M48e.md`'s table, with its wrist bends and its strike's tip.

## Where it goes wrong

- **Every built weapon goes stale together.** The kit and the `se_view` data set (every weapon's
  offset, the shoulders, the limits) are sources of each model, so changing either, even one
  weapon's offset, rebuilds all. `ge asset build` with no names builds exactly the stale ones.
- **A renamed slot** (the nailgun's slot holding the minigun): rename the look in `weapons.cpp`,
  and the row in `classic.assetmap` and `models.tsv`. Until the built model is in, the staging
  lab stages the candidate alone.
- **A weapon pointing straight away from the eye hides its length.** Its whole forward half falls
  into a sliver beside the receiver. Turn it a few degrees in with `finish(turn=Matrix.Rotation(
  0.05..0.09, 4, "Z"))` (the railgun, the grenade launcher, the rocket launcher) so its left side
  opens to the eye; the hands settle after the turn.
- **Nothing tall right by the eye.** A stock bar, a pad or a motor level with the receiver's top
  and behind the grip fills the picture. Keep what is behind the grip low and dark, and put a
  big motor ahead of the grip (the minigun).
- **A spin is linear.** A part that turns on every shot (barrels) is keyed with
  `kit.clip(..., ease="linear")` through a turn that leaves it looking as at rest (60 degrees for
  six barrels), so a held trigger spins it with no seam.

- **A part missing after the split**: a bpy edit operation acts on the selection the vertices and
  edges carry, not only the faces. Deselect every vertex and edge before selecting a group's
  faces (the kit's `_split` does).
- **Blender ignores PYTHONPATH** unless started with `--python-use-system-env`. A draft outside
  the tree finds `weapon_kit` only because `stage.sh` puts the kit's folder on the path.
- **Blender 5's actions are slotted**: F-curves live in `action.layers[].strips[].channelbags`
  (`_fcurves`). Each clip is pushed down as an NLA track of its name on every node, and the glTF
  exporter's `NLA_TRACKS` mode merges a name across nodes into one animation.
- **An upright model turned round**: the mount's frame and the builder's must agree. Every built
  weapon uses the same frame (above); a melee weapon built in another turns it with `finish(turn=)`.
