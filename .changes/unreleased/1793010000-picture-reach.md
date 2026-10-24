---
type: Added
---
A picture can draw only so far: `FrameView::reach_m` draws the ground's clipmap only to the reach, culls everything standing past it, and closes every surface but the cloud into the sky from half the reach, with the sky under the horizon drawn as the horizon's. Fall Line's quality ladder has a DISTANCE row (1, 2 or 4 km, or the whole ground), and the `world` lab takes `--reach`.
