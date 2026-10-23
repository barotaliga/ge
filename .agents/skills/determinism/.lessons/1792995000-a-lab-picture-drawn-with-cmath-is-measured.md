---
title: A lab draws with core's libm, and a picture that reads render's cmath is measured
date: 2026-10-23
scope: engine/lab/labs/gpu_moon_lab.cpp
concepts: [lab, digest, cmath, libm, picture, det-measured, cross-platform, libm-check]
---

`make libm-check` holds every labs/ directory to core's libm, since a lab's pictures and digest
must match on every CI row: write `libm::pow`, `libm::acos` in a lab from the start, never
`std::`. A lab that also calls render/ code (which uses the platform's `<cmath>`, like
render/night.hpp's shadow light) cannot promise those bits, so mark the pictures and metrics that
pass through it `Det::measured` and keep core's own numbers in the digest. Run a new lab's smoke
twice locally and compare the digests before trusting it on every row.
