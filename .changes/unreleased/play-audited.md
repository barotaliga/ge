---
type: Added
title: Runs are checked by playing them back to their digest
---

`play::play_audited` plays an input log on a fresh run and returns the run only if it reaches the digest the log records. A log edited after recording, or one with no digest, is refused. Fall Line's hub reads its runs this way, so a forged ghost never reaches the archive, and every time it shows is read off the run played out.
