---
type: Changed
title: Precheck names the one-step digest rewrite
---

When `make precheck`'s digests gate fails, it now names `ge digest --affected --write`, which reruns every lab the change reaches and rewrites each one's digest under `tests/digests/` in one step, for a move the change meant to make. `ge digest`'s `--write` help says that it combines with `--affected`.
