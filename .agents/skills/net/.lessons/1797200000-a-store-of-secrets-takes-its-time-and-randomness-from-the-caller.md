---
title: A store of secrets takes its clock and its random bytes from the caller, and its tests a cheap hash
date: 2026-11-12
scope: engine/account, engine/lab/labs/accounts_lab.cpp, tests/account_store_test.cpp
concepts: [account, argon2id, password-hash, salt, fresh, throttle, determinism, digest, lab]
---

`engine/account` draws no randomness and reads no clock: every call that needs a salt or a code
takes 32 fresh bytes (`Fresh`) and every check takes `now_us`, so the `accounts` lab runs a month of
sign-ups, logins and a day's brute force off one seed with one digest, in about 4 s. Each secret
keeps the cost it was hashed at (`SecretHash::kib`, `passes`), so a store's cost can rise without
breaking old hashes. Tests and the lab's store use `HashCost{8, 1}` or 64 KiB: a master's 64 MiB
Argon2id takes about 1 s a hash under the debug preset's sanitizers, and the lab times the real
costs as `measured` metrics only.
