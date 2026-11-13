// The account service and an asker's half (account/service.hpp).
//
// ge:summary The account service's datagrams: the key, sealed asks and answers, the recent ring
// ge:keywords account service ask answer seal open key recent resend op
// ge:skill net
// ge:test tests/account_service_test.cpp
#include "account/service.hpp"

#include <cstring>

#include "net/wire.hpp"

namespace ge::account {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'A', 'C'};
constexpr uint8_t kKeyAsk = 1, kPub = 2, kAsk = 3, kAnswer = 4;
constexpr uint8_t kAskNonce = 1, kAnswerNonce = 2;

void nonce_of(uint8_t* nonce, uint8_t kind) { net::counter_nonce(nonce, 0, kind); }

void write_start(net::Writer& w, uint8_t kind) {
  net::write_bytes(w, kMagic, 4);
  net::write_u8(w, kind);
}

bool read_start(net::Reader& r, uint8_t& kind) {
  const uint8_t* m = net::read_view(r, 4);
  if (!m || std::memcmp(m, kMagic, 4) != 0) return false;
  kind = net::read_u8(r);
  return !r.bad;
}

void write_text(net::Writer& w, std::string_view s) {
  const size_t n = s.size() < 255 ? s.size() : 255;
  net::write_u8(w, static_cast<uint8_t>(n));
  net::write_bytes(w, s.data(), n);
}

std::string read_text(net::Reader& r) {
  const uint8_t n = net::read_u8(r);
  const uint8_t* p = net::read_view(r, n);
  return p ? std::string(reinterpret_cast<const char*>(p), n) : std::string();
}

net::Key fresh_key(Service& svc) {
  net::Key k{};
  svc.fresh(svc.fresh_ctx, k.data(), k.size());
  return k;
}

// The op's answer from the store.
Answer run(Service& svc, Op op, const std::vector<std::string>& f, const net::Address& from,
           int64_t now_us, Ticket* ticket) {
  Store& s = svc.store;
  const std::string at = net::to_string(from);
  Answer a;
  auto need = [&](size_t n) {
    if (f.size() == n) return true;
    a.refusal = Refusal::unknown;
    a.words = refusal_words(a.refusal, 0);
    return false;
  };
  switch (op) {
    case Op::create:
      if (need(2)) a = create(s, f[0], f[1], now_us, fresh_key(svc));
      break;
    case Op::sign_in:
      if (need(2)) a = sign_in(s, f[0], f[1], at, now_us);
      break;
    case Op::code:
      if (need(2)) {
        a = sign_in(s, f[0], f[1], at, now_us);
        if (a.refusal == Refusal::none) a = make_code(s, a.id, now_us, fresh_key(svc));
      }
      break;
    case Op::login:
      if (need(3)) a = login_game(s, f[0], f[1], at + ">" + f[2], now_us);
      break;
    case Op::recover:
      if (need(3)) a = recover(s, f[0], f[1], f[2], at, now_us, fresh_key(svc));
      break;
    case Op::ticket_code:
      if (need(1)) {
        Ticket t;
        const auto* bytes = reinterpret_cast<const uint8_t*>(f[0].data());
        const Account* acc = nullptr;
        if (ticket_read(bytes, f[0].size(), svc.keys.pub, now_us / 1000000, t) == TicketCheck::ok)
          acc = find(s, t.account);
        if (acc == nullptr) {
          a.refusal = Refusal::wrong;
          a.words = "That sign-in has run out. Sign in again.";
        } else if (acc->blocked) {
          a.refusal = Refusal::blocked;
          a.words = refusal_words(a.refusal, 0);
        } else {
          a = make_code(s, acc->id, now_us, fresh_key(svc));
        }
      }
      break;
    case Op::name:
      if (need(1)) {
        const auto it = s.keys.find(f[0]);
        const Account* acc = it != s.keys.end() ? find(s, it->second) : nullptr;
        a.id = acc != nullptr ? acc->id : 0;
      }
      break;
  }
  if (a.refusal == Refusal::none && (op == Op::sign_in || op == Op::login)) {
    const Account* acc = find(s, a.id);
    if (acc) {
      *ticket =
          ticket_issue(svc.keys, acc->id, acc->name, now_us / 1000000, svc.config.ticket_life_s);
      svc.stats.tickets++;
    }
  }
  return a;
}

const Recent* recent_of(const Service& svc, const net::Key& asker) {
  for (const Recent& r : svc.recent) {
    if (r.asker == asker) return &r;
  }
  return nullptr;
}

}  // namespace

Answer service_login(Service& svc, std::string_view name, std::string_view code,
                     std::string_view player, int64_t now_us, Ticket& ticket) {
  svc.stats.by_op[static_cast<size_t>(Op::login)]++;
  Answer a = login_game(svc.store, name, code, "self>" + std::string(player), now_us);
  if (a.refusal != Refusal::none) return a;
  const Account* acc = find(svc.store, a.id);
  if (acc) {
    ticket = ticket_issue(svc.keys, acc->id, acc->name, now_us / 1000000, svc.config.ticket_life_s);
    svc.stats.tickets++;
  }
  return a;
}

