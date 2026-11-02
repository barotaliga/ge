---
title: The shared project folder drops big files silently; a cache saved there is cut into parts
date: 2026-11-01
scope: tools/ccache-share.sh
concepts: [ccache, compiler-cache, cloud-session, shared-folder, project-files, build-time]
---

The shared compiler cache was saved as one 544 MB tar into /mnt/project-files/.ccache, and every
later thread still built from scratch: the write succeeded in the container that made it, but
the folder never carried the file to another container (its listing elsewhere showed `.ccache/`
empty). Files under about 500 MB do arrive, so the cache is now 200 MB parts behind a manifest.
Two other gaps hid it: the session-start hook never fires in a project thread whose session
starts above the checkout, and without the hook nothing installed ccache, so `ccache-share.sh`
quietly did nothing. Prove a cache from a SECOND container (a fresh thread) before calling it
shared.
