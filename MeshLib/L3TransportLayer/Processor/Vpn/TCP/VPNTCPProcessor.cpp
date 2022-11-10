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

#include "VPNTCPProcessor.h"

#include <utility>
#include <Common/Timer.h>
#include <Core.h>

#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "Log/Frontend/Log.h"

#include "L3TransportLayer/IL3TransportLayer.h"
#include "L3TransportLayer/Packet/TCPIP_L2_Internet/IPv4Header.h"
#include "L3TransportLayer/Packet/TCPIP_L3_Transport/TCPHeader.h"
#include "L3TransportLayer/Packet/TCPIP_L3_Transport/TCPPacket.h"
#include "L3TransportLayer/Processor/Vpn/TCP/HTTP/HttpRequestParser.h"
#include "L3TransportLayer/ControlBlock/VPN/VpnTCB.h"


/**
 * Makes a simple TCP Packet.
 * Payload of the simple TCP packet is empty.
 */
NetworkBufferPointer _makeSimpleTCP(uint8_t flags, VpnTCB *tcb, Anyfi::Address src, Anyfi::Address dst,
                                              std::vector<SimpleTCPOption> options = std::vector<SimpleTCPOption>()) {
    NetworkBufferPointer buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();

    unsigned short optionLen = 0;
    for (auto &option : options)
        optionLen += TCPOption::lenOfKind(option.kind());
    if (optionLen % 4 != 0)
        optionLen += 4 - optionLen % 4;

    TCPHeaderBuilder tcpBuilder;
    tcpBuilder.seqNum(tcb->seq());
    tcpBuilder.ackNum(tcb->ack());
    tcpBuilder.flags(flags);
    tcpBuilder.srcAddr(src.getIPv4AddrAsNum(), src.port());
    tcpBuilder.dstAddr(dst.getIPv4AddrAsNum(), dst.port());
    tcpBuilder.tcpOptionLength(optionLen);
    tcpBuilder.offset(20 + optionLen);
    tcpBuilder.tcpLength(20 + optionLen);
    tcpBuilder.window(tcb->recvWindow());
    tcpBuilder.tcpOptions(std::move(options));

    IPv4HeaderBuilder ipBuilder;
    ipBuilder.totalLength(optionLen + 40);
    ipBuilder.protocol(IPNextProtocol::TCP);
    if (src.getIPv4AddrAsNum() == 0){
        Anyfi::Log::error(__func__, "Invalid src addr " + src.addr() + " / " + to_string(src.getIPv4AddrAsNum()));
        throw std::runtime_error("Invalid src addr " + src.addr());
    }
    ipBuilder.sourceAddress(src.getIPv4AddrAsNum());
    if (dst.getIPv4AddrAsNum() == 0){
        Anyfi::Log::error(__func__, "Invalid dst addr " + src.addr() + " / " + to_string(dst.getIPv4AddrAsNum()));
        throw std::runtime_error("Invalid dst addr " + src.addr());
    }
    ipBuilder.destAddress(dst.getIPv4AddrAsNum());

    if (dst.getIPv4AddrAsNum() == VPN_DNS_TUNNEL_IPV4) {
        tcpBuilder.srcAddr(VPN_DNS_TUNNEL_IPV4);
        tcpBuilder.dstAddr(VPN_ADDRESS_IPV4);
        ipBuilder.sourceAddress(VPN_DNS_TUNNEL_IPV4);
        ipBuilder.destAddress(VPN_ADDRESS_IPV4);
    }

    auto tcpHeader = tcpBuilder.build(buffer, buffer->getWritePos());
    auto ipHeader = ipBuilder.build(buffer, buffer->getWritePos());

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_TCP, "VPN TCP WRITE", "src: " + to_string(tcpHeader->sourcePort()) + ", dst: " + to_string(tcpHeader->destPort())
                                                                            + (tcpHeader->tcpOptionLength() > 0 ? ", opLen: " + to_string(tcpHeader->tcpOptionLength()) : "") +
                                                                            + ", flags: [" + ((tcpHeader->flags() & TCP_FLAG_SYN) > 0 ? "SYN " : "") +
                                                                            + ((tcpHeader->flags() & TCP_FLAG_FIN) > 0 ? "FIN " : "") +
                                                                            + ((tcpHeader->flags() & TCP_FLAG_ACK) > 0 ? "ACK " : "") +
                                                                            + ((tcpHeader->flags() & TCP_FLAG_RST) > 0 ? "RST " : "") + "]" +
                                                                            + ", seq: " + to_string(tcpHeader->seqNum()) + ", ack: " + to_string(tcpHeader->ackNum()) + ", len: 0"
                                                                            + ", wnd: " + to_string(tcpHeader->window()));
