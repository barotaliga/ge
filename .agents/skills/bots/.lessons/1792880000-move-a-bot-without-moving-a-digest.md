---
title: A bot moved into a plugin kept every digest by moving its arithmetic verbatim behind the old API
date: 2026-10-23
scope: plugins/skier/pilot.cpp, games/fall_line/sim/bot.cpp
concepts: [pilot, refactor, digest, determinism, wrapper, plugin, move]
---

M16c moved Fall Line's pursuit and speed plan into `plugins/skier/pilot.hpp` with every lab's
digest unchanged. What made it safe: the game's API (`BotPlan`, `plan_bot`, `bot_controls`)
stayed as a thin layer over the pilot, so none of its twenty callers moved; `BotPlan` derives
from `PilotPlan`, so every read of `plan.speed` or `plan.track` still compiles; the moved code's
float operations kept their order (the min over the lips is the one reordering, and min is
exact); and the game-only parts (the weave, which kickers are lips, the mode) stayed in the game
and hand the pilot plain data. Run `labs --all --smoke --write-digests` to a scratch file and
diff it against `tests/labs_digests.txt`: a moved bot owes an empty diff.
