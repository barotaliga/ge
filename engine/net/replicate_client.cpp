// Replication's client half: snapshots decoded against their baseline, the prediction replayed
// and smoothed, the others interpolated (net/replicate.hpp).
//
// ge:summary Replication's client: deltas decoded, prediction reconciled, interpolation, acks
// ge:keywords net replicate client decode delta predict reconcile replay smoothing interpolate
// ge:skill net
// ge:test tests/net_replicate_test.cpp
#include <algorithm>
#include <cstring>

#include "net/replicate.hpp"
#include "net/replicate_wire.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

using namespace replicate_wire;

constexpr uint32_t kInputRing = 256;   // inputs kept for replaying: seconds at any tick rate
constexpr uint32_t kStarveHold = 300;  // ticks a short window keeps the reserve raised

const Snap* find_snap(const ReplicateClient& c, uint32_t tick) {
  const Snap& s = c.ring[tick % c.ring.size()];
  return s.valid && s.tick == tick ? &s : nullptr;
}

const Row* find_row(const Snap& s, uint16_t id) {
  const auto it = std::lower_bound(s.rows.begin(), s.rows.end(), id,
                                   [](const Row& r, uint16_t want) { return r.id < want; });
  return it != s.rows.end() && it->id == id ? &*it : nullptr;
}

double wrap_diff(const FieldSpec& f, double d) {
  if (!f.angle) return d;
  const double range = f.max - f.min;
  while (d > range / 2) d -= range;
  while (d < -range / 2) d += range;
  return d;
}

void step_own(ReplicateClient& c, double* v, const uint8_t* input) {
  c.schema.step(c.schema.ctx, v, input);
  for (int f = 0; f < c.schema.fields; f++)
    v[f] = dequantize(c.schema.spec[f], quantize(c.schema.spec[f], v[f]));
}

void reconcile(ReplicateClient& c, const Snap& snap) {
  const Row* own = c.own == kNoEntity ? nullptr : find_row(snap, c.own);
  if (!own || !c.schema.step) return;
  const int nf = c.schema.fields;
  double v[kMaxFields];
  for (int f = 0; f < nf; f++) v[f] = dequantize(c.schema.spec[f], own->q[f]);
  const auto n = static_cast<size_t>(c.schema.input_size);
  // Every input the server has not applied yet, replayed over its word.
  for (uint32_t s = c.applied + 1; s <= c.seq && c.seq - s < kInputRing; s++)
    step_own(c, v, &c.history[(s % kInputRing) * n]);
  if (c.predicting) {
    double worst = 0;
    for (int f = 0; f < nf; f++) {
      const double d = wrap_diff(c.schema.spec[f], c.predicted[f] - v[f]);
      c.error[f] += d;
      const double a = d < 0 ? -d : d;
      if (a > worst) worst = a;
    }
    if (worst > 0) {
      c.stats.corrections++;
      c.last_error = worst;
    }
  }
  std::memcpy(c.predicted, v, sizeof(double) * static_cast<size_t>(nf));
  c.predicting = true;
}

// TIME SYNC on a snapshot's arrival: the clock's speed from the slack the server saw, and the
// reckoned server tick, the arrivals' spacing and their jitter from when it came.
void sync_clock(ReplicateClient& c, uint32_t tick, int slack) {
  const ReplicateConfig& k = c.config;
  c.slack = slack;
  // Running short starves the server (a step repeated, a correction), so the clock speeds up
  // twice as hard as it slows down.
  // A window that ran short raises the reserve by one for the next few seconds (`starve_hold`).
  if (slack < 0) c.starved_at = c.seq + 1;
  const bool recent = c.starved_at && c.seq + 1 - c.starved_at < kStarveHold;
  const int off = slack - (k.input_slack + (recent ? 1 : 0));
  const double ask = -k.dilate_gain * off * (off < 0 ? 2 : 1);
  c.dilation = 1 + std::clamp(ask, -k.dilate_max, k.dilate_max);
  const auto t = static_cast<double>(tick);
  if (!c.clocked) {
    c.clocked = true;
    c.server_tick = t;
    c.spacing = static_cast<double>(std::max(1, k.send_every));
    c.jitter = 0;
    c.delay = c.want = c.spacing + 1;
    c.render_x256 = 0;
    c.spacing_from = tick;
    return;
  }
  const double late = t - c.server_tick;  // + early, - late against the reckoning
  c.jitter += 0.05 * ((late < 0 ? -late : late) - c.jitter);
  // What this arrival needed: how far the reckoning had run past the newest snapshot before it.
  // A delay below it would have drawn past every snapshot on hand, the others standing still.
  if (!c.needs.empty()) {
    c.needs[c.need_count % c.needs.size()] = c.server_tick - static_cast<double>(c.spacing_from);
    c.need_count++;
    const size_t n = std::min<size_t>(c.need_count, c.needs.size());
    c.sorted.assign(c.needs.begin(), c.needs.begin() + static_cast<std::ptrdiff_t>(n));
    // The quantile rounded down to a rank: 1 is the window's most, 0 its least.
    const auto at = static_cast<size_t>(k.interp_quantile * static_cast<double>(n - 1));
    std::nth_element(c.sorted.begin(), c.sorted.begin() + static_cast<std::ptrdiff_t>(at),
                     c.sorted.end());
    c.want = std::max(c.sorted[at], 1.0) + 0.25;
  }
  c.server_tick += 0.1 * late;
  if (c.spacing_from) c.spacing += 0.1 * (static_cast<double>(tick - c.spacing_from) - c.spacing);
  c.spacing_from = tick;
}

