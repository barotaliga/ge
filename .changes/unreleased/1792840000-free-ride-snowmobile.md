---
type: Added
title: The snowmobile on the free ride
---

Every free ride now has a snowmobile, parked on a level patch of open snow beside the village. It is placed clear of the trunks, the lifts and the wind tunnels, and it rests asleep until someone takes it.

- **Taking it:** a skier stood beside it presses the new machine press (`Controls::machine`, a new flag on the tape). His skis are racked on it and he rides it on his own controls: the tuck is the throttle, the brake the brake, the steer the bars, and the new `Controls::lean` axis his weight.
- **Getting off:** the press again hops him off onto his skis, and the machine is left where it stands.
- **Being thrown:** a trunk, a roll, a hard landing or a wall throws him off. He lies on the snow, and when the run's down reset would stand him up he is put back on the machine where it lies.

The `free` lab gains a `sled` table: the bot's outing on the machine. On seed 38 it climbs 425 m up a blue run on the machine and then skis that run down to its foot. Every free ride's digest moves, because the run's hash now folds in the parked machine. Every number a run reports is unchanged.
