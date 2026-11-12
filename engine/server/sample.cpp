// The sample game, defined.
//
// ge:summary The server core's sample game: create, frame, hash
// ge:keywords server sample-game walkers
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/sample.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "net/packet.hpp"

namespace ge::server {

namespace {

constexpr int32_t kRing = 1000;

struct Sample {
  core::Rng rng{1};
  std::vector<int32_t> at;
  std::vector<int8_t> moved;  // each bot's last step: -1, 0 or 1
  int64_t frames = 0;
};

uint64_t ring_hash(int64_t frames, const std::vector<int32_t>& at) {
  uint64_t h = 1469598103934665603ull ^ static_cast<uint64_t>(frames);
  for (int32_t a : at) h = (h ^ static_cast<uint32_t>(a)) * 1099511628211ull;
  return h;
}

void put_u(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}

bool get_u(std::span<const uint8_t> b, size_t& at, uint64_t& v) {
  v = 0;
  for (int shift = 0; shift < 64 && at < b.size(); shift += 7) {
    const uint8_t c = b[at++];
    v |= static_cast<uint64_t>(c & 0x7F) << shift;
    if ((c & 0x80) == 0) return true;
  }
  return false;
}

void* create(const ServerConfig& c, std::string& error) {
  if (c.map != "ring") {
    error = "the sample game has one map, 'ring', not '" + c.map + "'";
    return nullptr;
  }
  auto* s = new Sample;
  s->rng = core::make_rng(c.seed);
  s->at.resize(static_cast<size_t>(c.bots));
  for (int32_t& a : s->at) a = core::int_in(s->rng, 0, kRing - 1);
  return s;
}

void destroy(void* g) { delete static_cast<Sample*>(g); }

double frame_seconds(const void*) { return 1.0 / 60.0; }

void frame(void* g) {
  auto* s = static_cast<Sample*>(g);
  s->moved.resize(s->at.size());
  for (size_t i = 0; i < s->at.size(); i++) {
    const int32_t step = core::int_in(s->rng, -1, 1);
    s->moved[i] = static_cast<int8_t>(step);
    s->at[i] = (s->at[i] + step + kRing) % kRing;
  }
  s->frames++;
}

int seated(const void* g) { return static_cast<int>(static_cast<const Sample*>(g)->at.size()); }

uint64_t hash(const void* g) {
  const auto* s = static_cast<const Sample*>(g);
  return ring_hash(s->frames, s->at);
}

void tv_frame(const void* g, std::vector<uint8_t>& out) {
  const auto* s = static_cast<const Sample*>(g);
  for (size_t i = 0; i < s->moved.size(); i += 4) {
    uint8_t b = 0;
    for (size_t k = 0; k < 4 && i + k < s->moved.size(); k++)
      b |= static_cast<uint8_t>((s->moved[i + k] + 1) << (2 * k));
    out.push_back(b);
  }
}

void tv_keyframe(const void* g, std::vector<uint8_t>& out) {
  const auto* s = static_cast<const Sample*>(g);
  put_u(out, static_cast<uint64_t>(s->frames));
  put_u(out, s->at.size());
  for (int32_t a : s->at) put_u(out, static_cast<uint32_t>(a));
}

// Its public status: a walker per bot, scored by its cell, in the mode "walk".
void fill_status(const void* g, net::Status& st) {
  const auto* s = static_cast<const Sample*>(g);
  st.mode = "walk";
  for (size_t i = 0; i < s->at.size(); i++) {
    net::StatusPlayer p;
    p.name = "walker" + std::to_string(i + 1);
    p.score = s->at[i];
    p.minutes = static_cast<uint16_t>(s->frames / 3600);
    p.flags = net::kPlayerBot;
    st.players.push_back(p);
  }
  st.keys.push_back({"frames", std::to_string(s->frames)});
}

std::string status(const void* g) {
  const auto* s = static_cast<const Sample*>(g);
  char out[48];
  std::snprintf(out, sizeof out, "first=%d", s->at.empty() ? -1 : s->at[0]);
  return out;
}

ServerGame make() {
  ServerGame g;
  g.binary = "sample_server";
  g.title = "the sample game";
  g.protocol = net::protocol_id("server_sample", 1);
  g.default_port = 27600;
  g.discovery_port = 27610;
  g.max_clients = 16;
  g.default_map = "ring";
  g.create = create;
  g.destroy = destroy;
  g.frame_seconds = frame_seconds;
  g.frame = frame;
  g.seated = seated;
  g.hash = hash;
  g.status = status;
  g.fill_status = fill_status;
  g.version = 1;
  g.tv_frame = tv_frame;
  g.tv_keyframe = tv_keyframe;
  return g;
}

}  // namespace

const ServerGame& sample_game() {
  static const ServerGame g = make();
  return g;
}

bool sample_watch_keyframe(SampleWatch& w, std::span<const uint8_t> bytes) {
  size_t at = 0;
  uint64_t frames = 0, count = 0;
  if (!get_u(bytes, at, frames) || !get_u(bytes, at, count) || count > bytes.size()) {
    w.error = "a keyframe's head is damaged";
    return false;
  }
  std::vector<int32_t> ring(count);
  for (int32_t& a : ring) {
    uint64_t v = 0;
    if (!get_u(bytes, at, v) || v >= kRing) {
      w.error = "a keyframe's cells are damaged";
      return false;
    }
    a = static_cast<int32_t>(v);
  }
  w.frames = static_cast<int64_t>(frames);
  w.at = std::move(ring);
  w.started = true;
  return true;
}

bool sample_watch_frame(SampleWatch& w, std::span<const uint8_t> bytes) {
  if (!w.started) return true;  // before its first keyframe a viewer has nothing to move
  if (bytes.size() != (w.at.size() + 3) / 4) {
    w.error = "a frame of " + std::to_string(bytes.size()) + " bytes for " +
              std::to_string(w.at.size()) + " bots";
    return false;
  }
  for (size_t i = 0; i < w.at.size(); i++) {
    const int step = ((bytes[i / 4] >> (2 * (i % 4))) & 3) - 1;
    w.at[i] = (w.at[i] + step + kRing) % kRing;
  }
  w.frames++;
  return true;
}

uint64_t sample_watch_hash(const SampleWatch& w) { return ring_hash(w.frames, w.at); }

}  // namespace ge::server
