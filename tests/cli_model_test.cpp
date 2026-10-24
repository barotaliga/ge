// engine/cli/model.hpp and json_tree.hpp, held on a hand-written model:
//
//   * THE TREE: a document read and written back keeps its keys' order and its numbers' spelling,
//     escapes and \u sequences round-trip, and a broken document says where.
//   * THE PACK: a .gltf whose buffer and texture are files beside it, and one whose buffer is a
//     data: URI, become one .glb that the engine's loader reads, its image a buffer view with a
//     mime type and no file left named.
//
// ge:summary Tests for ge model import and the JSON tree: round trips, packing a .gltf into a .glb
// ge:keywords test cli model import gltf glb pack json tree
// ge:skill agent-interface assets testing
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "check.hpp"
#include "cli/json_tree.hpp"
#include "cli/model.hpp"

namespace c = ge::cli;
namespace fs = std::filesystem;

namespace {

// One triangle: three float3 positions, then three uint16 indices (padded to 4 bytes).
std::vector<uint8_t> triangle_buffer() {
  const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint16_t idx[4] = {0, 1, 2, 0};
  std::vector<uint8_t> b(sizeof(pos) + sizeof(idx));
  std::memcpy(b.data(), pos, sizeof(pos));
  std::memcpy(b.data() + sizeof(pos), idx, sizeof(idx));
  return b;
}

std::string gltf_text(const std::string& buffer_uri) {
  return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
         R"("nodes":[{"name":"tri","mesh":0}],)"
         R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)"
         R"("materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],)"
         R"("textures":[{"source":0}],"images":[{"uri":"Textures/colour%20map.png"}],)"
         R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",)"
         R"("min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],)"
         R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],)"
         R"("buffers":[{"byteLength":44,"uri":")" +
         buffer_uri + R"("}]})";
}

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream o(p, std::ios::binary);
  o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::string base64(const std::vector<uint8_t>& b) {
  static constexpr char k[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s;
  for (size_t i = 0; i < b.size(); i += 3) {
    const uint32_t v = static_cast<uint32_t>(b[i]) << 16 |
                       (i + 1 < b.size() ? static_cast<uint32_t>(b[i + 1]) << 8 : 0) |
                       (i + 2 < b.size() ? b[i + 2] : 0);
    s += k[v >> 18 & 63];
    s += k[v >> 12 & 63];
    s += i + 1 < b.size() ? k[v >> 6 & 63] : '=';
    s += i + 2 < b.size() ? k[v & 63] : '=';
  }
  return s;
}

// The packed .glb's JSON chunk, parsed.
c::JsonNode glb_json(const std::vector<uint8_t>& g) {
  const uint32_t len = g[12] | g[13] << 8 | g[14] << 16 | static_cast<uint32_t>(g[15]) << 24;
  c::JsonNode doc;
  std::string error;
  c::parse_json(std::string(g.begin() + 20, g.begin() + 20 + len), doc, error);
  return doc;
}

const std::vector<uint8_t> kPng = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 0};

}  // namespace

TEST(a_json_document_round_trips_as_it_was) {
  const std::string text = R"({"z":1,"a":[1.50,-2e3,true,null],"s":"t\"ab\\\né😀","o":{}})";
  c::JsonNode doc;
  std::string error;
  CHECK_MSG(c::parse_json(text, doc, error), error);
  CHECK(doc.members.size() == 4 && doc.members[0].first == "z" && doc.members[1].first == "a");
  CHECK(doc.find("a")->items[0].text == "1.50" && doc.find("a")->items[1].number() == -2000);
  CHECK(doc.find("s")->text == "t\"ab\\\n\xc3\xa9\xf0\x9f\x98\x80");
  const std::string out = c::dump_json(doc);
  CHECK_MSG(out == R"({"z":1,"a":[1.50,-2e3,true,null],"s":"t\"ab\\\n)"
                   "\xc3\xa9\xf0\x9f\x98\x80"
                   R"(","o":{}})",
            out);
  doc.erase("z");
  doc.at("n") = c::JsonNode::of_number(36);
  CHECK(c::dump_json(doc).starts_with(R"({"a":)") && c::dump_json(doc).ends_with(R"("n":36})"));
  CHECK(!c::parse_json(R"({"a":1,})", doc, error) && error.find("byte") != std::string::npos);
}

TEST(a_gltf_with_files_beside_it_packs_into_one_glb) {
  const fs::path dir = fs::temp_directory_path() / "ge-cli-model-test";
  fs::remove_all(dir);
  write(dir / "tri.bin", triangle_buffer());
  write(dir / "Textures/colour map.png", kPng);
  const std::string text = gltf_text("tri.bin");
  write(dir / "tri.gltf", std::vector<uint8_t>(text.begin(), text.end()));
  c::Packed p;
  std::string error;
  CHECK_MSG(c::pack_glb(dir / "tri.gltf", p, error), error);
  CHECK(p.images_packed == 1 && p.buffers_packed == 1);
  CHECK(p.glb.size() % 4 == 0 && p.glb[0] == 'g' && p.glb[1] == 'l' && p.glb[2] == 'T');
  const c::JsonNode doc = glb_json(p.glb);
  const c::JsonNode& image = doc.find("images")->items[0];
  CHECK(!image.find("uri") && image.find("mimeType")->text == "image/png");
  CHECK(image.find("bufferView")->number() == 2);
  CHECK(doc.find("buffers")->items.size() == 1 && !doc.find("buffers")->items[0].find("uri"));
  const ge::assets::LoadResult loaded = ge::assets::load_gltf(p.glb, "tri.glb");
  CHECK_MSG(loaded.ok, loaded.error);
  CHECK(loaded.model.meshes.size() == 1 && loaded.model.meshes[0].indices.size() == 3);

  // The texture missing is an error that names it.
  fs::remove(dir / "Textures/colour map.png");
  CHECK(!c::pack_glb(dir / "tri.gltf", p, error) &&
        error.find("colour map.png") != std::string::npos);
  fs::remove_all(dir);
}

TEST(a_data_uri_buffer_packs_the_same) {
  const fs::path dir = fs::temp_directory_path() / "ge-cli-model-test-2";
  fs::remove_all(dir);
  write(dir / "Textures/colour map.png", kPng);
  const std::string text =
      gltf_text("data:application/octet-stream;base64," + base64(triangle_buffer()));
  c::Packed p;
  std::string error;
  CHECK_MSG(c::pack_glb_bytes(std::vector<uint8_t>(text.begin(), text.end()), false, dir, p, error),
            error);
  const ge::assets::LoadResult loaded = ge::assets::load_gltf(p.glb, "tri.glb");
  CHECK_MSG(loaded.ok, loaded.error);
  // Packing a .glb again changes nothing it reads.
  c::Packed again;
  CHECK_MSG(c::pack_glb_bytes(p.glb, true, dir, again, error), error);
  CHECK(again.images_packed == 0 && again.buffers_packed == 0 && again.glb == p.glb);
  fs::remove_all(dir);
}
