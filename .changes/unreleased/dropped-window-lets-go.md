---
type: Fixed
title: A window the server drops lets the server go
---

When a server dropped the window (a kick, a timeout, the server leaving), the window kept acting as if it were still joined: it stayed on the server's last frame and could not go back to the title. It now lets the server go, and with no game of its own behind it returns to the main menu over the game's recordings.
