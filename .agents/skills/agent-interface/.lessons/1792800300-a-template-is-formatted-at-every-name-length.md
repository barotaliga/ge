---
title: A template must be clang-formatted at every name length it is filled with
date: 2026-10-22
scope: scaffold/, templates/
concepts: [scaffold, template, clang-format, placeholder, ge-check]
---

`ge check` fails a new repo whose files are not clang-formatted, and a `{{ident}}` or `{{name}}`
changes a line's length, so a template that formats for one name can fail for another: a braced
list bin-packs differently, a call joins or splits. Keep placeholders out of long lines (a
`namespace sim = {{ident}}::sim;` alias, designated initializers one per line, no name in a
summary string), then check with `ge new` at a short, the CI's and a long name and
`clang-format --dry-run` over every `.cpp` and `.hpp` it wrote.
