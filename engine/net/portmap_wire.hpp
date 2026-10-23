// THE PORT-MAPPING PROTOCOLS' MESSAGES, stated once for both sides: the client that asks a home
// router to open a port (portmap.hpp) and the simulated router that answers it (sim_router.hpp).
// Three protocols, each a router may speak:
//
//   NAT-PMP (RFC 6886)  UDP to the gateway's port 5351, version 0: the external address, a map
//   PCP (RFC 6887)      the same port, version 2: MAP with a nonce; a NAT-PMP-only router answers
//                       a PCP request with its own version and UNSUPP_VERSION, so a client falls
//                       back
//   UPnP IGD            SSDP's M-SEARCH (HTTP over UDP, multicast 239.255.255.250:1900) finds the
//                       router's description; HTTP and SOAP ask its WANIPConnection service
//
// Both NAT-PMP and PCP are big endian. A lifetime of 0 deletes a mapping. A simulated node is
// written as IPv4 in 198.18.0.0/15 (address.hpp), as it is in STUN. Internal: not installed.
//
// ge:summary The port-mapping messages: NAT-PMP, PCP MAP, SSDP and UPnP's SOAP, written and read
// ge:keywords net portmap port-mapping nat-pmp pcp upnp igd ssdp soap wanipconnection
// ge:keywords addportmapping deleteportmapping getexternalipaddress lifetime lease
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "net/address.hpp"

