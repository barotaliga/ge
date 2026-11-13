---
type: Added
title: each classic game loaded in its own client's order
---

Each classic game ranks its files and runs its startup configs in its own client's order, one
table a game (`client/load_order.hpp`). The sequel's folders rank their packs over their loose
files, its archives sort by number then name (`.pkz` zips too), the rerelease's root archive sits
under them, and its `default.cfg` comes from its packs.