#endif

    return buffer;
}

/**
 * Add TCP header at the front of the given buffer(payload).
 */
NetworkBufferPointer _addTCPHeader(NetworkBufferPointer buffer, uint8_t flags, VpnTCB *tcb,
                                             Anyfi::Address src, Anyfi::Address dst) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    auto payloadLength = buffer->getWritePos() - buffer->getReadPos();

    TCPHeaderBuilder tcpBuilder;
    tcpBuilder.seqNum(tcb->seq());
    tcpBuilder.ackNum(tcb->ack());
    tcpBuilder.flags(flags);
    tcpBuilder.srcAddr(src.getIPv4AddrAsNum(), src.port());
    tcpBuilder.dstAddr(dst.getIPv4AddrAsNum(), dst.port());
    tcpBuilder.tcpOptionLength(0);
    tcpBuilder.offset(20);
    tcpBuilder.tcpLength(20 + payloadLength);
    tcpBuilder.window(tcb->recvWindow());

    IPv4HeaderBuilder ipBuilder;
    ipBuilder.totalLength(payloadLength + 40);
    ipBuilder.protocol(IPNextProtocol::TCP);
    ipBuilder.sourceAddress(src.getIPv4AddrAsNum());
    ipBuilder.destAddress(dst.getIPv4AddrAsNum());

    if (dst.getIPv4AddrAsNum() == VPN_DNS_TUNNEL_IPV4) {
        tcpBuilder.srcAddr(VPN_DNS_TUNNEL_IPV4);
        tcpBuilder.dstAddr(VPN_ADDRESS_IPV4);
        ipBuilder.sourceAddress(VPN_DNS_TUNNEL_IPV4);
        ipBuilder.destAddress(VPN_ADDRESS_IPV4);
    }

    auto tcpHeader = tcpBuilder.build(buffer, buffer->getWritePos());
    auto ipHeader = ipBuilder.build(buffer, buffer->getWritePos());

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_TCP, "VPN TCP WRITE2", "src: " + to_string(tcpHeader->sourcePort()) + ", dst: " + to_string(tcpHeader->destPort())
                                                                             + (tcpHeader->tcpOptionLength() > 0 ? ", opLen: " + to_string(tcpHeader->tcpOptionLength()) : "") +
                                                                             + ", flags: [" + ((tcpHeader->flags() & TCP_FLAG_SYN) > 0 ? "SYN " : "") +
                                                                             + ((tcpHeader->flags() & TCP_FLAG_FIN) > 0 ? "FIN " : "") +
                                                                             + ((tcpHeader->flags() & TCP_FLAG_ACK) > 0 ? "ACK " : "") +
                                                                             + ((tcpHeader->flags() & TCP_FLAG_RST) > 0 ? "RST " : "") + "]" +
                                                                             + ", seq: " + to_string(tcpHeader->seqNum()) + ", ack: " + to_string(tcpHeader->ackNum()) + ", len: " + to_string(payloadLength)
                                                                             + ", wnd: " + to_string(tcpHeader->window()));
#endif

    return buffer;
}

L3::VPNTCPProcessor::VPNTCPProcessor(std::shared_ptr<L3::IL3ForPacketProcessor> l3)
        : VPNPacketProcessor(std::move(l3)) {
    // Set tcp timer called every 100 ms.
    _timerTask = Anyfi::Timer::schedule(std::bind(&L3::VPNTCPProcessor::_tcpTimerTick, this), 100, 100);
}

L3::VPNTCPProcessor::~VPNTCPProcessor() {
    shutdown();
}

