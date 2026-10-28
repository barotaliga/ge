# The classic content enhanced on the player's own machine: a copy of the classic shareware pack
# the player already has (the release zip as published, or the pack in an installed game folder)
# verified unchanged, then enhanced into a local cache: sharper textures with generated normal,
# height, roughness and glow maps, and the lightmap stage rebaked light plugs into. Nothing it
# makes is committed or shipped (skill: classicenhance).
ge_add_plugin(classicenhance ENGINE core assets lab vfs PLUGINS brush)
