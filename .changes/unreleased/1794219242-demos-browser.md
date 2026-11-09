---
type: Added
title: DEMOS in Shooter's Edge's main menu
---

The main menu gains DEMOS (`menu_demos`): browse any folder on the machine (the game's folder, home, Downloads, Desktop, Documents, the drives) for recorded games, plain or packed (`.mvd.gz`, `.dem.zip`, `.mvd.7z`, `.mvd.tar.gz`, `.mvd.xz`, `.mvdz`); an archive is listed only when a recording's extension stands before its packing. A picked file is checked (unpacked and read to its level) before it plays, and refused with the reason when it is no demo. The engine's folder browser (`ge::vfs::FolderBrowser`, `vfs/browse.hpp`) does the listing for any game.
