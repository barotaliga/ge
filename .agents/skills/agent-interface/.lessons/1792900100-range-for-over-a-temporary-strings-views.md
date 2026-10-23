---
title: Never range-for split_lines over a temporary string; hold the text first
date: 2026-10-23
scope: engine/cli
concepts: [string-view, lifetime, split-lines, read-text, asan, use-after-free]
---

`for (auto line : split_lines(read_text(p).value_or("")))` is a use-after-free. Range-for
extends the lifetime of the vector `split_lines` returns, but not of the string inside it:
`value_or` returns a temporary, which dies before the first iteration, so every `string_view`
points into freed memory. A debug build's ASan caught it in `ge lab sweep`'s first run. Bind the
text to a named `const std::string` on the line before the loop, as `run_lab` does with
`r.output`.
