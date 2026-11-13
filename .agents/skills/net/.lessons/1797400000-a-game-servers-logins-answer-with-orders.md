---
title: The login seam answers with orders, and the game polls its seats
date: 2026-11-13
scope: engine/server/login.cpp, games/shooters_edge/net/classic_login.cpp
concepts: [login, sv-login, login-seam, classic-login, name-rule, auth-key, user-id]
---

`engine/server/login.hpp` never touches a game's wire. It takes the seats and the typed lines and
leaves `LoginOut` orders (tell, tell everyone, name, auth, drop) for the game to carry out once a
frame. So the classic endpoint needed only two hooks: `game_command`, which already existed, and
`name_rule`. It needed no join or drop callbacks. The game compares each spawned client's user id
with the seat's every frame, and a new id means a new player.

Three traps came up:

- The name check at connect runs before the seat is synced. A slot just freed can still hold the
  previous player's login, so `name_rule` leaves the seat first when the user id differs.
- The connecting client reads nothing yet, so the refusal is kept in `LoginSeat::renamed` and told
  at `login_seat`.
- `classic_set_info(s, slot, "name", ...)` used to set only the user info string, not
  `ClassicClient::name`, so the endpoint's own lists kept the old name. It now sets both.

A registered player who joins under their own name plays under the default name until they log in.
That follows from reserved names; the line tells them to log in to use it.