void parseHttpRequest (std::shared_ptr<IPPacket> packet, unsigned int ipHeaderLen, unsigned int tcpHeaderLen) {

    httpparser::Request request;
    httpparser::HttpRequestParser parser;

    unsigned short payloadStart =
            packet->header()->startPosition() + ipHeaderLen+ tcpHeaderLen;
    uint8_t* requestPayload = packet->buffer()->buffer() + payloadStart;
    uint payloadSize = packet->size() - tcpHeaderLen;
    httpparser::HttpRequestParser::ParseResult res = parser.parse(request, (char*) requestPayload, (char*)(requestPayload+payloadSize));

    if( res == httpparser::HttpRequestParser::ParsingCompleted )
    {
#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_TCP, "HTTP PARSE", request.inspect());
#endif
    }
    else
    {
        Anyfi::Log::error(__func__, "failed parsing HTTP Request " + res);
    }
}

void L3::VPNTCPProcessor::input(std::shared_ptr<IPPacket> packet) {
    if (packet->header()->version() == IPVersion::Version4) {
        auto ipv4Header = std::dynamic_pointer_cast<IPv4Header>(packet->header());
        auto tcpPacket = std::make_shared<TCPPacket>(packet->buffer(),
                                                     packet->startPosition() + packet->header()->length());
        auto tcpHeader = tcpPacket->header();

#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_TCP, "VPN TCP READ", "src: " + to_string(tcpHeader->sourcePort()) + ", dst: " + to_string(tcpHeader->destPort())
                                                                               + (tcpHeader->tcpOptionLength() > 0 ? ", opLen: " + to_string(tcpHeader->tcpOptionLength()) : "") +
                                                                               + ", flags: [" + ((tcpHeader->flags() & TCP_FLAG_SYN) > 0 ? "SYN " : "") +
                                                                               + ((tcpHeader->flags() & TCP_FLAG_FIN) > 0 ? "FIN " : "") +
                                                                               + ((tcpHeader->flags() & TCP_FLAG_ACK) > 0 ? "ACK " : "") +
                                                                               + ((tcpHeader->flags() & TCP_FLAG_RST) > 0 ? "RST " : "") + "]" +
                                                                               + ", seq: " + to_string(tcpHeader->seqNum()) + ", ack: " + to_string(tcpHeader->ackNum()) + ", len: " + to_string(tcpPacket->payloadSize())
                                                                               + ", wnd: " + to_string(tcpHeader->window()));
#endif

        Anyfi::Address srcAddr, dstAddr;
        srcAddr.setIPv4Addr(ipv4Header->sourceAddress());
        srcAddr.port(tcpHeader->sourcePort());
        srcAddr.connectionType(Anyfi::ConnectionType::VPN);
        srcAddr.connectionProtocol(Anyfi::ConnectionProtocol::TCP);
        dstAddr.setIPv4Addr(ipv4Header->destAddress());
        dstAddr.port(tcpHeader->destPort());
        dstAddr.connectionType(Anyfi::ConnectionType::Proxy);
        dstAddr.connectionProtocol(Anyfi::ConnectionProtocol::TCP);
        Anyfi::Core::getInstance()->ouim(dstAddr.addr());

        // When DNS response is larger than single UDP packet can convey,
        // the OS requests again using TCP.
        if (ipv4Header->destAddress() == VPN_DNS_TUNNEL_IPV4
        // Block ports other than 53 (e.g. 853) to disable DNS over TLS/HTTPS.
        && tcpHeader->destPort() == Anyfi::TransmissionProtocol::DNS) {
            ipv4Header->sourceAddress(VPN_DNS_TUNNEL_IPV4);
            ipv4Header->destAddress(VPN_DNS_SERVER_IPV4);
            srcAddr.setIPv4Addr(VPN_DNS_TUNNEL_IPV4);
            dstAddr.setIPv4Addr(VPN_DNS_SERVER_IPV4);
        }

                // TODO(insunj): no parsing for now.
        // parseHttpRequest(packet, ipv4Header->length(), tcpHeader->length());

        auto key = ControlBlockKey(srcAddr, dstAddr);
        auto tcb = _findTCB(key);
        if (tcb == nullptr) {
            if (tcpHeader->isSYN()) {
                tcb = _processSYN(tcpHeader.get(), key);
                _addTCB(tcb, key);
            } else if (tcpHeader->isFIN() && tcpPacket->payloadSize() == 0) {
                // Sends an empty ACK.
                auto tmpTCB = std::make_shared<VpnTCB>(key);
                tmpTCB->seq(tcpHeader->ackNum());
                tmpTCB->ack(tcpHeader->seqNum() + 1);
                tmpTCB->sendWindow(tcpHeader->window());
                tmpTCB->recvWindow(TCB_WINDOW_SIZE);
                _doSimpleACK(tmpTCB);
            } else if (tcpHeader->isRST()) {
                // Ignore
            } else {
                // Send RST
                auto tmpTCB = std::make_shared<VpnTCB>(key);
                tmpTCB->seq(tcpHeader->ackNum());
                tmpTCB->sendWindow(tcpHeader->window());
                tmpTCB->recvWindow(TCB_WINDOW_SIZE);
                _doSimpleRST(tmpTCB);
            }
        } else {
            ProcessResult result(NONE, nullptr);
            {
                // Locks the VpnTCB before process a packet with the VpnTCB.
                Anyfi::Config::lock_type guard(tcb->_mutex);

                if (tcpHeader->isSYN()) result = _processDuplicateSYN(tcpHeader.get(), tcb);
                else if (tcpHeader->isRST()) result = _processRST(tcpHeader.get(), tcb);
                else {
                    if (tcpHeader->isACK()) {
                        result = _processACK(tcpPacket.get(), tcb);
                    }
                    if (result.first == NONE) {
                        result = _processData(tcpPacket.get(), tcb);
                    }
                }
            }

            // Pass read buffer
            if (result.second != nullptr)
                _L3->onReadFromPacketProcessor(tcb, result.second);

            // Notify connection state change
            switch (result.first) {
                case CONNECTED: {
                    _L3->onSessionConnected(tcb);
                    break;
                }
                case CLOSED: {
                    _L3->onSessionClosed(tcb, false);
                    break;
                }
                case RESET: {
                    _L3->onSessionClosed(tcb, true);
                    break;
                }
                default:
                    break;
            }
        }
    } else if (packet->header()->version() == IPVersion::Version6) {
        Anyfi::Log::warn(__func__, "IPv6 Unsupported");
    }
}

