// The account service hosted on disk and a socket (account/host.hpp).
//
// ge:summary The account service hosted: the key file, the store's log, the socket
// ge:keywords account host key-file log append snapshot rename permissions
// ge:skill net
// ge:test tests/account_host_test.cpp
#include "account/host.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "account/store_text.hpp"
#include "net/token.hpp"

namespace ge::account {

namespace fs = std::filesystem;

namespace {

bool read_file(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream s;
  s << in.rdbuf();
  out = s.str();
  return true;
}

void os_fresh(void*, uint8_t* out, size_t size) { net::os_random_bytes(out, size); }

}  // namespace

bool load_or_make_key(const std::string& path, net::Key& out, bool& made, std::string& error) {
  made = false;
  std::string text;
  if (read_file(path, text)) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
      text.pop_back();
    }
    std::string why;
    if (!net::key_from_hex(text, out, why)) {
      error = path + ": " + why +
              "; next: restore the key file, or remove it to make a new key "
              "(every ticket signed with the old one is refused after)";
      return false;
    }
    return true;
  }
  net::os_random_bytes(out.data(), out.size());
  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
      error = "cannot write the account key " + path + "; next: make its folder writable";
      return false;
    }
    f << net::key_to_hex(out) << "\n";
  }
  std::error_code ec;
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                  ec);  // where the file system has no owner bits, the folder's own guard holds
  made = true;
  return true;
}

bool load_store(const std::string& path, Store& s, HashCost cost, int64_t unix_us,
                std::string& warning, std::string& error) {
  store_init(s, cost);
  std::string text;
  if (read_file(path, text)) {
    if (!text.empty() && text.back() != '\n') {
      const size_t cut = text.rfind('\n');
      const size_t keep = cut == std::string::npos ? 0 : cut + 1;
      warning = path + ": its last line was cut short and is dropped (" +
                std::to_string(text.size() - keep) + " bytes)";
      text.resize(keep);
    }
    if (!store_read(s, text, error)) {
      error = path + ": " + error;
      return false;
    }
  }
  s.cost = cost;
  s.log.clear();
  const std::string tmp = path + ".new";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
      error = "cannot write " + tmp + "; next: make its folder writable";
      return false;
    }
    f << store_text(s, unix_us);
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    error = "cannot replace " + path + ": " + ec.message() + "; next: make its folder writable";
    return false;
  }
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                  ec);
  return true;
}

bool host_open(Host& h, const std::string& dir, const net::Address& bind, HashCost cost,
               int64_t unix_us, std::string& error) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  h.dir = dir;
  const std::string key_path = (fs::path(dir) / "account.key").string();
  const std::string log_path = (fs::path(dir) / "accounts.log").string();
  ServiceConfig config;
  bool made = false;
  if (!load_or_make_key(key_path, config.seed, made, error)) return false;
  Store store;
  if (!load_store(log_path, store, cost, unix_us, h.warning, error)) return false;
  std::string why;
  if (!net::open_udp_transport(h.port, bind, &why)) {
    error = "cannot open the account service on " + net::to_string(bind) + ": " + why +
            "; next: pass another port";
    return false;
  }
  h.log = std::fopen(log_path.c_str(), "ab");
  if (!h.log) {
    net::close_transport(h.port);
    error = "cannot append to " + log_path + "; next: make it writable";
    return false;
  }
  service_init(h.svc, config, std::move(store), h.port, os_fresh, nullptr);
  net::wipe(config.seed.data(), config.seed.size());
  h.open = true;
  return true;
}

void host_update(Host& h, int64_t unix_us) {
  if (!h.open) return;
  service_update(h.svc, unix_us);
  if (!h.svc.store.log.empty()) {
    std::fwrite(h.svc.store.log.data(), 1, h.svc.store.log.size(), h.log);
    std::fflush(h.log);
    h.svc.store.log.clear();
  }
}

void host_close(Host& h) {
  if (!h.open) return;
  std::fclose(h.log);
  h.log = nullptr;
  net::close_transport(h.port);
  h.open = false;
}

}  // namespace ge::account
