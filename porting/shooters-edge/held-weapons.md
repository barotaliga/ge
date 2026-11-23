# The held weapon's frames

What the classic network game's program and its modern client do with the first-person weapon's
frames. Clean-room facts from a reader (the `clean-room` skill), each marked `read` (from the
source, by the reader) or `measured` (a lab run here).

## The axe's two swings

- `read` The axe's view model has nine frames: 0 at rest, 1-4 one swing, 5-8 the other.
- `read` Each attack picks a swing with a fresh uniform draw, even odds, with no memory of the
  last: no alternation and no pattern. (The body has four attack sequences picked by quarters of
  the draw; two of them show the first swing in first person and two the second.) The common
  modern server mod picks the same way.
- `read` The swing's first frame is the player's `weaponframe` in the same think that moves
  `attack_finished` on by 0.5 s; then a frame every 0.1 s: the hit on the third step, rest
  (frame 0) at 0.4 s, the next swing allowed at 0.5 s.
- `read` The client draws the frame the server sends (the same in a recorded game). Only with the
  server's weapon-prediction extension does it pick a swing itself, from its own clock, which can
  differ from the server's pick. Each change of frame is blended over 0.1 s, the drop back to
  rest included.
- `measured` On e1m1 with the shareware pack's own program, 16 swings with attack held: each
  swing's `weaponframe` was 1 or 5 as it started, and the client drew that swing's four frames
  alone (`se-client --progs=pack --swings=16`).

Built: `ClassicFire::takes` (plugins/classicenhance/weapons.hpp) splits the axe's firing frames
into two `fire` takes; the hand (games/shooters_edge/view/hand.hpp) plays the take holding the
`weaponframe` the shot starts on, or picks one at random when the program sets none.

## The blend between frames, the nailguns and the flash

From a second reader round (the modern client, its master branch as of 2026-11-22) and the
shareware pack's view models measured by the reader. Ours: `view/hand.cpp` (the listen game),
`client/held.cpp` (a joined server, a recording, Quake II's hand), `viewmodel/held.cpp`'s
`pose_frames` and `render/viewmodel.wgsl` (the flash rule).

- `read` Every change of the view model's frame is blended, in every game (joined, listen, demo),
  over a FIXED 0.1 s from the client clock at the first rendered frame the new number is seen;
  never a measured gap between updates. The share is capped at 1; a clock run back draws the new
  frame alone. Settings: `r_lerpframes` (default 1, all models; there is none for the view model
  alone). Ours: `r_lerpmodels`, with `r_lerpframes` its synonym.
- `read` The blend is from the previous TARGET frame to the current one, one frame late, never
  predicted toward a next frame; a change before 0.1 s is up starts over from the full previous
  target. A wrap (nailgun 8 to 1) and the drop to rest are ordinary blends. A change of weapon
  model, or the gun hidden and shown, draws the new frame alone until its next change.
- `read` The nailgun and super nailgun step frames 1 to 8 and wrap, one per 0.1 s while fire is
  held, back to 0 at the first 0.1 s step after release. Lightning gun 1 to 4 and wrap; shotguns
  and launchers 1 to 6 once a shot.
- `read` THE FLASH (`r_lerpmuzzlehack`, default 1, view models `progs/v_*` except the axe and a few
  mod tools): a vertex is not blended when its forward coordinate in the model's space changes
  sign between the two frames (one path), or moves 6.3 units or more (an older path); it is drawn
  at the NEW frame for any share above 0. Nothing is hidden or faded. Ours: a vertex of the
  model's own flash triangles (`HeldModel::flame`) on opposite sides of the eye's plane in the two
  frames is frame b's; always on.
- `measured` v_nail: 109 vertices, 9 frames; v_nail2: 107, 9. Twelve vertices each are the flash:
  behind the muzzle inside the gun at rest (forward -4 to -10, v_nail2 -12 to -24), at the muzzle
  on frames 1 to 8 (+30 to +38, v_nail2 +39 to +51), so rest to firing crosses the plane. Between
  firing frames they move 7.7 to 8.0 (6.4 to 6.9) units and stay in front: blended with the barrel.
  Every other vertex moves at most 1.6 (4.9) units between firing frames.
- `read` The view model's place has no smoothing of its own: rebuilt each rendered frame from the
  final eye and angles. Positions blend linearly per vertex; on its default renderer the normal is
  the old frame's alone (ours blends normals and renormalises).
- `read` Recordings: the same fixed 0.1 s on the demo's clock. A recording written 30 times a
  second sees a frame change up to about 33 ms late, and every blend is still 0.1 s long.
- `read` Weapon prediction (an extension of the modern server, `cl_pext_weaponprediction`,
  `cl_nopred_weapon`, `cl_predict_buffer`, `cl_predict_weaponsound`): the client runs the weapon
  with its own commands, so the frame steps at once on fire instead of after the round trip; the
  frame shown is held back `cl_predict_buffer` commands. The common server does not offer it at
  the release the antilag note read (antilag.md § 4); not built here.
