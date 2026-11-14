---
title: QOI's seen-pixel table starts all zero, alpha too, and differences wrap
date: 2026-11-14
scope: engine/save/qoi.cpp, engine/save/picture.cpp, engine/save/store.cpp
concepts: [qoi, picture, pict, index, prefix-read, checksum]
---

The previous pixel starts as (0, 0, 0, 255) but the 64-entry table of seen pixels starts as
(0, 0, 0, 0): give the table its own zero fill, or a file another QOI tool wrote decodes wrong at
the first index op. Differences are taken on the bytes and wrapped to signed, so pure red after
the start pixel is a DIFF op (r = 0 - 1), not an RGB op: test the tiny file's exact bytes. For a
menu to read only a file's front, the picture chunk must be written first after the meta and its
place (offset, stored bytes, checksum) kept in the index; verify the prefix's bytes against that
checksum and fall back to the whole file when the index is stale.
