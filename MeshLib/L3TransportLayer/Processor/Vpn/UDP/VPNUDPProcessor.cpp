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

#include <Core.h>
#include "VPNUDPProcessor.h"

#include "L3TransportLayer/L3TransportLayer.h"
#include "L3TransportLayer/Packet/TCPIP_L2_Internet/IPv4Header.h"
#include "L3TransportLayer/Packet/TCPIP_L3_Transport/UDPHeader.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/DNS/DNSMessage.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/DNS/DNSRData.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/DNS/DNSException.h"
#include "L3TransportLayer/Packet/TCPIP_L4_Application/DNS/DNSQuerySection.h"

namespace {
std::string to_string(unsigned char *ipAddr) {
    std::ostringstream os;
    for (int i = 0; i < 4; i++) {
        os << std::to_string(ipAddr[i]);
        if (i < 3) os << ".";
    }
    return os.str();
}

void parseDNSResponse(NetworkBufferPointer buffer, unsigned int payloadLength) {
    dns::DNSMessage m;
    try
    {
        uint8_t* dnsPayload = buffer->buffer();
        m.decode((char *) dnsPayload, payloadLength);
        for (int i = 0; i < m.getAnCount(); i++) {
            dns::ResourceRecord* record = m.getAnswer(i);
//          Anyfi::Log::error(__func__, record->asString());
            if (record->getType() != dns::RDATA_A) continue;
            auto domain = m.getQuery(0)->getName();
            auto ipAddr = to_string(record->getRData<dns::RDataA>()->getAddress());
            Anyfi::Core::getInstance()->oudm(domain, ipAddr);
//          Anyfi::Log::error(__func__, "(" + domain + ", " + ipAddr + ")");
        }
    }
    catch (dns::DNSException& e)
    {
        Anyfi::Log::error(__func__,
                "DNS exception occurred when parsing incoming data: " + std::string(e.what()));
    }
}

// (note: currently not used)
void notifyDNSQuery(uint8_t *dnsPayload, uint payloadSize) {
    try
    {
        dns::DNSMessage m;
        m.decode((char *) dnsPayload, payloadSize);
        std::string name = m.getQuery(0)->getName();
        // Anyfi::Core::getInstance()->oudm(name);
#ifndef NDEBUG
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP DNS QUERY", name);
#endif
    }
    catch (dns::DNSException& e)
    {
        Anyfi::Log::error(__func__,
                "DNS exception occured when parsing incoming data: " + std::string(e.what()));
    }
}
}

L3::VPNUDPProcessor::VPNUDPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3)
        : VPNPacketProcessor(std::move(l3)) {
    _ucbMap = make_unique<LRUCache<ControlBlockKey, std::shared_ptr<VpnUCB>, ControlBlockKey::Hasher>>(
            MAX_VPN_UCB_COUNT);
    _ucbMap->attachCleanUpCallback([this](ControlBlockKey key, std::shared_ptr<VpnUCB> ucb) {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP OnCleanUp", "src: " + to_string(ucb->key().srcAddress().port()) + ", dst: " + to_string(ucb->key().dstAddress().port()));
#endif
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(ucb, false);
    });

//    _timerTask = Anyfi::Timer::schedule(std::bind(&L3::VPNUDPProcessor::_timerTick, this),
//                                        VPN_UCB_EXPIRE_TIMER_PERIOD, VPN_UCB_EXPIRE_TIMER_PERIOD);
}

L3::VPNUDPProcessor::~VPNUDPProcessor() {
    shutdown();
}

