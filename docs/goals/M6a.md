Status: in progress
Milestone: **Frame pacing: the cap, vsync, frames in flight, the steady clock, the frame-time readout** (`docs/rendering.md` § Frame pacing)
Exit: Shooter's Edge paces its window by the player's cvars (vid_vsync, cl_maxfps, cl_maxfps_hz, vid_framelatency, vid_steady, vid_vsync_lag_fix, scr_showfps, framestats); the pacing lab shows the default's latency and judder below the old loop's; framestats on the Mac's ProMotion display shows 120 fps with no hitches
Labs: `pacing`

**First slice (in progress):** `engine/loop/pacing.hpp` (the cap's grid, the steady clock, the
frame-time record and its summary), `engine/platform/pacing.hpp` (the cvars, the precise wait,
`framestats`, the display's refresh, the swap chain's images on Metal), `engine/gpu/frame_gate.hpp`
(the frames in flight), `engine/hud/frame_graph.hpp` (`scr_showfps`), wired into Shooter's
Edge's window; the `pacing` lab. In the lab's model at 120 Hz the default (vsync, two images, two
frames in flight, steady) takes the input-to-picture lag from ~25 ms to ~16.6 ms with judder
under 0.05 ms, under both loads. `vid_vsync_lag_fix` (off by default) starts each frame just in time for its
refresh: ~8.8 ms under the light load.

**Open:** the Mac's measurement (`framestats` on the ProMotion display, windowed and fullscreen,
at each vid_framelatency, the issue labelled `needs-mac`); Fall Line's window wired the same way
(its `vid_vsync` is latched today); the lag fix's margin measured on the Mac (`vid_vsync_lag_tweak` 0.5 to 2).
