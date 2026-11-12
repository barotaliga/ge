---
type: Added
title: Weapon groups on the number keys
---

Shooter's Edge's `cl_weapongroups 1` makes each number key pick from a group of weapons: the best one carried with ammunition, and the next on a second press while the group is open. Each game has its own groups (the classic game's 1 rockets and grenades, 2 the beam gun, 3 the shotguns, 4 the nailguns, 5 the melee weapon), and `cl_weapongroup1` .. `cl_weapongroup9` change any of them. A HUD column on the right edge opens to the group's weapons on the key and folds into the weapon in hand when you fire or two seconds later, with a small gold mark while its group holds another weapon. The rule is any shooter's (`plugins/shooter/weapon_groups.hpp`).
