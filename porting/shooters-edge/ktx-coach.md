# The match mod's coach: a clean-room reader's facts

What a spectator elected coach in the QuakeWorld match mod can and cannot do: who may ask, the
election, tracking and chat, what a coach blocks, how it ends, the map change, what is shown.

Read from QW-Group KTX, default branch at commit b1b23a98 (2026-11-08). No code: behaviour,
limits, and the words players see, paraphrased. Each fact is marked **[read]** (seen in the
source) or **[derived]** (concluded from what was read). It builds on
`server-settings-browse.md` and `ktx-bots.md` (spawn parms across a map change; the module is
loaded fresh each map, `qvm.md`).

## 1. Who may become coach

- Only spectators: the `coach` command is registered for spectators only; a player typing it gets
  the mod's usual not-for-you refusal [read].
- Not in matchless mode: the command is not flagged for matchless mode, so it is refused there
  like any other match command [read].
- Phase: only before a match. During the countdown, the match and the intermission a request does
  nothing at all, silently [read] (the match flag is non-zero from the countdown on [derived from
  the mod's phase numbering: 1 countdown, 2 match]). The abort and the step-down branches (below)
  run before this check, so they work in every phase [read].
- Mode: the server must be in team mode or CTF; otherwise the spectator is told there is no team
  picking in a non-team mode [read]. Duel, FFA and other modes refuse it.
- At least 3 players (spectators not counted) must be on the server, else "Not enough players
  present" [read]. Bots count as players for this check [derived: the count is every player
  slot].
- The spectator must already have a non-empty `team` key; else "Set your team name first" [read].
  There is no argument to `coach`; the team is whatever the spectator's own team key holds, set
  the normal way (`team` / setinfo) before asking [read].
- Other refusals, in order: two coaches already ("Only 2 coaches are allowed"), any election of
  any kind already running ("An election is already in progress"), the spectator's own 30 s block
  after an aborted election ("Wait N second(s)!") [read].

## 2. How many coaches

- At most two in total, counted server-wide, not per team [read]. There is no rule that the two
  are on different teams' sides except the next one [read].
- A second coach for the same team is refused at request time: the request is checked against
  the first spectator found who is already a coach; if that coach's team key equals the
  requester's, the requester is told a coach with that team already exists and to choose a new
  team name [read]. Only the first coach found is compared [read]; with at most one other coach
  that is enough [derived].
- The team key is all that is compared: the coach's team need not match any player's team
  [derived: no check against players' teams].
- Outside team mode and CTF no one can be elected [read], though a coach elected earlier keeps
  the flag if the mode changes [derived: nothing clears it on a mode change].
- Internally each coach is given number 1 or 2; the search for an existing coach 1 looks only at
  players, never spectators, so in practice every coach gets number 1 [derived]. The number has no
  visible effect in this release [derived].

## 3. The election

- Start: the request is broadcast as "<name> has requested coach status!" [read]. Every other
  client who is a player (not spectators) is told privately to type `yes` in the console to
  approve [read]. The candidate is told to type `coach` again to abort [read]. A 60 s timer
  starts [read].
- While pending, the server-wide coach count is raised by a half, so the count reads non-zero
  (this blocks the commands in answer 6) [read].
- Voters: only players can type `yes` / `no` (spectators cannot vote) [read]. Players of every
  team vote, not just the coach's team [read]. The candidate cannot vote ("You cannot vote for
  yourself") [read], but the request itself sets the candidate's vote flag, and the tally counts
  every client with that flag, so the candidate starts with one vote [derived].
- Each `yes` is broadcast as "<name> gives his/her vote" and is followed by how many more votes
  are needed while any are [read]. A second `yes` from the same player answers that the vote is
  still good [read]. `no` withdraws one's own earlier vote (broadcast "<name> withdraws his/her
  vote") and does nothing if the player had not voted [read].
- Share and minimum: required = ceil(share x (players - bots)), at least 2, where share is
  `k_vp_coach` in percent clamped to 51..100 (an unset or lower value means 51 %) [read]. Example:
  3 human players at 51 % need 2 votes, so with the candidate's own one a single `yes` passes it
  [derived]. Bots: the "only one human with bots, one vote is enough" relief does not apply to
  elections [read].
- `status1` (and the shown vote line) prints "Coach election in progress: v/r votes" [read].
- Pass: checked after each `yes` / `no`. If no match is in progress the candidate becomes coach,
  broadcast "<name> becomes a coach"; if fewer than two coaches then exist, a second line says
  one more coach should be elected [read]. That second line is only a prompt; nothing waits for
  it [derived]. If the votes are reached while a match is in progress the election simply ends
  with no coach [read].
- Timeout (60 s): broadcast "The voting has timed out." then "Election aborted" [read].
- Abort by the candidate (`coach` again while pending): broadcast "<name> aborts election!" [read].
- Fail as such does not exist: an election ends only by passing, timing out, the candidate
  aborting or leaving, or the map ending [read].
- Every end other than a pass, and a pass too, clears all election votes; the candidate (only the
  candidate) is then blocked from starting another election for 30 s [read].

## 4. Coach without a vote

- None found: no admin command grants or removes coach, captains do not choose one, and a lone
  spectator still needs the vote [read: nothing outside the election sets the flag, except the
  map-change carry-over in answer 7].

## 5. What a coach sees or hears

- Tracking: a coach may only track players whose team key equals the coach's [read]. When the
  tracked player changes (by any means: the track keys, the `track` command, the client's auto
  tracking) and the new target is on another team, the mod moves the coach to the next player of
  the coach's team after that target, wrapping round to the first, by pushing a `track <id>` to
  the coach's client [read]. If no teammate exists it centre-prints "Found no suitable player to
  track" and pushes attack presses to the client to drop out of tracking into free flight [read].
  The correction happens after the switch, so the enemy view shows for a moment [read: the mod's
  own remark; derived: about one server frame plus the round trip].
- Free flight is not restricted: the rule only acts when a player is tracked [derived].
- Chat: nothing coach-specific [read]. A coach's `say_team` goes like any spectator's: to
  spectators only (marked as spectator talk), never to the team's players [read]. A player's
  `say_team` reaches a spectator, coach or not, only under the general spectator rule: the
  server's say-team-to-spectators switch is on (the mod drives it from `k_sayteam_to_spec`: 0
  never, 1 only during a match, 2 only outside a match, 3 always), the message is a team message
  marked for spectators, and the spectator is tracking a player of the speaker's team or, when
  not tracking, has the speaker's team key [read].
- No team overlay, positions or other data is sent to a coach that a spectator lacks [read:
  nothing else keys on the coach flag].
- Other spectators are told nothing beyond the broadcasts above [read].

## 6. What a coach may do

- No command is given to a coach: no pause, break, ready, lock, pick, invite or substitute
  [read: the flag is checked only by the places listed in answers 5 to 8].
- While coaches are present (count non-zero, including a pending election) `rpickup` is refused
  with "No random pickup when coach stuffing", and a random-pickup vote already counted does not
  fire [read]. `swapall` is refused the same way ("No swapall when coach stuffing") and its vote
  does not fire [read]. Captains, `pickup` and the rest are not blocked by coaches [read].
- `nospecs`: when the vote to kick spectators passes, coaches are not kicked, and a coach may
  connect while spectators are barred [read]. A coach who steps down while `nospecs` is on is
  disconnected [read].
- Team key: a coach or a pending candidate may not change team; a change is refused ("You may
  not change team") and the old team pushed back to the client [read].
- While the count is exactly two, any other spectator who tries to set a team is refused and
  pushed back to an empty team [read: the rule compares with two internal cvars that nothing in
  the mod ever writes; derived: so the forced team is always empty].

## 7. How a coach stops being coach

- `coach` again: broadcast "<name> is no longer a coach", then a status line [read]. Works in any
  phase, including mid-match [read].
- Leaving the server (or switching to player, which in this game is a reconnect as a player):
  broadcast "A coach has left", then the status line [read].
- The status line after either: "[1] coach present" if the count is still at least one, else
  "No coach present" [read]. With two coaches and one leaving it says 1; with one coach leaving
  while another spectator's election is pending the half left over floors to none [derived].
- No admin removal, no removal at match start or end [read]. A coach stays through the match and
  after it [derived].
- Map change: any running election is aborted when the map ends [read]. The coach flag is saved
  into spawn parm 12 when the level changes and restored from it when the spectator (or player)
  connects on the next map [read]. Parm 12 is zeroed for a client new to the server and on a
  respawn that is not the connect itself [read]. The server-wide coach count is not restored from
  the parms [read]; since the module is reloaded fresh each map (this repository's notes on the
  module interface), the count starts at zero on the new map while the returning coach still has
  the flag [derived]. Consequences [derived]: `status1` shows no coach, `rpickup` and `swapall`
  are allowed again, two more coaches can be elected, and when a carried coach steps down the
  count goes to -1 and the line reads "[1] coach present".
- `_k_coachteam1` / `_k_coachteam2`: registered and read, never written by the mod; they play no
  part in the carry-over [read].
- A cancel-all-coaches routine exists but nothing calls it; and it walks players, not spectators
  [read].

## 8. What is shown

- `klist` (the client list): the spectators' table has a `co` column with `c` for a coach [read].
- `status1`: "[1] coach present" when exactly one coach (the floored count is 1); nothing when two
  or none [read]. Also the election line while one is pending [read].
- No serverinfo key, no userinfo key, no change to the name, no scoreboard mark and nothing in the
  end-of-match report or stats [read: none written].

## 9. Settings

- `k_vp_coach`: share of votes in percent; the example config sets 51 [read]; clamped to 51..100
  in use [read]; no default is registered, so unset means 0 which clamps to 51 [derived].
- `_k_coachteam1`, `_k_coachteam2`: internal, unused in effect (answer 6) [read].
- Others that touch coaches without being coach settings: `k_sayteam_to_spec` (0..3, answer 5),
  `_k_nospecs` / `k_vp_nospecs` (answer 6) [read].
- Fixed, not settable: 2 coaches at most, 3 players minimum, at least 2 votes, 60 s election, 30 s
  block after an abort [read].

## 10. Edge cases

- The coach's team empties: the coach keeps the flag [read]; on the next track change with no
  teammate the coach is put in free flight with the "no suitable player" line [read].
- The team is renamed by its players: the coach's own key is frozen, so the coach now has no
  teammates and cannot track any player [derived].
- `swapall` while a coach is present is refused (answer 6), so teams cannot be swapped under a
  coach [read]. A swap by hand (players changing team keys) is not prevented and leaves the coach
  tracking whichever players now carry the coach's team name [derived].
- The candidate disconnects mid-election: broadcast "Election aborted" unless a match is running
  (then silent), and the election ends [read].
- Two spectators race for one team: only one election runs at a time, so the second is refused
  as an election in progress [read]. When the first passes, the second's request is refused as a
  coach with that team existing [read]. The same-team check is made only at request time, but a
  pending candidate cannot change team, so it holds [derived].
- The coach is the only one on a team key nobody plays: allowed; tracking any player then fails
  as above [derived].
