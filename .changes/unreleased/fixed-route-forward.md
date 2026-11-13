---
type: Added
title: A forward with a fixed route, for clients that cannot name one
---

A forward port can send every client to one server (`engine/forward/fixed.hpp`, `shooters_edge_hops forward --server=<host:port>`), so an unmodified client that knows only a server's address plays through it. Until a client has spoken enough, the forward holds its answers to a few bytes for each byte it sent, so it amplifies nothing.
