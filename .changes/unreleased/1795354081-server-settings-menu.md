---
type: Added
title: The server's settings in the menu
---

OPTIONS ▸ SERVER SETTINGS lists the server's map, match rules and every setting a player can change, with their values now and a page for each group (player help, movement, rules). Settings a vote decides are starred, and leaving one that was changed asks whether to call the vote. Our QuakeWorld server sends the settings over a new extension bit (`settings-sheet`, svc `ge_sheet`) and again whenever a vote changes one; stock QuakeWorld servers (with or without a match mod) and Quake II servers are read from what they already tell a client. The engine's `ServerGame::sheet` and `engine/server/sheet.hpp` let any game fill the page.
