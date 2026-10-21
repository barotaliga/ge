---
type: Added
title: Generator version 8 builds a ski area raced down one of its courses
---

`generate_level` at version 8 builds the ski area and stands one of its courses up as the map: the course's line is the track, its kickers, drops and drifts are named by their arc down it, slalom gates are set down it off its own hash (`course_gates`: alternately left and right of the line, inside the piste, on the line over a kicker or a drop), its day is its own (`course_day`: the sun held to the face's bearing), and the area is published as `Level::resort` (its runs, lifts, courses, village, hub and wind tunnels). An attempt is accepted when `analysis::analyze_resort` passes the area and at least one course passes `analysis::analyze_course`; the course raced is the one asked for by id (`GenerateOptions::course`), else the nearest colour asked for (`grade`), else the colour the seed deals (`deal_grade`). Versions 9 and 10 are refused by name until the ski routes are built. Fall Line's bot weaves through gates that stand off the line. The `level` and `analyze` labs take `--version=1,8`.
