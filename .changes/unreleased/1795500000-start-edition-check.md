---
type: Fixed
title: Shooter's Edge no longer reads every installed game's packs to list them
---

The game list's check of each install's edition (shareware or full) read every pack of the install whole on the main thread, hundreds of megabytes at start and again when the list refreshed, which stalled the window for seconds on a machine with several games installed. It now reads the packs' directories only (`vfs::game_has_file`). The `gpustats` overlay also reports during demo playback.
