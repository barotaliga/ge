---
type: Changed
title: The labs' digest baseline merges with git's union driver
---

`tests/labs_digests.txt` is marked `merge=union` in `.gitattributes`, so two branches that change different labs' rows no longer conflict. When both branches move the same lab's row, the file keeps two rows for that lab. `labs --check-digests` then fails, naming the lab and the command that keeps one row (`ge digest <id> --write`).
