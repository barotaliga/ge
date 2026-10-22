---
type: Added
title: The helicopter on the free ride
---

Every free ride now has a helicopter, parked on its pad: a level patch of open snow near the village, clear of the trunks, the lifts, the wind tunnels and the snowmobile, faced to the summit.

- **Taking it:** a skier beside its seat presses the machine press and is sat on its right skid. He flies it on his own controls: the new `Controls::lever` is the collective, the lean is the cyclic fore and aft, the steer the cyclic side to side, and the new `Controls::pedal` the pedals. The tape gains both axes.
- **The drop:** the press again pushes him off the skid. In the air, the machine's pilot flies it home to its pad and shuts down. Landed, he steps off and it shuts down where it stands.
- **The crash:** a crash throws him off. The ride then starts again from the pad, with him sat on a fresh machine.

The bot's pilot now slows for a rise ahead it cannot climb at speed, and for a long descent to its aim. The `free` lab gains a `heli` table: on seed 38 the bot flies 1,082 m up to the head of the highest run, is dropped there and skis it to its foot. Every free ride's digest moves, because the run's hash now folds in the helicopter; every number a run reports is unchanged.
