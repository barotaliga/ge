// `ge model`'s body (model.hpp): the glTF read, its buffers and images packed into one BIN chunk,
// the .glb written and loaded back with the engine's loader.
//
// ge:summary ge model's body: read a .glb or .gltf, pack buffers and images, write, load, measure
// ge:keywords cli model run_model pack_glb pack_glb_bytes base64 data-uri mime glb-chunks
// ge:skill agent-interface assets
// ge:test tests/cli_model_test.cpp
#include "cli/model.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "assets/gltf.hpp"
#include "cli/json.hpp"
#include "cli/json_tree.hpp"
#include "core/num_text.hpp"

namespace ge::cli {

namespace {

constexpr uint32_t kGlbMagic = 0x46546C67, kJsonChunk = 0x4E4F534A, kBinChunk = 0x004E4942;

Outcome failed(const std::string& what, const std::string& where, const std::string& next,
               int exit = 1) {
  Outcome o;
  o.findings.push_back({Severity::error, what, where, next});
  o.exit = exit;
  return o;
}

uint32_t get32(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void pad4(std::vector<uint8_t>& b, uint8_t with) {
  while (b.size() % 4) b.push_back(with);
}

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

// A data: URI's bytes (base64 only); false when it is not one.
bool data_uri(const std::string& uri, std::vector<uint8_t>& out) {
  if (!uri.starts_with("data:")) return false;
  const size_t comma = uri.find(',');
  if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos) {
    return false;
  }
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = comma + 1; i < uri.size(); i++) {
    const char c = uri[i];
    int v = -1;
    if (c >= 'A' && c <= 'Z') v = c - 'A';
    if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
    if (c >= '0' && c <= '9') v = c - '0' + 52;
    if (c == '+' || c == '-') v = 62;
    if (c == '/' || c == '_') v = 63;
    if (v < 0) continue;  // '=' padding, whitespace
    acc = acc << 6 | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>(acc >> bits));
    }
  }
  return true;
}

