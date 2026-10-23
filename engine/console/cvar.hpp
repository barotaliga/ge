// CONSOLE VARIABLES: every setting a player, a server's owner or a developer can change without a
// rebuild is a cvar, named and typed once here and set from a cfg file, the console, the command
// line or the server. A setting that only code may change is not a cvar: it stays a constant.
//
// NAMES are short, lower case and prefixed by who owns the setting (docs/console.md § Names):
// `sv_` the server's rules, `cl_` the client, `r_` the picture, `s_` sound, `in_` input, `m_` the
// mouse, `vid_` the window, `net_` the network's plumbing, `con_` the console, `hud_` the HUD,
// and a few bare classics (`name`, `rate`, `sensitivity`). Long words are abbreviated.
//
// THE THREE TIERS, by flag:
//   - a PLAYER'S setting carries `kArchive`: `cfg_save` writes it to the player's config, and any
//     cfg the player execs may change it;
//   - a SERVER'S setting carries `kServer`: only the server's own console or cfg (or a listen
//     host) changes it, and with `kServerInfo` every client gets its value in the server info and
//     follows it; a client's own `set` of it is refused with the reason;
//   - everything else a game reads stays code (a constant), never a cvar.
// `kCheat` cvars change only while the server's `sv_cheats` is 1; `kLatch` holds a new value
// until the game calls `apply_latched` (a new map, a restart); `kReadOnly` only code sets;
// `kUserInfo` cvars ride in the user info a client sends the server.
//
// BINDING: a cvar either owns its value or is BOUND to a field of a subsystem's settings struct
// (`bind`): a set parses, clamps and writes straight into the field, so the subsystem reads its
// own struct each frame with no lookup and no dependency on the console. `revision` counts the
// changes, for a reader that re-derives something only when its input moved.
//
// DETERMINISM: a value is text first; its number is read with `std::from_chars` (exact, the same
// on every platform) and a number written back as text with `core::num::to_string` (shortest
// round trip), so a cfg sets the same bits everywhere.
//
// ge:summary Console variables: named, typed, flagged settings, owned or bound to a struct field
// ge:keywords cvar cvars console-variable setting settings config cfg flag archive server-info
// ge:keywords user-info cheat latch read-only bind tier sv_ cl_ r_ info-string apply_info
// ge:keywords revision clamp sv_cheats
// ge:skill console
// ge:test tests/console_cvar_test.cpp
#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ge::console {

enum CvarFlag : uint8_t {
  kArchive = 1u << 0,     // a player's setting: cfg_save writes it (`seta`)
  kUserInfo = 1u << 1,    // sent to the server in the client's user info
  kServerInfo = 1u << 2,  // sent to every client in the server info
  kServer = 1u << 3,      // the server's: set only on the server (or a listen host)
  kCheat = 1u << 4,       // set only while sv_cheats is 1
  kLatch = 1u << 5,       // a new value waits for apply_latched (a new map, a restart)
  kReadOnly = 1u << 6,    // only code sets it (a version)
  kCreated = 1u << 7,     // made by `set` at run time: no code reads it, but it is kept
};

// How a value is read: text as it is; a number (clamped to the range); an integer (rounded
// half away from zero, then clamped); a boolean (0 or 1: "0", "" and "false" are 0).
enum class CvarKind : uint8_t { text, number, integer, boolean };

// Who is setting a value. Each tier refuses some (`Cvars::set`).
enum class Source : uint8_t {
  code,    // the game itself: always allowed
  local,   // this machine's console or a cfg it execs
  server,  // the server's info or a command it stuffed (on a client)
};

struct CvarSpec {
  std::string_view name = {};
  std::string_view fallback = {};  // the default, as text
  uint32_t flags = 0;
  std::string_view help = {};  // what it does, a phrase, with its unit
  CvarKind kind = CvarKind::number;
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
};

// The field a bound cvar writes, or none (the cvar owns its value).
using CvarTarget =
    std::variant<std::monostate, double*, float*, int32_t*, uint32_t*, bool*, std::string*>;

