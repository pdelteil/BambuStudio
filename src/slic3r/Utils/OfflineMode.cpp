#include "OfflineMode.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>

namespace Slic3r {

static std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool offline_mode_enabled()
{
    static const bool enabled = []() {
        const char *env = ::getenv("BAMBU_ALLOW_INTERNET");
        if (env == nullptr)
            return true;
        const std::string value = to_lower(env);
        return value.empty() || value == "0" || value == "false" || value == "no";
    }();
    return enabled;
}

// Schemes that actually put bytes on a socket. Anything else (file:, about:,
// data:, blob:, memory:, bbl:, wxfs:, mailto:, javascript:) is handled locally
// or handed to the desktop, so it is not our business here.
static bool is_network_scheme(const std::string &scheme)
{
    static const std::set<std::string> network_schemes = {
        "http", "https", "ftp", "ftps", "sftp", "tftp", "ws", "wss",
        "mqtt", "mqtts", "smtp", "smtps", "telnet", "rtsp", "rtsps",
    };
    return network_schemes.count(scheme) > 0;
}

std::string url_host(const std::string &url)
{
    const std::size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos)
        return std::string();

    std::string authority = url.substr(scheme_end + 3);
    authority             = authority.substr(0, authority.find_first_of("/?#"));

    // Drop any userinfo ("user:password@host").
    const std::size_t at = authority.rfind('@');
    if (at != std::string::npos)
        authority = authority.substr(at + 1);

    if (!authority.empty() && authority.front() == '[') {
        // Bracketed IPv6 literal, optionally followed by ":port".
        const std::size_t close = authority.find(']');
        if (close != std::string::npos)
            return authority.substr(1, close - 1);
        return authority.substr(1);
    }

    // Strip ":port" — only when what follows the colon really is a port, so a
    // bare (unbracketed) IPv6 literal survives intact.
    const std::size_t colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(':') == colon)
        authority = authority.substr(0, colon);

    return authority;
}

// Names that no public DNS server can answer.
static bool is_local_name(const std::string &host)
{
    static const char *local_suffixes[] = {
        ".local", ".localhost", ".localdomain", ".lan", ".home", ".home.arpa", ".internal", ".intranet",
    };

    if (host == "localhost")
        return true;
    for (const char *suffix : local_suffixes)
        if (boost::algorithm::ends_with(host, suffix))
            return true;
    return false;
}

static bool address_v4_is_local(const boost::asio::ip::address_v4::bytes_type &b)
{
    const uint32_t v4 = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
    return (v4 & 0xff000000u) == 0x7f000000u              // 127.0.0.0/8, loopback
           || (v4 & 0xff000000u) == 0x00000000u           // 0.0.0.0/8, "this network"
           || (v4 & 0xff000000u) == 0x0a000000u           // 10.0.0.0/8
           || (v4 & 0xfff00000u) == 0xac100000u           // 172.16.0.0/12
           || (v4 & 0xffff0000u) == 0xc0a80000u           // 192.168.0.0/16
           || (v4 & 0xffff0000u) == 0xa9fe0000u           // 169.254.0.0/16, link-local
           || (v4 & 0xffc00000u) == 0x64400000u;          // 100.64.0.0/10, RFC6598 (Tailscale et al.)
}

static bool address_is_local(const boost::asio::ip::address &address)
{
    if (address.is_v4())
        return address_v4_is_local(address.to_v4().to_bytes());

    const boost::asio::ip::address_v6 v6 = address.to_v6();
    const auto                        bytes = v6.to_bytes();
    if (v6.is_v4_mapped())
        return address_v4_is_local({bytes[12], bytes[13], bytes[14], bytes[15]});

    return v6.is_loopback()                        // ::1
           || v6.is_link_local()                   // fe80::/10
           || v6.is_unspecified()                  // ::
           || (bytes[0] & 0xfe) == 0xfc;           // fc00::/7, unique-local
}

bool host_is_local(const std::string &host)
{
    if (host.empty())
        return true;

    std::string name = to_lower(host);
    if (!name.empty() && name.back() == '.')    // fully qualified trailing dot
        name.pop_back();
    if (name.empty())
        return true;

    if (is_local_name(name))
        return true;

    boost::system::error_code ec;
    const boost::asio::ip::address address = boost::asio::ip::make_address(name, ec);
    if (!ec)
        return address_is_local(address);

    // A name we could not classify: it needs public DNS, so treat it as remote.
    return false;
}

bool url_is_local(const std::string &url)
{
    const std::size_t scheme_end = url.find(':');
    if (scheme_end == std::string::npos)
        return true;    // relative reference, resolved against an already-vetted base

    if (!is_network_scheme(to_lower(url.substr(0, scheme_end))))
        return true;

    return host_is_local(url_host(url));
}

} // namespace Slic3r
