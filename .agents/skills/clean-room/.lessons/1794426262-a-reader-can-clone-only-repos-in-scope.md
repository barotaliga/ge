---
title: A reader can clone only the repositories this session's GitHub access reaches
date: 2026-11-11
scope: porting/shooters-edge/
concepts: [reader, clone, github-access, add-repo, closed-source, black-box]
---

Three readers started together; the one sent to a project outside the session's GitHub scope came
back with every question open (the clone asked for credentials, `add_repo` was refused), while the
two sent to projects other threads had already used cloned fine. Before spawning a reader, check
that its project clones here (`git ls-remote` succeeds for in-scope repositories); if it does not,
ask the person to grant access instead of spending a reader. For CLOSED source the clean room has
no reader at all: a researcher reads only public documents and inspects data files byte by byte.
Running the closed program as a black box is refused by the session's automatic permission check
when the program is downloaded from the internet, so ask the person first if a measurement needs it.
