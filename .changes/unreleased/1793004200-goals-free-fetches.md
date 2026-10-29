---
type: Changed
title: ge goals free fetches first
---

`ge goals free` now fetches every branch before it answers, since a row taken on a branch fetched since the last `git fetch` was listed as free. `--no-fetch` answers offline from the branches as last fetched; `--fetch` is still accepted.
