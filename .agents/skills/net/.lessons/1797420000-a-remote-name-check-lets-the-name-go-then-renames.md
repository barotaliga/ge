---
title: A name checked by a master elsewhere is let go, then taken back by an order
date: 2026-11-13
scope: engine/server/login.cpp, games/shooters_edge/net/classic_login.cpp
concepts: [reserved-names, name-check, unname, login-seam, account-master]
---

`login_name` must answer now, because the game is renaming or connecting a client this frame.
The master's answer comes later. So with a master elsewhere, an unknown name is let go and an
`Op::name` ask starts. The seat keeps the name it was let go by in `LoginSeat::shown`, kept across
`login_seat` like `renamed`. When the master says an account holds it, the seam leaves an `unname`
order, and the game picks its default name.

A seat that is still connecting cannot read a line yet. Its refusal goes to `renamed` and is told
at `login_seat`, the same path the own-master check uses.

Answers are cached by folded key: a name is asked once while the server runs. An unanswered ask is
dropped, so the next use of the name asks again.
