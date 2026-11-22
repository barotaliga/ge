// Rulesets (ruleset.hpp): the `ruleset` setting, the book, the locks and gates under one owner.
//
// ge:summary Rulesets: choosing one, holding its locks and gates, the rulesets command
// ge:keywords ruleset rulesets lock gate book describe update set_connected
// ge:skill console
// ge:test tests/console_ruleset_test.cpp
#include "console/ruleset.hpp"

#include <limits>
#include <utility>

namespace ge::console {

namespace {

constexpr std::string_view kOwner = "ruleset";
constexpr std::string_view kConnectedOwner = "ruleset-connected";

std::string bound_text(double x) { return number_text(x); }

std::string lock_words(const RuleLock& l) {
  const std::string n(l.name);
  if (!l.value.empty()) return n + " " + std::string(l.value);
  constexpr double kInf = std::numeric_limits<double>::infinity();
  const bool low = l.min > -kInf, high = l.max < kInf;
  if (low && high) return n + " " + bound_text(l.min) + " to " + bound_text(l.max);
  if (high) return n + " at most " + bound_text(l.max);
  if (low) return n + " at least " + bound_text(l.min);
  return n;
}

}  // namespace

Rulesets::Rulesets(Console& console, std::function<bool()> match)
    : console_(console), match_(std::move(match)) {
  cvar_ = &console.cvars().add(
      {.name = "ruleset",
       .fallback = "default",
       .flags = kArchive,
       .help = "The competition rules your game keeps: settings held and commands refused, the "
               "same for every player who picks it.",
       .kind = CvarKind::text,
       .values = "default: no rules (the default); the rulesets command lists the others; "
                 "changed only while not connected to a server"});
  seen_revision_ = cvar_->revision;
  console.add_command("rulesets", "rulesets [name]: the rulesets you may pick, or what one holds",
                      [this](Console& c, const Args& a) {
                        if (a.size() >= 2) {
                          const Ruleset* r = find(a[1]);
                          if (r == nullptr) {
                            c.print("There is no ruleset called " + a[1] + ".");
                            return;
                          }
                          c.print(describe(*r));
                          return;
                        }
                        for (const Ruleset& r : book_)
                          c.print(std::string(&r == current_ ? "* " : "  ") + std::string(r.name) +
                                  ": " + std::string(r.title));
                      });
}

const Ruleset* Rulesets::find(std::string_view name) const {
  const std::string want = lower(name);
  for (const Ruleset& r : book_)
    if (r.name == want) return &r;
  return nullptr;
}

std::string Rulesets::describe(const Ruleset& r) const {
  std::string out = std::string(r.name) + ": " + std::string(r.title);
  if (r.locks.empty() && r.refused.empty()) return out + "\nNothing is held.";
  for (const RuleLock& l : r.locks) out += "\n  holds " + lock_words(l);
  for (const RuleRefusal& f : r.refused)
    out += "\n  refuses " + std::string(f.command) + (f.match_only ? " during a match" : "");
  return out;
}

void Rulesets::set_book(std::span<const Ruleset> book) {
  book_ = book;
  const Ruleset* r = find(cvar_->value);
  if (r == nullptr && !book_.empty()) r = &book_.front();
  current_ = nullptr;  // applied afresh: the new book's locks
  apply(r);
}

void Rulesets::lock_all() {
  if (current_ == nullptr) return;
  const std::string rname(current_->name);
  for (const RuleLock& l : current_->locks) {
    const Cvar* had = console_.cvars().find(l.name);
    if (had == nullptr) continue;
    if (had->lock && had->lock->owner == kOwner) continue;  // already held
    CvarLock lock{.owner = std::string(kOwner),
                  .why = had->name + " is locked by the " + rname + " ruleset.",
                  .fixed = !l.value.empty(),
                  .value = std::string(l.value),
                  .min = l.min,
                  .max = l.max,
                  .wanted = {}};
    console_.cvars().lock(l.name, std::move(lock));
  }
  seen_size_ = console_.cvars().size();
}

void Rulesets::apply(const Ruleset* r) {
  console_.cvars().unlock(kOwner);
  console_.ungate(kOwner);
  current_ = r;
  if (r == nullptr) return;
  lock_all();
  const std::string rname(r->name);
  for (const RuleRefusal& f : r->refused) {
    const std::string cmd(f.command);
    CommandGate gate;
    gate.owner = std::string(kOwner);
    if (f.match_only) {
      gate.why = cmd + " is not allowed during a match under the " + rname + " ruleset.";
      gate.when = [this] { return match_ && match_(); };
    } else {
      gate.why = cmd + " is not allowed under the " + rname + " ruleset.";
    }
    console_.gate_command(cmd, std::move(gate));
  }
  // The setting names the ruleset in force, as the book spells it.
  if (cvar_->value != r->name) {
    const bool held = cvar_->lock != nullptr;
    std::shared_ptr<CvarLock> keep = cvar_->lock;
    cvar_->lock.reset();
    console_.cvars().set(*cvar_, r->name, Source::code);
    cvar_->lock = keep;
    if (held) cvar_->lock->wanted = std::string(r->name);
  }
  seen_revision_ = cvar_->revision;
}

void Rulesets::update() {
  if (cvar_->revision != seen_revision_) {
    seen_revision_ = cvar_->revision;
    const Ruleset* r = find(cvar_->value);
    if (r == nullptr) {
      std::string names;
      for (const Ruleset& b : book_) names.append(names.empty() ? "" : ", ").append(b.name);
      console_.print("There is no ruleset called " + cvar_->value + ". The rulesets are " + names +
                     ".");
      apply(current_);  // the setting back to the one in force
      return;
    }
    if (r != current_) {
      apply(r);
      console_.print("Ruleset " + std::string(r->name) + ": " + std::string(r->title));
    }
  }
  if (console_.cvars().size() != seen_size_) lock_all();
}

void Rulesets::set_connected(bool on) {
  if (on == connected_) return;
  connected_ = on;
  if (!on) {
    // A choice typed while connected was refused, not kept for later.
    if (cvar_->lock && cvar_->lock->owner == kConnectedOwner) cvar_->lock->wanted = cvar_->value;
    console_.cvars().unlock(kConnectedOwner);
    return;
  }
  CvarLock lock;
  lock.owner = std::string(kConnectedOwner);
  lock.why = "The ruleset can only be changed while you are not connected to a server.";
  lock.value = cvar_->value;
  console_.cvars().lock(cvar_->name, std::move(lock));
}

}  // namespace ge::console
