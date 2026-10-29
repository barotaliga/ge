---
type: Changed
title: Each engine module builds and registers itself
---

Every engine module now has an `engine/<module>/module.cmake` that defines its target and registers it with one `ge_module(<target> [EXPORT <name>] [DIRS ...] [SIMULATION] [OPT_IN])` call (`cmake/Modules.cmake`). The installed package's exported targets and its header set, the libraries the tests and the labs link, `tools/check-plugins.mjs`'s map from engine directory to target, and `tools/check-libm.sh`'s simulation directories are all read from that registry. Before, each was a hand-kept list, so adding a module meant editing five shared files. The package itself is unchanged: the same `ge::` targets and the same headers. A change to a module's, plugin's or game's own build file now reaches only that directory's code in `ge test --affected`, `ge digest --affected` and precheck, not every test and lab.
