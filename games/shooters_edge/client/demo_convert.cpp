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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo_pack.hpp"

namespace shooters_edge::client {

namespace {

// A client recording to a single-player demo, in two stages: the recording made a multiview one
// (every player its recorder saw), then that read back riding the recorder's slot.
struct ClientToSingle final : ConvertStream {
  DemoConvert first, second;
  bool begun = false;
  std::vector<uint8_t> between;

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    if (!begun) {
      first.from = DemoFormat::client;
      first.to = second.from = DemoFormat::multiview;
      second.to = DemoFormat::single;
      first.options = second.options = c.options;
      first.stream = client_to_multiview_stream();
      second.stream = multiview_to_single_stream();
      begun = true;
    }
    first.in.insert(first.in.end(), c.in.begin(), c.in.end());
    c.in.clear();
    first.ended = c.ended;
    between.clear();
    if (!first.stream->step(first, between, error)) return false;
    if (c.options.pov == -1 && first.recorder >= 0) second.options.pov = first.recorder;
    c.recorder = first.recorder;
    second.in.insert(second.in.end(), between.begin(), between.end());
    second.ended = first.ended;
    if (!second.stream->step(second, out, error)) return false;
    // The notes: what the first stage dropped and rewrote, what the second did; its counts.
    c.notes = second.notes;
    for (const auto& [k, n] : first.notes.dropped) c.notes.dropped[k] += n;
    for (const auto& [k, n] : first.notes.rewritten) c.notes.rewritten[k] += n;
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> client_to_single_stream() {
  return std::make_unique<ClientToSingle>();
}

bool convert_supported(DemoFormat from, DemoFormat to) {
  return (from == DemoFormat::single &&
          (to == DemoFormat::multiview || to == DemoFormat::client)) ||
         (from == DemoFormat::multiview &&
          (to == DemoFormat::client || to == DemoFormat::single)) ||
         (from == DemoFormat::client &&
          (to == DemoFormat::multiview || to == DemoFormat::single)) ||
         ((from == DemoFormat::q2 || from == DemoFormat::q2_multiview) && to == DemoFormat::q2);
}

std::string format_words(DemoFormat f) {
  switch (f) {
    case DemoFormat::single:
      return "single-player demo (.dem)";
    case DemoFormat::client:
      return "client recording (" + plain_extension(DemoFormat::client) + ")";
    case DemoFormat::multiview:
      return "multiview recording (.mvd)";
    case DemoFormat::q2:
      return "Quake II recording (.dm2)";
    case DemoFormat::q2_multiview:
      return "Quake II multiview recording (.mvd2)";
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
  if (from == DemoFormat::client && to == DemoFormat::multiview) {
    c.stream = client_to_multiview_stream();
    return true;
  }
  if (from == DemoFormat::multiview && to == DemoFormat::client) {
    c.stream = multiview_to_client_stream();
    return true;
  }
  if (from == DemoFormat::multiview && to == DemoFormat::single) {
    c.stream = multiview_to_single_stream();
    return true;
  }
  if (from == DemoFormat::client && to == DemoFormat::single) {
    c.stream = client_to_single_stream();
    return true;
  }
  if (from == DemoFormat::q2 && to == DemoFormat::q2) {
    c.stream = q2_to_34_stream();
    return true;
  }
  if (from == DemoFormat::q2_multiview && to == DemoFormat::q2) {
    c.stream = mvd2_to_q2_stream();
    return true;
  }
  const std::string cl = plain_extension(DemoFormat::client);
  error = "a " + format_words(from) + " does not convert to a " + format_words(to) +
          " yet: a .dem converts to a .mvd or a " + cl + ", a .mvd to a " + cl +
          " or a .dem of one player's view, a " + cl + " to a .mvd or a .dem, and a .dm2 to a" +
          " protocol 34 .dm2, and a .mvd2 to a .dm2 of one player's view";
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
  // A multiview recording of Quake II converts from itself (the view `options.pov` asks for),
  // not from the view it opened as.
  const bool mvd2 = !d.multiview_source.empty();
  DemoConvert c;
  if (!convert_begin(c, mvd2 ? DemoFormat::q2_multiview : d.state.format, to, options, error))
    return false;
  convert_feed(c, mvd2 ? d.multiview_source : d.bytes);
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
    out = DemoFormat::q2;
  else if (ext == "mvd2")
    out = DemoFormat::q2_multiview;
  else
    return false;
  return true;
}

}  // namespace shooters_edge::client
