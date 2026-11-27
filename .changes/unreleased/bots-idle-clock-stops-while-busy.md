---
type: Fixed
title: Campaign bots no longer give up on a reachable button because of a stale idle clock
---

A campaign bot that briefly had nothing reachable (for example, falling off a walkway) treated every door as open right away. Its "nothing to do" clock had been running since an earlier idle moment and was never reset while it had work. On Quake II's city1 it then ran to a secret door still shut and waited there instead of going for the button that opens it. The clock now runs only while the bot really has nothing to do. City1 still times out: the bot misses a running jump on the way to that button.
