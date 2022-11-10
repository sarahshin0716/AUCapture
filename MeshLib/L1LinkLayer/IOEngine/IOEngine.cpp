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

#include "IOEngine.h"

#include <utility>

#include "Core.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"
#include "Log/Frontend/Log.h"


class ConcreteIOEngine : public IOEngine {
public:
    ConcreteIOEngine(std::shared_ptr<Selector> selector) : IOEngine(std::move(selector)) {}
};

std::shared_ptr<ConcreteIOEngine> IOEngine::_instance = nullptr;

std::shared_ptr<IOEngine> IOEngine::start(std::shared_ptr<Selector> selector) {
    if (_instance == nullptr)
        _instance = std::make_shared<ConcreteIOEngine>(selector);
    return _instance;
}

IOEngine::IOEngine(std::shared_ptr<Selector> selector)
        : _selector(std::move(selector)), _isOpen(true),
          _ioThreadPool(new ThreadPool("IOThread", IO_THREAD_COUNT)) {
    _selectorThread = std::thread(std::mem_fun(&IOEngine::_selectorThreadImpl), this, _selector);
}

IOEngine::~IOEngine() {
    shutdown();
}

void IOEngine::shutdown() {
    if (_isOpen) {
        _isOpen = false;

        {
            Anyfi::Config::lock_type guardWakeup(_selector->_wakeupLock);
            _selector->wakeup();
        }
        _selectorThread.join();

        delete _ioThreadPool;
        _ioThreadPool = nullptr;
    }
    _instance = nullptr;
}

void IOEngine::_selectorThreadImpl(std::shared_ptr<Selector> selector) {
    std::stringstream ss;
    ss << "selectorThread";
    pthread_setname_np(pthread_self(), ss.str().c_str());

    try {
        while (_isOpen) {
            selector->select();
            if (!_isOpen) {
                break;
            }

            std::set<std::shared_ptr<SelectionKey>> selectedKeys = selector->selectedKeys();
            for (const auto &key : selectedKeys) {
                if (key->isAcceptable())
                    _performAccept(key->link());

                if (key->isConnectable())
                    _performConnect(key->link());

                if (key->isWritable())
                    _performWrite(key->link());

                if (key->isReadable())
                    _performRead(key->link());
            }

            std::this_thread::yield();

            Anyfi::Config::lock_type guardWakeup(selector->_wakeupLock);
        }
    } catch (std::exception &e) {
        Anyfi::Log::error(__func__, "Exception catched in IOEngine#selectorThreadImpl : " +
                                    std::string(e.what()));
        throw std::runtime_error("IOEngine selector thread stopped");
    }
}

inline void IOEngine::_performAccept(std::shared_ptr<Link> link) {
    link->deregisterOperation(SelectionKeyOp::OP_ACCEPT);

    _ioThreadPool->enqueue([this, link]() {
        auto acceptedLink = link->doAccept();
        if(acceptedLink == nullptr) {
            Anyfi::Log::error(__func__, "acceptedLink is null");
            return;
        }

        link->registerOperation(_selector, SelectionKeyOp::OP_ACCEPT);

        if (acceptedLink)
            notifySockAccept(link, acceptedLink);
    });
}

inline void IOEngine::_performConnect(std::shared_ptr<Link> link) {
    link->deregisterOperation(SelectionKeyOp::OP_CONNECT);

    Anyfi::Core::getInstance()->enqueueTask([this, link]{
        link->finishSockConnect();
    });
}

inline void IOEngine::_performWrite(std::shared_ptr<Link> link) {
    link->deregisterOperation(SelectionKeyOp::OP_WRITE);

    _ioThreadPool->enqueue([this, link]() {
        try {
            {
                Anyfi::Config::lock_type guard(link->_writeQueueMutex);
                if(link->_writeQueue.empty())
                    return;

                while (!link->_writeQueue.empty()) {
                    auto buf = link->_writeQueue.front();

                    uint32_t wpos = buf->getWritePos(), rpos = buf->getReadPos();
                    int bytes = buf->getWritePos()-buf->getReadPos();
                    int writtenBytes = link->doWrite(buf);
                    if (writtenBytes == bytes) {
                        link->_writeQueue.pop_front();
                        link->writtenBytes += writtenBytes;
    #ifdef NDEBUG
    #else
                        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1_WRITE, "L1 WRITE", "sock: " + to_string(link->sock()) + " len: " + to_string(writtenBytes));
    #endif
                    } else {
                        if (writtenBytes > 0) {
                            buf->setWritePos(wpos);
                            buf->setReadPos(rpos + writtenBytes);
                            link->writtenBytes += writtenBytes;
                        } else if (errno == EAGAIN) {
                            Anyfi::Log::error(__func__, "L1 write EAGAIN - sock: " + to_string(link->sock()));
                            buf->setWritePos(wpos);
                            buf->setReadPos(rpos);
                        } else {
                            if (link->state() != LinkState::CLOSED){
                                Anyfi::Core::getInstance()->enqueueTask([this, link]{
                                    link->close();
                                });
                            }
                            return;
                        }
                        break;
                    }
                }

                if(!link->_writeQueue.empty()) {
                    link->registerOperation(_selector, SelectionKeyOp::OP_WRITE);
                }
            }
        } catch (std::exception &e) {
            Anyfi::Log::error(__func__, "Exception catched in IOEngine#performWrite : " +
                                        std::string(e.what()));
            if (link == nullptr) return;
            if (link->state() != LinkState::CLOSED) {
                Anyfi::Core::getInstance()->enqueueTask([this, link]{
                    link->close();
                });
            }
        }
    });
}

inline void IOEngine::_performRead(std::shared_ptr<Link> link) {
    link->deregisterOperation(SelectionKeyOp::OP_READ);

    _ioThreadPool->enqueue([this, link]() {
        try {
            auto readBuffer = link->doRead();

            if (readBuffer != nullptr) {
                link->readBytes += readBuffer->getWritePos() - readBuffer->getReadPos();
                notifyRead(link, readBuffer);
            } else {
                // Skip Try Again, otherwise close
                if (errno != EAGAIN) {
                    notifyRead(link, readBuffer);
                    return;
                }
            }

            if (link->state() != LinkState::CLOSED) {
                link->registerOperation(_selector, SelectionKeyOp::OP_READ);
            }
        } catch (std::exception &e) {
            Anyfi::Log::error(__func__, "Exception catched in IOEngine#performRead : " +
                                        std::string(e.what()));
            if (link == nullptr) return;
            if (link->state() != LinkState::CLOSED) {
                Anyfi::Core::getInstance()->enqueueTask([this, link]{
                    link->close();
                });
            }
        }
    });
}

void IOEngine::notifySockAccept(std::shared_ptr<Link> fromLink, std::shared_ptr<Link> acceptedLink) {
    Anyfi::Core::getInstance()->enqueueTask([this, fromLink, acceptedLink]{
        if (fromLink->channel() != nullptr) {
            fromLink->channel()->onAccepted(acceptedLink);
        }
    });
}

void IOEngine::notifyRead(std::shared_ptr<Link> link, NetworkBufferPointer buffer) {
    Anyfi::Core::getInstance()->enqueueTask([this, link, buffer]{
        // Connection closed
        if (buffer == nullptr) {
            link->close();
            return;
        }

#ifdef NDEBUG
#else
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L1_READ, "L1 READ", "sock: " + to_string(link->sock()) + " len: " + to_string(buffer->getWritePos() - buffer->getReadPos()));
#endif
        if (link->channel() != nullptr) {
            link->channel()->onRead(link, std::move(buffer));
        }
    });
}
