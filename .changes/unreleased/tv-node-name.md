---
type: Added
title: a TV node shows the match under its own name
---

A classic TV node now passes the match on under its own name: the server info's `hostname` becomes `<node> (live: <server>)`, in the signon, in every later change and in a late viewer's catch-up, and `*qtv` is set to 1. A node downstream of another replaces that node's name with its own. `TvConfig::hostname` sets the name (empty: the node's `name`).
