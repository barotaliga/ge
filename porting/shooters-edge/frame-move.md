# The original player move, specified

The single-player engine's player movement (the original rules, before the network game's
predicted move), as rules and numbers in our words (README.md, the rules). The predicted move is
`movement.md`; this note is the other half of the movement setting below. The building session
reads this note, never the existing implementations.

**Where each fact came from.** A research session (2026-10-30) read the 1996 source release's
single-player server and the single-player game program's QC. Nothing was measured yet: like
M18o1, the parity target will be recorded vectors (`vectors/framemove.txt`, written by a research
session that builds the original move outside this tree and drives it over the scenes of
`movement.md` § Test vectors). Until then, every number here is `verify`.

## The movement setting

One server setting picks the player move for every player, in any game:

| `sv_playermove` | Move | Default when |
| --- | --- | --- |
| `arena` | the predicted move (`movement.md`): each command moves the player by its own length as it arrives, the client predicts it, the jump and the water jump are the move's | the program is a multiplayer one |
| `original` | this note's move: the server's own step, the jump and the water jump the program's or (for a multiplayer program) the engine's, as below | the program is a single-player one |
| `auto` (default) | follows the program | |

So a multiplayer match plays with either move, and so does the campaign. The setting is read at
map start (changing it mid-map takes effect on the next map) and sent to clients with the
server's settings, so a predicting client predicts with the same rules. The names describe the
mechanism, never a product.

**Who jumps.** A single-player program jumps for itself in `PlayerPreThink` (it adds the jump to
`velocity` and clears `onground`), and checks for a jump out of water itself. A multiplayer
program expects the move to do both. So:

- single-player program, either move: the move never jumps or water-jumps; it honours the
  program's `onground`, `velocity` and the water-jump flag (`FL_WATERJUMP`, 2048) with its
  `movedir` and `teleport_time` (below);
- multiplayer program, `original` move: the engine does the program's part with the original
  numbers before the move (§ The jump);
- multiplayer program, `arena` move: as today.

**When it moves.** In the single-player host the move runs once per server frame with the
player's latest command (§ The frame), as the original did. In the multiplayer host it runs per
command with the command's own length (as the predicted move does), so the client can predict it
with the same function: the rules below are written for a step of length `dt` and hold for both.
`verify` how far per-command stepping of the original rules drifts from per-frame stepping on
the recorded scenes (it changes only the step lengths, not the rules).

## Settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `sv_gravity` | 800 | units/s² |
| `sv_friction` | 4 | ground friction |
| `edgefriction` | 2 | friction factor when the ground ahead falls away |
| `sv_stopspeed` | 100 | below this speed friction acts as if at this speed |
| `sv_accelerate` | 10 | ground, air and water acceleration |
| `sv_maxspeed` | 320 | the wish speed's cap |
| `sv_maxvelocity` | 2000 | each velocity component is clamped to ± this |
| `sv_nostep` | 0 | 1 turns off stepping up |
| `sv_idealpitchscale` | 0.8 | `campaign.md` § The client data |
| `cl_rollangle`, `cl_rollspeed` | 2.0, 200 | the body's roll that tilts the move (below) |

The player's box is the same as `movement.md`'s; the step height is 18.

## One step

For a player with movetype walk, in this order (the parts in brackets are the frame's, § The
frame):

1. **The punch** decays (`campaign.md` § The client data).
2. A dead player (health ≤ 0) gets no input: skip to 6.
3. **The body's angles.** `v_angle` is the command's view. Its sum with `punchangle` gives the
   aim view. Unless `fixangle` is set, the body's pitch is minus a third of the aim view's
   pitch and its yaw the aim view's yaw. Its roll is 4 × the view roll the velocity gives: the
   velocity's component along the body's right vector, its sign kept; below `cl_rollspeed` the
   roll is that speed × `cl_rollangle` / `cl_rollspeed`, at or above it `cl_rollangle`; the
   right vector's sign decides left or right. `verify` the sign.
4. **The wish.** forward, right from the BODY's angles (not the view's): so the body's pitch
   (a third of the view's) and roll tilt them. The wish velocity = forward × the forward axis +
   right × the side axis, its z replaced by 0 for walk (the up axis for fly and swim). While
   `teleport_time` is still ahead, a negative forward axis counts as 0 (no backing out of a
   teleporter). The wish speed is its length; above `sv_maxspeed` the wish is scaled down to it.
   - **Noclip**: the velocity becomes the wish velocity. Done.
   - **Water-jumping** (`FL_WATERJUMP` set): the horizontal velocity is set to `movedir`'s x
     and y; the flag and `teleport_time` are cleared once the server's time passes
     `teleport_time` or the player leaves the water. Skip the rest of 4.
   - **Swimming** (water level 2 or more, not noclip): as `movement.md`'s water move with these
     numbers: the wish from the VIEW's forward and right with the up axis added to z (or 60
     subtracted when all three axes are 0, a drift down); the wish speed capped at `sv_maxspeed`
     and then × 0.7; friction: speed −= `dt` × speed × `sv_friction`; acceleration as on the
     ground with that wish.
   - **On the ground**: friction, then acceleration (below).
   - **In the air**: air acceleration (below).
