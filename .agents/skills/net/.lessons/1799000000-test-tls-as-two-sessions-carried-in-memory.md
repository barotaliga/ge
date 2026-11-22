---
title: Test TLS as a client and a server session carried to each other in memory, then once on loopback
date: 2026-11-22
scope: engine/net/tls_stream.cpp
concepts: [tls, mbedtls, https, self-signed, connect-tunnel, http-fetch, loopback]
---

`tls_stream` never owns a socket, so its test needs none: a client and a `tls_server` on a
`tls_self_signed` certificate, each step's `tls_take_wire` handed to the other's `tls_put_wire`
until both are open. Trust, another name and the insecure setting are each one more pair; a
failed handshake shows as `TlsState::failed` and the player's words in `tls_error`.

`https` in `HttpFetch` is then one loopback test whose server wraps each accepted stream in a
`tls_server` (and, as a proxy, answers `CONNECT` in plain text before it). Check the plain
request the server read, not only the body: the `Host` line drops the port only when it is 443.

Mbed TLS 1.3 needs `psa_crypto_init()` once before the first handshake; `mbedtls_ssl_read` may
return `MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET`, which is not an error: read again. A plain
HTTP server on a secure address answers the hello with text, which reads as "SSL connect
error" (the web downloads turn off and hand the files to the channel). A live check through the
container's proxy (its `CONNECT`, `SSL_CERT_FILE` its bundle) fetched a raw GitHub file whole.
