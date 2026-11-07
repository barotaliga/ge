---
title: Check a lab's digest without building every test
date: 2026-11-06
scope: engine/cli, tools/precheck.sh
concepts: [digests, check-digests, disk-space, labs-smoke]
---

`ge digest --affected` builds the whole tree, every test executable included. In a debug ASan
build that is about 19 GB, which fills a cloud container's disk allowance. To check the digests
of a few labs, build only `labs` and run each one on its own:
`build/debug/labs <lab> --smoke --check-digests=tests/digests`. The runner takes one lab per run.
`--write-digests=tests/digests` rewrites the lab's baseline after a deliberate change.

A digest also moves when only a chart's axis label or a caption changes. Before you rewrite a
digest, run the lab with `--tag=before` and `--compare=before` to show the PR that every metric
and table is unchanged.
