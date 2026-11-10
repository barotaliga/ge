---
type: Added
title: a classic game folder laid out in one step
---

The `classicenhance` tool gains `setup <path|https-url> --basedir=<dir> [--cache=<dir>]`. It reads a classic source (the release zip, an installed folder or a pack) and writes its packs unchanged into `<dir>`'s game base folder, ready for a lab's or the client's `--game`. Given an https URL, it fetches the file once with curl into the cache's `downloads/` folder and refuses it, removing the copy, unless it is a known release. The library half is `classicenhance::lay_out_game`, beside `url_file_name`.