// A relative URI's file: percent escapes decoded, resolved against `base`.
fs::path uri_file(const fs::path& base, const std::string& uri) {
  std::string s;
  for (size_t i = 0; i < uri.size(); i++) {
    if (uri[i] == '%' && i + 2 < uri.size()) {
      s += static_cast<char>(std::strtol(uri.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    } else {
      s += uri[i];
    }
  }
  return base / fs::path(s);
}

std::string mime_of(const std::vector<uint8_t>& b) {
  if (b.size() > 4 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') return "image/png";
  if (b.size() > 3 && b[0] == 0xff && b[1] == 0xd8) return "image/jpeg";
  return "";
}

// A URI's bytes, from a data: URI or a file beside the model.
bool uri_bytes(const fs::path& base, const std::string& uri, std::vector<uint8_t>& out,
               std::string& error) {
  if (uri.starts_with("data:")) {
    if (data_uri(uri, out)) return true;
    error = "a data: URI that is not base64";
    return false;
  }
  const fs::path file = uri_file(base, uri);
  if (read_file(file, out)) return true;
  error = "the model names " + uri + ", and " + file.string() + " is not there";
  return false;
}

}  // namespace

bool pack_glb_bytes(const std::vector<uint8_t>& bytes, bool binary, const fs::path& base,
                    Packed& out, std::string& error) {
  std::string json_text;
  std::vector<uint8_t> glb_bin;
  bool has_glb_bin = false;
  if (binary) {
    if (bytes.size() < 20 || get32(bytes, 0) != kGlbMagic || get32(bytes, 4) != 2) {
      error = "not a binary glTF 2.0 file";
      return false;
    }
    size_t at = 12;
    while (at + 8 <= bytes.size()) {
      const uint32_t len = get32(bytes, at), type = get32(bytes, at + 4);
      if (at + 8 + len > bytes.size()) {
        error = "a chunk runs past the file's end";
        return false;
      }
      if (type == kJsonChunk) {
        json_text.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at + 8),
                         bytes.begin() + static_cast<std::ptrdiff_t>(at + 8 + len));
      } else if (type == kBinChunk && !has_glb_bin) {
        glb_bin.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at + 8),
                       bytes.begin() + static_cast<std::ptrdiff_t>(at + 8 + len));
        has_glb_bin = true;
      }
      at += 8 + len;
    }
  } else {
    json_text.assign(bytes.begin(), bytes.end());
  }
  JsonNode doc;
  if (!parse_json(json_text, doc, error)) {
    error = "its JSON does not parse: " + error;
    return false;
  }

  // Every buffer into one BIN, each starting on a 4-byte boundary.
  std::vector<uint8_t> bin;
  std::vector<size_t> offset;
  if (JsonNode* buffers = doc.find("buffers"); buffers && buffers->kind == JsonNode::Kind::array) {
    for (JsonNode& b : buffers->items) {
      pad4(bin, 0);
      offset.push_back(bin.size());
      const JsonNode* uri = b.find("uri");
      std::vector<uint8_t> data;
      if (!uri) {
        if (!has_glb_bin) {
          error = "a buffer with no uri outside a .glb";
          return false;
        }
        data = glb_bin;
      } else {
        if (!uri_bytes(base, uri->text, data, error)) return false;
        out.buffers_packed++;
      }
      const auto len =
          static_cast<size_t>(b.find("byteLength") ? b.find("byteLength")->number() : 0);
      if (data.size() < len) {
        error = "a buffer is shorter than its byteLength";
        return false;
      }
      bin.insert(bin.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(len));
    }
  }
  if (JsonNode* views = doc.find("bufferViews"); views && views->kind == JsonNode::Kind::array) {
    for (JsonNode& v : views->items) {
      const auto b = static_cast<size_t>(v.find("buffer") ? v.find("buffer")->number() : 0);
      if (b >= offset.size()) {
        error = "a buffer view names a buffer that is not there";
        return false;
      }
      const double off = v.find("byteOffset") ? v.find("byteOffset")->number() : 0;
      v.at("buffer") = JsonNode::of_number(0);
      v.at("byteOffset") = JsonNode::of_number(off + static_cast<double>(offset[b]));
    }
  }

  // Every image that is a file or a data: URI, into the BIN as a view of its own.
  if (JsonNode* images = doc.find("images"); images && images->kind == JsonNode::Kind::array) {
    for (JsonNode& im : images->items) {
      const JsonNode* uri = im.find("uri");
      if (!uri) continue;
      std::vector<uint8_t> data;
      if (!uri_bytes(base, uri->text, data, error)) return false;
      const std::string mime = mime_of(data);
      if (mime.empty()) {
        error = "the image " + uri->text + " is neither PNG nor JPEG";
        return false;
      }
      pad4(bin, 0);
      JsonNode view;
      view.kind = JsonNode::Kind::object;
      view.at("buffer") = JsonNode::of_number(0);
      view.at("byteOffset") = JsonNode::of_number(static_cast<double>(bin.size()));
      view.at("byteLength") = JsonNode::of_number(static_cast<double>(data.size()));
      bin.insert(bin.end(), data.begin(), data.end());
      JsonNode& views = doc.at("bufferViews");
      views.kind = JsonNode::Kind::array;
      views.items.push_back(std::move(view));
      im.erase("uri");
      im.at("bufferView") = JsonNode::of_number(static_cast<double>(views.items.size() - 1));
      im.at("mimeType") = JsonNode::of_string(mime);
      out.images_packed++;
    }
  }
  pad4(bin, 0);
  if (!bin.empty()) {
    JsonNode one;
    one.kind = JsonNode::Kind::object;
    one.at("byteLength") = JsonNode::of_number(static_cast<double>(bin.size()));
    JsonNode& buffers = doc.at("buffers");
    buffers.kind = JsonNode::Kind::array;
    buffers.items.clear();
    buffers.items.push_back(std::move(one));
  }

  std::string js = dump_json(doc);
  while (js.size() % 4) js += ' ';
  std::vector<uint8_t>& g = out.glb;
  g.clear();
  put32(g, kGlbMagic);
  put32(g, 2);
  put32(g, static_cast<uint32_t>(12 + 8 + js.size() + (bin.empty() ? 0 : 8 + bin.size())));
  put32(g, static_cast<uint32_t>(js.size()));
  put32(g, kJsonChunk);
  g.insert(g.end(), js.begin(), js.end());
  if (!bin.empty()) {
    put32(g, static_cast<uint32_t>(bin.size()));
    put32(g, kBinChunk);
    g.insert(g.end(), bin.begin(), bin.end());
  }
  return true;
}

