---
type: Added
title: Screenshot name templates, png compression and the window's size and place
---

`scr_template` names screenshots by a template whose X's are numbered (Quake II's
`gl_screenshot_template`, `quakeXXX`), `scr_compression` 0 stores a png's rows
(`engine/capture/screenshot.hpp`), and `vid_geometry` sets any game's window size and place as
`WxH+X+Y` (`engine/platform/window_cvars.hpp`, `platform::set_window_place`).
