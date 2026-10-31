---
type: Changed
---
Shooter's Edge: the modern look's shadow maps no longer hitch (at most two new light cubes a frame,
slots reused least recently first, dynamic lights without a shadow); the `se-timedemo` lab's
`--price=1` prices each drawn switch by playing the demo again with it off, in place of the GPU
time per pass; `-` flags are read anywhere on the command line, so `+r_look modern -benchmark demo1`
benchmarks; `vid_display` and `vid_displays` pick the display `vid_fullscreen` covers.