void L3::VPNTCPProcessor::write(NetworkBufferPointer buffer, ControlBlockKey key) {
    if (!buffer->isBackwardMode())
        throw std::invalid_argument("Buffer should be backward networkMode");

    // Drop empty buffer
    if (buffer->getWritePos() - buffer->getReadPos() == 0) {
        Anyfi::Log::error("L3", "Buffer is empty");
        return;
    }

    std::shared_ptr<VpnTCB> tcb = _findTCB(key);
    if (tcb == nullptr) {
        Anyfi::Log::error("L3", "VpnTCB is nullptr, VPNTCP#write");
        return;
    }

    Anyfi::Config::lock_type tcbGuard(tcb->_mutex);
    if (tcb->state() != TCBState::ESTABLISHED) {
        // Ignore
        return;
    }

    tcb->addWriteBuffer(buffer);

    // Do ack when send window is not full
    if (tcb->remainSendWindow() > 0) {
        _doACK(tcb);
    }
}

void L3::VPNTCPProcessor::close(ControlBlockKey key, bool force) {
    std::shared_ptr<VpnTCB> tcb = _findTCB(key);
    if (tcb == nullptr) {
        std::cerr << "VpnTCB is nullptr in VPNTCP#close" << std::endl;
        return;
    }

#ifdef NDEBUG
#else
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L3_VPN_TCP, "VPN TCP OnClose", "src: " + to_string(key.srcAddress().port()) + ", dst: " + to_string(key.dstAddress().port()) + " forced: " + std::string(force ? "true" : "false"));
#endif

    Anyfi::Config::lock_type guard(tcb->_mutex);
    _closeImplWithoutLock(tcb, force);
}

void L3::VPNTCPProcessor::shutdown() {
    clear();

    if (_timerTask != nullptr) {
        Anyfi::Timer::cancel(_timerTask);
        _timerTask = nullptr;
    }
}

void L3::VPNTCPProcessor::clear() {
    Anyfi::Config::lock_type guard(_tcbMapMutex);

    for (auto it = _tcbMap.begin(); it != _tcbMap.end();){
        auto key = it->first;
        auto tcb = it->second;
        it = _tcbMap.erase(it);
        _L3->onControlBlockDestroyed(key);
        _L3->onSessionClosed(tcb, true);
    }
}

