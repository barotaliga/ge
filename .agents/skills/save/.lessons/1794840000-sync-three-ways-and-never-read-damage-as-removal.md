---
title: Sync each slot three ways, pick a conflict's winner the same on every device, never read damage as a removal
date: 2026-11-14
scope: engine/save/cloud.cpp, engine/services/save_cloud.hpp
concepts: [cloud, sync, base, manifest, conflict, converge, removal]
---

Two sides alone cannot tell "changed here" from "changed there": keep a BASE per device (what
both held at its last sync) and compare local, cloud and base per slot. The conflict's winner must
not depend on which device syncs first, so order by play time, then date, then the checksum as a
last tie-break. A cloud file that does not open as this game's save must be set aside, not treated
as absent: absent against an unchanged local slot reads as "removed in the cloud" and deletes the
player's save. Prove convergence with random rounds (the `save-cloud` lab: three devices, edits and
removals off a seed, every device synced twice per round) rather than hand-picked cases alone.