const char* op_name(Op op) {
  switch (op) {
    case Op::create:
      return "create";
    case Op::sign_in:
      return "sign-in";
    case Op::code:
      return "code";
    case Op::login:
      return "login";
    case Op::recover:
      return "recover";
    case Op::ticket_code:
      return "ticket-code";
    case Op::name:
      return "name";
  }
  return "?";
}

void service_init(Service& svc, const ServiceConfig& config, Store&& store,
                  net::Transport& transport, FreshFn fresh, void* fresh_ctx) {
  svc.config = config;
  svc.store = std::move(store);
  svc.keys = net::sign_keys(config.seed);
  svc.box_secret = net::box_secret(config.seed);
  svc.box_public = net::box_public(svc.box_secret);
  net::wipe(svc.config.seed.data(), svc.config.seed.size());  // the keys are all it was for
  svc.transport = &transport;
  svc.fresh = fresh;
  svc.fresh_ctx = fresh_ctx;
  svc.recent.clear();
  svc.recent_next = 0;
  svc.stats = {};
}

bool service_read(Service& svc, const uint8_t* data, size_t size, const net::Address& from,
                  int64_t now_us) {
  net::Reader r = net::make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind)) return false;
  net::Transport& t = *svc.transport;
  if (kind == kKeyAsk) {
    if (size < kAccountKeyAskSize) {
      svc.stats.refused++;
      return true;
    }
    svc.stats.key_asks++;
    uint8_t out[64];
    net::Writer w = net::make_writer(out, sizeof out);
    write_start(w, kPub);
    net::write_bytes(w, svc.keys.pub.data(), 32);
    net::transport_send(t, from, w.data, w.size, now_us);
    return true;
  }
  if (kind != kAsk) {
    svc.stats.refused++;
    return true;
  }
  if (size != kAccountAskSize) {  // an ask is padded to the biggest answer
    svc.stats.refused++;
    return true;
  }
  net::Key asker{};
  net::read_bytes(r, asker.data(), 32);
  const uint8_t* tag = net::read_view(r, net::kTagBytes);
  if (r.bad || !tag) {
    svc.stats.refused++;
    return true;
  }
  if (const Recent* again = recent_of(svc, asker)) {
    svc.stats.resent++;
    net::transport_send(t, from, again->bytes.data(), again->bytes.size(), now_us);
    return true;
  }
  const net::Key key = net::box_key(svc.box_secret, asker, asker, svc.box_public);
  uint8_t body[kAccountAskSize];
  const size_t body_size = size - r.at;
  std::memcpy(body, data + r.at, body_size);
  uint8_t nonce[net::kNonceBytes];
  nonce_of(nonce, kAskNonce);
  if (!net::open(body, body_size, tag, key, nonce, data, 5 + 32)) {
    svc.stats.refused++;
    return true;
  }
  net::Reader b = net::make_reader(body, body_size);
  const uint8_t op_byte = net::read_u8(b);
  const uint8_t count = net::read_u8(b);
  std::vector<std::string> fields;
  for (uint8_t i = 0; i < count && i < 8 && !b.bad; i++) fields.push_back(read_text(b));
  if (b.bad || op_byte < 1 || op_byte > 7 || count > 8) {
    svc.stats.refused++;
    return true;
  }
  const Op op = static_cast<Op>(op_byte);
  svc.stats.asks++;
  svc.stats.by_op[op_byte]++;
  Ticket ticket;
  const Answer a = run(svc, op, fields, from, now_us, &ticket);
  for (std::string& f : fields) net::wipe(f.data(), f.size());

  uint8_t out[kAccountAskSize];
  net::Writer w = net::make_writer(out, sizeof out);
  write_start(w, kAnswer);
  net::write_bytes(w, asker.data(), 32);
  const size_t tag_at = w.size;
  uint8_t zero_tag[net::kTagBytes] = {};
  net::write_bytes(w, zero_tag, sizeof zero_tag);
  const size_t sealed_at = w.size;
  net::write_u8(w, static_cast<uint8_t>(a.refusal));
  net::write_u64(w, static_cast<uint64_t>(a.wait_us));
  net::write_u64(w, a.id);
  write_text(w, a.code);
  write_text(w, a.words);
  uint8_t tbytes[kTicketMax];
  const size_t tn = ticket.account ? ticket_write(ticket, tbytes, sizeof tbytes) : 0;
  net::write_u16(w, static_cast<uint16_t>(tn));
  net::write_bytes(w, tbytes, tn);
  if (w.overflow) {
    svc.stats.refused++;
    return true;
  }
  nonce_of(nonce, kAnswerNonce);
  net::seal(out + sealed_at, w.size - sealed_at, out + tag_at, key, nonce, out, 5 + 32);
  net::transport_send(t, from, out, w.size, now_us);
  svc.stats.answers++;
  Recent rec{asker, std::vector<uint8_t>(out, out + w.size)};
  if (svc.recent.size() < svc.config.recent) {
    svc.recent.push_back(std::move(rec));
  } else if (!svc.recent.empty()) {
    svc.recent[svc.recent_next] = std::move(rec);
    svc.recent_next = (svc.recent_next + 1) % svc.recent.size();
  }
  return true;
}

