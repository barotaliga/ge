---
type: Added
title: The skeletal model file (.iqm) read into the engine's Model and written back
---

`engine/assets/iqm.hpp` reads a skeletal model file (`.iqm`, version 2: joints, meshes, blend weights, animations) into `ge::assets::Model` in the engine's frame, so it poses and skins as an authored glTF model does, and `write_iqm` writes a `Model` back. The `models` lab takes `--format=iqm` (the test model through the file) and `--model=<file>.iqm --metres=<unit>`.
