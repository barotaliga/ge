---
type: Fixed
title: the proxy's copies arrive in the order the game sent them
---
A proxy now sends the copies that have fallen due before a newer datagram, so a game that drops
older packets keeps the one whose first frame was lost. `se-interop --proxy=<port>` plays the
lab's classic client through a near and a far proxy to a live server, with `--link-loss` and
`--copies`, and reports the loss the client sees and the link's bytes.
