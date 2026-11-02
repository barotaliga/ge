---
type: Fixed
---

`ge digest --affected`, `ge test --affected` and `ge lint` now see what a change reaches when the build directory lies inside the repository, where Ninja records its dependencies relative to the build directory: a change to a game library's source picks the labs and tests that include its headers again, and the reach is read in seconds rather than minutes.
