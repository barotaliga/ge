// The clock's macros (clock_macros.hpp).
//
// ge:summary $com_date, $com_time, $com_uptime and its long form, $random, from the host's clock
// ge:keywords add_clock_macros uptime_short uptime_long strftime time date timestamp dateiso rand
// ge:skill console
// ge:test tests/console_more_test.cpp
#include "console/clock_macros.hpp"

#include <cstdio>
#include <memory>

#include "core/prng.hpp"

namespace ge::console {

std::string level_clock(double seconds) {
  const long s = seconds > 0 ? static_cast<long>(seconds) : 0;
  char buf[48];
  if (s >= 3600)
    std::snprintf(buf, sizeof buf, "%ld:%ld:%02ld", s / 3600, s / 60 % 60, s % 60);
  else
    std::snprintf(buf, sizeof buf, "%ld:%02ld", s / 60, s % 60);
  return buf;
}

std::string tenths_clock(double seconds) {
  const long t = seconds > 0 ? static_cast<long>(seconds * 10 + 1e-6) : 0;
  char buf[48];
  std::snprintf(buf, sizeof buf, "%ld:%02ld.%ld", t / 600, t / 10 % 60, t % 10);
  return buf;
}

std::string uptime_short(double seconds) {
  const long s = seconds > 0 ? static_cast<long>(seconds) : 0;
  const long days = s / 86400, hours = s / 3600 % 24, mins = s / 60 % 60, secs = s % 60;
  char buf[48];
  if (days > 0)
    std::snprintf(buf, sizeof buf, "%ld+%ld:%02ld.%02ld", days, hours, mins, secs);
  else if (hours > 0)
    std::snprintf(buf, sizeof buf, "%ld:%02ld.%02ld", hours, mins, secs);
  else
    std::snprintf(buf, sizeof buf, "%02ld.%02ld", mins, secs);
  return buf;
}

std::string uptime_long(double seconds) {
  const long s = seconds > 0 ? static_cast<long>(seconds) : 0;
  const long parts[] = {s / 86400, s / 3600 % 24, s / 60 % 60, s % 60};
  const char* names[] = {"day", "hour", "min", "sec"};
  std::string out;
  for (int i = 0; i < 4; ++i) {
    if (parts[i] == 0) continue;
    if (!out.empty()) out += ", ";
    out += std::to_string(parts[i]) + " " + names[i] + (parts[i] == 1 ? "" : "s");
  }
  return out.empty() ? std::string("0 secs") : out;
}

void add_clock_macros(Console& con, Clock clock) {
  auto c = std::make_shared<Clock>(std::move(clock));
  con.keep(c);
  if (c->uptime_s) con.set_clock(c->uptime_s);  // a recipe's `sleep` counts on it too
  auto date = std::make_shared<std::string>();
  auto time = std::make_shared<std::string>();
  con.keep(date);
  con.keep(time);
  con.cvars().bind({.name = "com_date_format",
                    .fallback = "%Y-%m-%d",
                    .flags = kArchive,
                    .help = "How $com_date writes today's date.",
                    .kind = CvarKind::text,
                    .values = "the date letters: %Y year, %m month, %d day, %a %b names, ..."},
                   date.get());
  con.cvars().bind({.name = "com_time_format",
                    .fallback = "%H:%M",
                    .flags = kArchive,
                    .help = "How $com_time writes the time of day.",
                    .kind = CvarKind::text,
                    .values = "the time letters: %H hours, %M minutes, %S seconds, %p am/pm, ..."},
                   time.get());
  const auto stamp = [c](const std::shared_ptr<std::string>& format) {
    return [c, format] {
      if (!c->local || format->empty()) return std::string();
      const std::tm now = c->local();
      char buf[128];
      const size_t n = std::strftime(buf, sizeof buf, format->c_str(), &now);
      return std::string(buf, n);
    };
  };
  con.add_macro("com_date", stamp(date));
  con.add_macro("com_time", stamp(time));
  con.add_macro("com_uptime", [c] { return uptime_short(c->uptime_s ? c->uptime_s() : 0); });
  con.add_macro("com_uptime_long", [c] { return uptime_long(c->uptime_s ? c->uptime_s() : 0); });
  auto rng = std::make_shared<core::Rng>(core::make_rng(c->seed));
  con.keep(rng);
  con.add_macro("random", [rng] { return std::to_string(core::int_in(*rng, 0, 9)); });
  // The older clients' fixed forms: $time 14:05, $date 22.11.26, $timestamp 20261122-1405,
  // $dateiso 2026-11-22_14-05; $rand a number from 0 to 1 with six decimals.
  const auto fixed = [c](const char* format) {
    return [c, format] {
      if (!c->local) return std::string();
      const std::tm now = c->local();
      char buf[64];
      const size_t n = std::strftime(buf, sizeof buf, format, &now);
      return std::string(buf, n);
    };
  };
  con.add_macro("time", fixed("%H:%M"));
  con.add_macro("date", fixed("%d.%m.%y"));
  con.add_macro("timestamp", fixed("%Y%m%d-%H%M"));
  con.add_macro("dateiso", fixed("%Y-%m-%d_%H-%M"));
  con.add_macro("rand", [rng] {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6f", core::int_in(*rng, 0, 1000000) / 1e6);
    return std::string(buf);
  });
}

}  // namespace ge::console
