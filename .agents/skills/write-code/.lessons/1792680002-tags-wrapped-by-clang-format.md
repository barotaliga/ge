---
title: clang-format folds a long ge:summary into the next tag line
date: 2026-10-21
scope: plugins/skier/app/snow_bed.hpp, tools/agents-index.mjs
concepts: [ge-tags, clang-format, agents-check]
---

A `ge:summary` past the 100-column limit is reflowed by `make fmt`, which joins the following
`ge:keywords`, `ge:skill` and `ge:test` lines into one paragraph; `make agents-check` then
reports the file as missing its keywords and skill. Keep every tag line under 100 columns
(shorten the summary) and rerun `make agents-check` after `make fmt`, not before.
