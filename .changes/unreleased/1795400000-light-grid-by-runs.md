---
type: Changed
---
The modern look's light grid is binned in about half the CPU time (0.12 to 0.07 ms a frame at 720p on the M3 Max): each row of cells a light reaches is filled as one run instead of testing every cell. The `se-timedemo` lab now paces frames as the window does (one in flight) and lists where the CPU's part of each frame goes.
