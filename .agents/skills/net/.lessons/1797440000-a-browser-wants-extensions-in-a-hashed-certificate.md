---
title: A browser refuses a hashed WebTransport certificate with no extensions
date: 2026-11-13
concepts: [webtransport, certificate, server-certificate-hashes, chromium, x509, quic]
---

`engine/webtransport`'s first certificate was a minimal X.509 v3 (P-256, 13 days, no extensions):
openssl verified it and our own client took it, but headless Chromium failed the handshake with
`QUIC_TLS_CERTIFICATE_UNKNOWN` (`CERTIFICATE_VERIFY_FAILED`) even though the page named its hash.
Adding one extension (subjectAltName, the name as a DNS name) made it open the session. When a
browser refuses a hashed certificate, check the three things it holds one to before the hash:
at most 14 days, a P-256 key, and extensions present. The page and driver used are in M56i2a's PR.