5. **[The program's pre-think]**, which in the campaign jumps and checks for a water jump.
6. **[Gravity and the slide]**: unless the player is in water (level 2 or more) or water-jumping,
   `velocity.z −= gravity × sv_gravity × dt` (the program's `gravity` field, 0 meaning 1). Each
   component clamped to ± `sv_maxvelocity` (a non-number becomes 0). Then **unstick** (below),
   then the **walk move** (below), then touching triggers, then the program's post-think.

### Friction (on the ground)

Horizontal speed `s` (0: nothing). A point 16 units ahead along the horizontal velocity at the
box's bottom is traced 34 down (a point trace that ignores monsters); if it finds nothing,
the friction is `sv_friction × edgefriction`, else `sv_friction`. `control = max(s,
sv_stopspeed)`; the new speed `max(0, s − dt × control × friction)`, and all three components
of the velocity are scaled by `new / s`.

### Acceleration

On the ground: with the unit wish direction `d` and wish speed `w`, the current speed along `d`
is `v · d`; `add = w − v · d`, nothing when `add ≤ 0`; `step = sv_accelerate × dt × w`, at most
`add`; `v += step × d`.

In the air: the same with the wish speed used for `add` capped at **30**, while the step still
uses the full wish speed: `add = min(w, 30) − v · d`; `step = sv_accelerate × w × dt`, at most
`add`. This is what lets a player curve in the air (air strafing).

### The walk move

1. Clear `onground`, remember the origin and velocity, and slide (the fly-move of `gameloop.md`
   § Step) for `dt`, remembering any wall it hits (a plane whose normal z is exactly 0).
2. When the slide did not hit a wall, done. Also done (keeping the slide) when the player was
   not on the ground before and is not in water (no stepping up mid-jump), when the movetype is
   no longer walk, when `sv_nostep` is set, or when water-jumping.
3. **Step up**: keep the slide's result as the fallback. Back at the start: push straight up 18
   (a trace, not linked as a move), then slide again for `dt` with the horizontal velocity of
   the start and no vertical velocity.
4. If that slide hit something but moved less than 1/32 unit on both x and y from the start,
   **try to unstick**: eight times, push 2 units in one of (2,0), (0,2), (−2,0), (0,−2), (2,2),
   (−2,2), (2,−2), (−2,−2) and retry the slide for 0.1 s with the start's horizontal velocity;
   the first that ends more than 4 units from the start on x or y wins; when none does the
   player goes back to the start with no velocity.
5. **Wall friction**: when the step's slide hit a wall: `d` = the wall's normal · the VIEW's
   forward + 0.5; when `d < 0`, the velocity's part along the normal is removed and the
   horizontal remainder scaled by `1 + d` (running into a wall head-on slows; glancing does not).
6. **Step down**: push vertically by `−18 + v₀z × dt` (`v₀` the velocity at the start). When the floor found has a normal z above 0.7 the step is kept
   (and `onground` set with that ground, for a brush model's floor); otherwise the fallback is
   restored (no climbing a slope too steep by stepping).

### Unstick (before the walk move)

When the player's box is in a solid: try the origin of the last step (`oldorigin`); else try the
origin moved by −1, 0 or +1 on x and y and 0 to 17 up, in that order (z outermost, then x, then
y); the first free spot wins; else stay. When the box is free, `oldorigin` = the origin.

### Water level

From the box's bottom + 1, the box's middle and the eye (`view_ofs`): the type of the first
point is the water type when water or worse, with level 1; the second point in water makes it 2,
the eye 3. Out of water: type empty, level 0.

## The jump (when the engine jumps for a multiplayer program)

The numbers the campaign's program uses, done by the engine before the move:

- Water-jumping: no jump.
- Water level 2 or more: the vertical velocity is set to 100 in water, 80 in slime, 50 in lava
  (swimming up).
- On the ground with the jump button held and the jump released since the last jump: the
  vertical velocity gains 270, `onground` is cleared and the jump is marked held (no pogo until
  the button is released).
- **A jump out of water** (checked when the water level is 2): from 8 above the origin, a point
  trace 24 ahead along the body's horizontal forward; when it hits, the same trace from eye level
  (the box's top − 8 higher); when that one is clear: `FL_WATERJUMP` set, `movedir` = the first
  hit's plane normal × −50, vertical velocity 225, the jump marked held, `teleport_time` = now
  + 2.
- In water (not noclip, alive, not water-jumping) the velocity loses `0.8 × level × dt` of
  itself each step (the program's water drag).

## The frame (single-player host)

Unlike the multiplayer host (`gameloop.md` § The server's frame), the single-player host runs
everything once per host frame:

- **The host frame**: at most 72 a second (a frame waits until 1/72 s has passed);
  `frametime` = the real time since the last, clamped to 0.001..0.1 (`host_framerate` above 0
  forces it). We run a fixed step instead (the engine's run clock at 72 Hz, `docs/architecture.md`):
  the original's variable frame is not reproduced; `verify` the vectors at a fixed 1/72.
- In order: (1) read each client's packets, keeping its latest command (a command persists
  until a newer one arrives), and run step 1–4 of § One step for each spawned client with that
  command; (2) the physics: `StartFrame`, then every entity in number order: players run step
  5–6 (pre-think, their `think` if due, gravity, unstick, walk move, triggers, post-think),
  everything else by its movetype as `gameloop.md` § Running an entity; (3) the server's time
  advances by `frametime` **after** the physics (so the whole frame sees the time it started
  at); (4) send.
- **Thinking**: one think per entity per frame when `nextthink` is at most `time + frametime`
  (the program sees the later of `nextthink` and now); no chain of thinks in one frame.
- **New entities**: there is no `newmis`; an entity spawned during the frame with a higher number
  runs in the same frame when the loop reaches it.
- Toss, bounce, fly and step entities move by the same rules as `gameloop.md` § Tossing and
  § Step.
