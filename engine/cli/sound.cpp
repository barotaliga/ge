// `ge sound`'s body (sound.hpp): ffmpeg to decode, then the trim, the BS.1770 loudness, the match
// features and the WAV, each answered as text, findings and one JSON result.
//
// ge:summary ge sound's body: decode, trim, loudness, features, match ranking, import and split
// ge:keywords cli sound run_sound decode_sound loud_span sound_pieces loudness_lufs shape_sound
// ge:keywords sound_features match_score fft k-weighting gate wav
// ge:skill agent-interface audio
// ge:test tests/cli_sound_test.cpp
#include "cli/sound.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numbers>
#include <regex>

#include "audio/wav.hpp"
#include "cli/build.hpp"
#include "cli/json.hpp"
#include "core/num_text.hpp"

namespace ge::cli {

namespace {

std::string fixed(double v, int digits) { return core::num::to_fixed(v, digits); }

bool parse_double(const std::string& text, double& out) {
  if (text.empty()) return false;
  char* end = nullptr;
  out = std::strtod(text.c_str(), &end);
  return end && *end == '\0' && std::isfinite(out);
}

Outcome failed(const std::string& what, const std::string& where, const std::string& next,
               int exit = 1) {
  Outcome o;
  o.findings.push_back({Severity::error, what, where, next});
  o.exit = exit;
  return o;
}

constexpr const char* kNoFfmpeg =
    "install ffmpeg (apt install ffmpeg, brew install ffmpeg) and put it on the PATH";

double db_of(double amplitude) { return amplitude > 0 ? 20 * std::log10(amplitude) : -200; }

// One biquad, direct form I, run in place.
struct Biquad {
  double b0, b1, b2, a1, a2;  // normalised by a0
  void run(std::vector<double>& x) const {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (double& v : x) {
      const double y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
      x2 = x1;
      x1 = v;
      y2 = y1;
      y1 = y;
      v = y;
    }
  }
};

// BS.1770's K-weighting at `rate`: the head's high shelf (+4 dB above about 1.7 kHz), then the
// revised low-frequency highpass (about 38 Hz), each a bilinear-transform design from its corner,
// gain and Q that gives the standard's own 48 kHz coefficients to the last digit at 48 kHz.
std::array<Biquad, 2> k_weighting(int rate) {
  const double pi = std::numbers::pi;
  std::array<Biquad, 2> k{};
  {
    const double fc = 1681.974450955533, gain_db = 3.999843853973347, q = 0.7071752369554196;
    const double kk = std::tan(pi * fc / rate), vh = std::pow(10.0, gain_db / 20);
    const double vb = std::pow(vh, 0.4996667741545416), a0 = 1 + kk / q + kk * kk;
    k[0] = {(vh + vb * kk / q + kk * kk) / a0, 2 * (kk * kk - vh) / a0,
            (vh - vb * kk / q + kk * kk) / a0, 2 * (kk * kk - 1) / a0, (1 - kk / q + kk * kk) / a0};
  }
  {
    const double fc = 38.13547087602444, q = 0.5003270373238773;
    const double kk = std::tan(pi * fc / rate), a0 = 1 + kk / q + kk * kk;
    k[1] = {1, -2, 1, 2 * (kk * kk - 1) / a0, (1 - kk / q + kk * kk) / a0};
  }
  return k;
}

// In-place radix-2 FFT; `x.size()` is a power of two.
void fft(std::vector<std::complex<double>>& x) {
  const size_t n = x.size();
  for (size_t i = 1, j = 0; i < n; i++) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(x[i], x[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2 * std::numbers::pi / static_cast<double>(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<double> w(1);
      for (size_t k = 0; k < len / 2; k++) {
        const std::complex<double> u = x[i + k], v = x[i + k + len / 2] * w;
        x[i + k] = u + v;
        x[i + k + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}

// The file decoded by ffmpeg: false with `error` set when it could not be.
bool decode_sound(const fs::path& file, int rate, std::vector<float>& out, std::string& error) {
  std::error_code ec;
  if (!fs::is_regular_file(file, ec)) {
    error = "no such file: " + file.string();
    return false;
  }
  const fs::path tmp = fs::temp_directory_path(ec) /
                       ("ge-sound-" + fnv1a64_hex(fs::absolute(file, ec).string()) + ".f32");
  const int code = run_quiet(ffmpeg_decode_line(file, tmp, rate));
  if (code != 0) {
    error = run_quiet("ffmpeg -version") == 0 ? "ffmpeg could not decode " + file.string()
                                              : std::string("ffmpeg is not installed");
    fs::remove(tmp, ec);
    return false;
  }
  std::ifstream in(tmp, std::ios::binary);
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  fs::remove(tmp, ec);
  out.resize(bytes.size() / sizeof(float));
  if (!out.empty()) std::memcpy(out.data(), bytes.data(), out.size() * sizeof(float));
  return true;
}

bool is_sound_file(const fs::path& p) {
  std::string e = p.extension().string();
  std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return std::tolower(c); });
  return e == ".wav" || e == ".ogg" || e == ".flac" || e == ".mp3" || e == ".opus" || e == ".aif" ||
         e == ".aiff";
}

// The candidates named: files as they are, directories walked for every sound file, sorted.
std::vector<fs::path> expand_candidates(const std::vector<std::string>& words) {
  std::vector<fs::path> out;
  for (const std::string& w : words) {
    std::error_code ec;
    if (fs::is_directory(w, ec)) {
      for (auto it = fs::recursive_directory_iterator(w, ec); !ec && it != fs::end(it);
           it.increment(ec)) {
        if (it->is_regular_file(ec) && is_sound_file(it->path())) out.push_back(it->path());
      }
    } else {
      out.emplace_back(w);
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::string features_json(const SoundFeatures& f) {
  std::vector<std::string> b, e;
  for (double v : f.bands) b.push_back(fixed(v, 3));
  for (double v : f.envelope) e.push_back(fixed(v, 3));
  return json_obj(
      {{"seconds", fixed(f.seconds, 3)}, {"bands", json_arr(b)}, {"envelope", json_arr(e)}});
}

bool write_wav(const fs::path& path, std::span<const float> x, int rate, std::string& error) {
  std::error_code ec;
  if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
  const std::vector<uint8_t> bytes = audio::wav_bytes(x, 1, rate);
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    error = "could not write " + path.string();
    return false;
  }
  return true;
}

struct Common {
  int rate = 22050;
  Shape shape;
  fs::path to;
};

std::string shaped_line(const std::string& name, const Shaped& s) {
  return "  " + name + ": " + fixed(s.seconds, 2) + " s, " + fixed(s.lufs, 1) + " LUFS, peak " +
         fixed(s.peak_db, 1) + " dBFS" + (s.peak_limited ? " (held at the peak ceiling)" : "") +
         "\n";
}

std::string shaped_json(const std::string& name, const fs::path& file, const Shaped& s) {
  return json_obj({{"name", json_str(name)},
                   {"file", json_str(rel_path(fs::current_path(), file))},
                   {"seconds", fixed(s.seconds, 3)},
                   {"lufs", fixed(s.lufs, 2)},
                   {"peak_db", fixed(s.peak_db, 2)},
                   {"peak_limited", json_bool(s.peak_limited)}});
}

Outcome run_match(const Args& a, int rate) {
  std::string error;
  std::vector<float> ref;
  if (!decode_sound(a.words[0], rate, ref, error)) return failed(error, a.words[0], kNoFfmpeg);
  const SoundFeatures rf = sound_features(ref, rate);
  if (rf.seconds <= 0) {
    return failed("the reference is silent", a.words[0], "pass the sound to stand in for");
  }
  std::vector<std::string> rest(a.words.begin() + 1, a.words.end());
  std::vector<fs::path> files = expand_candidates(rest);
  const std::string pool = flag_value(a, "pool");
  if (!pool.empty()) {
    std::regex rx;
    try {
      rx = std::regex(pool, std::regex::icase | std::regex::ECMAScript);
    } catch (const std::regex_error&) {
      return failed("--pool=" + pool + " is not a regular expression", "the command line",
                    "write it as `splash|water`", 2);
    }
    std::erase_if(files, [&](const fs::path& p) { return !std::regex_search(p.string(), rx); });
  }
  double top = 5;
  if (const std::string t = flag_value(a, "top");
      !t.empty() && (!parse_double(t, top) || top < 1)) {
    return failed("--top=" + t + " is not a count", "the command line", "--top=5", 2);
  }
  Outcome o;
  struct Ranked {
    double score;
    fs::path file;
    SoundFeatures f;
  };
  std::vector<Ranked> ranked;
  int unread = 0;
  for (const fs::path& f : files) {
    std::vector<float> x;
    if (!decode_sound(f, rate, x, error)) {
      unread++;
      continue;
    }
    const SoundFeatures cf = sound_features(x, rate);
    if (cf.seconds <= 0) continue;
    ranked.push_back({match_score(rf, cf), f, cf});
  }
  if (unread) {
    o.findings.push_back({Severity::warn, std::to_string(unread) + " candidate(s) did not decode",
                          "", "check them with `ffmpeg -i FILE`"});
  }
  if (ranked.empty()) {
    return failed("no candidate to rank (" + std::to_string(files.size()) + " after --pool)",
                  "the command line", "name files or directories of sounds after the reference");
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const Ranked& x, const Ranked& y) { return x.score < y.score; });
  if (ranked.size() > static_cast<size_t>(top)) ranked.resize(static_cast<size_t>(top));
  o.text = "  reference " + a.words[0] + ": " + fixed(rf.seconds, 2) + " s, " +
           std::to_string(files.size()) + " candidate(s)\n";
  std::vector<std::string> rows;
  for (const Ranked& r : ranked) {
    o.text +=
        "  " + fixed(r.score, 2) + "  " + fixed(r.f.seconds, 2) + " s  " + r.file.string() + "\n";
    rows.push_back(json_obj({{"score", fixed(r.score, 4)},
                             {"file", json_str(r.file.string())},
                             {"features", features_json(r.f)}}));
  }
  o.result = json_obj(
      {{"reference", json_obj({{"file", json_str(a.words[0])}, {"features", features_json(rf)}})},
       {"candidates", json_num(static_cast<long long>(files.size()))},
       {"ranked", json_arr(rows)}});
  o.summary = "best of " + std::to_string(files.size()) + ": " + ranked[0].file.string();
  o.next = "ge sound import \"" + ranked[0].file.string() + "\" <our/name> --to=sounds";
  return o;
}

Outcome run_import(const Args& a, const Common& c) {
  std::string error;
  std::vector<float> x;
  if (!decode_sound(a.words[0], c.rate, x, error)) return failed(error, a.words[0], kNoFfmpeg);
  const Shaped s = shape_sound(x, c.rate, c.shape);
  if (s.samples.empty()) {
    return failed(
        "nothing in " + a.words[0] + " is louder than " + fixed(c.shape.threshold_db, 0) + " dBFS",
        a.words[0], "lower --threshold or pick another file");
  }
  const fs::path file = c.to / (a.words[1] + ".wav");
  if (!write_wav(file, s.samples, c.rate, error)) return failed(error, file.string(), "");
  Outcome o;
  o.text = shaped_line(a.words[1], s);
  o.written.push_back(rel_path(fs::current_path(), file));
  o.result = shaped_json(a.words[1], file, s);
  o.summary = "wrote " + rel_path(fs::current_path(), file);
  o.next = "record its source in the credits (skill: free-assets)";
  return o;
}

Outcome run_split(const Args& a, const Common& c) {
  std::string error;
  std::vector<float> x;
  if (!decode_sound(a.words[0], c.rate, x, error)) return failed(error, a.words[0], kNoFfmpeg);
  double gap = 0.15, min = 0.05;
  if (const std::string g = flag_value(a, "gap");
      !g.empty() && (!parse_double(g, gap) || gap < 0)) {
    return failed("--gap=" + g + " is not seconds", "the command line", "--gap=0.15", 2);
  }
  const std::vector<SampleSpan> pieces = sound_pieces(x, c.rate, c.shape.threshold_db, gap, min);
  if (pieces.empty()) {
    return failed("no sound in " + a.words[0], a.words[0], "lower --threshold");
  }
  Outcome o;
  std::vector<std::string> rows;
  int n = 0;
  for (const SampleSpan& p : pieces) {
    n++;
    const std::string num = (n < 10 ? "0" : "") + std::to_string(n);
    const std::string name = a.words[1] + "_" + num;
    const Shaped s =
        shape_sound(std::span<const float>(x).subspan(p.begin, p.end - p.begin), c.rate, c.shape);
    const fs::path file = c.to / (name + ".wav");
    if (!write_wav(file, s.samples, c.rate, error)) return failed(error, file.string(), "");
    o.text += shaped_line(name, s);
    o.written.push_back(rel_path(fs::current_path(), file));
    rows.push_back(shaped_json(name, file, s));
  }
  o.result = json_arr(rows);
  o.summary = "split into " + std::to_string(pieces.size()) + " piece(s)";
  return o;
}

}  // namespace

std::string ffmpeg_decode_line(const fs::path& in, const fs::path& out, int rate) {
  return "ffmpeg -nostdin -v error -y -i \"" + in.string() + "\" -ac 1 -ar " +
         std::to_string(rate) + " -f f32le \"" + out.string() + "\"";
}

SampleSpan loud_span(std::span<const float> x, double threshold_db) {
  const double t = std::pow(10.0, threshold_db / 20);
  size_t b = 0, e = x.size();
  while (b < e && std::abs(x[b]) <= t) b++;
  while (e > b && std::abs(x[e - 1]) <= t) e--;
  if (b == e) return {};
  return {b, e};
}

std::vector<SampleSpan> sound_pieces(std::span<const float> x, int rate, double threshold_db,
                                     double gap_s, double min_s) {
  const double t = std::pow(10.0, threshold_db / 20);
  const size_t gap = static_cast<size_t>(gap_s * rate), min = static_cast<size_t>(min_s * rate);
  // Loudness in 10 ms windows, so a waveform's own zero crossings are no gap.
  const size_t w = std::max<size_t>(1, static_cast<size_t>(rate) / 100);
  std::vector<SampleSpan> runs;
  for (size_t i = 0; i < x.size(); i += w) {
    const size_t e = std::min(x.size(), i + w);
    float peak = 0;
    for (size_t k = i; k < e; k++) peak = std::max(peak, std::abs(x[k]));
    if (peak <= t) continue;
    if (!runs.empty() && i - runs.back().end <= gap) {
      runs.back().end = e;
    } else {
      runs.push_back({i, e});
    }
  }
  std::erase_if(runs, [&](const SampleSpan& s) { return s.end - s.begin < min; });
  return runs;
}

double loudness_lufs(std::span<const float> x, int rate) {
  if (x.empty()) return -200;
  std::vector<double> y(x.begin(), x.end());
  for (const Biquad& b : k_weighting(rate)) b.run(y);
  const size_t block = static_cast<size_t>(0.4 * rate), step = static_cast<size_t>(0.1 * rate);
  const auto lufs_of = [](double z) { return z > 0 ? -0.691 + 10 * std::log10(z) : -200.0; };
  if (y.size() < block) {
    double z = 0;
    for (double v : y) z += v * v;
    return lufs_of(z / static_cast<double>(y.size()));
  }
  std::vector<double> zs;
  for (size_t i = 0; i + block <= y.size(); i += step) {
    double z = 0;
    for (size_t k = i; k < i + block; k++) z += y[k] * y[k];
    zs.push_back(z / static_cast<double>(block));
  }
  const auto mean_above = [&](double gate) {
    double sum = 0;
    size_t n = 0;
    for (double z : zs) {
      if (lufs_of(z) > gate) {
        sum += z;
        n++;
      }
    }
    return n ? sum / static_cast<double>(n) : 0.0;
  };
  const double ungated = mean_above(-70);
  if (ungated <= 0) return -200;
  return lufs_of(mean_above(lufs_of(ungated) - 10));
}

double peak_dbfs(std::span<const float> x) {
  float p = 0;
  for (float v : x) p = std::max(p, std::abs(v));
  return db_of(p);
}

Shaped shape_sound(std::span<const float> x, int rate, const Shape& s) {
  Shaped out;
  const SampleSpan span = loud_span(x, s.threshold_db);
  if (span.end == span.begin) return out;
  const size_t most = static_cast<size_t>(s.max_s * rate);
  const size_t end = std::min(span.end, span.begin + std::max<size_t>(1, most));
  out.samples.assign(x.begin() + static_cast<std::ptrdiff_t>(span.begin),
                     x.begin() + static_cast<std::ptrdiff_t>(end));
  std::vector<float>& y = out.samples;
  const size_t in = std::min(y.size(), static_cast<size_t>(0.005 * rate));
  const size_t tail = std::min(y.size(), static_cast<size_t>((s.loop ? 0.05 : 0.02) * rate));
  for (size_t i = 0; i < in; i++) y[i] *= static_cast<float>(i) / static_cast<float>(in);
  for (size_t i = 0; i < tail; i++) {
    y[y.size() - 1 - i] *= static_cast<float>(i) / static_cast<float>(tail);
  }
  const double now = loudness_lufs(y, rate), peak = peak_dbfs(y);
  double gain_db = s.lufs - now;
  if (peak + gain_db > s.peak_db) {
    gain_db = s.peak_db - peak;
    out.peak_limited = true;
  }
  const float g = static_cast<float>(std::pow(10.0, gain_db / 20));
  for (float& v : y) v *= g;
  out.seconds = static_cast<double>(y.size()) / rate;
  out.lufs = loudness_lufs(y, rate);
  out.peak_db = peak_dbfs(y);
  return out;
}

SoundFeatures sound_features(std::span<const float> x, int rate) {
  SoundFeatures f;
  float most = 0;
  for (float v : x) most = std::max(most, std::abs(v));
  if (most <= 0) return f;
  const SampleSpan s = loud_span(x, db_of(0.01 * most));
  if (s.end - s.begin < 64) return f;
  const std::span<const float> y = x.subspan(s.begin, s.end - s.begin);
  f.seconds = static_cast<double>(y.size()) / rate;

  const size_t head = std::min(y.size(), static_cast<size_t>(2 * rate));
  size_t n = 2048;
  while (n < head) n <<= 1;
  std::vector<std::complex<double>> spec(n);
  for (size_t i = 0; i < head; i++) spec[i] = y[i];
  fft(spec);
  static constexpr double kEdges[] = {0, 150, 400, 800, 1600, 3200, 6400, 11025};
  double loudest = -1e9;
  for (size_t b = 0; b < f.bands.size(); b++) {
    double sum = 0;
    for (size_t k = 0; k <= n / 2; k++) {
      const double hz = static_cast<double>(k) * rate / static_cast<double>(n);
      if (hz >= kEdges[b] && hz < kEdges[b + 1]) sum += std::abs(spec[k]);
    }
    f.bands[b] = std::log10(1e-9 + sum);
    loudest = std::max(loudest, f.bands[b]);
  }
  for (double& b : f.bands) b -= loudest;

  double top = 0;
  for (size_t i = 0; i < f.envelope.size(); i++) {
    const size_t a = y.size() * i / f.envelope.size(), e = y.size() * (i + 1) / f.envelope.size();
    double z = 0;
    for (size_t k = a; k < e; k++) z += static_cast<double>(y[k]) * y[k];
    f.envelope[i] = e > a ? std::sqrt(z / static_cast<double>(e - a)) : 0;
    top = std::max(top, f.envelope[i]);
  }
  if (top > 0) {
    for (double& v : f.envelope) v /= top;
  }
  return f;
}

double match_score(const SoundFeatures& r, const SoundFeatures& c) {
  const double len = std::abs(std::log(std::max(c.seconds, 0.02) / std::max(r.seconds, 0.02)));
  double bands = 0, env = 0;
  for (size_t i = 0; i < r.bands.size(); i++) bands += std::abs(r.bands[i] - c.bands[i]);
  for (size_t i = 0; i < r.envelope.size(); i++) env += std::abs(r.envelope[i] - c.envelope[i]);
  return len + 0.8 * bands / static_cast<double>(r.bands.size()) +
         2.0 * env / static_cast<double>(r.envelope.size());
}

Outcome run_sound(const Args& a) {
  const std::string name(a.command->name);
  Common c;
  double v = 0;
  const auto number = [&](const char* flag, double& out) {
    const std::string t = flag_value(a, flag);
    return t.empty() || parse_double(t, out);
  };
  const auto bad = [&](const char* flag) {
    return failed(std::string("--") + flag + "=" + flag_value(a, flag) + " is not a number",
                  "the command line", "run `ge help " + name + "` for its flags", 2);
  };
  v = c.rate;
  if (!number("rate", v) || v < 8000 || v > 192000) return bad("rate");
  c.rate = static_cast<int>(v);
  if (name == "sound match") return run_match(a, c.rate);
  if (!number("max", c.shape.max_s) || c.shape.max_s <= 0) return bad("max");
  if (!number("lufs", c.shape.lufs)) return bad("lufs");
  if (!number("threshold", c.shape.threshold_db)) return bad("threshold");
  c.shape.loop = has_flag(a, "loop");
  c.to = flag_value(a, "to", "sounds");
  if (name == "sound import") return run_import(a, c);
  return run_split(a, c);
}

}  // namespace ge::cli