inline void L3::VPNTCPProcessor::_addTCB(const std::shared_ptr<VpnTCB> &tcb, ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_tcbMapMutex);
        _tcbMap[key] = tcb;
    }

    _L3->onControlBlockCreated(tcb);
}

inline void L3::VPNTCPProcessor::_removeTCB(ControlBlockKey &key) {
    {
        Anyfi::Config::lock_type guard(_tcbMapMutex);
        _tcbMap.erase(key);
    }

    _L3->onControlBlockDestroyed(key);
}

inline std::shared_ptr<VpnTCB> L3::VPNTCPProcessor::_findTCB(ControlBlockKey &key) {
    Anyfi::Config::lock_type guard(_tcbMapMutex);

    for (auto &item : _tcbMap) {
        if (item.second->key() == key)
            return item.second;
    }

    return nullptr;
}

void L3::VPNTCPProcessor::_tcpTimerTick() {
    _tcbMapMutex.lock();
    auto tcbMap = _tcbMap;
    _tcbMapMutex.unlock();

    for (const auto &tcb : tcbMap) {
        // Locks the VpnTCB, before tick and perform triggered action.
        Anyfi::Config::lock_type guard(tcb.second->_mutex);

        uint8_t triggeredActions = tcb.second->tickTimers();
        if (triggeredActions == 0)
            continue;

        if ((triggeredActions & TCPTimer::ACK_NOW) == TCPTimer::ACK_NOW) {
            _doACK(tcb.second);
        }
    }
}

std::shared_ptr<VpnTCB> L3::VPNTCPProcessor::_processSYN(TCPHeader *tcpHeader, ControlBlockKey &key) {
    auto tcb = std::make_shared<VpnTCB>(key);
    tcpHeader->tcpOptions();

    tcb->seq_random();
    tcb->ack(tcpHeader->seqNum() + 1);
    tcb->sendWindow(tcpHeader->window());
    tcb->sendWindowScale(tcpHeader->windowScale());
    tcb->recvWindow(TCB_WINDOW_SIZE);
    tcb->recvWindowScale(TCB_WINDOW_SCALE);
    tcb->mss(TCB_MSS);

    // Writes a (SYN | ACK)
    auto buffer = _makeSimpleTCP(TCP_FLAG_SYN | TCP_FLAG_ACK, tcb.get(), key.dstAddress(), key.srcAddress(),
                                 std::vector<SimpleTCPOption>({
                                                                      SimpleTCPOption(TCPOption::Kind::MSS, TCB_MSS),
                                                                      SimpleTCPOption(TCPOption::Kind::WS, (const uint32_t) tcb->recvWindowScale()),
                                                                      SimpleTCPOption(TCPOption::Kind::SACK_PERMITTED, 0)
                                                              }));
    auto dest = key.srcAddress();
    dest.connectionType(Anyfi::ConnectionType::VPN);
    _L3->writeFromPacketProcessor(dest, buffer);

    tcb->state(TCBState::SYN_RECEIVED);
    tcb->seq(tcb->seq() + 1);

    return tcb;
}

L3::VPNTCPProcessor::ProcessResult
L3::VPNTCPProcessor::_processDuplicateSYN(TCPHeader *tcpHeader, std::shared_ptr<VpnTCB> tcb) {
    return ProcessResult(NONE, nullptr);
}

L3::VPNTCPProcessor::ProcessResult
L3::VPNTCPProcessor::_processRST(TCPHeader *tcpHeader, std::shared_ptr<VpnTCB> tcb) {
    tcb->state(TCBState::CLOSED);
    _removeTCB(tcb->key());

    return ProcessResult(RESET, nullptr);
}

