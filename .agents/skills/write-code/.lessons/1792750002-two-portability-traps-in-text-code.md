---
title: Apple's libc++ has no floating-point from_chars, and MSVC finds std::distance by ADL
date: 2026-10-23
scope: engine/console/cvar.cpp, engine/console/console.cpp
concepts: [portability, from-chars, strtod, adl, msvc, macos, std-distance]
---

Both built clean on Linux and broke only on CI's macOS and Windows rows. `std::from_chars` into a
`double` is a deleted overload in Apple's libc++, so a parser of decimal text uses `strtod` (correctly
rounded everywhere, in the C locale the engine never leaves) after checking the characters
itself. A local helper named `distance(std::string_view, std::string_view)` called with two
`std::string`s lets MSVC's argument-dependent lookup pick `std::distance<std::string>` and fail
inside `<xutility>`; give a helper a name no standard algorithm has (`edit_distance`).
