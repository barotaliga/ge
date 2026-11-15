---
type: Added
title: Downloaded demos open from Dzip, LHA and bzip2 archives
---

Demos shared as `.dz` (the speed runs' archive), `.lzh` or `.lha`, and `.bz2` or `.tar.bz2` now
play like the recording inside them, and the demos page lists them. A Dzip archive's demo is
written back exactly as it was recorded and checked against the archive's checksum; zip archives
whose entries are packed with bzip2 open too. The LHA reader that found the classic pack inside
its installer moved into the engine's file layer and now reads every header level and the
larger-window methods.
