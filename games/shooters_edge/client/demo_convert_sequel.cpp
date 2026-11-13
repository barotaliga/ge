// A SEQUEL RECORDING OF AN OLDER PROTOCOL REWRITTEN AS PROTOCOL 34 (demo_convert.hpp), block by
// block as it arrives, so a stock client that takes only protocol 34 (a live viewer of the TV,
// which refuses 26) plays the full game's own demos.
//
// THE DIFFERENCE, measured on the full game's two recordings (protocol 26): every message reads
// with protocol 34's layouts except the frame, whose header lacks the suppress count (the byte
// after the frame number and the frame it deltas from). So the rewrite sets the server data's
// protocol to 34 and puts a suppress count of 0 into every frame, the block's length one more.
// A protocol 34 recording comes back byte for byte. Each block is walked by the sequel reader
// (demo_sequel.cpp, its message spans), so a frame after a reliable message is found too.
//
// ge:summary A sequel recording of protocol 26 rewritten as protocol 34, block by block
// ge:keywords demo-convert sequel dm2 protocol-26 protocol-34 suppress-count frame rewrite tv-demo
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include <cstring>
#include <string>
#include <vector>

#include "shooters_edge/client/demo_convert.hpp"

namespace shooters_edge::client {

namespace {

constexpr uint8_t kServerData = 12, kFrame = 20;
constexpr int32_t kMostBlock = 1400;

int32_t i32_at(std::span<const uint8_t> b, size_t at) {
  uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);
  return static_cast<int32_t>(v);
}

void put_i32(std::vector<uint8_t>& out, int32_t v) {
  const auto u = static_cast<uint32_t>(v);
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(u >> (8 * i)));
}

struct SequelTo34 final : ConvertStream {
  Demo d;
  std::vector<DemoSpan> spans;
  bool begun = false, stopped = false, end_written = false;

  bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) override {
    if (!begun) {
      sequel_stream_begin(d);
      d.spans = &spans;
      begun = true;
    }
    const std::span<const uint8_t> b(c.in);
    size_t pos = 0;
    while (!stopped && pos + 4 <= b.size()) {
      const int32_t len = i32_at(b, pos);
      if (len == -1) {
        stopped = true;
        pos += 4;
        break;
      }
      if (len <= 0 || len > kMostBlock) {
        error = "the block length " + std::to_string(len) + " at byte " + std::to_string(pos) +
                " is not a block's (1 to 1400): the recording is damaged";
        return false;
      }
      if (pos + 4 + static_cast<size_t>(len) > b.size()) break;
      const std::span<const uint8_t> block = b.subspan(pos, 4 + static_cast<size_t>(len));
      // The block alone, read by the sequel reader, which keeps its state between blocks.
      d.bytes.assign(block.begin(), block.end());
      d.pos = 0;
      if (next_sequel(d, error) == DemoRead::error) {
        error += " (in the block at byte " + std::to_string(pos) + ")";
        return false;
      }
      const int protocol = d.state.protocol;  // the level's, once its server data is read
      std::vector<uint8_t> body(block.begin() + 4, block.end());
      // Back to front, so an insert leaves the earlier spans where they are.
      for (auto sp = spans.rbegin(); sp != spans.rend(); ++sp) {
        if (sp->id == kServerData) {
          c.notes.levels++;
          if (i32_at(body, sp->at + 1) != 34) {
            const uint8_t v34[4] = {34, 0, 0, 0};
            std::memcpy(body.data() + sp->at + 1, v34, 4);
            c.notes.rewritten["the server data's protocol as 34"]++;
          }
        } else if (sp->id == kFrame) {
          c.notes.frames++;
          if (protocol == 26) {
            body.insert(body.begin() + static_cast<std::ptrdiff_t>(sp->at) + 9, uint8_t{0});
            c.notes.rewritten["a frame given its suppress count"]++;
          }
        }
      }
      put_i32(out, static_cast<int32_t>(body.size()));
      out.insert(out.end(), body.begin(), body.end());
      pos += block.size();
    }
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    if ((c.ended || stopped) && !end_written) {
      if (!stopped && !c.in.empty()) c.notes.dropped["a block cut short at the end"]++;
      if (!c.options.no_end) put_i32(out, -1);
      end_written = true;
      c.in.clear();
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<ConvertStream> sequel_to_34_stream() { return std::make_unique<SequelTo34>(); }

}  // namespace shooters_edge::client