L3::VPNTCPProcessor::ProcessResult
L3::VPNTCPProcessor::_processData(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb) {
    if (tcb == nullptr) {
        std::cerr << "[VPNTCP] processData : tcb is null" << std::endl;
        return ProcessResult(NONE, nullptr);
    }

    if (tcb->state() < TCBState::ESTABLISHED) {
        std::cerr << "Invalid state(Not yet established) when receive FIN packet" << std::endl;
        return ProcessResult(NONE, nullptr);
    }

    // Drop invalid order packet.
    if (!_validateOrdering(tcpPacket->header().get(), tcb))
        return ProcessResult(NONE, nullptr);

    // When contains payload
    NetworkBufferPointer readBuffer;
    if (tcpPacket->payloadSize() != 0)
        readBuffer = _readPayload(tcpPacket, tcb);

    ProcessResult result(NONE, nullptr);
    switch (tcb->state()) {
        case TCBState::ESTABLISHED: {
            if (tcpPacket->header()->isFIN()) {
                tcb->state(TCBState::CLOSE_WAIT);
                tcb->ack(tcpPacket->header()->seqNum() + 1);
                tcb->addFlags(TCBFlag::DO_ACK);
                _doACK(tcb);

                /**
                 * Upon receiving the FIN in the ESTABLISHED state, close the connection immediately.
                 * Since receiving a FIN means that application close the socket, it's OK to close immediately.
                 */
                _closeImplWithoutLock(tcb, false);
            } else {
                // If received packet contains data or we can send more data, send packets
                // Delay ack if we do not need to ack now.
                if (tcpPacket->payloadSize() != 0) {
                    if (tcb->doACK()) _doACK(tcb);
                    else tcb->addFlags(TCBFlag::DELAYED_ACK);
                } else {
                    // Empty ACK, send if available
                    if ((tcb->remainSendWindow() > 0 && tcb->writeBufferTotalLength() > tcb->unackedBytes())
                        || tcb->doACK())
                        _doACK(tcb);
                }
            }
            result = ProcessResult(NONE, readBuffer);
            break;
        }
        case TCBState::FIN_WAIT_1: {
            if (tcpPacket->payloadSize() > 0) {
                tcb->state(TCBState::CLOSED);
                _doSimpleRST(tcb);
                _removeTCB(tcb->key());
            } else {
                // Send if data available
                if (tcb->remainSendWindow() > 0 && tcb->writeBufferTotalLength() > tcb->unackedBytes()) {
                    _doACK(tcb);
                }

                // Send FIN if not sent yet
                if (tcb->remainSendWindow() > 0 && (tcb->flags() & TCBFlag::DO_FIN) == TCBFlag::DO_FIN) {
                    _sendFIN(tcb);
                    tcb->removeFlags(TCBFlag::DO_FIN);
                }

                // Handle FIN received
                if (tcpPacket->header()->isFIN()) {
                    tcb->state(TCBState::CLOSING);
                    tcb->ack(tcb->ack() + 1);
                    tcb->addFlags(TCBFlag::DO_ACK);
                    _doSimpleACK(tcb);
                }
            }
            break;
        }
        case TCBState::FIN_WAIT_2: {
            if (tcpPacket->payloadSize() > 0) {
                tcb->state(TCBState::CLOSED);
                _doSimpleRST(tcb);
                _removeTCB(tcb->key());
            } else if (tcpPacket->header()->isFIN()) {
                tcb->state(TCBState::CLOSED);
                tcb->ack(tcb->ack() + 1);
                tcb->addFlags(TCBFlag::DO_ACK);
                _doSimpleACK(tcb);
                _removeTCB(tcb->key());
            }
            break;
        }
        case TCBState::CLOSING:
            if (tcpPacket->payloadSize() == 0) {
                // Send if data available
                if (tcb->remainSendWindow() > 0 && tcb->writeBufferTotalLength() > tcb->unackedBytes()) {
                    _doACK(tcb);
                }

                // Send FIN if not sent yet
                if (tcb->remainSendWindow() > 0 && (tcb->flags() & TCBFlag::DO_FIN) == TCBFlag::DO_FIN) {
                    _sendFIN(tcb);
                    tcb->removeFlags(TCBFlag::DO_FIN);
                }
            }
            break;
        case TCBState::CLOSE_WAIT:
        case TCBState::LAST_ACK: {
            _doSimpleACK(tcb);
            break;
        }
        default:
            break;
    }
    return result;
}

