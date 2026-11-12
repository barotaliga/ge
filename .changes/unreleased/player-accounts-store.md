---
type: Added
title: player accounts: the store a master keeps
---

A new module, `ge::account` (`engine/account`), keeps the player accounts a community's master holds (docs/accounts.md): sign-up with a name reserved however it is spelled (capitals, digits for look-alike letters, separators, a game's coloured text), passwords hashed with Argon2id (`net::password_hash`), one-time login codes and an optional game password for the classic clients' plain-text console login (which never takes the account's password), a recovery code in place of e-mail, a throttle that holds an account and an address after wrong secrets (doubling up to 15 minutes, never a block for good), blocking and removal, and the store's log and snapshot as text. The `accounts` lab runs a month of sign-ups, logins and a brute force.
