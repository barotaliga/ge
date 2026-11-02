---
type: Changed
---
The modern look's map pass is a little cheaper again (the M3 Max's GPU frame at 1440p from 5.08 to 4.85 ms): a frame lit in real time with no weather draws with a pipeline compiled without the lightmaps', the rebake's and the weather's code, and a light wholly in its own shadow is skipped before its highlight. Every picture is unchanged.
