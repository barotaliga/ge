// THE SERVER'S STREAM PORT FOR CLASSIC RELAYS (docs/tv.md § The classic face): a classic match
// broadcaster connects over TCP, asks for the match in a few lines of text, and is sent the match
// as a multiview recording as it is played, which it delays and fans out to its own viewers.
//
// THE HANDSHAKE: the relay's request is lines ending in a blank line (`QTV`, `VERSION: 1`,
// `RECEIVE` or `SOURCE: <what>`, `USERINFO: "<info>"`, `AUTH: <method>` offers, `PASSWORD:
// "<response>"`, `RAW: <n>`); the answer is `QTVSV 1` and then `BEGIN` (the records follow),
// `AUTH: <method>` with `CHALLENGE: <text>` (the relay sends its whole request again with the
// response), or `PERROR`, `TERROR`, `ERROR` and why. `RAW: 1` asks for the records with no answer
// text at all. With no password set every relay is let in; with one, the strongest method both
// sides know is asked for: SHA3-512 of the challenge and the password as 128 lower-case hex digits,
// or the plain password.
//
// THE STREAM: each relay has its own multiview recorder (classic_record.hpp) hung on the server's
// `streams`, started when the relay is let in, so its first frame is the whole signon (the relay's
// catch-up) and every map change another; `stream` set, the signon also stuffs the full server info
// and the `skins` line a relay waits for before it feeds its viewers. The bytes go out as the frame
// writes them; a relay that takes none for `stream_timeout_s`, or lets `max_queue` bytes pile up,
// is dropped.
//
// UP FROM A RELAY: messages of a 16-bit little-endian length (itself counted), a kind (1: a
// command, the only kind taken; any other drops the relay) and a NUL-ended command. `say`,
// `say_team` and `say_game` print `#0:qtv_<cmd>_game:#<viewer>:<relay>: <text>` to the players
// (`say_team`, and `say` unless `say_enabled`, to the spectators alone) and into every relay's
// stream; `qul <action> <id> "<name>"` keeps the relay's list of viewers (1 joined, 2 renamed, 3
// left) and tells the players `QTV: <relay>: <name> join`, `left` or `changed name to <new>`.
//
// The same port answers nothing else: a connection whose first line is not `QTV` is closed.
//
// ge:summary The server's stream port for classic relays: handshake, auth, per-relay stream, chat
// ge:keywords qtv stream-port broadcaster relay handshake qtvsv begin auth challenge sha3-512
// ge:keywords up-message say_game qul viewers catch-up qtv_streamport qtv_password qtv_maxstreams
// ge:keywords qtv_accept
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_qtv_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

// ---- The text both sides write ----

enum class QtvAuth : uint8_t { none, plain, sha3_512 };  // weakest first
const char* qtv_auth_name(QtvAuth a);

struct QtvRequest {
  int version = 0;  // 0: no VERSION line
  bool receive = false;
  std::string source;    // SOURCE (a server streams its one match whatever it names)
  std::string userinfo;  // USERINFO, its quotes taken off
  std::vector<QtvAuth> offers;
  bool has_password = false;
  std::string password;  // PASSWORD, its quotes taken off
  int raw = 0;
};

enum class QtvParse : uint8_t { more, ok, bad };

// The request at the front of `text`, through its blank line: `used` its bytes. `bad` when the
// first line is not `QTV`; unknown lines are skipped.
QtvParse qtv_parse_request(std::string_view text, QtvRequest& out, size_t& used);
std::string qtv_request_text(const QtvRequest& r);

enum class QtvAnswerKind : uint8_t { begin, auth, perror, terror, error };
struct QtvAnswer {
  QtvAnswerKind kind = QtvAnswerKind::begin;
  std::string text;  // the error's words
  QtvAuth method = QtvAuth::none;
  std::string challenge;
};
QtvParse qtv_parse_answer(std::string_view text, QtvAnswer& out, size_t& used);
std::string qtv_answer_text(const QtvAnswer& a);

// What a relay answers a challenge with for `method`.
std::string qtv_auth_response(QtvAuth method, const std::string& challenge,
                              const std::string& password);
// SHA3-512 of `bytes` as 128 lower-case hex digits.
std::string sha3_512_hex(std::string_view bytes);

// An up message: a command (kind 1).
void qtv_put_up(std::vector<uint8_t>& out, const std::string& command);
// The next up message at the front of `bytes`: `more` until it is whole, `bad` for a length out of
// range or a kind other than a command.
QtvParse qtv_next_up(const uint8_t* bytes, size_t size, std::string& command, size_t& used);

// ---- The server's port ----

struct QtvConfig {
  std::string password;                 // qtv_password (empty: no auth)
  int max_streams = 1;                  // qtv_maxstreams (0 or less: no limit)
  double pending_s = 5;                 // qtv_pendingtimeout: a handshake's time
  double stream_timeout_s = 45;         // qtv_streamtimeout: no bytes taken for this long drops
  size_t max_queue = size_t{16} << 20;  // qtv_stream_max_queue_size: bytes waiting, at most
  bool say_enabled = false;             // qtv_sayenabled: a relay's say reaches the players too
};

enum class QtvState : uint8_t { pending, challenged, streaming };

struct QtvViewer {
  int id = 0;
  std::string name;
};

struct QtvRelay {
  int id = 0;
  gn::TcpStream tcp;
  QtvState state = QtvState::pending;
  double since = 0;          // the server's time it connected
  double last_taken = 0;     // the server's time the OS last took bytes
  std::string in;            // the handshake's text so far
  std::vector<uint8_t> up;   // up messages' bytes so far
  std::vector<uint8_t> out;  // stream bytes waiting for the OS
  std::string name;          // from USERINFO
  std::string challenge;
  QtvAuth method = QtvAuth::none;
  MultiviewRecorder rec;
  std::vector<QtvViewer> viewers;  // from qul
  int user_bytes = 0;              // the viewers' names held, bytes
  int64_t sent = 0;
  bool gone = false;
  double gone_at = -1;  // the server's time it was let go
};

struct QtvCounts {
  int64_t accepted = 0, refused = 0, streams = 0, dropped = 0, chat = 0, qul = 0;
};

struct QtvPort {
  QtvConfig cfg;
  gn::TcpListener listener;
  std::vector<std::unique_ptr<QtvRelay>> relays;
  int next_id = 1;
  ge::core::Rng rng{7};
  QtvCounts counts;
  std::vector<std::string> log;  // relays let in, refused and dropped, in order
};

// Listens at `bind` (port 0: any free one). False and `error` when the OS refuses.
bool qtv_open(QtvPort& p, const gn::Address& bind, const QtvConfig& cfg, std::string& error);
// After each server frame: new relays taken, handshakes answered, the frame's bytes sent, up
// messages run, relays that stalled dropped.
void qtv_update(QtvPort& p, ClassicServer& s);
// Only the new relays taken (`qtv_update` starts with this). A test or a lab holding both ends
// calls it first, so it can settle a new relay's request in before the update reads it.
void qtv_accept(QtvPort& p, const ClassicServer& s);
// Every relay ended with the end of the recording, and the port closed.
void qtv_close(QtvPort& p, ClassicServer& s);
int qtv_streaming(const QtvPort& p);

}  // namespace shooters_edge::net
