---
title: Place a Quake II move at its packet's arrival, and step the server on its tick
date: 2026-11-23
scope: games/shooters_edge/net/q2_weapon.cpp
concepts: [weapon-prediction, q2, frame-phase, move-timing, press-latch, kTickUs, server-frame]
---

A Quake II window sends a packet about every 48 ms with three moves in it, so the server runs a
packet's moves together when it arrives, not spread by their msec: a prediction that sums msec
from the frame's start puts shots in the wrong server frame. Time each move by its packet's send
time less an offset, and learn the offset from the frames (`Q2FramePhase`: a frame's start fell
after the packet it acks arrived and before the next one; narrow that window frame by frame).
A stock server clears a latched press at every frame start, so a tap during a shot is lost; do
not keep the press until a shot. In a lab or test, step the q2 net server on its host tick
(`cn::kTickUs`), never once per window frame, or the server runs twice as fast as it should.
Dedupe a predicted shot by the server frame it falls in: one step a frame, so one shot.
