---
type: Added
title: The server browser's and screenshot settings players carry in their configs
---

JOIN A GAME takes `sb_hidenotempty`, `sb_hidedead`, `sb_hidehighping` with `sb_pinglimit`, `sb_info_filter` (words on the servers' own settings, such as `+map=dm` or `-*gamedir=fortress`), the `sb_show*` columns (address, game folder, frag and time limits), `sb_status`, `sb_sortservers` (column codes; LEFT and RIGHT put a column first, D turns the sort round), the query pacing settings, `sb_liveupdate` (the selected server kept current), `sb_autohide` and `sb_autoupdate`. The engine's server list gains the info filter, sorting by columns and a live update of one server. Screenshots take a name (`screenshot <name>`, never written over another) and a folder of their own (`scr_folder`, Quake's `sshot_dir`); Quake's `sshot_format` and `image_jpeg_quality_level` reach the screenshot settings; `match_auto_sshot` keeps the scores when a match ends.
