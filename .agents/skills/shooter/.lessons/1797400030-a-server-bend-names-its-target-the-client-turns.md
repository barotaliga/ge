---
title: A server-side aim bend names its target; the client turns the view toward it, not by the bend's angle
date: 2026-11-13
scope: plugins/shooter/aim_turn.hpp, games/shooters_edge/server/shot_help.cpp, games/shooters_edge/net/classic_frame.cpp, games/shooters_edge/client/controls.cpp
concepts: [aim-help, aim-bend, view-turn, spring, ge-extension, demo, wire]
---

To make helped aim look like a mouse move on a demo, the server sends only WHO it bent onto
(`ge_aim`, ge extension `aim-bend`), and the client tracks that target with a critically damped
spring while attack is held, then springs back to the player's own aim. Sending the bend's angle
instead feeds back: once the view is on target the shots hit unbent and no further message comes,
so an angle would be stale at once. Apply the turn as the offset's CHANGE each frame on top of the
view, so the mouse keeps working and letting go returns exactly to the player's own aim. A new
first-tier ge bit changes the serverinfo's `*ge_ext`, which moves any lab digest that records it
(se-hub), and a new svc row moves the classic_messages golden.
