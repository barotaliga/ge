// THE SERVICE HOSTED: the account service on a real socket, its store and key on disk
// (docs/accounts.md § A server is its own master). A master's process runs one (the hub's
// `--accounts=<dir>`), and so does a game server with no master set. In `dir`:
//   accounts.log  the store: read at start, rewritten as its snapshot, then each change appended
//   account.key   the seed of the service's key pair, 64 hex digits; made at the first start from
//                 the operating system's randomness, readable by its owner alone, never in the tree
//   reports.log   the match reports game servers sent (matches.hpp's log), read back at start and
//                 each new one appended
// The servers whose reports count toward accounts' totals are the keys `trusted` names (the hub's
// `--trusted-servers=<file>`, read by `load_trusted`), and the service's own key: a game server
// that is its own master counts its own matches.
// A last line cut short (the process stopped mid-write) is dropped with a warning; any other line
// that does not read stops the start, naming it.
//
//   host_open(host, dir, bind, cost, unix_us, error, trusted)   every frame: host_update(host,
//   unix_us) load_trusted(path, keys, error)              a file of servers' keys, one per line, #
//   comments host_close(host)
//
// ge:summary The account service hosted: a UDP socket, the store's log and the key on disk
// ge:keywords account host accounts-log account-key key-file store-file service own-master
// ge:keywords host_open host_update sv_accountmaster serve-accounts
// ge:skill net
// ge:test tests/account_host_test.cpp
#pragma once
#include <cstdio>
#include <string>
#include <vector>

#include "account/service.hpp"

namespace ge::account {

struct Host {
  Service svc;
  net::Transport port;
  std::string dir;
  std::FILE* log = nullptr;
  std::FILE* reports = nullptr;  // reports.log
  std::string warning;           // what the start dropped, if anything ("" when nothing)
  bool open = false;
};

// The key in `path`, made there first when it is missing. False with `error` when it cannot be
// read or written.
bool load_or_make_key(const std::string& path, net::Key& out, bool& made, std::string& error);
// The store in `path` (none: a new one at `cost`), its log rewritten as the snapshot.
bool load_store(const std::string& path, Store& s, HashCost cost, int64_t unix_us,
                std::string& warning, std::string& error);

// Open the service at `bind` (UDP) over the store and key in `dir`.
bool host_open(Host& h, const std::string& dir, const net::Address& bind, HashCost cost,
               int64_t unix_us, std::string& error, const std::vector<net::Key>& trusted = {});
// The servers' keys in `path`: one per line, 64 hex digits, a `#` starting a comment. False with
// `error` naming the line that does not read.
bool load_trusted(const std::string& path, std::vector<net::Key>& out, std::string& error);
void host_update(Host& h, int64_t unix_us);
void host_close(Host& h);

}  // namespace ge::account
