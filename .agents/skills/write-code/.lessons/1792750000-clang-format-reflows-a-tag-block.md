---
title: A header comment that grows makes clang-format reflow the ge tags into each other
date: 2026-10-23
scope: engine/console/view.hpp
concepts: [ge-tags, clang-format, reflow, agents-check, make-fmt]
---

clang-format reflows a long `//` comment paragraph, and a `ge:keywords` line with no blank line
before the next tag is part of that paragraph: widen it past the column limit and `make fmt`
joins it with `ge:skill` and `ge:test` on shared lines, so `make agents-check` then reports
"missing ge:skill". Keep each `ge:keywords` line short enough to fit (split long ones into
several `ge:keywords` lines yourself), and after `make fmt` look at the tag block of every
header whose comment you touched.
