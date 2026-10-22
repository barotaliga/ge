---
title: Bake real-world data in a lab, commit it as generated sources, and prove the bake reproduces
date: 2026-10-22
scope: plugins/mountain/labs/real_faces_lab.cpp
concepts: [real-face, bake, elevation-model, generated, geotiff, inflate]
---

The real faces' heights come off a public dataset (a 30 m elevation model in a public bucket,
no credentials). The bake is a lab (`real-faces --bake --fetch --write`): it fetches with `curl`
into a gitignored cache, reads the tiles with the labs' own GeoTIFF reader and inflater (no
compression dependency), and writes GENERATED C++ sources the build compiles, so no build reads a
file or a network. Write the generated file exactly as `clang-format` leaves it (run the bake,
then `clang-format <file> | diff` until it is stable), or `make fmt-check` fails every face. Then
show the bake reproduces: a re-run moves no sample (`samples moved` 0), and the golden test holds a
hash of every face's decoded heights.
