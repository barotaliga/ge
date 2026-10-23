---
type: Changed
title: ge test --filter builds only the matching tests, and a full digest rewrite keeps unregistered labs
---

`ge test --filter=<regex>` now builds only the targets of the tests the filter picks, not the whole tree. The labs executable counts as the smoke pass's target, and a test with no target of its own falls back to the full build. `labs --all --smoke --write-digests` keeps the rows of labs this build does not register, such as the GPU labs on a GPU-less build, and names each row it keeps. Before, it dropped those rows.
