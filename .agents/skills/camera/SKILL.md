---
name: camera
description: "Use for engine/camera in gameengine2 and anything about how a game is seen: the pose and the renderer's roll, the followers (second-order by frequency, damping and response; the half-life spring; the dead band), the subject a game fills and the world it queries, the rigs (first person with view height, step smoothing, bob, strafe roll, view kick, punch and landing dip; the boom with yaw follow, lift, incline, composition, the sense of speed, the fall look, pull-in and push-off; the mounted head on a neck; orbit, planted and free), trauma shake and comfort, transitions (cut, blend, inertialize) and warm rigs, the camera stack and its ladder, the scripted subjects, and the camera-follow, camera-rigs, camera-transitions, camera-shake and camera-world labs; Fall Line's ladder (games/fall_line/camera); the M26 rows."
---

# The camera

`engine/camera/` (`ge_camera`, namespace `ge::camera`, no GPU) is every view any game on the
engine is seen through: a shooter's eyes, a skier's chase boom and helmet, a racer's cockpit, a
broadcast's planted lens. The design and the research it rests on are
[`docs/camera.md`](../../../docs/camera.md); the rows are `docs/goals.md` M26a–M26f. Nothing
here names a game's thing: a game fills a `Subject` (where its body is, how it moves, what just
happened to it) and a `World` (its ground, what is clear, what is in the way), picks a ladder of
rigs and calls `update` once a display frame.

| File | What |
| --- | --- |
| `camera/pose.hpp` | `Pose` (eye, yaw, pitch, roll, vertical fov, near), aim and blend, Hor+ fov (`fov_y_of`, `fov_h_of`) |
| `camera/follow.hpp` | The second-order follower (`Poles`), the half-life `Spring`, `damp`, `dead_band` |
| `camera/subject.hpp` | `Subject` and its `Moment`s (landing, hit, shot, explosion, teleport) |
| `camera/world.hpp` | `World`: the ground, the clear share of a lens's line, the obstacles near |
| `camera/shake.hpp` | Trauma shake and the smooth `wave` every buzz is made of |
| `camera/first_person.hpp` | The eyes: view height, steps, bob, strafe roll, kick, punch, dip, zoom, sway |
| `camera/boom.hpp` | The boom: yaw follow, lift, incline, composition, speed, fall look, clearance |
| `camera/mounted.hpp` | A lens worn by the body: the head on a neck |
| `camera/lenses.hpp` | Orbit, planted (zoom to hold a size), free |
| `camera/transition.hpp` | Cut, blend in the subject's frame, inertialize (the quintic decay) |
| `camera/camera.hpp` | The stack: the ladder kept warm, `set_rung`, the moments, the settings |
| `camera/to_view.hpp` | A `Pose` into `render::FrameView` (header only; not installed) |
| `camera/scenarios.hpp` | The scripted subjects the labs and the golden run every rig through |
| `camera/ladders.hpp` | The reference ladder the labs use |
| `games/fall_line/camera/` | A game's ladder on the engine: Fall Line's rungs and camera key (`rungs.hpp`), the skier as the subject with the worn eyes and the per-frame `frame_camera` (`subject.hpp`), the trunks and posts (`world.hpp`); its lab is `camera-world` (GPU) |

## Rules

- **The renderer never moves the camera, and the camera never reads the renderer.** The camera
  makes a `Pose`; `to_view.hpp` hands it over. A rig that needs the world asks the `World`.
- **Nothing a rig chases is copied or eased by a bare exponential.** A reading goes through a
  follower or a spring (`follow.hpp`), solved exactly over the frame, so a lens has weight and
  the same path at every display rate. A new rig field with a time constant is a `Poles` or a
  half-life, never a per-frame factor.
- **Every rig is warm.** All rungs are framed every frame, so a change starts from where the new
  rig would be. Do not skip framing a rung to save time without measuring it (`perf`).
- **The default change between rungs on one subject is inertialize**; a cut is for a new place
  or a new subject (a respawn, a broadcast's corner), a blend for two lenses far apart in kind.
- **Comfort is a scale, never a branch.** `CameraSettings::motion` and `shake` multiply every
  motion the player did not command; at 0 the picture holds still (the `camera-shake` lab fails
  a comfort of 0 that moves it).
- **Deterministic.** `core::libm` only (`tools/check-libm.sh` scans `engine/camera`), no clock but
  `dt`, the shake's waves dealt from a seed. A change to a rig's numbers rewrites
  `tests/golden/camera_scenarios.txt` (`GE_GOLDEN_WRITE=1`) in the same PR and says why.
- **Roll is right side down positive**, and `FrameView::roll` of 0 leaves the matrices untouched.
- **The field of view a player sets is horizontal at 4:3** (`cl_fov`'s convention); a `Pose`
  carries the vertical one.

## Adding a rig kind or a field

1. The row and the state in the rig's header, the frame in its `.cpp`; read `Subject` and the
   `World` only.
2. `RigKind`, `Rig`, `RigState` and the switch in `camera.cpp`'s `frame_rig`; a builder beside
   the others.
3. A test in `tests/camera_<file>_test.cpp`; a rung in `ladders.cpp` if the labs should show it;
   a scripted subject in `scenarios.cpp` if no existing one exercises it.
4. The golden rewritten, the labs run before and after, the digests rewritten
   (`build/<preset>/ge digest <lab> --write`).

## Labs

- **`camera-rigs`** (scenario): every rung through every scripted subject. Read the lenses
  sheets first, then the table: `least-clearance` (a boom under its clearance is a bug),
  `worst-accel` (judder and snaps show here), `stray` (a boom's subject out of its frame),
  `jumps` (any eye jump the script did not teleport fails).
- **`camera-follow`** (scenario): the followers at 30–240 Hz and through hitches. Any change to
  `follow.cpp` runs it; `stray` is the number to hold.
- **`camera-transitions`** (scenario): every edit between rungs mid-carve. `switch-kick` must
  stay near 0 for a blend and an inertialized change, and `landed` within the duration.
- **`camera-shake`** (scenario): trauma by size, comfort and display rate.
