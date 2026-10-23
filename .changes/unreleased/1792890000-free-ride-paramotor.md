---
type: Added
title: The paramotor on Fall Line's free ride
---

A free ride can now begin under the paramotor (`create_para_ride`). The skier starts at the head of the highest run with the wing ready over him:

- **His controls fly it:** the tuck is the throttle, the brake both brakes, the steer the toggles and the lean the speed bar. The tape is unchanged.
- **The press:** the machine press lets the rig go.
- **The reset:** his reset under the rig begins the ride again on the summit. Once the rig is off him, his reset is his run's.
- **The bot's outing (`simulate_para_outing`):** off the summit on full throttle, straight to a gentle stretch of the run he began on, a spiral down over it, a glide in along the run, a landing on his skis, then down to a foot.

The `free` lab's new `--para` table and `tests/fall_line_para_test.cpp` hold it.
