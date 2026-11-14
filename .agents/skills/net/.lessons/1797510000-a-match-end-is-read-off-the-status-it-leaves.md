---
title: A classic server's match end is read off the status record it leaves, and a lab clock starts near zero
date: 2026-11-14
scope: engine/hub/results.cpp
concepts: [classic-adapter, results, match-end, status-phase, lab-clock]
---

A server that reports to nobody still shows its match ending in its status answers: a `playing`
record followed by an intermission (take the intermission's scores: they are the final ones), a
warmup, another map, or every score dropping. Keep the rule on two records (before, after) so a
test feeds it plain `net::Status` values with no network. The match's start is known only when
the hub saw its phase begin: a node first heard mid-match has `phase_us == first_us`, so pass -1
then. In a lab the simulated clock starts at 0, so `now_us - length` for a made-up length goes
negative and reads as "unknown": move the end forward (`now_us + length`) instead.
