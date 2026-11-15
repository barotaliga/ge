---
type: Added
title: Filters in Shooter's Edge's server browser
---

JOIN A GAME can hide full servers (F, `sb_hidefull`) and empty ones (E, `sb_hideempty`), show one mode (M, `sb_mode`) and drop servers over a ping (P, `sb_maxping`: 50, 100, 150 or 250 ms). Each filter is kept with the config; the title row says which are on and how many servers they hide. `ge::ui::MenuInput` gains `letter`, a letter key a page reads as its own.
