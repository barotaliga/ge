---
title: A simulated store layer reads its pads with the engine's own evaluate
date: 2026-10-28
scope: engine/services/sim_controllers.cpp
concepts: [steam-input, simulated-controllers, evaluate, store-configuration]
---

The simulated controller layer keeps the store's configuration as an `input::ActionMap` cut to the
pad bindings and reads its virtual pads with `input::evaluate`, so "the store gives the same
actions as the pad read directly" holds by construction and the `store-input` lab's gap is
exactly 0. Writing a second evaluator for the store would have needed its own shapes and edges
and would drift from the engine's.