void service_update(Service& svc, int64_t now_us) {
  net::Address from;
  int n = 0;
  while ((n = net::transport_receive(*svc.transport, svc.buffer, sizeof svc.buffer, from,
                                     now_us)) >= 0) {
    if (!service_read(svc, svc.buffer, static_cast<size_t>(n), from, now_us)) svc.stats.refused++;
  }
}

void ask_start(Ask& a, Op op, const net::Address& service, const net::Key& service_key,
               const std::vector<std::string>& fields, const net::Key& fresh, int64_t now_us) {
  a = Ask{};
  a.op = op;
  a.service = service;
  a.service_key = service_key;
  a.box_secret = fresh;
  a.box_public = net::box_public(fresh);
  const net::Key answerer = net::box_public_of(service_key);
  a.key = net::box_key(a.box_secret, answerer, a.box_public, answerer);
  a.next_us = now_us;
  a.datagram.assign(kAccountAskSize, 0);
  uint8_t* out = a.datagram.data();
  net::Writer w = net::make_writer(out, kAccountAskSize);
  write_start(w, kAsk);
  net::write_bytes(w, a.box_public.data(), 32);
  const size_t tag_at = w.size;
  w.size += net::kTagBytes;
  const size_t sealed_at = w.size;
  net::write_u8(w, static_cast<uint8_t>(op));
  net::write_u8(w, static_cast<uint8_t>(fields.size()));
  for (const std::string& f : fields) write_text(w, f);
  if (w.overflow) {  // fields too long for one ask: nothing is sent
    a.done = true;
    return;
  }
  uint8_t nonce[net::kNonceBytes];
  nonce_of(nonce, kAskNonce);
  // Sealed to the end: the padding is sealed too, so no byte of the ask can change unseen.
  net::seal(out + sealed_at, kAccountAskSize - sealed_at, out + tag_at, a.key, nonce, out, 5 + 32);
}

void ask_update(Ask& a, net::Transport& t, int64_t now_us) {
  if (a.done || now_us < a.next_us) return;
  if (a.tries >= a.max_tries) {
    a.done = true;
    return;
  }
  a.tries++;
  a.next_us = now_us + a.ask_us;
  net::transport_send(t, a.service, a.datagram.data(), a.datagram.size(), now_us);
}

bool ask_read(Ask& a, const uint8_t* data, size_t size, const net::Address& from) {
  if (a.done || from != a.service) return false;
  net::Reader r = net::make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind) || kind != kAnswer) return false;
  const uint8_t* asker = net::read_view(r, 32);
  const uint8_t* tag = net::read_view(r, net::kTagBytes);
  if (r.bad || std::memcmp(asker, a.box_public.data(), 32) != 0) return false;
  std::vector<uint8_t> body(data + r.at, data + size);
  uint8_t nonce[net::kNonceBytes];
  nonce_of(nonce, kAnswerNonce);
  if (!net::open(body.data(), body.size(), tag, a.key, nonce, data, 5 + 32)) return true;
  net::Reader b = net::make_reader(body.data(), body.size());
  Reply rep;
  rep.refusal = static_cast<Refusal>(net::read_u8(b));
  rep.wait_us = static_cast<int64_t>(net::read_u64(b));
  rep.account = net::read_u64(b);
  rep.code = read_text(b);
  rep.words = read_text(b);
  const uint16_t tn = net::read_u16(b);
  const uint8_t* tb = net::read_view(b, tn);
  if (b.bad) return true;
  rep.ticket.assign(tb, tb + tn);
  a.reply = std::move(rep);
  a.done = a.answered = true;
  net::wipe(a.box_secret.data(), a.box_secret.size());
  return true;
}

void key_ask_start(KeyAsk& k, const net::Address& service, int64_t now_us) {
  k = KeyAsk{};
  k.service = service;
  k.next_us = now_us;
}

void key_ask_update(KeyAsk& k, net::Transport& t, int64_t now_us) {
  if (k.done || now_us < k.next_us) return;
  if (k.tries >= k.max_tries) {
    k.done = true;
    return;
  }
  k.tries++;
  k.next_us = now_us + k.ask_us;
  uint8_t out[kAccountKeyAskSize] = {};
  net::Writer w = net::make_writer(out, sizeof out);
  write_start(w, kKeyAsk);
  net::transport_send(t, k.service, out, sizeof out, now_us);
}

bool key_ask_read(KeyAsk& k, const uint8_t* data, size_t size, const net::Address& from) {
  if (k.done || from != k.service) return false;
  net::Reader r = net::make_reader(data, size);
  uint8_t kind = 0;
  if (!read_start(r, kind) || kind != kPub) return false;
  net::read_bytes(r, k.key.data(), 32);
  if (r.bad) return true;
  k.done = k.answered = true;
  return true;
}

}  // namespace ge::account
