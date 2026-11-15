---
type: Added
title: ge map export builds version 38 maps
---

`ge map export --version=38` (and the map SDK's `build(..., version=38)`) builds `.map` text into a version 38 map that keeps its brushes for the box sweep, with clip brushes, water, RGB light and one cluster, so rooms imported from both classic families can be merged and built as one map.
