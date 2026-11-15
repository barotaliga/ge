---
type: Added
title: Mod packages carry a wasm32 module, and store builds load only it
---

`ge mod build --wasm` (or `-DGE_MOD_BUILD_WASM=ON`) builds every mod's WebAssembly module beside its library and lists it in the manifest as `file.wasm32`. The loader reads that row by the offer's form (`GameOffer::form`): a desktop build falls back to the module when the package has no library for its platform, and iOS and Android builds (`default_form()`) load only the module. `ge mod check --wasm` and Shooter's Edge's `secli mod run --wasm` load the module, and the game's servers host a module from a package. `LoadedMod::fields` holds the table's checked fields for either form.
