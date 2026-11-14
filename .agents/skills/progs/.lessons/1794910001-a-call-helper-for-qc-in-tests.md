---
title: One call helper puts any program function under test, and a field beats a borrowed one
date: 2026-11-04
scope: tests/shooters_edge_campaign_monsters_test.cpp, games/shooters_edge/rules/
concepts: [qc, monsters, enter-qc, run-qc, radius-damage, blast-share, fields]
---

A test that needs an exact event (a blast of 110 here, a blow of 100 there) calls the program's
own function with `call(name, {words})` over `enter_qc`/`run_qc`/`leave_qc`, entities passed as
`pg::entity_value` and floats as `pg::as_word`: `hit` and `blast` are one line each on it, so a
rule like "the giant takes half of a blast" is held without staging the rocket that would cause
it. In QC, a value an attack in progress needs (a claw's damage factor) gets a field of its own;
borrowing `attack_state` for it works until a pain interrupts the attack and leaves the run state
holding a number the mind does not know.
