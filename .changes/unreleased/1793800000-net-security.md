---
type: Added
title: Connect tokens, sealed packets and flood guards
---

Multiplayer security (`docs/networking.md` § Security): connect tokens a matchmaker seals under a key derived per host (`engine/net/token.hpp`), every packet after the handshake sealed with XChaCha20-Poly1305 under the token's session keys with a replay window (`NodeSecurity`, `node_connect_secure`), flood guards on handshakes and on each peer's packets (`NodeGuards`, `default_guards()`), keys read from the environment (`key_from_env`), Monocypher for the crypto, and `net-link --secure=1` measuring a host under forged and stolen tokens, replays, forgeries and floods. Off by default: a plain node's bytes are unchanged.