bool pack_glb(const fs::path& source, Packed& out, std::string& error) {
  std::vector<uint8_t> bytes;
  if (!read_file(source, bytes)) {
    error = "no such file: " + source.string();
    return false;
  }
  std::string ext = source.extension().string();
  for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const bool binary = ext == ".glb" || (bytes.size() >= 4 && get32(bytes, 0) == kGlbMagic);
  return pack_glb_bytes(bytes, binary, source.parent_path(), out, error);
}

Outcome run_model(const Args& a) {
  const fs::path source = a.words[0];
  const std::string name = a.words[1];
  double budget = 300;
  if (const std::string b = flag_value(a, "budget-kib"); !b.empty()) {
    char* end = nullptr;
    budget = std::strtod(b.c_str(), &end);
    if (!end || *end || budget <= 0) {
      return failed("--budget-kib=" + b + " is not a size", "the command line", "--budget-kib=300",
                    2);
    }
  }
  Packed p;
  std::string error;
  if (!pack_glb(source, p, error)) {
    return failed(error, source.string(), "pass a .glb or a .gltf with its files beside it");
  }
  const assets::LoadResult loaded = assets::load_gltf(p.glb, source.string());
  if (!loaded.ok) {
    return failed("the packed model does not load: " + loaded.error, source.string(),
                  "open it in a glTF viewer; the engine's loader names what it refused");
  }
  const fs::path file = fs::path(flag_value(a, "to", "models")) / (name + ".glb");
  std::error_code ec;
  if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
  {
    std::ofstream o(file, std::ios::binary);
    o.write(reinterpret_cast<const char*>(p.glb.data()),
            static_cast<std::streamsize>(p.glb.size()));
    if (!o) return failed("could not write " + file.string(), file.string(), "");
  }
  size_t triangles = 0;
  for (const assets::Mesh& m : loaded.model.meshes) triangles += m.indices.size() / 3;
  std::vector<std::string> clips;
  clips.reserve(loaded.model.clips.size());
  for (const assets::Clip& c : loaded.model.clips) clips.push_back(c.name);
  const double kib = static_cast<double>(p.glb.size()) / 1024;
  Outcome o;
  const std::string rel = rel_path(fs::current_path(), file);
  o.written.push_back(rel);
  o.text = "  " + name + ": " + core::num::to_fixed(kib, 1) + " KiB, " +
           std::to_string(loaded.model.joints.size()) + " joint(s), " +
           std::to_string(loaded.model.meshes.size()) + " mesh(es), " + std::to_string(triangles) +
           " triangles, " + std::to_string(clips.size()) + " clip(s); packed " +
           std::to_string(p.images_packed) + " image(s) and " + std::to_string(p.buffers_packed) +
           " buffer file(s)\n";
  for (const std::string& n : loaded.notes) o.text += "  note: " + n + "\n";
  if (kib > budget) {
    o.findings.push_back({Severity::warn,
                          name + " is " + core::num::to_fixed(kib, 0) + " KiB, over the " +
                              core::num::to_fixed(budget, 0) + " KiB budget",
                          rel,
                          "pick a lighter model or cut its textures; raising it is a decision"});
  }
  o.result = json_obj({{"name", json_str(name)},
                       {"file", json_str(rel)},
                       {"kib", core::num::to_fixed(kib, 1)},
                       {"joints", json_num(static_cast<long long>(loaded.model.joints.size()))},
                       {"meshes", json_num(static_cast<long long>(loaded.model.meshes.size()))},
                       {"triangles", json_num(static_cast<long long>(triangles))},
                       {"clips", json_strs(clips)},
                       {"images_packed", json_num(p.images_packed)},
                       {"buffers_packed", json_num(p.buffers_packed)}});
  o.summary = "wrote " + rel;
  o.next = "look at it: build/<preset>/labs models --model=" + rel;
  return o;
}

}  // namespace ge::cli
