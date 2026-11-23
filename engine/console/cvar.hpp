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
// `kCheat` cvars change only while the server's `sv_cheats` is 1 (turning it off puts each back to
// its default) and no joined game holds them off (`hold_cheats`); `kLatch` holds a new value until
// the game calls `apply_latched` (a new map, a restart); `kReadOnly` only code sets; `kUserInfo`
// cvars ride in the user info a client sends the server.
//
// BINDING: a cvar either owns its value or is BOUND to a field of a subsystem's settings struct
// (`bind`): a set parses, clamps and writes straight into the field, so the subsystem reads its
// own struct each frame with no lookup and no dependency on the console. `revision` counts the
// changes, for a reader that re-derives something only when its input moved.
//
// DETERMINISM: a value is text first; its number is read as plain decimal with `strtod` (correctly
// rounded by every platform's C library, in the C locale the engine never leaves) and a number
// written back as text with `core::num::to_string` (shortest round trip), so a cfg sets the
// same bits everywhere.
//
// SYNONYMS: another name for a setting (`add_synonym`): a game's own name for one of the engine's,
// such as a classic mod's `maxclients` for the server's `sv_maxclients`. A WHOLE synonym is the
// cvar under a second name: `find` answers the target, so a cfg line, the console and a program's
// reads and writes all reach it. A PART synonym is one word of a target that holds several ("N S
// T"): `set` by its name replaces that word and `synonym_value` reads it. Synonyms are not listed
// by `sorted`, so a saved cfg writes each setting once, under its own name.
//
// META-CVARS (`add_meta`, docs/servers.md § Meta-cvars): a cvar whose value names a LEVEL that
// sets a group of others, declared as data beside the feature that owns it (`sv_modern 1`: the
// network's modern settings). EXPLICIT BEATS IMPLIED: a member set from outside code (a cfg, the
// console, the command line, the server) keeps its value whatever order the two ran in; a level
// fills only what nothing set. Each cvar keeps `below`, the value the layers under every meta give
// it, so turning a meta off (a level that leaves a member out) RESTORES the member to it. A level
// may INCLUDE another meta's level by naming it as a member (`sv_modern 1`), its own level too; a
// loop of includes is refused when the meta is declared, naming the loop. A latched member waits
// for `apply_latched` like any set.
//
// LAYERS AND ONE NAME, ONE MEANING: below, beside `Overlap`.
//
// ge:summary Console variables: named, typed, flagged settings, owned or bound to a struct field
// ge:keywords cvar cvars console-variable setting settings config cfg flag archive server-info
// ge:keywords user-info cheat latch read-only bind tier sv_ cl_ r_ info-string apply_info
// ge:keywords revision clamp sv_cheats set_watch watch synonym add_synonym other-name setting-alias
// ge:keywords meta-cvar meta add_meta level implied explicit restore include cycle meta_members
// ge:keywords cvar-layer layer drop_layer overlap clash same-name one-name-one-meaning own same
// ge:keywords game-scope per-game-cvar shadow set-aside retired
// ge:keywords lock unlock locked held-value clamp-range ruleset competition-lock
// ge:skill console
// ge:test tests/console_cvar_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
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

// LAYERS AND ONE NAME, ONE MEANING (docs/console.md § Layers): a program registers its cvars in
// LAYERS, the engine's first, then a genre plugin's, then the game's (`CvarLayer`, a name such as
// "engine", "shooter" or "game:q2"); each cvar keeps the layer that registered it. A name
// meeting one an earlier layer registered says what it is (`CvarSpec::overlap`):
//   - `same`: the same setting (same effect, units and direction): the later layer's default,
//     help, values and range replace the earlier's, its field is written too, and a value the
//     player set is kept;
//   - `own`: the game's own setting under that name (another meaning): the earlier one is set
//     aside, unreachable while the layer lasts, and the name means the game's;
//   - `none` (the default): a CLASH. Nothing is merged: the earlier cvar stays, the later field is
//     never written, and the clash is recorded (`clashes`, `clash_text`) for a test to fail on.
// `drop_layer` takes a layer away (another game loaded): its own cvars are retired (hidden, their
// addresses kept; registering the name again revives it) and what it set aside or replaced comes
// back with its value. The same layer registering a name twice gets the first, as before.

