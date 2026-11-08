---
title: A dodge can only pay against an eye that reads a change of course late; and the dodger's own aim must allow for his strafe
date: 2026-11-08
scope: plugins/shooter/senses.cpp, plugins/shooter/bot.cpp, plugins/shooter/features.hpp, games/shooters_edge/bots/footwork.cpp, games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [dodge, strafe, aim, velocity, course, eye, selfaim, own-motion, accuracy, head-to-head, water, wet, se-people]
---

The dodge lost head to head (one side without it won about 35 frags a six-seed set) and the reason
was not the dodge. The aim led on the velocity the server sends, so a strafer's turn was read the
instant it happened: no side step can fool that, while a person reads it a reaction late. And the
aim did not allow for the shooter's own motion, so the dodger's hands chased the turn its own
strafe made: its accuracy fell (34.0 % without the dodge, 32.4 with it) by about what it gained
in defence. With the course read a reaction late (`eye`, `Contact::course`) and the view worked
out from the eye `lead_s` ahead along its own velocity (`selfaim`), the dodge became neutral
(+2 against -2) and accuracy rose 31.7 -> 35.9 %. Before changing a behaviour that "does not work",
ask whether the opponent model can be affected by it at all: a bot judged only against bots can
look useless at what fools a person.

Measure the feet before the keys. `se-people` now reads, alike for people and bots, the speed
across the line to an enemy faced (`fight-across`: people 174, bots 94, 85 without the dodge) and
the time waist deep (`wet`: people 9 %, bots 37 %). The dodge ran in under a quarter of the fight,
blocked mostly by water, so its length or press could not move much (a people-shaped dodge was
neutral). The water is its own gap: bots wade along pool floors fully under at half a person's
speed; pricing water in the route search, a level full wish and a rise to the surface all failed
(the wet time is the pools' routes and their turns, not the keys).