void on_snapshot(ReplicateClient& c, const uint8_t* data, size_t size) {
  Reader r = make_reader(data, size);
  if (read_u8(r) != kSnapshot) return;
  const uint32_t tick = read_u32(r), baseline = read_u32(r), applied = read_u32(r);
  const uint16_t own = read_u16(r);
  const auto slack = static_cast<int8_t>(read_u8(r));
  const uint64_t entries = read_varint(r);
  if (r.bad || size < kSnapshotHeader + 3) return;
  if (c.has_snapshot && tick <= c.newest) {
    c.stats.dropped++;  // older than one already decoded
    return;
  }
  const Snap* base = baseline ? find_snap(c, baseline) : nullptr;
  if (baseline && !base) {
    c.stats.dropped++;  // its baseline is gone: the next one, against a newer ack, will do
    return;
  }
  const int nf = c.schema.fields;
  std::vector<Row>& rows = c.scratch;
  rows.clear();
  if (base) rows.insert(rows.end(), base->rows.begin(), base->rows.end());
  BitReader br{data + kSnapshotHeader + 3, size - kSnapshotHeader - 3};
  bool gone = false;
  for (uint64_t e = 0; e < entries && !br.bad; e++) {
    const auto id = static_cast<uint16_t>(get_bits(br, 16));
    const uint32_t op = get_bits(br, 2);
    auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& x) { return x.id == id; });
    if (op == kGone) {
      if (it != rows.end()) {
        it->id = kNoEntity;
        gone = true;
      }
    } else if (op == kUpdate) {
      const uint32_t mask = get_bits(br, nf);
      if (it == rows.end()) {
        br.bad = true;  // an update to a row the baseline lacks
        break;
      }
      for (int f = 0; f < nf; f++) {
        if (mask & (1u << f)) it->q[f] = get_bits(br, c.schema.spec[f].bits);
      }
    } else if (op == kNew) {
      Row row;
      row.id = id;
      for (int f = 0; f < nf; f++) row.q[f] = get_bits(br, c.schema.spec[f].bits);
      if (it != rows.end()) {
        *it = row;
      } else {
        rows.push_back(row);
      }
    } else {
      br.bad = true;
    }
  }
  if (br.bad) {
    c.stats.dropped++;
    return;
  }
  if (gone)
    rows.erase(
        std::remove_if(rows.begin(), rows.end(), [](const Row& x) { return x.id == kNoEntity; }),
        rows.end());
  std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.id < b.id; });
  Snap& slot = c.ring[tick % c.ring.size()];
  slot.rows.swap(rows);
  slot.tick = tick;
  slot.valid = true;
  c.newest = tick;
  c.has_snapshot = true;
  c.local_tick = 0;
  c.applied = applied;
  if (own != c.own) c.predicting = false;  // a new entity: nothing to smooth from
  c.own = own;
  c.stats.snapshots++;
  c.stats.bytes += size;
  if (!baseline) c.stats.full++;
  if (c.config.sync) sync_clock(c, tick, slack);
  reconcile(c, slot);
}

}  // namespace

void replicate_client_init(ReplicateClient& c, const Schema& schema, const ReplicateConfig& config,
                           Node& node, int server_peer) {
  c = ReplicateClient{};
  c.schema = schema;
  c.config = config;
  if (c.config.ring < 2) c.config.ring = 2;
  if (c.config.tick_hz < 1) c.config.tick_hz = 1;
  c.node = &node;
  c.server = server_peer;
  c.ring.resize(static_cast<size_t>(c.config.ring));
  for (Snap& s : c.ring) s.rows.reserve(static_cast<size_t>(config.max_entities));
  c.scratch.reserve(static_cast<size_t>(config.max_entities));
  c.history.assign(kInputRing * static_cast<size_t>(schema.input_size), 0);
  c.needs.assign(static_cast<size_t>(std::max(1, c.config.interp_window)), 0.0);
  c.sorted.reserve(c.needs.size());
}

void replicate_client_receive(ReplicateClient& c) {
  for (const Event& e : c.node->events) {
    if (e.type == EventType::message && e.peer == c.server &&
        e.channel == c.config.snapshot_channel)
      on_snapshot(c, event_data(*c.node, e), e.size);
  }
}

