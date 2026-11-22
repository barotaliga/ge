---
type: Changed
title: The campaign bot plans its whole route through a map
---

The bot that plays Quake's campaign at 100% now works out the quickest order through every
button, secret and monster and then the exit, the way a speed runner plans a route, instead of
going to the nearest thing each time. It backs off from monsters that bite or leap, leaves what
it cannot reach instead of standing at it, retries what it left when nothing else is left, and
drops through floors that slide away.
