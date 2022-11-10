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

#ifndef ANYFIMESH_CORE_GRAPHCHANGEINFO_H
#define ANYFIMESH_CORE_GRAPHCHANGEINFO_H

#include <string>

#include "MeshEdge.h"

/**
 * 변동된 MeshNetwork 그래프 정보를 주고받기 위한 클래스입니다.
 */
class GraphChangeInfo {
public:
    GraphChangeInfo(bool isAdd, const std::shared_ptr<MeshEdge> &meshEdge) {
        _isAdd = isAdd;
        _meshEdge = meshEdge;
    }

    /**
     * Getters
     */
    bool isAdd() { return _isAdd; };
    std::shared_ptr<MeshEdge> meshEdge() { return _meshEdge; };

    std::string toString() {
        std::string desc = "{";
        desc += " isAdd: " + std::string((_isAdd)?"true":"false");
        desc += ", " + ((_meshEdge)?_meshEdge->toString(): std::string("null"));
        desc += "}";
        return desc;
    }

private:
    bool _isAdd;
    std::shared_ptr<MeshEdge> _meshEdge;
};


#endif //ANYFIMESH_CORE_GRAPHCHANGEINFO_H
