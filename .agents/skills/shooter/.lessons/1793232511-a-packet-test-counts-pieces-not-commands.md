---
title: A packet's commands over 50 ms run as halves, so a test counts pieces, not commands
date: 2026-10-29
scope: tests/shooter_client_test.cpp, plugins/shooter/client.cpp
concepts: [packet, lost-commands, cmd-chop, split, pre-think, test]
---

`a_packet_replays_what_was_lost_before_its_newest_command` counted pre-thinks and expected one per
command, but its later packets carried 51, 52 and 62 ms commands: each runs as two halves of 25,
26 or 31 ms, with a pre-think per half, so 19 lost packets gave 22 pre-thinks, not 20. The loop
was right and the test wrong. When a test of `run_packet` or `run_player_command` counts hook
calls or reads `frametime`, keep its commands at 50 ms or under, or count the halves on purpose
and say so beside the check.
