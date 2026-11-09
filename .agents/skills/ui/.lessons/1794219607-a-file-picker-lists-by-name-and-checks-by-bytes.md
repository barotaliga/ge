---
title: A file picker lists by name and checks by bytes; the client recording's plain extension is derived, never spelled
date: 2026-11-09
scope: engine/vfs/browse.hpp, games/shooters_edge/menu/demos_page.cpp, games/shooters_edge/client/demo_pack.cpp, games/shooters_edge/client/demo_check.cpp
concepts: [file-browser, demos, packed-name, check, legacy-scan, plain_extension, listed_extensions, places, digest]
---

DEMOS lists a file by its name alone (`vfs::named_as`: a game extension, plain or under the
packings `unpack_one` peels) and checks the one picked by its bytes (`client::check_demo_file`:
unpacked, read to its level's map) before anything plays: a web page saved as `x.mvd` is listed and
then refused with the reason, never started. Keep the two apart; a name never proves a format.

The clean-slate scan (`tools/agents-index.mjs`, LEGACY) refuses the client recording's plain
extension and the classic game folder's name as words anywhere outside `porting/`, in code, tests,
labs and docs alike. The profile's `demo_extensions` leaves it out for that reason; the list the
browser shows is `client::listed_extensions` (the profile's plus each format's `plain_extension`,
the client one derived from its voice form). Tests and labs name fixture files with another
extension (`.rec`) or build the name from `plain_extension`.

A page that lists the machine's own folders draws their paths and the places that exist here, so a
lab hands it fixed places (`DemosPage::set_player_places`) and relative folders under `previews/`,
or its digest moves from machine to machine.
