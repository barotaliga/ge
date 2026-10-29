---
title: A test that edits a file in place reads it before opening it for writing
date: 2026-10-29
scope: tests/mod_loader_test.cpp
concepts: [testing, ofstream, truncation, manifest, fixture]
---

The loader's test copies the sample package and edits its manifest to stage each refusal.
`std::ofstream(path) << edit(read(path))` truncates the file when the stream is constructed,
which can happen before `read` runs, so the edit was applied to an empty text and the test
checked the wrong refusal. Read into a string first, then open the stream and write.
