---
type: Fixed
title: The LZMA SDK downloads from 7-Zip's GitHub release
---

A build now fetches the LZMA SDK (the 7z reader's codec) from 7-Zip's own GitHub release instead of its website, with the same pinned SHA-256, so a configure no longer fails when that one site does not resolve.
