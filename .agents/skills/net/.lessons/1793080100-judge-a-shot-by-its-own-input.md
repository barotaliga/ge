---
title: Judge a shot by the moment its own input was drawn, not by the message that carried it
date: 2026-10-24
scope: engine/net/replicate_server.cpp, engine/net/replicate_client.cpp, engine/net/replicate_trial.cpp
concepts: [lag-compensation, rewind, view-tick, input, shots]
---

The input message used to carry one view tick: the moment its newest input was drawn. The
server applies inputs from a queue, so the input that fired was usually several ticks older
than the message it arrived in. Judged by the message's view, a shot at a weaving crowd landed
17 to 35 cm off at the 95th percentile from `broadband` to `mobile`. Judged by its own drawn
moment, it landed under 1 cm off. The cost is a u16 per input, at most 2 bytes times the
redundancy per message.

Two things the trial needed before the numbers meant anything:

- **Targets that turn.** On straight lines a cubic curve and a straight one agree, so every gap
  is zero whatever the rewind does.
- **A cap swept, not guessed.** At 0.2 s, 71% of `mobile`'s shots were refused. At 0.35 s every
  link up to `mobile` passed and `bad` was mostly refused.
