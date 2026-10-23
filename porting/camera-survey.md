# The sibling games' cameras (working notes)

What each reference game's camera does, and which part of `docs/camera.md` takes the idea over.
Behaviour only: nothing here is copied as code, and the engine's names are its own. Deleted with
the rest of `porting/` when porting ends.

| Idea | Where it lives in the references | Engine home (`docs/camera.md`) |
| --- | --- | --- |
| A ladder of rigs as data, one row a rung; worn lenses (`bolted`) and booms | game5 `camera-rigs.ts` (tips, helmet, chase, far, high, orbit); game4/game3 `camera-rigs.ts`; game2 `camera-rigs.ts`, `camera-eye-rigs.ts` | The rigs; Fall Line's ladder (M26c) |
| Second-order follower (f, zeta, r), mass clamped for a long frame | game5 `camera-spring.ts` | Followers |
| The hand-over: both rigs framed for a fixed time and blended; other booms kept warm; ladders per machine | game5 `camera.ts` (`HANDOVER`, `blendLens`) | Transitions (blend, warm rigs) |
| The flight between rungs held in the vehicle's frame, landing on the rig, a floor under it | game2 and game3 `camera-change.ts` | Transitions (blend in the subject's frame) |
| Cut to a corner camera, fly back to the boom | game2 `camera-tv-cut.ts`, `camera-tv.ts`; game3/4/5 `camera-tv.ts`, game5 `camera-replay.ts`, `camera-cinema.ts` | Directors (M26d) |
| Grip as height, attitude as tilt, the flight as the boom's angle, surge, tremor | game2 `camera-feel.ts` | Boom (surge, tremor, bank, fall look) |
| Sense of speed: stretch (fov widens, arm pulls in), surge, tremor; bank in the air | game5 `camera-rigs.ts` (`PACE`), `camera-pace.ts` | Boom (sense of speed) |
| Incline with the slope, composition (`place`, `frame`), lag cap | game5 `camera-rigs.ts` | Boom (incline, composition) |
| Head on a neck driven by the body's velocity difference; road grain; hits throw the head | game2 `camera-eye.ts` | Mounted |
| Shake as a decaying wave, outside rigs ignore the vehicle's own knocks | game2 `camera-shake.ts` | Modifiers (shake) |
| Arm pulled in at once, let out slowly; pushed off trunks like a magnet | game5 `camera-rigs.ts` (`pullIn`, `repel`), `camera-clear.ts`; game4 `camera-clear.ts` | Boom (clearance), the world's queries |
| The fall look down a long fall | game5 `camera-fall.ts` | Boom (fall look) |
| Roll laid on from a world up; a worn lens's up | game5 `camera-roll.ts` | The pose (roll) |
| Death, crash, impact and X-ray cameras | game4 `camera-death.ts`; game5 `camera-death.ts`, `camera-crash.ts`, `camera-impact.ts`, `camera-xray.ts` | Planted rigs and directors (M26d) |
| Machine lenses (sled, heli, para, plane, balloon, wingsuit, skydive) | game5 `camera-sled.ts`, `camera-heli.ts`, `camera-para.ts`, `camera-plane.ts`, … | The machines' lenses (M26f) |
| The classic first-person view: bob from speed on a cycle, strafe roll, view kick | the classic shooter's client view code (`porting/shooters-edge/`) | First person; Shooter's Edge's view (M26e) |

The current engine has one camera: `games/fall_line/app/views.cpp`'s `chase_view` (a fixed
5.5 m arm, 2 m up, no memory) and the named views for the labs. M26c replaces `chase_view` with
the ladder; the named views stay (they are lab shots, not a camera that moves).