// How a cvar meets a name an earlier layer registered (LAYERS).
enum class Overlap : uint8_t {
  none,  // a name of its own: meeting an earlier layer's is a clash, recorded, nothing merged
  same,  // the same setting: this layer's default, help, values and range; both fields written
  own,   // this layer's own meaning: the earlier setting is set aside while the layer lasts
};

struct CvarSpec {
  std::string_view name = {};
  std::string_view fallback = {};  // the default, as text
  uint32_t flags = 0;
  std::string_view help = {};  // what it does: one sentence (console/words.hpp)
  CvarKind kind = CvarKind::number;
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
  std::string_view values = {};     // what each value does (console/words.hpp)
  Overlap overlap = Overlap::none;  // meeting an earlier layer's cvar of the name (LAYERS above)
};

// The field a bound cvar writes, or none (the cvar owns its value).
using CvarTarget =
    std::variant<std::monostate, double*, float*, int32_t*, uint32_t*, bool*, std::string*>;

// LOCKS (`lock`, `unlock`; console/ruleset.hpp builds rulesets on them): an owner (a competition
// ruleset, a game in progress) HOLDS a cvar at one value or KEEPS it inside a range while the lock
// lasts. A set from any source while it holds is refused (`SetResult::locked`, the lock's own
// words say why), and a set into a range is clamped into it; either way the value asked for is
// remembered as the player's, so `unlock` gives it back and `cfg_save` writes it, never the
// locked one. Locking a name no layer registers yet does nothing (the owner locks again when the
// game's settings arrive).
// A LOCK on a cvar (LOCKS above): held at `value` when `fixed`, else kept inside [min, max].
struct CvarLock {
  std::string owner;  // who holds it ("ruleset smackdown"): `unlock` takes an owner's locks away
  std::string why;    // the refusal a player reads ("cl_hud is locked by the smackdown ruleset.")
  bool fixed = true;
  std::string value;  // the held value, when fixed
  double min = -std::numeric_limits<double>::infinity();
  double max = std::numeric_limits<double>::infinity();
  std::string wanted;  // the player's own value, given back by `unlock` and saved by cfg_save
};

struct Cvar;
// A definition a later layer replaced (`same`) or set aside (`own`), kept to come back with it.
struct CvarEarlier {
  std::string layer;  // the layer that replaced it
  std::shared_ptr<Cvar> was;
  Overlap how = Overlap::own;  // `same`: one setting, its value the player's; `own`: set aside
};

struct Cvar {
  std::string name;
  std::string value;     // as text, after clamping
  std::string fallback;  // the default
  std::string help;
  std::string values;   // what each value does (console/words.hpp)
  std::string latched;  // a value waiting for apply_latched; empty when none
  double number = 0;    // the value read as a number (0 when it is not one)
  uint32_t flags = 0;
  CvarKind kind = CvarKind::number;
  double min = 0, max = 0;
  uint32_t revision = 0;  // bumped by every change of value
  bool has_latched = false;
  CvarTarget target;
  // META-CVARS (above): the value the layers under every meta give it; the meta whose level set
  // the value now ("" none); and whether anything but code and a meta has set it (explicit).
  std::string below;
  std::string implied_by;
  bool explicit_set = false;
  // LAYERS (above): who registered it, the fields a `same` layer added, what it replaced, and
  // whether its layer was dropped (hidden from every lookup until registered again).
  std::string layer;
  std::vector<CvarTarget> also;
  std::vector<CvarEarlier> earlier;
  bool retired = false;
  // SCOPES (below): a value per game scope over the common one ("" and empty when none), and the
  // common value while the loaded scope overrides it.
  std::map<std::string, std::string> scoped;
  std::string common;
  // LOCKS (above): the lock holding it, or null.
  std::shared_ptr<CvarLock> lock;

  bool on() const { return number != 0; }
  int32_t integer() const;
  bool is_default() const { return value == fallback; }
};

// Another name for a cvar (SYNONYMS above).
struct Synonym {
  std::string name;  // lower case
  Cvar* target = nullptr;
  int part = -1;      // -1: the whole value; else the index of the word it names
  std::string layer;  // the layer that named it: `drop_layer` takes it away (LAYERS above)
};

