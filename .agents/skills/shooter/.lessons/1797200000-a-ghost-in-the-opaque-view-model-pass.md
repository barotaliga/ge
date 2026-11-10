---
title: A see-through held weapon is a moving screen door, and a lab must number its redraws
date: 2026-11-10
scope: engine/render/viewmodel.wgsl, plugins/shooter/quick_weapon.hpp, games/shooters_edge/labs/gpu_se_quickweapon_lab.cpp
concepts: [quick-weapon, ghost-weapon, opacity, screen-door, dither, taa, view-model, lab-redraw]
---

The view-model pass is opaque (colour plus motion for the TAA-U), so `HeldDraw::opacity` below 1
discards pixels by interleaved gradient noise offset by the frame number (`frame.time.y`), and the
history averages it to a see-through gun. A lab that redraws one moment several times to settle
the TAA must give each redraw its own `FrameView::frame`, or the pattern stays put and shows as a
checker. A ghost alone in a dark classic room reads as a dark patch: it takes a faint cool light of
its own (`kGhostGlow`). The quick weapon's tap is judged by the server's ammunition falling, never
by "the weapon came into hand": the same command switches and fires, so the first state the client
sees with the weapon in hand may already show the shot spent.
