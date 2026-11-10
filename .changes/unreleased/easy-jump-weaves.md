---
type: Changed
title: The easy jump strafes for you, weaving its hops
---

With `cl_speedjump 1`, holding forward and jump now has the game strafe for you on every server: the hops weave left and right of where you aim, like a bunny hopper's, and gain more speed (the window's own server's `pm_speedjump` is now 0.8 of the best strafe's gain, up from 0.5). Your mouse left and right steers the hops, so you can turn while hopping; looking up or down changes nothing. The new `cl_speedjump_view` (0..1, 0.5 by default) sets how much of the left and right swing your view shows: 0 keeps it still, 1 shows it all, as a strafer sees; a shot always goes where your screen looks.
