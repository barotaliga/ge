---
type: Added
title: Lightning drawn in the frame
---
The frame carries up to four lightning flashes as lamps (`render/view.hpp`'s `Flashes`). `render/wgsl/flash.wgsl` lights the clouds from inside (sheet lightning), the sky's air, a deck's base, the ground and the trees with them, and `render/bolts.hpp` draws the channel as glowing ribbons before the bloom. The cloud's spread march is held off for four frames after a flash, so no flash lingers. Fall Line's frame runs the new `bolts` pass, and the `storm-sky` lab pictures a strike, sheet lightning, a bolt from the blue and the flicker frame by frame.
