---
type: Added
title: Paths and thinking on a schedule
---

`ge::ai` gains the one cheapest-path search every bot shares (`ai/path.hpp`: a deterministic heap, Dijkstra or A* over any graph, an 8-neighbour grid search with an octile heuristic, string-pulling) and thinking on a schedule (`ai/think.hpp`: every N steps staggered by index, slower with the distance from the player); the mountain's route search, the outing and the army's look now use them, and the `path` lab draws a walk across the ski area.
