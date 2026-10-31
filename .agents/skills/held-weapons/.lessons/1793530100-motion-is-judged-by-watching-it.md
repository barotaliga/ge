---
title: A held weapon's motion is judged by watching it, and the first fixes were all about the body, not the weapon
date: 2026-10-31
scope: games/shooters_edge/content/models/view, games/shooters_edge/labs/se_weapon_stage_lab.cpp
concepts: [held-weapon, animation, hands, arm, ik, wrist, swing, gif, webm, review]
---

The axe took eight rounds with the person directing it, and every correction after the first was
about how a body holds and swings it, not about the axe's form. In order: the kit's axe faced the
wrong way; it needed a hand; every weapon needs hands; the hands must be identical; coarser, with
a shoulder, an elbow and a wrist; the shoulder static unless the body moves; the wrist bent too
much; the strike landed too high. Each looked fine as a strip of frames and wrong in motion. Send
the GIF or the WebM (`se-weapon-stage` writes both) at the first plausible build, and expect the
body's rules (one hand for all, a fixed shoulder, an arm-led swing, a near-neutral wrist) to be
asked for; they are in SKILL.md now. Two numbers made the swing quick to tune: the kit's worst
wrist bend and the lab's table of where the tip lands each frame.
