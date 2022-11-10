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

#ifndef ANYFIMESH_CORE_CHANNEL_H
#define ANYFIMESH_CORE_CHANNEL_H

#include <utility>

#include "../../L1LinkLayer/IOEngine/Selector.h"
#include "../../L1LinkLayer/Link/Link.h"
#include "../../Common/Network/Address.h"


class BaseChannel : public std::enable_shared_from_this<BaseChannel>  {
public:
    enum State : uint8_t {
        CLOSED,
        CONNECTING,
        CONNECTED,
    };

    BaseChannel(unsigned short channelId, std::shared_ptr<Selector> selector)
            : _id(channelId), _selector(std::move(selector)) {};
    BaseChannel(unsigned short channelId, std::shared_ptr<Selector> selector, Anyfi::Address address)
            : _id(channelId), _selector(std::move(selector)), _address(std::move(address)) {};

    unsigned short id() { return _id; }
    State state() { return _state; }
    void state(State state) { _state = state; }
    Anyfi::Address addr() { return _address; }

    virtual void connect(Anyfi::Address address, const std::function<void(bool result)> &callback) = 0;
    virtual bool open(Anyfi::Address address) = 0;
    virtual void close() = 0;
    virtual void write(NetworkBufferPointer buffer) = 0;

    virtual std::vector<int> socks() = 0;

    virtual void onRead(const std::shared_ptr<Link> &link, const NetworkBufferPointer &buffer);
    virtual void onAccepted(const std::shared_ptr<Link> &newLink);
    virtual void onClose(std::shared_ptr<Link> link) = 0;

    typedef std::function<void(std::shared_ptr<BaseChannel> channel, const std::shared_ptr<Link> &link, NetworkBufferPointer buffer)> ReadCallback;
    typedef std::function<void(std::shared_ptr<BaseChannel> acceptFrom, std::shared_ptr<Link> acceptedLink)> AcceptCallback;
    typedef std::function<void(std::shared_ptr<BaseChannel> link)> CloseCallback;
    void setReadCallback(const ReadCallback &callback) { _readCallback = callback; }
    void setAcceptCallback(const AcceptCallback &callback) { _acceptCallback = callback; }
    void setCloseCallback(const CloseCallback &callback) { _closeCallback = callback; }

    bool operator==(const BaseChannel &other) const;
    bool operator<(const BaseChannel &other) const;

protected:
    const unsigned short _id;
    State _state = State::CLOSED;

    const std::shared_ptr<Selector> _selector;
    Anyfi::Address _address;

    ReadCallback _readCallback = nullptr;
    AcceptCallback _acceptCallback = nullptr;
    CloseCallback _closeCallback = nullptr;

public:
    virtual const std::string toString();
};


/**
 * SimpleChannel owns only one link.
 */
class SimpleChannel : public BaseChannel {
public:
    SimpleChannel(unsigned short channelId, std::shared_ptr<Selector> selector)
            : BaseChannel(channelId, std::move(selector)) { }
    SimpleChannel(unsigned short channelId, std::shared_ptr<Selector> selector, const Anyfi::Address &address)
            : BaseChannel(channelId, std::move(selector), address) { }

    std::shared_ptr<Link> link() const { return _link; }
    std::vector<int> socks() override;

protected:
    std::shared_ptr<Link> _link;

public:
    virtual const std::string toString() override;
};


/**
 * ComplexChannel owns multiple links.
 */
class ComplexChannel : public BaseChannel {
public:
    ComplexChannel(unsigned short channelId, std::shared_ptr<Selector> selector, const Anyfi::Address &address, unsigned char linkCount)
            : BaseChannel(channelId, std::move(selector), address), _linkCount(linkCount), _links(linkCount) { }

    std::vector<int> socks() override;

protected:
    const unsigned char _linkCount;
    std::vector<std::shared_ptr<Link>> _links;

public:
    virtual const std::string toString() override;
};

/**
 * A channel information which contains channel id and address.
 * This struct used to pass channel information to other layer.
 */
struct ChannelInfo {
public:
    ChannelInfo() : channelId(0) {}
    ChannelInfo(unsigned short id, Anyfi::Address address) : channelId(id), addr(std::move(address)) {}

    std::string toString() {
        std::string desc = "{ id: ";
        desc.append(to_string(channelId));
        desc.append(", addr: " + addr.toString());
        return desc;
    };

    unsigned short channelId;
    Anyfi::Address addr;
};

#endif //ANYFIMESH_CORE_CHANNEL_H
