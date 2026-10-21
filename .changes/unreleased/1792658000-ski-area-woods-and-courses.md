---
type: Added
title: The ski area's woods, its day and its courses, and the resort and junctions labs
---

`resort_woods` grows the woods by height under the tree line (closed low down, thinning through the ecotone, a region's own curve) and keeps every trunk off the runs' corridors, the lifts' lines, the pads, the ramps, the way to the next queue and the hub. `resort_courses` deals the day, turns the face to its sun, and composes a course down the network from every piste's top to the village (`compose_course`, `course_arc`), each in R28's length band and coloured by its steepest stretch; `build_resort` runs every stage in order. The `resort` lab draws a ski area from above and with its courses picked out, with its lifts, runs, courses and attempts as tables; the `junctions` lab finds where the groomed snow steps where runs meet and draws the worst as relief.
