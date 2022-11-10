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

#ifndef ANYFIMESH_CORE_IL1LINKLAYER_H
#define ANYFIMESH_CORE_IL1LINKLAYER_H

#include "../Common/Network/Buffer/NetworkBuffer.h"

#include "Link/Link.h"


class IL1LinkLayer {
public:
    virtual ~IL1LinkLayer() = default;
};

/**
 * L1LinkLayer interface for IOTunnel
 */
class IL1LinkLayerForIOTunnel : IL1LinkLayer {
public:
    /**
     * Callback function called when external vpn link state is changed.
     *
     * @param state changed link state
     */
    virtual void onExternalVpnLinkStateChanged(BaseChannel::State state) = 0;
};

/**
 * L1LinkLayer interface for L2NetworkLayer
 */
class IL1LinkLayerForL2 : IL1LinkLayer {
public:
    /**
     * Open the internal vpn link with file descriptor.
     * I/O operations of the internal vpn link will be processed in the L1LinkLayer.
     *
     * @param fd VPN file descriptor
     * @return information of the new internal vpn link
     */
    virtual ChannelInfo openInternalVpnChannel(int fd) = 0;

    /**
     * Open the external vpn link.
     * I/O operations of the external vpn link should be processed outside.
     * VPN I/O operations registered in L1LinkLayer will invoke through IOTunnel.
     *
     * @return information of the new external vpn link
     */
    virtual ChannelInfo openExternalVpnChannel() = 0;

    /**
     * Connect a new link.
     *
     * @param linkAddr link address to connect.
     * @param callback it called when connect finished, link_id in the LinkInfo is 0 if connect failed.
     */
    virtual void connectChannel(Anyfi::Address linkAddr, const std::function<void(ChannelInfo linkInfo)> &callback) = 0;

    /**
     * Open a new link.
     * The Opened link accepts connections from the other links.
     *
     * @param linkAddr link address to open.
     * @return id in the LinkInfo is -1 if open failed, link_id otherwise.
     */
    virtual ChannelInfo openChannel(Anyfi::Address linkAddr) = 0;

    /**
     * Close the link.
     *
     * @param channelId id of the link to close
     */
    virtual void closeChannel(unsigned short channelId) = 0;

    /**
     * Write a packet to the link.
     *
     * @param channelId id of the link to write
     * @param buffer shared_ptr of NetworkBuffer to write
     */
    virtual void writeToChannel(unsigned short channelId, NetworkBufferPointer buffer) = 0;
};


#endif //ANYFIMESH_CORE_IL1LINKLAYER_H
