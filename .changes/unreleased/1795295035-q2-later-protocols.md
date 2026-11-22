---
type: Added
title: Quake II's server speaks protocols 35 and 36, and the engine gains a deflater
---

Quake II's server admits clients of the later protocols 35 and 36 beside 34: the connect's tail and its refusals, `sv_maxmsglen` (`net_maxmsglen`), the newer channel with its pieces, the packed frame, the signon as one game state or streams and deflated, the batched and compact moves, user info deltas and the client's settings (no gun, no blend, no gibs, no footsteps, no prediction). The classic layer's channel speaks the newer form behind fields that default to the old one, and `engine/core/deflate.hpp` packs bytes as a raw deflate stream any reader inflates.
