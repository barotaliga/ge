---
type: Fixed
title: The sequel's mouse turns the view again, and its lifts and view roll move smoothly
---

The sequel's own default config no longer unbinds the window's move keys or turns the mouse into a
sidestep, and `sensitivity` now reads as the classic number (times the new `m_yaw` and `m_pitch`),
so a pack's `sensitivity 3` turns at the speed it means. On the sequel's rules a lift's rise and
the view's roll, which change ten times a second, are eased across each tenth, so the ride up and
the lean while sidestepping no longer jump.
