/*
 *  Copyright (c) 2016. Anyfi Inc.
 *
 *  All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Anyfi Inc. and its suppliers, if any.  The intellectual and technical concepts
 *  contained herein are proprietary to Anyfi Inc. and its suppliers and may be
 *  covered by U.S. and Foreign Patents, patents in process, and are protected
 *  by trade secret or copyright law.
 *
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from Anyfi Inc.
 */

#include <arpa/inet.h>
#include <net/if.h>
#include <list>
#include "Address.h"

#include "Common/Network/Buffer/Endian.h"
#include "Common/stdafx.h"
#include "Common/Uuid.h"

#if defined(ANDROID) || defined(__ANDROID__)
#include "Common/Network/ifaddrs-android.h"
#else
#include <ifaddrs.h>
#endif // #ifdef __ANDROID

bool Anyfi::Address::isEmpty() const {
    return (_addr.empty() && _port == 0);
}

bool Anyfi::Address::isP2PConnectionType() const {
    switch (_connectionType) {
        case ConnectionType::Proxy:
        case ConnectionType::VPN:
            return false;
        case ConnectionType::BluetoothP2P:
        case ConnectionType::WifiP2P:
            return true;
    }
    return false;
}

bool Anyfi::Address::isMeshProtocol() const {
    return (_connectionProtocol == ConnectionProtocol::MeshUDP || _connectionProtocol == ConnectionProtocol::MeshRUDP);
}

void Anyfi::Address::setIPv4Addr(uint32_t ipv4Addr) {
    unsigned short a, b, c, d;
    a = (ipv4Addr & (0xFF << 24)) >> 24;
    b = (ipv4Addr & (0xFF << 16)) >> 16;
    c = (ipv4Addr & (0xFF << 8)) >> 8;
    d = ipv4Addr & 0xFF;

    char strAddr[16];
    sprintf(strAddr, "%hu.%hu.%hu.%hu", a, b, c, d);
    _addr = strAddr;
}

uint32_t Anyfi::Address::getIPv4AddrAsNum() {
    unsigned short a, b, c, d;
    sscanf(_addr.c_str(), "%hu.%hu.%hu.%hu", &a, &b, &c, &d);

    return (a << 24) | (b << 16) | (c << 8) | d;
}

bool Anyfi::Address::getIPv4AddrAsRawBytes(uint8_t *rawAddress) {
    return _getIPv4AddrAsRawBytes(_addr.c_str(), rawAddress);
}
bool Anyfi::Address::_getIPv4AddrAsRawBytes(const char *src, uint8_t *rawAddress) {
//    https://github.com/lattera/glibc/blob/master/resolv/inet_pton.c
    int saw_digit, octets, ch;
    const unsigned short ipv4Size = 4;
    uint8_t tmp[4], *tp;
    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;
    while ((ch = *src++) != '\0') {
        if (ch >= '0' && ch <= '9') {
            auto newTp = (uint8_t)(*tp * 10 + (ch - '0'));
            if (saw_digit && *tp == 0)
                return false;
            if (newTp > 255)
                return false;
            *tp = newTp;
            if (! saw_digit) {
                if (++octets > 4)
                    return false;
                saw_digit = 1;
            }
        } else if (ch == '.' && saw_digit) {
            if (octets == 4)
                return false;
            *++tp = 0;
            saw_digit = 0;
        } else {
            return false;
        }
    }
    if (octets < 4)
        return false;
    memcpy(rawAddress, tmp, ipv4Size);
    return true;
}
bool Anyfi::Address::setIPv4AddrWithRawBytes(uint8_t *rawAddress) {
    char addr[16];
    if (_IPv4RawBytesToChar(rawAddress, addr, 15) == NULL) {
        return false;
    }
    _addr = std::string(addr);
    return true;
}
char *Anyfi::Address::_IPv4RawBytesToChar(const uint8_t *src, char *dst, size_t size) {
    char tmp[16];
    size_t len;

    tmp[0] = '\0';
    (void)snprintf(tmp, sizeof(tmp), "%d.%d.%d.%d",
                   ((int)((unsigned char)src[0])) & 0xff,
                   ((int)((unsigned char)src[1])) & 0xff,
                   ((int)((unsigned char)src[2])) & 0xff,
                   ((int)((unsigned char)src[3])) & 0xff);

    len = strlen(tmp);
    if(len == 0 || len >= size) {
        errno = ENOSPC;
        return nullptr;
    }
    strcpy(dst, tmp);
    return dst;
}

