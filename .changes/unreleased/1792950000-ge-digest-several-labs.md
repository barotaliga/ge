---
type: Changed
title: ge digest takes several labs, and the labs executable refuses a second name
---

`ge digest <a> <b> [--write]` checks or rewrites the rows of every lab it names in one call, running each lab on its own. The `labs` executable used to run only the first of several names and ignore the rest, which meant a digest could be written for one lab while the user believed both were written. It now refuses with exit 2 and says to run each lab on its own or to use `--all`.
