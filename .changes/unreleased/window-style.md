---
type: Added
title: The window's border, title bar, place on top and edges as settings
---

Every game's window takes `win_noborder`, `win_notitle`, `win_alwaysontop` and `win_noresize`
(`engine/platform/window_cvars.hpp`), followed each frame with full screen by
`follow_window_cvars` and `platform::set_window_style`.
