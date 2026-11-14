// Proxy (engine/proxy/proxy.hpp): packing round-trips and refuses to grow; one link end to the
// other drops repeats and takes a packed frame only against a datagram it still holds; and a
// game's client and server through a near and a far proxy on the simulated network: with 10% loss
// between the proxies, three copies each way cut the loss the game sees more than five times, and
// packing at least halves the link's bytes for a game whose datagrams change little; a server off
// the far proxy's allow list is refused with the reason; each proxy records what it passes as a
// replay file that reads back to exactly what the game's ends got, and a TV node fed the file as
// it grows serves an observer who joins mid-match the whole match from its start.
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
#include "proxy/link_code.hpp"
#include "proxy/proxy.hpp"
#include "proxy/record.hpp"
#include "proxy/trial.hpp"
#include "tv/node.hpp"

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

// The coder (link_code.hpp): a datagram back to the byte from its coding and the same reference,
// both ends' models left the same, never bigger than the datagram, a cut or damaged coding never
// read past its bounds; and a snapshot like the last costs a small share of its bytes.
TEST(coding_round_trips_leaves_both_models_alike_and_never_grows) {
  ge::core::Rng rng = ge::core::make_rng(4);
  px::LinkModels sent, got;
  size_t coded = 0, plain = 0;
  for (int k = 0; k < 200; k++) {
    std::vector<uint8_t> ref = snapshot(k, 300 + static_cast<size_t>(k));
    std::vector<uint8_t> d = snapshot(k + 1, 300 + static_cast<size_t>(k + k % 4));
    if (k % 5 == 0)
      for (uint8_t& b : d) b = static_cast<uint8_t>(ge::core::int_in(rng, 0, 255));  // noise
    uint8_t out[2000];
    const px::LinkModels before = sent;
    const size_t n = px::code_datagram(d, ref, sent, out, sizeof out);
    if (n == 0) {
      CHECK(k % 5 == 0);  // only noise fails to get smaller, and leaves the models as they were
      CHECK(sent.tree == before.tree);
      continue;
    }
    CHECK(n < d.size());
    std::vector<uint8_t> back;
    CHECK(px::decode_datagram({out, n}, ref, got, back, px::kMostDatagram));
    CHECK(back == d);
    CHECK(got.tree == sent.tree);
    if (k >= 100 && k % 5 != 0) {
      coded += n;
      plain += d.size();
    }
    // Damaged: whatever comes back stays within the bound and leaves the models untouched.
    px::LinkModels scratch = got;
    out[n / 2] ^= 0x5A;
    if (px::decode_datagram({out, n}, ref, scratch, back, px::kMostDatagram))
      CHECK(back.size() <= px::kMostDatagram);
    CHECK(!px::decode_datagram({out, n}, ref, scratch, back, 3) || back.size() <= 3);
  }
  CHECK(plain > 0 && coded * 8 < plain);  // learned: an eighth of the bytes and less
}