// One member of a meta-cvar's level: a cvar and the value the level gives it, or another meta's
// level included (the name of a meta and that level's value).
struct MetaSet {
  std::string name;
  std::string value;
};
struct MetaLevel {
  std::string value;  // the meta's value that picks this level ("1")
  std::vector<MetaSet> sets;
};
struct Meta {
  std::string name;
  std::vector<MetaLevel> levels;
};

// A name two layers registered without saying how they meet (LAYERS above).
struct CvarClash {
  std::string name;
  std::string earlier_layer, layer;  // who registered it first, and who met it
  std::string earlier_help, help;
  std::string why;  // what kept them apart ("no overlap declared", "another kind")
};
// Every field `c` writes (its own and a `same` layer's), from its value.
void write_fields(const Cvar& c);

// One line naming the clash and the next action.
std::string clash_text(const CvarClash& c);

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
  locked,     // held by a lock, or kept inside its range (LOCKS above): its `why` says by whom
};

// One line saying why `r` refused `name`, with the next action; empty for changed, same and
// latched.
std::string set_result_text(SetResult r, std::string_view name);
// The same for a cvar in hand: a locked one answers its lock's own words.
std::string set_result_text(SetResult r, const Cvar& cvar);

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
  // A cvar `set` made (kCreated) taken away, as if never set; false when `name` is none or code
  // registered it.
  bool remove_created(std::string_view name);

  // LOCKS (above): `name` held or kept by `lock` (its `wanted` is filled from the value now). A
  // second lock replaces the first, keeping the player's value. False when no live cvar has the
  // name.
  bool lock(std::string_view name, CvarLock lock);
  // Every lock `owner` holds taken away, each cvar back to the player's own value; returns how
  // many.
  int unlock(std::string_view owner);

  // Every latched value takes effect; returns how many moved.
  int apply_latched();

  // `\key\value` pairs of every cvar carrying `flag` (kUserInfo or kServerInfo), sorted by name.
  std::string info_string(uint32_t flag) const;
  // Each pair of an info string set from `source`; unknown keys are created (kCreated plus
  // kServerInfo when the server sent them). Returns how many values moved.
  int apply_info(std::string_view info, Source source);

  // A host's watch on every set from outside code that the tier allowed (changed, the same value
  // again, or latched), with the value as given: a server keeps where each of its values came from
  // (engine/server/config.hpp). One watch; null removes it.
  using Watch = std::function<void(const Cvar&, std::string_view value, Source source)>;
  void set_watch(Watch watch) { watch_ = std::move(watch); }

  bool is_server() const { return is_server_; }
  bool cheats() const;  // sv_cheats is on, and no game holds cheats off (`hold_cheats`)
  // A game the client joined whose rules allow no cheats (Quake II's with more than one player
  // slot): `held` true puts every cheat back to its default and refuses each set of one, whatever
  // the local sv_cheats says, until it is let go (false: the game left).
  void hold_cheats(bool held);
  bool cheats_held() const { return cheats_held_; }

  // `name` as another name for the cvar `target` (`part` -1: its whole value; k: its word k).
  // False when `name` is not a valid name or is a cvar of its own, when `target` is unknown, or
  // when `name` already names something else; true again for the same synonym.
  bool add_synonym(std::string_view name, std::string_view target, int part = -1);
  // The synonym `name` (whole or part), or null.
  const Synonym* synonym(std::string_view name) const;
  // A synonym's value: the target's, or its word `part` ("" past its last word).
  std::string synonym_value(const Synonym& s) const;
  const std::vector<Synonym>& synonyms() const { return synonyms_; }

  // `name` as a meta-cvar with these levels (META-CVARS above): the cvar is made (text, "0",
  // kCreated) when no one registered it, and its current level applied now. Declaring a name
  // again replaces its levels. False, with the loop named in `error`, when a level includes
  // itself through others, or names a level another meta does not have; nothing changes then.
  bool add_meta(std::string_view name, std::vector<MetaLevel> levels, std::string& error);
  // The meta-cvar `name`, or null.
  const Meta* meta(std::string_view name) const;
  const std::vector<Meta>& metas() const { return metas_; }
  // What level `value` of `meta` sets, its includes opened in place, each cvar once (the last
  // word on it wins); empty for a level it does not have.
  std::vector<MetaSet> meta_members(const Meta& meta, std::string_view value) const;

  // LAYERS (above): the layer cvars registered from now on belong to ("engine" until another is
  // named).
  void set_layer(std::string_view layer);
  const std::string& layer() const { return layer_; }
  // `layer` taken away: its own cvars and synonyms retired, what it replaced or set aside back.
  // Returns how many cvars and synonyms it touched.
  int drop_layer(std::string_view layer);
  // Every clash met so far, in the order met.
  const std::vector<CvarClash>& clashes() const { return clashes_; }

  // SCOPES: a setting may hold a value per game scope over its common one (`<scope>:<name>`,
  // `game:<name>` for the loaded scope). The loaded scope's value is the live one; the common
  // value waits under it and comes back when another scope loads (`set_scope`).
  //   - `set_scoped` (or `set("<scope>:<name>", ...)`): the scope's own value;
  //   - while a config runs (`set_reading_config`): a game's spelling of a setting (a synonym)
  //     sets the loaded scope's value, and our name under a scope's value sets the common one,
  //     so `gamma 1` with `vid_gamma 0.9` gives 0.9 in that game in any order;
  //   - typed (no config): our name sets the common value and clears the loaded scope's.
  void set_scope(std::string_view scope);
  const std::string& scope() const { return scope_; }
  SetResult set_scoped(std::string_view scope, std::string_view name, std::string_view value,
                       Source source);
  // The scope's own value taken away (the common one comes back if it is loaded); false when it
  // had none.
  bool unset_scoped(std::string_view scope, std::string_view name);
  // `<scope>:<name>` split (`game` the loaded scope); false when `name` has no scope.
  bool split_scoped(std::string_view name, std::string& scope, std::string& base) const;
  void set_reading_config(bool on) { config_ = on; }
  bool reading_config() const { return config_; }

  // Every cvar, sorted by name (a fresh vector: for listing, not for a frame).
  std::vector<const Cvar*> sorted() const;
  std::vector<Cvar*> sorted();
  size_t size() const;  // the live cvars (a dropped layer's are not counted)

 private:
  Cvar& make(const CvarSpec& spec, CvarTarget target);
  Cvar& meet(Cvar& c, const CvarSpec& spec, CvarTarget target);   // an earlier layer's name
  void define(Cvar& c, const CvarSpec& spec, CvarTarget target);  // the spec, at its default
  Cvar* find_any(std::string_view name);                          // retired ones too
  std::string layer_ = "engine";
  std::vector<CvarClash> clashes_;
  std::string scope_;
  bool config_ = false;
  bool applying_ = false;  // a scope's value going live: not the player's common value
  bool locking_ = false;   // a lock putting its value in place (LOCKS)
  void store(Cvar& cvar, std::string_view text);
  void put(Cvar& cvar, std::string_view text);  // a meta's value: latched when the cvar is
  void apply_meta(const Meta& meta);
  std::vector<std::unique_ptr<Cvar>> all_;
  std::vector<Meta> metas_;
  int meta_depth_ = 0;
  std::vector<Synonym> synonyms_;
  bool is_server_;
  Cvar* sv_cheats_ = nullptr;
  bool cheats_held_ = false;
  Watch watch_;
};

// Registers into `layer` while it lives, then gives the registry back its layer before.
class CvarLayer {
 public:
  CvarLayer(Cvars& cvars, std::string_view layer) : cvars_(cvars), was_(cvars.layer()) {
    cvars.set_layer(layer);
  }
  ~CvarLayer() { cvars_.set_layer(was_); }
  CvarLayer(const CvarLayer&) = delete;
  CvarLayer& operator=(const CvarLayer&) = delete;

 private:
  Cvars& cvars_;
  std::string was_;
};

// A valid cvar or command name: 1–63 characters of [a-z0-9_+.-], no leading digit.
bool valid_name(std::string_view name);
// `s` read as a plain decimal number (a leading '+' allowed, surrounding blanks ignored; no hex,
// inf or nan); false when it is not wholly one.
bool parse_number(std::string_view s, double& out);
// A number as a cvar writes it: an integer without a point, else the shortest round trip.
std::string number_text(double x);
// Lower case ASCII.
std::string lower(std::string_view s);

}  // namespace ge::console
