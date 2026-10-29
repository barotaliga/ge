---
title: The cloud container's proxy refuses plain-HTTP apt mirrors; use the HTTPS ones
date: 2026-10-29
scope: tools/setup-ubuntu.sh, tools/session-start.sh
concepts: [session-start, apt, proxy, https, container, setup-ubuntu]
---

The container's sources point at `http://archive.ubuntu.com` and `http://security.ubuntu.com`,
and the session's proxy takes only HTTPS (CONNECT): `apt-get update` fails with `405 Method Not
Allowed`, so a hook that installed packages "best effort" silently installed none for days
(SDL then builds without its X11 extensions and audio backends). `tools/setup-ubuntu.sh
--https-mirrors` rewrites those sources to `https://` (both archives serve HTTPS), and the hook
passes it. To test the script in a fresh `docker run ubuntu:24.04` here: start `dockerd`, run
with `--network host`, point apt at the proxy (`Acquire::https::Proxy`), switch the sources to
https, and copy `/root/.ccr/ca-bundle.crt` both to `/etc/ssl/certs/ca-certificates.crt` (for the
first `apt-get update`) and to `/usr/local/share/ca-certificates/` (so installing
ca-certificates keeps it). Docker Hub rate-limits pulls; `mirror.gcr.io/library/ubuntu:22.04`
serves the same images.
