---
title: A page that takes a password refuses plain HTTP before it reads the form
date: 2026-11-12
scope: engine/hub/account_page.cpp, engine/hub/http.cpp
concepts: [hub, account-page, form, post, https, x-forwarded-proto, x-forwarded-for, throttle]
---

The hub speaks plain HTTP and leaves TLS to the operator's reverse proxy, so the Account page
cannot see TLS itself. It trusts `X-Forwarded-Proto: https`, or a connection from the loopback. A
client that forges the header over plain HTTP exposes only its own password, so the header is
enough to keep the page from inviting one in the clear. Behind a proxy every connection comes from
the proxy's address, so the throttle must count by `X-Forwarded-For` when the peer is the
loopback. Otherwise one player's wrong passwords hold everyone.

In a test, a wrong password holds both the address and the account, so the next right try in the
same second is refused. Give it another address and a later time. The HTTP head's new headers
(`Content-Length`, the two forwarded ones) are parsed in their own block in `http_parse`'s header
loop, as the hub's owner asked, so its parallel header changes merge cleanly.
