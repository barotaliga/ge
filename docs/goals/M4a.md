Status: done
Milestone: **The single piste** (generator v1, the single piste): `plugins/mountain/mapgen` (the mountain, the piste, its kickers, cliffs, drifts and crust, the forest, the day and the weather, the gates and the start line, the search over attempts, the compile and `level_digest`) and the single piste's `analyze_level` (`plugins/mountain/analysis`)
Exit: `level_digest`, every grid and every section of the level match the golden on the level seeds, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused (`tests/mountain_generate_test.cpp`)
Labs: `level`, `analyze`, `mapgen-cost`

0.53–0.77 s a map; the terrain park, R20, waits for M4b
