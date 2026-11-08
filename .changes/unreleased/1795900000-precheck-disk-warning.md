---
type: Changed
title: Precheck warns before a build can fill the disk
---

`make precheck` checks the free space under the build directory before it builds. Under 20 GB (`PRECHECK_MIN_FREE_GB`) it warns and names the safe cleanup: the test executables, which are linked again as needed. When the build fails on a full disk, it says that the disk was the cause.
