---
title: A dependency with no release archive is cloned at a pinned commit, and its OpenSSL glue mapped
date: 2026-11-13
concepts: [dependency, git, picotls, ngtcp2, quic, minicrypto, cloud-sandbox]
---

picotls publishes no releases, and a cloud session's proxy refuses GitHub's source archives
(codeload) while `git clone` of a public repository works, so `cmake/Quic.cmake` clones it into
GE_DEPS_DIR at a full commit hash (a stamp file marks it done) and downloads ngtcp2's release
archive by SHA-256. ngtcp2's picotls glue names picotls's OpenSSL backend; a generated
`picotls/openssl.h` maps those names onto minicrypto's, so the glue builds unchanged with no
OpenSSL anywhere. Include such libraries' headers with `SYSTEM` (the tree builds `-Wpedantic`).