namespace ge::net::portmap_wire {

constexpr uint16_t kServerPort = 5351;  // NAT-PMP and PCP, on the gateway
constexpr uint16_t kSsdpPort = 1900;
constexpr size_t kMaxMessage = 1100;  // PCP's ceiling; NAT-PMP's are smaller

// ---- NAT-PMP --------------------------------------------------------------------------------

constexpr uint8_t kNatpmpVersion = 0;
constexpr uint8_t kOpExternal = 0, kOpMapUdp = 1;
enum NatpmpResult : uint8_t {  // carried in 16 bits
  kNatpmpOk = 0,
  kNatpmpUnsupportedVersion = 1,
  kNatpmpNotAuthorized = 2,
  kNatpmpNetworkFailure = 3,
  kNatpmpNoResources = 4,
  kNatpmpUnsupportedOpcode = 5,
};

struct NatpmpMap {
  uint16_t internal_port = 0, external_port = 0;
  uint32_t lifetime_s = 0;
};

struct NatpmpAnswer {
  uint8_t op = 0;  // the request's opcode (the answer's less 128)
  uint16_t result = 0;
  uint32_t epoch_s = 0;
  uint8_t ip[4] = {};  // the external address's answer
  NatpmpMap map;       // the map's answer
};

size_t natpmp_external_request(uint8_t out[2]);
size_t natpmp_map_request(uint8_t out[12], const NatpmpMap& m);
// A request read: its opcode and, for a map, its fields; false when it is not one.
bool natpmp_read_request(const uint8_t* data, size_t size, uint8_t& op, NatpmpMap& m);
size_t natpmp_answer(uint8_t out[16], const NatpmpAnswer& a);
bool natpmp_read_answer(const uint8_t* data, size_t size, NatpmpAnswer& a);

// ---- PCP ------------------------------------------------------------------------------------

constexpr uint8_t kPcpVersion = 2;
constexpr uint8_t kPcpMap = 1;
constexpr uint8_t kProtoUdp = 17;
constexpr size_t kPcpMapSize = 60;  // the header's 24 and MAP's 36, request and answer alike
enum PcpResult : uint8_t {
  kPcpOk = 0,
  kPcpUnsupportedVersion = 1,
  kPcpNotAuthorized = 2,
  kPcpMalformed = 3,
  kPcpUnsupportedOpcode = 4,
  kPcpNoResources = 8,
  kPcpUnsupportedProtocol = 9,
  kPcpCannotProvideExternal = 11,
  kPcpAddressMismatch = 12,
};

struct PcpMap {
  bool answer = false;
  uint8_t result = 0;  // an answer's
  uint32_t lifetime_s = 0;
  uint32_t epoch_s = 0;       // an answer's
  uint8_t client_ip[4] = {};  // a request's: the client's own address as it knows it
  uint8_t nonce[12] = {};
  uint8_t protocol = kProtoUdp;
  uint16_t internal_port = 0, external_port = 0;
  uint8_t external_ip[4] = {};  // suggested in a request, assigned in an answer
};

size_t pcp_map_write(uint8_t out[kPcpMapSize], const PcpMap& m);
// A MAP request or answer read; false when it is neither (a short one, another opcode or version).
bool pcp_map_read(const uint8_t* data, size_t size, PcpMap& m);
// The version byte, the first a router reads: 0 for NAT-PMP, 2 for PCP.
inline int message_version(const uint8_t* data, size_t size) { return size ? data[0] : -1; }

// ---- SSDP and UPnP IGD ----------------------------------------------------------------------

constexpr std::string_view kIgdDevice = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
// The services a mapping is asked of, in the order a client tries them in a description.
constexpr std::string_view kIgdServices[] = {
    "urn:schemas-upnp-org:service:WANIPConnection:2",
    "urn:schemas-upnp-org:service:WANIPConnection:1",
    "urn:schemas-upnp-org:service:WANPPPConnection:1",
};
constexpr int kUpnpConflict = 718;       // ConflictInMappingEntry
constexpr int kUpnpPermanentOnly = 725;  // OnlyPermanentLeasesSupported: ask again with 0
constexpr int kUpnpNoSuchEntry = 714;    // NoSuchEntryInArray (a delete of nothing)

// SSDP's search for an internet gateway (HTTP over UDP).
std::string ssdp_search(int mx_s = 1);
bool is_ssdp_search(std::string_view text);
// A search's answer naming the description's URL; its LOCATION read back.
std::string ssdp_answer(std::string_view location);
bool ssdp_location(std::string_view text, std::string& location);

// "http://<ipv4>:<port>/<path>": the host (a 198.18.0.0/15 one is the simulated node) and path.
bool parse_http_url(std::string_view url, Address& host, std::string& path);
std::string http_url(const Address& host, std::string_view path);

// The value of an HTTP header, case-insensitive; empty when absent.
std::string_view http_header(std::string_view message, std::string_view name);
// An HTTP answer's status (200, 500, ...) and body; false when it is not whole yet. Whole means
// the headers and Content-Length's bytes, or, without one, everything once `closed`.
bool http_answer_whole(std::string_view message, bool closed, int& status, std::string_view& body);
std::string http_get(const Address& host, std::string_view path);
std::string http_answer(int status, std::string_view content_type, std::string_view body);

// The control URL of the first service in kIgdServices a description lists; its type.
bool igd_control(std::string_view description, std::string& service, std::string& control_path);

// The SOAP calls a client makes and the answers a router gives.
struct UpnpAdd {
  uint16_t external_port = 0, internal_port = 0;
  uint8_t internal_ip[4] = {};
  uint32_t lease_s = 0;  // 0: permanent
  std::string description;
};
std::string soap_add(const Address& host, std::string_view path, std::string_view service,
                     const UpnpAdd& a);
std::string soap_delete(const Address& host, std::string_view path, std::string_view service,
                        uint16_t external_port);
std::string soap_external_ip(const Address& host, std::string_view path, std::string_view service);
// The action a SOAP request names ("AddPortMapping", ...), from its SOAPAction header.
std::string_view soap_action(std::string_view request);
// An element's text anywhere in an XML body, whatever its namespace prefix; empty when absent.
std::string_view xml_value(std::string_view xml, std::string_view name);
std::string soap_answer(std::string_view service, std::string_view action,
                        std::string_view inner_xml);
std::string soap_fault(int code, std::string_view description);
// A fault's UPnP error code; 0 when the body carries none.
int soap_error(std::string_view body);

// Dotted IPv4 text and back.
std::string ipv4_text(const uint8_t ip[4]);
bool parse_ipv4(std::string_view text, uint8_t ip[4]);

}  // namespace ge::net::portmap_wire
