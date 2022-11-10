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

#ifndef ANYFIMESH_CORE_ANYFIPROXYPACKET_H
#define ANYFIMESH_CORE_ANYFIPROXYPACKET_H

#include "../../../Common/Network/Address.h"
#include "../../../Common/Network/Buffer/NetworkBuffer.h"

class AnyfiBridgePacket {
public:
    enum Type {
        Unknown = 0,
        Connect = 1,
        Data    = 2
    };
    static std::string toString(Type type) {
        switch (type) {
            case Type::Connect:
                return "ProxyConn";
            case Type::Data:
                return "Data";
            default:
                return "Unknown";
        }
    }

    AnyfiBridgePacket();
    ~AnyfiBridgePacket() = default;

    Type type() { return _type; }
//    void type(Type type) { _type = type; }

    /**
     * ByteBuffer 형태로 Packing합니다.
     * @return std::shared_ptr<Bytebuffer> packing된 버퍼
     */
    virtual NetworkBufferPointer toPacket() = 0;
    /**
     * 디버깅 편의성을 위해 각종 변수 값을 std::string형태로 값을 반환합니다
     * @return std::string 문자열로 변환된 패킷 설정값
     */
    virtual std::string toString() = 0;
    /**
     * ByteBuffer 형태로 변환된 AnyfiPacket을 다시 클래스 형태로 되돌립니다.
     * @return std::shared_ptr<AnyfiProxyPacket>
     */
//   virtual std::shared_ptr<{AnyfiBridgePacket}> toSerialize() { return nullptr; };

    /**
     * AnyfiProxyPacket이 어떤 타입인지 반환합니다.
     * @param buffer NetworkBuffer로 변환된 AnyfiProxyPacket
     * @return AnyfiProxyPacket::Type 타입
     */
    static AnyfiBridgePacket::Type getType(NetworkBufferPointer buffer);

protected:
    Type _type;
};

/**
 * 패킷 형태
 * |---------------------------------------------------|
 * | 1 byte | 4 bytes | 2 bytes  | 1 byte   | 1 byte   |
 * |---------------------------------------------------|
 * | Type   | DestIP  | DestPort | Protocol | AddrType |
 * |---------------------------------------------------|
 */
class AnyfiBridgeConnPacket : public AnyfiBridgePacket {
public:
    AnyfiBridgeConnPacket();
    explicit AnyfiBridgeConnPacket(const Anyfi::Address &proxyAddr);

    /**
     * Getter & setter
     */
    void destIp(uint32_t ipAddr) { _destAddr.setIPv4Addr(ipAddr); }
    void destPort(uint16_t port) { _destAddr.port(port); }
    void protocol(Anyfi::ConnectionProtocol protocol) { _destAddr.connectionProtocol(protocol); }
    void addr(const Anyfi::Address &addr) { _destAddr = addr; }
    uint32_t destIp() { return _destAddr.getIPv4AddrAsNum(); }
    uint16_t port() { return _destAddr.port(); }
    Anyfi::ConnectionProtocol protocol() { return _destAddr.connectionProtocol(); }
    Anyfi::Address addr() { return _destAddr; }

    /**
     * Override : AnyfiProxyPacket
     */
    NetworkBufferPointer toPacket() override;
    std::string toString() override;

    static std::shared_ptr<AnyfiBridgeConnPacket> toSerialize(NetworkBufferPointer buffer);
private:
    Anyfi::Address _destAddr;
};

/**
 * 패킷 형태
 * |------------------|
 * | 1 byte | N Bytes |
 * |------------------|
 * | Type   | PayLoad |
 * |------------------|
 */
class AnyfiBridgeDataPacket : public AnyfiBridgePacket {
public:
    AnyfiBridgeDataPacket();
    explicit AnyfiBridgeDataPacket(const NetworkBufferPointer &buffer);

    /**
     * Getter & setter
     */
    void payload(const NetworkBufferPointer &payload) { _payload = payload; }
    NetworkBufferPointer payload() { return _payload; }

    /**
     * Override : AnyfiProxyPacket
     */
    NetworkBufferPointer toPacket() override;
    std::string toString() override;

    static std::shared_ptr<AnyfiBridgeDataPacket> toSerialize(NetworkBufferPointer buffer);
private:
    NetworkBufferPointer _payload;
};

#endif //ANYFIAnyfi_CORE_ANYFIPROXYPACKET_H
