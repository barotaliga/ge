Status: done
Milestone: **A ski area raced as a map** (generator v8): a course stood up as a `Level` (its slalom gates, its own day, the area published as `Level::resort`), choosing the course (by id, by colour, or the seed's dealt colour), the area's analysis that accepts an attempt and the course's that offers a course, and the bot weaving through the gates
Exit: Deterministic, with golden digests (`tests/mountain_resort_level_test.cpp`); a 30-seed sweep builds clean; the bot finishes the reference seeds at v8
Labs: `level` and `analyze` at `--version=1,8`; `sim --version=8`

30 of 30 seeds built, 2.3–5 s a map; 36 of 36 racers home on 9 seeds, none missed a gate
