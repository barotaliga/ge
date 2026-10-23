---
type: Added
title: Motion vectors for skinned meshes
---

A skinned mesh's vertices now carry their own motion into the motion target, so TAA keeps a swinging limb sharp instead of smearing it. The `skinning` pass keeps each vertex's last frame's position beside the family's vertices before writing the new one, and the instances' colour pass reads it. `settle_skin` starts a skin with no motion after a cut or a still, and `read_skin_last` reads the kept positions back for tests. Fall Line's frame settles its skins on a cut, on a settled scene and for an undrawn slot. The `taa` lab gains `--scenario=<ride>`, which chases a ride scenario's skinned skier.