void replicate_client_input(ReplicateClient& c, const uint8_t* input) {
  // Nothing is sent before the first snapshot: an input the connection could not carry yet would
  // be one the prediction replays and the server never applied.
  if (!c.has_snapshot) return;
  const auto n = static_cast<size_t>(c.schema.input_size);
  c.seq++;
  std::memcpy(&c.history[(c.seq % kInputRing) * n], input, n);
  c.local_tick++;
  if (c.config.sync && c.clocked) {
    // A tick of the server's per tick of the server's: this one runs `dilation` times as fast.
    c.server_tick += 1 / c.dilation;
    // The delay grows quickly (the others slowed a few percent, so they do not stand still) and
    // shrinks slowly (sped a little), so a burst's need outlasts the burst.
    c.delay += std::clamp(c.want - c.delay, -c.config.interp_ease / 4, c.config.interp_ease);
    const double at = c.server_tick - c.delay;
    const uint32_t x256 = at <= 0 ? 0 : static_cast<uint32_t>(at * 256);
    c.render_x256 = std::max(c.render_x256, x256);
  }
  if (c.predicting) step_own(c, c.predicted, input);
  for (int f = 0; f < c.schema.fields; f++) c.error[f] *= c.config.smoothing;
  // cmd_rate: a message every tick_hz / cmd_rate ticks, carrying every input since the last one
  // and `redundancy` before them.
  c.unsent++;
  const int every =
      c.config.cmd_rate > 0
          ? std::max(1, (c.config.tick_hz + c.config.cmd_rate - 1) / c.config.cmd_rate)
          : 1;
  if (c.unsent < every) return;
  const auto room = static_cast<uint32_t>(
      std::min<size_t>(255, (kMaxMessage - kInputHeader) / std::max<size_t>(1, n)));
  const auto want = static_cast<uint32_t>(std::max(1, c.config.redundancy) + c.unsent - 1);
  const uint32_t count = std::min({c.seq, want, room, kInputRing});
  c.unsent = 0;
  uint8_t out[kMaxMessage];
  Writer w = make_writer(out, sizeof out);
  write_u8(w, kInput);
  write_u32(w, c.has_snapshot ? c.newest : 0);
  write_u32(w, replicate_client_render_tick(c));
  write_u32(w, c.config.rate);
  write_u16(w, static_cast<uint16_t>(std::clamp(c.config.update_rate, 0, 65535)));
  write_u32(w, c.seq);
  write_u8(w, static_cast<uint8_t>(count));
  for (uint32_t s = c.seq - count + 1; s <= c.seq; s++)
    write_bytes(w, &c.history[(s % kInputRing) * n], n);
  if (!w.overflow) node_send(*c.node, c.server, c.config.input_channel, out, w.size);
}

void replicate_client_predicted(const ReplicateClient& c, double* out) {
  for (int f = 0; f < c.schema.fields; f++) out[f] = c.predicted[f] + c.error[f];
}

uint32_t replicate_client_render_tick(const ReplicateClient& c) {
  if (!c.has_snapshot) return 0;
  if (c.config.sync) return c.render_x256;
  const int64_t t = int64_t{c.newest} + c.local_tick - c.config.interp_ticks;
  return t <= 0 ? 0 : static_cast<uint32_t>(t) << 8;
}

bool replicate_client_sample(const ReplicateClient& c, uint16_t id, double* out) {
  const uint32_t t = replicate_client_render_tick(c);
  const Snap* a = nullptr;
  const Snap* b = nullptr;
  const Row* ra = nullptr;
  const Row* rb = nullptr;
  for (const Snap& s : c.ring) {
    if (!s.valid) continue;
    const Row* r = find_row(s, id);
    if (!r) continue;
    const uint32_t at = s.tick << 8;
    if (at <= t && (!a || s.tick > a->tick)) {
      a = &s;
      ra = r;
    } else if (at > t && (!b || s.tick < b->tick)) {
      b = &s;
      rb = r;
    }
  }
  if (!a && !b) return false;
  if (!a || !b) {
    const Row* r = a ? ra : rb;  // only one side: held where it is
    for (int f = 0; f < c.schema.fields; f++) out[f] = dequantize(c.schema.spec[f], r->q[f]);
    return true;
  }
  const double frac =
      static_cast<double>(t - (a->tick << 8)) / static_cast<double>((b->tick - a->tick) << 8);
  for (int f = 0; f < c.schema.fields; f++) {
    const FieldSpec& spec = c.schema.spec[f];
    const double va = dequantize(spec, ra->q[f]);
    const double vb = va + wrap_diff(spec, dequantize(spec, rb->q[f]) - va);
    out[f] = va + (vb - va) * frac;
  }
  return true;
}

}  // namespace ge::net
