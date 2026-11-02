---
type: Changed
---
The modern look draws a map's lit rooms about a quarter faster at 1440p (the M3 Max's GPU frame from 7.5 to 5.1 ms): a pixel now tests only the lights listed for its 4 m cell of a grid round the eye, not every light near the eye, and each texture lookup wraps its texels once instead of four times. Every picture is unchanged.
