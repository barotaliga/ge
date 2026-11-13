---
type: Added
title: The shooter's move and nav trace both map families
---

A move's world (`ge::shooter::MoveWorld`) now holds a version 38 map as a solid of its own kind, traced through the brush tracer, beside the clip hulls of the older family. The player trace, the free-box test, the contents and a new point trace answer the same way on either, so the navigation's sight, rocket and grenade checks run on both.
