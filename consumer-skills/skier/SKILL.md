---
name: skier
description: "Use when the game links ge::skier (`ge use skier`, which brings ge::mountain): the skier on the snow. step_skier, the snow under the skis, what a skier can do (limits), the reference skis, the shared tuning, the trunks' grid, the hand-built maps and the ride scenarios."
plugin: skier
---

# The skier plugin

`ge::skier` (headers `skier/*.hpp`) is a skier and his pair of skis on a mountain's snow (a
`ge::mountain` Level, or a hand-built map). It never learns which game rides it: the modes,
the course's rules, the bot and the field are the game's.

| Header | What | Start with |
| --- | --- | --- |
| `skier/skier.hpp` | The model, its order stated in the header: `SkierState`, `SkierInput`, `World`, `step_skier`, `stand_skier`, `skier_hash` | `ge explain step_skier` |
| `skier/limits.hpp` | What a skier can do: edge, carve curvature, corner and terminal speed | `ge explain skier/limits.hpp` |
| `skier/snow.hpp` | The snow by its packed share: sink, base friction, powder drag, grip, brake | `ge explain skier/snow.hpp` |
| `skier/skis.hpp` | `SkiSpec` and the reference pair every shared number was tuned on | `ge explain SkiSpec` |
| `skier/tuning.hpp` | Every shared number, with its unit | `ge explain skier/tuning.hpp` |
| `skier/synthetic.hpp` | Hand-built maps for tests: a slope, a drag strip | `ge explain synthetic_slope` |
| `skier/scenarios.hpp` | Staged moments with scripted inputs, run and measured | `ge explain run_scenario` |

## Use it

- **One skier is one `step_skier` call a step**, in the game's fixed order; the game owns the
  state, the course and when a fallen skier stands up (`stand_skier`).
- **A pilot plans on `limits.hpp`**, never on numbers of its own: a bot that reads the same
  limits as the physics never asks for a turn the snow cannot hold.
- **Test on the hand-built maps.** Stage the skier with `stand_skier` on `synthetic_slope()` or
  `flat_strip(...)`, script inputs step by step, and check bits (`CHECK_BITS`): the tests then
  hold whatever the generator does.
- `ge find skier <word>` finds the rest.
