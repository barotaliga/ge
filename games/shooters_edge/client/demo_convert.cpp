// The conversion stream's calls and the whole-file conversion (demo_convert.hpp): the pair picked,
// the bytes fed, the steps taken; a file's format told by its name.
//
// ge:summary The demo conversion stream's calls, the whole-file conversion and format names
// ge:keywords demo-convert convert_begin convert_take convert_demo format_of_name format_words
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include "shooters_edge/client/demo_convert.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "shooters_edge/client/demo_pack.hpp"

namespace shooters_edge::client {

bool convert_supported(DemoFormat from, DemoFormat to) {
  return (from == DemoFormat::single &&
          (to == DemoFormat::multiview || to == DemoFormat::client)) ||
         (from == DemoFormat::multiview && to == DemoFormat::client) ||
         (from == DemoFormat::sequel && to == DemoFormat::sequel);
}

std::string format_words(DemoFormat f) {
  switch (f) {
    case DemoFormat::single:
      return "single-player demo (.dem)";
    case DemoFormat::client:
      return "client recording (" + plain_extension(DemoFormat::client) + ")";
    case DemoFormat::multiview:
      return "multiview recording (.mvd)";
    case DemoFormat::sequel:
      return "sequel recording (.dm2)";
  }
  return "recording";
}

bool convert_begin(DemoConvert& c, DemoFormat from, DemoFormat to, const ConvertOptions& options,
                   std::string& error) {
  c = DemoConvert{};
  c.from = from;
  c.to = to;
  c.options = options;
  if (from == DemoFormat::single && to == DemoFormat::multiview) {
    c.stream = single_to_multiview_stream();
    return true;
  }
  if (from == DemoFormat::single && to == DemoFormat::client) {
    c.stream = single_to_client_stream();
    return true;
  }
  if (from == DemoFormat::multiview && to == DemoFormat::client) {
    c.stream = multiview_to_client_stream();
    return true;
  }
  if (from == DemoFormat::sequel && to == DemoFormat::sequel) {
    c.stream = sequel_to_34_stream();
    return true;
  }
  error = "a " + format_words(from) + " does not convert to a " + format_words(to) +
          " yet: a .dem converts to a .mvd or a " + plain_extension(DemoFormat::client) +
          ", a .mvd to a " + plain_extension(DemoFormat::client) +
          " of one player's view, and a .dm2 to a protocol 34 .dm2";
  return false;
}

void convert_feed(DemoConvert& c, std::span<const uint8_t> bytes) {
  c.in.insert(c.in.end(), bytes.begin(), bytes.end());
}

void convert_end(DemoConvert& c) { c.ended = true; }

bool convert_take(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) {
  if (c.failed) {
    error = c.error;
    return false;
  }
  if (!c.stream) {
    error = "the conversion was not begun";
    return false;
  }
  if (!c.stream->step(c, out, error)) {
    c.failed = true;
    c.error = error;
    return false;
  }
  return true;
}

bool convert_demo(std::vector<uint8_t> bytes, DemoFormat to, const ConvertOptions& options,
                  std::vector<uint8_t>& out, ConvertNotes& notes, std::string& error) {
  Demo d;
  if (!open_demo(std::move(bytes), d, error)) return false;
  DemoConvert c;
  if (!convert_begin(c, d.state.format, to, options, error)) return false;
  convert_feed(c, d.bytes);
  convert_end(c);
  out.clear();
  const bool ok = convert_take(c, out, error);
  notes = c.notes;
  return ok;
}

bool format_of_name(const std::string& path, DemoFormat& out) {
  std::string name = path.substr(path.find_last_of("/\\") + 1);
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (name.ends_with(".gz")) name.resize(name.size() - 3);
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos) return false;
  std::string ext = name.substr(dot + 1);
  if (ext.size() == 4 && ext.back() == 'z') ext.pop_back();  // a packed name: .mvdz
  if (ext == "dem")
    out = DemoFormat::single;
  else if ("." + ext == plain_extension(DemoFormat::client))
    out = DemoFormat::client;
  else if (ext == "mvd")
    out = DemoFormat::multiview;
  else if (ext == "dm2")
    out = DemoFormat::sequel;
  else
    return false;
  return true;
}

}  // namespace shooters_edge::client