L3::VPNTCPProcessor::ProcessResult
L3::VPNTCPProcessor::_processACK(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb) {
    if (tcb == nullptr) {
        std::cerr << "[VPNTCP] processACK : tcb is null" << std::endl;
        return ProcessResult(NONE, nullptr);
    }

    // Update VpnTCB
    tcb->sendWindow(tcpPacket->header()->window());

    // Drop invalid order packet.
    if (!_validateOrdering(tcpPacket->header().get(), tcb))
        return ProcessResult(NONE, nullptr);

    if (tcb->state() == TCBState::SYN_RECEIVED) {
        tcb->state(TCBState::ESTABLISHED);
        tcb->acked(tcpPacket->header()->ackNum());

        return ProcessResult(CONNECTED, nullptr);
    } else if (tcb->state() == TCBState::LAST_ACK) {
        // Close TCP Session
        tcb->state(TCBState::CLOSED);
        _removeTCB(tcb->key());
        return ProcessResult(CLOSED, nullptr);
    } else if (tcb->state() == TCBState::FIN_WAIT_1) {
        uint32_t newAckedBytes = tcpPacket->header()->ackNum() - tcb->acked();
        if (newAckedBytes > 0) {
            if (tcb->dropWriteBuffer(newAckedBytes) > 0) {
                // FIN acknowledged
                tcb->state(TCBState::FIN_WAIT_2);
            }

            // Update VpnTCB
            tcb->acked(tcpPacket->header()->ackNum());
        }
        return ProcessResult(NONE, nullptr);
    } else if (tcb->state() == TCBState::CLOSING) {
        uint32_t newAckedBytes = tcpPacket->header()->ackNum() - tcb->acked();
        if (newAckedBytes > 0) {
            if (tcb->dropWriteBuffer(newAckedBytes) > 0) {
                // FIN acknowledged
                tcb->state(TCBState::CLOSED);
                _removeTCB(tcb->key());
                return ProcessResult(CLOSED, nullptr);
            }
        }
        return ProcessResult(NONE, nullptr);
    }

    // Check VpnTCB is created
    if (tcb->state() != TCBState::ESTABLISHED) {
        std::cerr << "[VPNTCP] Process ACK : VpnTCB state is not ESTABLISHED : " << (unsigned short) tcb->state()
                  << std::endl;
        return ProcessResult(NONE, nullptr);
    }

    // Check Duplicate ACK
    if (tcpPacket->payloadSize() == 0 && tcb->acked() == tcpPacket->header()->ackNum())
        tcb->add_dup_ack_count();
    else tcb->dup_ack_count(0);
    if (tcb->dup_ack_count() >= 3) {
        // Fast retransmit
        _retransmit(tcb);
    }

    uint32_t newAckedBytes = tcpPacket->header()->ackNum() - tcb->acked();
    if (newAckedBytes >= 0) {
        tcb->dropWriteBuffer(newAckedBytes);

        // Update VpnTCB
        tcb->acked(tcpPacket->header()->ackNum());
    }

    return ProcessResult(NONE, nullptr);
}

void L3::VPNTCPProcessor::_closeImplWithoutLock(std::shared_ptr<VpnTCB> tcb, bool force) {
    if (tcb->state() == TCBState::ESTABLISHED) {
        if (force) {
            // Buffers not sent yet can be ignored
            tcb->dropUnsentWriteBuffer();
            tcb->state(TCBState::CLOSED);
            _doSimpleRST(tcb);
            _removeTCB(tcb->key());
        } else {
            tcb->state(TCBState::FIN_WAIT_1);
            // Send FIN if all data sent has window available
            if (tcb->remainSendWindow() > 0 && tcb->writeBufferTotalLength() == tcb->unackedBytes()) {
                _sendFIN(tcb);
            } else {
                tcb->addFlags(TCBFlag::DO_FIN);
            }
        }
    } else if (tcb->state() == TCBState::CLOSE_WAIT) {
        // Buffers not sent yet can be ignored
        tcb->dropUnsentWriteBuffer();

        _sendFIN(tcb);
        tcb->state(TCBState::LAST_ACK);
    }
}

NetworkBufferPointer L3::VPNTCPProcessor::_readPayload(TCPPacket *tcpPacket, std::shared_ptr<VpnTCB> tcb) {
    auto payloadBuf = tcpPacket->buffer();
    payloadBuf->setReadPos(tcpPacket->payloadPos());
    tcb->ack(tcpPacket->header()->seqNum() + tcpPacket->payloadSize());
    return payloadBuf;
}

