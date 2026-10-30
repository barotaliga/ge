---
type: Changed
title: Builds compile through ccache and link with LLD when they are installed
---

`cmake/CompilerCache.cmake` makes ccache (or sccache) the compiler launcher whenever one is on PATH, so a rebuild of anything built before takes its objects from the cache; `-DGE_CCACHE=OFF` turns it off and a launcher already set wins. With MSVC the debug configurations embed their debug information (/Z7) so the cache can store them. On Linux every link goes through mold or LLD when one is installed (`cmake/Linker.cmake`, `-DGE_FAST_LINKER=OFF` to keep the default). CI's Windows row and the packaging jobs now compile through ccache too, and `tools/ccache-share.sh` carries a cloud container's cache to the next one.
