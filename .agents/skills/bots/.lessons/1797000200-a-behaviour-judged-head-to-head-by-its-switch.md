---
title: Judge a behaviour head to head, one side without its switch, and confirm it on six more seeds
date: 2026-11-08
scope: plugins/shooter/features.hpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/bots/fighter.cpp
concepts: [features, feature-switch, a-b, head-to-head, by-team, seeds, noise, calm-view, lead, screen, rocket-jump]
---

Every behaviour of the shooter bots got a switch (`shooter/features.hpp`), and the first read of
all 21 was red without one against a full blue on e1m2 (`se-team --features-red=-<name>`, six seeds
each, a minute a set). It found what the A/B on both sides at once never showed: a behaviour that
makes both teams fight more can still lose head to head. The calm view had "cost no frags" on both
sides; one side without it beat the side with it by about 61 frags a six-seed set (12 seeds). The
lead (the intercept and the arc) cost about 47, the 120-degree screen won about 36, and a plain red
team (every switch off) scored 289 to the full team's 295. Read each number against the same
build's full-against-full set (the sides are not even: blue's spawns gave it 13 to 16 a set).
Six seeds swing about 35 frags a set by team: the hop (+71 then 0), the dodge (+60 then +17) and
the quad's count all moved that much on the next six seeds (`run6b.sh`: seeds 7-12), so confirm an
outlier there before acting on it. A switch whose set equals the baseline to the frag never ran:
rocket and grenade jumps do not happen on e1m2 (`team.rocket-jumps` 0), so judge them on dm2.
