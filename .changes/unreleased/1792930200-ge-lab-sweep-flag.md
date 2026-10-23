---
type: Added
title: ge lab sweep sweeps a lab's own flag
---

`ge lab sweep <id> --flag=<flag>=a,b,c` runs the lab with `--<flag>=<value>` at each value. The labs are built once and nothing is rewritten. Each run is tagged `sweep-<flag>-<value>`, and the metrics come out side by side in the same table as a `--set` sweep. `--set` and `--flag` together are refused.
