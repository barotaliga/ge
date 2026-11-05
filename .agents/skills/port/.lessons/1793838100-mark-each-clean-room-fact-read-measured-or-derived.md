---
title: In a clean-room note, mark each fact read, measured or derived, and re-check the derived ones
date: 2026-11-05
scope: porting/shooters-edge
concepts: [clean-room, research-notes, provenance, sequel, demo-pack]
---

The sequel's notes (M18t2) mixed three kinds of fact: read in the GPL source, measured by a
scratch script on the demo pack's bytes, and derived (a weapon's period worked out from its frame
numbers). A second pass over the source just to check the notes caught a dozen errors, and every
one of them was in a derived or summarised fact. None was in a fact read directly. Examples were
skin counts, a monster's self-heal that does not exist, the trap's defaults, and a pain debounce.
So mark each fact on its page as read, measured or to verify. Before the commit, re-read the
source for every derived line. Keep the scratch scripts that measured the pack outside the tree.
The demo installer was unreachable on archive.org (503) the day of this session. An ftp mirror
served it, and its sha256 in `sequel.md` is what identifies it.
