---
type: Added
title: The downhill, super-G and giant slalom set over a built map (R32, R33, R36) and their lab
---

`mapgen::set_downhill`, `set_super_g` and `set_giant_slalom(level, run)` set the speed events over a built map, each as a new map whose `Level::race` names the course and keeps the map it was set over. The downhill runs the whole piste (on a ski area, the course with the most vertical in its band, `downhill_course_of`): a racing line that bends less than the piste, red speed gates centred on it about every 80 m and none on a jump, the A-nets along both edges and a speed trap late on a straight. The super-G and the giant slalom start lowered down the course with the most vertical (`most_vertical_course_of`) into their band, their gates turning the racer side to side as many as the vertical's direction changes ask, each turning pole inside the line's apex, none on a jump's lip or landing; the giant slalom has two runs. The course preparation now shaves the crests round for a speed event. `analysis::analyze_race` holds each to its rule, and the `speed-events` lab draws every course straightened and the map from above, with the gate tables.
