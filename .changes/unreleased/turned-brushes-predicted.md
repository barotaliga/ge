---
type: Fixed
title: A Quake II client predicts its movement against turned doors and fans
---

A Quake II client used to predict its own movement against a turned door or fan as if it stood upright, so the player could stutter or snap back at a hinged door or a spinning fan. The prediction now turns the brush as the server traces it. The server's trace and the client's prediction share one turned frame, the frame the picture turns a brush model in.
