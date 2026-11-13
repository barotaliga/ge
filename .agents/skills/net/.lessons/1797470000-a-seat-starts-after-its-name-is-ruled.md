---
title: Rule a connecting player's name before the login seat exists
date: 2026-11-13
scope: games/shooters_edge/net/sequel_login.cpp, engine/server/login.cpp
concepts: [login, reserved-names, login-seat, sequel-login]
---

`login_name(..., quiet)` on a connecting player keeps the refused name in `seat.renamed`, and
`login_seat` tells it when the seat is taken. A game's name hook that takes the seat first (to sync
it) loses that notice: the seat already exists when the name is ruled, so nothing tells the guest
why they play as `player`. In the hook, only clear a seat another player held (`login_leave` when
the userid differs) and let the frame take the seat afterwards, as the classic endpoint does.
