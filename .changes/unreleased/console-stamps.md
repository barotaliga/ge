---
type: Added
title: Console lines stamped with the time
---

The engine's console view keeps a line's time beside its text when a game turns stamps on
(`ConsoleView::stamps`, `stamp_format`, `stamp_colour`), drawn before it, left off the notify lines
and written by condump; `ui::colour_from_text` reads a colour as a setting writes one. Quake II's
`con_timestamps`, `con_timestampsformat` and `con_timestampscolor` turn it on.
