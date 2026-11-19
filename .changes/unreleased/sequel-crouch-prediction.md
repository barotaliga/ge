---
type: Added
title: Ducking predicted on sequel servers, and the client's version sent
---

On a server of the sequel, ducking now lowers the view the moment the key goes down instead of a round trip later (`cl_predict_crouch`: 0 off, 1 on our servers, the default, 2 on every server). The sequel client also sends its name and version in its user info.