std::string Anyfi::Address::getIPv4Addr() const {
    // Get ip address from Address.addr() which can be host name.
//    struct hostent *he = gethostbyname(removeUrlPrefixIfExists(addr()).c_str());
//    if (he == nullptr) return "";
//
//    char ip[16] = {0};
//    strcpy(ip, inet_ntoa(*((struct in_addr *) he->h_addr_list[0])));
    return addr();
}

bool Anyfi::Address::setIPv6AddrWithRawBytes(const uint8_t *address) {
    // https://github.com/curl/curl/blob/master/lib/inet_ntop.c
    size_t size = sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255");
    const unsigned short ipv6sz = 16;
    const unsigned short int16sz = sizeof(uint16_t);
    char tmp[size];
    char *tp;
    struct {
        long base;
        long len;
    } best, cur;

    unsigned long words[ipv6sz / int16sz];
    int i;

    /* Preprocess:
     *  Copy the input (bytewise) array into a wordwise array.
     *  Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    memset(words, '\0', sizeof(words));
    for(i = 0; i < ipv6sz; i++)
        words[i/2] |= (address[i] << ((1 - (i % 2)) << 3));

    best.base = -1;
    cur.base  = -1;
    best.len = 0;
    cur.len = 0;

    for(i = 0; i < (ipv6sz / int16sz); i++) {
        if(words[i] == 0) {
            if(cur.base == -1)
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        }
        else if(cur.base != -1) {
            if(best.base == -1 || cur.len > best.len)
                best = cur;
            cur.base = -1;
        }
    }
    if((cur.base != -1) && (best.base == -1 || cur.len > best.len))
        best = cur;
    if(best.base != -1 && best.len < 2)
        best.base = -1;
    /* Format the result. */
    tp = tmp;
    for(i = 0; i < (ipv6sz / int16sz); i++) {
        /* Are we inside the best run of 0x00's? */
        if(best.base != -1 && i >= best.base && i < (best.base + best.len)) {
            if(i == best.base)
                *tp++ = ':';
            continue;
        }

        /* Are we following an initial run of 0x00s or any real hex?
         */
        if(i != 0)
            *tp++ = ':';

        /* Is this address an encapsulated IPv4?
         */
        if(i == 6 && best.base == 0 &&
           (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
            if(!_IPv4RawBytesToChar(address + 12, tp, sizeof(tmp) - (tp - tmp))) {
                errno = ENOSPC;
                return false;
            }
            tp += strlen(tp);
            break;
        }
        tp += snprintf(tp, 5, "%lx", words[i]);
    }

    /* Was it a trailing run of 0x00's?
     */
    if(best.base != -1 && (best.base + best.len) == (ipv6sz / int16sz))
        *tp++ = ':';
    *tp++ = '\0';

    /* Check for overflow, copy, and we're done.
     */
    if((size_t)(tp - tmp) > size) {
        errno = ENOSPC;
        return false;
    }
//    strcpy(dst, tmp);
    _addr = std::string(tmp);
    return true;
}

int Anyfi::Address::getIPv6AddrAsRawBytes(uint8_t *rawAddress) {
    return _getIPv6AddrAsRawBytes(_addr.c_str(), rawAddress);
}
int Anyfi::Address::_getIPv6AddrAsRawBytes(const char *src, uint8_t *dst) {
// https://github.com/lattera/glibc/blob/master/resolv/inet_pton.c
    const unsigned short NS_IN6ADDRSZ = 16;
    const unsigned short NS_INADDRSZ = 4;
    const unsigned short NS_INT16SZ = 2;
    static const char xdigits[] = "0123456789abcdef";
    uint8_t tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
    const char *curtok;
    int ch, saw_xdigit;
    u_int val;

    tp = (uint8_t *)memset(tmp, '\0', NS_IN6ADDRSZ);
    endp = tp + NS_IN6ADDRSZ;
    colonp = NULL;
    /* Leading :: requires some special handling. */
    if (*src == ':')
        if (*++src != ':')
            return (0);
    curtok = src;
    saw_xdigit = 0;
    val = 0;
    while ((ch = tolower (*src++)) != '\0') {
        const char *pch;

        pch = strchr(xdigits, ch);
        if (pch != NULL) {
            val <<= 4;
            val |= (pch - xdigits);
            if (val > 0xffff)
                return (0);
            saw_xdigit = 1;
            continue;
        }
        if (ch == ':') {
            curtok = src;
            if (!saw_xdigit) {
                if (colonp)
                    return (0);
                colonp = tp;
                continue;
            } else if (*src == '\0') {
                return (0);
            }
            if (tp + NS_INT16SZ > endp)
                return (0);
            *tp++ = (u_char) (val >> 8) & 0xff;
            *tp++ = (u_char) val & 0xff;
            saw_xdigit = 0;
            val = 0;
            continue;
        }
        if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
            _getIPv4AddrAsRawBytes(curtok, tp) > 0) {
            tp += NS_INADDRSZ;
            saw_xdigit = 0;
            break;	/* '\0' was seen by inet_pton4(). */
        }
        return (0);
    }
    if (saw_xdigit) {
        if (tp + NS_INT16SZ > endp)
            return (0);
        *tp++ = (u_char) (val >> 8) & 0xff;
        *tp++ = (u_char) val & 0xff;
    }
    if (colonp != NULL) {
        /*
         * Since some memmove()'s erroneously fail to handle
         * overlapping regions, we'll do the shift by hand.
         */
        const int n = tp - colonp;
        int i;

        if (tp == endp)
            return (0);
        for (i = 1; i <= n; i++) {
            endp[- i] = colonp[n - i];
            colonp[n - i] = 0;
        }
        tp = endp;
    }
    if (tp != endp)
        return (0);
    memcpy(dst, tmp, NS_IN6ADDRSZ);
    return (1);
}

