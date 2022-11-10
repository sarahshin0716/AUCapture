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

#ifndef ANYFIMESH_CORE_SHREADPTRWITHCALLBACK_H
#define ANYFIMESH_CORE_SHREADPTRWITHCALLBACK_H

#include <memory>


template<class T>
class CallbackableSharedPtr : public std::shared_ptr<T> {
public:
    typedef std::shared_ptr<T> base;

    typedef void (*callback)(const CallbackableSharedPtr<T> &);

    callback _cbRefCountInc;
    callback _cbRefCountDec;


    CallbackableSharedPtr() : base() {}

    CallbackableSharedPtr(std::nullptr_t null) : base(null) {}

    CallbackableSharedPtr(const CallbackableSharedPtr &p) : base(p),
                                                            _cbRefCountInc(p._cbRefCountInc),
                                                            _cbRefCountDec(p._cbRefCountDec) {
        if (this->get() && _cbRefCountInc)
            _cbRefCountInc(*this);
    }

    template<class U>
    CallbackableSharedPtr(CallbackableSharedPtr<U> &&p) : base(std::move(p)),
                                                          _cbRefCountInc(p._cbRefCountInc),
                                                          _cbRefCountDec(p._cbRefCountDec) {
    }

    template<class U>
    CallbackableSharedPtr(std::shared_ptr<U> p, callback inc = nullptr, callback dec = nullptr)
            :std::shared_ptr<T>(std::move(p)), _cbRefCountInc(inc), _cbRefCountDec(dec) {
        if (this->get() && _cbRefCountInc)
            _cbRefCountInc(*this);
    }

    CallbackableSharedPtr &operator=(CallbackableSharedPtr &&p) {
        if (this != &p) {
            if (this->get() && _cbRefCountDec)
                _cbRefCountDec(*this);

            base::operator=(std::move(p));
            _cbRefCountInc = p._cbRefCountInc;
            _cbRefCountDec = p._cbRefCountDec;
        }
        return *this;
    }

    CallbackableSharedPtr &operator=(const CallbackableSharedPtr &p) {
        if (this != &p) {
            if (this->get() && _cbRefCountDec)
                _cbRefCountDec(*this);

            base::operator=(p);
            _cbRefCountInc = p._cbRefCountInc;
            _cbRefCountDec = p._cbRefCountDec;

            if (this->get() && _cbRefCountInc)
                _cbRefCountInc(*this);
        }
        return *this;
    }

    void reset() {
        if (this->get() && _cbRefCountDec)
            _cbRefCountDec(*this);

        std::shared_ptr<T>::reset();
    }

    ~CallbackableSharedPtr() {
        if (this->get() && _cbRefCountDec)
            _cbRefCountDec(*this);
    }
};

#endif //ANYFIMESH_CORE_SHREADPTRWITHCALLBACK_H
