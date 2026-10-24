// `ge model import`: a model someone else made free taken in as one self-contained binary glTF
// under a game's own name (skill: free-assets). Free kits ship a .glb or a .gltf whose buffers and
// textures sit in files beside it (often one shared `Textures/colormap.png` that differs from kit
// to kit), so copying the file alone loses its colours. The import:
//
//   1. reads a .glb (its JSON and BIN chunks) or a .gltf (its buffers from files or data: URIs);
//   2. packs every buffer into the one BIN chunk, rebasing each buffer view onto it;
//   3. packs every image that names a file or a data: URI into the BIN chunk as a buffer view
//      with its mime type (PNG or JPEG, read from its bytes);
//   4. writes <to>/<name>.glb, then loads it with the engine's own loader (assets/gltf.hpp) and
//      answers with its joints, meshes, triangles, clips and size: a load error is an error, a
//      size over --budget-kib a warning.
//
// ge:summary ge model import: a free kit's model packed into one .glb, loaded and measured
// ge:keywords cli ge model model-import glb gltf pack embed texture image buffer-view bin chunk
// ge:keywords colormap free-assets budget load_gltf
// ge:skill agent-interface assets
// ge:test tests/cli_model_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_model(const Args& a);

// Exposed for the tests.
struct Packed {
  std::vector<uint8_t> glb;
  int buffers_packed = 0;  // buffers that were files or data: URIs
  int images_packed = 0;   // images that were files or data: URIs
};
// The file at `source` as one self-contained .glb; false with `error` set (what, and where).
bool pack_glb(const fs::path& source, Packed& out, std::string& error);
// The same from bytes already read; `base` is the directory relative URIs resolve against.
bool pack_glb_bytes(const std::vector<uint8_t>& bytes, bool binary, const fs::path& base,
                    Packed& out, std::string& error);

}  // namespace ge::cli