void L3::VPNUDPProcessor::input(std::shared_ptr<IPPacket> packet) {
    if (packet->header()->version() == IPVersion::Version4) {
        auto ipv4Header = std::dynamic_pointer_cast<IPv4Header>(packet->header());
        auto udpHeader = std::make_shared<UDPHeader>(packet->buffer(),
                                                     packet->startPosition() +
                                                     packet->header()->length());
        unsigned short payloadStart =
                packet->header()->startPosition() + ipv4Header->length() + udpHeader->length();

        Anyfi::Address srcAddr, dstAddr;
        srcAddr.setIPv4Addr(ipv4Header->sourceAddress());
        srcAddr.port(udpHeader->sourcePort());
        srcAddr.connectionType(Anyfi::ConnectionType::VPN);
        srcAddr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);
        dstAddr.setIPv4Addr(ipv4Header->destAddress());
        dstAddr.port(udpHeader->destPort());
        dstAddr.connectionType(Anyfi::ConnectionType::Proxy);
        dstAddr.connectionProtocol(Anyfi::ConnectionProtocol::UDP);

        if (ipv4Header->destAddress() == VPN_DNS_TUNNEL_IPV4) {
            ipv4Header->sourceAddress(VPN_DNS_TUNNEL_IPV4);
            ipv4Header->destAddress(VPN_DNS_SERVER_IPV4);
            srcAddr.setIPv4Addr(VPN_DNS_TUNNEL_IPV4);
            dstAddr.setIPv4Addr(VPN_DNS_SERVER_IPV4);
//            notifyDNSQuery(packet->buffer()->buffer() + payloadStart, packet->size() - udpHeader->length());
        }

        auto key = ControlBlockKey(srcAddr, dstAddr);
        auto ucb = _findUCB(key);
        if (ucb == nullptr) {
            ucb = std::make_shared<VpnUCB>(key);
            _addUCB(ucb, key);

#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP CONNECT",
                              "src: " + to_string(udpHeader->sourcePort()) +
                              ", dst: " + to_string(udpHeader->destPort()));
#endif

            _L3->onSessionConnected(ucb);
        }

        ucb->clearExpireTimerTicks();
        packet->buffer()->setReadPos(payloadStart);

////        FOR DEBUG ONLY
//        if (udpHeader->destPort() == PORT_NUM_DNS_PORT) {
//            dns::DNSMessage m;
//            uint8_t* dnsPayload = packet->buffer()->buffer() + payloadStart;
//            uint payloadSize = packet->size() - udpHeader->length();
//            try
//            {
//                m.decode((char *) dnsPayload, payloadSize);
//                std::string name = m.getQuery(0)->getName();
//
//                Anyfi::Log::error("domain: ", name);
//            }
//            catch (dns::DNSException& e)
//            {
//                Anyfi::Log::error(__func__,
//                        "DNS exception occured when parsing incoming data: " + std::string(e.what()));
//            }
//        }

#ifndef NDEBUG
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP READ",
                          "src: " + to_string(ipv4Header->sourceAddress()) + ":" + to_string(udpHeader->sourcePort()) +
                          ", dst: " + to_string(ipv4Header->destAddress()) + ":" + to_string(udpHeader->destPort()) +
                          ", len: " + to_string(packet->buffer()->getWritePos() - packet->buffer()->getReadPos()));
#endif

        _L3->onReadFromPacketProcessor(ucb, packet->buffer());
    } else if (packet->header()->version() == IPVersion::Version6) {
        Anyfi::Log::warn(__func__, "IPv6 Unsupported");
    }
}

void L3::VPNUDPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Write buffer must be backward networkMode");

    if (key.srcAddress().type() == Anyfi::AddrType::IPv4) {
        auto payloadLength = buffer->getWritePos() - buffer->getReadPos();

        UDPHeaderBuilder udpBuilder;
        udpBuilder.setSrcAddrPort(key.dstAddress());
        udpBuilder.setDstAddrPort(key.srcAddress());
        udpBuilder.udpLength(static_cast<uint16_t>(UDP_HEADER_LENGTH + payloadLength));

        IPv4HeaderBuilder ipBuilder;
        ipBuilder.totalLength(payloadLength + UDP_HEADER_LENGTH + 20);
        ipBuilder.protocol(IPNextProtocol::UDP);
        ipBuilder.sourceAddress(key.dstAddress().getIPv4AddrAsNum());
        ipBuilder.destAddress(key.srcAddress().getIPv4AddrAsNum());

        if (key.srcAddress().getIPv4AddrAsNum() == VPN_DNS_TUNNEL_IPV4) {
            udpBuilder.srcAddr(VPN_DNS_TUNNEL_IPV4);
            udpBuilder.dstAddr(VPN_ADDRESS_IPV4);
            ipBuilder.sourceAddress(VPN_DNS_TUNNEL_IPV4);
            ipBuilder.destAddress(VPN_ADDRESS_IPV4);
            parseDNSResponse(buffer, payloadLength);
        }

        auto udpHeader = udpBuilder.build(buffer, buffer->getWritePos());
        auto ipHeader = ipBuilder.build(buffer, buffer->getWritePos());

#ifndef NDEBUG
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP WRITE",
                           "src: " + to_string(key.srcAddress().getIPv4AddrAsNum()) + ":" + to_string(udpHeader->sourcePort()) +
                          ", dst: " + to_string(key.dstAddress().getIPv4AddrAsNum()) + ":" + to_string(udpHeader->destPort()) +
                          ", len: " + to_string(payloadLength));
