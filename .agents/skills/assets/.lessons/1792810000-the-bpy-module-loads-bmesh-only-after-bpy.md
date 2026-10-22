---
title: In the bpy module (pip install bpy) bmesh is importable only after bpy, and Blender ignores PYTHONPATH unless told
date: 2026-10-22
scope: engine/assets/blender/ge_asset.py, engine/cli/asset.cpp
concepts: [blender, bpy, builder, ge-asset, pythonpath]
---

A builder run as a plain Python with the bpy module failed at `import bmesh` with "No module
named 'bmesh'" when the library imported bmesh before bpy: the module registers bmesh, mathutils
and the rest only when bpy itself is loaded, so `import bpy` goes first. Blender proper is the
other way round: it ignores PYTHONPATH unless started with `--python-use-system-env`, which is
how `ge asset` puts ge_asset.py on a builder's path in both. A third trap: piping a data set
into a file with a shell `>` inside a command ge already redirects to its log sends the JSON to
the log; capture the output and write the file instead. With the weights set by hand and the
same data, Blender 5.2.2 exports the same .glb bytes twice, so a rebuild with nothing changed
shows no diff.
