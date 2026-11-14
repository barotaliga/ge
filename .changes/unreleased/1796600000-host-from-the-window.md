---
type: Added
title: Others can join the match you play in the window
---

The window's match now runs on the same server as `sesv`, with you in its first seat. Set `sv_port 27500` in the console before `map <name>`, and other players join over UDP with `connect <your-address>:27500`, the way they would join a dedicated server. They see you as a player, the bots and the lobby work the same for them, and `sv_maxclients` sets the slots (32). With `sv_port 0`, the default, the match stays yours alone.
