---
title: A test that removes a file it still holds open aborts on Windows with 0xc0000409
date: 2026-10-31
scope: tests/capture_clip_test.cpp, games/shooters_edge/labs/se_weapon_stage_lab.cpp
concepts: [windows, filesystem, remove, ifstream, exception, 0xc0000409, msvc, temp-file]
---

`capture_clip_test` passed on Linux and macOS and died on the Windows row with exit code
0xc0000409 and no output. It read a temporary WebM back through an `std::ifstream` that was still
in scope when `std::filesystem::remove(path)` ran. Windows refuses to delete an open file, and
the throwing overload raised `filesystem_error`; uncaught, it ended in the MSVC runtime's fail-fast.
Read a file inside its own block so the stream closes first, and remove it with the `error_code`
overload: a temp file that cannot be removed is not worth an abort.
