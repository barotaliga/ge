---
type: Added
title: the sequel's recordings pack into the replay file and back exactly
---

Shooter's Edge's replay-file packer (`pack_demo`) now takes the classic sequel's client recordings (`.dm2`) as well: each is walked message by message and written to a replay file (`.ged`) through the field codec, and unpacking gives back the very same bytes. The demo pack's own recording packs to 29% of its size, a quarter smaller than zstd at level 19. A block the walk does not know is kept as it is, so every recording comes back. The `se-sequel-pack` lab shows the sizes, on a made-up recording or, with `--pak`, on the demo's own.