bool L3::VPNTCPProcessor::_validateOrdering(TCPHeader *tcpHeader, const std::shared_ptr<VpnTCB> &tcb) {
    if (tcpHeader->seqNum() == tcb->ack()) {
        // Valid : Receive in-order packet
        return true;
    } else if (tcpHeader->seqNum() > tcb->ack()) {
        Anyfi::Log::warn(__func__, "Receive out-of-order packet : Send Duplicate ACK");
        _doACK(tcb);
    } else if (tcpHeader->seqNum() < tcb->ack()) {
        Anyfi::Log::warn(__func__, "Spurious retransmission  src: " + to_string(tcpHeader->sourcePort()) + ", dst: " + to_string(tcpHeader->destPort()) +
                                                                               ", seq: " + to_string(tcpHeader->seqNum()) + ", ack: " + to_string(tcpHeader->ackNum()));

        // TODO : handle spurious retransmission
    }
    return false;
}

void L3::VPNTCPProcessor::_doSimpleRST(std::shared_ptr<VpnTCB> tcb) {
    // Sends an empty ACK.
    auto buffer = _makeSimpleTCP(TCP_FLAG_RST, tcb.get(), tcb->key().dstAddress(), tcb->key().srcAddress());

    auto dest = tcb->key().srcAddress();
    dest.connectionType(Anyfi::ConnectionType::VPN);

    _L3->writeFromPacketProcessor(dest, buffer);
    return;
}

void L3::VPNTCPProcessor::_doSimpleACK(std::shared_ptr<VpnTCB> tcb) {
    // Sends an empty ACK.
    auto buffer = _makeSimpleTCP(TCP_FLAG_ACK, tcb.get(), tcb->key().dstAddress(), tcb->key().srcAddress());

    auto dest = tcb->key().srcAddress();
    dest.connectionType(Anyfi::ConnectionType::VPN);

    _L3->writeFromPacketProcessor(dest, buffer);

    tcb->removeFlags(TCBFlag::DELAYED_ACK);
    tcb->removeFlags(TCBFlag::DO_ACK);
    return;
}

void L3::VPNTCPProcessor::_doACK(std::shared_ptr<VpnTCB> tcb) {
    auto offset = tcb->unackedBytes();
    auto buffer = tcb->getWriteBuffer(offset,
                                      (unsigned short) (tcb->mss() < tcb->remainSendWindow()
                                                        ? tcb->mss() : tcb->remainSendWindow()));
    if (buffer == nullptr) {
        // Sends an empty ACK.
        buffer = _makeSimpleTCP(TCP_FLAG_ACK, tcb.get(), tcb->key().dstAddress(), tcb->key().srcAddress());

        Anyfi::Address dest = tcb->key().srcAddress();
        dest.connectionType(Anyfi::ConnectionType::VPN);
        _L3->writeFromPacketProcessor(dest, buffer);

        tcb->removeFlags(TCBFlag::DELAYED_ACK);
        tcb->removeFlags(TCBFlag::DO_ACK);
        return;
    }

    // Sends write buffers until the send window is full.
    unsigned int payloadLength = 0;
    while (buffer != nullptr) {
        payloadLength = buffer->getWritePos() - buffer->getReadPos();
        if (payloadLength == 0) {
            break;
        }

        _addTCPHeader(buffer, TCP_FLAG_ACK, tcb.get(), tcb->key().dstAddress(), tcb->key().srcAddress());
        Anyfi::Address dest = tcb->key().srcAddress();
        dest.connectionType(Anyfi::ConnectionType::VPN);
        _L3->writeFromPacketProcessor(dest, buffer);

        tcb->seq(tcb->seq() + payloadLength);
        tcb->removeFlags(TCBFlag::DELAYED_ACK);
        tcb->removeFlags(TCBFlag::DO_ACK);

        offset += payloadLength;
        buffer = tcb->getWriteBuffer(offset,
                                     (unsigned short) (tcb->mss() < tcb->remainSendWindow()
                                                       ? tcb->mss() : tcb->remainSendWindow()));
    }
}

void L3::VPNTCPProcessor::_retransmit(std::shared_ptr<VpnTCB> tcb) {
}

void L3::VPNTCPProcessor::_sendFIN(std::shared_ptr<VpnTCB> tcb) {
    auto buffer = _makeSimpleTCP(TCP_FLAG_FIN | TCP_FLAG_ACK, tcb.get(), tcb->key().dstAddress(),
                                 tcb->key().srcAddress());

    auto dest = tcb->key().srcAddress();
    dest.connectionType(Anyfi::ConnectionType::VPN);

    _L3->writeFromPacketProcessor(dest, buffer);

    tcb->seq(tcb->seq() + 1);
}
