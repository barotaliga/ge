---
type: Added
title: The skier's body on Fall Line's run, behind the INJURIES switch
---

Every racer now carries his body (`Racer::body`). With `GameState::injuries` on, the body is stepped after his skis on every step, so the blows his skis meet hurt him and can kill him. A reset mends him. A mortally hurt skier is not stood back up, and once dead his run is over. Each step's injuries come out as a `hurt` event and a death as a `died` event. The switch is off by default, and while it is off no body is touched and no digest moves.
