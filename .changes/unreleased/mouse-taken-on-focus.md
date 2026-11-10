---
type: Changed
title: Shooter's Edge takes the mouse the moment its window gets the focus
---

A click on the window or on its icon in the Dock or the taskbar gives the look the mouse at once, with no second click in the window. Only the console lets it go by default (closing it takes the mouse back); `in_mouse_release` adds the menu (2) or drops the demo's play bar (4). The engine's `input::follow_capture` decides it from the frame's facts and `platform::has_focus` says whether a window has the focus.
