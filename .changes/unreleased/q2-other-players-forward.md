---
type: Added
title: Other players run forward on our server of Quake II
---

A window joined to our server of Quake II now draws the other players where they are rather than where the last 10 Hz frame had them: the server sends each other player's state and last move to a client that asks (`other-moves`), and the window runs them forward by the same rule its join to a classic server uses. `cl_predict_players 0` turns it off; a stock server is unaffected.
