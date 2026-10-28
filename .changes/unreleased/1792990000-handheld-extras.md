---
type: Added
title: The handheld and the extras on the platform services
---

`ge::services::Backend` gains the store's on-screen keyboard (`show_keyboard`, answered by `text_entered`), the machine sleeping and waking (`suspended`, `resumed`), marks and modes on the store's recording timeline (`timeline_mark`, `timeline_mode`) and remote play (`invite_remote_play`, `remote_players`, `remote_player_joined`, `remote_player_left`). The simulated store opens the keyboard only on a handheld or in the big-picture mode, holds each call to a store's limits and answers server calls `offline` while the machine sleeps.
