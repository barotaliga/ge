---
title: A bot beside a person waits behind where he looks and steps out of his way, never at his elbow
date: 2026-11-23
scope: plugins/shooterbots/party.cpp, games/shooters_edge/bots/coop_episode.cpp, games/shooters_edge/client/coop_party.cpp
concepts: [party, helper, co-op, leash, give-way, doorway, blocked, party-stay]
---

- A helper sent back to the person with nothing near left walked onto him, or stopped at his
  elbow: in e1m1's corridors that blocked the person's way, and a door a player stands in turns
  back, so the person waited on it for minutes (449 s against 124 s alone). Send it to a node
  kPartyNear behind where the person looks (he looks where he goes) and step straight away from
  him inside kPartyClear (`party_give_way`): 80 s.
- Judge a helper by the person's time, not the helper's: the stand-in leader of `--beside` shows
  whether the bots help or hinder. Read the leader's plan log (`--plan=1`): "pilot wait" at one
  place for minutes is a body in a door.
