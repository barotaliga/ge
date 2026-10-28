# The classic content enhanced on the player's own machine: the classic packs the player already
# has (the shareware release zip as published, or the packs in an installed game folder, the full
# game's included), then enhanced into a local cache: sharper textures with generated normal,
# height, roughness and glow maps, and the lightmap stage rebaked light plugs into; and the
# pack's held weapons as view models, lit by the map under the player. Nothing it
# makes is committed or shipped (skill: classicenhance).
ge_add_plugin(classicenhance ENGINE core assets audio camera lab vfs viewmodel PLUGINS brush morph)
