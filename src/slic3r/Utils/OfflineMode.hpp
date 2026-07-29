#ifndef slic3r_OfflineMode_hpp_
#define slic3r_OfflineMode_hpp_

#include <string>

namespace Slic3r {

// Offline mode: this build refuses to talk to anything outside the local
// network. It is on by default and can be turned off for a single run by
// setting BAMBU_ALLOW_INTERNET=1 in the environment.
//
// What it covers: every request made through Slic3r::Http (version and privacy
// checks, preset/profile sync, HMS messages, telemetry, plugin downloads) and
// every navigation of an embedded web view (Home page, MakerWorld, wiki, login).
//
// What it cannot cover: the closed-source networking plugin
// (libbambu_networking), which does its own HTTP/MQTT and is still loaded so
// that LAN-mode printer control keeps working. Block *.bambulab.com at the
// firewall if you need that half muzzled too.
bool offline_mode_enabled();

// True when the URL either uses a non-network scheme (file:, about:, data:,
// memory:, bbl:, ...) or points at a host on the local machine / local network.
bool url_is_local(const std::string &url);

// True for loopback, RFC1918 / RFC6598 / link-local addresses, IPv6 loopback,
// link-local and unique-local addresses, and for names that can only resolve
// locally (localhost, *.local, *.lan, *.home, *.internal, *.home.arpa).
bool host_is_local(const std::string &host);

// Host part of the URL, for log and error messages. Empty when there is none.
std::string url_host(const std::string &url);

} // namespace Slic3r

#endif // slic3r_OfflineMode_hpp_
