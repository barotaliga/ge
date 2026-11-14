// THE MATCHMAKER'S OPS on the account service (service.hpp; docs/accounts.md § The matchmaker):
// a player asks with the ticket it holds from signing in; a server offers its seats signed by its
// own key, which the master's owner must trust (`--trusted-servers`), since every game the
// matchmaker makes is rated.
//
//   queue  ticket, verb (join, leave, look), game, mode, duo's name (two-on-two)
//          -> the words, and the queue's state in the answer's bytes
//   offer  the server's key (32 bytes), its signature (64) over offer_text(game, mode, port,
//          seats, time_s), game, mode, port, seats, time_s   -> ok
// An offer's address is the ask's own (its IP) with the port named, so a server offers only
// itself; its time must be within kOfferSkew_s of the master's, so a caught offer replays only
// briefly (and only offers seats the server had).
//
// THE ANSWER'S BYTES (little endian, a text is a length u8 and its bytes):
//   state u8 (0 not queued, 1 waiting, 2 matched), waited s u16, window in points u16, the
//   server's address, the mode, then your side's names and the other side's: each a count u8
//   and the names
//
//   matchmaker_run(svc, op, fields, from, now_s, out)    on the service
//   service_offer(svc, server, game, mode, address, seats, now_s)   in process (own master)
//   offer_text(...), queue_read(bytes, view)              the server's and the asker's sides
//
// ge:summary The matchmaker's ops: players queue by ticket, trusted servers offer seats, signed
// ge:keywords matchmaker-ops queue offer matchmaker_run queue_read offer_text signed-offer
// ge:skill net
// ge:test tests/account_service_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "account/matchmaker.hpp"
#include "account/store.hpp"
#include "net/address.hpp"

namespace ge::account {

struct Service;
enum class Op : uint8_t;

constexpr int64_t kOfferSkew_s = 60;

// What the offer's signature covers.
std::string offer_text(std::string_view game, std::string_view mode, uint16_t port, int seats,
                       int64_t time_s);

Answer matchmaker_run(Service& svc, Op op, const std::vector<std::string>& fields,
                      const net::Address& from, int64_t now_s, std::vector<uint8_t>& out);

// An offer taken in process, from a server that is its own master (its key need not be trusted:
// its own master's games are its own).
void service_offer(Service& svc, const net::Key& server, std::string_view game,
                   std::string_view mode, std::string_view address, int seats, int64_t now_s);

struct QueueView {
  LookState state = LookState::none;
  int64_t waited_s = 0, window_points = 0;
  std::string address, mode;
  std::vector<std::string> mine, theirs;
};
bool queue_read(const std::vector<uint8_t>& bytes, QueueView& view);

}  // namespace ge::account
