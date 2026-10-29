---
type: Added
title: Post effects
---

The post pass gains contrast-adaptive sharpening, lateral colour fringes, a vignette, film grain and lens dirt (each an `override` switch with a live strength, `PostSettings::look` and `set_post_look`), the grade's knobs (contrast, saturation, temperature, tint, gamma) baked into its table (`render/grade.hpp`), and a quarter-size `lens` pass (`render/lens.hpp`) for the sun streaming through what stands in front of it and the sun's flare; `post_group` takes the lens pass's target, and `kMaxPasses` is 40.
