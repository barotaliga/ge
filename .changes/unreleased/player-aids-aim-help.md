---
type: Added
title: Player aids and the aim help
---

A server can give one chosen player help nobody else gets, in every game on the engine (`engine/server/aids.hpp`): the players grant it by a vote (`vote aimhelp <player> on`) or the administrator does (`aimhelp <player> on`), it is disclosed in the player's info, everyone is told, a player who joins is told every grant, and `users` lists each player's aids, allowed and used. Shooter's Edge's first new aid is the aim help: a granted player's hitscan shot that just misses another player (within `sv_aimhelp_x` units sideways and `sv_aimhelp_y` up and down) is bent onto him, held fire included, and a rocket, grenade or nail is led to where a player it would just miss will be, in the classic game and its sequel. `sv_aimhelp_allow` and `sv_aimhelp_deny` name the weapons it may bend. The easy jump moved onto the same grants.
