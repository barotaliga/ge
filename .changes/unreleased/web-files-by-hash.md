---
type: Added
title: The browser keeps a hub's files by hash and a joined game follows a map change
---

Beside each content pack the hub keeps every file on its own by the SHA-256 of its bytes (`/content/f/<sha>.zst`), with the pack's list (`/content/<sha>.files`) and a map's list by name (`/content/map/<map>.files`); `ge::web::read_file_list` and `write_file_list` are the list's two sides. In a web build `ge::web::fetch_listed` takes each listed file from the browser's own store when it kept it, fetches the rest (the whole pack when the store lacks most of it), checks every file against its hash and keeps it for the next visit. Shooter's Edge's web player fetches its packs this way and, joined to a server that changes map, loads the next map's files from the hub.
