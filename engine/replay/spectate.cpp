// Spectating by inputs (spectate.hpp): the writer's blocks and keyframes, the spectator's follow.
//
// ge:summary Spectating by inputs: the writer's blocks and keyframes, the spectator's follow
// ge:keywords spectate inputs-chunk grid-index keyframe follow digest
// ge:skill replay
// ge:test tests/fall_line_spectate_test.cpp
#include "replay/spectate.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "core/num.hpp"
#include "play/inputs.hpp"
#include "play/saves.hpp"
#include "play/session.hpp"

namespace ge::replay {

namespace {

void put_u(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}
void put_s(std::vector<uint8_t>& o, int64_t v) {
  put_u(o, (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63));
}

struct In {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  uint64_t u() {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      if (at >= b.size()) break;
      const uint8_t c = b[at++];
      v |= static_cast<uint64_t>(c & 0x7F) << shift;
      if ((c & 0x80) == 0) return v;
    }
    bad = true;
    return 0;
  }
  int64_t s() {
    const uint64_t z = u();
    return static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1);
  }
  uint64_t u64() {
    if (b.size() - at < 8 || at > b.size()) {
      bad = true;
      return 0;
    }
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
      v |= static_cast<uint64_t>(b[at + static_cast<size_t>(i)]) << (8 * i);
    at += 8;
    return v;
  }
};

constexpr char kDeltaMagic[8] = {'G', 'E', 'K', 'D', 'E', 'L', 'T', 'A'};

bool is_delta(std::span<const uint8_t> c) {
  return c.size() >= 8 && std::equal(c.begin(), c.begin() + 8, kDeltaMagic);
}

// `save` as a delta against `base` (saved at `base_step`), `save` saved at `step`.
std::vector<uint8_t> make_delta(std::span<const uint8_t> save, int64_t step,
                                std::span<const uint8_t> base, int64_t base_step) {
  std::vector<uint8_t> out(kDeltaMagic, kDeltaMagic + 8);
  put_u(out, static_cast<uint64_t>(step));
  put_u(out, static_cast<uint64_t>(base_step));
  put_u(out, save.size());
  const size_t head = out.size();
  out.resize(head + save.size());
  for (size_t i = 0; i < save.size(); i++)
    out[head + i] = static_cast<uint8_t>(save[i] ^ (i < base.size() ? base[i] : 0));
  return out;
}

// A delta applied to `base` (saved at `base_step`): the save and its step. False with the reason.
bool apply_delta(std::span<const uint8_t> delta, std::vector<uint8_t>& base, int64_t& base_step,
                 std::string& error) {
  In in{delta, 8};
  const auto step = static_cast<int64_t>(in.u());
  const auto against = static_cast<int64_t>(in.u());
  const uint64_t size = in.u();
  if (in.bad || size != delta.size() - in.at) {
    error = "a delta keyframe is damaged";
    return false;
  }
  if (against != base_step) {
    error = "a delta keyframe at step " + std::to_string(step) + " is taken against step " +
            std::to_string(against) + " but the keyframe before it is at step " +
            std::to_string(base_step);
    return false;
  }
  std::vector<uint8_t> save(size);
  for (size_t i = 0; i < save.size(); i++)
    save[i] = static_cast<uint8_t>(delta[in.at + i] ^ (i < base.size() ? base[i] : 0));
  base = std::move(save);
  base_step = step;
  return true;
}

// The first step of a block's inputs chunk (-1: none).
int64_t first_step(const Block& b) {
  for (const Chunk& c : b.chunks) {
    if (c.track != kInputsTrack) continue;
    In in{c.bytes};
    const auto v = static_cast<int64_t>(in.u());
    return in.bad ? -1 : v;
  }
  return -1;
}

double grid_steps(play::AxisKind kind) {
  if (kind == play::AxisKind::signed_unit) return play::kSignedSteps;
  if (kind == play::AxisKind::lever) return play::kLeverSteps;
  return 1;
}

int64_t index_of(double snapped, play::AxisKind kind) {
  return static_cast<int64_t>(core::num::round(snapped * grid_steps(kind)));
}

