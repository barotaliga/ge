---
type: Added
title: the hub's Account page
---

A hub run with `--accounts` serves `/account`, where a player signs up (the recovery code is shown once), gets a one-time login code with the line a classic client types, or sets a new password with the recovery code. It uses plain HTML forms, with no script and no cookie. The page takes a password only over HTTPS (the operator's proxy sends `X-Forwarded-Proto: https`) or from the hub's own machine; otherwise its forms are shown disabled. The hub's HTTP now takes POST, but only to `/account/...`, and only a form of at most 2 KB.
