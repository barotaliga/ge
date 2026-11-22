---
type: Added
title: Web downloads take packed files
---

A web source (a GitHub repository or any web server) may keep each file packed: `maps/q2dm1.bsp.xz`, or `.zst`, `.bz2` or `.gz`. The first file of a connection finds the form the source uses, then every later file asks that form first and the plain file when it is missing. A server that packs a file as it sends it (gzip, zstd, xz or bzip2) is understood too. Each file is unpacked and checked before it is kept, and one that does not unpack is said and left lacking. The game folder being played is added to a repository's address automatically; `{game}` or a `#` at the end sets the place by hand for a repository laid out otherwise.