bool Anyfi::Address::setIPv6AddrWithMACAddr(const std::string &macAddress) {
//    28:f0:76:37:02:c0
//    to
//    fe80::2af0:76ff:fe37:02c0
}


void Anyfi::Address::setMeshUID(UUID uid) {
    auto uidValue = uid.value();
    std::string addr;
    for (int i = 0; i < UUID_BYTES; i++) {
        addr += uidValue[i];
    }
    _addr = addr;
    _type = Anyfi::AddrType::MESH_UID;
}

Anyfi::UUID Anyfi::Address::getMeshUID() const {
    auto uid = Anyfi::UUID(_addr.c_str());
    return uid;
}

bool Anyfi::Address::operator==(const Anyfi::Address &other) const {
    if (this->type() != other.type()) return false;
    if (this->connectionProtocol() != other.connectionProtocol()) return false;
    if (this->addr() != other.addr()) return false;
    if (this->port() != other.port()) return false;
    return true;
}

bool Anyfi::Address::operator!=(const Anyfi::Address &other) const {
    if (this->type() != other.type()) return true;
    if (this->connectionProtocol() != other.connectionProtocol()) return true;
    if (this->addr() != other.addr()) return true;
    if (this->port() != other.port()) return true;
    return false;
}

std::string removeUrlPrefixIfExists(std::string url) {
    if (url.find("http://") == 0) {
        url = url.substr(7, url.size());
    }
    if (url.find("https://") == 0) {
        url = url.substr(8, url.size());
    }
    return url;
}

std::size_t Anyfi::Address::Hasher::operator()(const Anyfi::Address &k) const {
    return std::hash<std::string>()(k.addr());
}

bool Anyfi::Address::isIPv4Addr(std::string &address) {
    auto len = address.length();

    // Length should be more than 7(1.1.1.1) and less that 15 (255.255.255.255)
    if (len > 15 || len < 7)
        return false;

    unsigned char numCount = 0;
    unsigned char dotCount = 0;
    for (int i = 0; i < len; ++i) {
        if (address[i] >= '0' || address[i] <= '9') {
            if (++numCount > 3)
                return false;
        } else if (address[i] == '.') {
            dotCount++;
            numCount = 0;
        } else return false;
    }

    return dotCount == 3;
}

Anyfi::Address Anyfi::Address::getIPv6AddrFromInterface(const std::string &ifname) {
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;
    void *tmpAddrPtr = nullptr;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET6 && strcmp(ifa->ifa_name, ifname.c_str()) == 0) {
            tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
//            char addressBuffer[INET6_ADDRSTRLEN];
            // 2진 데이터 AddressBuffer를 문자열로 변환 필요.
//            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            auto addr = Anyfi::Address();
            addr.setIPv6AddrWithRawBytes((uint8_t*)tmpAddrPtr);
            addr.type(Anyfi::AddrType::IPv6);
            return addr;
//            Anyfi::Log::error(__func__, std::string(ifa->ifa_name)+" IP Address "+ addressBuffer);
        }else {
            // pass IPv4
            continue;
        }
    }
    return Anyfi::Address();
}

