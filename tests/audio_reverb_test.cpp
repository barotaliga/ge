// engine/audio/reverb.hpp: the tail falls 60 dB in its decay time at any size (Schroeder's
// backward integral, the decay read off its -5 to -25 dB stretch); `wet` 1 is a tail with the
// send's energy; an early tap answers after its delay on its side; nothing of the tail comes
// before its predelay; a room gone dry stops running; the same calls give the same bits; and a
// synth's room is heard through its send.
//
// ge:summary Tests for the room's reverb: decay time, level, taps, predelay, rest, the synth's room
// ge:keywords test audio reverb rt60 schroeder fdn early-tap predelay energy determinism
// ge:skill audio testing
#include <cmath>
#include <string>
#include <vector>

#include "audio/reverb.hpp"
#include "audio/synth.hpp"
#include "check.hpp"

namespace {

using namespace ge::audio;

struct Ir {
  std::vector<float> l, r;
};

// The response to a unit pulse into the send, `seconds` long.
Ir pulse(Reverb& rv, double seconds) {
  const int n = static_cast<int>(seconds * kRate);
  Ir ir{std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n))};
  std::vector<float> in(kBlock, 0);
  for (int at = 0; at < n; at += kBlock) {
    const int k = n - at < kBlock ? n - at : kBlock;
    in[0] = at == 0 ? 1.0f : 0.0f;
    rv.process(in.data(), ir.l.data() + at, ir.r.data() + at, k);
  }
  return ir;
}

double energy(const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX) {
  double e = 0;
  for (size_t i = from; i < v.size() && i < to; ++i) e += static_cast<double>(v[i]) * v[i];
  return e;
}

// The decay time off the backward integral: the time from -5 dB to -25 dB, times three.
double rt60_of(const std::vector<float>& v) {
  std::vector<double> back(v.size() + 1, 0);
  for (size_t i = v.size(); i-- > 0;) back[i] = back[i + 1] + static_cast<double>(v[i]) * v[i];
  const double total = back[0];
  double t5 = -1, t25 = -1;
  for (size_t i = 0; i < v.size(); ++i) {
    const double db = 10 * std::log10(back[i] / total);
    if (t5 < 0 && db <= -5) t5 = static_cast<double>(i) / kRate;
    if (t25 < 0 && db <= -25) {
      t25 = static_cast<double>(i) / kRate;
      break;
    }
  }
  return 3 * (t25 - t5);
}

RoomSound tail_only(float decay, float size) {
  RoomSound r;
  r.wet = 1;
  r.decay_s = decay;
  r.size_m = size;
  r.damp_hz = 20000;
  r.predelay_ms = 0;
  r.echo = 1;
  return r;
}

}  // namespace

TEST(the_tail_falls_sixty_db_in_its_decay_time) {
  for (const float decay : {0.4f, 1.5f, 4.0f}) {
    for (const float size : {3.0f, 12.0f, 40.0f}) {
      Reverb rv;
      rv.set(tail_only(decay, size), 0);
      const Ir ir = pulse(rv, decay * 1.2 + 0.3);
      const double got = rt60_of(ir.l);
      CHECK_MSG(std::fabs(got / decay - 1) < 0.15, "decay " + std::to_string(decay) + " size " +
                                                       std::to_string(size) + ": " +
                                                       std::to_string(got));
    }
  }
}

TEST(wet_one_is_a_tail_with_the_sends_energy) {
  for (const float decay : {0.5f, 2.0f}) {
    for (const float size : {4.0f, 20.0f}) {
      Reverb rv;
      rv.set(tail_only(decay, size), 0);
      const Ir ir = pulse(rv, decay * 2.5 + 0.3);
      const double db = 10 * std::log10(energy(ir.l));
      CHECK_MSG(std::fabs(db) < 3, "decay " + std::to_string(decay) + " size " +
                                       std::to_string(size) + ": " + std::to_string(db) + " dB");
      CHECK(std::fabs(10 * std::log10(energy(ir.r)) - db) < 2);  // both sides alike
    }
  }
}

TEST(an_early_tap_answers_after_its_delay_on_its_side) {
  Reverb rv;
  RoomSound r;
  r.taps = 1;
  r.tap[0] = {.delay_ms = 20, .gain = 0.5f, .pan = 1, .damp_hz = 20000};
  rv.set(r, 0);
  const Ir ir = pulse(rv, 0.1);
  const size_t at = static_cast<size_t>(0.020 * kRate);
  CHECK(energy(ir.l, 0, at - 2) == 0 && energy(ir.r, 0, at - 2) == 0);
  CHECK(energy(ir.l) == 0);  // hard right
  CHECK(energy(ir.r, at - 2, at + 64) > 0.1);
}

TEST(nothing_of_the_tail_comes_before_its_predelay) {
  Reverb rv;
  RoomSound r = tail_only(1, 8);
  r.predelay_ms = 30;
  rv.set(r, 0);
  const Ir ir = pulse(rv, 0.4);
  // The first answer: the predelay plus the shortest line's length.
  const size_t first = static_cast<size_t>((0.030 + 8 * 0.613 / 343.0) * kRate) - 2;
  CHECK(energy(ir.l, 0, first) == 0);
  CHECK(energy(ir.l, first) > 0);
}

TEST(a_room_gone_dry_stops_running_and_replays_its_bits) {
  Reverb rv;
  CHECK(!rv.active());
  rv.set(tail_only(1, 8), 0);
  CHECK(rv.active());
  const Ir a = pulse(rv, 0.5);
  rv.set(RoomSound{}, 0.05f);
  pulse(rv, 2.0);
  CHECK(!rv.active());  // silent and cleared
  rv.set(tail_only(1, 8), 0);
  const Ir b = pulse(rv, 0.5);
  CHECK(a.l == b.l && a.r == b.r);  // from cleared lines, the same bits again
}

TEST(a_synths_room_is_heard_through_its_send) {
  auto play = [](bool room, float echo) {
    Synth s(1, EchoRoom{.delay_s = 0.25f, .feedback = 0.3f, .damp_hz = 3000});
    if (room) {
      RoomSound r = tail_only(1.2f, 10);
      r.echo = echo;
      s.room(r, 0);
    }
    s.noise({.duration_ms = 10, .volume = 0.2f, .echo = 1});
    std::vector<float> out(2 * static_cast<size_t>(0.6 * kRate));
    s.render(out.data(), static_cast<int>(out.size() / 2));
    return out;
  };
  const std::vector<float> bare = play(false, 1);
  const std::vector<float> dry_room = play(true, 1);
  const std::vector<float> closed = play(true, 0);
  // Between the sound and the echo (50 to 240 ms) only the room's tail is heard.
  const size_t a = 2 * static_cast<size_t>(0.05 * kRate), b = 2 * static_cast<size_t>(0.24 * kRate);
  CHECK(energy(bare, a, b) < 1e-12);
  CHECK(energy(dry_room, a, b) > 1e-6);
  // A closed room (echo 0) silences the outdoor echo: past it, the room's tail is all.
  const size_t c = 2 * static_cast<size_t>(0.25 * kRate), d = 2 * static_cast<size_t>(0.27 * kRate);
  const double with_echo = energy(dry_room, c, d), without = energy(closed, c, d);
  CHECK(with_echo > without * 1.5);
}
