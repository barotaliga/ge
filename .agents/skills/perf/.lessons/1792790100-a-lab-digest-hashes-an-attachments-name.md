---
title: A lab's digest holds every attachment's name, measured or not
date: 2026-10-22
scope: games/fall_line/labs/gpu_profile_lab.cpp
concepts: [labs, digest, attach, measured, gpu-labs]
---

`Lab::attach` hashes the file's name even when its bytes are `Det::measured`. A GPU lab that
returns early without an adapter must attach the same names (empty) on that path, or the rows
with no adapter and the rows with one disagree about the lab's digest in `tests/labs_digests.txt`.
The `profile` lab attaches an empty `frames.tsv` when it has no device for this reason.
