---
title: Measure people's recordings and the bots' with one lab before blaming a behaviour, and judge a fight change on six seeds
date: 2026-11-07
scope: games/shooters_edge/labs/se_people_lab.cpp, games/shooters_edge/bots/mind.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/net/classic_record.cpp
concepts: [se-people, realism, in-view, line-of-sight, engagement, launcher, rockets, push, seeds, mvd, userinfo]
---

"Bots roam with nobody known 60 % of the match" read as bots in the wrong places, and two fixes
were built on that: a team push and a hunt that walks the front. Neither moved damage. `se-people`
then measured people's MVDs and the bots' by the same rule (an enemy with a clear line through
the world hull, any way the player looks; then inside the bots' 100-degree cone off the recorded
view angles): people 25.5 % in view and 20.7 % faced, bots 25.8 % and 18.5 %. The gap was the
weapon in hand (a loaded launcher 32 % against 10 %), which only the item bits showed: the
recordings' active-weapon stat is stale for most players (people "held" the starting gun 86 %
of the time), so read possession off `kStatItems` and the ammo, never off the active weapon.
Our own MVDs carried no user info, so a lab read no names or teams in them: the recorder now writes
each player's public info. On e1m2 three seeds cannot see a 10 % change (a run's launcher pickups
moved 6 to 19 between seeds with nothing changed): A/B six seeds from one tree with a temporary
compile switch (two binaries copied aside), and read the mean with its standard error. A gate
that never fires does nothing: tally which condition stops it (`strength < 120` stopped 80 % of
the hunts: most free players carry no armour) before tuning its threshold.
A behaviour gated on a resource needs the resource counted first: the spam at news waited on 10 to
19 rockets and fired 0 times on three maps because no bot ever carried more than 7 (a histogram of
the rockets in hand while roaming showed it in one run); the gap was a rocket box no route reached.
Team calls are rate-limited (`kSayGap_s`), so a call that must follow another at once (the
opening's claim after the spawn report) is let through on purpose, or it lands 2.5 s late.
