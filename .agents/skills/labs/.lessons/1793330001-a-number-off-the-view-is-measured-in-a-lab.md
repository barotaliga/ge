---
title: A lab number worked out from a named view or the clouds is measured, or Windows cuts another digest
date: 2026-10-30
scope: games/fall_line/labs/gpu_storm_lab.cpp, games/fall_line/app/views.cpp
concepts: [lab, digest, det-measured, libm, msvc, windows, view]
---

`storm-sky` hashed where its storm cell stood and how far off each flash was. Both follow the
named view (`make_view`), and `games/fall_line/app/views.cpp` is presentation code that calls the
platform's `std::atan2`, `std::sin` and `std::cos`. MSVC's libm rounds some of those differently
from glibc and Apple's, so the Windows row cut its own digest, even on the placeholder path that
matched Linux's when forced. Linux and macOS agreed, so a forced placeholder run on Linux cannot
catch this. Hash only what the simulation alone decides (here the flashes' times, the cell's seed's
alone), and mark every number that passes through a view, a camera or the clouds `Det::measured`.
