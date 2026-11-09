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
