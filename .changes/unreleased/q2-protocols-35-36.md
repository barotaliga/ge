---
type: Added
title: Quake II's client joins servers on protocols 35 and 36
---

Quake II's client speaks protocols 35 and 36 beside 34: `cl_protocol`, `net_maxmsglen`, `net_chantype` and the batched moves (`cl_batchcmds`, `cl_packetdup`, `cl_maxpackets`, `cl_instantpacket`). Compressed packets and streamed downloads are read, and demos are still recorded as protocol 34.
