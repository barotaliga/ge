// The port-mapping protocols' messages (net/portmap_wire.hpp).
//
// ge:summary NAT-PMP, PCP MAP, SSDP, HTTP and SOAP messages written and parsed for port mapping
// ge:keywords net portmap nat-pmp pcp upnp ssdp http soap xml big-endian parse
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#include "net/portmap_wire.hpp"

#include <cstring>

namespace ge::net::portmap_wire {

namespace {

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(v >> (24 - 8 * i));
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
         static_cast<uint32_t>(p[2]) << 8 | p[3];
}

// An IPv4 address as PCP carries every address: IPv4-mapped IPv6 (::ffff:a.b.c.d).
void put_mapped(uint8_t* p, const uint8_t ip[4]) {
  std::memset(p, 0, 10);
  p[10] = p[11] = 0xff;
  std::memcpy(p + 12, ip, 4);
}
bool get_mapped(const uint8_t* p, uint8_t ip[4]) {
  for (int i = 0; i < 10; i++) {
    if (p[i]) return false;
  }
  if (p[10] != 0xff || p[11] != 0xff) {
    // All zeros is PCP's "no suggestion".
    if (p[10] || p[11] || p[12] || p[13] || p[14] || p[15]) return false;
  }
  std::memcpy(ip, p + 12, 4);
  return true;
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool iequal(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

bool parse_uint(std::string_view s, uint32_t max, uint32_t& out) {
  if (s.empty() || s.size() > 10) return false;
  uint64_t v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<uint64_t>(c - '0');
  }
  if (v > max) return false;
  out = static_cast<uint32_t>(v);
  return true;
}

std::string host_text(const Address& host) {
  uint8_t ip[4] = {};
  if (host.family == Family::sim) {
    sim_to_ipv4(host, ip);
  } else if (host.family == Family::ipv4) {
    std::memcpy(ip, host.bytes, 4);
  }
  return ipv4_text(ip) + ":" + std::to_string(host.port);
}

std::string soap_post(const Address& host, std::string_view path, std::string_view service,
                      std::string_view action, std::string_view args) {
  std::string body =
      "<?xml version=\"1.0\"?>\r\n"
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:";
  body += action;
  body += " xmlns:u=\"";
  body += service;
  body += "\">";
  body += args;
  body += "</u:";
  body += action;
  body += "></s:Body></s:Envelope>\r\n";
  std::string out = "POST ";
  out += path;
  out += " HTTP/1.1\r\nHost: " + host_text(host) +
         "\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"";
  out += service;
  out += "#";
  out += action;
  out += "\"\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
  return out + body;
}

std::string arg(std::string_view name, std::string_view value) {
  std::string s = "<";
  s += name;
  s += ">";
  s += value;
  s += "</";
  s += name;
  s += ">";
  return s;
}

}  // namespace

// ---- NAT-PMP --------------------------------------------------------------------------------

size_t natpmp_external_request(uint8_t out[2]) {
  out[0] = kNatpmpVersion;
  out[1] = kOpExternal;
  return 2;
}

size_t natpmp_map_request(uint8_t out[12], const NatpmpMap& m) {
  out[0] = kNatpmpVersion;
  out[1] = kOpMapUdp;
  put16(out + 2, 0);
  put16(out + 4, m.internal_port);
  put16(out + 6, m.external_port);
  put32(out + 8, m.lifetime_s);
  return 12;
}

bool natpmp_read_request(const uint8_t* data, size_t size, uint8_t& op, NatpmpMap& m) {
  if (size < 2 || data[0] != kNatpmpVersion) return false;
  op = data[1];
  if (op == kOpExternal) return size == 2;
  if (op != kOpMapUdp || size != 12) return op != kOpMapUdp;  // another opcode: refused later
  m.internal_port = get16(data + 4);
  m.external_port = get16(data + 6);
  m.lifetime_s = get32(data + 8);
  return true;
}

size_t natpmp_answer(uint8_t out[16], const NatpmpAnswer& a) {
  std::memset(out, 0, 16);
  out[0] = kNatpmpVersion;
  out[1] = static_cast<uint8_t>(128 + a.op);
  put16(out + 2, a.result);
  put32(out + 4, a.epoch_s);
  if (a.op == kOpExternal) {
    if (a.result == kNatpmpOk) std::memcpy(out + 8, a.ip, 4);
    return 12;
  }
  if (a.op != kOpMapUdp) return 8;  // an opcode it does not know: the header alone
  if (a.result == kNatpmpOk) {
    put16(out + 8, a.map.internal_port);
    put16(out + 10, a.map.external_port);
    put32(out + 12, a.map.lifetime_s);
  }
  return 16;
}

bool natpmp_read_answer(const uint8_t* data, size_t size, NatpmpAnswer& a) {
  if (size < 8 || data[0] != kNatpmpVersion || data[1] < 128) return false;
  a = NatpmpAnswer{};
  a.op = static_cast<uint8_t>(data[1] - 128);
  a.result = get16(data + 2);
  a.epoch_s = get32(data + 4);
  if (a.result != kNatpmpOk) return true;
  if (a.op == kOpExternal) {
    if (size < 12) return false;
    std::memcpy(a.ip, data + 8, 4);
    return true;
  }
  if (a.op != kOpMapUdp || size < 16) return false;
  a.map.internal_port = get16(data + 8);
  a.map.external_port = get16(data + 10);
  a.map.lifetime_s = get32(data + 12);
  return true;
}

// ---- PCP ------------------------------------------------------------------------------------

size_t pcp_map_write(uint8_t out[kPcpMapSize], const PcpMap& m) {
  std::memset(out, 0, kPcpMapSize);
  out[0] = kPcpVersion;
  out[1] = static_cast<uint8_t>((m.answer ? 0x80 : 0) | kPcpMap);
  if (m.answer) out[3] = m.result;
  put32(out + 4, m.lifetime_s);
  if (m.answer) {
    put32(out + 8, m.epoch_s);
  } else {
    put_mapped(out + 8, m.client_ip);
  }
  uint8_t* p = out + 24;
  std::memcpy(p, m.nonce, 12);
  p[12] = m.protocol;
  put16(p + 16, m.internal_port);
  put16(p + 18, m.external_port);
  const uint8_t none[4] = {};
  if (std::memcmp(m.external_ip, none, 4) != 0) put_mapped(p + 20, m.external_ip);
  return kPcpMapSize;
}

bool pcp_map_read(const uint8_t* data, size_t size, PcpMap& m) {
  if (size < kPcpMapSize || size > kMaxMessage || (size & 3) || data[0] != kPcpVersion ||
      (data[1] & 0x7f) != kPcpMap)
    return false;
  m = PcpMap{};
  m.answer = (data[1] & 0x80) != 0;
  m.result = m.answer ? data[3] : 0;
  m.lifetime_s = get32(data + 4);
  if (m.answer) {
    m.epoch_s = get32(data + 8);
  } else if (!get_mapped(data + 8, m.client_ip)) {
    return false;
  }
  const uint8_t* p = data + 24;
  std::memcpy(m.nonce, p, 12);
  m.protocol = p[12];
  m.internal_port = get16(p + 16);
  m.external_port = get16(p + 18);
  return get_mapped(p + 20, m.external_ip);
}

// ---- SSDP and UPnP IGD ----------------------------------------------------------------------

std::string ssdp_search(int mx_s) {
  std::string s = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\n";
  s += "MX: " + std::to_string(mx_s) + "\r\nST: ";
  s += kIgdDevice;
  s += "\r\n\r\n";
  return s;
}

bool is_ssdp_search(std::string_view text) {
  return text.substr(0, 9) == "M-SEARCH " &&
         (http_header(text, "ST") == kIgdDevice || http_header(text, "ST") == "ssdp:all");
}

std::string ssdp_answer(std::string_view location) {
  std::string s = "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: ";
  s += kIgdDevice;
  s += "\r\nUSN: uuid:00000000-0000-0000-0000-000000000001::";
  s += kIgdDevice;
  s += "\r\nEXT:\r\nSERVER: ge/1 UPnP/1.1 ge-sim-router/1\r\nLOCATION: ";
  s += location;
  s += "\r\n\r\n";
  return s;
}

bool ssdp_location(std::string_view text, std::string& location) {
  if (text.substr(0, 12) != "HTTP/1.1 200" && text.substr(0, 12) != "HTTP/1.0 200") return false;
  const std::string_view v = http_header(text, "LOCATION");
  if (v.empty()) return false;
  location.assign(v);
  return true;
}

std::string ipv4_text(const uint8_t ip[4]) {
  return std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." + std::to_string(ip[2]) + "." +
         std::to_string(ip[3]);
}

bool parse_ipv4(std::string_view text, uint8_t ip[4]) {
  for (int i = 0; i < 4; i++) {
    const size_t dot = i < 3 ? text.find('.') : text.size();
    if (dot == std::string_view::npos) return false;
    uint32_t v = 0;
    if (!parse_uint(text.substr(0, dot), 255, v)) return false;
    ip[i] = static_cast<uint8_t>(v);
    text.remove_prefix(i < 3 ? dot + 1 : dot);
  }
  return text.empty();
}

bool parse_http_url(std::string_view url, Address& host, std::string& path) {
  if (url.substr(0, 7) != "http://") return false;
  url.remove_prefix(7);
  const size_t slash = url.find('/');
  const std::string_view authority = url.substr(0, slash);
  path = slash == std::string_view::npos ? "/" : std::string(url.substr(slash));
  const size_t colon = authority.find(':');
  uint32_t port = 80;
  if (colon != std::string_view::npos && !parse_uint(authority.substr(colon + 1), 65535, port))
    return false;
  uint8_t ip[4];
  if (!parse_ipv4(authority.substr(0, colon), ip)) return false;
  host = ipv4_or_sim(ip, static_cast<uint16_t>(port));
  return true;
}

std::string http_url(const Address& host, std::string_view path) {
  std::string s = "http://" + host_text(host);
  s += path;
  return s;
}

std::string_view http_header(std::string_view message, std::string_view name) {
  size_t at = message.find("\r\n");
  while (at != std::string_view::npos) {
    at += 2;
    const size_t end = message.find("\r\n", at);
    const std::string_view line = message.substr(at, end == std::string_view::npos ? 0 : end - at);
    if (line.empty()) return {};  // the headers' end
    const size_t colon = line.find(':');
    if (colon != std::string_view::npos && iequal(trim(line.substr(0, colon)), name))
      return trim(line.substr(colon + 1));
    at = end;
  }
  return {};
}

bool http_answer_whole(std::string_view message, bool closed, int& status, std::string_view& body) {
  const size_t end = message.find("\r\n\r\n");
  if (end == std::string_view::npos || message.substr(0, 5) != "HTTP/") return false;
  const size_t sp = message.find(' ');
  uint32_t code = 0;
  if (sp == std::string_view::npos || !parse_uint(message.substr(sp + 1, 3), 999, code))
    return false;
  status = static_cast<int>(code);
  body = message.substr(end + 4);
  const std::string_view length = http_header(message.substr(0, end + 2), "Content-Length");
  uint32_t n = 0;
  if (!length.empty() && parse_uint(length, 1u << 24, n)) {
    if (body.size() < n) return false;
    body = body.substr(0, n);
    return true;
  }
  return closed;
}

std::string http_get(const Address& host, std::string_view path) {
  std::string s = "GET ";
  s += path;
  s += " HTTP/1.1\r\nHost: " + host_text(host) + "\r\nConnection: close\r\n\r\n";
  return s;
}

std::string http_answer(int status, std::string_view content_type, std::string_view body) {
  std::string s = "HTTP/1.1 " + std::to_string(status) +
                  (status == 200 ? " OK" : " Internal Server Error") + "\r\nContent-Type: ";
  s += content_type;
  s += "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
  s += body;
  return s;
}

bool igd_control(std::string_view description, std::string& service, std::string& control_path) {
  for (std::string_view want : kIgdServices) {
    size_t at = 0;
    while ((at = description.find("<serviceType>", at)) != std::string_view::npos) {
      at += 13;
      const size_t end = description.find("</serviceType>", at);
      if (end == std::string_view::npos) break;
      if (trim(description.substr(at, end - at)) != want) continue;
      const size_t close = description.find("</service>", end);
      const std::string_view block = description.substr(end, close - end);
      const std::string_view url = xml_value(block, "controlURL");
      if (url.empty()) continue;
      service.assign(want);
      if (url.substr(0, 7) == "http://") {
        // A full URL: only its path is the control's (the host is the description's).
        Address ignored;
        if (!parse_http_url(url, ignored, control_path)) return false;
      } else {
        control_path.assign(url);
        if (control_path.empty() || control_path[0] != '/') control_path.insert(0, "/");
      }
      return true;
    }
  }
  return false;
}

std::string soap_add(const Address& host, std::string_view path, std::string_view service,
                     const UpnpAdd& a) {
  const std::string args =
      arg("NewRemoteHost", "") + arg("NewExternalPort", std::to_string(a.external_port)) +
      arg("NewProtocol", "UDP") + arg("NewInternalPort", std::to_string(a.internal_port)) +
      arg("NewInternalClient", ipv4_text(a.internal_ip)) + arg("NewEnabled", "1") +
      arg("NewPortMappingDescription", a.description) +
      arg("NewLeaseDuration", std::to_string(a.lease_s));
  return soap_post(host, path, service, "AddPortMapping", args);
}

std::string soap_delete(const Address& host, std::string_view path, std::string_view service,
                        uint16_t external_port) {
  const std::string args = arg("NewRemoteHost", "") +
                           arg("NewExternalPort", std::to_string(external_port)) +
                           arg("NewProtocol", "UDP");
  return soap_post(host, path, service, "DeletePortMapping", args);
}

std::string soap_external_ip(const Address& host, std::string_view path, std::string_view service) {
  return soap_post(host, path, service, "GetExternalIPAddress", "");
}

std::string_view soap_action(std::string_view request) {
  std::string_view v = http_header(request, "SOAPAction");
  if (!v.empty() && v.front() == '"') v.remove_prefix(1);
  if (!v.empty() && v.back() == '"') v.remove_suffix(1);
  const size_t hash = v.find('#');
  return hash == std::string_view::npos ? std::string_view{} : v.substr(hash + 1);
}

std::string_view xml_value(std::string_view xml, std::string_view name) {
  size_t at = 0;
  while ((at = xml.find(name, at)) != std::string_view::npos) {
    // An opening tag: `<name>` or `<prefix:name>`, the name followed by `>`.
    const size_t lt = xml.rfind('<', at);
    const bool opening =
        lt != std::string_view::npos && at + name.size() < xml.size() &&
        xml[at + name.size()] == '>' && xml[lt + 1] != '/' &&
        xml.substr(lt + 1, at - lt - 1).find_first_of("<> ") == std::string_view::npos;
    at += name.size();
    if (!opening) continue;
    const size_t start = at + 1;
    const size_t end = xml.find('<', start);
    if (end == std::string_view::npos) return {};
    return trim(xml.substr(start, end - start));
  }
  return {};
}

std::string soap_answer(std::string_view service, std::string_view action,
                        std::string_view inner_xml) {
  std::string body =
      "<?xml version=\"1.0\"?>\r\n<s:Envelope "
      "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:";
  body += action;
  body += "Response xmlns:u=\"";
  body += service;
  body += "\">";
  body += inner_xml;
  body += "</u:";
  body += action;
  body += "Response></s:Body></s:Envelope>\r\n";
  return http_answer(200, "text/xml; charset=\"utf-8\"", body);
}

std::string soap_fault(int code, std::string_view description) {
  std::string body =
      "<?xml version=\"1.0\"?>\r\n<s:Envelope "
      "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><s:Fault>"
      "<faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>"
      "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\"><errorCode>" +
      std::to_string(code) + "</errorCode><errorDescription>";
  body += description;
  body += "</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>\r\n";
  return http_answer(500, "text/xml; charset=\"utf-8\"", body);
}

int soap_error(std::string_view body) {
  uint32_t code = 0;
  return parse_uint(xml_value(body, "errorCode"), 9999, code) ? static_cast<int>(code) : 0;
}

}  // namespace ge::net::portmap_wire
