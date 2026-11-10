# Team strategy: how a 4on4 team wins, and what our bots should change

Research notes for the Shooter's Edge team bots (row M18r), beside `bot-strategies.md`. A
working document of the porting area: what the community's guides say a 4on4 team does on the
match mod's standard settings (teamplay 2, team damage on; weapons do NOT stay, a weapon taken is
gone 30 s), restated in our own words, then turned into changes to `games/shooters_edge/bots/`.

Sources read (November 2026): the community wiki at quakeworld.nu (the per-map pages for e1m2,
dm2 and dm3, the items page, the spawn-modes page, the strategy page); the forum's long-kept 4on4
guideline thread (quakeworld.nu forum, topics 2327 and 3854, the same guide); a blog post on the
player types of a 4on4 lineup and its comments (quakeworld.nu blog 239); a personal strategy page
of 4v4 tips and map plans (strlen.com, quakestrat); the old map guide's e1m2 pages (quaketerminus,
the quake bible: the key to the castle, situations); a team config page that lists every teamplay
message bind (quakeworld.nu configs, a teamplay cfg) and the client manual's teamplay
placeholders (ezQuake's manual, archived at quaddicted.com); and a duel strategy thread
(quakeworld.nu forum topic 2131) for one player's fighting habits. No video commentary was
reachable as text. **Correction to `bot-strategies.md`:** in 4on4 weapons do not stay (the
match mod's 4on4 mode is deathmatch 1); only duel and 2on2 keep them.

## 0. What our bots did (the live server, e1m2, 4on4 of our bots, 10 minutes)

The caller's measurement: about 140 frags; 107 of 160 deaths around the mega health (`mh-low`);
128 of 160 while retreating to heal; only the scattergun, the double and grenades used; 1–3 team
kills; scattergun 30–50 %, double 30–70 %. Read again here in the third iteration's logs:

- 68 of 119 deaths at `mh-low` and 8 more at `mh-entry`; 102 of 119 in `retreat`, the goal
  `health` (56) or `mh` (39). Next places: `quad` 14, `quad-stairs` 11.
- **Nobody took the yellow armour or the mega health in ten minutes**, and nobody the launcher
  (the report's items: health boxes, the green armour, the quad). No narrator line ever shows a
  goal `ya`; the claims said were `coming mh` 15 times, `coming rl` once, `coming ya` never. In
  `mind_plan` the best armour falls back to the mega when no route reaches it, so **the yellow
  armour is out of the bots' nav on e1m2**, and the whole team plan (the claim, the hold, the
  `lost` area) turns on the mega instead. Both teams claim the mega, every hurt bot heals there,
  and the mega is the one place on the map where the two teams are sure to meet.
- 227 `enemy at` lines, 98 `coming quad`, 25 `help [mh-low]` in ten minutes: the talk is mostly
  noise round the trap.
- Some reports read `a:0 h:1000 rl` from a bot with no launcher: to check (the match mod's
  pre-match state is the likely source); a report before the match should not be sent.

## 1. The principles

1. **Equipped and alive before frags.** Keep the team better armoured and armed than the other;
   frags follow (strlen; the forum guide). A team that loses control usually loses it when
   several of its players die close together, one after the other (strlen).
2. **Control is a few areas**, each round an item that wins fights: the best armour, the
   launcher, the quad, the mega. Deny them as much as hold them: on dm2 control means the enemy
   has at most one red armour and, ideally, no launcher (wiki, dm2).
3. **A lineup mixes kinds of player** (blog 239): one who sneaks a launcher to the armour and
   avoids shotgun fights, one or more who take every shotgun fight to keep the enemy weak, one
   roamer (often the quad's runner, the team's best) and, for a weaker player, a defender camping
   the most defensible spot (dm3 red armour, dm2 teleporter, e1m2 yellow armour or mega). Four of
   one kind loses: four roamers fight over packs, four shotgun fighters let the enemy collect
   launchers. On e1m2 the advice is no or one sneaker, two or three shotgun fighters, one roamer.
4. **Stack**: pass armour on. On e1m2 one player holding the yellow armour arms the whole team in
   one quad cycle (wiki, e1m2). Take what you need, point at what you do not, and take a pointed
   item yourself if no mate claims it (forum guide). Keep the launchers stocked with rockets and
   protect them (strlen).
5. **Time and call.** Every key item is timed (armour 20 s; mega 20 s after its taker is back
   down to 100; quad 60 s after it is taken; weapons and ammunition 30 s; health 20 s). A move is
   called before it is made, so the team floods together (forum guide; strlen).
6. **Attack with the odds, together, by surprise.** A takeover with a small chance is not worth
   starting; wait out of sight instead of coming when expected; send a cheap player in first to
   draw the first rocket; come in from several entrances in the same second, or at least two
   together (strlen). Attack the moment the enemy's quad runs out.
7. **Weak, do not fight.** Hide, take health off the obvious routes, keep a way out, never run
   into the open where someone waits (the duel thread). The mega is where a fleeing player goes,
   so it is where his hunter follows him (the old e1m2 guide's advice to the hunter).
8. **The losing team regroups** away from the enemy's control, stacks, ambushes the enemy's quad
   run, and only then retakes (wiki, dm2: gather at the low area, stack the red armour and two
   launchers, nails and rockets at the platforms as the quad comes). Calling `help` in a hopeless
   spot only feeds (forum guide).
9. **Height and crossfire.** High ground wins; split into two pairs on two routes so a defender
   is shot from two sides (wiki, dm3: two through the teleporter, two along the catwalk).
10. **No team damage.** No explosive at a spawn point or a teleporter exit unless sure no mate
    just died or comes out there; with the quad, no blast near a mate; stand where the enemy's
    splash does not reach (dm2: by the lava, not the wall) (forum guide; wiki, dm2).

## 2. The maps

### e1m2 (the key to it: the yellow armour and the area to the quad)

**Items.** One quad; no pentagram, ring, red armour or lightning gun. One yellow armour, one
green, one mega. Weapons: one launcher, one grenade launcher, two super nailguns, one super
shotgun, one nailgun. Few weapons and no red armour: packs are kept for teammates (forum guide).

**Areas** (the community's names, which our place names follow): the **yellow armour room**
(entrances: the spikes corridor, noisy; the grenade launcher's stairs, easy to rocket; under the
catwalk; a drop from the super nailgun's ledge; and from beneath through the water), the
**grenade launcher's room** beside it (many health boxes and 15 rockets, two ways in, hard to be
chased into: the place to heal and re-arm, the old guide says), the **quad room**, its stairs and
the quad box (a hiding spot often forgotten), the **launcher's room** (small, three entrances,
one from the cross's water; a spawn between the two water entrances; up from the water by a
mate's head or a rocket or grenade jump; a back way to the start), the **mega room** (a high
ledge over the cross, an exit, a teleporter on each side whose exits are swapped), the **start**
and the **bridge** between it and the yellow armour, the **cross** and its water, and the
teleporter under the yellow armour whose exit is by the green armour.

**The team in control:** one holds the yellow armour standing on its top near the ledge (not on
the item, so he does not take it by accident and can take it the moment an enemy comes), and
gives it to each mate in turn; the rest defend from the yellow armour to the quad. The quad rush
floods the quad from ten seconds before it is due, harder at five; the launcher player comes last
with the green armour, taking the mega from the water by the quad on the way at about two
seconds. With the quad: sweep the yellow armour, grenade launcher, quad and cross area to catch
spawns; then push to the grenade launcher and the yellow armour (its camper need only be flushed,
not killed) and stack everyone there. The launcher's room is visited a few seconds before the
launcher is due, armoured and armed, not held. Spam: from the mega's high ledge towards the
cross; grenades bounced off the far wall of the quad stairs into the cross's water at players
leaving the launcher; standing behind the teleporter exit by the green armour, a free shot at
whoever comes through. Ahead late: turtle at the start (the boxes, the arch over the door, the
window) or on the mega's ledge.

**The team behind:** outnumbered, two roam together hunting the enemy's launcher holders and two
hold the yellow armour and the quad. Retaking the yellow armour: one through the spikes while one
drops down the grenade launcher's stairs; grenades down those stairs while a mate comes from the
super nailgun underneath; or from the water, walking without a hop so it is not heard, a rocket
jump onto the armour as it respawns, timed against the enemy's quad. Strlen: e1m2 is the easiest
map to take back, since nobody can be very stacked, so a well-timed quad rush works.

### dm2 (the key: the quad, the nailgun and the teleporter area)

**Items.** One quad (upper quad area, with a launcher). Two red armours (the secret, opened by
shooting a button seen from the low launcher's hallway; the red armour and mega room across the
lava). Two megas (upper big room across a moving platform; the red armour and mega room). Three
yellow armours (two in the big room's cages, one behind the bars at the low teleporter). Two
launchers (upper quad, the low launcher room), a grenade launcher (the big room's upper niche),
the nailgun (water room), the super shotgun (by the red armour and mega room).

**Areas.** The water room has the most spawns and is the only way down to the low teleporter; a
switch in the teleporter hallway opens a grate to a yellow armour and the top of the big room.
**In control:** deny one red armour and both launchers; the teleporter's defender never leaves
his post (a teammate relieves him first: five seconds empty lets two through). With a big lead,
lock the map: one at the low teleporter, one in the water room, one at the low launcher and a
quad runner going quad, water, low, red armour and mega, often the same player for several quads.
**Behind:** a teleporter rush by the whole team, two weak players first as bait and a player with
red armour and launcher holding the exit; or a slow build at the low red armour: wait until two
have launchers, then one takes the armour and the mega through the big room and the other takes
the low armour and supports the quad move; or gather at low, stack, ambush the enemy quad, fall
back to the red armour and mega room, and send two through the teleporter to take back the water
and the teleporter. The red armour's room is defended by the super shotgun's spot near the lava.

### dm3 (front: the launcher, the yellow armour and the quad; back: the red armour)

**Items.** The quad, the ring and the pentagram once each; one red and one yellow armour; megas;
launcher, lightning gun, grenade launcher, super nailgun, nailgun, super shotgun. **Areas:** the
launcher's window, the red armour room with its catwalks, the yellow armour, the lifts, the water
(to the window and the pentagram), the quad, the ring, the hill above them, the super nailgun's
ledge and its teleporter (to the ring, then the quad), the high bridge. The teleporter by the
yellow armour comes out at the window.
**In control:** yellow armour, then launcher, then the quad through the window with a mate
splitting the defence, then the red armour with mates going in ahead. The red armour is held by
at least one well-armed player (armour with only a shotgun is not enough) and never left without
two near it. Hold the yellow armour from halfway up the stairs (the catwalk to the quad and the
water corner in view); stand in the box beside it twenty seconds before it is due. Split two
through the teleporter and two along the catwalk to crossfire the quad's defenders.
**The pentagram run:** thirty seconds to kill two or three enemy launcher holders, no packs,
ending on the red armour with five seconds left. Rockets at the wall by the water steps five
seconds before the pentagram is due keep hidden takers off it.
**Behind (taken at the red armour):** a launcher or lightning gun and the yellow armour (or the
pentagram's mega), sneak to the super nailgun over the lifts with a mate as bait, a second mega and
ammunition, then into the red armour from behind just after the enemy's quad ends.

## 3. What a team says, and when

The binds a team config gives every player (the config page; the forum guide; the client's
manual) and when each is used:

- **report**: armour, health, best weapon, powers and place, readable at a glance; pressed when
  asked, when an item is in dispute, or when a mate might shoot. Dead, it says the place is lost.
- **lost [place]** with the enemies counted near the speaker when he died (the client's enemy
  count is kept five seconds past a death) and the pack he left; said on death at a key area.
- **safe [place]**, **help [place]** (only when the place matters), **coming [place or item]**
  (a claim, or a warning before arriving), **took [item]** (also starts everyone's timer),
  **point [item] at [place]** (an offer; unclaimed, take it), **need [ammo, armour, launcher,
  health]** (rarely), **switch / replace** (take over my post, I have to leave), **waiting
  [place]**, **attack [place]** with the enemy count (a counter-attack called), **slipped** (an
  enemy got past, dm2's teleporter), **get quad** with its time, **quad over**, **enemy quad /
  enemy pentagram**, **enemy launcher at [place]** and **launcher dead [place]** (the enemy's
  launcher count), **kill me** (a mate takes my launcher off me: team damage is on and the pack
  carries it; there is no other way to give a weapon).
- What carries the game: the timers ahead of the item (strlen: twenty seconds ahead for a flood;
  e1m2's quad from ten), where the enemy is for the quad's runner (who must not spend his thirty
  seconds looking), each death at a key area with how many enemies were there, the enemy's powers
  and launchers. A report on change, not on a clock; no repeat of what a mate just said.

## 4. One player's killing habits

- **Pre-aim** the doorway an enemy will come through; the first shot wins an even fight.
- **Prefire and spam**: rockets at a teleporter exit, a spawn by the area, a door being heard;
  grenades round a corner, down stairs, into water a player swims out of; a rocket at where he
  is going (through a teleporter, at its exit) rather than where he is.
- **Dodge**: never a straight line in a fight; change side often and unevenly; keep off walls a
  rocket can burst on; jump less while dodging close, more against splash.
- **Do not fight weak**: hide and heal off the obvious routes, keep a way out, switch back (take
  an item by one exit, leave by another); after a trade take all the health so he cannot.
- **Height**: stay high where you can; grenades from above.
- **Peek**: show little, step out, shoot, step back; do not stand in the open on a power's spawn.
- **Sound**: jumps, landings, splashes and pickups are heard (this game has no footsteps); walk,
  do not hop, when sneaking or healing; read the enemy's pickups and jumps.
- **Weapon by range**: the double close (its spread is wider than a man past a few hundred
  units), the lightning gun within its 600, the launcher mid to far (at the feet far, the body
  close), grenades round corners and from above, the scattergun or nails at range to soften or
  stop a hopper. Finish the weakened; let the stronger enemy go by.

## 5. The changes, in order of value

Each with what to change, where (as read), the expected effect and how to measure it on the live
server (`report.md`: frags, deaths, team kills, accuracy per weapon, items; the `death` lines:
place, mode and role, goal) and in `se-team --map=e1m2 --client-fed=1` before the live run.
"Deaths in retreat" should be read against retreats begun: add a retreat count and the seconds
in retreat and the nearest enemy's range to the `death` line.

**1. Put the yellow armour back in the bots' map (e1m2).** The plan falls back to the mega
because no route reaches the armour (`mind.cpp` `mind_plan`: `if (!reachable(f, ra)) ra =
first_of(f, ItemKind::mega)`). Find why with `se-nav` on e1m2 (the node by the armour among the
unreached): the way up from the grenade launcher's stairs, the spikes corridor or the drop from
the super nailgun's ledge is not proven, or the item's node is off the floor it stands on
(`ItemSpot::node`, `plugins/shooter/nav.cpp`). Until it is fixed, the fallback should not make the
mega the claimed area (claim nothing, or the green armour). Effect: the map's real key area is
held, stacked and fought over; the two teams stop meeting at one spot. Measure: `ya` in the items
column (ten or more takes a match), `goal ya` and `safe [ya]` lines, the share of deaths at
`mh-low` (from 57 % to under 20 %).

**2. The mega is not a place to heal.** In heal mode `raw_value` gives the mega 20 + 0.8 x (250
- health), about 200 to a player at 30, eight times a health box, so the danger spot's 0.25
(`best_item`, `kDangerScale`) still leaves it first; and a weak player is sure to meet both his
hunter and the other team's hurt players there. In `best_item` with `heal`: the mega worth only
the health it gives up to 100 (a box's worth plus a little), and a place where two or more
players of either team died in the last minute (own deaths, mates' obituaries, and the enemy's
deaths seen or heard) out of the heal goals altogether rather than scaled. The mega stays a
stacking item for a healthy player with no enemy known near it. Effect: the death trap breaks.
Measure: deaths with goal `mh` (39 now), deaths at `mh-low`, deaths in retreat per retreat begun.

**3. Weak means recover, even with nobody in sight, quietly, towards the team.** `think` makes a
heal goal only when a contact is known (`c && weak`); a weak bot with nobody about roams for the
item worth most (the quad, the mega, the launcher across the map) and walks into fights weak.
Make weak a goal of its own: the nearest health or armour away from the places mates died and
from the enemy's last called place (`mind.enemy_at`), preferring health within reach of a mate's
fresh place or the team's held area (on e1m2 the grenade launcher's room), the hop off on the way
(a jump is heard), and the danger spots read for every goal while weak, not only in heal mode.
Effect: fewer deaths on the way to items; fewer frags given to the enemy's shotgun fighters.
Measure: deaths at strength under 50 by mode (`roam` and `retreat`), time from weak to strong
again, deaths per minute.

**4. Do not turn a back up close; break sight instead of running a long way.** In `think` the
weak retreat is chosen before the `cornered` rule (an enemy within 192 units), so a weak bot
with an enemy at his side runs off along a route while the enemy keeps shooting. Within the
double's range, or when the enemy is believed weak too (`foe_might`), fight; otherwise the first
goal of a retreat is the nearest node out of the enemy's sight (a corner, a door, a drop), and
the healing route starts from there. Effect: fewer of the 128 retreat deaths; better trades.
Measure: deaths in retreat with the enemy under 250 units; kills made in retreat; the trade ratio
(kills over deaths) of weak players.

**5. Dodge in a fight.** `shooter/bot.cpp` turns the strafe and the hop off once aiming, and the
pilot then runs the route straight: a fighting bot is a target on rails, which is why both teams
hit each other so often (double up to 70 %, past the best division's band). Add a dodge on the
side axis in `fighter_command` while a visible enemy is aimed at: the side switched every 0.25 to
0.7 s drawn from the fighter's own stream, the forward axis kept towards the goal, wider and
with a jump now and then against rockets, never towards a wall within a blast's reach; its
quality by the `moves` trait (novice rarely, expert always). Effect: the enemy's hit rate on our
bots down to the bands (double 35–45 % for the best, scattergun lower), fights longer, the better
player wins more often. Measure: accuracy per weapon of the team that faces the dodgers (one
team dodging, one not, then both), deaths per fight, the `se-team` hit bands.

**6. Hold from a spot, view on the way in.** A holder stands on his item (`kSafeNear`) and his
view follows the pilot's way when nobody is seen (`bot.cpp`, the branch with no contact), so he
never pre-aims. Give the bot a point to watch when idle (`Bot::watch`, used in place of the
pilot's yaw when no contact is fresh): for a holder or a quad waiter, the entrance nearest the
enemy's last called place or last heard sound; and a hold spot a little off the item with sight
of its entrances (e1m2: on top of the armour near the ledge; dm3: halfway up the stairs). Effect:
more sightings taken at half the reaction (`se-team` prints the pre-aimed share), holders win
the first shot. Measure: pre-aimed share, holders' kills over deaths, time an area is held.

**7. Regroup, then attack together.** A strong player alone retakes a lost area (`mind_plan`'s
`retake`, `kRetakeStrong` 120), and each hurt player comes back on his own: the team arrives one
by one and feeds. Add an `attack [X]` call and a gather: a retake waits at a staging node out of
the area's sight until two mates who said `coming` are near (their fresh places), then goes, the
weakest first, from two entrances when the map has them; after the enemy's `quad over`, or 30 s
after his quad was heard taken. When two or more mates died in the last 20 s (obituaries,
`lost`), everyone weak falls back to the team's safest area (e1m2: the start or the green armour
side) to stack before the attack. Effect: fewer deaths in streaks, areas won back. Measure:
deaths within five seconds of a mate's death at the same place ("feeding"), area held time per
team, frags in the 20 s after a retake call.

**8. No blast near a mate.** `mate_in_line` checks only the line of fire (40 units); a rocket
or a grenade bursts 160 units round where it lands, and the target is often fighting a mate.
Hold the trigger with a blast weapon when a mate stands within about 140 units of the aim point
or of the first wall the line meets (`fighter.cpp`, beside `mate_in_line`; grenades wider, they
bounce), and in `spam.cpp` never at a spawn or a teleporter exit within a few seconds of a mate's
death or a mate's `coming` through it. Effect: team kills from 1–3 a match to none. Measure: the
`tk` column, `held_fire` counts.

**9. Get the launcher, keep it, give it.** On e1m2 the launcher is reached through the water
(the `dive` route, `kDiveMost_s` 8) or from the cross's water by a jump the nav may not prove;
bots set the goal (`goal rl at water-rl`) and die or give up. Check its route on `se-nav`; go
for it armoured and with the double, timed to be there a few seconds before its 30 s return,
not to stay; keep the team's launcher holders believed (a report says `rl`) and give the armour
to them (`gives_up`); the enemy's launchers counted from what is seen (`enemy rl at`, `rl dead`).
Effect: rockets in the team's hands; the spam, the quad's damage and the area holds start to
work. Measure: the `rl` column, launcher held time per team, frags with rockets.

**10. Weapons by range and height** (`weapon_score`): grenades only at an enemy below, out of
sight round a corner, or at a door being heard, not at a man in the open past 300 units; the
super nailgun from 250 to 600 when carried; the scattergun at range to soften. Measure: grenade
accuracy (now 0–20 % on a handful of shots), damage given per minute.

**11. The quad goes to the stacked one, and the enemy's quad is avoided.** The rush is the two
nearest (`mind_plan`, `kRushAhead_s`); prefer the strongest and best armed, the weak one pointing
the quad to him. With the quad: go where the enemy was last called, never first through a door,
no blast near mates. Against an enemy quad (heard or called): weak players out of its way, the
rest wait for it to run out, then attack. Measure: frags per quad (now 1–3 quads a team), deaths
to a quad carrier, deaths of our quad carrier.

**12. Say less, say what matters.** `lost [X]` with the enemy count and the pack left; `enemy
quad`, `enemy rl at [X]`, `rl dead`; `switch` from a holder going to heal; reports on change;
`enemy at` once per place and enemy, not every eight seconds per bot (`kCallAgain_s`). Measure:
lines per minute, the share read (`mind.read`), how often a mate acted on a line (a `coming`
after a `help` or a `point`).

**13. Routes that avoid danger.** `nav_route` costs only the links' times (`plugins/shooter/
nav.hpp`); give it a per-node extra cost (a span) so a hurt player's route bends round the danger
spots and the enemy's believed places rather than through them. Measure: as in 2 and 3.

**14. Per-map plans as data** (the next step of `bot-strategies.md`): the hold spot and watch
points per key area, the staging nodes of a retake, the spam points, the turtle spots, and the
rush times (e1m2 quad: flood from 10 s, harder at 5, the mega at 2) loaded with the map's names.
