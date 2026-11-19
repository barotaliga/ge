---
type: Added
title: Mouse smoothing and acceleration, and the sequel's hand and gender
---

Shooters gain `m_filter` (blend each frame's mouse move with the last), mouse acceleration (`m_accel`, `m_accel_power`, `m_accel_offset`, `m_accel_senscap`) and `in_raw` (0 applies the desktop's pointer speed). In the sequel `m_accel` keeps its own meaning, and `hand`, `gender` and `gender_auto` are sent to the server.
