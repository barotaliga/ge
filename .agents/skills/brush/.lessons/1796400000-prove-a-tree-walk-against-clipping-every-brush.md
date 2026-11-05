---
title: Prove a brush trace's tree walk against clipping every brush, bit for bit
date: 2026-11-05
scope: plugins/brush/trace.hpp, plugins/brush/trace.cpp, plugins/brush/world_writer.cpp, tests/brush_trace_test.cpp
concepts: [trace, sweep, brush, leaf-brushes, tree, brush-world, golden]
---

A brush clip uses the WHOLE trace's start and end, and keeps a fraction only when it is below the trace's fraction so far. So the tree decides only which brushes a trace reaches, never the bits it reports. The test that proves the walk is a copy of the map in which every leaf lists every brush of the model and answers every contents bit: each trace through it must equal the real map's trace exactly (`the_tree_finds_what_clipping_every_brush_finds`). A missed split, a wrong offset, or a leaf that does not list a brush shows up there as a different fraction.

Build that flat copy from ONE MODEL's brushes. A brush model's brushes (a door) are not in the world's tree, so listing them in the world's leaves makes every trace near the door disagree.

`write_brush_world` needs each brush's six axial sides, because they are the planes the tree is cut along. With them, every leaf is wholly inside or outside each box brush, and a leaf's contents (taken at its centre) are exact. A slanted brush's leaves are only right at their centres.
