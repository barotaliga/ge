// The sample game, defined.
//
// ge:summary The server core's sample game: create, frame, hash
// ge:keywords server sample-game walkers
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/sample.hpp"

#include <cstdio>
#include <vector>

#include "core/prng.hpp"
#include "net/packet.hpp"

namespace ge::server {

namespace {

constexpr int32_t kRing = 1000;

struct Sample {
  core::Rng rng{1};
  std::vector<int32_t> at;
  int64_t frames = 0;
};

void* create(const ServerConfig& c, std::string& error) {
  if (c.map != "ring") {
    error = "the sample game has one map, 'ring', not '" + c.map + "'; next: --map=ring";
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
  for (int32_t& a : s->at) a = (a + core::int_in(s->rng, -1, 1) + kRing) % kRing;
  s->frames++;
}

int seated(const void* g) { return static_cast<int>(static_cast<const Sample*>(g)->at.size()); }

uint64_t hash(const void* g) {
  const auto* s = static_cast<const Sample*>(g);
  uint64_t h = 1469598103934665603ull ^ static_cast<uint64_t>(s->frames);
  for (int32_t a : s->at) h = (h ^ static_cast<uint32_t>(a)) * 1099511628211ull;
  return h;
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
  return g;
}

}  // namespace

const ServerGame& sample_game() {
  static const ServerGame g = make();
  return g;
}

}  // namespace ge::server
