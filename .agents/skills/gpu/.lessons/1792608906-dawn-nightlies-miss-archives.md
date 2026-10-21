---
title: Not every Dawn release tag has the prebuilt archives; probe all three before pinning one
date: 2026-10-21
scope: cmake/Dawn.cmake
concepts: [dawn, prebuilt, pin, release, download]
---

Dawn tags a release most weekdays, but a tag whose build failed has no
`Dawn-<commit>-<os>-Release.tar.gz` assets (the download is a 404), and two tags can share
one commit. Before moving `GE_DAWN_TAG`, list the tags with `git ls-remote --tags
https://github.com/google/dawn` and probe each archive URL
(`curl -sSL -o /dev/null -w '%{http_code}' .../releases/download/<tag>/Dawn-<commit>-ubuntu-latest-Release.tar.gz`,
and the macos and windows ones); pin a tag where all three answer 200. The container's proxy
serves the release downloads but not the HTML release pages or the GitHub API for that repo.
