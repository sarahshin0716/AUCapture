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

#ifndef ANYFIMESH_CORE_ADDRESS_H
#define ANYFIMESH_CORE_ADDRESS_H

#include <string>
#include <utility>
#include <cinttypes>
#include <list>

#include "../../Common/Uuid.h"


#ifndef _WIN32
#include <netinet/in.h>

#else
#ifndef AF_INET
#define AF_INET     2
#endif

#ifndef AF_INET6
#define AF_INET6    23 // POSIX : 30
#endif


#endif // #ifndef _WIN32

namespace Anyfi {

/**
 * Type of the connection protocol.
 */
enum ConnectionProtocol {
    /**
     * Transmission Control Protocol
     */
    TCP = 6,

    /**
     * User Data Protocol
     */
    UDP = 17,

    /**
     * Mesh UDP Protocol
     */
    MeshUDP = 20,

    /**
     * Mesh Reliable-UDP Protocol
     */
    MeshRUDP = 21,
};

/**
 * Type of the transmission protocol.
 */
enum TransmissionProtocol {
    /**
     * Domain Name Service
     */
    DNS = 53,
    DHCP = 67,
};

/**
 * Type of P2P connection.
 */
enum class P2PType : unsigned char {
    /**
     * P2P Connection type of the link that connected through Wifi-P2P(Wifi-Direct).
     */
    WifiP2P         = 0b10000001,

    /**
     * P2P Connection type of the link that connected through Bluetooth.
     */
    BluetoothP2P    = 0b10000010
};

/**
 * Type of the link connection.
 * Link's connection can be formed as following types.
 */
enum class ConnectionType : unsigned char {
    /**
     * Connection type of the link that connected through VPN(Device local).
     */
    VPN             = 0b00000001,
    /**
     * Connection type of the link that connected through Proxy(Outside of the mesh network).
     */
    Proxy           = 0b00000010,

    /**
     * P2P Connection type of the link that connected through Wifi-P2P(Wifi-Direct).
     */
    WifiP2P         = static_cast<int>(P2PType::WifiP2P),
    /**
     * P2P Connection type of the link that connected through Bluetooth.
     */
    BluetoothP2P    = static_cast<int>(P2PType::BluetoothP2P),
};

/**
 * Type of the address.
 */
enum AddrType {
    /**
     * IP Version 4
     */
    IPv4 = AF_INET,

    /**
     * IP Version 6
     */
    IPv6 = AF_INET6,

    /**
     * Unique ID used in the mesh network.
     */
    MESH_UID = 100,

    /**
     * Bluetooth UID.
     * Not yet supported.
     */
    BLUETOOTH_UID = -1,
};

/**
 * Type of network.
 */
enum NetworkType: unsigned char {
    INVALID                 = 0b10000000,
    NONE                    = 0b10000001,
    DEFAULT                 = 0b10000010,
    WIFI                    = 0b10000100,
    MOBILE                  = 0b10001000,
    MOBILE_WITH_WIFI_TEST   = 0b10010000,
    DUAL                    = 0b10100000,
    P2P                     = 0b11000000
};

/**
 * An Address class which support
 * All layers use and communicate with this Address class.
 */
class Address {
public:
    Address() = default;
    Address(std::string addr, uint16_t port = 0) : _addr(addr), _port(port) {}
    Address(AddrType type, ConnectionType connection, ConnectionProtocol protocol, std::string addr, uint16_t port)
            : _type(type), _connectionType(connection), _connectionProtocol(protocol), _addr(addr), _port(port) {}

    /**
     * Setters
     */
    void type(AddrType type) { _type = type; }
    void connectionType(ConnectionType connectionType) { _connectionType = connectionType; }
    void connectionProtocol(ConnectionProtocol connectionProtocol) { _connectionProtocol = connectionProtocol; }
    void addr(const std::string &address) { _addr = address; }
    void addr(const std::string &&address) { addr(address); }
    void port(uint16_t port) { _port = port; }
    void networkType(NetworkType networkType) { _networkType = networkType; }