double value_of(int64_t index, play::AxisKind kind) {
  return index == 0 ? 0.0 : static_cast<double>(index) / grid_steps(kind);
}

int64_t time_ms_of(int64_t step, double hz) {
  return static_cast<int64_t>(static_cast<double>(step) * 1000.0 / hz);
}

}  // namespace

SpectateWriter::SpectateWriter(const play::GameSpec& spec, const play::Setup& setup,
                               std::vector<uint8_t>& out, const SpectateOptions& options)
    : spec_(spec), setup_(setup), options_(options) {
  per_block_ = std::max<int64_t>(1, static_cast<int64_t>(core::num::round(spec.hz)));
  StreamHeader h;
  h.game = spec.name;
  h.game_version = spec.version;
  h.meta.emplace_back("seed", play::hex64(static_cast<uint64_t>(setup.seed)));
  h.meta.emplace_back("hz", std::to_string(per_block_));
  for (const auto& [k, v] : setup.options) h.meta.emplace_back("option." + k, v);
  h.tracks = {{kInputsTrack, TrackKind::inputs, TrackCodec::zstd, "inputs"},
              {kKeyframeTrack, TrackKind::keyframe, TrackCodec::zstd, "save"}};
  stream_begin(writer_, h, out);
  held_.assign(spec.axes.size(), 0);
}

bool SpectateWriter::keyframe(const play::Run& run, std::string& error) {
  std::vector<uint8_t> save;
  if (!play::save_run(spec_, setup_, run, steps_, {}, save, error)) return false;
  const bool whole =
      last_save_.empty() || options_.full_every <= 1 || keys_since_full_ + 1 >= options_.full_every;
  if (whole) {
    stream_add(writer_, kKeyframeTrack, time_ms_of(steps_, spec_.hz), save);
    block_keyframe_ = true;
    keys_since_full_ = 0;
  } else {
    stream_add(writer_, kKeyframeTrack, time_ms_of(steps_, spec_.hz),
               make_delta(save, steps_, last_save_, last_save_step_));
    keys_since_full_++;
  }
  block_save_ = true;
  last_save_ = std::move(save);
  last_save_step_ = steps_;
  return true;
}

void SpectateWriter::close_block(const play::Run& run) {
  if (held_count_ > 0) {
    put_u(runs_, static_cast<uint64_t>(held_count_));
    for (const int64_t i : held_) put_s(runs_, i);
    runs_n_++;
    held_count_ = 0;
  }
  std::vector<uint8_t> chunk;
  put_u(chunk, static_cast<uint64_t>(block_first_));
  put_u(chunk, static_cast<uint64_t>(runs_n_));
  chunk.insert(chunk.end(), runs_.begin(), runs_.end());
  const uint64_t d = run.digest();
  for (int i = 0; i < 8; i++) chunk.push_back(static_cast<uint8_t>(d >> (8 * i)));
  stream_add(writer_, kInputsTrack, time_ms_of(block_first_, spec_.hz), chunk);
  stream_end_block(writer_, block_keyframe_);
  runs_.clear();
  runs_n_ = 0;
  block_keyframe_ = false;
  block_save_ = false;
  block_first_ = steps_;
}

bool SpectateWriter::start(const play::Run& run, std::string& error) {
  return options_.keyframe_seconds <= 0 || keyframe(run, error);
}

bool SpectateWriter::step(std::span<const double> snapped, const play::Run& run,
                          std::string& error) {
  std::vector<int64_t> now(spec_.axes.size(), 0);
  for (size_t i = 0; i < now.size() && i < snapped.size(); i++)
    now[i] = index_of(snapped[i], spec_.axes[i].kind);
  if (held_count_ > 0 && now != held_) {
    put_u(runs_, static_cast<uint64_t>(held_count_));
    for (const int64_t i : held_) put_s(runs_, i);
    runs_n_++;
    held_count_ = 0;
  }
  held_ = now;
  held_count_++;
  steps_++;
  if (steps_ - block_first_ >= per_block_) {
    close_block(run);
    const int64_t block = steps_ / per_block_;
    if (options_.keyframe_seconds > 0 && block % options_.keyframe_seconds == 0)
      return keyframe(run, error);
  }
  return true;
}

