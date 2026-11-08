---
type: Changed
title: Precheck links only the tests a change reaches
---

`make precheck` compiles every object and links every library and program, but links only the test executables the change reaches, which are the tests it then runs. The ASan test executables of a full debug build are about 20 GB and filled a cloud container's disk. `PRECHECK_FULL_BUILD=1` builds everything as before. Precheck also fetches its base (`origin/main` by default) first, so a stale local copy no longer makes the lint read main's own files as the change's.