#endif

        auto ucb = _findUCB(key);
        if (ucb != nullptr)
            ucb->clearExpireTimerTicks();

        _L3->writeFromPacketProcessor(key.srcAddress(), buffer);
    } else if (key.srcAddress().type() == Anyfi::AddrType::IPv6) {
        // TODO : IPv6 not yet supported
    }
}

void L3::VPNUDPProcessor::close(ControlBlockKey key, bool force) {
#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP OnClose", "src: " + to_string(key.srcAddress().port()) + ", dst: " + to_string(key.dstAddress().port()) + " forced: " + std::string(force ? "true" : "false"));
#endif

    _removeUCB(key);
}

void L3::VPNUDPProcessor::shutdown() {
    clear();

    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }
}

void L3::VPNUDPProcessor::clear() {
    auto ucbMap = _copyUCBMap();

    Anyfi::Config::lock_type guard(_ucbMapMutex);
    for (auto &item: ucbMap) {
        auto ucb = item.second;
        _L3->onControlBlockDestroyed(ucb->key());
        _L3->onSessionClosed(ucb, true);
    }

    _ucbMap->clear();
}

void L3::VPNUDPProcessor::_addUCB(std::shared_ptr<VpnUCB> ucb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);
        _ucbMap->put(key, ucb);
    }

    _L3->onControlBlockCreated(ucb);
}

void L3::VPNUDPProcessor::_removeUCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_ucbMapMutex);

        _ucbMap->remove(key);
    }

    _L3->onControlBlockDestroyed(key);
}

std::shared_ptr<VpnUCB> L3::VPNUDPProcessor::_findUCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    if (_ucbMap->exists(key))
        return _ucbMap->get(key);

    return nullptr;
}

std::unordered_map<ControlBlockKey, std::shared_ptr<VpnUCB>, ControlBlockKey::Hasher>
L3::VPNUDPProcessor::_copyUCBMap() {
    Anyfi::Config::lock_type guard(_ucbMapMutex);

    auto map = std::unordered_map<ControlBlockKey, std::shared_ptr<VpnUCB>, ControlBlockKey::Hasher>();
    auto it = _ucbMap->listIterBegin();
    while (it != _ucbMap->listIterEnd()) {
        auto cache = *it++;
        auto key = cache.first;
        auto ucb = cache.second;
        map[key] = ucb;
    }

    return map;
}

void L3::VPNUDPProcessor::_timerTick() {
    auto ucbMap = _copyUCBMap();

    for (auto &item: ucbMap) {
        auto ucb = item.second;
        ucb->addExpireTimerTicks();

        auto expiredTick = VPN_UCB_EXPIRE_TIME / VPN_UCB_EXPIRE_TIMER_PERIOD;
        if (ucb->expireTimerTicks() >= expiredTick) {
#ifdef NDEBUG
#else
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_UDP, "VPN UDP OnExpire", "src: " + to_string(ucb->key().srcAddress().port()) + ", dst: " + to_string(ucb->key().dstAddress().port()));
#endif

            // Expired
            _removeUCB(ucb->key());

           _L3->onSessionClosed(ucb, true);
        }
    }
}