    /**
     * Getters
     */
    AddrType type() const { return _type; }
    ConnectionType connectionType() const { return _connectionType; }
    ConnectionProtocol connectionProtocol() const { return _connectionProtocol; }
    std::string addr() const { return _addr; }
    uint16_t port() const { return _port; }
    NetworkType networkType() const { return _networkType; }
    bool isEmpty() const;

    /**
     * Returns whether connection type is P2P.
     */
    bool isP2PConnectionType() const;
    /**
     * Returns whether connection is mesh protocol.
     */
    bool isMeshProtocol() const;

    /**
     * Set address with a given uint32_T IPv4 address.
     */
    void setIPv4Addr(uint32_t ipv4Addr);
    /**
     * Gets the ipv4 address as a uint32_t.
     */
    uint32_t getIPv4AddrAsNum();
    /**
     * Gets the address as a raw IPv4 address.
     * @param rawAddress Raw ipv4 address will be filled.
     */
    bool getIPv4AddrAsRawBytes(uint8_t *rawAddress);
    bool setIPv4AddrWithRawBytes(uint8_t *rawAddress);
    /**
     * Gets the raw IPv4 address of this address.
     *
     * @param output Raw IPv4 address will be located at here.
     */
    std::string getIPv4Addr() const;

    /**
     * Set address with a given raw IPv6 address.
     *
     * @param address Raw IPv6 address.
     */
    bool setIPv6AddrWithRawBytes(const uint8_t *address);
    /**
     * Gets the address as a raw IPv6 address.
     * @param rawAddress Raw ipv6 address will be filled.
     */
    int getIPv6AddrAsRawBytes(uint8_t *rawAddress);
    bool setIPv6AddrWithMACAddr(const std::string &macAddress);
    /**
     * Set address with a given raw mesh uid.
     *
     * @param uid raw mesh uid.
     */
    void setMeshUID(UUID uid);
    /**
     * Returns a raw mesh uid.
     *
     * @return raw mesh uid.
     */
    UUID getMeshUID() const;

    /**
     * Operator overridings
     */
    bool operator==(const Anyfi::Address &other) const;
    bool operator!=(const Anyfi::Address &other) const;

    struct Hasher {
        std::size_t operator()(const Anyfi::Address& k) const;
    };

    /**
     * Validate that a given string is a valid IPv4 address
     */
    static bool isIPv4Addr(std::string &address);
    /**
     * Network Interface가 부여받은 IPv6 Address를 가져옴.
     * @param ifname IPv6 Address를 알고싶은 Network interface의 이름
     * @return 해당 Interface의 고유 IPv6 Address. Network interface를 찾을 수 없을 경우 비어있는 Anyfi::Address()
     */
    static Anyfi::Address getIPv6AddrFromInterface(const std::string &ifname);
    static Anyfi::Address getIPv4AddrFromInterface(const std::string &ifname);

    static const std::string getInterfaceNameWithIPv4Addr(const std::string& ip);
    static const std::string getInterfaceNameContainsIPv4Addr(const std::string& ip);

    static const std::unordered_map<std::string, std::list<std::string>> dumpInterfaces();

    static std::string toString(Anyfi::ConnectionType connType);
    static std::string toString(Anyfi::ConnectionProtocol protocol);
    static std::string toString(Anyfi::AddrType addrType);
    std::string toString() const;

private:
    AddrType _type = AddrType::IPv4;
    ConnectionType _connectionType = ConnectionType::Proxy;
    ConnectionProtocol _connectionProtocol = ConnectionProtocol::UDP;
    NetworkType  _networkType = NetworkType::DEFAULT;

    std::string _addr;
    uint16_t _port = 0;

    bool _getIPv4AddrAsRawBytes(const char* src, uint8_t *rawAddress);
    char *_IPv4RawBytesToChar(const uint8_t *src, char *dst, size_t size);
    int _getIPv6AddrAsRawBytes(const char* src, uint8_t *rawAddress);

};

}   // namespace Anyfi


/**
 * Removes a URL prefix from the given url if exists.
 */
std::string removeUrlPrefixIfExists(std::string url);


#endif //ANYFIMESH_CORE_ADDRESS_H
