// THE NETWORK'S CVARS: the settings a cfg or the console changes, each bound to a field of the
// `ReplicateConfig` a server or a client is made with (docs/networking.md § Rates and live tools).
// `sv_` the server's (set on the server, sent to every client in the server info), `cl_` and
// `rate` the player's own (archived, sent to the server in the user info, which caps them).
//
//   sv_fps            the simulation's ticks a second (latched: a new map or a restart)
//   sv_maxupdaterate  sv_maxrate          the most snapshots a second and bytes a second sent
//   sv_rewindmax                          the furthest back a hit is judged (M17q, s)
//   cl_updaterate     cl_cmdrate  rate    the snapshots and input messages a second asked, bytes
//   cl_timesync       cl_smooth           time sync (M17n) and smoothing (M17o) on or off
//   cl_interp         cl_extrapolate      the fixed delay without sync (s), the extrapolation cap
//   cl_cmdbackup                          inputs repeated in each input message
//   cl_predictworld   cl_predictmax       the predicted world's radius (M17p, m), its horizon (s)
//
// VOICE CHAT's (`add_voice_cvars`, net/voice.hpp), bound to a VoiceConfig:
//   voice_mode        0 push to talk, 1 by voice activity, 2 always open
//   voice_threshold   voice_hangover      activity: the gate's opening level (dBFS), its hold (s)
//   voice_bitrate     voice_fec           the codec's bit/s and its in-band FEC
//   voice_loss                            the loss the FEC is sized for, %
//   voice_jitter      the longest playout delay a listener may choose, s
//   voice_volume      every voice heard
//   sv_voiceflood     the frames a talker may send at once before the host refuses them
//
// ge:summary The network's cvars: sv_fps, the rates, time sync and smoothing, bound to the config
// ge:keywords net cvars sv_fps tick-rate tick-hz sv_maxupdaterate sv_maxrate cl_updaterate
// ge:keywords cl_cmdrate rate cl_interp cl_timesync cl_smooth cl_extrapolate console
// ge:keywords cl_cmdbackup cl_predictworld cl_predictmax redundancy predicted-world sv_rewindmax
// ge:keywords voice_mode voice_threshold voice_hangover voice_bitrate voice_fec voice_volume
// ge:keywords voice_jitter sv_voiceflood push-to-talk
// ge:skill net
// ge:test tests/net_cvars_test.cpp
#pragma once

#include "console/cvar.hpp"
#include "net/replicate.hpp"
#include "net/voice.hpp"

namespace ge::net {

// Binds every network cvar to `config` (which must outlive `cvars`), writing each fallback into
// it now: a game passes the defaults it wants for its genre (a shooter's 72 Hz, a racer's 120).
// `cl_interp` writes `interp_s`, turned into `config.interp_ticks` by `net_cvars_apply`.
struct NetCvars {
  ReplicateConfig* config = nullptr;
  double interp_s = 0.1;  // cl_interp
};
void add_net_cvars(console::Cvars& cvars, NetCvars& out);
// The values that are not fields of the config as they stand (cl_interp to ticks at sv_fps):
// call after a cfg is exec'd and before the config is used.
void net_cvars_apply(NetCvars& n);

// Voice chat's cvars bound to `config` (which must outlive `cvars`), the game's values as the
// fallbacks. `voice_mode` and `voice_jitter` write `mode` and `jitter_s`, turned into the config's
// fields by `voice_cvars_apply`.
struct VoiceCvars {
  VoiceConfig* config = nullptr;
  int mode = 0;         // voice_mode
  double jitter_s = 0;  // voice_jitter
};
void add_voice_cvars(console::Cvars& cvars, VoiceCvars& out);
void voice_cvars_apply(VoiceCvars& v);

}  // namespace ge::net
