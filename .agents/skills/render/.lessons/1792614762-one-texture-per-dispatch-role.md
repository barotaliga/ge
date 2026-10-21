---
title: A compute pass that writes a table cannot also bind it for reading; give writers their own group 0
date: 2026-10-21
scope: engine/render/sky.cpp
concepts: [bind-group, storage-texture, usage-scope, compute, sky-tables]
---

Within one dispatch a texture may be a storage target or sampled, never both, and the check
covers every binding of every bound group, used or not. The draw passes' group 0
(`frame_layout`) binds every sky table for sampling, so the passes that BUILD those tables use
their own group 0 (`sky.in`: the frame block, the sampler and two tables to read, a blank 1x1
texture where a pass reads fewer) and one output in group 1. Separate dispatches in one compute
pass are separate usage scopes, so the sky view and the sky's light (which samples it) can sit
in the same pass.
