---
type: Added
title: ge lint --like-ci, and ge lint naming what the build does not compile
---

`ge lint --like-ci` lints as CI's lint job does: the release preset, the change since `--base`, then the shaders' check where the build has it, and a reached file the build does not compile is an error. Without it, such a file (a GPU or window source in a build with that feature off) is now a warning that names it, instead of being handed to clang-tidy with no compile flags.
