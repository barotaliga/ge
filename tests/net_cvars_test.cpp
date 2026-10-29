// The network's cvars: bound to the replication config, the game's defaults kept, sv_fps latched
// and the durations right at any tick rate; the input backup and the predicted world set; voice
// chat's gate, codec and buffer.
//
// ge:summary Tests the network's cvars: binding, defaults, sv_fps latched, cl_interp to ticks
// ge:keywords test net cvars sv_fps tick-rate cl_interp latch
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "console/cvar.hpp"
#include "net/net_cvars.hpp"

using namespace ge;

TEST(net_cvars_keep_the_games_defaults) {
  console::Cvars cv;
  net::ReplicateConfig k;
  k.tick_hz = 72;  // a shooter's
  net::NetCvars n{&k};
  net::add_net_cvars(cv, n);
  CHECK(cv.find("sv_fps") && cv.find("sv_fps")->value == "72");
  CHECK(k.tick_hz == 72);
  net::net_cvars_apply(n);
  CHECK(k.interp_ticks == 7);  // 0.1 s at 72 Hz
}

TEST(net_cvars_sv_fps_waits_for_a_restart) {
  console::Cvars cv;
  net::ReplicateConfig k;
  net::NetCvars n{&k};
  net::add_net_cvars(cv, n);
  CHECK(cv.set("sv_fps", "72", console::Source::local) == console::SetResult::latched);
  CHECK(k.tick_hz == 60);
  cv.apply_latched();
  CHECK(k.tick_hz == 72);
  CHECK(cv.set("sv_fps", "1000", console::Source::local) == console::SetResult::latched);
  cv.apply_latched();
  CHECK(k.tick_hz == 240);  // clamped
}

TEST(net_cvars_set_the_players_rates_and_smoothing) {
  console::Cvars cv;
  net::ReplicateConfig k;
  net::NetCvars n{&k};
  net::add_net_cvars(cv, n);
  cv.set("cl_updaterate", "30", console::Source::local);
  cv.set("rate", "25000", console::Source::local);
  cv.set("cl_smooth", "0", console::Source::local);
  cv.set("cl_interp", "0.05", console::Source::local);
  net::net_cvars_apply(n);
  CHECK(k.update_rate == 30 && k.rate == 25000);
  CHECK(!k.spring && !k.cubic);
  CHECK(k.interp_ticks == 3);  // 0.05 s at 60 Hz
}

TEST(net_cvars_set_the_input_backup_and_the_predicted_world) {
  console::Cvars cv;
  net::ReplicateConfig k;
  net::NetCvars n{&k};
  net::add_net_cvars(cv, n);
  CHECK(cv.find("cl_cmdbackup") && cv.find("cl_cmdbackup")->value == std::to_string(k.redundancy));
  cv.set("cl_cmdbackup", "8", console::Source::local);
  cv.set("cl_predictworld", "6", console::Source::local);
  cv.set("cl_predictmax", "0.15", console::Source::local);
  net::net_cvars_apply(n);
  CHECK(k.redundancy == 8 && k.predict_radius == 6 && k.predict_max_s == 0.15);
  CHECK(cv.set("sv_rewindmax", "0.25", console::Source::local) == console::SetResult::changed);
  CHECK(k.rewind_max_s == 0.25);
  cv.set("cl_cmdbackup", "0", console::Source::local);
  CHECK(k.redundancy == 1);  // clamped: the input itself always goes
}

TEST(voice_cvars_set_the_gate_the_codec_and_the_buffer) {
  console::Cvars cv;
  net::VoiceConfig k;
  k.mode = net::VoiceMode::activity;  // a game's default
  net::VoiceCvars v{&k};
  net::add_voice_cvars(cv, v);
  CHECK(cv.find("voice_mode") && cv.find("voice_mode")->value == "1");
  cv.set("voice_mode", "0", console::Source::local);
  cv.set("voice_threshold", "-30", console::Source::local);
  cv.set("voice_bitrate", "16000", console::Source::local);
  cv.set("voice_fec", "0", console::Source::local);
  cv.set("voice_jitter", "0.1", console::Source::local);
  cv.set("voice_volume", "0.5", console::Source::local);
  net::voice_cvars_apply(v);
  CHECK(k.mode == net::VoiceMode::push_to_talk);
  CHECK(k.open_db == -30 && k.codec.bitrate == 16000 && !k.codec.fec);
  CHECK(k.jitter_max == 5);  // 0.1 s of 20 ms frames
  CHECK(k.jitter_start <= k.jitter_max);
  CHECK(k.volume == 0.5f);
  CHECK(cv.find("sv_voiceflood") && cv.find("sv_voiceflood")->value == "10");
}