void SpectateWriter::finish(const play::Run& run) {
  if (steps_ > block_first_ || block_save_) close_block(run);
  stream_finish(writer_,
                {{"steps", std::to_string(steps_)}, {"digest", play::hex64(run.digest())}});
}

bool spectate_pack(const play::GameSpec& spec, const play::InputLog& log, std::vector<uint8_t>& out,
                   std::string& error, const SpectateOptions& options) {
  if (!play::fits(log, spec, error)) return false;
  std::map<std::string, std::string> given(log.options.begin(), log.options.end());
  play::Setup setup;
  if (!play::make_setup(spec, log.seed, given, setup, error)) return false;
  play::Session session(spec, setup);
  if (!session.ok()) {
    error = "the run does not open: " + session.error();
    return false;
  }
  out.clear();
  SpectateWriter writer(spec, setup, out, options);
  if (!writer.start(session.run(), error)) return false;
  for (const play::InputRun& r : log.runs) {
    for (int64_t k = 0; k < r.count; k++) {
      session.step(r.values);
      if (!writer.step(r.values, session.run(), error)) return false;
    }
  }
  writer.finish(session.run());
  return true;
}

bool spectate_inputs(const play::GameSpec& spec, std::span<const uint8_t> stream,
                     play::InputLog& out, std::string& error) {
  StreamReader r;
  if (!read_stream_header(stream, r, error)) return false;
  const StreamHeader& h = r.header;
  if (h.game != spec.name) {
    error = "the stream is a " + h.game + " run, not " + spec.name;
    return false;
  }
  play::InputLog log;
  log.game = spec.name;
  log.hz = spec.hz;
  for (const play::Axis& a : spec.axes) log.axes.emplace_back(a.name);
  for (const auto& [k, v] : h.meta) {
    if (k == "seed") {
      const auto seed = play::parse_hex64(v);
      if (!seed) {
        error = "the stream's seed '" + v + "' is not sixteen hex digits";
        return false;
      }
      log.seed = static_cast<double>(*seed);
    } else if (k.starts_with("option.")) {
      log.options.emplace_back(k.substr(7), v);
    }
  }
  play::InputRecorder rec(std::move(log));
  int64_t step = 0;
  Block b;
  while (next_block(r, b, error)) {
    for (const Chunk& c : b.chunks) {
      if (c.track != kInputsTrack) continue;
      In in{c.bytes};
      const auto first = static_cast<int64_t>(in.u());
      if (first != step) {
        error = step == 0 ? "the stream starts at step " + std::to_string(first) +
                                ", not its first: a viewer's late stream holds no whole run"
                          : "block " + std::to_string(b.index) + " starts at step " +
                                std::to_string(first) + " after " + std::to_string(step);
        return false;
      }
      const uint64_t runs = in.u();
      std::vector<double> values(spec.axes.size(), 0);
      for (uint64_t k = 0; k < runs && !in.bad; k++) {
        const uint64_t count = in.u();
        for (size_t i = 0; i < values.size(); i++) values[i] = value_of(in.s(), spec.axes[i].kind);
        if (in.bad || count > (uint64_t{1} << 24)) break;
        for (uint64_t n = 0; n < count; n++) rec.record(values);
        step += static_cast<int64_t>(count);
      }
      in.u64();
      if (in.bad) {
        error = "block " + std::to_string(b.index) + "'s inputs are damaged";
        return false;
      }
    }
  }
  if (!error.empty()) return false;
  out = rec.log();
  if (const auto t = read_trailer(stream)) {
    for (const auto& [k, v] : t->meta)
      if (k == "digest") out.digest = play::parse_hex64(v);
  }
  return true;
}

