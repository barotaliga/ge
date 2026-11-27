---
type: Fixed
title: A shot explosive in Quake II fires all of its targets, and campaign bots get through hangar1's duct
---

When a breakable (`func_explosive`) was shot, it fired only the first of its targets and then stopped, because it already counted as removed. On hangar1 the power box's other targets include the relay that removes the duct's push, so the push kept blowing into the fan forever. Every target now fires, as it does for an item taken or anything else removed by its own use. The campaign bot also learned the power box: it shoots it before going down the duct. A door that opens when shot no longer looks open to the bot from the start. The 100% run on hangar1 used to spend ten of its fifteen minutes pinned in the duct; it now gets through to the far side.
