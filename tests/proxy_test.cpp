// Proxy (engine/proxy/proxy.hpp): packing round-trips and refuses to grow; one link end to the
// other drops repeats and takes a packed frame only against a datagram it still holds; and a
// game's client and server through a near and a far proxy on the simulated network: with 10% loss
// between the proxies, three copies each way cut the loss the game sees more than five times, and
// packing at least halves the link's bytes for a game whose datagrams change little; a server off
// the far proxy's allow list is refused with the reason; each proxy records what it passes as a
// replay file that reads back to exactly what the game's ends got.
//
// ge:summary Tests the proxy: packing, repeats, the near and far pair under loss, the allow list
// ge:keywords test proxy pack repeat loss link near far allow-list session
// ge:skill net testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "net/sim_network.hpp"
#include "proxy/proxy.hpp"
#include "proxy/record.hpp"
#include "proxy/trial.hpp"

namespace px = ge::proxy;
namespace net = ge::net;

namespace {

// A snapshot-like datagram: `n` bytes, most of them the same from one tick to the next.
std::vector<uint8_t> snapshot(int tick, size_t n) {
  std::vector<uint8_t> d(n);
  for (size_t i = 0; i < n; i++) d[i] = static_cast<uint8_t>(i * 7 + 3);
  for (size_t i = 0; i < n; i += 12) d[i] = static_cast<uint8_t>(tick + static_cast<int>(i));
  d[0] = static_cast<uint8_t>(tick);
  d[1] = static_cast<uint8_t>(tick >> 8);
  return d;
}

}  // namespace

TEST(packing_round_trips_and_never_grows) {
  ge::core::Rng rng = ge::core::make_rng(3);
  for (int k = 0; k < 200; k++) {
    std::vector<uint8_t> ref = snapshot(k, 300 + static_cast<size_t>(k));
    std::vector<uint8_t> d = snapshot(k + 1, 290 + static_cast<size_t>(k) * 2);
    if (k % 3 == 0)
      for (uint8_t& b : d) b = static_cast<uint8_t>(ge::core::int_in(rng, 0, 255));  // noise
    uint8_t out[2000];
    const size_t n = px::pack_datagram(d, ref, out, sizeof out);
    if (k % 3 == 0) {
      CHECK(n == 0);  // noise does not get smaller: sent plain
      continue;
    }
    CHECK(n > 0 && n < d.size() / 2);
    std::vector<uint8_t> back;
    CHECK(px::unpack_datagram({out, n}, ref, back));
    CHECK(back == d);
    CHECK(!px::unpack_datagram({out, n - 1}, ref, back));  // cut short: refused
  }
}

TEST(one_end_drops_repeats_and_packs_only_against_what_the_other_holds) {
  px::LinkEnd a, b;
  a.config.repeat = 3;
  a.config.gap_us = 1000;
  a.config.pack = true;
  std::vector<std::vector<uint8_t>> wire;
  const px::FrameOut out = [&](std::span<const uint8_t> f) {
    wire.emplace_back(f.begin(), f.end());
  };
  std::vector<uint8_t> d;
  int got = 0;
  for (int64_t k = 0; k < 20; k++) {
    const auto s = snapshot(static_cast<int>(k), 200);
    px::link_send(a, s, k * 10000, out);
    px::link_update(a, k * 10000 + 5000, out);
    for (const auto& f : wire)
      if (px::link_receive(b, f, d) == 1) {
        got++;
        CHECK(d == s);
      }
    wire.clear();
    // b's ack back to a, so a may pack against what b has.
    b.last_out_us = 0;
    px::link_update(b, k * 10000 + 100000, [&](std::span<const uint8_t> f) {
      std::vector<uint8_t> none;
      CHECK(px::link_receive(a, f, none) == 0);
    });
  }
  CHECK(got == 20);
  CHECK(b.stats.duplicates == 40);
  CHECK(a.stats.packed >= 18);
  CHECK(a.stats.bytes_out * 2 < a.stats.payload_out * 3);  // three copies cost under one and a half
}

TEST(repeats_cut_the_loss_a_game_sees_and_packing_halves_the_link) {
  const auto trial = [](double loss, int repeat, bool pack) {
    px::TrialConfig c;
    c.loss = loss;
    c.repeat = repeat;
    c.pack = pack;
    return px::run_proxy_trial(c);
  };
  const px::TrialResult plain = trial(0.10, 1, false), repeated = trial(0.10, 3, false);
  const px::TrialResult packed = trial(0, 1, true), unpacked = trial(0, 1, false);
  const auto lost = [](const px::TrialResult& r) { return px::trial_lost(r); };
  CHECK(lost(plain) > 0.05);
  CHECK(lost(repeated) * 5 < lost(plain));
  CHECK(lost(unpacked) < 0.01);
  CHECK(packed.got_down == packed.sent_down);
  CHECK(packed.link_down * 2 <= unpacked.link_down);
}

TEST(a_server_off_the_far_proxys_allow_list_is_refused_with_the_reason) {
  px::TrialConfig c;
  c.seconds = 2;
  c.allowed = false;
  const px::TrialResult r = px::run_proxy_trial(c);
  CHECK(r.got_up == 0);
  CHECK(r.refused == "proxy does not reach sim:2:27500");
}

TEST(each_proxy_records_what_it_passes_as_a_replay_file) {
  px::TrialConfig c;
  c.seconds = 4;
  c.loss = 0.10;
  c.repeat = 2;
  c.record = true;
  const px::TrialResult r = px::run_proxy_trial(c);
  std::string error;
  std::vector<px::Recorded> near, far;
  CHECK_MSG(px::proxy_read_recording(r.near_file, near, error), error);
  CHECK_MSG(px::proxy_read_recording(r.far_file, far, error), error);
  const auto pick = [](const std::vector<px::Recorded>& in, bool to_server) {
    std::vector<std::vector<uint8_t>> out;
    int64_t last = 0;
    bool ordered = true;
    for (const px::Recorded& d : in) {
      ordered = ordered && d.time_ms >= last;
      last = d.time_ms;
      if (d.to_server == to_server) out.push_back(d.bytes);
    }
    CHECK(ordered);
    return out;
  };
  // The near proxy gave the client exactly what it recorded; the far one gave the server so.
  CHECK(!r.client_got.empty() && !r.server_got.empty());
  CHECK(pick(near, false) == r.client_got);
  CHECK(pick(far, true) == r.server_got);
  // The far proxy took every snapshot from the server; the near one every command once open.
  CHECK(static_cast<int>(pick(far, false).size()) == r.server_sent);
  CHECK(pick(near, true).size() >= r.server_got.size());
  // Packed: well under the game's bytes for datagrams that change little.
  size_t payload = 0;
  for (const auto& d : near) payload += d.bytes.size();
  CHECK(r.near_file.size() * 3 < payload);
  // A replay file that is not a proxy's is refused with the reason.
  std::vector<uint8_t> other;
  ge::replay::StreamWriter w;
  ge::replay::stream_begin(w, {}, other);
  ge::replay::stream_finish(w, {});
  CHECK(!px::proxy_read_recording(other, near, error));
  CHECK(error.find("not a proxy's recording") != std::string::npos);
}
