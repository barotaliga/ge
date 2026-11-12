---
type: Fixed
title: A moved lab digest on CI prints what it hashed
---

The hub-archive lab's digest baseline follows the archive's `.ged` file names, and a lab whose
digest moves on CI now prints what it hashed, so a mismatch on one platform can be compared line by
line with another's `digest.tsv`.
