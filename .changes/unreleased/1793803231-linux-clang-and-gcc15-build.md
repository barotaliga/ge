---
type: Fixed
title: The engine builds again with Linux Clang and GCC 15
---

Shooter's Edge's world frame compiles with Linux Clang (a missing `<algorithm>`), and the glTF writer no longer trips GCC 15's `-Wstringop-overflow` under `-Werror`.
