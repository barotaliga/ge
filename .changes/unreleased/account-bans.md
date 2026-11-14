---
type: Added
title: Cheaters put out by a trusted server are banned on their accounts
---

A match report now carries the anti-cheat verdict of any player its server put out (`ReportPlayer::flagged`, from `guard_verdict`). A clean report keeps version 1's bytes; a report with a flagged player is version 2. A master that trusts the server bans the account for 14 days from the match's end. A banned account gets no ticket, no login code and no seat on a classic server. The match is rated on no ladder. Bans are worked out from the reports log, so a master read back keeps them.
