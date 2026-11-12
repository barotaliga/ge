---
type: Added
title: player accounts: tickets and the account service
---

The account store now answers over the network (docs/accounts.md § The service on the wire). `net/crypto.hpp` gains Ed25519 signatures and X25519 key exchange (Monocypher), with one published key doing both. A master signs a 24-hour ticket (`account/ticket.hpp`) that any server checks offline with that key. The account service (`account/service.hpp`) takes sealed asks: sign up, sign in for a ticket, make a one-time code, and a game server's login by code for a classic client, plus recovery. An ask sent again gets the answer it got the first time, so nothing is made or spent twice. `account/host.hpp` runs the service on a UDP socket over the store and key kept in a folder, and the hub runs it with `--accounts=<dir>`.
