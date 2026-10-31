---
title: A test reads an entity field with entity_number, and a world draw needs the model list
date: 2026-10-31
scope: games/shooters_edge/qc/campaign/, games/shooters_edge/labs/
concepts: [qc, monsters, entity-field, entity-number, world-draw, model-list, se-client, deathmatch]
---

An entity field (`enemy`, `goalentity`) holds the VM's entity VALUE, not the edict number: a
test compares `pg::entity_number(vm, *entity_field(...))` with the edict, or every "is his enemy
the player" check fails while the monster is fighting fine. A monster with a melee blow that
lands only within 60 units but is chosen within 120 must close in during the attack (a charge
step), or it swings at the air forever. The player's own fields are set by his first frame:
change `flags` (no-target) after one frame, not before. And `create_world_draw` draws no item,
monster or player unless its `ModelLoad` carries the server's model list
(`{l.server->models, {}}`): a lab that passes `{}` shows the map empty and looks like a
renderer bug. A campaign program in a lab also needs `deathmatch 0`, as NEW GAME sets it, or
a mod that removes its monsters in deathmatch shows none.
