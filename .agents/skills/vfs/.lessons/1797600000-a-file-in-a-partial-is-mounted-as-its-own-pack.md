---
title: A download kept in a partial pack is mounted as a one-file pack, not by remounting the partial
date: 2026-11-18
scope: engine/vfs/partial.hpp, plugins/classic/content/downloads.hpp
concepts: [partial-pack, download, mount, add-archive, origin]
---

A file put into `pak1.partial.pak` is used at once by mounting a pack of that file alone
(`write_pak({{path, bytes}})` then `add_archive`), never by mounting the partial again: the
partial grows with every file, and mounting it each time would copy it whole into memory once a
download. At the next start `add_game` mounts the partial like any other `*.pak`. A server that is
itself filling a partial reports no origin for its files (`archive_origin` refuses a
`.partial.` name), or a client would start `pak1.partial.partial.pak`.
