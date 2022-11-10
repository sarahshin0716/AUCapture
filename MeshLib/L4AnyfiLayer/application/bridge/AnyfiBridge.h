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

#ifndef ANYFIMESH_CORE_ANYFIBRIDGE_H
#define ANYFIMESH_CORE_ANYFIBRIDGE_H

#include <queue>
#include <mutex>

#include "../../../L4AnyfiLayer/IL4AnyfiLayer.h"


namespace L4 {
class AnyfiBridge {
public:
    enum Type : unsigned char {
        Unknown                 = 0b10000000,
        VpnToMesh               = 0b10000001,
        MeshToProxy             = 0b10000010,
        VpnToProxy              = 0b10000100
    };
    explicit AnyfiBridge(std::shared_ptr<IL4AnyfiLayerForApplication> L4) {
        _L4 = std::move(L4);
        _type = Type::Unknown;
    };
    explicit AnyfiBridge(std::shared_ptr<IL4AnyfiLayerForApplication> L4, unsigned short cbId) {
        L4 = std::move(L4);
        _meshCbId = cbId;
        _type = Type::Unknown;
    }

    void type(AnyfiBridge::Type type) { _type = type; }
    AnyfiBridge::Type type() { return _type; }
    void meshCbId(unsigned short meshCbId) { _meshCbId = meshCbId; }
    unsigned short meshCbId() { return _meshCbId; }
    void vpnCbId(unsigned short vpnCbId) { _vpnCbId = vpnCbId; }
    unsigned short vpnCbId() { return _vpnCbId; }
    void proxyCbId(unsigned short proxyCbId) { _proxyCbId = proxyCbId; }
    unsigned short proxyCbId() { return _proxyCbId; }
    void srcAddr(Anyfi::Address srcAddr) { _srcAddr = srcAddr; }
    Anyfi::Address srcAddr() { return _srcAddr; }
    void connectAddr(Anyfi::Address connectAddr) { _connectAddr = connectAddr; }
    Anyfi::Address connectAddr() { return _connectAddr; }
    void setMain(bool main) { _main = main; }
    bool isMain() { return _main; }

    virtual void connect(Anyfi::Address destAddr, std::function<void(unsigned short)> callback);
    virtual void read(NetworkBufferPointer buffer);
    virtual void write(NetworkBufferPointer buffer);
    virtual void close(unsigned short cbId, bool forced);
    virtual void closeAll(bool forced);
    void writeAllInQueue();

    unsigned short getSourceCbId();
    unsigned short getDestCbId();

protected:
    void _pushBuffer(NetworkBufferPointer buffer);
    std::queue<NetworkBufferPointer> _popAllBuffer();
    std::queue<NetworkBufferPointer> _popAllBufferWithoutLock();
    void _clearBuffer();

    Anyfi::Config::mutex_type _writeBufferQueueMutex;
    std::queue<NetworkBufferPointer> _writeBufferQueue;

    AnyfiBridge::Type _type;
    unsigned short _vpnCbId = 0;
    unsigned short _meshCbId = 0;
    unsigned short _proxyCbId = 0;
    Anyfi::Address _srcAddr;
    Anyfi::Address _connectAddr;
    bool _main = false;
    std::shared_ptr<IL4AnyfiLayerForApplication> _L4;
};
};
#endif //ANYFIMESH_CORE_ANYFIBRIDGE_H
