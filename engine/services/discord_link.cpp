// The Discord link over the OS: the local pipe (net/local_pipe.hpp) at the first of the paths a
// Discord client listens on that answers.
//
// ge:summary Discord's link over the OS's local pipe, and where a Discord client listens
// ge:keywords services discord ipc local-pipe socket path xdg-runtime-dir flatpak snap named-pipe
// ge:keywords ge-discord-ipc
// ge:skill services
// ge:test tests/services_discord_test.cpp
#include <cstdlib>

#include "net/local_pipe.hpp"
#include "services/discord.hpp"

namespace ge::services {

namespace {

class OsLink final : public DiscordLink {
 public:
  explicit OsLink(std::vector<std::string> paths) : paths_(std::move(paths)) {}
  ~OsLink() override { close(); }

  bool open(std::string* error) override {
    close();
    std::string last;
    for (const std::string& path : paths_)
      if (net::local_connect(pipe_, path, &last)) return true;
    if (error)
      *error = "discord: no client answered at " + std::to_string(paths_.size()) +
               " places (last: " + last + "); next: start Discord, or set GE_DISCORD_IPC";
    return false;
  }

  bool exchange(int64_t /*now_us*/, const std::string& out, std::string& in) override {
    if (pipe_.closed) return false;
    if (!out.empty()) net::local_send(pipe_, out);
    net::local_pump(pipe_);
    in += pipe_.in;
    pipe_.in.clear();
    return !pipe_.closed;
  }

  void close() override { net::local_close(pipe_); }

  int64_t process_id() const override { return net::local_process_id(); }

 private:
  std::vector<std::string> paths_;
  net::LocalPipe pipe_;
};

}  // namespace

std::vector<std::string> discord_ipc_paths() {
  std::vector<std::string> out;
  if (const char* only = std::getenv("GE_DISCORD_IPC"); only && *only) {
    out.emplace_back(only);
    return out;
  }
#if defined(_WIN32)
  for (int n = 0; n < 10; n++)
    out.push_back(net::local_pipe_path({}, "discord-ipc-" + std::to_string(n)));
#else
  std::vector<std::string> dirs;
  for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"})
    if (const char* v = std::getenv(var); v && *v) dirs.emplace_back(v);
  dirs.emplace_back("/tmp");
  for (int n = 0; n < 10; n++)
    for (const std::string& dir : dirs)
      for (const char* sub :
           {"", "app/com.discordapp.Discord", "snap.discord", "snap.discord-canary"})
        out.push_back(net::local_pipe_path(*sub ? net::local_pipe_path(dir, sub) : dir,
                                           "discord-ipc-" + std::to_string(n)));
#endif
  return out;
}

std::unique_ptr<DiscordLink> discord_os_link(std::vector<std::string> paths) {
  return std::make_unique<OsLink>(std::move(paths));
}

}  // namespace ge::services
