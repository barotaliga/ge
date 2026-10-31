---
title: Read our own server's recordings back with the demo reader, not the codec that wrote them
date: 2026-10-31
scope: games/shooters_edge/net/classic_record.cpp, games/shooters_edge/net/classic_svc.cpp, games/shooters_edge/client/demo_client.cpp
concepts: [demo, recording, codec, spawnbaseline2, chunked-downloads, round-trip, reader]
---

The classic codec's round trips (`read_svc` of what `write_svc` wrote) prove only that the two
halves agree with each other. The first client recording our server made did not read in the demo
reader, which was checked against real recordings: `spawnbaseline2` had a short entity number
before the entity record (the number belongs in the record's own word), and the reader knew only
the plain download form while the signon used chunked downloads. A recording test that reads its
file back through `client/demo.hpp` to the end is the check that catches a wire form our own
client happens to share; run one whenever the server's messages change.
