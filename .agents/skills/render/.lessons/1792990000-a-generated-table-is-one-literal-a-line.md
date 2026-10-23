---
title: A generated data table is an array of one literal a line, never one long literal
date: 2026-10-23
scope: engine/render/star_data.cpp
concepts: [generated-table, string-literal, clang, msvc, overlength-strings, base64]
---

GCC built the star table as one concatenated string literal of 72,768 characters; Clang's
`-Woverlength-strings` (an error under the tree's warnings) rejects any literal over 65,536,
and MSVC caps one piece at 16 KB. Adjacent literals concatenate into one, so splitting the
text across lines does not help. Generate a `nullptr`-ended array of short literals, one a line,
and join them where the table is decoded. A local GCC precheck does not catch this; only CI's
Clang and macOS rows do.
