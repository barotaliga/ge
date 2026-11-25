---
type: Fixed
title: Joining a Quake II server or a TV node works when the router moves your port
---

Every classic server and TV node now hands out and checks the connect challenge the same way: one per address, the same number when you ask again, and found by your host and the number when a router moved your port between the two asks. Quake II's server used to refuse such a join with "No challenge for address.", and a TV node's table grew without bound under a flood of forged asks. Each now keeps at most 1024 at once.
