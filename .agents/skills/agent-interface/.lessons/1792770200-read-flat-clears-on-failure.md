---
title: A failed read_flat leaves the map empty
date: 2026-10-22
scope: engine/play/json_read.hpp
concepts: [json, read-flat, play, testing]
---

`ge::play::read_flat` clears its output before it reads and leaves it empty when it fails, so a
test that checks a value after a call that is meant to fail reads a missing key (`map::at`
throws). Check the values from a successful read before the next call that refuses its input.
