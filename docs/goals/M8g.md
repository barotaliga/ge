Status: done
Milestone: **`ge asset`**: the game's models built in headless Blender by Python builders off the game's own data (the one exception to C++ only: builders run offline and ship in no game), a source stamp so a model is rebuilt only when its sources change, and a size budget on every committed model
Exit: `ge asset` builds a model from its builder on the Linux container; a test fails a model older than its sources or over its budget
Labs: `models` (each built model)

`ge asset list`, `build` and `check` over an `assets` executable that holds the game's data sets (`GE_ASSET_DATA`) and checks every model; `engine/assets/manifest.hpp`'s manifest, stamp and budget; the builders' library `ge_asset.py` compiled into `ge`; the first asset, the skier on the reference skis with his poles, built off the rig in `plugins/skier/app/rig.hpp` by Blender 5.2.2 as the bpy module on the Linux container: 1,808 triangles, 102 KiB of a 384 KiB budget, the same bytes on a rebuild; the `assets_check` test fails a model older than its builder, a source or its data, a missing one and one over budget
