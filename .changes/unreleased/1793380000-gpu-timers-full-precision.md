---
type: Fixed
title: GPU pass timers keep their full precision
---

Dawn rounded every GPU timestamp down to 2^16 ns by default, so on a real GPU a pass's time came in steps of 0.0655 ms and a small pass read 0. The device now turns that rounding off, and the quality ladder's price table holds the reference machine's (an Apple M3 Max at 1920x1080) prices measured with the precise timers.
