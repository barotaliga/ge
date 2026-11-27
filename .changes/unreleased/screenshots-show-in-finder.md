---
type: Added
title: The screenshots page shows a picture in Finder, Explorer or the desktop's file manager
---

On Shooter's Edge's screenshots page, O (or a click on SHOW IN FINDER at the footer's right; EXPLORER on Windows, FILES on Linux) opens the system's file manager on the screenshots folder with the selected picture highlighted, and an empty folder's O opens the folder. Any game gets it from the engine's console command `reveal <path>` (`ge::platform::add_reveal_command`), which runs `open -R` on macOS, `explorer /select,` on Windows and the freedesktop file manager's ShowItems (or `xdg-open` on the folder) on Linux.