Anyfi::Address Anyfi::Address::getIPv4AddrFromInterface(const std::string &ifname) {
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;
    void *tmpAddrPtr = nullptr;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET || !(ifa->ifa_flags & (IFF_RUNNING)) || (ifa->ifa_flags & (IFF_LOOPBACK)) || strcmp(ifa->ifa_name, ifname.c_str()) != 0)
            continue;

        tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

        auto addr = Anyfi::Address();
        addr.addr(addressBuffer);
        addr.type(Anyfi::AddrType::IPv4);
        return addr;
    }
    return Anyfi::Address();
}

const std::string Anyfi::Address::getInterfaceNameWithIPv4Addr(const std::string& ip) {
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;

    void *tmpAddrPtr = nullptr;
    char ifipv4addr[INET_ADDRSTRLEN];

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, tmpAddrPtr, ifipv4addr, INET_ADDRSTRLEN);
        if (strcmp(ifipv4addr, ip.c_str()) != 0)
            continue;
        return ifa->ifa_name != nullptr ? std::string(ifa->ifa_name) : std::string();
    }
    return std::string();
}

const std::string Anyfi::Address::getInterfaceNameContainsIPv4Addr(const std::string& ip) {
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;

    void *tmpAddrPtr = nullptr;
    char ifipv4addr[INET_ADDRSTRLEN];

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (!(ifa->ifa_flags & (IFF_RUNNING)) || (ifa->ifa_flags & (IFF_LOOPBACK)) || (ifa->ifa_flags & (IFF_NOARP)))
            continue;

        tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, tmpAddrPtr, ifipv4addr, INET_ADDRSTRLEN);
        if (strstr(ifipv4addr, ip.c_str()) == NULL)
            continue;

        return ifa->ifa_name != nullptr ? std::string(ifa->ifa_name) : std::string();
    }
    return std::string();
}

const std::unordered_map<std::string, std::list<std::string>> Anyfi::Address::dumpInterfaces() {
    std::unordered_map<std::string, std::list<std::string>> result;

    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;
    void *tmpAddrPtr = nullptr;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (!(ifa->ifa_flags & (IFF_RUNNING)) || (ifa->ifa_flags & (IFF_LOOPBACK)))
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

            result[ifa->ifa_name].push_back(addressBuffer);
        } else {
            tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
            auto addr = Anyfi::Address();
            addr.setIPv6AddrWithRawBytes((uint8_t *) tmpAddrPtr);
            addr.type(Anyfi::AddrType::IPv6);

            result[ifa->ifa_name].push_back(addr.addr());
        }
    }
    return result;
}

std::string Anyfi::Address::toString(Anyfi::ConnectionType connType) {
    switch (connType) {
        case ConnectionType::WifiP2P:
            return "WifiP2P";
        case ConnectionType::BluetoothP2P:
            return "BluetoothP2P";
        case ConnectionType::Proxy:
            return "Proxy";
        case ConnectionType::VPN:
            return "VPN";
    }
    return "Invalid";
}

std::string Anyfi::Address::toString(Anyfi::ConnectionProtocol protocol) {
    switch (protocol) {
        case ConnectionProtocol::TCP:
            return "TCP";
        case ConnectionProtocol::UDP:
            return "UDP";
        case ConnectionProtocol::MeshUDP:
            return "MeshUDP";
        case ConnectionProtocol::MeshRUDP:
            return "MeshRUDP";
    }
    return "Invalid";
}

std::string Anyfi::Address::toString(Anyfi::AddrType addrType) {
    switch (addrType) {
        case AddrType::IPv4:
            return "IPv4";
        case AddrType::IPv6:
            return "IPv6";
        case AddrType::MESH_UID:
            return "MeshUID";
        case AddrType::BLUETOOTH_UID:
            return "BluetoothUID";
    }
    return "Invalid";
}

std::string Anyfi::Address::toString() const {
    std::string desc = "{ type:" + to_string(type());
    desc.append(", connType: " + to_string(static_cast<int>(connectionType())));
    desc.append(", connProtocol:" + to_string(connectionProtocol()));
    desc.append(", netType:" + to_string(networkType()));
    desc.append(", addr:'" + addr() + "'");
    desc.append(", port:" + to_string(port()));
    desc.append(" }");
    return desc;
}