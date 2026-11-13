---
title: One Ed25519 seed also gives the X25519 secret, so one published key both checks and seals
date: 2026-11-12
scope: engine/net/crypto.cpp, engine/account/service.cpp
concepts: [ed25519, eddsa, x25519, key-exchange, monocypher, box-key, sealed-ask, recent-answers, retry]
---

Monocypher 4's `crypto_eddsa_key_pair` hashes the seed with BLAKE2b-512 (not SHA-512) and trims the
first half into the signing scalar. X25519 trims its secret the same way, so the first 32 bytes of
`BLAKE2b-512(seed)` (`net::box_secret`) is the exchange secret whose public key is
`crypto_eddsa_to_x25519(pub)` (`net::box_public_of`); `net_crypto_test` holds the pair equal. A
service therefore publishes ONE key. Two other traps: `crypto_eddsa_key_pair` WIPES the seed it is
given (`sign_keys` passes a copy), and an asker that draws a new exchange key for every ask needs no
nonce of its own: a fixed nonce for the ask and another for the answer never repeat under one key.
A retried ask must not run twice (a second account, a second code): the service keeps recent answers
by the asker's exchange key and resends the one it gave.
