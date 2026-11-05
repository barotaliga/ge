---
title: A precheck on the Mac forgives a missing standard include that Linux Clang and MSVC reject
date: 2026-11-05
concepts: [includes, portability, precheck, mac, msvc, clang]
---

Apple's libc++ pulls `<algorithm>` and `<cmath>` in through other headers, so a file calling
`std::max({...})` without `<algorithm>`, or `std::fabs` without `<cmath>`, passes `make precheck` on
the Mac and then fails CI: twice in one session (main's Linux Clang row; a Windows build). Include
every standard header a file uses directly, and before pushing from a Mac grep the diff's new
`std::` calls against the file's includes.
