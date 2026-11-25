---
type: Fixed
title: Quake II's status bar keeps its own copy of the crosshair's settings
---
Quake II's status bar remembered the crosshair settings from its last draw by pointer, and the next frame's scope picture read them back. A caller whose settings did not outlive the draw left the bar reading freed memory. The bar now keeps a copy of the settings. The `se-client` lab can also film a shot (`--fire-film`): the flash, the kick and the spent case thrown and landing.
