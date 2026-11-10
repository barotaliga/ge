# Classic vertex-animated models and sprites read into one view (skins, the seam split, every
# frame a morph target), refusing damage; the fixture writers; per-frame smooth normals and the
# optional point-normal rounding. Generic to any game on these file kinds (skill: morph). Its lab
# draws with the labs' model view (APP lab); a skeletal model baked into frames (skeletal.hpp)
# poses it with engine/assets, so the plugin itself links assets.
ge_add_plugin(morph ENGINE core assets APP lab)