struct Cvar {
  std::string name;
  std::string value;     // as text, after clamping
  std::string fallback;  // the default
  std::string help;
  std::string latched;  // a value waiting for apply_latched; empty when none
  double number = 0;    // the value read as a number (0 when it is not one)
  uint32_t flags = 0;
  CvarKind kind = CvarKind::number;
  double min = 0, max = 0;
  uint32_t revision = 0;  // bumped by every change of value
  bool has_latched = false;
  CvarTarget target;

  bool on() const { return number != 0; }
  int32_t integer() const;
  bool is_default() const { return value == fallback; }
};

// What `Cvars::set` did.
enum class SetResult : uint8_t {
  changed,
  same,       // the value was already that
  latched,    // stored, waiting for apply_latched
  unknown,    // no cvar by that name (only `set` and `seta` create one)
  read_only,  // kReadOnly, from anything but code
  server,     // kServer, from a client's own console or cfg
  cheat,      // kCheat while sv_cheats is 0
  refused,    // the server may not set a player's own cvar
  bad_name,   // a name with a character outside [a-z0-9_+.-]
};

// One line saying why `r` refused `name`, with the next action; empty for changed, same and
// latched.
std::string set_result_text(SetResult r, std::string_view name);

// The registry. Addresses are stable: a `Cvar&` held at startup stays valid.
class Cvars {
 public:
  // Whether this registry is the server's (a dedicated server or a listen host): then kServer
  // cvars are set locally; on a client they follow the server info alone.
  explicit Cvars(bool is_server = true);

  // A cvar owning its value, at its fallback. Registering a name twice returns the first,
  // except a cvar `set` created at run time, which takes the spec (keeping the value set).
  Cvar& add(const CvarSpec& spec);
  // A cvar writing `field` on every change; the field is written with the fallback now.
  Cvar& bind(const CvarSpec& spec, double* field);
  Cvar& bind(const CvarSpec& spec, float* field);
  Cvar& bind(const CvarSpec& spec, int32_t* field);
  Cvar& bind(const CvarSpec& spec, uint32_t* field);
  Cvar& bind(const CvarSpec& spec, bool* field);
  Cvar& bind(const CvarSpec& spec, std::string* field);

  // By name, case-insensitive; null when none.
  Cvar* find(std::string_view name);
  const Cvar* find(std::string_view name) const;

  // Set by name from `source`. `create` (the `set` and `seta` commands) makes an unknown name a
  // kCreated cvar (with `extra_flags`, kArchive for seta) instead of refusing it.
  SetResult set(std::string_view name, std::string_view value, Source source, bool create = false,
                uint32_t extra_flags = 0);
  // The same on a cvar in hand.
  SetResult set(Cvar& cvar, std::string_view value, Source source);
  // Back to the fallback.
  SetResult reset(Cvar& cvar, Source source);

  // Every latched value takes effect; returns how many moved.
  int apply_latched();

  // `\key\value` pairs of every cvar carrying `flag` (kUserInfo or kServerInfo), sorted by name.
  std::string info_string(uint32_t flag) const;
  // Each pair of an info string set from `source`; unknown keys are created (kCreated plus
  // kServerInfo when the server sent them). Returns how many values moved.
  int apply_info(std::string_view info, Source source);

  bool is_server() const { return is_server_; }
  bool cheats() const;  // sv_cheats is on

  // Every cvar, sorted by name (a fresh vector: for listing, not for a frame).
  std::vector<const Cvar*> sorted() const;
  std::vector<Cvar*> sorted();
  size_t size() const { return all_.size(); }

 private:
  Cvar& make(const CvarSpec& spec, CvarTarget target);
  void store(Cvar& cvar, std::string_view text);
  std::vector<std::unique_ptr<Cvar>> all_;
  bool is_server_;
  Cvar* sv_cheats_ = nullptr;
};

// A valid cvar or command name: 1–63 characters of [a-z0-9_+.-], no leading digit.
bool valid_name(std::string_view name);
// `s` read as a number with std::from_chars (a leading '+' allowed, surrounding blanks ignored);
// false when it is not wholly one.
bool parse_number(std::string_view s, double& out);
// A number as a cvar writes it: an integer without a point, else the shortest round trip.
std::string number_text(double x);
// Lower case ASCII.
std::string lower(std::string_view s);

}  // namespace ge::console
