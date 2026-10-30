Status: done
Milestone: **Depth and time**: the depth prepass and the `equal`-tested opaque pass, the depth pyramid (Hi-Z), the motion target, TAA-U (Halton jitter, YCoCg clamp, the reactive mask), `reset_history` for stills
Exit: Every opaque pixel shaded once (a lab counter); stills repeat with history reset; the frame upscales from 50–100 %
Labs: `world` (TAA on and off), `taa` (the history's ghosting on a moving camera)

each covered pixel shaded once but where two clipmap rings meet at the same depth, a few in ten thousand; two stills after a cut are the same bits; `--scale` renders from 25 to 100 %; the reset is `fall_line::app::cut` over `render::reset_taa`
