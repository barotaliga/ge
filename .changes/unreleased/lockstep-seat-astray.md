---
type: Added
title: A lockstep desync names the seat astray
---

Once a lockstep peer holds every seat's digest for the step a desync was found on, it names the one seat whose digest differs from all the others' (`LockstepDesync::odd`). Honest peers then know whom to leave out. With two seats nobody can be named. The `net-lockstep` lab shows the seat astray and who named it.
