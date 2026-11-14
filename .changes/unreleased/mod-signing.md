---
type: Added
title: Mod packages can be signed
---

A mod package can carry `mod.sig`, its author's signature over the manifest (which holds every file's SHA-256). The loader refuses a package whose signature does not hold, names the signer from the game's trusted keys, and refuses anything unsigned or signed by an unknown key when the game asks for trusted packages only. `ge mod key` makes a signing key, `ge mod sign` signs a package, and `ge mod check --trusted=<file>` names who signed it.
