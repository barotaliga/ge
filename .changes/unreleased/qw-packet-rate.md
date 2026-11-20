---
type: Added
title: cl_c2spps sets how many packets a second you send a QuakeWorld server
---

`cl_c2spps` (0 by default: one packet with every move; 10 to 72: that many) sends fewer packets on a slow or busy link: the moves in between ride in the next packet, so the server still runs every one. `cl_c2sImpulseBackup` (3) keeps a move with a weapon or command key from being held back.
