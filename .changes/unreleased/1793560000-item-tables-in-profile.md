---
type: Changed
title: the bots read a game's items, call words and sounds from its profile
---

The items a map holds (by class name, with their amounts and respawn times), the words a teammate uses for each one, and the sounds a bot listens for are now one set of tables in the classic game's profile (`games/shooters_edge/profiles/items.hpp`) instead of code in the bots. A second classic game gives its own tables, and the same bots play it. The bots play exactly as before.
