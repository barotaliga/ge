# Bot strategies: what the classic team, duel and free-for-all players do

Research notes for the Shooter's Edge bots (row M18r). It is a working document of the porting
area: the facts the community's guides give, restated in our own words, the map names kept
here only. The engine's code and docs say what a bot does in the engine's own terms (a "key
area", a "power-up", a "trick move"), and a map's own plan is data the game loads.

Sources read (November 2026): the community wiki's game-mode, strategy and per-map pages, a
long-standing personal page of map strategies, the community's 4on4 forum guides, the map
guides of an old strategy site, and the general trick-jump page of a Quake wiki. Several pages
were down (HTTP 503) on the day and are to be read again before the plans are written as data.

## What every mode shares

- **Staying alive and equipped comes before frags.** A team better armoured and armed than the
  other wins its fights; frags follow control.
- **Control is a set of AREAS, each round an item that wins fights**: the rocket launcher (RL),
  the red armour (RA, 200), the yellow armour (YA, 150), the mega health (MH, 100 over the top),
  and the powers: the quad (four times the damage, 30 s, back 60 s after it is taken) and the
  pentagram (no damage, 30 s, back 300 s). The lightning gun (LG) matters on a map with cells
  near it.
- **Timing.** Every strong player times the items he sees taken: armour and mega back 20 s after,
  ammunition and weapons 30 s (weapons stay in team modes), health 20 s; the powers as above. A
  pickup is heard as well as seen, and the clock is read at the moment. Times can be kept in step
  (two items due together) or out of step on purpose.
- **Surprise.** The first shot wins an even fight; a holder who is predictable loses.

## 4on4: the "big five"

The 4on4 maps the community plays most are dm2, dm3, e1m2, and the two custom maps cmt3
(Schloss) and cmt4. The habits a team has on each:

- **Roles.** A team splits by what each player holds: one or two on the armour they control, one
  on the launcher, and the rest flooding to the quad before it is due. Rockets are scarce on some
  maps and are kept for the RL holders.
- **The opening.** The team takes the launcher and the armour areas first ("in the first minutes,
  everything on the RL area: then every player has a launcher, and the yellow armour comes with
  it").
- **Warn before a move**, so the team floods an area together for about 20 s; attack from two or
  more entrances at once; send a cheap player (shotgun only) in first to draw the first rocket.
- **After winning the map back**, stack up for about a minute; never leave the red armour without
  two players by it.
- **Attack right after the enemy's quad runs out.**

Per map (the community's plans, restated):

- **dm2.** Everything turns on the quad, the nailgun and the teleporter area. A "tele rush" sends
  the whole team at the yellow armour's teleporter; once its camper is gone the team floods the
  grenade launcher and the high launcher, covering the next quad. A build-up from the low red
  armour waits until two players have launchers: one takes the armour and the mega through the
  big room and rocket-jumps to the teleporter's exit, the other takes the armour low and supports
  the quad. The best-known trick: a rocket jump from the quad up to the launcher. A strafe jump
  across the lava from the launcher's room to the super shotgun's room saves the long way round.
- **dm3.** The most played 4on4 map. Its front is the launcher, the yellow armour and the quad;
  the middle is the mound; the back the red armour and the super nailgun; outside the pentagram.
  The plan: take the yellow armour and the launcher first, then the quad through the window with
  a teammate's help (best entered from high yellow armour while a mate comes through the window).
  A pentagram run aims for two or three enemy launcher holders killed in its 30 s and ends on the
  red armour with five seconds left. Tricks: the quad jump (from the ring's platform to the quad's;
  a rocket jump a little early beats one a little late), the yellow armour to the quad, the jump
  from the water under the launcher's window up onto its ledge, a quick way from the high bridge
  to the launcher's window, and the grenade-and-rocket jump from the pentagram up to the window.
  Waiting on the quad, grenades are thrown round the corner towards the window.
- **e1m2.** No red armour and no lightning gun; the yellow armour, the quad and the launcher. The
  quad rush: the team floods the quad from ten seconds before it is due, harder at five, the last
  in with a launcher and the green armour (the mega taken on the way in at two seconds). After the
  quad, push to the grenade launcher and the yellow armour: the camper there only needs flushing
  out, not killing. Retaking the yellow armour from behind: green armour and launcher (mega
  better), through the water, a rocket jump over the yellow armour as it comes back.
- **cmt3 (Schloss).** Every power-up, eight spawn points. The red armour is the centre: one or two
  launcher players patrol it and guard it at its time against teleporter entries; holding it, the
  quad is easy. Retake from behind with yellow armour and launcher (mega better), up under the
  red armour, at its time. The pentagram is often forgotten, so it is often free.
- **cmt4.** Reach the quad at least ten seconds before the enemy. Two players take the red
  armour, one the launcher (the water mega on the way), one the lightning gun. The launcher holder
  stands against the wall by the health covering the teleporter exits; the lightning gun waits on
  the water ledge. One player holds the yellow armour and stays there; two stay near the quad
  before it is due. The pentagram belongs to whoever holds the yellow armour. After a quad, hit the
  red armour's pool through the water mega. Many cells by the lightning gun: a team of good
  lightning gunners holds the big rooms.

## 2on2: dm2, dm4, dm6, e1m2

Ten minutes, weapons stay, power-ups on.

- **dm6.** The red armour's room is the map: control it and a weaker pair beats a better one. The
  mega and launcher's room is next: it has the only other armour and the only pack of rockets.
  "Always control both the rockets and the red armour."
- **dm4.** The lightning gun's aim and the strategy round it decide the map (a small map: the
  launcher, the mega and the yellow armour close together); to be read further.
- **dm2.** In a small game the lower launcher's room is the point of control; keep the launchers
  away from the other pair.
- **e1m2.** As 4on4 with fewer players: the quad on its time and the yellow armour.

A pair splits the map: one holds the armour, one times and takes the next item, and they cross
over on calls ("ra in 10", "coming quad").

## 1on1 (duel): dm4, dm6, dm2, ztndm3, aerowalk

Ten minutes, weapons stay, power-ups off.

- **Map control is the strategy; the fight is tactics.** Control lets the player in front take
  the frags as his opponent respawns weak (the spawn kill).
- **Listen.** A pickup's sound tells which item went and so where the opponent is; the most
  vulnerable opponent is one who does not hide what he does.
- **Time everything**: the armours and the mega to the second, keeping them in step to circle.
- **Behind, do not trade**: avoid deaths, take an item when the leader is far, attack by surprise.
- **aerowalk**: control the mega health, take the armour, then hunt.

## Free-for-all

Many players, no team: control matters less than in a team game, because nobody can hold an area
alone. The habits: keep moving between the armours and the mega on their times, take fights
already started (the weakened winner), avoid the middle of a crowd, and take the quad when it is
due rather than hold it.

## Movement and trick moves

What every good player does, in the engine's terms:

- **Strafe jumping and bunny hopping**: in the air, strafe and turn the view the same way to gain
  speed past the run speed; land and jump again at once to keep it. The classic movement gains
  more from it than later games.
- **Ramp jump**: jump just before the top of a ramp to add its upward speed to the jump's.
- **Rocket jump** (launcher, armour preferred): look down, jump, fire under the feet; the blast
  throws the player up and on. **Grenade jump**: a grenade under the feet (fired straight up on
  flat ground, it lands back and bursts on its second bounce). **Grenade-and-rocket**: both at
  once, higher and further, at more health and ammunition.
- **A map's trick moves** are routes the map's plan names (above): a start, a run-up, a jump or a
  rocket jump, an expected landing. They are data a map's plan carries, proven by the move's own
  simulation before a bot uses one (the nav graph proves walking and jumping links this way now).

## What the code should be

Generic (any arena shooter: the classic one now, later ones after), in a plugin the games link:

- the **mind** (beliefs from sight and team lines, read by skill), the **timers** (exact or off by
  skill), the **claims and roles**, the **team words** as a vocabulary with each game's item names
  plugged in, and the **plan** a map or a mode gives as data: its key areas, the opening, which
  item goes to whom, the rush times ("flood the quad from 10 s, harder at 5"), and its trick moves;
- **the mode's plan** (4on4, 2on2, duel, free-for-all) as data on the same scheme: what is held,
  what is timed, how a pair splits.

Game-specific: the item catalogue (what each item is called, its respawn, its value), the
movement (the move's own simulation proves a trick move), and the maps' plans themselves.
