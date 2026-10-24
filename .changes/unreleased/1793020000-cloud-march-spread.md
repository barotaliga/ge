---
type: Changed
---
The quarter-resolution cloud march is spread over four frames (`Clouds::spread`, on by default): each frame one of every two by two of the half-size pixels marches, in a compact pass of its own (`clouds.fresh`), and the other three reproject the last frame's half-size picture by the distance its cloud lay at, read bilinear over the taps written the frame before that marched to the same scene, marching after all where no history stands. The two half-size targets swap each frame, and the `weather` and `ladder` labs take `--spread=0` for the whole march every frame (the `weather` lab also `--turn` to swing the view).
