---
type: Added
title: ge golden diff names the part of a state digest that moved
---

A golden word of the form `name:hex,name:hex,...` is a digest's parts, and `ge golden diff` diffs it part by part (`parts.army moved in 5 rows`). Fall Line's `state_parts` hashes its state in eight parts (skiers, sleds, heli, para, crowd, wanted, army, gunfire), and every golden that records a state digest now writes its parts beside it; the digests themselves did not move.
