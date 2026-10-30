---
type: Added
title: ge queue and ge pr clash show the merge queue and a real merge onto it
---

`ge queue` lists the merge queue in the order it merges (each pull request's last queue event, then the armed ones not yet in it), every open pull request that conflicts with main by a real `git merge-tree`, and the failed queue group runs with their failing jobs. `ge pr clash <n>` merges one pull request onto main and onto everything queued ahead of it, naming the conflicting files and the pull request ahead that brings them in.
