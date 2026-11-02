---
type: Added
title: Recordings packed as they are written, and demos played out of any archive
---

`ge::vfs` opens the one file inside gzip, zstd, xz, tar (and `.tar.gz`, `.tar.xz`, `.tar.zst`), zip (stored, deflated, LZMA or zstd entries) and 7z, peeled by their first bytes (`vfs/unpack.hpp`), and packs bytes as a stream of zstd frames that unpack after every flush (`vfs/zstd.hpp`). zstd and the LZMA SDK are compiled into `ge_vfs`, so the package exports nothing more. Shooter's Edge records packed client and multiview recordings with and without voice (`.qwdz`, `.mvdz`, `.qwdvz`, `.mvdvz`) and its demo player opens any recording inside any of those wrappers.