bool spectate_follow(Spectator& s, const play::GameSpec& spec, std::span<const uint8_t> stream) {
  if (!s.error.empty()) return false;
  s.spec = &spec;
  if (!s.have_header) {
    std::string error;
    if (!read_stream_header(stream, s.reader, error)) return true;  // not whole yet
    const StreamHeader& h = s.reader.header;
    if (h.game != spec.name) {
      s.error = "the stream is a " + h.game + " run, not " + spec.name;
      return false;
    }
    if (h.game_version != spec.version) {
      s.error = "the stream was played on version " + std::to_string(h.game_version) + " of " +
                spec.name + "'s rules; this build has version " + std::to_string(spec.version);
      return false;
    }
    s.have_header = true;
  }
  s.reader.bytes = stream;
  for (;;) {
    Block b;
    std::string error;
    const size_t at = s.reader.at;
    if (!next_block(s.reader, b, error)) {
      s.reader.at = at;
      if (!error.empty() && error.find("cut short") == std::string::npos) {
        s.error = error;
        return false;
      }
      return true;
    }
    if (!s.run) {
      if (!b.keyframe) continue;  // wait for a keyframe block
      // Look ahead: the newest save the whole blocks so far hold, from this one's and the deltas
      // after it, and the block it is in.
      std::vector<uint8_t> save;
      int64_t save_step = -1, deltas = 0;
      size_t save_at = at;
      StreamReader look = s.reader;
      look.at = at;
      Block lb;
      size_t lat = at;
      while (next_block(look, lb, error)) {
        if (!save.empty() && !s.skip_ahead) break;
        for (const Chunk& c : lb.chunks) {
          if (c.track != kKeyframeTrack) continue;
          if (is_delta(c.bytes)) {
            if (save.empty()) continue;
            if (!apply_delta(c.bytes, save, save_step, error)) {
              s.error = error + " (block " + std::to_string(lb.index) + ")";
              return false;
            }
            deltas++;
          } else if (lb.keyframe) {
            save = c.bytes;
            save_step = first_step(lb);
            deltas = 0;
          }
          save_at = lat;
        }
        lat = look.at;
      }
      error.clear();
      if (save.empty()) {
        s.error = "a keyframe block with no keyframe in it (block " + std::to_string(b.index) + ")";
        return false;
      }
      int64_t step = 0;
      s.run = play::load_run(spec, save, step, error, "the stream's keyframe");
      if (!s.run) {
        s.error = error;
        return false;
      }
      s.step = step;
      s.started = step;
      s.deltas = deltas;
      if (save_at != at) {  // start at the block that holds it
        s.reader.at = save_at;
        if (!next_block(s.reader, b, error)) {
          s.error = "the block that held the newest keyframe no longer reads: " + error;
          return false;
        }
      }
    }
    for (const Chunk& c : b.chunks) {
      if (c.track != kInputsTrack) continue;
      In in{c.bytes};
      const int64_t first = static_cast<int64_t>(in.u());
      if (first != s.step) {
        s.error = "block " + std::to_string(b.index) + " starts at step " + std::to_string(first) +
                  " but the run is at step " + std::to_string(s.step);
        return false;
      }
      const uint64_t runs = in.u();
      std::vector<double> values(spec.axes.size(), 0);
      for (uint64_t r = 0; r < runs && !in.bad; r++) {
        const uint64_t count = in.u();
        for (size_t i = 0; i < values.size(); i++) values[i] = value_of(in.s(), spec.axes[i].kind);
        if (in.bad || count > (uint64_t{1} << 24)) break;
        for (uint64_t k = 0; k < count; k++) s.run->step(values);
        s.step += static_cast<int64_t>(count);
      }
      const uint64_t want = in.u64();
      if (in.bad) {
        s.error = "block " + std::to_string(b.index) + "'s inputs are damaged";
        return false;
      }
      s.digest = s.run->digest();
      s.blocks++;
      if (s.digest != want) {
        s.parted = b.index;
        s.error = "the run parted from the player's after block " + std::to_string(b.index) +
                  " (step " + std::to_string(s.step) + ")";
        return false;
      }
    }
  }
}

}  // namespace ge::replay
