// LAB `voice`: voice chat over the simulated network, once under each condition preset. A
// dedicated host seats four talking clients, each with a voice of its own on a route of its own
// (everyone, its team, one other, the near ones); every client hears the others through its jitter
// buffer onto its synth. It answers what a link does to a voice: that every voice reached exactly
// the listeners its route names, the frames played, rebuilt from the next frame's FEC copy,
// concealed, late and dropped, the playout delay the buffers chose, the mouth-to-ear latency and
// the bytes each way; then, measured against the same frames decoded with nothing lost, how much
// of the voice survived (segmental SNR) and how loud it came out; and, on the worst links, the same
// again with FEC off. The playout delay is charted over the run, the latencies drawn as boxes, the
// ear of the client most routes reach charted as a level against the clean link's, and each
// preset's mix written as a WAV to listen to. The network and the session are deterministic; the
// decoded samples are the codec's (measured, never hashed), so a build without Opus has the same
// digest.
//
// ge:summary Lab: voice chat over each preset: routes, FEC, concealment, delay, latency, SNR, WAVs
// ge:keywords lab net voice voice-chat opus jitter-buffer playout fec conceal latency mouth-to-ear
// ge:keywords snr segmental-snr route team whisper proximity preset wav multiplayer
// ge:skill net audio labs
// ge:test tests/net_voice_test.cpp
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/wav.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/voice_trial.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::string hex(uint64_t h) {
  char b[20];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

std::string pct(uint64_t part, uint64_t whole) {
  return whole ? fmt(100.0 * static_cast<double>(part) / static_cast<double>(whole), 1) + "%" : "-";
}

// The concealed share a preset's voice is held to. A voice crosses two links (the talker's to the
// host, the host's to the listener), so it meets the loss twice; FEC rebuilds a single loss, so
// at least a quarter of the frames lost even in the presets' bursts come back.
double conceal_bound(const gn::LinkConditions& c) {
  const double lost = 1 - (1 - c.loss) * (1 - c.loss);
  return 0.005 + 0.75 * lost;
}

// The ear's level, dB RMS, every 20 ms.
std::vector<double> envelope(const std::vector<float>& stereo) {
  std::vector<double> out;
  const size_t frame = 2 * static_cast<size_t>(gn::kVoiceFrame);
  for (size_t at = 0; at + frame <= stereo.size(); at += frame) {
    double sum = 0;
    for (size_t i = 0; i < 2 * static_cast<size_t>(gn::kVoiceFrame); i++)
      sum += static_cast<double>(stereo[at + i]) * stereo[at + i];
    const double ms = sum / (2 * gn::kVoiceFrame);
    out.push_back(ms > 1e-12 ? 4.3429448190325182 * ge::core::libm::log(ms) : -120);
  }
  return out;
}

void run(gl::Lab& lab) {
  gn::VoiceTrialConfig k;
  k.clients = static_cast<int>(lab.arg_num("clients"));
  k.seconds = lab.arg_num("seconds");
  k.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  k.voice.codec.bitrate = static_cast<int>(lab.arg_num("bitrate"));
  k.voice.codec.fec = lab.arg_num("fec") != 0;
  k.voice.codec.loss_percent = static_cast<int>(lab.arg_num("loss"));
  k.keep_audio = true;
  if (k.clients < 2 || k.clients > gn::kMaxPlayers || k.seconds < 2 || k.seconds > 600 ||
      k.voice.codec.bitrate < 8000 || k.voice.codec.bitrate > 80000 ||
      k.voice.codec.loss_percent < 0 || k.voice.codec.loss_percent > 50) {
    lab.fail("--clients, --seconds, --bitrate or --loss is out of range",
             "Pass --clients=2 to 32, --seconds=2 to 600, --bitrate=8000 to 80000 and --loss=0 "
             "to 50; skill: net.");
    return;
  }
  if (!gn::voice_codec_available())
    lab.note(
        "this build has no Opus (GE_ENCODERS is OFF): every voice decodes to silence, so the "
        "measured table and the WAVs are empty; the deterministic tables are the same");
  const std::string fec_word = k.voice.codec.fec ? "FEC on" : "FEC off";
  gl::LabTable heard{std::to_string(k.clients) + " talkers, " +
                         std::to_string(k.voice.codec.bitrate / 1000) + " kbit/s, " + fec_word +
                         ", " + fmt(k.seconds, 0) + " s",
                     {"preset", "routes", "sent", "received", "played", "rebuilt", "concealed",
                      "late", "dropped", "delay ms p50", "max", "mouth-to-ear p50", "p95",
                      "up kbit/s", "down kbit/s", "meets", "log"},
                     {}};
  gl::LabTable sound{"What was heard against the clean decode (measured: the codec's samples)",
                     {"preset", "segmental SNR dB", "worst pair", "level vs clean dB"},
                     {}};
  gl::LabTable nofec{"The same links with FEC off",
                     {"preset", "rebuilt", "concealed", "concealed with FEC", "log"},
                     {}};
  gl::LabTable nofec_sound{
      "With FEC off, what was heard (measured)", {"preset", "segmental SNR dB", "with FEC"}, {}};
  gl::LineChart delay{{"PLAYOUT DELAY, EACH PRESET", "the first heard voice's jitter buffer",
                       "t (s)", "delay (ms)"},
                      {},
                      {},
                      {},
                      {},
                      false,
                      false,
                      false};
  gl::LineChart ear{{"ONE CLIENT'S EAR", "the mix of the client most routes reach, every 20 ms",
                     "t (s)", "level (dBFS)"},
                    {},
                    {},
                    {},
                    {},
                    false,
                    false,
                    false};
  std::vector<gl::Sample> latencies;
  const std::vector<std::string> worst = split(lab.arg("nofec"));
  for (const std::string& name : split(lab.arg("presets"))) {
    if (!gn::conditions_preset(name, k.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    const gn::VoiceTrial o = gn::run_voice_trial(k);
    const gl::Summary lat = gl::summarize(o.latency_ms), del = gl::summarize(o.delay_ms);
    const uint64_t turns = o.played + o.recovered + o.concealed;
    const double concealed =
        turns ? static_cast<double>(o.concealed) / static_cast<double>(turns) : 0;
    const bool meets = o.routes_ok && concealed <= conceal_bound(k.link);
    heard.rows.push_back({name, o.routes_ok ? "ok" : "NO", std::to_string(o.sent),
                          std::to_string(o.received), pct(o.played, turns), pct(o.recovered, turns),
                          pct(o.concealed, turns), std::to_string(o.late),
                          std::to_string(o.dropped), fmt(del.p50, 0), fmt(del.max, 0),
                          fmt(lat.p50, 0), fmt(lat.p95, 0), fmt(o.up_kbps, 1), fmt(o.down_kbps, 1),
                          meets ? "yes" : "NO", hex(o.hash)});
    sound.rows.push_back({name, fmt(o.snr_mean_db, 1), fmt(o.snr_worst_db, 1), fmt(o.level_db, 1)});
    const std::string key = name + "-";
    lab.metric(key + "concealed", concealed, "ratio");
    lab.metric(key + "rebuilt",
               turns ? static_cast<double>(o.recovered) / static_cast<double>(turns) : 0, "ratio");
    lab.metric(key + "mouth-to-ear-p95", lat.p95, "ms");
    lab.metric(key + "delay-p50", del.p50, "ms");
    lab.metric(key + "up-kbps", o.up_kbps, "kbit/s");
    lab.metric(key + "snr", o.snr_mean_db, "dB", gl::Det::measured);
    delay.traces.push_back({name, o.t_s, o.target_ms});
    latencies.push_back({name, o.latency_ms});
    if (!o.routes_ok)
      lab.fail(name + ": a voice reached a listener its route does not name, or missed one (" +
                   std::to_string(o.seated) + "/" + std::to_string(k.clients) + " seated)",
               "Read voice_route and relay_one in engine/net/voice.cpp, then run "
               "tests/net_voice_test.cpp; skill: net.");
    if (concealed > conceal_bound(k.link))
      lab.fail(name + ": " + pct(o.concealed, turns) + " of the voice was concealed, past the " +
                   fmt(100 * conceal_bound(k.link), 1) + "% its loss allows",
               "Read voice_talker_pull's recovery and choose_target in engine/net/voice.cpp; a "
               "frame concealed that arrived late means the playout delay is too short; skill: "
               "net.");
    if (o.relay.flooded)
      lab.fail(name + ": the relay refused " + std::to_string(o.relay.flooded) +
                   " honest frames as a flood",
               "A talker sends one frame each 20 ms: read the bucket in relay_one "
               "(engine/net/voice.cpp) and VoiceConfig::flood_burst; skill: net.");
    // The ear, seconds 2 to 8, and the mix to listen to.
    std::vector<double> env = envelope(o.mix);
    std::vector<double> xs, ys;
    for (size_t i = 100; i < env.size() && i < 400; i++) {
      xs.push_back(static_cast<double>(i) * 0.02);
      ys.push_back(std::max(-80.0, env[i]));
    }
    ear.traces.push_back({name, xs, ys});
    lab.attach(name + ".wav", ge::audio::wav_bytes(o.mix), gl::Det::measured);
    if (std::find(worst.begin(), worst.end(), name) != worst.end() && k.voice.codec.fec) {
      gn::VoiceTrialConfig off = k;
      off.voice.codec.fec = false;
      const gn::VoiceTrial b = gn::run_voice_trial(off);
      const uint64_t bt = b.played + b.recovered + b.concealed;
      nofec.rows.push_back(
          {name, pct(b.recovered, bt), pct(b.concealed, bt), pct(o.concealed, turns), hex(b.hash)});
      nofec_sound.rows.push_back({name, fmt(b.snr_mean_db, 1), fmt(o.snr_mean_db, 1)});
      lab.metric(key + "concealed-nofec",
                 bt ? static_cast<double>(b.concealed) / static_cast<double>(bt) : 0, "ratio");
    }
  }
  lab.table(std::move(heard));
  lab.table(std::move(sound), gl::Det::measured);
  if (!nofec.rows.empty()) {
    lab.table(std::move(nofec));
    lab.table(std::move(nofec_sound), gl::Det::measured);
  }
  lab.note(
      "routes: every voice reached exactly the listeners its route names (the first talker to "
      "everyone, pushing to talk; the second to its team, the third to the fourth alone, the "
      "fourth to the clients within earshot; the rest by voice activity). sent: frames the "
      "talkers' gates let through. played, rebuilt, concealed: each listener's turns, the frame "
      "decoded, rebuilt from the next frame's in-band FEC copy, or the decoder's concealment. "
      "late: frames that came after their turn; dropped: frames dropped to bring a deep buffer "
      "back to its target. delay: the playout delay chosen at each spurt's start. mouth-to-ear: "
      "from a frame's first sample spoken to its first sample heard, the synth's two queued "
      "frames included. up: every packet a talker sent; down: every packet the host sent, per "
      "listener. meets: routes ok and the concealed share within 0.5% plus three quarters of the "
      "loss over the two links a voice crosses. segmental SNR: what each listener heard against "
      "the same frames decoded with nothing "
      "lost, every voiced 20 ms clamped to -10..35 dB and averaged (35: the same samples).");
  const auto shoot = [&](const std::string& id, const gl::Chart& chart, const std::string& caption,
                         const std::string& look_for, gl::Det det) {
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()), det);
    lab.shot({id,
              chart.title,
              caption,
              look_for,
              {{"presets", lab.arg("presets")},
               {"clients", lab.arg("clients")},
               {"bitrate", lab.arg("bitrate")},
               {"fec", lab.arg("fec")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot), det);
  };
  if (delay.traces.empty()) return;
  shoot("delay", gl::line_chart(delay),
        "The playout delay of the first voice any client heard, every 100 ms, one line a preset "
        "(0 between its spurts).",
        "a flat line one or two frames up on a quiet link; a jittery one starts higher and the "
        "steps move only at a spurt's start, never inside a word.",
        gl::Det::deterministic);
  shoot("latency",
        gl::box_chart(
            {"MOUTH TO EAR, EACH PRESET", "every frame played or rebuilt", "latency (ms)", ""},
            latencies),
        "How long a voice takes from the talker's mouth to a listener's ear, one box a preset: "
        "p25 to p75, the median, whiskers p5 to p95.",
        "the LAN's box near the frame, the queue and a frame of buffer (about 60 ms); each worse "
        "link adds its one-way latency and the delay its jitter asks for.",
        gl::Det::deterministic);
  shoot("ear", gl::line_chart(ear),
        "The level of the mix of the client the most routes reach, every 20 ms from second 2 to 8, "
        "one line a preset.",
        "the lines one atop another, the worse links a little later; a dip under the LAN's line is "
        "a stretch concealed, a word missing altogether a route or a gate gone wrong.",
        gl::Det::measured);
}

const gl::LabSpec kSpec{
    "voice",
    gl::LabKind::sim,
    "engine",
    "engine/net (voice, voice_codec, voice_trial), engine/audio (streams)",
    "net",
    "On each network condition, does every voice reach exactly the listeners its route names, how "
    "much of it is rebuilt or concealed, how long does it take mouth to ear, and how close is what "
    "is heard to the clean decode?",
    {
        {"presets", "lan,wifi,bad,awful", "condition presets, comma-separated"},
        {"clients", "4", "talking clients (2 to 32)"},
        {"seconds", "20", "how long each session talks"},
        {"bitrate", "24000", "the codec's bitrate, bit/s"},
        {"fec", "1", "in-band FEC (0: off)"},
        {"loss", "10", "the loss the FEC is sized for, %"},
        {"nofec", "bad,awful", "presets run again with FEC off, comma-separated"},
        {"seed", "1", "the network's and the voices' stream"},
    },
    "--seconds=4 --presets=lan,bad --nofec=bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
