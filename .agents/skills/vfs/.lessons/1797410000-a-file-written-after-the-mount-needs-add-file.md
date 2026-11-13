---
title: A file written into a mounted folder is unseen until add_file indexes it
date: 2026-11-13
scope: engine/vfs/search.hpp, games/shooters_edge/client/main.cpp, games/shooters_edge/net/classic_remote.cpp
concepts: [search-path, add-file, download, game-folder, mount]
---

A directory source lists its files once, when it is mounted, so a skin or figure a client
downloads and writes into its game folder is not found by `find_file` until `vfs::add_file`
indexes it over everything else. Write the bytes first, then add the file, then redraw the level:
the remote marks `kept` and `remote_step` returns a new level once the signon has begun, so the
models are read again with the downloaded pictures. A server serving from its own folder needs no
index at all: `ServerConfig::read_file` finds the file when first asked and caches it in `files`.
