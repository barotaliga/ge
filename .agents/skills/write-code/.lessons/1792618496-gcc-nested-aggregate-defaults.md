---
title: GCC warns on a designated initializer that skips a nested aggregate member with no default
date: 2026-10-21
scope: engine
concepts: [warnings, designated-initializers, aggregate]
---

With `-Wextra -Werror`, GCC flags `{.kind = ..., .level = ...}` as
`-Wmissing-field-initializers` whenever a skipped member is itself an aggregate written
`Filter filter;`. Give such members an explicit default, `Filter filter{};`, and the
warning goes; Clang does not warn either way, so a Clang-only build hides it.
