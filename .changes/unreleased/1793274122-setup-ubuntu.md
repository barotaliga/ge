---
type: Added
title: One script sets up an Ubuntu machine
---

`tools/setup-ubuntu.sh` brings Ubuntu 24.04 (or 22.04, through extra apt sources) from a minimal server install to one that builds the engine and runs its tests, gates and labs headless: it installs only what is missing or too old, returns in under a second with no network when nothing is, can keep its downloads in a cache directory for CI (`--cache-dir`), and is what CI and the cloud session's start hook now run.
