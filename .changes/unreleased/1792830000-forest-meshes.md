---
type: Added
title: The forest's own trees
---

`plugins/mountain/app/trees.hpp` builds every tree of a snow mountain in code: five forms (a conifer's drooping tiers, a pine's flat pads on a bare trunk, a winter larch's whorls of thin arms, a bare broadleaf forking into limbs and twigs, a snag), four variants a form picked by the tree's place, and three levels of detail that keep the full tree's outline. The snow is in the mesh, chosen by how far each face turns up as the tree stands, so every tier is white above and shaded below its lip; each of the twenty kinds has its own needles, bark and twigs. Fall Line's forest draws them, and the new `forest` lab shows every form, variant, LOD and kind with each mesh's triangles held to its LOD's budget.
