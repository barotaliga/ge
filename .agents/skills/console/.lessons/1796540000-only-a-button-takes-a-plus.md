---
title: Only a button takes a +; tell a player the bare command
date: 2026-11-06
scope: engine/console/console.cpp, games/shooters_edge/client/args.cpp, engine/server/main.cpp
concepts: [plus-commands, button, command-line, coopgame, dispatch, add-button]
---

A `+` marks a button's press (its `-` the release); it is not how a command is typed. Players were
told to type `+coopgame 2 expert` because the command line spelled it so, and read the `+` as
part of the command. Now the console drops a `+` on a plain command or alias (`dispatch`), and
the first command on any command line needs none (`append_command_line`; the server adds one
before its own `+set` lines). When you tell a player how to start something, write the bare
command (`coopgame 2 expert`, in the console or after the binary), never `+name` unless it is a
button.
