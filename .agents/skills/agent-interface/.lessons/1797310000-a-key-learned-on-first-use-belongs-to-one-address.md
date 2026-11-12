---
title: A key learned on first use belongs to the address it was learned from
date: 2026-11-12
scope: engine/gamecli/account.cpp, tests/gamecli_account_test.cpp
concepts: [gamecli, account, trust-on-first-use, master-key, account-file, state]
---

The game CLI's `account` group keeps the service's address, its key and the last ticket in
`<state>/account.txt`. A key kept there is checked only against the same address: naming another
`--master` starts the file afresh (a new key, no ticket). Otherwise a player who moves to another
community's master is refused as if the first had changed its key. The test for a changed key
therefore rewrites the kept address to the second service's, rather than passing `--master`. A CLI
command that needs a live service runs one on the loopback in a thread (`service_update` on a real
UDP socket at port 0, its port read from `Transport::local`); the gamecli tests already do this
for a stand-in game server.