// A coding link under loss: every datagram that arrives is the one sent, frames whose reference
// was lost are dropped and counted, and the two ends' models never part (no frame is broken).
TEST(a_coding_link_under_loss_keeps_both_ends_models_in_step) {
  px::LinkEnd a, b;
  a.config.code = true;
  a.config.pack = true;
  ge::core::Rng rng = ge::core::make_rng(9);
  std::vector<std::vector<uint8_t>> sent;
  std::vector<std::vector<uint8_t>> wire;
  const px::FrameOut out = [&](std::span<const uint8_t> f) {
    wire.emplace_back(f.begin(), f.end());
  };
  std::vector<uint8_t> d;
  int got = 0;
  for (int64_t k = 0; k < 400; k++) {
    const auto s = snapshot(static_cast<int>(k), 120 + static_cast<size_t>(k % 7) * 9);
    px::link_send(a, s, k * 10000, out);
    for (const auto& f : wire) {
      if (ge::core::int_in(rng, 0, 99) < 15) continue;  // 15% lost
      if (px::link_receive(b, f, d) == 1) {
        got++;
        CHECK(d == s);
      }
    }
    wire.clear();
    if (k % 3 == 0) {  // b's ack back, now and then, itself lost at times
      b.last_out_us = 0;
      px::link_update(b, k * 10000 + 100000, [&](std::span<const uint8_t> f) {
        std::vector<uint8_t> none;
        if (ge::core::int_in(rng, 0, 99) >= 15) CHECK(px::link_receive(a, f, none) == 0);
      });
    }
  }
  CHECK(b.stats.broken == 0);
  CHECK(a.stats.coded > 250);
  CHECK(got + static_cast<int>(b.stats.no_ref) > 300);
  CHECK(a.stats.bytes_out * 4 < a.stats.payload_out);  // a quarter of the game's bytes and less
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

// A proxy whose loop runs no faster than the game sends: the copies of one datagram fall due
// while the next one arrives, and they go first, so a lost first copy is made good before the
// newer datagram and the far end passes the game's datagrams on in the order they were sent.
TEST(due_copies_go_before_a_newer_datagram) {
  px::LinkEnd a, b;
  a.config.repeat = 2;
  a.config.gap_us = 4000;
  std::vector<std::vector<uint8_t>> wire;
  const px::FrameOut out = [&](std::span<const uint8_t> f) {
    wire.emplace_back(f.begin(), f.end());
  };
  std::vector<uint8_t> d;
  std::vector<int> order;
  for (int64_t k = 0; k < 10; k++) {
    // The proxy's frame: what came from the game, then what is due. Every other datagram's first
    // frame is lost on the way, so only its copy arrives.
    px::link_send(a, snapshot(static_cast<int>(k), 100), k * 13000, out);
    if (k % 2 == 0) wire.pop_back();
    px::link_update(a, k * 13000, out);
    for (const auto& f : wire)
      if (px::link_receive(b, f, d) == 1) order.push_back(d[0]);
    wire.clear();
  }
  px::link_update(a, 200000, out);
  for (const auto& f : wire)
    if (px::link_receive(b, f, d) == 1) order.push_back(d[0]);
  CHECK(order.size() == 10);
  CHECK(b.stats.duplicates == 5);
  for (size_t i = 0; i < order.size(); i++) CHECK(order[i] == static_cast<int>(i));
}

TEST(repeats_cut_the_loss_a_game_sees_and_packing_halves_the_link) {
  const auto trial = [](double loss, int repeat, bool pack, bool code = false) {
    px::TrialConfig c;
    c.loss = loss;
    c.repeat = repeat;
    c.pack = pack;
    c.code = code;
    return px::run_proxy_trial(c);
  };
  const px::TrialResult plain = trial(0.10, 1, false), repeated = trial(0.10, 3, false);
  const px::TrialResult packed = trial(0, 1, true), unpacked = trial(0, 1, false);
  const px::TrialResult coded = trial(0, 1, true, true), coded_lossy = trial(0.10, 3, true, true);
  const auto lost = [](const px::TrialResult& r) { return px::trial_lost(r); };
  CHECK(lost(plain) > 0.05);
  CHECK(lost(repeated) * 5 < lost(plain));
  CHECK(lost(unpacked) < 0.01);
  CHECK(packed.got_down == packed.sent_down);
  CHECK(packed.link_down * 2 <= unpacked.link_down);
  // Coding halves the packed link again, and loses nothing more than plain copies do.
  CHECK(coded.got_down == coded.sent_down);
  CHECK(coded.link_down * 2 <= packed.link_down);
  CHECK(lost(coded_lossy) * 5 < lost(plain));
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

TEST(an_observer_watches_the_recording_through_a_tv_node_as_it_grows) {
  px::ProxyRecording rec;
  rec.block_us = 500000;
  proxy_record_begin(rec, "trial", {{"side", "near"}}, 0);
  ge::tv::Node node;
  ge::tv::Watch early, late;
  const ge::tv::Send send = [&](int link, std::span<const uint8_t> m) {
    return ge::tv::watch_receive(link == 1 ? early : late, m);
  };
  ge::tv::tv_join(node, 1);
  CHECK(ge::tv::tv_receive(node, 1, ge::tv::tv_hello("early", false)));
  for (int64_t t = 0; t <= 6000000; t += 10000) {
    if (t % 20000 == 0) px::proxy_record(rec, true, snapshot(static_cast<int>(t / 20000), 24), t);
    if (t % 50000 == 0) px::proxy_record(rec, false, snapshot(static_cast<int>(t / 50000), 400), t);
    if (t == 3000000) {  // half way through
      ge::tv::tv_join(node, 2);
      CHECK(ge::tv::tv_receive(node, 2, ge::tv::tv_hello("late", false)));
    }
    if (t == 6000000) px::proxy_record_finish(rec, t);
    CHECK(ge::tv::tv_feed(node, px::proxy_record_fresh(rec), t / 1000));
    ge::tv::tv_update(node, t / 1000, send);
  }
  CHECK_MSG(node.finished, node.error);
  ge::tv::tv_update(node, 7000, send);
  CHECK(early.ended && late.ended);
  // Both observers hold the whole match: the late one was sent it from the start.
  std::vector<px::Recorded> got, want;
  std::string error;
  CHECK_MSG(px::proxy_read_recording(rec.bytes, want, error), error);
  CHECK_MSG(px::proxy_read_recording(late.stream, got, error), error);
  CHECK(want.size() == 300 + 120 + 2);
  CHECK(got.size() == want.size());
  bool same = got.size() == want.size();
  for (size_t i = 0; same && i < got.size(); i++)
    same = got[i].bytes == want[i].bytes && got[i].to_server == want[i].to_server;
  CHECK(same);
  CHECK_MSG(px::proxy_read_recording(early.stream, got, error), error);
  CHECK(got.size() == want.size());
}
